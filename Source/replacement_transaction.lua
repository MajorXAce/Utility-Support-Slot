-- An inventory slot and its selected weapon are different. Native pickup may
-- replace slot3 while primary/secondary stays held. Keep that stable origin
-- explicit so it uses native cleanup without changing Default Bindings toggles.
local function replacement_origin(p,pair,kept)
    if (pair.selected==1 or pair.selected==2) and pair.utility_drawn==false
        and (p.active==pair.selected or p.active==3) then return 'STANDARD_WEAPON' end
    if p.active==3 and pair.utility_drawn==(kept==pair.utility) then return 'SUPPORT' end
end
-- Native replacement transaction services. Embedded after Core helpers so the
-- closures retain their native/context guards. No memory-tool control is required.
local function replacement_transaction_services(guard_queue)
    local function must(value,why)if not value then error(why,0)end return value end
    local function parent(entity,id)
        return must(attached_entity(entity,id),'CONTROL_ENTITY_MISSING').parent
    end
    local function mapped(entity,id)
        local m=must(ptr(game+0x346BF98),'CONTROL_MAPPING')
        local i=must(lookup(m+0xF1AEB0,entity),'CONTROL_FORWARD')
        must(i<16384,'CONTROL_MAPPING_LIMIT')
        local d=must(api.read(m+0xF32F18+i*24,24),'CONTROL_DESCRIPTOR')
        must(u32(d,8)==entity and (not id or d:sub(1,8)==id),'CONTROL_IDENTITY')
        local wire=u32(d,16);must(wire<=32766,'CONTROL_WIRE')
        local r=must(lookup(m+0xF22EC8,wire),'CONTROL_REVERSE')
        must(r<16384 and api.read(m+0xF32F18+r*24,24)==d,'CONTROL_ROUNDTRIP')
        return wire
    end
    local function peers()
        local session=must(ptr(game+0x347CEF0),'CONTROL_SESSION')
        must(u32(api.read(session+0x162D8,4),0)==2,'CONTROL_PEER_COUNT')
        local self=api.read(session+0xB398,8)
        local raw=must(api.read(session+0x162E0,16),'CONTROL_PEERS')
        local a,b=raw:sub(1,8),raw:sub(9,16)
        must(a==self and b~=self and b~=string.rep(string.char(0),8),'CONTROL_PEER_ORDER')
        if b<a then a,b=b,a end
        return (a..b):gsub('.',function(c)return string.format('%02x',c:byte())end)
    end
    local s={clock=function()return tonumber(kernel.uss_win1_GetTickCount64())end,
        emit=emit,pause=function(w)if w and pair==w.pair then paused=true end end}
    s.sample=sample
    local function route()
        if guard_queue then
            local ok,why=publication.replacement_route_check()
            must(ok,why)
        end
    end
    function s.arm()
        route()
        local p=must(sample(),'CONTROL_INVENTORY')
        local state=must(publication,'CONTROL_PUBLICATION').status()
        must(state.mission and state.host and state.formed and not state.disabled
            and not state.suspended and state.targets,'CONTROL_FORMED_HOST_REQUIRED')
        must(not paused and not down and pair and check_pair(p) and can_switch(p)
            and pair.support_id==unhex('609eb048dc0b9d2e') and utility_id==SOLO_SILO_ID
            and p.support==pair.utility and pair.real==pair.utility and p.active==3,
            'CONTROL_SILO_DRAWN_REQUIRED')
        must(parent(pair.utility,utility_id)==p.avatar
            and parent(pair.support,pair.support_id)==p.avatar,'CONTROL_BASELINE_PARENTS')
        return {pair=pair,context=p,old=pair.support,old_id=pair.support_id,
            utility=pair.utility,utility_id=utility_id,selected=p.active,
            ordinary=pair.support,ordinary_id=pair.support_id,
            epoch=state.epoch,peers=peers()}
    end
    function s.seed(p)
        -- Seed only reads the intended replacement. begin() freshly checks the
        -- route before its filter restoration/primary selection/native preparation.
        local state=publication.status()
        must(state.mission and state.host and state.formed and state.targets
            and not state.disabled and not state.suspended,'REPLACEMENT_FORMED_HOST_REQUIRED')
        local incoming_id=identity(p.support)
        local replacing_utility=incoming_id and is_utility(incoming_id)
        local kept=pair and (replacing_utility and pair.support or pair.utility)
        local old=pair and (replacing_utility and pair.utility or pair.support)
        must(not paused and not down and pair and can_switch(p) and incoming_id
            and is_utility(utility_id) and not is_utility(pair.support_id)
            and pair.real==kept and replacement_origin(p,pair,kept),'REPLACEMENT_SCOPE_CHANGED')
        -- No preserves the retained weapon, not merely slot3, which will name
        -- the incoming weapon after pickup. Yes selects that incoming member.
        local pickup_setting=utility_pickup_setting()
        must(pickup_setting~=nil,'REPLACEMENT_PICKUP_SETTING_UNAVAILABLE')
        local origin=replacement_origin(p,pair,kept)
        if origin=='STANDARD_WEAPON' then
            must(p.active==(pickup_setting and 3 or pair.selected),'REPLACEMENT_PICKUP_SELECTION_CHANGED')
        end
        return {pair=pair,context=p,old=old,
            old_id=replacing_utility and utility_id or pair.support_id,
            kept=kept,kept_id=replacing_utility and pair.support_id or utility_id,
            utility=pair.utility,utility_id=utility_id,
            ordinary=pair.support,ordinary_id=pair.support_id,
            replacing_utility=replacing_utility,new_id=incoming_id,
            selected=p.active,origin=origin,restore_kept=pickup_setting==false,
            epoch=state.epoch,peers=peers()}
    end
    function s.begin(w,p)
        route()
        -- Historical private controls replace ordinary from a drawn utility.
        -- Normalize their work record without changing that archived protocol.
        w.kept=w.kept or w.utility;w.kept_id=w.kept_id or w.utility_id
        w.ordinary=w.ordinary or w.old;w.ordinary_id=w.ordinary_id or w.old_id
        w.new_id=w.new_id or w.old_id
        must(pair==w.pair and same_context(w.context,p) and can_switch(p)
            and p.support~=w.old and p.support~=w.kept
            and identity(p.support)==w.new_id and pair.real==w.kept
            and is_utility(w.new_id)==is_utility(w.old_id)
            and is_utility(w.kept_id)~=is_utility(w.new_id)
            and parent(w.kept,w.kept_id)==0
            and parent(w.old,w.old_id)==p.avatar
            and parent(p.support,w.new_id)==p.avatar,'CONTROL_REPLACEMENT_SCOPE')
        -- Archived private controls keep their original drawn-support contract.
        -- Public seeds explicitly identify a stable standard-weapon origin.
        must(w.origin=='STANDARD_WEAPON' and p.active==w.selected
            or w.origin~='STANDARD_WEAPON' and p.active==3,'CONTROL_PICKUP_SELECTION')
        w.new=p.support;w.selected=p.active
        w.wires={avatar=mapped(p.avatar),primary=mapped(p.primary),
            utility=mapped(w.utility,w.utility_id),kept=mapped(w.kept,w.kept_id),old=mapped(w.old,w.old_id),
            incoming=mapped(w.new,w.new_id)}
        -- Clear only the lifetime dedup record, never budgets/uncertain disable.
        filter_restore()
        publication.reset()
        must(native_guard(),'CONTROL_NATIVE_GUARD')
        w.primary_requested_at=s.clock()
        must(p.active==1 or choose(p,1),'CONTROL_PRIMARY_PREPARE')
        w.primary_deadline=s.clock()+2000
    end
    function s.validate(w,p,phase)
        must(publication.replacement_scope_ready(w.epoch),'CONTROL_MISSION_CHANGED')
        local state=publication.status()
        must(peers()==w.peers,'CONTROL_PEERS_CHANGED')
        must(state.mission and state.host and state.epoch==w.epoch
            and not state.disabled and not state.suspended,'CONTROL_MISSION_CHANGED')
        must(not paused and not down and pair==w.pair and same_context(w.context,p)
            and can_switch(p) and pair.utility==w.utility
            and pair.support==w.ordinary and pair.support_id==w.ordinary_id
            and utility_id==w.utility_id and identity(w.ordinary)==w.ordinary_id
            and identity(w.utility)==w.utility_id,'CONTROL_CONTEXT_CHANGED')
        if phase=='ARMED' then return end
        must(p.active==1 and identity(w.new)==w.new_id
            and mapped(w.new,w.new_id)==w.wires.incoming
            and parent(w.new,w.new_id)==p.avatar,'CONTROL_NEW_OR_SELECTION_CHANGED')
        local expected=phase=='WAIT_PRIMARY' and w.new or phase=='UTILITY' and w.kept
            or phase=='OLD' and w.old or phase=='DROPPED' and u32(api.read(game+0x348456C,4),0)
        must(expected and p.support==expected,'CONTROL_SLOT_CHANGED')
        must(parent(w.kept,w.kept_id)==(phase=='WAIT_PRIMARY' and 0 or p.avatar)
            and parent(w.old,w.old_id)==(phase=='DROPPED' and 0 or p.avatar),'CONTROL_PARENTS_CHANGED')
    end
    s.ready=function(p)return publication.replacement_primary_ready(p)end
    function s.recover(w)
        route()
        local label=is_utility(w.kept_id) and 'REPLACE_RECOVER_UTILITY' or 'REPLACE_RECOVER_ORDINARY'
        must(switch_entity(sample(),w.kept,w.kept_id,false,label),'CONTROL_KEPT_FAILED')
        local snap,why=publication.replacement_control_capture()
        must(snap,why);must(snap.peer_key and snap.wires[w.replacing_utility and 'utility' or 'ordinary']==w.wires.old,
            'CONTROL_CAPTURE_CHANGED')
        w.peer_key=snap.peer_key
    end
    function s.represent(w)
        route()
        must(switch_entity(sample(),w.old,w.old_id,false,'REPLACE_RESTORE_ORDINARY'),
            'CONTROL_OLD_REPRESENT_FAILED')
    end
    function s.drop(w)
        -- Both fresh control captures validate the drop route, message policy
        -- and native pins. Do not precede them with a third copy of that pass.
        local before,why=publication.replacement_control_capture()
        must(before,why)
        must(before.owner.support==w.old and before.peer_key==w.peer_key,'CONTROL_DROP_TARGET')
        s.validate(w,sample(),'OLD')
        local after,why=publication.replacement_control_capture()
        must(after,why)
        must(before.fingerprint==after.fingerprint and before.key==after.key
            and before.thread==after.thread and after.owner.support==w.old
            and after.peer_key==w.peer_key,'CONTROL_DROP_ENTRY_CHANGED')
        must(native_guard(),'CONTROL_DROP_NATIVE_GUARD')
        -- One native local slot drop plus its own explicit publication; consume
        -- before entry. No pure sender, receiver call or raw inventory write.
        w.drop_entered=true
        local native=ffi.cast('void (*)(void*,uint32_t,uint32_t,uint32_t)',game+0x9AF090)
        native(ffi.cast('void*',after.owner.inventory),after.owner.avatar,3,1)
        replacement_drop_calls=replacement_drop_calls+1
        s.validate(w,sample(),'DROPPED')
    end
    function s.finish(w)
        route()
        s.validate(w,sample(),'DROPPED')
        if w.replacing_utility then pair.utility=w.new;utility_id=w.new_id
        else pair.support=w.new;pair.support_id=w.new_id end
        pair.real=w.new
        pair.drop_attempted=nil;pair.death_since=nil
        publication.reset()
        local label=w.replacing_utility and 'REPLACE_RECOVER_UTILITY' or 'REPLACE_RESTORE_ORDINARY'
        must(switch_entity(sample(),w.new,w.new_id,false,label),
            'CONTROL_FINAL_RESTORE_FAILED')
        local p=must(sample(),'CONTROL_FINAL_INVENTORY')
        must(same_context(w.context,p) and check_pair(p) and p.support==w.new
            and parent(w.old,w.old_id)==0,'CONTROL_FINAL_CONTEXT')
        if w.restore_kept then
            -- Reuse the normal guarded draw/publication path after new-pair
            -- formation. Do not draw the incoming weapon just to select slot3.
            local draw=w.selected==3
            local restore_label
            if draw then restore_label=is_utility(w.kept_id) and 'DRAW_UTILITY' or 'DRAW_SUPPORT'
            else
                -- These existing attachment labels publish incoming membership
                -- at primary without drawing slot3. Restore secondary afterward.
                restore_label=is_utility(w.kept_id) and 'REPLACE_RECOVER_UTILITY' or 'REPLACE_RECOVER_ORDINARY'
            end
            must(switch_entity(p,w.kept,w.kept_id,draw,restore_label),
                'REPLACEMENT_RESTORE_KEPT_FAILED')
            local final=must(sample(),'REPLACEMENT_FINAL_INVENTORY')
            if not draw then
                must(same_context(w.context,final) and check_pair(final)
                    and final.support==w.kept and final.active==1
                    and parent(w.old,w.old_id)==0,'REPLACEMENT_RESTORED_MEMBER_CONTEXT')
                must(w.selected==1 or choose(final,w.selected),'REPLACEMENT_RESTORE_SELECTION_FAILED')
                final=must(sample(),'REPLACEMENT_SELECTED_INVENTORY')
            end
            must(same_context(w.context,final) and check_pair(final)
                and final.support==w.kept and final.active==w.selected
                and parent(w.old,w.old_id)==0,'REPLACEMENT_FINAL_KEPT_CONTEXT')
            pair.real=w.kept
        else
            must(p.active==w.selected or choose(p,w.selected),'CONTROL_FINAL_SELECTION')
        end
        pair.selected=w.selected;pair.utility_drawn=pair.real==pair.utility and w.selected==3
        last_support={entity=pair.support,id=pair.support_id}
        trace_replacement('COMPLETE',nil,w.new,w.old,w.kept)
    end
    return s
end
