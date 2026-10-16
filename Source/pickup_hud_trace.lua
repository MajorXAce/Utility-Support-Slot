-- Read-only, pickup-scoped HUD diagnostic. It installs no hook and owns no
-- writes: the existing HUD/resupply update bridge calls poll while armed.
local function pickup_hud_trace(c,api,game,ptr,uint,kernel,emit,ROOT,RECORD)
 local trace,number=nil,0
 -- Ten samples including the immediate boundary, over at most five seconds.
 -- Scheduling is sparse after the early frames; missed intervals are not replayed.
 local times={25,50,100,200,400,800,1600,3200,5000}
 local function fields(bytes,index)
  if not bytes or #bytes~=40 then return 'RECORD=UNREADABLE' end
  return 'TYPE='..tostring(uint(bytes,0))..' DISPLAY_ID='..tostring(uint(bytes,4))
   ..' PREVIOUS_COUNT='..tostring(uint(bytes,16))..' PENDING='..tostring(uint(bytes,20))
   ..' CURRENT_COUNT='..tostring(uint(bytes,24))..' CURRENT_ENTITY='..tostring(uint(bytes,28))
   ..' PREVIOUS_ENTITY='..tostring(uint(bytes,32))..' ACTIVE_INDEX='..tostring(index)
 end
 -- These are existing, traced native HUD fields; no new offsets are guessed.
 -- Read the selection/quantity/dirty block together to reduce read calls.
 local function hex(bytes)
  if not bytes then return 'UNREADABLE' end
  return(bytes:gsub('.',function(b)return string.format('%02X',b:byte())end))
 end
 -- Snapshot only inline widget storage established by the native setters.
 -- Never follow text/image handles: their ownership belongs to the engine.
 -- Each read is isolated, so an unavailable widget cannot abort suppression.
 local function presentation(root)
  local widget=root+0x392828
  local function read(offset,size)
   local ok,bytes=pcall(api.read,widget+offset,size)
   if ok and bytes and #bytes==size then return bytes end
  end
  local function word(bytes,offset)
   return bytes and tostring(uint(bytes,offset)) or 'UNREADABLE'
  end
  local function opaque(bytes,offset)
   return bytes and hex(bytes:sub(offset+1,offset+8)) or 'UNREADABLE'
  end
  local name=read(0x13A8,0x158)
  local icon=read(0x1CC0,0x278)
  local prefix=read(0x7F8,0x114)
  local quantity=read(0x1A08,0x114)
  -- NAME_150 is an opaque value. Its meaning depends on the live node kind;
  -- it must not be treated as a pointer or assumed to be the rendered string.
  return ' UI_NAME_FLAGS='..word(name,0)
   ..' UI_NAME_FORMAT_HEX='..opaque(name,0x148)
   ..' UI_NAME_150_HEX='..opaque(name,0x150)
   ..' UI_ICON_FLAGS='..word(icon,0)..' UI_ICON_ID='..word(icon,0x110)
   ..' UI_ICON_RESOURCE_HEX='..opaque(icon,0x270)
   ..' UI_PREFIX_FLAGS='..word(prefix,0)..' UI_PREFIX_ID='..word(prefix,0x110)
   ..' UI_QUANTITY_FLAGS='..word(quantity,0)..' UI_QUANTITY_TEMPLATE_ID='..word(quantity,0x110)
 end
 local function seconds(bytes)
  if not bytes or #bytes~=4 then return 'UNKNOWN' end
  local bits=uint(bytes,0)
  local exponent=math.floor(bits/8388608)%256
  if exponent==255 then return 'NONFINITE' end
  local fraction=bits%8388608/8388608
  local result=exponent==0 and fraction*2^-126 or (1+fraction)*2^(exponent-127)
  if bits>=0x80000000 then result=-result end
  return string.format('%.6f',result)
 end
 local function animation(root)
  local selection=api.read(root+0x394FDC,24)
  local timer=api.read(root+0x394B44,4)
  if not selection or #selection~=24 then
   return 'SELECTION=UNREADABLE TIMER_HEX='..hex(timer)..' TIMER_SECONDS='..seconds(timer)
  end
  return 'ACTIVE_QUANTITY='..tostring(uint(selection,4))
   ..' RENDERED_QUANTITY='..tostring(uint(selection,16))
   ..' DIRTY_BYTE='..tostring(selection:byte(21))
   ..' SELECTION_HEX='..hex(selection)..' TIMER_HEX='..hex(timer)
   ..' TIMER_SECONDS='..seconds(timer)..presentation(root)
 end
 -- Observe the original native utility pickup before our partner attachment.
 -- The core supplies its freshly validated context. This callback neither
 -- arms follow-up work nor changes a weapon/HUD field.
 local function before(from,target,p)
  local pair=c.current_pair()
  if _G.UtilitySupportSlotContext~=c or not pair or pair.utility~=from
   or (pair[c.fields.support] or pair.support)~=target or pair.owner~=p.owner
   or pair[c.fields.avatar]~=p[c.fields.avatar] then return end
  local root=ptr(game+ROOT)
  if not root then return end
  local bytes=api.read(root+RECORD,40)
  local index=api.read(root+0x394FDC,4)
  emit('PICKUP_HUD_BEFORE FROM='..from..' TO='..target..' ACTIVE='..tostring(p.active)
   ..' '..fields(bytes,index and uint(index,0))..' '..animation(root))
 end
 local function cancel(reason)
  if trace then emit('PICKUP_HUD_END NUMBER='..trace.number..' REASON='..reason);trace=nil end
 end
 local function start(from,target,p,root,bytes,index)
  cancel('NEW_PICKUP')
  number=number+1
  local pair=c.current_pair()
  local support=pair and (pair[c.fields.support] or pair.support)
  if not pair or support~=target or pair.utility~=from then return end
  trace={number=number,pair=pair,owner=p.owner,avatar=p[c.fields.avatar],address=p[c.fields.address],row=p.row,
   primary=p.primary,secondary=p.secondary,from=from,target=target,
   utility_id=c.weapon_identity(from),support_id=c.weapon_identity(target),root=root,
   started=tonumber(kernel.uss_win1_GetTickCount64()),next=1}
  emit('PICKUP_HUD_BEGIN NUMBER='..number..' FROM='..from..' TO='..target
   ..' ACTIVE='..tostring(p.active)..' WINDOW_MS=5000 MAX_SAMPLES=10 BOUNDARY_SNAPSHOTS=2 ANIMATION_STATE=true PRESENTATION_READ_ONLY=true')
  emit('PICKUP_HUD_STATE NUMBER='..number..' MS=0 '..fields(bytes,index)..' '..animation(root))
 end
 local function outcome(reason)
  if trace then
   emit('PICKUP_HUD_RESULT NUMBER='..trace.number..' RESULT='..reason)
   -- Same-frame post-ack state distinguishes our history/cache writes from
   -- a later native refresh. This is one extra snapshot per pickup, not a retry.
   if ptr(game+ROOT)==trace.root then
    local bytes=api.read(trace.root+RECORD,40)
    local index=api.read(trace.root+0x394FDC,4)
    emit('PICKUP_HUD_AFTER NUMBER='..trace.number..' RESULT='..reason
     ..' '..fields(bytes,index and uint(index,0))..' '..animation(trace.root))
   end
  end
 end
 -- Capture all stages without assuming that a temporary native slot is the
 -- completed pair. Only COMPLETE arms the existing five-second read schedule.
 local function replacement(stage,p,incoming,old,kept)
  local pair=c.current_pair()
  if _G.UtilitySupportSlotContext~=c or not p or not pair or pair.owner~=p.owner
   or pair[c.fields.avatar]~=p[c.fields.avatar]then return end
  if stage=='BEGIN'then cancel('NEW_REPLACEMENT')end
  local root=ptr(game+ROOT)
  if not root then return end
  local bytes=api.read(root+RECORD,40)
  local index=api.read(root+0x394FDC,4)
  emit('REPLACEMENT_HUD_BOUNDARY STAGE='..stage..' INCOMING='..incoming
   ..' OLD='..old..' KEPT='..kept..' SUPPORT='..p[c.fields.support]..' ACTIVE='..tostring(p.active)
   ..' '..fields(bytes,index and uint(index,0))..' '..animation(root))
  if stage=='COMPLETE'then
   local support=pair[c.fields.support] or pair.support
   start(pair.utility,support,p,root,bytes,index and uint(index,0))
   if trace then trace.replacement=true;trace.normal_slot=p[c.fields.support]
    emit('PICKUP_HUD_RESULT NUMBER='..trace.number..' RESULT=REPLACEMENT_COMPLETE INCOMING='..incoming)end
  end
 end
 local function poll(now)
  local t=trace
  if not t then return end
  local elapsed=now-t.started
  if elapsed>5000 then cancel('DEADLINE');return end
  if elapsed<times[t.next]then return end
  -- No observation after another pair, avatar, death, drop or weapon swap.
  -- A different selected primary/secondary is harmless; this diagnostic only
  -- observes while the restored regular remains in the native support slot.
  local pair,p=c.current_pair(),c.sample_inventory()
  if _G.UtilitySupportSlotContext~=c or pair~=t.pair or not p or p.owner~=t.owner
   or p[c.fields.avatar]~=t.avatar or p[c.fields.address]~=t.address or p.row~=t.row
   or p.primary~=t.primary or p.secondary~=t.secondary or p[c.fields.support]~=(t.normal_slot or t.target)
   or pair.utility~=t.from or (pair[c.fields.support] or pair.support)~=t.target
   or c.weapon_identity(t.from)~=t.utility_id or c.weapon_identity(t.target)~=t.support_id then
   cancel('CONTEXT_CHANGED');return
  end
  if ptr(game+ROOT)~=t.root then cancel('HUD_ROOT_CHANGED');return end
  local bytes=api.read(t.root+RECORD,40)
  local index=api.read(t.root+0x394FDC,4)
  emit('PICKUP_HUD_STATE NUMBER='..t.number..' MS='..elapsed
   ..' ACTIVE='..tostring(p.active)..' '..fields(bytes,index and uint(index,0))..' '..animation(t.root))
  -- Advance past all overdue slots without doing several reads in one frame.
  repeat t.next=t.next+1 until t.next>#times or times[t.next]>elapsed
  if t.next>#times then cancel('COMPLETE')end
 end
 return {replacement=replacement,before=before,start=start,poll=poll,cancel=cancel,outcome=outcome,active=function()return trace~=nil end}
end
