-- Automatic bounded source classifier. Nearby resources never trigger refill by themselves.
-- CLASSIFIER DATA FLOW: native chosen source -> completed entity/zone ->
-- one-shot serial event. Never refill just because a candidate is nearby.
-- Cache entries describe evidence, not permission to act on an arbitrary item.
-- Canonical classifier source: the assembler embeds it in the resupply entry.
-- Edit this file; do not manually maintain a second copy in the marked block.
-- Selected resources are evidence only; seeing one cannot grant ammo.
-- TERMINOLOGY / EVIDENCE BOUNDARY
-- "zone" is our label for the zero-based subrecord index carried by a native
-- interaction. It is NOT a map area and is not an established engine type name.
-- Captured instructions use zone * 0x68 and read type at +0x44 within a source
-- row of 0x370 bytes. We accept indexes 0..7; do not infer that all eight exist.
-- "selected" means the current native prompt winner. "completion" means an
-- observed local action transition. Selection alone cannot authorize a refill.
-- Entity IDs identify live instances; source_id holds eight identity bytes.
-- Cache bundles retain source policy briefly if consumption removes the entity.
-- They do not retain pointers that may become stale, or create new completions.
do
 local original_install=_G.UtilitySupportSlotInstall
-- Install once through the shared context; do not add another update hook.
 local function install_probe(c)
  if c.pickup_filter then return end
  c.pickup_filter=true
  local ok,err=pcall(function()
   local api,game,ffi=c.memory,c.game_base,c.ffi
   local read,u32,lookup=api.read,c.read_u32,c.lookup
   local fields=c.fields
   local function ptr(a)return api.pointer(read(a,8))end
   local function unhex(s)return(s:gsub('..',function(x)return string.char(tonumber(x,16))end))end
   -- Adjacent owner/row pointers are read together and rechecked as the same 16 bytes.
   -- Read adjacent owner/row pointers together. Later equality checks detect
   -- a changed table; batching alone does not make the whole operation atomic.
   local function pointer_pair(at)
    local bytes=read(at,16)
    if not bytes or #bytes~=16 then return end
    return api.pointer(bytes:sub(1,8)),api.pointer(bytes:sub(9,16)),bytes
   end
   -- Native evidence: action row 0x1238, target/zone at +9F4/+9F8; interactable zones have stride 0x68 and type +0x44.
   local pins={{0x9A286B,'498911'},
    {0x9A0B67,'8b46104c8b1df76198024c8da0001c00004e8d2460438b04a34f8d24a34c89642458'},
    {0x9A34B0,'488b42408bcb4869f180080000488b2cd848037248'},
    {0x97C378,'496bc168'},{0x97C38A,'8b4844'},
    {0x97D9C8,'4869c138120000'},{0x977110,'498b8568400100418bdc4e8b3ce0'}}
   for _,p in ipairs(pins)do assert(read(game+p[1],#p[2]/2)==unhex(p[2]),'PICKUP_LAYOUT_PIN')end
   local next_capture,serial=0,0
   local owner,avatar,target,identity=nil,nil,nil,nil
   local history,cache={}, {}
   local cached_count=0
-- Drop candidate/history state when avatar, world or stored target changes.
   local function clear()
    owner,avatar,target,identity=nil,nil,nil,nil
    history={};cache={};cached_count=0
    c.pickup_completion=nil;c.pickup_activity=nil
   end
-- Read verified local interaction state to detect a completed pickup action.
   local function action(p)
    local m=ptr(game+0x3326D20)
    local i=m and lookup(m+0xF8,p[fields.avatar])
    local count=m and u32(read(m+0x70,4),0)
    if not i or not count or count>4096 or i>=count then return end
    local unit=ptr(m+0x110+i*8)
    local descriptor=unit and read(unit,24)
    if not descriptor or u32(descriptor,8)~=p[fields.avatar]then return end
    local address=m+0x53DCD0+i*0x1238
    local owned=read(address,4)
    local a=read(address+0x9EC,16)
    if not owned or not a or u32(owned,0)~=p[fields.avatar]or u32(a,0)~=p[fields.avatar]then return end
    if ptr(game+0x3326D20)~=m or read(unit,24)~=descriptor
     or lookup(m+0xF8,p[fields.avatar])~=i then return end
    return a,address+0x9EC
   end
   -- Read only the native winner, never the candidate list or its count.
   -- The row holds a spatial-record pointer. Its +10 index selects the
   -- compact entity/zone entry; treating the pointer as an entity is wrong.
   -- Recheck all identities because these reads are not an atomic snapshot.
   local function selected(p)
    local m=ptr(game+0x33269A0)
    local count=m and u32(read(m+0x1C,4),0)
    local i=m and lookup(m+0x28,p[fields.avatar])
    if not count or count>4096 or not i or i<0 or i>=count then return end
    local units,rows,arrays=pointer_pair(m+0x40)
    if not units or not rows then return end
    local unit=ptr(units+i*8)
    local descriptor=unit and read(unit,24)
    if not descriptor or u32(descriptor,8)~=p[fields.avatar]then return end
    local at=rows+i*0x880
    local winner=ptr(at)
    local spatial=winner and read(winner,20)
    if not spatial or #spatial~=20 then return end
    local index=u32(spatial,16)
    if not index or index>=2048 then return end
    local manager=ptr(game+0x3326D68)
    local entry=manager and manager+0xE000+index*12
    local source=entry and read(entry,8)
    if not source or #source~=8 then return end
    local e,z=u32(source,0),u32(source,4)
    if e==0 or e==0xffffffff or z>=8 then return end
    if ptr(game+0x33269A0)~=m or u32(read(m+0x1C,4),0)~=count
     or read(m+0x40,16)~=arrays or ptr(units+i*8)~=unit
     or read(unit,24)~=descriptor or lookup(m+0x28,p[fields.avatar])~=i
     or ptr(at)~=winner or read(winner,20)~=spatial
     or ptr(game+0x3326D68)~=manager or read(entry,8)~=source then return end
    return e,z,winner..':'..index..':'..source
   end
-- Confirm entity/zone/type and local interaction eligibility before caching.
   -- Cached candidates still require a matching completion and time window.
   local function interact(p,e,zone,selection,completion)
    if not e or e==0 or e==0xffffffff or not zone or zone>=8 then return nil,'INVALID_TARGET_OR_ZONE' end
    -- This component's typed unit array is indexed by its entity hash result.
    -- The global owner hash is keyed by unit handles, not by entity IDs.
    local m=ptr(game+0x3326D68)
    local i=m and lookup(m+0x14050,e)
    local units,rows,arrays
    if m then units,rows,arrays=pointer_pair(m+0x14068)end
    if not m then return nil,'INTERACTION_MANAGER_UNAVAILABLE' end
    if not i or i<0 or i>=16384 then
     -- Diagnostics reuse the lookup result; no extra native reads or retries.
     local detail=c.debug and ('LOOKUP='..(i==nil and 'NO_INDEX' or 'OUT_OF_RANGE')
      ..' INDEX='..tostring(i)..' MANAGER='..tostring(m))or nil
     return nil,'INTERACTION_HASH_UNAVAILABLE',detail
    end
    if not rows then return nil,'INTERACTION_ROWS_UNAVAILABLE' end
    if not units then return nil,'INTERACTION_UNITS_UNAVAILABLE' end
    local unit=ptr(units+i*8)
    local descriptor=unit and read(unit,24)
    if not descriptor or u32(descriptor,8)~=e then return nil,'ENTITY_OWNER_MISMATCH' end
    local at=rows+i*0x370
    -- A selected entity can expose several interaction zones. Preserve their
    -- exact types before the entity disappears; never substitute the selected
    -- zone's type for the zone reported by completion. One bounded row read
    -- replaces the old four-byte type read, with the same context rechecks.
    local r=read(completion and at+zone*0x68+0x44 or at,completion and 4 or 0x370)
    if not r then return nil,'RECORD_UNREADABLE' end
    if ptr(game+0x3326D68)~=m or read(m+0x14068,16)~=arrays or ptr(units+i*8)~=unit
     or lookup(m+0x14050,e)~=i or ptr(game+0x3326B90)~=p.owner
     or read(unit,24)~=descriptor then return nil,'ENTITY_CONTEXT_CHANGED' end
    local kind=u32(r,completion and 0 or zone*0x68+0x44)
    local scale,policy_why
    if c.refill_policy then scale,policy_why=c.refill_policy.ground(kind,e)end
    local zones
    if not completion then
     zones={}
     local policies={[kind]={scale,policy_why}}
     for z=0,7 do
      local k=u32(r,z*0x68+0x44)
      if k==4 or k==7 or k==8 or k==9 then
       local policy=policies[k]
       if not policy then
        local amount,why
        if c.refill_policy then amount,why=c.refill_policy.ground(k,e)end
        policy={amount,why};policies[k]=policy
       end
       zones[z]={type=k,scale=policy[1],policy=policy[2],source_id=descriptor:sub(1,8)}
      end
     end
    end
    if ptr(game+0x3326D68)~=m or read(m+0x14068,16)~=arrays
     or ptr(units+i*8)~=unit or read(unit,24)~=descriptor
     or read(completion and at+zone*0x68+0x44 or at,completion and 4 or 0x370)~=r
     or lookup(m+0x14050,e)~=i then return nil,'POLICY_SOURCE_CHANGED' end
    local fresh=c.sample_inventory()
    if not fresh or fresh.owner~=p.owner or fresh[fields.avatar]~=p[fields.avatar]
     or fresh[fields.address]~=p[fields.address]then return nil,'PLAYER_CONTEXT_CHANGED' end
    if completion then
     -- A bounded fallback reads only the exact completed entity/zone while
     -- it is still alive. It does not scan nearby items or infer from gains.
     -- Recheck the local completion after all source/player reads.
     if action(fresh)~=completion then return nil,'COMPLETION_CHANGED' end
    else
     local se,sz,token=selected(fresh)
     if se~=e or sz~=zone or token~=selection then return nil,'SELECTED_SOURCE_CHANGED' end
    end
    return {type=kind,scale=scale,policy=policy_why,source_id=descriptor:sub(1,8),zones=zones}
   end
-- Consume each matching completion once, linking it to a recent candidate.
   -- Preserve unknown completions as activity so self-supply cannot misclassify
   -- an overlapping ground pickup as a backpack refill.
   -- A missing resupply_state means there is no eligible virtual ordinary
   -- target. Clear the old history so no event leaks across target changes.
   -- c.resupply_state is published by the refill observer for one eligible
   -- stored ordinary target. history.completed deduplicates transitions;
   -- cache[entity].zones[index] retains each verified policy separately.
   -- A shared consumed flag spends the whole source snapshot, not one zone.
   -- serial is our monotonically increasing event number, not a native serial.
   local function capture(now)
    local active=c.resupply_state
    if not active then
     if owner then clear()end
     return
    end
    if owner~=active.owner or avatar~=active.avatar or target~=active.target or identity~=active.id then
     clear();owner,avatar,target,identity=active.owner,active.avatar,active.target,active.id
    end
    local p=(c.observe_inventory or c.sample_inventory)()
    if not p or p.owner~=owner or p[fields.avatar]~=avatar or not c.validate_pair(p) then clear();return end
    local completed=action(p)
    if not completed then clear();return end
    local e,z=u32(completed,8),u32(completed,12)
    local summary=e..':'..z
    if history.completed~=summary then
     local previous=history.completed;history.completed=summary
     if previous and e~=0 and e~=0xffffffff then
      c.pickup_activity={time=now,owner=owner,avatar=avatar,target=target,id=identity}
      -- Source key combines entity and zone. Matching only an entity could
      -- confuse different interaction resources exposed by that entity.
      local bundle=cache[e]
      local known=bundle and bundle.zones[z]
      local evidence='SELECTED_CACHE'
      local failure,lookup_detail
      if c.debug then
       -- Describe evidence before fallback can replace it. A completion still
       -- needs the same validated context and source policy to grant ammo.
       c.emit('PICKUP_COMPLETION_EVIDENCE ENTITY='..e..' ZONE='..z
        ..' CACHE='..(known and 'PRESENT' or 'ABSENT')
        ..' AGE_MS='..tostring(known and now-known.time)
        ..' CONSUMED='..tostring(known and (known.consumed==true or bundle.consumed==true))
        ..' TYPE='..tostring(known and known.type))
      end
      -- This is a NEW observed completion transition, not a cache replay.
      -- A box may be used again before the next source refresh, or its
      -- previous source may have aged while the player remained nearby.
      -- In either case discard stale evidence and revalidate only this exact
      -- live entity/zone, including its source policy and completion state.
      -- An unchanged completion never reaches this branch, so fresh evidence
      -- cannot manufacture repeated grants from a held interaction.
      if not known or known.consumed or bundle.consumed or now-known.time<0 or now-known.time>1500 then
       known,failure,lookup_detail=interact(p,e,z,nil,completed)
       if known then known.time=now;evidence='LIVE_COMPLETION';bundle=nil end
      end
      if known and not known.consumed and now-known.time>=0 and now-known.time<=1500 then
       -- Mark evidence consumed before publishing its serial, so subsequent
       -- polls cannot turn the same completion into another refill.
       known.consumed=true
       -- All cached zones describe one source snapshot. A later completion
       -- must obtain fresh live evidence rather than spend a sibling zone.
       if bundle then bundle.consumed=true end
       serial=serial+1
       c.pickup_completion={serial=serial,time=now,owner=owner,avatar=avatar,target=target,id=identity,
        entity=e,zone=z,type=known.type,scale=known.scale,policy=known.policy,source_id=known.source_id,policy_time=known.time}
       c.emit('PICKUP_CLASSIFIED ENTITY='..e..' ZONE='..z..' TYPE='..known.type..' SERIAL='..serial..' EVIDENCE='..evidence..' SCALE='..tostring(known.scale)..' POLICY='..tostring(known.policy)) 
      else c.emit('PICKUP_UNCLASSIFIED ENTITY='..e..' ZONE='..z..' REASON='..tostring(failure or (known and (known.consumed and 'CONSUMED' or 'EXPIRED'))or 'UNSEEN')..(lookup_detail and ' '..lookup_detail or '')) end
     end
    end
    -- Cache only what the engine has chosen. Retain recent previous winners
    -- because a consumed pickup often vanishes before completion is sampled.
    -- A winner alone never publishes a completion or grants ammunition.
    local entity,zone,token=selected(p)
    if not entity then history.selected=nil;return end
    local key=entity..':'..zone
    local old=cache[entity]
    local changed=history.selected~=token
    history.selected=token
    if not changed and now<(history.next_source or 0)then return end
    history.next_source=now+250
    if old and old.consumed and history.completed==key then return end
    local record,cache_failure,cache_detail=interact(p,entity,zone,token)
    if c.debug then
     -- Log new selections and changed outcomes only. An unchanged failed
     -- source can be retried every 250 ms without producing repeated lines.
     local note=key..':'..tostring(record and record.type)..':'..tostring(cache_failure)..':'..tostring(cache_detail)
     if changed or history.cache_note~=note then
      c.emit('PICKUP_SOURCE_CACHE ENTITY='..entity..' ZONE='..zone
       ..' RESULT='..(record and 'VERIFIED' or 'FAILED')
       ..' TYPE='..tostring(record and record.type)
       ..' REASON='..tostring(cache_failure)..(cache_detail and ' '..cache_detail or ''))
     end
     history.cache_note=note
    end
    if record then
     for k,v in pairs(cache)do
      if now-v.time<0 or now-v.time>1500 then cache[k]=nil;cached_count=cached_count-1 end
     end
     if not cache[entity] and cached_count>=16 then cache={};cached_count=0 end
     if not cache[entity]then cached_count=cached_count+1 end
     -- Retain the selected zone even if it is not an ammo interaction, so
     -- existing non-ammo completion classification remains explicit.
     local zones=record.zones or {};zones[zone]=record
     record.zones=nil
     local types={}
     for z,entry in pairs(zones)do
      entry.time=now;types[#types+1]=z..':'..entry.type
     end
     table.sort(types)
     record={time=now,selection=token,zones=zones}
     cache[entity]=record
     if c.debug then
      local note=entity..':'..table.concat(types,',')
      if changed or history.zone_note~=note then
       c.emit('PICKUP_SOURCE_ZONES ENTITY='..entity..' SELECTED_ZONE='..zone..' TYPES='..table.concat(types,',')..' SHARED_CONSUMPTION=true')
      end
      history.zone_note=note
     end
    end
   end
   local disabled=false
-- Keep polling bounded and inactive when no stored ordinary refill target exists.
   -- Selection is polled with action state; type/policy refresh is bounded separately.
   -- The limits in this file bound CPU work and the lifetime/size of history.
   local function observe(now)
    if now<next_capture then return end
    next_capture=now+25;capture(now)
   end
   local resupply=_G.UtilitySupportSlotUpdate
   _G.UtilitySupportSlotUpdate=function(now)
    if _G.UtilitySupportSlotContext~=c then return end
    if not disabled then
     local good,problem=pcall(observe,now)
     if not good then disabled=true;clear()
      c.emit('PICKUP_FILTER_DISABLED ERROR='..tostring(problem))end
    end
    if resupply then return resupply(now)end
   end
   c.emit('PICKUP_FILTER_READY VERSION=0.7.0 AUTOMATIC=true ACTION_POLL_MS=25 SOURCE_REFRESH_MS=250 CACHE_LIMIT=16 SELECTED_SOURCE_ONLY=true NO_PAIR_SCANS=false')
  end)
  if not ok then c.emit('PICKUP_FILTER_INIT_ERROR='..tostring(err))end
 end
 _G.UtilitySupportSlotInstall=function(c)
  if original_install then original_install(c)end
  install_probe(c)
 end
 if _G.UtilitySupportSlotContext then _G.UtilitySupportSlotInstall(_G.UtilitySupportSlotContext)end
end
