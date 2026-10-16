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
