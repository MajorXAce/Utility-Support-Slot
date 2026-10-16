-- Host/two-player formation, replacement and recovery passed scoped live checks.
-- Automatic inventory publication, independent of the research workbench.
-- A member handle, wire ID, resource identity and selected slot are distinct.
-- Typed pair eligibility retains the tested host/two-player native contract.
local function inventory_publication(services)
    local memory, game, ffi = services.memory, services.game_base, services.ffi
    local uint = services.read_u32
    local pins = inventory_publication_pins()
    local check_dispatch=inventory_publication_dispatch(services)
    local publisher = {}
    local replacement_primary=replacement_primary_boundary(services)
    function publisher.begin_replacement(w)replacement_primary.begin(w)end
    function publisher.end_replacement()replacement_primary.close()end
    local record, game_thread, last_reason
    local latest_guard, refused_guard, guard_sequence = nil, nil, 0
    local guard_logs = 0 -- At most16 detailed refusal lines per mission/reset.
    local suspended, next_poll = false, 0
    local used, rate_until, entered, returned = 0, 0, 0, 0
    local epoch, last_mission = 0, false
    local disabled = false
    local function emit(message) services.emit('MP_PUBLICATION '..message) end
    local function require_value(value, reason)
        if value == nil or value == false then error(reason, 0) end
        return value
    end
    local reads, fingerprint
    local count_reads_only=false
    local function read(address, size)
        if fingerprint or count_reads_only then
            reads = reads + 1
            if reads > 320 then error('READ_BUDGET', 0) end
        end
        local bytes = require_value(memory.read(address, size), 'READ')
        if #bytes ~= size then error('PARTIAL_READ', 0) end
        if fingerprint then fingerprint[#fingerprint+1] = tostring(address)..':'..bytes end
        return bytes
    end
    local function pointer(address)
        local value = require_value(memory.pointer(read(address, 8)), 'POINTER')
        if value < 65536 or value >= 0x800000000000 or value%8 ~= 0 then
            error('POINTER_RANGE_OR_ALIGNMENT', 0)
        end
        return value
    end
    -- Hash tables contain two DWORDs per bucket. Multiplication is modulo2^32;
    -- split it into16-bit halves so Lua doubles cannot lose integer precision.
    local function lookup(address, key)
        local header = read(address, 20)
        local capacity = uint(header, 8)
        if capacity == 0 then return nil end
        if capacity > 1048576 then error('HASH_CAPACITY', 0) end
        local power = 1
        while power < capacity do power = power*2 end
        if power ~= capacity then error('HASH_CAPACITY_POWER', 0) end
        local buckets = require_value(memory.pointer(header:sub(1,8)), 'HASH_BUCKETS')
        if buckets < 65536 or buckets%4 ~= 0 then error('HASH_BUCKET_ALIGNMENT', 0) end
        local multiplier = uint(header, 16)
        local product = ((key%65536)*(multiplier%65536)
            + ((math.floor(key/65536)*(multiplier%65536)
            + (key%65536)*math.floor(multiplier/65536))%65536)*65536)%4294967296
        for offset = 0, math.min(capacity,32)-1 do
            local bucket = read(buckets+((product+offset)%capacity)*8,8)
            if uint(bucket,0) == key then
                local index = uint(bucket,4)
                if index == 0xFFFFFFFF then return nil end
                if index > 16383 then error('HASH_INDEX',0) end
                return index
            end
            if uint(bucket,0) == uint(header,12) then return nil end
        end
        error('HASH_LOOKUP_LIMIT',0)
    end
    local function copy_guard(value)
        if not value then return nil end
        local copy={}
        for key,field in pairs(value) do copy[key]=field end
        return copy
    end
    local function note(reason)
        -- Capture at the callback, not later while the weapon is already drawn.
        -- Repeated SKIP reasons are deduplicated, but both swap boundaries matter.
        if reason=='PRIMARY_BOUNDARY_NOT_READY' and latest_guard then
            refused_guard=copy_guard(latest_guard)
            if latest_guard.stage~='POLL' and guard_logs<16 then
                guard_logs=guard_logs+1
                local fields={}
                for _,key in ipairs({'sequence','stage','epoch','tick_ms','avatar','primary',
                    'support','active','held','row_avatar','selected','prior','phase','pending',
                    'timer_bits','row1c','bit47','state_byte','global_guard','ready'}) do
                    fields[#fields+1]=key:upper()..'='..tostring(refused_guard[key])
                end
                emit('GUARD '..table.concat(fields,' '))
            end
        end
        if last_reason ~= reason then emit('SKIP REASON='..reason);last_reason=reason end
        return false, reason
    end
    local function scope()
        local state = pointer(game+0x3326340)
        local mode = pointer(game+0x33266A0)
        local session = pointer(game+0x347CEF0)
        local mission = uint(read(state+0xAC21C,4),0)==4 and uint(read(mode+8,4),0)==1
        local self = read(session+0xB398,8)
        local host = read(session+0xB3A8,8)
        local zero = string.rep(string.char(0),8)
        if self == zero then error('SELF_PEER_MISSING',0) end
        return mission, self==host or host==zero, session, self, state
    end
    local function update_epoch(mission)
        if mission and not last_mission then epoch=epoch+1;record=nil;replacement_primary.close()end
        if not mission then record=nil;latest_guard=nil;refused_guard=nil;guard_logs=0;replacement_primary.close()end
        last_mission=mission
    end
    local function supported_pair(pair)
        local utility=pair and services.weapon_identity(pair.utility)
        local ordinary=pair and services.weapon_identity(pair.support)
        -- Classification widens the weapon catalog, not native eligibility. Each
        -- capture still checks exact descriptor identities and both live parents.
        return utility and ordinary and services.is_utility(utility)
            and not services.is_utility(ordinary) and ordinary==pair.support_id
    end
    -- The table identifies one Core pair lifetime. A later pair may reuse both
    -- entity handles; it must not inherit an earlier lifetime's completed send.
    local function formed_pair(pair)
        return pair and record and record.formed and record.pair_ref==pair
            and record.owner==pair.owner and record.avatar==pair.avatar
            and record.utility==pair.utility and record.ordinary==pair.support or false
    end
    -- Eligibility for inserting a local primary boundary, not permission to send.
    -- The normal boundary still repeats the complete native entry contract.
    function publisher.pending_formation(owner)
        if disabled or suspended or not owner then return false end
        local ok,eligible=pcall(function()
            local pair=services.current_pair()
            if not supported_pair(pair) or formed_pair(pair)
                or pair.owner~=owner.owner or pair.avatar~=owner.avatar
                or _G.UtilitySupportSlotContext~=services.context then return false end
            local mission,host,session=scope()
            return mission and host and uint(read(session+0x162D8,4),0)==2
        end)
        return ok and eligible or false
    end
    local function character_primary(owner,game_state,stage)
        local manager = pointer(game+0x3326D20)
        local index = require_value(lookup(manager+0xF8,owner.avatar),'CHARACTER_MISSING')
        if index > 4096 then error('CHARACTER_INDEX',0) end
        local row = read(manager+0x53DCD0+index*0x1238,32)
        local flags = read(manager+0x53E880+index*0x1238,24)
        local bit47 = math.floor(uint(flags,12)/32768)%2
        local phase, timer = uint(row,16), uint(row,24)
        local settled = phase==0 and timer==0 and bit47==0
        -- Both origins were captured at the real managed-draw callback with
        -- primary actually held. Native selection preserves either prior slot;
        -- the inventory sender maps handles and does not consume this phase row.
        -- Keep other origins/transitions refused; this is not any-phase eligibility.
        local prior=uint(row,8)
        local holstering = phase==1 and (prior==2 or prior==3) and uint(row,20)==1
            and bit47==1 and timer>0 and timer<0x7F800000
        -- These are copied values, not setters. Eagerly read the entire predicate
        -- on refusal so short-circuit evaluation cannot hide which field differed.
        local held=services.held_weapon(owner.avatar)
        local global_guard=uint(read(game_state+0xAC218,4),0)
        local state_byte=read(manager+0x53EAE0+index*0x1238,1):byte()
        local ready=uint(row,0)==owner.avatar and uint(row,4)==1 and owner.active==1
            and held==owner.primary and uint(row,28)==0 and global_guard==0
            and state_byte==0 and (settled or holstering)
        guard_sequence=guard_sequence+1
        latest_guard={sequence=guard_sequence,stage=stage,epoch=epoch,tick_ms=services.clock(),
            avatar=owner.avatar,primary=owner.primary,support=owner.support,active=owner.active,
            held=held,row_avatar=uint(row,0),selected=uint(row,4),prior=uint(row,8),
            phase=phase,pending=uint(row,20),timer_bits=timer,row1c=uint(row,28),
            bit47=bit47,state_byte=state_byte,global_guard=global_guard,ready=ready}
        -- Only Core's active replacement may use the measured phase2 shape.
        -- Formation and ordinary swaps retain the original predicate above.
        local early=replacement_primary.admit(owner,latest_guard,stage,epoch)
        latest_guard.early_replacement=early
        if early then ready=true;latest_guard.ready=true end
        if not ready then error('PRIMARY_BOUNDARY_NOT_READY',0) end
    end
    -- Formation uses the unchanged primary predicate, including the row1c guard.
    -- This readiness read authorizes no send; complete captures still guard entry.
    function publisher.formation_primary_ready(owner)
        if disabled or suspended then return false,'PUBLICATION_UNAVAILABLE' end
        local ok,reason=pcall(function()
            local mission,host,_,_,game_state=scope()
            if not mission or not host or not supported_pair(services.current_pair()) then
                error('FORMATION_SCOPE_CHANGED',0)
            end
            character_primary(owner,game_state,'FORMATION_WAIT')
        end)
        return ok,not ok and tostring(reason)or nil
    end

    -- Replacement may wait across updates while the real support still holds
    -- the new pickup. Its dropped partner is not reattached until primary is
    -- ready. This reads the existing primary predicate only; native entry still
    -- performs both complete captures after ownership has been restored.
    -- Between replacement stages the engine can advance. Read current mission
    -- and authority without sending, resetting epochs or trusting cached status.
    function publisher.replacement_scope_ready(expected_epoch)
        if disabled or suspended or epoch~=expected_epoch
            or _G.UtilitySupportSlotContext~=services.context then return false end
        local ok,mission,host=pcall(scope)
        return ok and mission and host or false
    end
    function publisher.replacement_primary_ready(owner)
        local previous_sequence=guard_sequence
        if not publisher.pending_formation(owner) then return false,'REPLACEMENT_SCOPE_CHANGED' end
        local ok,reason=pcall(function()
            local mission,host,_,_,game_state=scope()
            if not mission or not host then error('REPLACEMENT_SCOPE_CHANGED',0) end
            character_primary(owner,game_state,'REPLACEMENT_WAIT')
        end)
        -- Reuse this exact guard sample; diagnostics never issue another read.
        return ok,not ok and tostring(reason) or nil,
            guard_sequence>previous_sequence and copy_guard(latest_guard) or nil
    end
    local function capture(stage,include_slot_drop,recovery)
        latest_guard=nil
        reads=0;fingerprint={}
        local mission, host, session, self, game_state = scope()
        if not mission or not host then error('HOST_MISSION_REQUIRED',0) end
        if _G.UtilitySupportSlotContext~=services.context then error('CONTEXT_CHANGED',0) end
        local owner = require_value(services.sample_inventory(),'INVENTORY_MISSING')
        local pair = services.current_pair()
        if recovery then
            if pair or recovery.epoch~=epoch or recovery.session~=session
                or recovery.owner~=owner.owner or recovery.avatar~=owner.avatar
                or recovery.inventory~=owner.inventory or recovery.address~=owner.address
                or recovery.primary~=owner.primary or recovery.secondary~=owner.secondary
                or recovery.target~=owner.support or recovery.id~=services.weapon_identity(owner.support)
                or recovery.attempted then error('LONE_CONTEXT_CHANGED',0) end
        elseif not supported_pair(pair) then error('PAIR_PROFILE_UNTESTED',0) end
        if (not recovery and (pair.owner~=owner.owner or pair.avatar~=owner.avatar))
            or owner.inventory~=pointer(game+0x3326738)
            or owner.owner~=pointer(game+0x3326B90)
            or owner.row~=read(owner.address,48)
            or uint(owner.row,0)~=owner.primary or uint(owner.row,8)~=owner.support
            or uint(owner.row,28)~=owner.active
            or (not recovery and owner.support~=pair.utility and owner.support~=pair.support) then
            error('OWNER_INVENTORY_CHANGED',0)
        end
        character_primary(owner,game_state,stage)
        local count = uint(read(session+0x162D8,4),0)
        if count~=2 then error('TWO_PLAYER_SCOPE_REQUIRED',0) end
        local peers = read(session+0x162E0,16)
        local zero = string.rep(string.char(0),8)
        if peers:sub(1,8)~=self or peers:sub(9,16)==zero or peers:sub(9,16)==self then
            error('RECIPIENT_SELF_ORDER',0)
        end
        if recovery and peers~=recovery.peers then error('LONE_PEERS_CHANGED',0) end
        local auxiliary=pointer(session+0x1F860) -- Native-owned dispatch object.
        local helpers = pointer(game+0x33263E8)
        local root = pointer(game+0x3326308)
        local interface = pointer(root+0x38)
        local targets={pointer(helpers+0x70),pointer(helpers+0x78),pointer(interface+0x40)}
        local function keep(value)fingerprint[#fingerprint+1]=tostring(value)end
        check_dispatch(read,keep,session,helpers,targets,auxiliary,include_slot_drop)
        local mapping = pointer(game+0x346BF98)
        local function mapped(handle)
            local index = require_value(lookup(mapping+0xF1AEB0,handle),'HANDLE_MAPPING')
            local descriptor = read(mapping+0xF32F18+index*24,24)
            local compact = uint(descriptor,16)
            if uint(descriptor,8)~=handle or compact>32766 then error('HANDLE_IDENTITY',0) end
            local reverse = require_value(lookup(mapping+0xF22EC8,compact),'WIRE_MAPPING')
            if read(mapping+0xF32F18+reverse*24,24)~=descriptor then error('MAPPING_ROUNDTRIP',0) end
            return descriptor,compact
        end
        local _, avatar_wire = mapped(owner.avatar)
        local registry = pointer(game+0x3326E68)
        local component_count = uint(read(registry+0x6C0,4),0)
        if component_count>32 then error('COMPONENT_REGISTRY_LIMIT',0) end
        local component
        for index=0,component_count-1 do
            local entry = registry+0x6C8+index*16
            if uint(read(entry+8,4),0)==141 then
                if component then error('DUPLICATE_COMPONENT_MANAGER',0) end
                component=pointer(entry)
            end
        end
        component=require_value(component,'COMPONENT_MANAGER_MISSING')
        local relationships = pointer(game+0x3326698)
        local units = pointer(game+0x3326DC0)
        local unit_header = read(units,16)
        local unit_count = uint(unit_header,12)
        if unit_count<1 or unit_count>16384 or unit_count>uint(unit_header,0) then error('UNIT_COUNTS',0) end
        local distinct = {}
        local wires = {avatar=avatar_wire}
        local roles=recovery and {{'primary',owner.primary},{'survivor',owner.support}}
            or {{'primary',owner.primary},{'utility',pair.utility},{'ordinary',pair.support}}
        if recovery then
            -- The outgoing entity may have been deleted by consumption. A live
            -- entity must retain its exact identity and be detached, never owned
            -- by us or another avatar. Missing units must also be absent from
            -- Core's support identity reader; stale/reused handles refuse entry.
            local outgoing_id=services.weapon_identity(recovery.outgoing)
            local index=lookup(units+0x20,recovery.outgoing)
            if index then
                if index>=unit_count or outgoing_id~=recovery.outgoing_id then error('LONE_OUTGOING_CHANGED',0) end
                local descriptor=read(pointer(pointer(units+0x38)+index*8),24)
                local row=read(pointer(units+0x40)+index*48,20)
                if descriptor:sub(1,8)~=recovery.outgoing_id
                    or uint(descriptor,8)~=recovery.outgoing or uint(row,4)~=0 then error('LONE_OUTGOING_PARENT',0) end
            elseif outgoing_id~=nil then error('LONE_OUTGOING_AMBIGUOUS',0) end
        end
        for _, role in ipairs(roles) do
            local name,handle = role[1],role[2]
            if handle<1 or handle>=0xFFFFFFFF or handle==owner.avatar or distinct[handle] then error('ROLE_ALIAS',0) end
            distinct[handle]=true
            local descriptor,wire = mapped(handle)
            local identity = descriptor:sub(1,8)
            -- Core's identity helper is support-only (unit type2). Primary is
            -- validated by the mapping roundtrip, unit descriptor and parent below,
            -- matching the proven probe rather than treating its nil API result as loss.
            if name~='primary' and services.weapon_identity(handle)~=identity
                or name=='utility' and not services.is_utility(identity)
                or name=='ordinary' and (identity~=pair.support_id or services.is_utility(identity)) then error('RESOURCE_IDENTITY',0) end
            local index = require_value(lookup(units+0x20,handle),'UNIT_MISSING')
            if index>=unit_count then error('UNIT_INDEX',0) end
            local unit_descriptor = read(pointer(pointer(units+0x38)+index*8),24)
            local unit_row = read(pointer(units+0x40)+index*48,20)
            if unit_descriptor:sub(1,8)~=identity or uint(unit_descriptor,8)~=handle
                or uint(unit_row,4)~=owner.avatar then error('UNIT_PARENT_OR_IDENTITY',0) end
            if descriptor:byte(21)%2~=1 or lookup(component+0x20,handle)~=nil then
                error('COMPONENT_ELIGIBILITY',0)
            end
            local relation = require_value(lookup(relationships+0x18,handle),'RELATIONSHIP_MISSING')
            if uint(read(pointer(relationships+0x38)+relation*172,4),0)==0 then error('RELATIONSHIP_PREDICATE',0) end
            wires[name]=wire
        end
        local sentinel = uint(read(game+0x348456C,4),0)
        if uint(read(game+0x2154544,4),0)~=32767 then error('EMPTY_WIRE_ID',0) end
        local payload_seen = {}
        for _,offset in ipairs({0,4,8,12,24}) do
            local handle = uint(owner.row,offset)
            if recovery and handle==recovery.outgoing then error('LONE_OUTGOING_IN_INVENTORY',0) end
            if handle==sentinel then
                if offset==0 or offset==8 then error('REQUIRED_MEMBER_EMPTY',0) end
            else
                if handle==0 or handle==0xFFFFFFFF or handle==owner.avatar or payload_seen[handle] then
                    error('PAYLOAD_ROLE_ALIAS',0)
                end
                payload_seen[handle]=true;mapped(handle)
            end
        end
        -- Recheck every native-entry pin. No code or arbitrary pointer is executed
        -- to discover compatibility, and all game state above is freshly sampled.
        for _,pin in ipairs(pins) do
            if read(game+pin.rva,#pin.bytes)~=pin.bytes then error('PUBLICATION_PIN_CHANGED',0) end
        end
        local after = require_value(services.sample_inventory(),'INVENTORY_RECHECK')
        if after.row~=owner.row or after.address~=owner.address or after.avatar~=owner.avatar
            or after.owner~=owner.owner or after.inventory~=owner.inventory
            or services.current_pair()~=pair then error('CAPTURE_RECHECK',0) end
        local thread = services.thread_id()
        if not thread or thread<1 or game_thread and thread~=game_thread then error('GAME_THREAD_CHANGED',0) end
        local key=table.concat({session,mapping,owner.owner,owner.avatar,owner.inventory,
            recovery and recovery.outgoing or pair.utility,recovery and recovery.target or pair.support},':')..peers
        local result={owner=owner,pair=pair,recovery=recovery,wires=wires,peer_key=peers,key=key,session=session,
            fingerprint=table.concat(fingerprint,'|'),thread=thread}
        fingerprint=nil
        return result
    end
    local function capture_checked(stage,include_slot_drop,recovery)
        local ok,result=pcall(capture,stage,include_slot_drop,recovery);fingerprint=nil
        if not ok then return nil,tostring(result) end
        return result
    end

    -- Replacement control captures check both message policies in one shared
    -- dependency pass, then retain a separate bounded 320-read native-pin pass.
    -- Each native entry still takes its own fresh capture. No idle/solo work.
    local drop_check
    local function check_drop(...)
        if not drop_check then drop_check=inventory_publication_dispatch(services,true) end
        return drop_check(...)
    end
    -- Cache immutable reference bytes, never a result of checking live code.
    local drop_pins
    local function slot_drop_pins()
        if not drop_pins then drop_pins=inventory_slot_drop_pins()end
        return drop_pins
    end
    function publisher.replacement_control_capture()
        if disabled or suspended then return nil,'CONTROL_PUBLICATION_DISABLED' end
        local snap,why=capture_checked('REPLACEMENT_CONTROL',true)
        if not snap then return nil,why end
        reads=0;fingerprint={}
        local ok,reason=pcall(function()
            local _,_,session=scope()
            -- Shared dispatch dependencies and both message policies were freshly
            -- checked together above. Keep the separate local-drop native pins
            -- and refuse a session change during the remainder of this capture.
            require_value(session==snap.session,'CONTROL_SESSION_CHANGED')
            for _,pin in ipairs(slot_drop_pins()) do
                local at=game+pin.rva
                local page=services.query_dispatch(at)
                local protection=page and page.protection%256
                require_value(page and page.allocation==game and page.kind==0x1000000
                    and page.state==0x1000 and at>=page.base
                    and at+#pin.bytes<=page.base+page.size
                    and page.protection<256
                    and (protection==32 or protection==64 or protection==128),
                    'CONTROL_NATIVE_CODE_PAGE')
                if read(at,#pin.bytes)~=pin.bytes then error('CONTROL_NATIVE_PIN',0) end
            end
        end)
        if ok then snap.fingerprint=snap.fingerprint..'|'..table.concat(fingerprint,'|') end
        fingerprint=nil
        if not ok then return nil,tostring(reason) end
        return snap
    end

    function publisher.replacement_route_check()
        if disabled or suspended then return nil,'CONTROL_PUBLICATION_DISABLED' end
        reads=0;fingerprint=nil;count_reads_only=true
        local ok,reason=pcall(function()
            local _,_,session=scope()
            local helpers=pointer(game+0x33263E8)
            local interface=pointer(pointer(game+0x3326308)+0x38)
            local targets={pointer(helpers+0x70),pointer(helpers+0x78),pointer(interface+0x40)}
            check_drop(read,function()end,
                session,helpers,targets,pointer(session+0x1F860))
            for _,pin in ipairs(slot_drop_pins()) do
                local at=game+pin.rva
                local page=services.query_dispatch(at)
                local protection=page and page.protection%256
                require_value(page and page.allocation==game and page.kind==0x1000000
                    and page.state==0x1000 and at>=page.base
                    and at+#pin.bytes<=page.base+page.size
                    and page.protection<256
                    and (protection==32 or protection==64 or protection==128),
                    'CONTROL_NATIVE_CODE_PAGE')
                if read(at,#pin.bytes)~=pin.bytes then error('CONTROL_NATIVE_PIN',0) end
            end
        end)

        count_reads_only=false;fingerprint=nil
        if not ok then return nil,tostring(reason) end
        -- Retain the reviewed route check inside subsequent membership captures,
        -- including after native attachment has run. A pre-stage check alone
        -- must not substitute for checking the actual publication entry.
        services.replacement_queue_checks=true
        return true
    end

    local function publish(kind,snapshot)
        local now=services.clock()
        if now>=rate_until then used=0;rate_until=now+10000 end
        if used>=64 then return note('PUBLICATION_RATE_LIMIT') end
        local fresh,reason=capture_checked('ENTRY_'..kind,nil,snapshot.recovery)
        if not fresh then return note(reason) end
        if fresh.pair~=snapshot.pair or fresh.fingerprint~=snapshot.fingerprint or fresh.key~=snapshot.key then return note('ENTRY_CHANGED') end
        local metadata=ffi.new('uint64_t[1]')
        ffi.copy(metadata,fresh.owner.row:sub(41,48),8)
        if ffi.string(metadata,8)~=fresh.owner.row:sub(41,48) then return note('METADATA_COPY') end
        used=used+1;entered=entered+1;game_thread=fresh.thread
        -- Consume before native entry. A Lua exception leaves delivery uncertain;
        -- do not replay the call or invent a compensating network operation.
        if snapshot.recovery then snapshot.recovery.attempted=true
        elseif kind=='FORMATION' then record.formation_attempted=true end
        local ok,why=pcall(services.send,fresh.owner,metadata[0])
        if not ok then disabled=true;emit('UNCONFIRMED KIND='..kind..' RETRY=false');return false,tostring(why) end
        returned=returned+1;last_reason=nil
        if not snapshot.recovery then record.formed=true;record.member=fresh.owner.support end
        emit('RETURNED KIND='..kind..' AVATAR='..fresh.owner.avatar..' MEMBER='..fresh.owner.support
            ..' CALLS='..returned..' EPOCH='..epoch..' DELIVERY_PROVEN=false')
        if kind=='FORMATION' then
            local a,b=fresh.peer_key:sub(1,8),fresh.peer_key:sub(9,16)
            if b<a then a,b=b,a end
            record.targets={avatar=fresh.wires.avatar,primary=fresh.wires.primary,
                utility=fresh.wires.utility,ordinary=fresh.wires.ordinary,
                peers=(a..b):gsub('.',function(c)return string.format('%02x',c:byte())end)}
        end
        return true
    end
    function publisher.boundary(owner,formation_only,stage)
        if disabled then return note('DISABLED') end
        if suspended then return false,'SUSPENDED' end
        local pair=services.current_pair()
        if not supported_pair(pair) or not owner or owner.active~=1 then return false,'NOT_APPLICABLE' end
        local snapshot,reason=capture_checked(stage or (formation_only and 'PRE_ATTACH_FORMATION' or 'POST_ATTACH_INCOMING'))
        if not snapshot then return note(reason) end
        -- Do not publish a different inventory than the caller just verified.
        if owner.row~=snapshot.owner.row or owner.address~=snapshot.owner.address
            or owner.owner~=snapshot.owner.owner or owner.avatar~=snapshot.owner.avatar then
            return note('CALLER_SNAPSHOT_CHANGED')
        end
        owner,pair=snapshot.owner,snapshot.pair
        update_epoch(true)
        if not record or record.pair_ref~=pair or record.key~=snapshot.key then record={key=snapshot.key,pair_ref=pair,formed=false,owner=owner.owner,avatar=owner.avatar,
            inventory=owner.inventory,utility=pair.utility,ordinary=pair.support,session=snapshot.session,peers=snapshot.peer_key,
            utility_id=services.weapon_identity(pair.utility),ordinary_id=pair.support_id} end
        if not record.formed then
            -- Ordinary-first setup publishes the restored ordinary member. In
            -- utility-first setup that would omit the locally recovered utility:
            -- publish it at its explicit capture boundary, then publish ordinary
            -- on restoration. Both use the real row+8 and all native guards.
            local recovered_utility=(stage=='UTILITY_FIRST_CAPTURE' or stage=='REPLACEMENT_RETAINED_UTILITY')
                and owner.support==pair.utility
            if owner.support~=pair.support and not recovered_utility then
                return note('FORMATION_PENDING_ORDINARY')
            end
            if record.formation_attempted then return note('FORMATION_ALREADY_ATTEMPTED') end
            return publish('FORMATION',snapshot)
        end
        if formation_only then return true,'FORMATION_UNCHANGED' end
        if owner.support==record.member then return true,'MEMBER_UNCHANGED' end
        return publish('INCOMING',snapshot)
    end
    -- Capture the accepted pair lifetime before Core clears it. This is an
    -- operation record for one real survivor, never a fabricated current pair.
    function publisher.begin_lone(owner,target,id,outgoing,outgoing_id)
        if disabled or suspended or not owner or not formed_pair(services.current_pair())
            or record.owner~=owner.owner or record.avatar~=owner.avatar
            or record.inventory~=owner.inventory or target==outgoing
            or (target~=record.utility and target~=record.ordinary)
            or (outgoing~=record.utility and outgoing~=record.ordinary)
            or id~=(target==record.utility and record.utility_id or record.ordinary_id)
            or outgoing_id~=(outgoing==record.utility and record.utility_id or record.ordinary_id) then return nil end
        if not publisher.replacement_scope_ready(epoch) then return nil end
        return {owner=owner.owner,avatar=owner.avatar,inventory=owner.inventory,address=owner.address,
            primary=owner.primary,secondary=owner.secondary,target=target,id=id,
            outgoing=outgoing,outgoing_id=outgoing_id,epoch=epoch,session=record.session,peers=record.peers}
    end
    function publisher.lone_primary_ready(owner,recovery)
        if not publisher.replacement_scope_ready(recovery.epoch) then return false,'LONE_SCOPE_CHANGED' end
        local ok,why=pcall(function()
            local _,_,_,_,game_state=scope()
            character_primary(owner,game_state,'LONE_WAIT')
        end)
        return ok,not ok and tostring(why)or nil
    end
    function publisher.lone_boundary(owner,recovery)
        if disabled or suspended or recovery.attempted then return false,'LONE_UNAVAILABLE' end
        local snapshot,why=capture_checked('LONE_RESTORATION',nil,recovery)
        if not snapshot then return note(why) end
        if snapshot.owner.row~=owner.row or snapshot.owner.address~=owner.address
            or snapshot.owner.owner~=owner.owner or snapshot.owner.avatar~=owner.avatar then
            return note('CALLER_SNAPSHOT_CHANGED')
        end
        return publish('LONE_RESTORATION',snapshot)
    end
    function publisher.formation_boundary(owner)return publisher.boundary(owner,true)end
    -- Only Core's utility-first capture opts into a utility-first formation.
    function publisher.utility_first_boundary(owner)
        return publisher.boundary(owner,false,'UTILITY_FIRST_CAPTURE')
    end
    function publisher.replacement_utility_boundary(owner)
        return publisher.boundary(owner,false,'REPLACEMENT_RETAINED_UTILITY')
    end
    -- The ordinary update may finish a pending formation when primary is held.
    -- Poll at most every250ms; once formed, only explicit swap boundaries send.
    function publisher.poll(owner)
        if disabled or suspended then return false end
        local now=services.clock()
        if now<next_poll then return false end
        next_poll=now+250
        local ok,mission=pcall(scope)
        if not ok then return note(tostring(mission)) end
        update_epoch(mission)
        if not mission or not owner then return false end
        local pair=services.current_pair()
        -- current_pair also hides a temporarily down/paused pair. Keep its
        -- deduplication record; a genuinely rebuilt Core pair has a new table.
        if not pair then return false end
        if formed_pair(pair) and supported_pair(pair)
            and record.owner==owner.owner and record.avatar==owner.avatar
            and record.inventory==owner.inventory and record.utility==pair.utility
            and record.ordinary==pair.support then return true end
        return publisher.boundary(owner,false,'POLL')
    end
    -- Private diagnostics suspend automatic publication before the first update.
    -- Suspension has no resume command; it cannot become an accidental retry path.
    function publisher.suspend(reason)
        suspended=true
        emit('SUSPENDED REASON='..tostring(reason))
    end
    function publisher.reset()
        record=nil;next_poll=0;latest_guard=nil;refused_guard=nil;guard_logs=0
    end
    function publisher.status()
        local ok,mission,host=pcall(scope)
        if ok then update_epoch(mission) else mission=nil;host=nil end
        local formed=formed_pair(services.current_pair())
        return {replacement_primary=replacement_primary.status(),epoch=epoch,mission=mission,host=host,formed=formed,
            entered=entered,returned=returned,disabled=disabled,suspended=suspended,last_reason=last_reason,
            targets=formed and record.targets or nil,
            latest_guard=copy_guard(latest_guard),refused_guard=copy_guard(refused_guard)}
    end
    local ok,reason=pcall(function()
        local pe=uint(read(game+60,4),0)
        if pe<64 or pe>=4096 or read(game+pe,4)~='PE\0\0'
            or uint(read(game+pe+8,4),0)~=0x6AB3B43F
            or uint(read(game+pe+80,4),0)~=0x4744000 then error('PUBLICATION_BUILD_UNVERIFIED',0) end
        for _,pin in ipairs(pins) do
            if read(game+pin.rva,#pin.bytes)~=pin.bytes then error('PUBLICATION_STARTUP_PIN',0) end
        end
    end)
    if not ok then disabled=true;note(tostring(reason)) end
    emit('READY MODE=AUTOMATIC SCOPE=HOST_TWO_PLAYERS_TYPED_SUPPORT_PAIR ENABLED='..tostring(not disabled)..' VERSION=26')
    return publisher
end
