-- v0.6.65: event-driven support-notification history acknowledgement.
-- The traced HUD compares its current and previous support entity IDs. A
-- difference creates +1 even when count stays one. This experiment changes
-- only the previous ID, once, at a verified mod swap boundary. An automatic
-- restoration from settled empty history also acknowledges presence count=1.
-- Existing synchronization of cached display fields allows
-- acknowledgement during an already displayed +1. Queued +1 remains guarded.
-- v0.6.50 waits for native redraw completion before cache handoff and uses
-- a bounded cheap inventory check to detect the second drop sooner.
-- It separates only a proven second drop merged into an active -2.
-- It retains the second -1 in Lua, then submits after native first expiry.
-- A guarded four-byte hold-timer write allows native expiry; no active-index,
-- dirty, animation-object or inventory writes. Presentation uses guarded native setters.
-- Read-only counterparts of the HUD's weapon resource/icon lookups. No native
-- UI functions are called by the metadata reader. The separate presentation adapter
-- uses native setters. This factory is embedded in the HUD addon below.
-- Results are scoped to this HUD root and at most the two current pair IDs.
-- USS-SOURCE-BEGIN hud_presentation_adapter.lua
-- BEGIN SHARED ADDRESS RESOLVER
-- USS-SOURCE-BEGIN address_resolver.lua
-- Resolve code locations without calling them. A pattern identifies bytes, not
-- an ABI or object layout: the caller must retain its separate build/layout gate.
-- Offsets are module-relative; ?? accepts a changing displacement byte.
local function resolve_code_addresses(api, game, definitions, uint)
 local compiled, resolved = {}, {}
 local function compile(pattern)
  local bytes, longest, anchor_offset, run = {}, '', 0, ''
  assert(#pattern > 0 and #pattern <= 8192 and #pattern % 2 == 0, 'ADDRESS_PATTERN_LENGTH')
  for offset = 0, #pattern / 2 - 1 do
   local token = pattern:sub(offset * 2 + 1, offset * 2 + 2)
   if token == '??' then bytes[offset + 1] = false; run = ''
   else
    local byte = assert(tonumber(token, 16), 'ADDRESS_PATTERN_BYTE')
    bytes[offset + 1] = byte; run = run .. string.char(byte)
    if #run > #longest then longest = run; anchor_offset = offset - #run + 1 end
   end
  end
  assert(#longest > 0, 'ADDRESS_PATTERN_EMPTY_ANCHOR')
  return {bytes = bytes, length = #pattern / 2, anchor = longest, offset = anchor_offset}
 end
 local function matches(data, start, pattern)
  if not data or start < 1 or start + pattern.length - 1 > #data then return false end
  for index, byte in ipairs(pattern.bytes) do
   if byte ~= false and data:byte(start + index - 1) ~= byte then return false end
  end
  return true
 end
 local missing, overlap = false, 0
 for index, definition in ipairs(definitions) do
  local pattern = compile(definition.pattern)
  compiled[index] = pattern; overlap = math.max(overlap, pattern.length - 1)
  -- These hints belong to the explicitly approved build. They are a fast path,
  -- not an unvalidated disk cache from another game version.
  if matches(api.read(game + definition.hint, pattern.length), 1, pattern) then
   resolved[index] = definition.hint
  else missing = true end
 end
 if not missing then return resolved, 'VALIDATED_HINTS' end
 -- Parse the actual mapped AMD64 PE image, never scan heap data or disk bytes.
 local header = api.read_module(game, 64)
 if not header or #header ~= 64 or header:sub(1, 2) ~= 'MZ' then return nil, 'ADDRESS_DOS_HEADER' end
 local pe = uint(header, 0x3C)
 if pe < 64 or pe > 1048576 then return nil, 'ADDRESS_PE_OFFSET' end
 local nt = api.read_module(game + pe, 24)
 local function word(s, offset) local a,b=s:byte(offset+1,offset+2);return a+b*256 end
 if not nt or #nt ~= 24 or nt:sub(1,4) ~= 'PE\0\0' or word(nt,4) ~= 0x8664 then
  return nil, 'ADDRESS_PE_HEADER'
 end
 local count, optional_size = word(nt,6), word(nt,20)
 if count < 1 or count > 96 or optional_size < 112 or optional_size > 4096 then
  return nil, 'ADDRESS_PE_SECTIONS'
 end
 local optional = api.read_module(game + pe + 24, 64)
 if not optional or #optional ~= 64 or word(optional,0) ~= 0x20B then return nil, 'ADDRESS_PE_OPTIONAL' end
 local image_size = uint(optional,56)
 if image_size < 4096 or image_size > 536870912 then return nil, 'ADDRESS_IMAGE_SIZE' end
 local sections = api.read_module(game + pe + 24 + optional_size, count * 40)
 if not sections or #sections ~= count * 40 then return nil, 'ADDRESS_SECTION_READ' end
 local ranges, total = {}, 0
 for index = 0, count - 1 do
  local offset = index * 40
  local start, size, flags = uint(sections,offset+12), uint(sections,offset+8), uint(sections,offset+36)
  if math.floor(flags / 0x20000000) % 2 == 1 and size > 0 then
   if start < 4096 or start + size > image_size then return nil, 'ADDRESS_SECTION_RANGE' end
   total = total + size; ranges[#ranges+1] = {start, size}
  end
 end
 if total == 0 or total > 67108864 then return nil, 'ADDRESS_SCAN_BUDGET' end
 -- Scan all patterns together; capped reads and overlap find cross-chunk
 -- matches. Only starts in the current chunk count, avoiding duplicates.
 local found = {}
 for _, range in ipairs(ranges) do
  local offset = 0
  while offset < range[2] do
   local owned = math.min(262144 - overlap, range[2] - offset)
   local length = math.min(owned + overlap, range[2] - offset)
   local data = api.read_module(game + range[1] + offset, length)
   if not data or #data ~= length then return nil, 'ADDRESS_SCAN_READ' end
   for index, pattern in ipairs(compiled) do
    local cursor = 1
    while true do
     local anchor = data:find(pattern.anchor, cursor, true)
     if not anchor then break end
     cursor = anchor + 1
     local start = anchor - pattern.offset
     if start <= owned and matches(data,start,pattern) then
      local rva = range[1] + offset + start - 1
      if found[index] and found[index] ~= rva then return nil, 'ADDRESS_AMBIGUOUS_' .. index end
      found[index] = rva
     end
    end
   end
   offset = offset + owned
  end
 end
 for index in ipairs(definitions) do
  if not found[index] then return nil, 'ADDRESS_NOT_FOUND_' .. index end
 end
 return found, 'UNIQUE_SCAN'
end
-- USS-SOURCE-END address_resolver.lua
-- END SHARED ADDRESS RESOLVER
-- Native presentation adapter, called only by the scoped pickup
-- observer after its event-specific settled checks. It installs no hook and owns no timer,
-- history, pending quantity or inventory writes. Offsets belong to the pinned
-- game build; a signature mismatch leaves the older cosmetic fallback available.
local function hud_presentation_adapter(c,api,game,ptr,uint,make_calls)
 local signatures={
  {0x1447160,'48895c241848896c24204889542410565741574883ec20'},
  {0x198BD20,'488b05e1a599014c8d45c7488d55b7488bcb4c8b88d000000041ff91380300'},
  {0x1450230,'48895c2410574883ec30498bd8488bf9450fb6c1e8b7f5ffff'},
  {0x143BF90,'4883ec284c8bd93991100100000f8480000000899110010000'},
  {0x198C430,'48895c241048896c2418565741564883ec30488b81b4130000488be9f30f1005'},
 }
 local function unhex(s)return(s:gsub('..',function(b)return string.char(tonumber(b,16))end))end
 -- Longer scan patterns distinguish functions with shared prologues. Relative
 -- operands are wildcarded only where unique; the duplicated icon setter keeps
 -- its call displacement exact. Field offsets remain intentional.
 local patterns={
  '48895c241848896c24204889542410565741574883ec20f30f10410c488bda0f2e442448488bf1',
  '488b05????????4c8d45c7488d55b7488bcb4c8b88d000000041ff91380300',
  '48895c2410574883ec30498bd8488bf9450fb6c1e8????????',
  '4883ec284c8bd93991100100000f8480000000899110010000488b91700200004885d27410e84617ebff49c7837002000000000000418b93b80000008bc283c802418983b8',
  '48895c241048896c2418565741564883ec30488b81b4130000488be9f30f1005',
 }
 local definitions={}
 for index,entry in ipairs(signatures)do
  definitions[index]={hint=entry[1],pattern=patterns[index]}
 end
 local addresses,status=resolve_code_addresses(api,game,definitions,uint)
 if not addresses then return nil,status end
 c.emit('HUD_ADDRESS_RESOLUTION MODE='..status)
 -- Save exact resolved bytes for the existing event-time integrity recheck.
 -- No scan or PE parsing occurs during HUD corrections.
 local snapshots={}
 for index,pattern in ipairs(patterns)do
  snapshots[index]=api.read(game+addresses[index],#pattern/2)
  if not snapshots[index]or #snapshots[index]~=#pattern/2 then return nil,'ADDRESS_SNAPSHOT_READ'end
 end
 local function code_matches()
  for index,bytes in ipairs(snapshots)do
   if api.read(game+addresses[index],#bytes)~=bytes then return false end
  end
  return true
 end
 -- mov rax,[rip+disp32]: the signed displacement points to the engine global.
 local displacement=uint(snapshots[2],3)
 if displacement>=2147483648 then displacement=displacement-4294967296 end
 local engine_global=game+addresses[2]+7+displacement
 -- Private, named types avoid repeated anonymous FFI type creation. Keeping
 -- uint64 values in cdata preserves localization IDs above Lua's exact range.
 local function native_calls()
  local ffi=c.ffi
  local declaration=[[
   typedef void (*uss_hud1_name_fn)(void *, uint64_t, uint64_t, uint8_t);
   typedef void (*uss_hud1_icon_fn)(void *, uint32_t);
   typedef void (*uss_hud1_layout_fn)(void *);
   typedef void (*uss_hud1_size_fn)(void *, uint64_t);
   typedef void (*uss_hud1_measure_fn)(uint64_t, uint32_t *, uint32_t *, void *);
  ]]
  if not pcall(ffi.typeof,'uss_hud1_size_fn')then
   -- The previous build may already have declared its three private types.
   if pcall(ffi.typeof,'uss_hud1_name_fn')then
    ffi.cdef(declaration:match('   typedef void %(%*uss_hud1_size_fn.*'))
   else ffi.cdef(declaration)end
  end
  local name=ffi.cast(ffi.typeof('uss_hud1_name_fn'),game+addresses[3])
  local icon=ffi.cast(ffi.typeof('uss_hud1_icon_fn'),game+addresses[4])
  local layout=ffi.cast(ffi.typeof('uss_hud1_layout_fn'),game+addresses[5])
  local size=ffi.cast(ffi.typeof('uss_hud1_size_fn'),game+addresses[1])
  local dimensions=ffi.new('uint32_t[2]')
  local packed_size=ffi.new('uint64_t[1]')
  local floats=ffi.cast('float *',packed_size)
  local template=ffi.new('uint64_t[1]')
  local label=ffi.new('uint64_t[1]')
  ffi.copy(template,unhex('fcd6f36d34c18fd0'),8)
  return {
   measure=function(bytes,dispatch,target)
    ffi.copy(label,bytes,8);dimensions[0]=0;dimensions[1]=0
    -- Native formatter ABI: identity, width output, height output, UI dispatch.
    local measure=ffi.cast(ffi.typeof('uss_hud1_measure_fn'),target)
    measure(label[0],dimensions,dimensions+1,ffi.cast('void *',dispatch))
    return tonumber(dimensions[0]),tonumber(dimensions[1])
   end,
   size=function(node,width,height)
    -- Win64 passes this float pair as one integer register, not a C struct.
    floats[0]=width;floats[1]=height
    size(ffi.cast('void *',node),packed_size[0])
   end,
   name=function(node,bytes)
    ffi.copy(label,bytes,8)
    name(ffi.cast('void *',node),template[0],label[0],0)
   end,
   icon=function(node,value)icon(ffi.cast('void *',node),value)end,
   layout=function(node)layout(ffi.cast('void *',node))end,
  }
 end
 local ok,calls=pcall(make_calls or native_calls)
 if not ok or type(calls)~='table'then return nil,'PRESENTATION_ADAPTER_UNAVAILABLE' end
 local stopped=false
 local function writable(address,size)
  local q=api.query(address)
  return q and q.readable and q.kind==0x20000
   and(q.protection==4 or q.protection==8)
   and address>=q.base and address+size<=q.base+q.size
 end
 -- Dispatch functions can live in a different loaded image from game.dll.
 -- Check the allocation's own AMD64 PE image and executable section instead
 -- of assuming that every engine service has the same module base.
 local function executable_image(address,q)
  if not address or not q or q.kind~=0x1000000 or not q.allocation
   or address<q.base or address+1>q.base+q.size
   or(q.protection~=0x20 and q.protection~=0x40)then return false end
  local function word(bytes,offset)
   local a,b=bytes:byte(offset+1,offset+2);return a+b*256
  end
  local base=q.allocation
  local dos=api.read_module(base,64)
  if not dos or #dos~=64 or dos:sub(1,2)~='MZ'then return false end
  local pe_offset=uint(dos,0x3C)
  if pe_offset<64 or pe_offset>1048576 then return false end
  local nt=api.read_module(base+pe_offset,24)
  if not nt or #nt~=24 or nt:sub(1,4)~='PE\0\0'
   or word(nt,4)~=0x8664 then return false end
  local count,optional_size=word(nt,6),word(nt,20)
  if count<1 or count>96 or optional_size<112 or optional_size>4096 then return false end
  local optional=api.read_module(base+pe_offset+24,64)
  if not optional or #optional~=64 or word(optional,0)~=0x20B then return false end
  local image_size=uint(optional,56)
  local rva=address-base
  if rva<0 or rva>=image_size then return false end
  local sections=api.read_module(base+pe_offset+24+optional_size,count*40)
  if not sections or #sections~=count*40 then return false end
  for i=0,count-1 do
   local offset=i*40
   local start=uint(sections,offset+12)
   local size=math.max(uint(sections,offset+8),uint(sections,offset+16))
   local flags=uint(sections,offset+36)
   if start+size<=image_size and rva>=start and rva<start+size
    and math.floor(flags/0x20000000)%2==1 then
    return api.read_module(address,1)~=nil
   end
  end
  return false
 end
 local function correct(root,expected,desired,unchanged)
  if stopped then return false,'PRESENTATION_DISABLED' end
  if type(expected)~='string'or #expected~=12 or type(desired)~='string'or #desired~=12
   or desired:sub(5)==string.rep('\0',8)then return false,'PRESENTATION_METADATA' end
  local widget=root+0x392828
  local name_node,icon_node=widget+0x13A8,widget+0x1CC0
  if not writable(widget,0x2900)then return false,'PRESENTATION_REGION' end
  local name=api.read(name_node,0x158)
  local icon=api.read(icon_node,0x278)
  local mode=api.read(widget+0x27CC,4)
  if not name or #name~=0x158 or not icon or #icon~=0x278
   or not mode or uint(mode,0)~=1 then return false,'PRESENTATION_LAYOUT' end
  -- Live captures prove text kind 3 and image kind 7. Refuse any other
  -- subtype instead of guessing which setter or ownership layout applies.
  if math.floor(uint(name,0)/0x40000)%16~=3
   or math.floor(uint(icon,0)/0x40000)%16~=7 then return false,'PRESENTATION_NODE_KIND' end
  if name:sub(0x151,0x158)~=expected:sub(5)
   or uint(icon,0x110)~=uint(expected,0)then return false,'PRESENTATION_NOT_EXPECTED' end
  local context=ptr(name_node+0xF8)
  local engine=ptr(engine_global)
  if not context or not engine or not ptr(context+8)
   or not ptr(engine+0x28)or not ptr(engine+0xD0)then return false,'PRESENTATION_ENGINE_CONTEXT' end
  local dispatch=ptr(engine+0xD0)
  local measure_target=dispatch and ptr(dispatch+0x338)
  -- The native formatter supplies this dispatch slot. Validate the actual
  -- owning image; the last capture proved it is outside the game.dll image.
  local executable=measure_target and api.query(measure_target)
  if not executable_image(measure_target,executable)then
   c.emit('PICKUP_PRESENTATION_MEASURE_REJECT TARGET_RVA='..tostring(measure_target and measure_target-game)
    ..' OWNER_RVA='..tostring(measure_target and executable and executable.allocation and measure_target-executable.allocation)
    ..' KIND='..tostring(executable and executable.kind)
    ..' PROTECTION='..tostring(executable and executable.protection))
   return false,'PRESENTATION_MEASURE_TARGET'
  end
  c.emit('PICKUP_PRESENTATION_MEASURE_BEGIN OWNER_RVA='..(measure_target-executable.allocation))
  -- Query the native dimensions before altering either visible widget. The
  -- formatter uses ceil(32 * width / height); no weapon-specific sizes here.
  local measured,width,height=pcall(calls.measure,desired:sub(5),dispatch,measure_target)
  if not measured then stopped=true;return false,'PRESENTATION_MEASURE_ERROR'end
  if type(width)~='number'or type(height)~='number'or width<=0 or height<=0
   or width>16384 or height>16384 then return false,'PRESENTATION_DIMENSIONS'end
  local display_width=math.ceil(32*width/height)
  if display_width<1 or display_width>2048 then return false,'PRESENTATION_ASPECT'end
  if not code_matches()or not unchanged()
   or api.read(name_node,0x158)~=name or api.read(icon_node,0x278)~=icon
   or api.read(widget+0x27CC,4)~=mode or ptr(name_node+0xF8)~=context
   or ptr(engine_global)~=engine or ptr(engine+0xD0)~=dispatch
   or ptr(dispatch+0x338)~=measure_target then return false,'PRESENTATION_RECHECK_CHANGED' end
  -- Match the native formatter's sequence. Do not prewrite fields: setters
  -- may skip their work when the value already matches. These engine calls
  -- own text formatting, texture release/invalidation and layout updates.
  -- pcall handles Lua errors; it cannot make a native access violation safe.
  c.emit('PICKUP_PRESENTATION_CALL_BEGIN WIDTH='..display_width..' HEIGHT=32 SOURCE_WIDTH='..width..' SOURCE_HEIGHT='..height)
  local good,why=pcall(function()
   calls.icon(icon_node,uint(desired,0))
   calls.size(name_node,display_width,32)
   calls.name(name_node,desired:sub(5))
   calls.layout(widget)
  end)
  if not good then stopped=true;return false,'PRESENTATION_CALL_ERROR_'..tostring(why)end
  local name_after=api.read(name_node+0x150,8)
  local icon_after=api.read(icon_node+0x110,4)
  if name_after~=desired:sub(5)or not icon_after or uint(icon_after,0)~=uint(desired,0)
   or not unchanged()then stopped=true;return false,'PRESENTATION_READBACK_CHANGED'end
  return true,'PRESENTATION_CORRECTED'
 end
 -- Interpret this small event-only adapter, following the sample's approach.
 -- This avoids filling the shared JIT cache; it is not crash protection.
 local jit_library=rawget(_G,'jit')
 if jit_library and jit_library.off then
  jit_library.off(correct,true);jit_library.off(native_calls,true)
 end
 return correct
end
-- USS-SOURCE-END hud_presentation_adapter.lua

-- USS-SOURCE-BEGIN pickup_hud_trace.lua
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
-- USS-SOURCE-END pickup_hud_trace.lua

-- USS-SOURCE-BEGIN hud_weapon_metadata.lua
local function metadata_reader(c,api,game,ptr,uint)
 local saved={}
 local function asset(table_address,id,slots,bias,stride)
  local table_ptr=ptr(table_address)
  if not table_ptr then return nil,'TABLE_POINTER' end
  -- The core reader caps an individual read at 4096 bytes. Page the fixed
  -- bucket array and stop at the match; never turn this into a world scan.
  for block=0,slots-1,256 do
   local n=math.min(256,slots-block)
   local data=api.read(table_ptr+block*16,n*16)
   if not data or #data~=n*16 then return nil,'BUCKET_READ' end
   for offset=0,n-1 do
    local i=block+offset
    local row=data:sub(offset*16+1,offset*16+16)
   if row:sub(1,8)==id then
    local index=uint(row,8)
    if not index or index>1048576 then return nil,'ROW_INDEX' end
    local address=table_ptr+(index+bias)*stride
    if ptr(table_address)~=table_ptr or api.read(table_ptr+i*16,16)~=row then return nil,'TABLE_CHANGED' end
    return address
    end
   end
  end
  return nil,'HASH_NOT_FOUND'
 end
 local function unit(entity)
  local manager=ptr(game+0x3326DC0)
  local header=manager and api.read(manager,72)
  local index=header and c.lookup(manager+0x20,entity)
  local units=header and api.pointer(header:sub(57,64))
  if not index or index<0 or index>=16384 or index>=uint(header,12)or not units then return end
  local address=ptr(units+index*8)
  local data=address and api.read(address,24)
  if not data or uint(data,8)~=entity or data:sub(1,8)~=c.weapon_identity(entity)
   or ptr(game+0x3326DC0)~=manager or api.read(manager,72)~=header
   or ptr(units+index*8)~=address or api.read(address,24)~=data then return end
  return data:sub(1,8)
 end
 return function(entity,root,pair)
  local id=unit(entity)
  local owner=ptr(game+0x346BF98)
  if not id or not owner then return nil,'WEAPON_RESOURCE_CONTEXT' end
  -- Resource pointers remain checked on cache hits. This avoids repeated
  -- bounded table reads during rapid swaps and forgets replaced pair members.
  for key in pairs(saved)do
   if key~=c.weapon_identity(pair.utility)and key~=pair[c.fields.support_id]then saved[key]=nil end
  end
  local old=saved[id]
  if old and old.root==root and old.owner==owner
   and ptr(owner+0xF126D8)==old.weapons and ptr(owner+0xF129F8)==old.icons
   and api.read(old.name_address,8)==old.bytes:sub(5,12)
   and api.read(old.icon_address,4)==old.bytes:sub(1,4)then return old.bytes,nil,old.icon_source end
  saved[id]=nil
  -- 0x4F8A50: 0x532 hash buckets, weapon rows after a 0x17C-row bias.
  local weapon,weapon_why=asset(owner+0xF126D8,id,0x532,0x17C,0x38)
  if not weapon then return nil,'WEAPON_RESOURCE_LOOKUP_'..tostring(weapon_why)end
  local name=api.read(weapon+0x30,8)
  -- The native formatter carries this eight-byte display value unchanged.
  -- It is opaque metadata, not an address to dereference. Pointer-range
  -- validation wrongly rejects high-bit identifiers. Preserve exact bytes;
  -- require a complete nonzero value instead of converting it to a number.
  if not name or #name~=8 then return nil,'WEAPON_NAME_READ_FAILED'end
  if name==string.rep('\0',8)then return nil,'WEAPON_NAME_EMPTY'end
  -- Native formatter 0x198BD99 seeds the icon from weapon metadata +8;
  -- 0xB5C310 only replaces it when an optional display component resolves.
  -- Missing override is a valid native path, not a missing weapon icon.
  local icon=api.read(weapon+8,4)
  if not icon or #icon~=4 then return nil,'WEAPON_DEFAULT_ICON_READ_FAILED'end
  local icon_address,icon_source=weapon+8,'DEFAULT'
  -- 0xB5C310: find component type 100, then its entity's unit/resource.
  local system=ptr(game+0x3326E68)
  local count_data=system and api.read(system+0x1C14,4)
  local count=count_data and uint(count_data,0)
  if not count or count>256 then return nil,'ICON_COMPONENT_LIST'end
  local list=count==0 and ''or api.read(system+0x1C18,count*16)
  if not list or #list~=count*16 then return nil,'ICON_COMPONENT_LIST'end
  for i=0,count-1 do
   local entry=list:sub(i*16+1,i*16+16)
   if uint(entry,8)==100 then
    local component=api.pointer(entry:sub(1,8))
    local index=component and c.lookup(component+0x20,entity)
    local units=component and ptr(component+0x38)
    if index and index>=0 and index<16384 and units then
     local address=ptr(units+index*8)
     local data=address and api.read(address,24)
     if data and uint(data,8)==entity then
      -- 0x502190: 16 buckets and 32-byte rows after an eight-row bias.
      local resource=asset(owner+0xF129F8,data:sub(1,8),16,8,32)
      local handle_data=resource and api.read(resource,4)
      local handle=handle_data and uint(handle_data,0)
      if handle and handle>0 and handle<1048576 then
       local display=ptr(game+0x37C7670+handle*8)
       local candidate=display and api.read(display+4,4)
       if candidate and uint(candidate,0)~=0 and ptr(units+index*8)==address
        and api.read(address,24)==data then icon,icon_address,icon_source=candidate,display+4,'OVERRIDE' end
      end
     end
    end
   end
  end
  if uint(icon,0)==0 then return nil,'WEAPON_ICON_EMPTY'end
  if ptr(game+0x346BF98)~=owner or ptr(game+0x3326E68)~=system
   or (count>0 and api.read(system+0x1C18,count*16)~=list)or unit(entity)~=id
   or api.read(weapon+0x30,8)~=name or api.read(icon_address,4)~=icon then
   return nil,'WEAPON_METADATA_CHANGED'
  end
  local bytes=icon..name
  saved[id]={root=root,owner=owner,weapons=ptr(owner+0xF126D8),icons=ptr(owner+0xF129F8),
   name_address=weapon+0x30,icon_address=icon_address,icon_source=icon_source,bytes=bytes}
  return bytes,nil,icon_source
 end
end
-- USS-SOURCE-END hud_weapon_metadata.lua

do
 local prior_install=_G.UtilitySupportSlotInstall
 local function install(c)
  if c.hud_notification_ack then return end
  c.hud_notification_ack=true
  local loader=rawget(_G,'CowboyBingusModLoader')
  local debug=c.debug~=false
  local function emit(s) c.emit(s,nil,'HUD') end
  local ok,problem=pcall(function()
   local ffi,api,game=c.ffi,c.memory,c.game_base
   pcall(ffi.cdef,'int uss_win1_WriteProcessMemory(void *,void *,const void *,size_t,size_t *) __asm__("WriteProcessMemory");')
   local kernel=ffi.load('kernel32')
   local process=kernel.uss_win1_GetCurrentProcess()
   local written=ffi.new('size_t[1]')
   local value=ffi.new('uint32_t[1]')
   local ROOT,RECORD=0x346D538,0x394CD8
   local function unhex(s)return(s:gsub('..',function(b)return string.char(tonumber(b,16))end))end
   local function ptr(a)return api.pointer(api.read(a,8))end
   local function uint(s,i)return c.read_u32(s,i)end
   -- The core resolves entries and checks dependencies on unfamiliar builds.
   -- Also check the exact observer/comparison/acknowledgement code used here.
   local signatures={
    {0x4F8AC9,'4885c074f88b48084881c17c010000486bc1384903c2c3'},
    {0x5021E4,'4885c074f88b40084883c00848c1e0054903c2c3cccccc'},
    {0xB5C43B,'8b0885c974148bc1488d0d26b2c602488b04c18b480441894d00'},
    {0x198BD73,'4585c97e6485f67460'},
    {0x198BDDC,'418b7d04498b5d08'},
    {0x198AAE9,'4183ffff0f8536010000'},
    {0x198A183,'8b4f042b4ffc8b470839470c0f84bb000000'},
    {0x198A384,'8b470441ffc78947fc8b470889470c4883c728'},
    {0x198B802,'33c0413946080f95c08907418b460885c00f84d8000000'},
    {0x198A4B9,'4180bec8270000000f84d0010000'},
    {0x198A599,'458b8eb8270000448bc64963c7488d8819010000488d0c88498d14ce498bcee8a3130000'},
    {0x198A684,'41c686c827000000'},
    {0x198AC06,'41837f14000f8578010000'},
    {0x198AD89,'488b4580448bc64589a6b4270000498bd7498986a8270000498bce458b4f14'},
    {0x198AC37,'418bb6c4270000418b9eb82700003bde'},
    {0x198AC97,'41c7861c2300000000a040'},
    {0x198ACB3,'410f2fbe1c2300000f8219030000'},
    {0x198AFDA,'f3410f10861c230000f3410f5cc3f3410f11861c230000'},
    {0x198AEB2,'488d0c8041c784ce3423000000000000'},
    {0x198AED8,'41c786b4270000ffffffff41c686bc'},
   }
   for _,entry in ipairs(signatures)do
    assert(api.read(game+entry[1],#entry[2]/2)==unhex(entry[2]),'HUD_CODE_SIGNATURE_'..string.format('%X',entry[1]))
   end
   -- Adapter setup is isolated from established HUD acknowledgement.
   local adapter_ok,correct_presentation,adapter_reason=pcall(hud_presentation_adapter,c,api,game,ptr,uint)
   if not adapter_ok then adapter_reason=tostring(correct_presentation);correct_presentation=nil end
   emit('PICKUP_PRESENTATION_READY ENABLED='..tostring(correct_presentation~=nil)..' REASON='..tostring(adapter_reason or 'SUPPORTED_SIGNATURES'))
   local resolve_metadata=metadata_reader(c,api,game,ptr,uint)
   local trace
   if debug then trace=pickup_hud_trace(c,api,game,ptr,uint,kernel,c.emit,ROOT,RECORD)
   else
    -- Tracing is diagnostic only; cosmetic/gameplay state machines stay active.
    trace={start=function()end,outcome=function()end,cancel=function()end,
     before=function()end,replacement=function()end,
     poll=function()end,active=function()return false end}
   end
   -- Diagnostic read/log exceptions must never disable working suppression.
   for _,operation in ipairs({'start','outcome','cancel'})do
    local callback=trace[operation]
    trace[operation]=function(...)
     local good,why=pcall(callback,...)
     if not good then pcall(c.emit,'PICKUP_HUD_TRACE_ERROR OP='..operation..' REASON='..tostring(why))end
    end
   end
   local disabled,sequence=false,0
   local wrong_pickup=nil
   local popup_disabled=false
   local function cancel_wrong_pickup(reason)
    if wrong_pickup then emit('PICKUP_POPUP_END REASON='..reason)end
    wrong_pickup=nil
   end
   local function arm_wrong_pickup(utility,support,p,root,reason,displayed,presented)
    local pair=c.current_pair()
    if popup_disabled or not pair or pair.utility~=utility
     or (pair[c.fields.support]or pair.support)~=support then return end
    local now=tonumber(kernel.uss_win1_GetTickCount64())
    wrong_pickup={pair=pair,owner=p.owner,avatar=p[c.fields.avatar],address=p[c.fields.address],row=p.row,
     primary=p.primary,secondary=p.secondary,normal_slot=p[c.fields.support],utility=utility,support=support,
     utility_id=c.weapon_identity(utility),support_id=c.weapon_identity(support),root=root,started=now,next=now+25,checks=0,
     displayed=displayed or support,presented=presented or utility,retained_restore=reason=='RETAINED_UTILITY_REPLACEMENT'}
    emit('PICKUP_POPUP_ARM ACTION='..reason..' UTILITY='..utility..' SUPPORT='..support
     ..' DISPLAYED='..wrong_pickup.displayed..' PRESENTED='..wrong_pickup.presented..' MAX_MS=1500 CHECK_MS=25')
   end
   local function wrong_pickup_context(h)
    local pair,p=c.current_pair(),c.sample_inventory()
    if _G.UtilitySupportSlotContext~=c or pair~=h.pair or not p or p.owner~=h.owner
     or p[c.fields.avatar]~=h.avatar or p[c.fields.address]~=h.address or p.row~=h.row
     or p.primary~=h.primary or p.secondary~=h.secondary or p[c.fields.support]~=h.normal_slot
     or pair.utility~=h.utility or (pair[c.fields.support]or pair.support)~=h.support
     or c.weapon_identity(h.utility)~=h.utility_id or c.weapon_identity(h.support)~=h.support_id
     or ptr(game+ROOT)~=h.root or not c.validate_pair(p)then return end
    local utility=c.attached_entity(h.utility,h.utility_id);local support=c.attached_entity(h.support,h.support_id)
    if not utility or not support or utility.parent~=h.avatar or support.parent~=h.avatar then return end
    return p
   end
   -- Cosmetic correction for this acquisition only: ordinary shown for a new
   -- utility, or retained utility shown for a new ordinary weapon. Verify exact
   -- pair/display history before using the existing native presentation adapter.
   -- History is never falsified to invent a label. If no adapter is available,
   -- shorten only this proven wrong popup; native exit removes it normally.
   local function follow_wrong_pickup(now)
    local h=wrong_pickup
    if not h then return end
    if popup_disabled or disabled then cancel_wrong_pickup('DISABLED');return end
    if now<h.started or now-h.started>=1500 or h.checks>=60 then cancel_wrong_pickup('DEADLINE');return end
    if now<h.next then return end
    h.next=now+25;h.checks=h.checks+1
    if not wrong_pickup_context(h)then cancel_wrong_pickup('CONTEXT_CHANGED');return end
    local bytes=api.read(h.root+RECORD,40)
    local selection=api.read(h.root+0x394FDC,24)
    if not bytes or #bytes~=40 or not selection or #selection~=24 then cancel_wrong_pickup('HUD_UNREADABLE');return end
    local delta,index=uint(bytes,20),uint(selection,0)
    if delta==0 then
     -- A multi-update replacement can finish before the HUD observes its final
     -- slot. Wait read-only within the same1500ms cap, never invent a popup.
     if not h.retained_restore then cancel_wrong_pickup('NO_PENDING_PICKUP')end
     return
    end
    if delta~=1 then cancel_wrong_pickup('NEW_NOTIFICATION');return end
    -- Dirty states can still reformat/reset hold. Wait on the HUD work only,
    -- after immediate attachment, and never write its dirty/animation fields.
    if index~=10 or selection:byte(21)~=0 then return end
    if uint(bytes,0)~=10 or uint(bytes,16)~=1 or uint(bytes,24)~=1
     or uint(bytes,28)~=h.displayed or uint(bytes,32)~=h.displayed
     or uint(selection,4)~=1 or uint(selection,16)~=1
     or uint(selection,12)~=h.displayed then return end
    local display=resolve_metadata(h.displayed,h.root,h.pair)
    if not display or bytes:sub(5,16)~=display then return end
    local timer=api.read(h.root+0x394B44,4)
    local bits=timer and #timer==4 and uint(timer,0)
    if not bits or bits<0x00800000 or bits>0x40a00000 then cancel_wrong_pickup('HOLD_NOT_ACTIVE');return end
    local address=h.root+0x394B44
    local region=api.query(address)
    if not region or not region.readable or region.kind~=0x20000
     or(region.protection~=4 and region.protection~=8)
     or address<region.base or address+4>region.base+region.size then
     cancel_wrong_pickup('NOT_WRITABLE_PRIVATE_DATA');return
    end
    if not wrong_pickup_context(h)or api.read(h.root+RECORD,40)~=bytes
     or api.read(h.root+0x394FDC,24)~=selection or api.read(address,4)~=timer then
     cancel_wrong_pickup('RECHECK_CHANGED');return
    end
    if correct_presentation then
     local corrected_display=resolve_metadata(h.presented,h.root,h.pair)
     if not corrected_display then cancel_wrong_pickup('EXPECTED_METADATA_UNAVAILABLE');return end
     local function unchanged()
      return wrong_pickup_context(h)~=nil and api.read(h.root+RECORD,40)==bytes
       and api.read(h.root+0x394FDC,24)==selection
     end
     local good,reason=correct_presentation(h.root,display,corrected_display,unchanged)
     emit('PICKUP_PRESENTATION_RESULT UTILITY='..h.utility..' SUPPORT='..h.support..' DISPLAYED='..h.displayed..' PRESENTED='..h.presented
      ..' MS='..(now-h.started)..' SUCCESS='..tostring(good)..' REASON='..tostring(reason))
     -- One call sequence per acquisition. No retry or timer shortening after
     -- a partial correction: that would hide evidence or act on a newer UI.
     wrong_pickup=nil
     return
    end
    value[0]=0;written[0]=0
    if kernel.uss_win1_WriteProcessMemory(process,ffi.cast('void *',address),value,4,written)==0 or tonumber(written[0])~=4 then
     popup_disabled=true;cancel_wrong_pickup('WRITE_FAILED');return
    end
    if api.read(address,4)~=unhex('00000000')or not wrong_pickup_context(h)
     or api.read(h.root+RECORD,40)~=bytes or api.read(h.root+0x394FDC,24)~=selection then
     popup_disabled=true;cancel_wrong_pickup('READBACK_CHANGED');return
    end
    emit('PICKUP_POPUP_SHORTENED UTILITY='..h.utility..' DISPLAYED='..h.displayed..' PRESENTED='..h.presented
     ..' MS='..(now-h.started)..' BYTES_WRITTEN=4 NATIVE_EXIT=true')
    wrong_pickup=nil
   end
   local HANDOFF_CHECK_MS,HANDOFF_WINDOW_MS=50,6500
   -- Budget the existing full window:24polls expired after only1.2s while another
   -- native notification could still be ahead of this weapon's removal message.
   local HANDOFF_MAX_CHECKS=math.ceil(HANDOFF_WINDOW_MS/HANDOFF_CHECK_MS)
   local handoff=nil
   local observation=nil
   -- Native HUD owns a coalesced per-type pending quantity. Only the exact
   -- proven two-drop state may be split into two native selections. Failures
   -- disable this queue experiment independently of working swap suppression.
   local refresh_disabled=false
   local function hex(bytes)
    return(bytes:gsub('.',function(b)return string.format('%02X',b:byte())end))
   end
   local function stop_observation(reason)
    if observation then if debug then emit('DROP_PROBE_END NUMBER='..observation.number..' REASON='..reason..' SAMPLES='..observation.samples..' FAST_CHECKS='..observation.fast_checks) end end
    observation=nil
   end
   -- One deferred drop, owned by this restoration's observation. The first
   -- native message is already visible. Only an exactly observed second -1
   -- merged into that message may be split; there is no general HUD queue.
   local function drop_context(h)
    if _G.UtilitySupportSlotContext~=c or c.current_pair()~=nil or ptr(game+ROOT)~=h.root or c.weapon_identity(h.target)~=h.id then return false end
    local p=c.sample_inventory()
    return p and p.owner==h.owner and p[c.fields.avatar]==h.avatar
     and p[c.fields.address]==h.address and p.primary==h.primary
     and p.secondary==h.secondary and p[c.fields.support]==0
   end
   local function drop_record(h,bytes,delta)
    return uint(bytes,0)==10 and uint(bytes,20)==delta
     and uint(bytes,16)==0 and uint(bytes,24)==0
     and uint(bytes,28)==h.target and uint(bytes,32)==h.target
     and bytes:sub(5,16)==h.display
   end
   local function drop_delta(h,bytes,index,dirty,new_delta,stage)
    local address=h.root+RECORD+20
    local region=api.query(address)
    if not region or not region.readable or region.kind~=0x20000
     or(region.protection~=4 and region.protection~=8)
     or address<region.base or address+4>region.base+region.size then
     refresh_disabled=true;emit('DROP_QUEUE_DISABLED NUMBER='..h.number..' REASON=NOT_WRITABLE_PRIVATE_DATA');return false
    end
    if not drop_context(h)or api.read(h.root+RECORD,40)~=bytes
     or api.read(h.root+0x394FDC,4)~=index or api.read(h.root+0x394FF0,4)~=dirty then
     if debug then emit('DROP_QUEUE_SKIP NUMBER='..h.number..' STAGE='..stage..' REASON=RECHECK_CHANGED') end ;return false
    end
    -- Four bytes only: cache, counts, identities, active index, timer and dirty
    -- byte are unchanged. Native selection/formatting/animation still own UI.
    value[0]=new_delta;written[0]=0
    if kernel.uss_win1_WriteProcessMemory(process,ffi.cast('void *',address),value,4,written)==0 or tonumber(written[0])~=4 then
     refresh_disabled=true;emit('DROP_QUEUE_DISABLED NUMBER='..h.number..' REASON=DELTA_WRITE_FAILED');return false
    end
    local after=api.read(h.root+RECORD,40)
    local expected=bytes:sub(1,20)..unhex('ffffffff')..bytes:sub(25)
    if after~=expected then
     refresh_disabled=true;emit('DROP_QUEUE_DISABLED NUMBER='..h.number..' REASON=DELTA_READBACK_FAILED');return false
    end
    -- This is a guarded heap write, not an atomic engine transaction. If the
    -- state moves during readback, disable this experiment; never roll back
    -- through an unknown newer message. Working swap acknowledgement survives.
    if not drop_context(h)then
     refresh_disabled=true;emit('DROP_QUEUE_DISABLED NUMBER='..h.number..' REASON=CONTEXT_AFTER_WRITE');return false
    end
    return true
   end
   -- Shared presentation, separate drop policy. Only relabel a settled -1
   -- for this restoration's exact outgoing/remaining identities. The native
   -- adapter owns geometry/name/icon/layout; this function owns no quantity,
   -- timer, cache or queue writes. Each stage is checked once when eligible.
   local function correct_drop_presentation(h,p,bytes,index,dirty,second)
    local key=second and 'second_presentation_done'or 'first_presentation_done'
    if h[key]or not correct_presentation or not h.outgoing_display or not h.outgoing_id then return end
    if uint(index,0)~=10 or dirty:byte(1)~=0 or uint(bytes,20)~=0xffffffff then return end
    local desired_entity=second and h.target or h.outgoing
    local expected=second and h.outgoing_display or h.display
    local desired=second and h.display or h.outgoing_display
    local selection=api.read(h.root+0x394FDC,24)
    if selection and #selection==24 and not h[key..'_logged']then
     h[key..'_logged']=true
     emit('DROP_PRESENTATION_CHECK NUMBER='..h.number..' STAGE='..(second and 'SECOND'or 'FIRST')
      ..' EXPECTED_ENTITY='..desired_entity..' RENDERED_ENTITY='..uint(selection,12)
      ..' ACTIVE_QUANTITY='..uint(selection,4)..' RENDERED_QUANTITY='..uint(selection,16))
    end
    if not selection or #selection~=24 or uint(selection,0)~=10
     or uint(selection,4)~=0xffffffff or uint(selection,16)~=0xffffffff
     or uint(selection,12)~=desired_entity or selection:byte(21)~=0 then return end
    local function owned()
     if _G.UtilitySupportSlotContext~=c or c.current_pair()~=nil or ptr(game+ROOT)~=h.root
      or c.weapon_identity(h.target)~=h.id or c.weapon_identity(h.outgoing)~=h.outgoing_id then return false end
     local current=c.sample_inventory()
     return current and current.owner==h.owner and current[c.fields.avatar]==h.avatar
      and current[c.fields.address]==h.address and current.primary==h.primary
      and current.secondary==h.secondary and current[c.fields.support]==(second and 0 or h.target)
    end
    if not owned()then return end
    local widget=h.root+0x392828
    local name=api.read(widget+0x13A8+0x150,8)
    local icon=api.read(widget+0x1CC0+0x110,4)
    if not name or #name~=8 or not icon or #icon~=4 then return end
    if name==desired:sub(5)and icon==desired:sub(1,4)then
     h[key]=true
     emit('DROP_PRESENTATION_RESULT NUMBER='..h.number..' STAGE='..(second and 'SECOND'or 'FIRST')..' REASON=ALREADY_CORRECT')
     return
    end
    -- A foreign or mixed name/icon is not this proven swap of pair labels.
    if name~=expected:sub(5)or icon~=expected:sub(1,4)then return end
    local function unchanged()
     return owned()and api.read(h.root+RECORD,40)==bytes
      and api.read(h.root+0x394FDC,24)==selection
    end
    if not unchanged()then return end
    h[key]=true
    local good,reason=correct_presentation(h.root,expected,desired,unchanged)
    emit('DROP_PRESENTATION_RESULT NUMBER='..h.number..' STAGE='..(second and 'SECOND'or 'FIRST')
     ..' ENTITY='..desired_entity..' SUCCESS='..tostring(good)..' REASON='..tostring(reason))
   end
   local function refresh_drop(h,p,bytes,index,dirty,now)
    if refresh_disabled or disabled or h.queued or not h.baseline then return end
    if p[c.fields.support]~=0 or uint(index,0)~=10 or dirty:byte(1)~=0
     or not drop_record(h,bytes,0xfffffffe)then return end
    -- Normalize -2 back to the first -1 WITHOUT requesting a redraw. The
    -- remaining weapon's cache has already been handed off, but the first
    -- rendered label remains native UI state. Retain second -1 in Lua until
    -- native expiry clears pending and the active index becomes idle.
    if not drop_delta(h,bytes,index,dirty,0xffffffff,'NORMALIZE_FIRST')then return end
    h.queued=true;h.queue_stage='WAIT_FIRST_END';h.deadline=now+20000
    h.samples=0;h.fast_checks=0;h.fast_next=now+100;h.next=now+100
    if debug then emit('DROP_QUEUE_ARM NUMBER='..h.number..' FIRST_DROPPED='..h.outgoing..' SECOND_DROPPED='..h.target
     ..' PENDING_BEFORE=-2 PENDING_AFTER=-1 BYTES_WRITTEN=4 NATIVE_REDRAW_REQUESTED=false WAIT_FOR_NATIVE_IDLE=true') end 
   end
   -- The timer is native float seconds. Shortening its hold lets the normal
   -- expiry path schedule the exit animation; we never call UI functions or
   -- alter animation jobs. Zero has identical IEEE-754 and uint32 bit patterns.
   -- Only attempt this once AFTER a later poll observes normalized -1 in both
   -- active and rendered quantities. Native quantity changes reset the timer
   -- to five seconds, so writing during normalization would be ineffective.
   local function shorten_drop_hold(h,bytes,index,dirty)
    if h.hold_attempted or uint(index,0)~=10 or dirty:byte(1)~=0
     or not drop_record(h,bytes,0xffffffff)then return end
    local quantities=api.read(h.root+0x394FE0,16)
    local timer=api.read(h.root+0x394B44,4)
    if not quantities or #quantities~=16 or not timer or #timer~=4 then return end
    if uint(quantities,0)~=0xffffffff or uint(quantities,12)~=0xffffffff then return end
    local bits=uint(timer,0)
    -- Positive finite normal float, at most the engine's five-second hold.
    -- Zero/negative already expires normally; NaN/foreign ranges are untouched.
    if bits<0x00800000 or bits>0x40a00000 then return end
    h.hold_attempted=true
    local address=h.root+0x394B44
    local region=api.query(address)
    if not region or not region.readable or region.kind~=0x20000
     or(region.protection~=4 and region.protection~=8)
     or address<region.base or address+4>region.base+region.size then
     if debug then emit('DROP_HOLD_FALLBACK NUMBER='..h.number..' REASON=NOT_WRITABLE_PRIVATE_DATA') end ;return
    end
    if not drop_context(h)or api.read(h.root+RECORD,40)~=bytes
     or api.read(h.root+0x394FDC,4)~=index or api.read(h.root+0x394FF0,4)~=dirty
     or api.read(h.root+0x394FE0,16)~=quantities or api.read(address,4)~=timer then
     if debug then emit('DROP_HOLD_FALLBACK NUMBER='..h.number..' REASON=RECHECK_CHANGED') end ;return
    end
    value[0]=0;written[0]=0
    if kernel.uss_win1_WriteProcessMemory(process,ffi.cast('void *',address),value,4,written)==0 or tonumber(written[0])~=4 then
     refresh_disabled=true;emit('DROP_QUEUE_DISABLED NUMBER='..h.number..' REASON=HOLD_WRITE_FAILED');return
    end
    if api.read(address,4)~=unhex('00000000')or not drop_context(h)
     or api.read(h.root+RECORD,40)~=bytes then
     refresh_disabled=true;emit('DROP_QUEUE_DISABLED NUMBER='..h.number..' REASON=HOLD_READBACK_CHANGED');return
    end
    if debug then emit('DROP_HOLD_SHORTENED NUMBER='..h.number..' BEFORE_BITS='..string.format('%08X',bits)
     ..' AFTER_SECONDS=0 BYTES_WRITTEN=4 NATIVE_EXPIRY_AND_EXIT=true') end 
   end
   local function follow_drop_queue(h,bytes,index,dirty,now)
    if refresh_disabled or disabled then stop_observation('QUEUE_DISABLED');return end
    local delta=uint(bytes,20);local active=uint(index,0)
    if not drop_context(h)or not drop_record(h,bytes,delta)then stop_observation('QUEUE_CONTEXT_OR_RECORD_CHANGED');return end
    if delta~=0 and delta~=0xffffffff then stop_observation('QUEUE_NEW_NOTIFICATION');return end
    if h.queue_stage=='WAIT_FIRST_END'then
     -- First presentation was settled before the second drop was queued.
     shorten_drop_hold(h,bytes,index,dirty)
     if delta~=0 or active~=0xffffffff or dirty:byte(1)~=0 then return end
     -- Initial selection at198AC06 checks nonzero pending. Expiry at198AEB2
     -- clears it and198AED8 sets idle index. Reinsert only after BOTH occur.
     -- No native event is overwritten; other record types remain untouched.
     if not drop_delta(h,bytes,index,dirty,0xffffffff,'SUBMIT_SECOND')then return end
     h.queue_stage='WAIT_SECOND_SELECTION';h.submitted=now;h.last=nil
     if debug then emit('DROP_QUEUE_SUBMITTED NUMBER='..h.number..' ENTITY='..h.target..' DELTA=-1 BYTES_WRITTEN=4 NATIVE_INITIAL_SELECTION=true') end 
    elseif active==10 and delta==0xffffffff and dirty:byte(1)==0 then
     correct_drop_presentation(h,nil,bytes,index,dirty,true)
     if debug then emit('DROP_QUEUE_SELECTED NUMBER='..h.number..' ENTITY='..h.target..' DELTA=-1 ELAPSED_SINCE_SUBMIT_MS='..(now-h.submitted)) end 
     stop_observation('SECOND_SELECTED')
    elseif delta==0 then
     -- We did not see selection. Do not repeat a write: native may have
     -- consumed the event between polls or a context transition may have won.
     stop_observation('SECOND_CLEARED_WITHOUT_OBSERVED_SELECTION')
    end
   end
   local function observe_drop(now)
    local h=observation
    if not h then return end
    if now<h.started or now>=h.deadline or h.samples>=250 or h.fast_checks>=320 then stop_observation('EXPIRED');return end
    if now<h.fast_next then return end
    h.fast_next=now+(h.queued and 100 or 25);h.fast_checks=h.fast_checks+1
    if _G.UtilitySupportSlotContext~=c or c.current_pair()~=nil then stop_observation('CONTEXT_OR_PAIR_CHANGED');return end
    -- One four-byte inventory read on the existing core poll. This old row
    -- is only a hint: every full observation and all writes still require a
    -- fresh player/root/weapon context. Recycled rows cannot authorize writes.
    local support_bytes=api.read(h.address+8,4)
    if not support_bytes or #support_bytes~=4 then stop_observation('INVENTORY_UNREADABLE');return end
    local hinted_support=uint(support_bytes,0)
    local changed=hinted_support~=h.last_support
    h.last_support=hinted_support
    if changed and hinted_support==0 and h.baseline then h.drop_since=now end
    -- Only the empty-slot transition gets up to 500 ms of faster HUD checks.
    -- Normally the full observer still runs at 100 ms. No per-frame scans.
    local urgent=not h.queued and h.drop_since and now-h.drop_since<500
    if now<h.next and not changed and not urgent then return end
    h.next=now+100;h.samples=h.samples+1
    local p=c.sample_inventory()
    if not p or p.owner~=h.owner or p[c.fields.avatar]~=h.avatar
     or p[c.fields.address]~=h.address or p.primary~=h.primary or p.secondary~=h.secondary
     or ptr(game+ROOT)~=h.root then stop_observation('PLAYER_OR_ROOT_CHANGED');return end
    local support=p[c.fields.support]
    -- Empty support is the second-drop state. New/recycled weapons cancel.
    if support~=0 and support~=h.target then stop_observation('OTHER_SUPPORT');return end
    if support==h.target and c.weapon_identity(support)~=h.id then stop_observation('IDENTITY_CHANGED');return end
    local bytes=api.read(h.root+RECORD,40)
    local index=api.read(h.root+0x394FDC,4)
    local timer=debug and api.read(h.root+0x394B44,4)or string.rep('\0',4)
    local dirty=api.read(h.root+0x394FF0,4)
    if not bytes or #bytes~=40 or not index or #index~=4
     or not timer or #timer~=4 or not dirty or #dirty~=4 then stop_observation('HUD_UNREADABLE');return end
    -- These are sampled diagnostics, not an atomic native UI transaction.
    if ptr(game+ROOT)~=h.root then stop_observation('ROOT_CHANGED');return end
    -- Timer progresses continuously; exclude it from change detection so the
    -- log stays bounded to inventory/record/active-index/dirty changes.
    local signature=bytes..index..dirty..tostring(support)..':'..tostring(p.active)
    if h.queued then follow_drop_queue(h,bytes,index,dirty,now);return end
    if signature==h.last then return end
    h.last=signature
    local pending=uint(bytes,20)
    -- Require an observed already active first-drop state with the remaining
    -- weapon still present and its cache handed off before permitting writes.
    if not h.baseline and support==h.target and uint(index,0)==10 and pending==0xffffffff
     and uint(bytes,0)==10 and uint(bytes,16)==1 and uint(bytes,24)==1
     and uint(bytes,28)==h.target and uint(bytes,32)==h.target
     and bytes:sub(5,16)==h.display and dirty:byte(1)==0 then
     h.baseline=true;if debug then emit('DROP_REFRESH_BASELINE NUMBER='..h.number..' REMAINING='..h.target) end 
    end
    if h.baseline and support==h.target then
     correct_drop_presentation(h,p,bytes,index,dirty,false)
    end
    local signed=pending>=0x80000000 and pending-0x100000000 or pending
    if debug then emit('DROP_PROBE_STATE NUMBER='..h.number..' ELAPSED_MS='..(now-h.started)
     ..' FIRST_DROPPED='..h.outgoing..' REMAINING='..h.target..' INVENTORY_SUPPORT='..support
     ..' DRAWN_SLOT='..tostring(p.active)..' ACTIVE_INDEX='..uint(index,0)
     ..' TYPE='..uint(bytes,0)..' PREVIOUS_COUNT='..uint(bytes,16)..' PENDING_SIGNED='..signed
     ..' CURRENT_COUNT='..uint(bytes,24)..' CURRENT_ENTITY='..uint(bytes,28)
     ..' PREVIOUS_ENTITY='..uint(bytes,32)..' CACHE_HEX='..hex(bytes:sub(5,16))
     ..' EXPECTED_REMAINING_CACHE_HEX='..hex(h.display)
     ..' TIMER_HEX='..hex(timer)..' DIRTY_HEX='..hex(dirty)) end 
    if pending~=0xffffffff and pending~=0xfffffffe then
     stop_observation('NOTIFICATION_CHANGED');return
    end
    if h.baseline and uint(index,0)~=10 then stop_observation('ACTIVE_MESSAGE_CHANGED');return end
    refresh_drop(h,p,bytes,index,dirty,now)
   end
   -- Only a restoration arms this bounded follow-up. The first drop must
   -- retain its cached label until the HUD has selected/rendered it. Once
   -- the observed inventory/history have advanced to the remaining member,
   -- synchronize only the cache for a possible second normal drop.
   local function cancel_handoff(reason)
    if handoff then if debug then emit('HANDOFF_CANCEL NUMBER='..handoff.number..' REASON='..reason) end end
    handoff=nil
   end
   local function handoff_context(h)
    if _G.UtilitySupportSlotContext~=c or c.current_pair()~=nil then return nil end
    local p=c.sample_inventory()
    if not p or p.owner~=h.owner or p[c.fields.avatar]~=h.avatar
     or p[c.fields.support]~=h.target or p.primary~=h.primary or p.secondary~=h.secondary
     or p[c.fields.address]~=h.address or ptr(game+ROOT)~=h.root
     or c.weapon_identity(h.target)~=h.id then return nil end
    local attached=c.attached_entity(h.target,h.id)
    if not attached or attached.parent~=h.avatar then return nil end
    return p
   end
   local function follow_handoff(now)
    local h=handoff
    if not h then return end
    if disabled then handoff=nil;return end
    if now<h.started or now>=h.deadline or h.attempts>=HANDOFF_MAX_CHECKS then cancel_handoff('EXPIRED');return end
    if now<h.next then return end
    h.next=now+HANDOFF_CHECK_MS;h.attempts=h.attempts+1
    if not handoff_context(h)then cancel_handoff('CONTEXT_CHANGED');return end
    local bytes=api.read(h.root+RECORD,40)
    local index_bytes=api.read(h.root+0x394FDC,4)
    local dirty_bytes=api.read(h.root+0x394FF0,4)
    if not bytes or #bytes~=40 or not index_bytes or not dirty_bytes or #dirty_bytes~=4 then cancel_handoff('HUD_UNREADABLE');return end
    -- Active index alone does not mean the label is final. v0.6.10 captured
    -- C4 cases with dirty=1: a queued native refresh can reread the cache.
    -- Keep the outgoing label until that refresh clears, then hand off.
    if dirty_bytes:byte(1)~=0 then
     if not h.dirty_wait then h.dirty_wait=true;if debug then emit('HANDOFF_WAIT_DIRTY NUMBER='..h.number..' TARGET='..h.target) end end
     return
    end
    if h.dirty_wait then h.dirty_wait=nil;if debug then emit('HANDOFF_DIRTY_CLEARED NUMBER='..h.number..' TARGET='..h.target) end end
    local pending=uint(bytes,20)
    -- +1 can belong to a genuine subsequent pickup. Do not relabel it.
    if pending~=0 and pending~=0xffffffff then cancel_handoff('NEW_NOTIFICATION');return end
    local shown=pending==0 or uint(index_bytes,0)==10
    local advanced=uint(bytes,0)==10 and uint(bytes,16)==1 and uint(bytes,24)==1
     and uint(bytes,28)==h.target and uint(bytes,32)==h.target
    if not shown or not advanced then return end
    if bytes:sub(5,16)==h.display then
     if debug then emit('HANDOFF_DONE NUMBER='..h.number..' TARGET='..h.target..' ALREADY_CURRENT=true') end ;if observation then observation.next=now end;handoff=nil;return
    end
    local address=h.root+RECORD+4
    local region=api.query(address)
    if not region or not region.readable or region.kind~=0x20000
     or(region.protection~=4 and region.protection~=8)
     or address<region.base or address+12>region.base+region.size then cancel_handoff('NOT_WRITABLE_PRIVATE_DATA');return end
    if not handoff_context(h)or api.read(h.root+RECORD,40)~=bytes
     or api.read(h.root+0x394FDC,4)~=index_bytes or api.read(h.root+0x394FF0,4)~=dirty_bytes then cancel_handoff('RECHECK_CHANGED');return end
    local buffer=ffi.new('uint8_t[12]')
    for i=1,12 do buffer[i-1]=h.display:byte(i)end
    written[0]=0
    if kernel.uss_win1_WriteProcessMemory(process,ffi.cast('void *',address),buffer,12,written)==0 or tonumber(written[0])~=12 then
     disabled=true;cancel_handoff('WRITE_FAILED');return
    end
    local after=api.read(h.root+RECORD,40)
    if not after or after:sub(5,16)~=h.display or after:sub(1,4)~=bytes:sub(1,4)or after:sub(17)~=bytes:sub(17)then
     disabled=true;cancel_handoff('READBACK_FAILED');return
    end
    if observation then observation.next=now end
    if debug then emit('HANDOFF_APPLIED NUMBER='..h.number..' TARGET='..h.target..' BYTES_WRITTEN=12 PENDING_PRESERVED='..pending..' ATTEMPTS='..h.attempts) end 
    handoff=nil
   end
   -- Reuse the existing core-driven resupply callback, preserving all returns.
   -- No separate update hook. The idle branch reads no HUD or player memory.
   local prior_poll=_G.UtilitySupportSlotUpdate
   local function pack(...)return{n=select('#',...),...}end
   _G.UtilitySupportSlotUpdate=function(...)
    if not handoff and not observation and not wrong_pickup and not trace.active()then
     if prior_poll then return prior_poll(...)end
     return
    end
    local results=prior_poll and pack(prior_poll(...))or{n=0}
    if handoff then
     local good,why=pcall(follow_handoff,tonumber(kernel.uss_win1_GetTickCount64()))
     if not good then disabled=true;cancel_handoff('ERROR_'..tostring(why))end
    end
    if observation then
     local good,why=pcall(observe_drop,tonumber(kernel.uss_win1_GetTickCount64()))
     if not good then stop_observation('ERROR_'..tostring(why))end
    end
    if wrong_pickup then
     local good,why=pcall(follow_wrong_pickup,tonumber(kernel.uss_win1_GetTickCount64()))
     if not good then popup_disabled=true;cancel_wrong_pickup('ERROR_'..tostring(why))end
    end
    if trace.active()then
     local good,why=pcall(trace.poll,tonumber(kernel.uss_win1_GetTickCount64()))
     if not good then trace.cancel('ERROR_'..tostring(why))end
    end
    return unpack(results,1,results.n)
   end
   local function state(bytes)
    return 'TYPE='..uint(bytes,0)..' PREVIOUS_COUNT='..uint(bytes,16)
     ..' PENDING='..uint(bytes,20)..' CURRENT_COUNT='..uint(bytes,24)
     ..' CURRENT_ENTITY='..uint(bytes,28)..' PREVIOUS_ENTITY='..uint(bytes,32)
   end
   local function pair_support(pair)return pair[c.fields.support] or pair.support end
   local function context(from,target,restore,background,pickup)
    if _G.UtilitySupportSlotContext~=c then return nil,'CONTEXT_REPLACED' end
    local p,pair=c.sample_inventory(),c.current_pair()
    if not p or not pair then return nil,'PAIR_UNVERIFIED'end
    local support=pair_support(pair)
    if restore then
     -- The native core already verified the empty slot and released item.
     -- Do not require that a consumed utility still has a live identity.
     local outgoing=target==support and pair.utility or target==pair.utility and support
     if from~=0 or not outgoing or pair.real~=outgoing
      or pair[c.fields.avatar]~=p[c.fields.avatar] or pair.owner~=p.owner
      or p[c.fields.support]~=target or(p.active~=1 and p.active~=2)then return nil,'NOT_PAIR_RESTORATION'end
     local expected=target==support and pair[c.fields.support_id] or c.weapon_identity(target)
     if not expected or c.weapon_identity(target)~=expected or c.is_utility(expected)~=(target==pair.utility)then return nil,'TARGET_IDENTITY_CHANGED'end
     local attached=c.attached_entity(target,expected)
     if not attached or attached.parent~=p[c.fields.avatar]then return nil,'TARGET_NOT_ATTACHED'end
     return p,nil,outgoing
    end
    if not c.validate_pair(p)then return nil,'PAIR_UNVERIFIED'end
    if pickup then
     -- Pair formation is not a user swap. Require the new utility -> restored
     -- regular direction and ownership of both attachments, even with primary
     -- or secondary still selected. Never suppress the opposite pickup order.
     if from~=pair.utility or target~=support or pair.real~=support
      or pair[c.fields.avatar]~=p[c.fields.avatar]or pair.owner~=p.owner
      or p[c.fields.support]~=target or(p.active~=1 and p.active~=2 and p.active~=3)then
      return nil,'NOT_UTILITY_PICKUP_RESTORATION'end
     local regular=c.weapon_identity(target);local utility=c.weapon_identity(from)
     if regular~=pair[c.fields.support_id]or c.is_utility(regular)or not utility or not c.is_utility(utility)then
      return nil,'PICKUP_IDENTITY_CHANGED'end
     local a,b=c.attached_entity(target,regular),c.attached_entity(from,utility)
     if not a or not b or a.parent~=p[c.fields.avatar]or b.parent~=p[c.fields.avatar]then
      return nil,'PICKUP_ATTACHMENT_CHANGED'end
     return p
    end
    if from==target or not((from==pair.utility and target==support)or(from==support and target==pair.utility))then return nil,'NOT_PAIR_SWAP'end
    if p[c.fields.support]~=target or (background and p.active~=1 and p.active~=2) or (not background and p.active~=3)then return nil,'TARGET_NOT_DRAWN'end
    return p
   end
   local function skip(reason)trace.outcome('SKIP_'..tostring(reason));if debug then emit('ACK_SKIP NUMBER='..sequence..' REASON='..reason) end end
   local function acknowledge(from,target,label)
    -- No update wrapper or observation schedule: only verified boundaries run.
    local restore=label=='RESTORE_REMAINING_AFTER_DROP_OR_USE'
    local background=label=='STORE_UTILITY_ON_WEAPON_CHANGE'
    local pickup=label=='STORE_NEW_UTILITY'
    if disabled or(not restore and not background and not pickup and label~='DRAW_SUPPORT'and label~='DRAW_UTILITY')then return end
    cancel_wrong_pickup('NEW_SWAP_OR_RESTORATION')
    trace.cancel('NEW_SWAP_OR_RESTORATION')
    cancel_handoff('NEW_SWAP_OR_RESTORATION')
    stop_observation('NEW_SWAP_OR_RESTORATION')
    sequence=sequence+1
    local p,why,outgoing=context(from,target,restore,background,pickup)
    if not p then if pickup then c.emit('PICKUP_HUD_SKIP REASON='..tostring(why))end;skip(why);return end
    local root=ptr(game+ROOT)
    if not root then skip('NO_HUD_ROOT');return end
    local bytes=api.read(root+RECORD,40)
    if not bytes or #bytes~=40 then skip('UNREADABLE_RECORD');return end
    if pickup then
     arm_wrong_pickup(from,target,p,root,label)
     local index_bytes=api.read(root+0x394FDC,4)
     trace.start(from,target,p,root,bytes,index_bytes and uint(index_bytes,0))
    end
    -- Never discard an already pending weapon notification or guess through
    -- stale history. Counts and both old IDs must be settled on the outgoing
    -- weapon. Leave active resource messages completely untouched.
    local count=uint(bytes,16)
    if restore then
     -- Empty history may retain the last entity. Accept only this pair's IDs
     -- or zero, with equal presence counts and no queued weapon message.
     -- The v0.6.2 run also captured eleven identical removal states: both
     -- counts zero, both IDs still the outgoing item, pending signed -1.
     -- Permit that exact state only at this verified restoration boundary.
     -- Preserve -1: it belongs to the genuine drop, not the restored item.
     local function member(v)return v==outgoing or v==target or(count==0 and v==0)end
     local pending=uint(bytes,20)
     local removal=pending==0xffffffff and count==0 and uint(bytes,24)==0
      and uint(bytes,28)==outgoing and uint(bytes,32)==outgoing
     if uint(bytes,0)~=10 or(pending~=0 and not removal)or(count~=0 and count~=1)
      or uint(bytes,24)~=count or not member(uint(bytes,28))or not member(uint(bytes,32))then
      if debug then emit('RESTORE_SKIP NUMBER='..sequence..' '..state(bytes)) end ;return
     end
    else
     local pending=uint(bytes,20)
     local index_bytes=api.read(root+0x394FDC,4)
     local active_index=index_bytes and uint(index_bytes,0)
     -- A displayed +1 has already been formatted. Matching history prevents
     -- another dirty/refresh event, while its rendered pickup stays intact.
     -- A queued +1 behind another notification is NOT safe to relabel.
     local shown_pickup=pending==1 and active_index==10
     -- Initial restoration can precede the HUD's utility observation. Accept
     -- settled history on either exact pair member, never mixed/foreign IDs.
     -- Pending +1 is preserved only when already displayed, as for real swaps.
     local observed=uint(bytes,28)
     local settled=uint(bytes,32)==observed and(observed==from or(pickup and observed==target))
     if uint(bytes,0)~=10 or count~=1 or uint(bytes,24)~=1
      or(pending~=0 and not shown_pickup)or not settled then
     -- Reuse the already-read record only when a swap is skipped. These fields
     -- distinguish an active genuine pickup from stale or partially advanced
     -- history. Active-index observation is one read at this event boundary;
     -- there is no idle HUD polling or suppression time window.
     if pickup then trace.outcome('SKIP_RECORD_NOT_SETTLED_ON_SOURCE')end
     if debug then emit('ACK_SKIP NUMBER='..sequence..' REASON=RECORD_NOT_SETTLED_ON_SOURCE ACTION='..label
      ..' FROM='..from..' TO='..target..' ACTIVE_INDEX='..tostring(active_index)..' '..state(bytes)) end ;return
     end
    end
    -- Negative notifications read the cached display fields, not history.
    -- Synchronize those with the real weapon being dropped; for swaps, with
    -- the newly drawn member. Already rendered +1 messages keep their UI.
    local display_entity=restore and uint(bytes,20)==0xffffffff and outgoing or target
    local display,display_why,icon_source
    if restore and uint(bytes,20)==0xffffffff and not c.weapon_identity(outgoing)then
     -- Consumption may destroy Silo's unit before Core restores the survivor.
     -- The native negative formatter uses its cached name/icon, not a live unit.
     -- Preserve that cache only for exact outgoing history with a nonempty display;
     -- the remaining weapon is independently validated by context() above.
     if uint(bytes,28)~=outgoing or uint(bytes,32)~=outgoing
      or uint(bytes,4)==0 or bytes:sub(9,16)==string.rep('\0',8)then
      skip('CONSUMED_DISPLAY_UNVERIFIED');return
     end
     display=bytes:sub(5,16);icon_source='NATIVE_REMOVAL_CACHE'
    else display,display_why,icon_source=resolve_metadata(display_entity,root,c.current_pair())end
    if not display then skip(display_why);return end
    local address=root+RECORD+4
    local region=api.query(address)
    -- Private committed heap data, already readable, exactly writable with no
    -- guard flag, inside one region. Do not change memory protection.
    if not region or not region.readable or region.kind~=0x20000
     or(region.protection~=4 and region.protection~=8)
     or address<region.base or address+32>region.base+region.size then skip('NOT_WRITABLE_PRIVATE_DATA');return end
    local current,reason=context(from,target,restore,background,pickup)
    if not current or current.owner~=p.owner or current[c.fields.avatar]~=p[c.fields.avatar]
     or current.row~=p.row or ptr(game+ROOT)~=root
     or api.read(root+RECORD,40)~=bytes then skip(reason or 'CONTEXT_OR_RECORD_CHANGED');return end
    -- Write each history field once and verify every other byte is unchanged.
    -- Current values, pending delta and queue are never written. Display cache
    -- is synchronized separately below using verified read-only metadata.
    -- An empty restoration needs count=1 as well as the target ID. Entity is
    -- acknowledged first; a later count-write failure leaves normal feedback.
    local function write_field(offset,new_value)
     value[0]=new_value;written[0]=0
     if kernel.uss_win1_WriteProcessMemory(process,ffi.cast('void *',root+RECORD+offset),value,4,written)==0 or tonumber(written[0])~=4 then
      disabled=true;emit('ACK_DISABLED NUMBER='..sequence..' REASON=WRITE_FAILED');return false
     end
     local after=api.read(root+RECORD,40)
     if not after or #after~=40 or uint(after,offset)~=new_value then
      disabled=true;emit('ACK_DISABLED NUMBER='..sequence..' REASON=READBACK_FAILED');return false
     end
     if after:sub(1,offset)~=bytes:sub(1,offset)or after:sub(offset+5)~=bytes:sub(offset+5)then
      disabled=true;emit('ACK_DISABLED NUMBER='..sequence..' REASON=OTHER_FIELDS_CHANGED');return false
     end
     bytes=after;return true
    end
    local cache_before=bytes
    if bytes:sub(5,16)~=display then
     local buffer=ffi.new('uint8_t[12]')
     for i=1,12 do buffer[i-1]=display:byte(i)end
     written[0]=0
     if kernel.uss_win1_WriteProcessMemory(process,ffi.cast('void *',root+RECORD+4),buffer,12,written)==0 or tonumber(written[0])~=12 then
      disabled=true;emit('ACK_DISABLED NUMBER='..sequence..' REASON=DISPLAY_WRITE_FAILED');return
     end
     local after=api.read(root+RECORD,40)
     if not after or after:sub(5,16)~=display or after:sub(1,4)~=bytes:sub(1,4)or after:sub(17)~=bytes:sub(17)then
      disabled=true;emit('ACK_DISABLED NUMBER='..sequence..' REASON=DISPLAY_READBACK_FAILED');return
     end
     bytes=after
    end
    if not write_field(32,target)then return end
    if restore and count==0 then
     current,reason=context(from,target,true)
     if not current or current.owner~=p.owner or current[c.fields.avatar]~=p[c.fields.avatar]or current.row~=p.row
      or ptr(game+ROOT)~=root or api.read(root+RECORD,40)~=bytes then
      disabled=true;emit('ACK_DISABLED NUMBER='..sequence..' REASON=RESTORE_CONTEXT_CHANGED');return
     end
     if not write_field(16,1)then return end
    end
    if restore and uint(bytes,20)==0xffffffff then
     local remaining,remaining_why=resolve_metadata(target,root,c.current_pair())
     if remaining then
      local now=tonumber(kernel.uss_win1_GetTickCount64())
      handoff={number=sequence,owner=p.owner,avatar=p[c.fields.avatar],address=p[c.fields.address],
       primary=p.primary,secondary=p.secondary,target=target,id=c.weapon_identity(target),
       root=root,display=remaining,started=now,next=now+1,deadline=now+HANDOFF_WINDOW_MS,attempts=0}
      observation={number=sequence,owner=p.owner,avatar=p[c.fields.avatar],address=p[c.fields.address],
       primary=p.primary,secondary=p.secondary,target=target,id=c.weapon_identity(target),outgoing=outgoing,outgoing_id=c.weapon_identity(outgoing),outgoing_display=display,
       root=root,display=remaining,started=now,next=now+1,deadline=now+8000,samples=0,fast_checks=0,fast_next=now+1,last_support=target}
      if debug then emit('DROP_PROBE_ARMED NUMBER='..sequence..' FIRST_DROPPED='..outgoing..' REMAINING='..target..' WINDOW_MS=8000 MAX_SAMPLES=250 FAST_CHECK_MS=25 MAX_FAST_CHECKS=320') end 
      if debug then emit('HANDOFF_ARMED NUMBER='..sequence..' TARGET='..target..' DEADLINE_MS='..HANDOFF_WINDOW_MS..' MAX_ATTEMPTS='..HANDOFF_MAX_CHECKS) end
     else if debug then emit('HANDOFF_SKIP NUMBER='..sequence..' REASON='..tostring(remaining_why)) end end
    end
    if pickup then trace.outcome('ACK_APPLIED')end
    if debug then emit('ACK_APPLIED NUMBER='..sequence..' ACTION='..label..' TO='..target..' BYTES_WRITTEN='..((restore and count==0 and 8 or 4)+(cache_before:sub(5,16)~=display and 12 or 0))..' DISPLAY_UPDATED='..tostring(cache_before:sub(5,16)~=display)..' DISPLAY_ENTITY='..display_entity..' ICON_SOURCE='..tostring(icon_source)..' PENDING_PRESERVED='..uint(bytes,20)) end 
   end
   c.replacement_hud=function(stage,p,incoming,old,kept)
    if stage=='BEGIN'then cancel_wrong_pickup('NEW_REPLACEMENT')end
    local good,why=pcall(trace.replacement,stage,p,incoming,old,kept)
    if not good then pcall(c.emit,'REPLACEMENT_HUD_TRACE_ERROR STAGE='..stage..' REASON='..tostring(why))end
    if stage=='COMPLETE'and p then
     local pair=c.current_pair();local root=ptr(game+ROOT)
     if pair and incoming==pair.utility and root and c.validate_pair(p)then
      arm_wrong_pickup(pair.utility,pair[c.fields.support]or pair.support,p,root,'UTILITY_REPLACEMENT')
     elseif pair and incoming==(pair[c.fields.support]or pair.support)
      and kept==pair.utility and old~=incoming and old~=kept and root
      and p[c.fields.support]==pair.utility and p.active==3 and c.validate_pair(p)then
      -- The actual acquisition is ordinary; Silo was only restored because pickup
      -- switching is No. Correct only a proven displayed Silo+1 to this acquisition.
      -- Never enqueue a notification, clear its delta or touch unrelated messages.
      local old_id=c.weapon_identity(old)
      local dropped=old_id and c.attached_entity(old,old_id)
      if dropped and dropped.parent==0 and not c.is_utility(old_id)then
       arm_wrong_pickup(pair.utility,incoming,p,root,'RETAINED_UTILITY_REPLACEMENT',pair.utility,incoming)
      end
     end
    end
   end
   -- Errors in diagnostics are isolated from native attachment and suppression.
   c.before_pickup_restore=function(from,target,p)
    local good,why=pcall(trace.before,from,target,p)
    if not good then pcall(c.emit,'PICKUP_HUD_BEFORE_ERROR='..tostring(why))end
   end
   -- The compact core calls this only after its attachment and selection checks
   -- succeed. Catch all experimental errors here so they cannot pause gameplay.
   c.acknowledge_inventory_change=function(from,target,label)
    local good,why=pcall(acknowledge,from,target,label)
    if not good then disabled=true;pcall(emit,'ACK_DISABLED ERROR='..tostring(why))end
   end
   emit('READY VERSION=0.7.1 BASE_GAMEPLAY=0.6.0 MODE=EVENT_DRIVEN_HISTORY_ACK RESTORATION=true EXACT_REMOVAL_STATE=true DISPLAY_CACHE_SYNC=true DISPLAYED_PICKUP_ACK=true SKIP_STATE=true RESTORATION_HANDOFF=true OBSERVATIONS=BOUNDED_DROP_RECORD SECOND_DROP_LUA_QUEUE=true NATIVE_HOLD_SHORTENING=true QUEUE_IDLE_CHECK_MS=100 HANDOFF_DIRTY_GUARD=true FAST_INVENTORY_CHECK_MS=25 F8=UNUSED')
   if debug then c.emit('PICKUP_HUD_TRACE_READY VERSION=0.7.1 READ_ONLY=true AUTO_WINDOW_MS=5000 MAX_SAMPLES=10 PRESENTATION_READ_ONLY=true LOG=UtilitySupportSlot.log')end
   c.emit('HUD_ACK_READY VERSION=0.7.1 LOG=UtilitySupportSlot.log')
  end)
  if not ok then pcall(emit,'INIT_ERROR='..tostring(problem));c.emit('HUD_ACK_INIT_ERROR='..tostring(problem))end
 end
 _G.UtilitySupportSlotInstall=function(c)
  if prior_install then prior_install(c)end
  install(c)
 end
 if _G.UtilitySupportSlotContext then install(_G.UtilitySupportSlotContext)end
end
