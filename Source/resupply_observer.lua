-- Read-only, bounded diagnostic for finding a cleaner resupply trigger.
-- Existing refill decisions and game state are unaffected by this observer.
-- Sample the local action row and the single selected pickup prompt.
-- No other players or nearby candidates are scanned by this diagnostic.
do
 local prior_install=_G.UtilitySupportSlotInstall
 local function install(c)
  if c.debug==false then return end
  if c.resupply_observer then return end
  c.resupply_observer=true
  local ok,why=pcall(function()
   local api,ffi,game=c.memory,c.ffi,c.game_base
   ffi.cdef[[short uss_win1_GetAsyncKeyState(int) __asm__("GetAsyncKeyState");]]
   local user=ffi.load('user32')
   local function ptr(a)return api.pointer(api.read(a,8))end
   local function hex(s)return(s:gsub('.',function(v)return string.format('%02x',v:byte())end))end
   local active,f8,next_sample,sequence=false,false,0,0
   local layout_checked=false
   local started,deadline,context,signature,lines=0,0,nil,nil,0
   local last_source=nil
   local selected_signature,selected_cache,selected_count=nil,{},0
   local raw_signature,raw_records=nil,0
   local attempted_key,next_attempt=nil,0
   local function stop(reason,now)
    if active then c.emit('RESUPPLY_OBSERVER_END WINDOW='..sequence..' REASON='..reason..' ELAPSED_MS='..(now-started)..' STATES='..lines)end
    active=false;context=nil;signature=nil;last_source=nil
    selected_signature=nil;selected_cache={};selected_count=0
    raw_signature=nil;raw_records=0;attempted_key=nil;next_attempt=0
   end
   local function eligible()
    local s=c.resupply_state
    if not s or not c.current_pair()then return end
    local p=c.sample_inventory()
    if not p or not c.validate_pair(p) or p.owner~=s.owner or p[c.fields.avatar]~=s.avatar
     or p[c.fields.support]~=s.support or c.weapon_identity(s.target)~=s.id then return end
    return s,p
   end
   local function same(s,t)
    return s and t and s.owner==t.owner and s.avatar==t.avatar
     and s.target==t.target and s.id==t.id and s.support==t.support
     and s.backpack==t.backpack
   end
   -- Resolve only the entity/zone already named by the local interaction.
   -- This is independent of the normal classifier's nearby-candidate cache.
   -- Failure is diagnostic; it must never publish evidence or grant ammo.
   local function exact_source(entity,zone)
    if entity==0 or entity==0xffffffff or zone>=8 then return nil,'INVALID_SOURCE' end
    local m=ptr(game+0x3326D68)
    local i=m and c.lookup(m+0x14050,entity)
    if not m or not i or i<0 or i>=16384 then return nil,'SOURCE_HASH_UNAVAILABLE' end
    local arrays=api.read(m+0x14068,16)
    local units=arrays and #arrays==16 and api.pointer(arrays:sub(1,8))
    local rows=arrays and #arrays==16 and api.pointer(arrays:sub(9,16))
    if not units or not rows then return nil,'SOURCE_ARRAYS_UNAVAILABLE' end
    local unit=ptr(units+i*8)
    local d=unit and api.read(unit,24)
    if not d or #d~=24 or c.read_u32(d,8)~=entity then return nil,'SOURCE_OWNER_UNAVAILABLE' end
    local address=rows+i*0x370+zone*0x68+0x44
    local bytes=api.read(address,4)
    if not bytes or #bytes~=4 then return nil,'SOURCE_TYPE_UNAVAILABLE' end
    local kind=c.read_u32(bytes,0)
    local scale,policy
    if c.refill_policy then scale,policy=c.refill_policy.ground(kind,entity)end
    if ptr(game+0x3326D68)~=m or api.read(m+0x14068,16)~=arrays
     or ptr(units+i*8)~=unit or api.read(unit,24)~=d or api.read(address,4)~=bytes
     or c.lookup(m+0x14050,entity)~=i then return nil,'SOURCE_RECHECK_CHANGED' end
    return {kind=kind,scale=scale,policy=policy}
   end
   -- The native prompt reader uses row+820 (entity) and +824 (zone).
   -- Its 979570 producer copies descriptor+8 and zone into those fields.
   -- Read only this selected prompt, not its candidate pointer list. A prompt
   -- is NOT completion or permission to refill; it supplies earlier type data.
   local function legacy_prompt(s)
    local m=ptr(game+0x33269A0)
    if not m then return nil,'PROMPT_MANAGER_UNAVAILABLE' end
    local i=c.lookup(m+0x28,s.avatar)
    local count_bytes=api.read(m+0x1C,4)
    local count=count_bytes and #count_bytes==4 and c.read_u32(count_bytes,0)
    if not i or i<0 or not count or count>4096 or i>=count then return nil,'PROMPT_OWNER_UNAVAILABLE' end
    local arrays=api.read(m+0x40,16)
    if not arrays or #arrays~=16 then return nil,'PROMPT_ARRAYS_UNAVAILABLE' end
    local units,rows=api.pointer(arrays:sub(1,8)),api.pointer(arrays:sub(9,16))
    if not units or not rows then return nil,'PROMPT_ARRAYS_UNAVAILABLE' end
    local unit=ptr(units+i*8)
    local d=unit and api.read(unit,24)
    if not d or #d~=24 or c.read_u32(d,8)~=s.avatar then return nil,'PROMPT_OWNER_CHANGED' end
    local at=rows+i*0x880
    local selection=api.read(at+0x818,16)
    local enabled=api.read(at+0x834,1)
    local candidates=api.read(at+0x808,4)
    if not selection or #selection~=16 or not enabled or #enabled~=1 or not candidates or #candidates~=4 then return nil,'PROMPT_UNREADABLE' end
    if ptr(game+0x33269A0)~=m or c.lookup(m+0x28,s.avatar)~=i
     or api.read(m+0x1C,4)~=count_bytes or api.read(m+0x40,16)~=arrays
     or ptr(units+i*8)~=unit or api.read(unit,24)~=d
     or api.read(at+0x818,16)~=selection or api.read(at+0x834,1)~=enabled
     or api.read(at+0x808,4)~=candidates
     or not same(eligible(),context)then return nil,'PROMPT_RECHECK_CHANGED' end
    local entity,zone=c.read_u32(selection,8),c.read_u32(selection,12)
    -- Report each gate independently. The update flag is NOT established as
    -- a visible-prompt flag; keep the conservative selection rule for now.
    -- Raw fields remain observable even when that rule rejects the snapshot.
    local raw={entity=entity,zone=zone,enabled=enabled:byte(),
     candidates=c.read_u32(candidates,0),bytes=selection,address=at}
    if entity==0 then return nil,'EMPTY_ENTITY',raw end
    if entity==0xffffffff then return nil,'ENTITY_SENTINEL',raw end
    if zone>=8 then return nil,'INVALID_ZONE',raw end
    if enabled:byte()==0 then return nil,'PROMPT_UPDATE_DISABLED',raw end
    return {entity=entity,zone=zone},nil,raw
   end
   -- 9A264C retains the best-scoring spatial record, and 9A286B stores
   -- its pointer at prompt row+0. The pointer is NOT a candidate entity/zone
   -- record: it is a20-byte spatial record, with compact-source index at+10.
   -- 9A0B67 resolves that index as manager+E000+index*12. Read only that
   -- exact source entry. The fixed E000..14000 region bounds index to2048.
   -- Empty presentation fields at+818 never block this independent path.
   local function selected_prompt(s)
    local _,why,presentation=legacy_prompt(s)
    if not presentation then return nil,why end
    local row=presentation.address
    local winner=ptr(row)
    if not winner or winner==0 then return nil,'NO_NATIVE_WINNER',presentation end
    local spatial=api.read(winner,20)
    if not spatial or #spatial~=20 then return nil,'WINNER_UNREADABLE' end
    local index=c.read_u32(spatial,16)
    if index>=2048 then return nil,'WINNER_INDEX_OUT_OF_RANGE' end
    local m=ptr(game+0x3326D68)
    if not m then return nil,'WINNER_MANAGER_UNAVAILABLE' end
    local address=m+0xE000+index*12
    local source=api.read(address,12)
    if not source or #source~=12 then return nil,'WINNER_SOURCE_UNREADABLE' end
    local _,_,fresh=legacy_prompt(s)
    if not fresh or fresh.address~=row or ptr(row)~=winner
     or api.read(winner,20)~=spatial or ptr(game+0x3326D68)~=m
     or api.read(address,8)~=source:sub(1,8)
     or not same(eligible(),context)then return nil,'WINNER_RECHECK_CHANGED' end
    local entity,zone=c.read_u32(source,0),c.read_u32(source,4)
    local raw={entity=entity,zone=zone,enabled=presentation.enabled,
     candidates=presentation.candidates,index=index,
     bytes=source:sub(1,8)..spatial:sub(17,20),
     presentation=hex(presentation.bytes)}
    if entity==0 then return nil,'EMPTY_WINNER_ENTITY',raw end
    if entity==0xffffffff then return nil,'WINNER_ENTITY_SENTINEL',raw end
    if zone>=8 then return nil,'INVALID_WINNER_ZONE',raw end
    -- The native winner supplies earlier source identity, not completion.
    -- A match remains diagnostic and never grants or publishes refill evidence.
    return {entity=entity,zone=zone},nil,raw
   end
   local function observe_selection(s,now)
    local selection,why,raw=selected_prompt(s)
    -- At most64 raw transition records per five-second window. No timer or
    -- candidate pointers are read, and an unchanged snapshot produces no line.
    if raw then
     local signature=raw.bytes..':'..raw.enabled..':'..raw.candidates
     if signature~=raw_signature and raw_records<64 then
      raw_signature=signature;raw_records=raw_records+1
      local source,reason
      if raw.entity~=0 and raw.entity~=0xffffffff and raw.zone<8 then
       source,reason=exact_source(raw.entity,raw.zone)
       local _,_,fresh=selected_prompt(s)
       if not fresh or fresh.bytes~=raw.bytes or fresh.enabled~=raw.enabled
        then source=nil;reason='RAW_PROMPT_CHANGED' end
      end
      c.emit('RESUPPLY_PROMPT_RAW WINDOW='..sequence..' ELAPSED_MS='..(now-started)
       ..' ENTITY='..raw.entity..' ZONE='..raw.zone..' UPDATE_FLAG='..raw.enabled
       ..' CANDIDATE_COUNT='..raw.candidates..' WINNER_INDEX='..tostring(raw.index)
       ..' PRESENTATION='..tostring(raw.presentation)..' RAW='..hex(raw.bytes)
       ..' GATE='..tostring(why or 'ACCEPTED')..' RAW_TYPE='..tostring(source and source.kind)
       ..' RAW_SCALE='..tostring(source and source.scale)..' RAW_POLICY='..tostring(source and source.policy)
       ..' RAW_SOURCE_REASON='..tostring(reason))
      -- A readable source behind a rejected gate is research evidence only.
      -- Never insert this raw result into the selected cache or gameplay state.
     end
    end
    local key=selection and selection.entity..':'..selection.zone
    local display=key or why
    local old=key and selected_cache[key]
    local result,reason
    if key and (key~=attempted_key or now>=next_attempt)then
     attempted_key=key;next_attempt=now+250
     result,reason=exact_source(selection.entity,selection.zone)
     -- After resolving type/policy, retain it only if the same local prompt
     -- and pair are still observed. Missing sources never extend old evidence.
     local fresh=selected_prompt(s)
     if not fresh or fresh.entity~=selection.entity or fresh.zone~=selection.zone
      then result=nil;reason='SELECTION_CHANGED' end
     if result then
      if not old and selected_count>=16 then selected_cache={};selected_count=0 end
      if not selected_cache[key]then selected_count=selected_count+1 end
      result.time=now;selected_cache[key]=result;old=result
     end
    end
    -- Successful routine refreshes keep the same diagnostic status. This
    -- avoids CLASSIFIED/RETAINED chatter without changing evidence lifetime.
    if old and (now-old.time<0 or now-old.time>1500)then old=nil end
    local status=old and 'CLASSIFIED' or 'UNAVAILABLE'
    local current=tostring(display)..':'..status..':'..tostring(old and old.kind)
     ..':'..tostring(old and old.scale)..':'..tostring(old and old.policy)
    if current~=selected_signature then
     selected_signature=current
     c.emit('RESUPPLY_SELECTED_SOURCE WINDOW='..sequence..' ELAPSED_MS='..(now-started)
      ..' ENTITY='..tostring(selection and selection.entity)..' ZONE='..tostring(selection and selection.zone)
      ..' STATUS='..status..' TYPE='..tostring(old and old.kind)
      ..' SCALE='..tostring(old and old.scale)..' POLICY='..tostring(old and old.policy)
      ..' REASON='..tostring(reason or why)..' ORIGIN=NATIVE_WINNER')
    end
   end
   local function poll(now)
    local key=tonumber(user.uss_win1_GetAsyncKeyState(0x77))<0
    if key and not f8 then
     stop('RESTART',now)
     local s=eligible()
     if s then
      if not layout_checked then
       assert(api.read(game+0xA6E921,8)=='\x4a\x8b\x84\x10\x88\xe8\x53\x00','ACTION_FLAGS_LAYOUT')
       assert(api.read(game+0xA6EA3B,8)=='\x4a\x8b\x84\x09\x80\xe8\x53\x00','ACTION_FLAGS_LAYOUT')
       -- A43100 is called by the Supply Backpack path at11B009E. Its getter
       -- reads the low32 bits of row+BB8 and returns bit21; later native
       -- checks can still reject the refill, so this bit is NOT success.
       assert(api.read(game+0xA431B1,14)=='\x42\x8b\x84\x10\x88\xe8\x53\x00\x48\xc1\xe8\x15\x24\x01','SUPPLY_ACTION_LAYOUT')
       assert(api.read(game+0x9A3590,22)=='\x8b\x86\x24\x08\x00\x00\x48\x8d\x54\x24\x78\x44\x8b\x4d\x08\x44\x8b\x86\x20\x08\x00\x00','SELECTED_PROMPT_LAYOUT')
       assert(api.read(game+0x9A286B,3)=='\x49\x89\x11','WINNER_STORE_LAYOUT')
       assert(api.read(game+0x9A0B67,34)=='\x8b\x46\x10\x4c\x8b\x1d\xf7\x61\x98\x02\x4c\x8d\xa0\x00\x1c\x00\x00\x4e\x8d\x24\x60\x43\x8b\x04\xa3\x4f\x8d\x24\xa3\x4c\x89\x64\x24\x58','WINNER_SOURCE_LAYOUT')
       layout_checked=true
      end
      sequence=sequence+1;active=true;started=now;deadline=now+5000
      context={owner=s.owner,avatar=s.avatar,target=s.target,id=s.id,
       support=s.support,backpack=s.backpack}
      lines=0;signature=nil;next_sample=now
      c.emit('RESUPPLY_OBSERVER_BEGIN WINDOW='..sequence..' DURATION_MS=5000 TARGET='..s.target..' READ_ONLY=true')
     else c.emit('RESUPPLY_OBSERVER_SKIP NO_ELIGIBLE_STORED_WEAPON')end
    end
    f8=key
    if not active then return end
    if now<started or now>=deadline then stop('COMPLETE',now);return end
    if lines>=128 then stop('STATE_LIMIT',now);return end
    if now<next_sample then return end
    next_sample=now+25
    local s=eligible()
    if not same(s,context)then stop('CONTEXT_CHANGED',now);return end
    -- Resolve only the verified local avatar through its existing hash lookup.
    -- Rechecking the descriptor, count and manager prevents mixed row identities.
    local m=ptr(game+0x3326D20)
    local i=m and c.lookup(m+0xF8,s.avatar)
    local count=m and c.read_u32(api.read(m+0x70,4),0)
    if not i or i<0 or not count or count>4096 or i>=count then stop('ACTION_UNAVAILABLE',now);return end
    -- +110 is an inline array of unit pointers, not a pointer to an array.
    local unit_slot=m+0x110+i*8
    local unit=ptr(unit_slot)
    local descriptor=unit and api.read(unit,24)
    local row=m+0x53DCD0+i*0x1238
    local owned=api.read(row,4)
    if not descriptor or #descriptor~=24 or c.read_u32(descriptor,8)~=s.avatar
     or not owned or c.read_u32(owned,0)~=s.avatar then stop('ACTION_OWNER_CHANGED',now);return end
    -- Existing interaction state (+9EC) and nearby native action flags (+BB0).
    -- A6E921 reads row+BB8; A6EA34 reads row+BB0. These are state, not
    -- proven refill sequence numbers. Do not grant ammo from these snapshots.
    local interaction=api.read(row+0x9EC,32)
    local flags=api.read(row+0xBB0,24)
    if not interaction or #interaction~=32 or not flags or #flags~=24 then stop('ACTION_UNREADABLE',now);return end
    if c.read_u32(interaction,0)~=s.avatar then stop('INTERACTION_OWNER_CHANGED',now);return end
    if ptr(game+0x3326D20)~=m or ptr(unit_slot)~=unit
     or c.lookup(m+0xF8,s.avatar)~=i or c.read_u32(api.read(m+0x70,4),0)~=count
     or api.read(unit,24)~=descriptor or api.read(row,4)~=owned
     or api.read(row+0x9EC,32)~=interaction or api.read(row+0xBB0,24)~=flags
     or not same(eligible(),context)then stop('ACTION_RECHECK_CHANGED',now);return end
    observe_selection(s,now)
    local supply_action=math.floor(c.read_u32(flags,8)/2097152)%2
    local interaction_action=math.floor(c.read_u32(flags,12)/131072)%2
    local entity,zone=c.read_u32(interaction,8),c.read_u32(interaction,12)
    local source_key=interaction_action==1 and entity~=0 and entity~=0xffffffff and entity..':'..zone or nil
    if source_key~=last_source then
     last_source=source_key
     if source_key then
      local source,reason=exact_source(entity,zone)
      local retained=selected_cache[source_key]
      local age=retained and now-retained.time
      if not age or age<0 or age>1500 then retained=nil end
      -- Source resolution must also retain the same local observation. No
      -- type result is logged as verified across a changed actor/action row.
      local fresh=eligible()
      local again=api.read(row+0x9EC,32)
      if not same(fresh,context)or ptr(game+0x3326D20)~=m or ptr(unit_slot)~=unit
       or api.read(unit,24)~=descriptor or c.lookup(m+0xF8,s.avatar)~=i
       or not again or #again~=32 or again:sub(1,4)~=interaction:sub(1,4)
       or again:sub(9,16)~=interaction:sub(9,16)then source=nil;retained=nil;reason='LOCAL_ACTION_CHANGED'end
      c.emit('RESUPPLY_EXACT_SOURCE WINDOW='..sequence..' ELAPSED_MS='..(now-started)
       ..' ENTITY='..entity..' ZONE='..zone..' STATUS='..(source and 'CLASSIFIED'or'UNAVAILABLE')
       ..' TYPE='..tostring(source and source.kind)..' SCALE='..tostring(source and source.scale)
       ..' POLICY='..tostring(source and source.policy)..' REASON='..tostring(reason)
       ..' SELECTED_MATCH='..tostring(retained~=nil)..' SELECTED_TYPE='..tostring(retained and retained.kind)
       ..' SELECTED_SCALE='..tostring(retained and retained.scale)..' SELECTED_POLICY='..tostring(retained and retained.policy)
       ..' SELECTED_AGE_MS='..tostring(retained and age))
     end
    end
    local completion=c.pickup_completion
    local evidence=completion and completion.owner==s.owner and completion.avatar==s.avatar
     and completion.target==s.target and completion.id==s.id and tostring(completion.serial)or'NONE'
    local supply=s.supply and tostring(s.supply.reserve)or'NONE'
    local signals={}
    for j=1,5 do signals[j]=tostring(s.signals and s.signals[j])end
    -- +9F0 is a continuously changing timer, demonstrated by live capture.
    -- Exclude it as well as position bytes from change detection. Include
    -- the mapped low-word supply bit even when no charge/resource changes.
    local discrete=interaction:sub(1,4)..interaction:sub(9,16)
     ..flags:sub(7,8)..flags:sub(15,16)..flags:sub(23,24)..supply_action
    local current=discrete..'|'..s.reserve..'|'..supply..'|'..evidence..'|'..table.concat(signals,',')
    if current==signature then return end
    signature=current;lines=lines+1
    c.emit('RESUPPLY_OBSERVER_STATE WINDOW='..sequence..' ELAPSED_MS='..(now-started)
     ..' ENTITY='..tostring(c.read_u32(interaction,8))..' ZONE='..tostring(c.read_u32(interaction,12))
     ..' INTERACTION='..hex(interaction)..' ACTION_FLAGS='..hex(flags)
     ..' STORED_RESERVE='..s.reserve..' SUPPLY_CHARGES='..supply
     ..' RESOURCE_COUNTS='..table.concat(signals,',')..' CLASSIFIER_SERIAL='..evidence
     ..' SUPPLY_ACTION='..supply_action..' INTERACTION_ACTION='..interaction_action)
   end
   local prior=_G.UtilitySupportSlotUpdate
   local disabled=false
   local function pack(...)return{n=select('#',...),...}end
   _G.UtilitySupportSlotUpdate=function(now,...)
    local result=prior and pack(prior(now,...))or{n=0}
    if _G.UtilitySupportSlotContext==c and not disabled then
     local good,err=pcall(poll,now)
     if not good then disabled=true;active=false;c.emit('RESUPPLY_OBSERVER_DISABLED ERROR='..tostring(err))end
    end
    return unpack(result,1,result.n)
   end
   c.emit('RESUPPLY_OBSERVER_READY VERSION=0.7.0 F8=START_5_SECOND_WINDOW READ_ONLY=true')
  end)
  if not ok then c.emit('RESUPPLY_OBSERVER_INIT_ERROR='..tostring(why))end
 end
 _G.UtilitySupportSlotInstall=function(c)
  if prior_install then prior_install(c)end
  install(c)
 end
 if _G.UtilitySupportSlotContext then install(_G.UtilitySupportSlotContext)end
end
