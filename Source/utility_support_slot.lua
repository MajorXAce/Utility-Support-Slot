-- HD2-Addon: mods/local/utility_support_slot
--[[
READING ORDER / EXECUTION MODEL
1. Construct the read API, identify the build and validate required dependencies.
2. Resolve the local avatar/inventory with sample(), not another player's data.
3. Discover weapon identities, then form or maintain one typed pair.
4. Read input edges and held-weapon state; attach/select only after fresh checks.
5. Synchronize the stored member's ping helper and observe possible refills.
6. On drop/death/context change, restore temporary state and release the pair.

STATE TO KEEP IN MIND
  pair == nil: no established virtual pair; ordinary gameplay continues.
  last_support: last relevant ordinary entity remembered for ordinary-first pickup.
  pending_utility: verified utility remembered for utility-first pickup.
  pair.utility: utility entity handle (designator, detonator, or future utility).
  utility_id: binary identity hash of that utility, not an entity handle.
  pair.support / pair.support_id: ordinary entity and its identity hash.
  pair.real: last observed member occupying the game's normal support slot.
  p.support: fresh normal-slot value; p.active: selected slot, 1/2/3.
  paused: stop uncertain core actions until a context reset.
  down: inventory is in the death/empty-standard-weapon path.
These are roles, not two native inventory slots. Visible attachment alone does
not prove normal-slot membership, identity, ownership or interaction safety.

HOW TO READ THE LOW-LEVEL EXPRESSIONS
  game + offset is an address relative to the loaded game.dll base (an RVA).
  manager + index * stride addresses a record in a mapped game array.
  read(address, length) returns copied bytes or nil on a failed/short read.
  u32(bytes, offset) uses a zero-based byte offset into those copied bytes.
  ptr(address) reads an eight-byte little-endian pointer, then checks its range.
The constants describe a proven game build, not an API guaranteed by the game.
Nil checks and bounded indexes deliberately refuse actions when data is missing.
A before/after snapshot narrows races; it does not make game memory atomic.

READABLE EXECUTABLE SOURCE
This commented executable is the only main core source. Shared factories in
marked USS-SOURCE blocks are assembled from their named Source/*.lua files by
tools/assemble_sources.py; edit the named file rather than its embedded block.
The builder assembles these blocks before packing the same readable Lua into
Core. No compaction or separate readable copy is required. The resupply and
classifier files use the shared bridge near the end.
See DEVELOPMENT_HISTORY.md for why the branches and limitations exist.
]]
-- Utility Support Slot: canonical readable source for the public executable core.
-- Pair invariants: pair.utility is the utility; pair.support is the ordinary weapon;
-- pair.real tracks the member in the normal support slot. Only one utility and
-- one ordinary weapon may pair. utility_id is the current utility identity.
-- Memory layouts and native signatures below are tied to the supported build.
-- Keep these guards: offsets alone do not establish safe native-call targets.
-- This API checks readable regions before copying memory into its own buffer.
local create_readonly_api=(function()
return function()
-- LuaJIT FFI describes the C ABI used for Windows reads and native game calls.
-- These declarations do not validate a pointer; the surrounding checks do.
   local ffi=require('ffi')
   assert(ffi.os=='Windows' and ffi.abi('64bit'),'windows_x64_required')
   ffi.cdef[[
       void *uss_win1_GetModuleHandleA(const char *) __asm__("GetModuleHandleA");
       uint32_t uss_win1_GetModuleFileNameW(void *,uint16_t *,uint32_t) __asm__("GetModuleFileNameW");
       void *uss_win1_GetCurrentProcess(void) __asm__("GetCurrentProcess");
       int uss_win1_ReadProcessMemory(void *,const void *,void *,size_t,size_t *) __asm__("ReadProcessMemory");
       size_t uss_win1_VirtualQuery(const void *,void *,size_t) __asm__("VirtualQuery");
       uint32_t uss_win1_GetLastError(void) __asm__("GetLastError");
       void *uss_win1_CreateFileW(const uint16_t *,uint32_t,uint32_t,void *,uint32_t,uint32_t,void *) __asm__("CreateFileW");
       int uss_win1_ReadFile(void *,void *,uint32_t,uint32_t *,void *) __asm__("ReadFile");
       int uss_win1_CloseHandle(void *) __asm__("CloseHandle");
       int32_t uss_win1_BCryptOpenAlgorithmProvider(void **,const uint16_t *,const uint16_t *,uint32_t) __asm__("BCryptOpenAlgorithmProvider");
       int32_t uss_win1_BCryptCreateHash(void *,void **,void *,uint32_t,const void *,uint32_t,uint32_t) __asm__("BCryptCreateHash");
       int32_t uss_win1_BCryptHashData(void *,const void *,uint32_t,uint32_t) __asm__("BCryptHashData");
       int32_t uss_win1_BCryptFinishHash(void *,void *,uint32_t,uint32_t) __asm__("BCryptFinishHash");
       int32_t uss_win1_BCryptDestroyHash(void *) __asm__("BCryptDestroyHash");
       int32_t uss_win1_BCryptCloseAlgorithmProvider(void *,uint32_t) __asm__("BCryptCloseAlgorithmProvider");
       typedef struct { void *base; void *allocation; uint32_t protection0;
           uint16_t partition; uint16_t reserved; size_t size;
           uint32_t state; uint32_t protection; uint32_t kind; } UtilitySupportSlotWin1Region;
   ]]
   local k,b=ffi.load('kernel32'),ffi.load('bcrypt')
   local process=k.uss_win1_GetCurrentProcess()
   local api={}
   local scan_buffer=ffi.new('uint8_t[262144]')
   local scan_count=ffi.new('size_t[1]')
   local scan_address=tonumber(ffi.cast('uintptr_t',scan_buffer))
   -- The scan buffer is our own allocation. Excluding it prevents fallback
   -- identity scans from rediscovering bytes copied by this mod as game data.
   function api.excluded(address,size)
       return address<scan_address+262144 and address+size>scan_address
   end
   -- VirtualQuery describes the containing allocation. Check committed state,
   -- protection and region bounds before larger scans; reject guard pages.
   function api.query(address)
       if address>=0x800000000000 then return nil,'end' end
       local info=ffi.new('UtilitySupportSlotWin1Region[1]')
       if k.uss_win1_VirtualQuery(ffi.cast('void *',address),info,ffi.sizeof(info[0]))~=ffi.sizeof(info[0]) then
           return nil,k.uss_win1_GetLastError()==87 and 'end' or 'query_failed'
       end
       local r=info[0]
       local p=tonumber(r.protection)%256
       local kind=tonumber(r.kind)
       return {base=tonumber(ffi.cast('uintptr_t',r.base)),allocation=tonumber(ffi.cast('uintptr_t',r.allocation)),size=tonumber(r.size),
           protection=tonumber(r.protection),
           readable=r.state==0x1000 and (kind==0x20000 or kind==0x40000)
               and (p==2 or p==4 or p==8 or p==32 or p==64 or p==128)
               and tonumber(r.protection)<256,kind=kind}
   end
   -- Larger bounded reads use one reusable buffer and require a complete copy.
   -- Return an immutable Lua string so a later read cannot change this result.
   function api.read_blob(address,size)
       if type(size)~='number' or size<=0 or size>262144 or api.excluded(address,size) then return nil end
       local r=api.query(address)
       if not r or not r.readable or address<r.base or address+size>r.base+r.size then return nil end
       if k.uss_win1_ReadProcessMemory(process,ffi.cast('const void *',address),scan_buffer,size,scan_count)==0
           or tonumber(scan_count[0])~=size then return nil end
       return ffi.string(scan_buffer,size)
   end
   -- Module image pages have a different memory kind from heap/mapped data.
   -- Use this path for module bytes instead of broadening heap scan rules.
   function api.read_module(address,size)
       if type(size)~='number' or size<=0 or size>262144 or api.excluded(address,size) then return nil end
       local r=api.query(address)
       if not r or r.kind~=0x1000000 or address<r.base or address+size>r.base+r.size
           or r.protection>=256 then return nil end
       local p=r.protection%256
       if p~=2 and p~=4 and p~=8 and p~=32 and p~=64 and p~=128 then return nil end
       if k.uss_win1_ReadProcessMemory(process,ffi.cast('const void *',address),scan_buffer,size,scan_count)==0
           or tonumber(scan_count[0])~=size then return nil end
       return ffi.string(scan_buffer,size)
   end
   function api.module(name)
       local p=k.uss_win1_GetModuleHandleA(name)
       if p==nil or p==ffi.NULL then return nil end
       return tonumber(ffi.cast('uintptr_t',p))
   end
   -- FFI decodes all eight pointer bytes without a hand-written 64-bit sum.
   -- The accepted user-space range is representable exactly by a Lua number.
   function api.pointer(bytes)
       if not bytes or #bytes~=8 then return nil end
       local p=ffi.new('uint64_t[1]')
       ffi.copy(p,bytes,8)
       if p[0]<65536 or p[0]>=0x800000000000 then return nil end
       return tonumber(p[0])
   end
   -- Small reads share scratch storage within this synchronous API instance.
   -- VirtualQuery/ReadProcessMemory do not invoke Lua callbacks here; no buffer
   -- escapes. ffi.string copies the result before the next read may overwrite it.
   -- This saves three FFI allocations per ordinary read without caching pages,
   -- skipping validation, changing read frequency or sharing native pointers.
   local read_region=ffi.new('UtilitySupportSlotWin1Region[1]')
   local read_buffer=ffi.new('uint8_t[4096]')
   local read_count=ffi.new('size_t[1]')
   local function region(address,size)
       if type(address)~='number' or type(size)~='number' or size<=0
           or address<65536 or address+size>=0x800000000000 then return nil end
       local info=read_region
       if k.uss_win1_VirtualQuery(ffi.cast('void *',address),info,ffi.sizeof(info[0]))~=ffi.sizeof(info[0]) then return nil end
       local r=info[0]
       local base=tonumber(ffi.cast('uintptr_t',r.base))
       local protection=tonumber(r.protection)
       if r.state~=0x1000 or address<base or address+size>base+tonumber(r.size)
           or (r.kind~=0x20000 and r.kind~=0x40000 and r.kind~=0x1000000)
           or protection>=256 then return nil end
       return protection,tonumber(r.kind)
   end
   function api.read(address,size)
       if type(size)~='number' or size<1 or size>4096 or size%1~=0
           or not region(address,size) then return nil end
       read_count[0]=0 -- Never accept a byte count left over from a prior call.
       if k.uss_win1_ReadProcessMemory(process,ffi.cast('const void *',address),read_buffer,size,read_count)==0
           or tonumber(read_count[0])~=size then return nil end
       return ffi.string(read_buffer,size) -- Immutable copy, never the scratch array.
   end
   -- Fingerprint the DLL file on disk using SHA-256. Cache by loaded module
   -- address so repeated startup work need not reread the whole file. Native
   -- entry signatures are checked separately before their specific calls.
   function api.module_hash(address)
       local cached=rawget(_G,'UtilitySupportSlotGameHash')
       if type(cached)=='table' and cached.address==address
           and type(cached.sha256)=='string' and #cached.sha256==64 then
           return cached.sha256
       end
       local path=ffi.new('uint16_t[32768]')
       local n=k.uss_win1_GetModuleFileNameW(ffi.cast('void *',address),path,32768)
       assert(n>0 and n<32768,'game_dll_path_unavailable')
       local file=k.uss_win1_CreateFileW(path,0x80000000,7,nil,3,0x08000000,nil)
       assert(file~=ffi.cast('void *',-1),'game_dll_file_unavailable')
       local algorithm,hash=ffi.new('void *[1]'),ffi.new('void *[1]')
       local ok,result=pcall(function()
           local name=ffi.new('uint16_t[7]',{83,72,65,50,53,54,0})
           assert(b.uss_win1_BCryptOpenAlgorithmProvider(algorithm,name,nil,0)==0,'sha256_provider')
           assert(b.uss_win1_BCryptCreateHash(algorithm[0],hash,nil,0,nil,0,0)==0,'sha256_create')
           local chunk,count=ffi.new('uint8_t[65536]'),ffi.new('uint32_t[1]')
           local total=0
           while true do
               assert(k.uss_win1_ReadFile(file,chunk,65536,count,nil)~=0,'game_dll_read')
               if count[0]==0 then break end
               total=total+tonumber(count[0])
               assert(total<=64*1024*1024,'game_dll_too_large')
               assert(b.uss_win1_BCryptHashData(hash[0],chunk,count[0],0)==0,'sha256_update')
           end
           local digest,parts=ffi.new('uint8_t[32]'),{}
           assert(b.uss_win1_BCryptFinishHash(hash[0],digest,32,0)==0,'sha256_finish')
           for i=0,31 do parts[#parts+1]=string.format('%02x',digest[i]) end
           return table.concat(parts)
       end)
       if hash[0]~=nil then b.uss_win1_BCryptDestroyHash(hash[0]) end
       if algorithm[0]~=nil then b.uss_win1_BCryptCloseAlgorithmProvider(algorithm[0],0) end
       k.uss_win1_CloseHandle(file)
       assert(ok,result)
       rawset(_G,'UtilitySupportSlotGameHash',{address=address,sha256=result})
       return result
   end
   return api
end

end)()


-- Loader presence, API version and its log API are dependencies. A failed
-- file open/write is diagnostic-only and must not disable native gameplay.
-- pcall turns initialization errors into a log entry rather than allowing an
-- incompletely initialized update hook to proceed silently.
-- USS-SOURCE-BEGIN pickup_preferences.lua
-- Read the live preference used by the native pickup consumer. This factory
-- performs no writes and installs no update hook. The caller already checks the
-- required build dependencies; validate these specific instructions once as well.
local function pickup_preferences(api, game)
    local signatures = {
        -- Writer associates the key string with preference-object +F7.
        {0x102FB73, '488d1566f92201440fb683f7000000488bce4c8b'},
        -- Loader uses object +BC, then loads the key into subobject +3B.
        {0x1030260, '48895c2408574883ec20488d99bc0000004c8bd2488bcb'},
        {0x1030631, '488d15a8ee220188433a488b05c65c2f02488b8828010000488b8190000000488bcfffd0440fb6433f488d1597ee220188433b'},
        -- Pickup code reaches the preference pointer through manager +14028.
        {0x97CA43, '488b4580488b802840010080b8f700000000750c4585e47407443b6424507507c644'},
        -- Account-settings load caller uses global owner +AC3DC.
        {0x13779A7, '488b0d92e9fa01458bc14881c1dcc30a00498bd2e8a088cbff4c8b0579'},
    }
 local function unhex(s)
  return (s:gsub('..',function(x)return string.char(tonumber(x,16))end))
 end
 local permitted=true
 for _,signature in ipairs(signatures)do
  local expected=unhex(signature[2])
  if api.read(game+signature[1],#expected)~=expected then permitted=false;break end
 end
 local function ptr(address)return api.pointer(api.read(address,8))end
 return function()
  if not permitted then return nil,'CODE_SIGNATURE_MISMATCH' end
  -- Use the game-selected account, never a guessed Steam save directory.
  -- Both confirmed paths must identify the same preference object.
  local owner=ptr(game+0x3326340)
  local manager=ptr(game+0x3326D68)
  if not owner or not manager then return nil,'NO_SETTINGS_CONTEXT' end
  local active=ptr(manager+0x14028)
  if active~=owner+0xAC3DC then return nil,'PREFERENCE_OBJECT_MISMATCH' end
  local bytes=api.read(active+0xF7,1)
  if not bytes or #bytes~=1 then return nil,'UNREADABLE_PICKUP_SETTING' end
  local value=bytes:byte(1)
  if value~=0 and value~=1 then return nil,'NON_BOOLEAN_PICKUP_SETTING' end
  -- Refuse a value sampled across a settings/account/scene transition. False
  -- is a valid No setting; only nil means that the decision must be deferred.
  if api.read(active+0xF7,1)~=bytes or ptr(game+0x3326340)~=owner
   or ptr(game+0x3326D68)~=manager or ptr(manager+0x14028)~=active then
   return nil,'SETTINGS_CONTEXT_CHANGED'
  end
  return value==1
 end
end
-- USS-SOURCE-END pickup_preferences.lua

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
-- USS-SOURCE-BEGIN native_addresses.lua
-- Native entry points are distinct from object-layout offsets. Startup finds
-- their instruction patterns. Unknown builds additionally validate fixed
-- dependencies before startup. No scanner runs in the update loop.
-- One definition list serves normal initialization and unknown-build diagnostics.
-- Definition vocabulary (our names, not exported game API names):
--   hint: last verified instruction RVA; try this before a bounded scan.
--   pattern: hex instruction bytes; ?? accepts a byte that may vary.
--   displacement_offset / instruction_length: decode an x64 RIP-relative
--   reference. Its target is a global POINTER LOCATION, not the manager itself.
-- Function definitions without these fields resolve to callable entry points.
-- A byte match establishes a location; it does not prove the full C ABI/layout.
local function native_address_definitions()
 return {
  {name='owner_manager',hint=0x518C2A,pattern='488b1d????????488bf9448b4308443b4328720c488bcbe8ca490d00448b4308',displacement_offset=3,instruction_length=7},
  {name='player_manager',hint=0x3A4BDE,pattern='488b05????????488d542448488b88e8000000448b4108e8361a2600488b0d2f',displacement_offset=3,instruction_length=7},
  {name='inventory_manager',hint=0x3A9E00,pattern='4c8b05????????33db8bcb458b4830458b5038440fafd2418d71ff4585c9744c',displacement_offset=3,instruction_length=7},
  {name='held_weapon_manager',hint=0x4AABCA,pattern='4c8b15????????7517b8ffffffff8bc84869c1d0010000490342604883c408c3',displacement_offset=3,instruction_length=7},
  {name='attach',hint=0x9AA290,pattern='488bc44489482089501055564881ec88000000488958e8418bf04c8960d88bea4c8968d04c8be14533ed4c8970c83b15????????4c8978c0744b448b4130448b'},
  {name='select_weapon',hint=0x9AAA50,pattern='488bc444894018535541554881ec800000003b15????????418be88bda4c8be90f8494060000448b4930448b5138488970084c8970d04533f6440fafd3418d71'},
  {name='select_state',hint=0xA93E90,pattern='4c8bdc5557488bec4883ec78488b05????????4833c4488945e08b01488bf949895b1033db3b05????????498973188bf24d8963204d896be84d8973e04c8b35'},
  {name='drop_entity',hint=0x9A8840,pattern='40535641564881eca0000000488b05????????4833c44889442478418bd844884c2448448bc28bf2488bd1450fb6f18bcbe8????????8bcbe8????????488944'},
  {name='refill_mag',hint=0x76E6D0,pattern='48895c240848896c2410488974241848897c242041564883ec303b15????????8bda488b2d????????0f297424200f28f27452448b452833c9448b4d30440faf'},
  {name='refill_rounds',hint=0x77A430,pattern='48895c240848896c2410488974241857415641574883ec303b15????????8bda4c8b3d????????0f297424200f28f27444458b473033c9458b4f38440fafcb41'},
 }
end
local function resolve_native_addresses(api,game,uint,emit)
 local definitions=native_address_definitions()
 local resolved,status=resolve_code_addresses(api,game,definitions,uint)
 if not resolved then return nil,status end
 local addresses={}
 for index,definition in ipairs(definitions)do
  local address=game+resolved[index]
  if definition.displacement_offset then
   local bytes=api.read(address+definition.displacement_offset,4)
   if not bytes or #bytes~=4 then return nil,'GLOBAL_DISPLACEMENT_READ'end
   local displacement=uint(bytes,0)
   -- uint reads an unsigned 32-bit value. x64 encodes this displacement as
   -- signed: subtract 2^32 when the sign bit is set. RIP refers to the address
   -- just AFTER this instruction, so target = next instruction + displacement.
   if displacement>=2147483648 then displacement=displacement-4294967296 end
   address=address+definition.instruction_length+displacement
  end
  addresses[definition.name]=address
 end
 emit('NATIVE_ADDRESS_RESOLUTION MODE='..status..' COUNT='..#definitions)
 return addresses
end
-- USS-SOURCE-END native_addresses.lua

-- USS-SOURCE-BEGIN compatibility_dependencies.lua
-- Existing fixed dependencies are checked on unfamiliar DLLs before gameplay.
-- RIP references prove the instruction still names the global our code uses.
-- expected_global is deliberately not a live pointer: null managers at the
-- ship/startup are normal. Changed global placement currently needs an update.
-- Instruction pins are observed layout evidence, not official game symbols.
-- These checks do not execute a native function or prove its entire ABI.
local function compatibility_dependency_definitions()
 return {
  {name='physics_interface',hint=0xBA61B,pattern='488b05????????f30f59c6f30f59def30f5905dacf3002f30f58dbf30f5c053e',displacement_offset=3,instruction_length=7,expected_global=0x3326338},
  {name='settings_owner',hint=0x144024,pattern='488b0d????????48bff39de1388c41e2124881c158fa0100488bd741b10141b8',displacement_offset=3,instruction_length=7,expected_global=0x3326340},
  {name='physics_body_manager',hint=0x4B1019,pattern='488b0d????????33db448bc3448b4928448b5930440fafd8458d71ff4585c974',displacement_offset=3,instruction_length=7,expected_global=0x3326450},
  {name='action_manager',hint=0x289F36,pattern='488b15????????33dbf2440f10088b7008f2440f114c2440395a6c0f86ec0000',displacement_offset=3,instruction_length=7,expected_global=0x3326D20},
  {name='interactable_manager',hint=0x4BE47E,pattern='4c8b1d????????33c948895c24404c89742420458b8b58400100418b9b604001',displacement_offset=3,instruction_length=7,expected_global=0x3326D68},
  {name='weapon_identity_manager',hint=0xC30B8,pattern='488b3d????????4c89742438448b4f28448b5f30440fafd8458d71ff4585c90f',displacement_offset=3,instruction_length=7,expected_global=0x3326DC0},
  {name='ui_manager',hint=0x60961E,pattern='4c8b0d????????84d274314d8db98842000041397f107524418b470c83f82977',displacement_offset=3,instruction_length=7,expected_global=0x347CE28},
  {name='backpack_deposit_manager',hint=0x4B653D,pattern='488b1d????????4533db4c8be2458bc3448b4b104585c9741c488b014c8b5350',displacement_offset=3,instruction_length=7,expected_global=0x33265E8},
  {name='magazine_manager',hint=0x3A6F09,pattern='488b05????????488b4850393c91400f9fc6b902000000e8bbb4130083f8ff74',displacement_offset=3,instruction_length=7,expected_global=0x3326648},
  {name='inventory_manager',hint=0x3A9E00,pattern='4c8b05????????33db8bcb458b4830458b5038440fafd2418d71ff4585c9744c',displacement_offset=3,instruction_length=7,expected_global=0x3326738},
  {name='selected_interaction_manager',hint=0x54174A,pattern='488b35????????4c8bf18b6e143b6e30720b488bcee88c3b46008b6e143b6e08',displacement_offset=3,instruction_length=7,expected_global=0x33269A0},
  {name='source_property_manager',hint=0x540C4A,pattern='488b1d????????488bf9448b4308443b4320720c488bcbe83a784c00448b4308',displacement_offset=3,instruction_length=7,expected_global=0x3326AC0},
  {name='owner_manager',hint=0x518C2A,pattern='488b1d????????488bf9448b4308443b4328720c488bcbe8ca490d00448b4308',displacement_offset=3,instruction_length=7,expected_global=0x3326B90},
  {name='rounds_manager',hint=0x4BA6D2,pattern='488b1d????????448bce448b53308b7b380faff8458d7aff4585d20f840c0100',displacement_offset=3,instruction_length=7,expected_global=0x3326CF0},
  {name='hud_icon_manager',hint=0x4ED9C5,pattern='4c8b05????????33c0885129413980703a00007632660f1f4400008bc84803c9',displacement_offset=3,instruction_length=7,expected_global=0x3326E68},
  {name='resource_catalog_owner',hint=0xBA043,pattern='488b0d????????8b50084883c4205be9b922f200ccccccccccccccccccb81f00',displacement_offset=3,instruction_length=7,expected_global=0x346BF98},
  {name='hud_display_table',hint=0x5F576A,pattern='488d0d????????488b04c148894424708b43088b4f14f2440f10570cf3440f10',displacement_offset=3,instruction_length=7,expected_global=0x37C7670},
  {name='hud_root',hint=0x3A2896,pattern='488b0d????????4885c974138b8150440100a802750983c80289815044010048',displacement_offset=3,instruction_length=7,expected_global=0x346D538},
 }
end
local function compatibility_instruction_pins()
 return {
  {name='layout_4F32FF',rva=0x4F32FF,bytes='4c8b90a024f100'},
  {name='layout_4F37E2',rva=0x4F37E2,bytes='488d048048c1e005490383a0000000'},
  {name='layout_4F8AC9',rva=0x4F8AC9,bytes='4885c074f88b48084881c17c010000486bc1384903c2c3'},
  {name='layout_4FD8EF',rva=0x4FD8EF,bytes='4c8b902028f100'},
  {name='layout_4FDDC2',rva=0x4FDDC2,bytes='4869c088000000490383a8000000'},
  {name='layout_5021E4',rva=0x5021E4,bytes='4885c074f88b40084883c00848c1e0054903c2c3cccccc'},
  {name='layout_76E770',rva=0x76E770,native='refill_mag',offset=0xA0,bytes='8b9090000000e8b596feff038790000000'},
  {name='layout_77A4C0',rva=0x77A4C0,native='refill_rounds',offset=0x90,bytes='8b5054e868d9fdff034754'},
  {name='layout_87FDDA',rva=0x87FDDA,bytes='488b4d38488b7d50'},
  {name='layout_87FE30',rva=0x87FE30,bytes='891cf7'},
  {name='layout_88085A',rva=0x88085A,bytes='4c8b15875daa02'},
  {name='layout_8808EA',rva=0x8808EA,bytes='498b4250'},
  {name='layout_8808EE',rva=0x8808EE,bytes='8b04c8'},
  {name='layout_880E2E',rva=0x880E2E,bytes='4c8b0d0359aa02'},
  {name='layout_880EDC',rva=0x880EDC,bytes='e88f70edff'},
  {name='layout_880FA5',rva=0x880FA5,bytes='e8c66fedff'},
  {name='layout_8810DD',rva=0x8810DD,bytes='e88e6eedff'},
  {name='layout_8810E7',rva=0x8810E7,bytes='e8c4c91200'},
  {name='layout_977110',rva=0x977110,bytes='498b8568400100418bdc4e8b3ce0'},
  {name='layout_97C378',rva=0x97C378,bytes='496bc168'},
  {name='layout_97C38A',rva=0x97C38A,bytes='8b4844'},
  {name='layout_97D9C8',rva=0x97D9C8,bytes='4869c138120000'},
  {name='layout_97DAD0',rva=0x97DAD0,bytes='f30f101df88fa401'},
  {name='layout_97DD4E',rva=0x97DD4E,bytes='488b0d6b8d9a0241b8090000004d8b34248bd6'},
  {name='layout_97DD77',rva=0x97DD77,bytes='e8349c0800'},
  {name='layout_97DD92',rva=0x97DD92,bytes='f30f1145d88bd3e812f60200'},
  {name='layout_9A0B67',rva=0x9A0B67,bytes='8b46104c8b1df76198024c8da0001c00004e8d2460438b04a34f8d24a34c89642458'},
  {name='layout_9A286B',rva=0x9A286B,bytes='498911'},
  {name='layout_9A34B0',rva=0x9A34B0,bytes='488b42408bcb4869f180080000488b2cd848037248'},
  {name='layout_9A3590',rva=0x9A3590,bytes='8b8624080000488d542478448b4d08448b8620080000'},
  {name='layout_9ADBA8',rva=0x9ADBA8,bytes='488b57588b4d10'},
  {name='layout_9B1358',rva=0x9B1358,bytes='498b45588bd38bcf8b6cc804'},
  {name='layout_A07A1D',rva=0xA07A1D,bytes='418bc0488d0d699d7301f30f100481'},
  {name='layout_A07A54',rva=0xA07A54,bytes='488b4f40486bd038f20f104c0a'},
  {name='layout_A07AA2',rva=0xA07AA2,bytes='428b448c0485c00f846effffff413bc0740d41ffc1443bca72e6e95c'},
  {name='layout_A07AC1',rva=0xA07AC1,bytes='f3420f10448c24e9'},
  {name='layout_A431B1',rva=0xA431B1,bytes='428b841088e8530048c1e8152401'},
  {name='layout_A6E921',rva=0xA6E921,bytes='4a8b841088e85300'},
  {name='layout_A6EA3B',rva=0xA6EA3B,bytes='4a8b840980e85300'},
  {name='layout_B5C43B',rva=0xB5C43B,bytes='8b0885c974148bc1488d0d26b2c602488b04c18b480441894d00'},
  {name='layout_143BF90',rva=0x143BF90,bytes='4883ec284c8bd93991100100000f8480000000899110010000'},
  {name='layout_1447160',rva=0x1447160,bytes='48895c241848896c24204889542410565741574883ec20'},
  {name='layout_1450230',rva=0x1450230,bytes='48895c2410574883ec30498bd8488bf9450fb6c1e8b7f5ffff'},
  {name='layout_198A183',rva=0x198A183,bytes='8b4f042b4ffc8b470839470c0f84bb000000'},
  {name='layout_198A384',rva=0x198A384,bytes='8b470441ffc78947fc8b470889470c4883c728'},
  {name='layout_198A4B9',rva=0x198A4B9,bytes='4180bec8270000000f84d0010000'},
  {name='layout_198A599',rva=0x198A599,bytes='458b8eb8270000448bc64963c7488d8819010000488d0c88498d14ce498bcee8a3130000'},
  {name='layout_198A684',rva=0x198A684,bytes='41c686c827000000'},
  {name='layout_198AAE9',rva=0x198AAE9,bytes='4183ffff0f8536010000'},
  {name='layout_198AC06',rva=0x198AC06,bytes='41837f14000f8578010000'},
  {name='layout_198AC37',rva=0x198AC37,bytes='418bb6c4270000418b9eb82700003bde'},
  {name='layout_198AC97',rva=0x198AC97,bytes='41c7861c2300000000a040'},
  {name='layout_198ACB3',rva=0x198ACB3,bytes='410f2fbe1c2300000f8219030000'},
  {name='layout_198AD89',rva=0x198AD89,bytes='488b4580448bc64589a6b4270000498bd7498986a8270000498bce458b4f14'},
  {name='layout_198AEB2',rva=0x198AEB2,bytes='488d0c8041c784ce3423000000000000'},
  {name='layout_198AED8',rva=0x198AED8,bytes='41c786b4270000ffffffff41c686bc'},
  {name='layout_198AFDA',rva=0x198AFDA,bytes='f3410f10861c230000f3410f5cc3f3410f11861c230000'},
  {name='layout_198B802',rva=0x198B802,bytes='33c0413946080f95c08907418b460885c00f84d8000000'},
  {name='layout_198BD20',rva=0x198BD20,bytes='488b05e1a599014c8d45c7488d55b7488bcb4c8b88d000000041ff91380300'},
  {name='layout_198BD73',rva=0x198BD73,bytes='4585c97e6485f67460'},
  {name='layout_198BDDC',rva=0x198BDDC,bytes='418b7d04498b5d08'},
  {name='layout_198C430',rva=0x198C430,bytes='48895c241048896c2418565741564883ec30488b81b4130000488be9f30f1005'},
 }
end
-- USS-SOURCE-END compatibility_dependencies.lua

-- USS-SOURCE-BEGIN build_compatibility.lua
-- Build identity is diagnostic; dependency validation decides whether startup
-- can continue. Unfamiliar builds are attempted without editing a hash allowlist.
-- A matching instruction is evidence of compatibility, not a full ABI proof.
-- Native-call results retain their normal runtime checks. Lua pcall isolates
-- Lua/read failures here; it cannot recover a native access violation.
local function check_build_compatibility(api, game, game_hash, emit)
 local verified_hash='2e2c3b7c2500646dadd5f2b4c6e0504dbb7e7896139f64cddc0d1813c718f51e'
 if game_hash==verified_hash then
  emit('BUILD_COMPATIBILITY PROFILE=VERIFIED SHA256='..game_hash,'info','COMPAT')
  return true -- Normal initialization resolves the native entries once.
 end
 -- Maintenance warnings must remain visible with the default error-only log.
 -- This reports once at startup and adds no polling or repeated warnings.
 local function report(message)emit(message,'error','COMPAT')end
 report('COMPAT_BUILD_MISMATCH SHA256='..tostring(game_hash)..' MODE=VALIDATE_AND_ATTEMPT')
 local function read_u32(bytes, offset)
  if not bytes or #bytes<offset+4 then return nil end
  local a,b,c,d=bytes:byte(offset+1,offset+4)
  return a+b*256+c*65536+d*16777216
 end
 local function fail(reason)
  report('COMPAT_STARTUP_BLOCKED REASON='..tostring(reason)..' MANUAL_UPDATE_REQUIRED=true')
  return false
 end
 -- pcall returns (Lua execution succeeded, function result 1, result 2).
 -- Thus ok=false means a Lua exception; permitted=false means a deliberate
 -- validation refusal. On success, result carries resolved addresses for reuse.
 -- The verified-hash path above skips this EXTRA unknown-build validation;
 -- ordinary initialization and the per-operation checks still run afterward.
 local ok, permitted, result=pcall(function()
  local addresses,reason=resolve_native_addresses(api,game,read_u32,report)
  if not addresses then
   local index=tonumber(tostring(reason):match('ADDRESS_[A-Z_]+_(%d+)$'))
   local definition=index and native_address_definitions()[index]
   return false,tostring(reason)..(definition and ' TARGET='..definition.name or '')
  end
  -- Existing fixed globals need explicit code evidence as well as the ten
  -- relocatable native entries. Scan references if their instruction moved;
  -- reject a changed global target because its readers still use fixed RVAs.
  local definitions=compatibility_dependency_definitions()
  local references,status=resolve_code_addresses(api,game,definitions,read_u32)
  if not references then
   local index=tonumber(tostring(status):match('ADDRESS_[A-Z_]+_(%d+)$'))
   local definition=index and definitions[index]
   return false,tostring(status)..(definition and ' TARGET='..definition.name or '')
  end
  for index,definition in ipairs(definitions)do
   local instruction=game+references[index]
   local displacement=read_u32(api.read(instruction+definition.displacement_offset,4),0)
   if not displacement then return false,'GLOBAL_REFERENCE_READ TARGET='..definition.name end
   if displacement>=2147483648 then displacement=displacement-4294967296 end
   local target=instruction+definition.instruction_length+displacement
   if target~=game+definition.expected_global then
    return false,'GLOBAL_LAYOUT_CHANGED TARGET='..definition.name..' DISCOVERED_RVA='..string.format('0x%X',target-game)
   end
  end
  local pins=compatibility_instruction_pins()
  for _,pin in ipairs(pins)do
   local expected=pin.bytes:gsub('..',function(byte)return string.char(tonumber(byte,16))end)
   -- Refill-body evidence follows its resolved entry, including relocation.
   local address=pin.native and addresses[pin.native]+pin.offset or game+pin.rva
   if api.read(address,#expected)~=expected then return false,'INSTRUCTION_CHANGED TARGET='..pin.name end
  end
  for _,rva in ipairs({0x21417B4,0x23C6D70})do
   if api.read(game+rva,4)~=string.char(0,0,128,63)then
    return false,'REFILL_CONSTANT_CHANGED RVA='..string.format('0x%X',rva)
   end
  end
  for _,definition in ipairs(native_address_definitions())do
   report('COMPAT_NATIVE_RESOLVED NAME='..definition.name..' RVA='..string.format('0x%X',addresses[definition.name]-game))
  end
  report('COMPAT_DEPENDENCIES_PASSED GLOBAL_REFERENCES='..#definitions..' INSTRUCTION_PINS='..#pins..' MODE='..status)
  return true,addresses
 end)
 if not ok then return fail(permitted)end
 if not permitted then return fail(result)end
 report('COMPAT_STARTUP_ALLOWED PROFILE=UNVERIFIED SHA256='..tostring(game_hash)..' LIVE_TEST_REQUIRED=true')
 return true,result -- Reuse the validated entries instead of scanning twice.
end
-- USS-SOURCE-END build_compatibility.lua

-- USS-SOURCE-BEGIN binding_mode_options.lua
-- The optional options menu owns the applied mode; Arsenal remains the fallback.
-- Its choice is read after registration and cached through on_change callbacks.
-- Do not use set(): doing so would overwrite the player's saved menu choice.
local function binding_mode_options(emit)
 local id='utility_support_slot.binding_mode'
 local arsenal,identity,registered,subscribed,choice
 local next_check,attempts=0,0
 local last_mode,last_source
 local function valid(value)return value==1 or value==2 end
 local function poll(now)
  -- Wait until the first update so configuration-only addons can load in either
  -- order. The deployed Arsenal selection remains stable during this session.
  if arsenal==nil then arsenal=rawget(_G,'UtilitySupportSlotBindingMode')=='dedicated' end
  local menu=rawget(_G,'ModOptionsMenu')
  -- This identity check is cheap; all registration/get calls are throttled.
  -- Removing/replacing the dependency invalidates callbacks from the old menu.
  if menu~=identity then
   identity=menu;registered=false;subscribed=false;choice=nil
   next_check=0;attempts=0
  end
  if now>=next_check then
   next_check=now+1000
   local usable=type(menu)=='table' and menu.api==1
    and type(menu.register_option)=='function' and type(menu.get)=='function'
    and type(menu.on_change)=='function'
   if usable and attempts<60 then
    if not registered then
     attempts=attempts+1
     local ok,accepted,reason=pcall(menu.register_option,id,{
      type='choice',label='Binding Mode',mod='Utility Support Slot',
      mod_id='utility_support_slot',choices={'Default Bindings','Dedicated Key'},
      default=arsenal and 2 or 1,
      description='Default Bindings swaps weapons using your normal support controls. Dedicated Key draws the utility with 6 by default or your Utility Weapon binding. Press APPLY to change modes.'
     })
     registered=ok and accepted==true
     if not registered then
      emit('BINDING_OPTIONS_REGISTRATION_RETRY REASON='..tostring(ok and reason or accepted))
     end
    end
    if registered and not subscribed then
     local ok,accepted=pcall(menu.on_change,id,function(value)
      -- The menu calls this only for an applied change. Ignore stale callbacks
      -- after dependency replacement, and refuse unexpected choice values.
      if identity==menu and rawget(_G,'ModOptionsMenu')==menu then
       choice=valid(value) and value or nil
      end
     end)
     subscribed=ok and accepted~=false
     if not subscribed then attempts=attempts+1 end
    end
    if registered and subscribed and not valid(choice) then
     local ok,value=pcall(menu.get,id)
     if ok and valid(value) then choice=value
     else attempts=attempts+1 end
    end
   end
  end
  local from_menu=registered and subscribed and valid(choice)
  local dedicated=arsenal
  if from_menu then dedicated=choice==2 end
  local source=from_menu and 'MOD_OPTIONS_MENU' or 'ARSENAL'
  if dedicated~=last_mode or source~=last_source then
   emit('BINDING_MODE='..(dedicated and 'DEDICATED' or 'DEFAULT')..' SOURCE='..source)
   last_mode,last_source=dedicated,source
  end
  return dedicated
 end
 return poll
end
-- USS-SOURCE-END binding_mode_options.lua

-- USS-SOURCE-BEGIN utility_configuration.lua
-- One shared INI read at startup, regardless of the selected input mode.
-- Preserve existing keys/comments; append only settings which are missing.
local function utility_configuration()
 -- 0x36 is the Windows virtual-key code for keyboard 6, not game action 6.
 -- This INI fallback is separate from the optional native binding menu action.
 -- Invalid/missing values retain these defaults and produce startup warnings.
 local result={utility_key=0x36,log_level='error',warnings={}}
 local levels={error=true,info=true,debug=true}
 local function warning(s)result.warnings[#result.warnings+1]=s end
 local function parse_key(value)
  value=value:lower():gsub('%s',''):gsub('_',''):gsub('-','')
  if #value==1 and value:match('[a-z0-9]')then return value:upper():byte()end
  local number=value:match('^f(%d+)$')
  if number and tonumber(number)>=1 and tonumber(number)<=12 then return 111+tonumber(number)end
  number=value:match('^numpad(%d)$')
  if number then return 96+tonumber(number)end
  return ({pageup=0x21,pagedown=0x22,home=0x24,['end']=0x23,insert=0x2D,delete=0x2E})[value]
 end
 local directory=os.getenv('APPDATA')
 if not directory then warning('INI_UNAVAILABLE APPDATA_MISSING');return result end
 result.path=directory..'/Arrowhead/Helldivers2/utility_support_slot.ini'
 local ok,why=pcall(function()
  local file=io.open(result.path,'rb')
  local data
  if file then data=file:read(65537)or'';file:close()end
  if data and #data>65536 then error('INI_TOO_LARGE')end
  local seen={}
  for line in (data or''):gmatch('[^\r\n]+')do
   local name,value=line:match('^%s*([%w_]+)%s*=%s*([^;#]*)')
   if name and (name=='utility_key' or name=='log_level') and not seen[name]then
    seen[name]=true
    value=value:match('^%s*(.-)%s*$')
    if name=='utility_key' then
     local key=parse_key(value)
     if key then result.utility_key=key else warning('INI_INVALID_KEY USING=6')end
    elseif levels[value:lower()]then result.log_level=value:lower()
    else warning('INI_INVALID_LOG_LEVEL USING=error')end
   end
  end
  local missing={}
  if not seen.utility_key then missing[#missing+1]='utility_key=6' end
  if not seen.log_level then missing[#missing+1]='log_level=error' end
  if #missing>0 then
   local out=assert(io.open(result.path,data and'ab' or'wb'))
   local prefix=data and (#data>0 and '\n' or '') or
    '; Utility Support Slot\n; Restart after editing. log_level: error, info, debug\n; utility_key: a-z, 0-9, f1-f12, numpad0-9, page up/down, home/end, insert/delete\n'
   local written=out:write(prefix..table.concat(missing,'\n')..'\n');out:close()
   assert(written,'INI_WRITE_FAILED')
  end
 end)
 if not ok then warning('INI_UNAVAILABLE REASON='..tostring(why))end
 return result
end
-- USS-SOURCE-END utility_configuration.lua

-- USS-SOURCE-BEGIN utility_logging.lua
-- All components share one bounded writer. Detailed samplers must also obey
-- configuration.log_level, so quiet logging avoids the work, not just the text.
local function utility_logging(write,level)
 local rank={error=1,info=2,debug=3}
 local threshold=rank[level] or 1
 local function inferred_level(message)
  if message:find('ERROR',1,true) or message:find('FAILED',1,true)
   or message:find('PAUSED',1,true) or message:find('UNSUPPORTED',1,true)
   or message:find('UNAVAILABLE',1,true) or message:find('INVALID',1,true)
   or message:find('REJECTED',1,true) or message:find('RATE_LIMIT',1,true)
   or message:find('TIMEOUT',1,true) or message:find('DISABLED',1,true) then return 'error' end
  return 'info'
 end
 return function(message,severity,component)
  severity=severity or inferred_level(message)
  if (rank[severity] or 2)>threshold then return end
  if not component then
   if message:match('^HUD_') or message:match('^PICKUP_HUD_') or message:match('^PICKUP_PRESENTATION_')then component='HUD'
   elseif message:match('^RESUPPLY_') or message:match('^SELF_SUPPLY_') or message:match('^PICKUP_FILTER_')then component='RESUPPLY'
   elseif message:match('^INI_') or message:match('^BINDING_')then component='CONFIG'
   else component='CORE' end
  end
  write('['..component..']['..severity:upper()..'] '..message)
 end
end
-- USS-SOURCE-END utility_logging.lua

local loader=rawget(_G,'CowboyBingusModLoader')
local DEBUG=false -- Startup INI selects detailed diagnostics explicitly.
-- USS-SOURCE-BEGIN utility_log_writer.lua
-- One shared log writer with filesystem-failure isolation, no byte cap.
local function utility_log_writer(file)
 local failed=false
 return function(s)
  if not file or failed then return end
  local ok=pcall(function()
   assert(file:write(os.date('!%Y-%m-%dT%H:%M:%SZ ')..s..'\n'))
   file:flush()
  end)
  -- Do not retry a broken handle on every update. Verbosity filtering occurs
  -- before this writer; healthy logs continue regardless of their size.
  if not ok then failed=true end
 end
end
-- USS-SOURCE-END utility_log_writer.lua
-- USS-SOURCE-BEGIN utility_binding_registration.lua
-- Discover/register the optional action in either behavior mode. Registration
-- exposes an unbound menu row; it never polls the key or draws a weapon.
local function utility_binding_registration(emit)
 local id='utility_support_slot.draw_utility'
 -- identity is the optional menu TABLE, not a weapon identity. Replacing it
 -- resets registration so loader/menu reloads can recover. nil in the register
 -- call leaves the menu key unbound; the INI supplies the separate fallback.
 -- Retry at most once per second, up to 60 attempts for each menu instance.
 local identity,registered,next_try,tries=nil,false,0,0
 return {poll=function(now)
  local menu=rawget(_G,'ModBindingsMenu')
  if menu~=identity then identity=menu;registered=false;next_try=0;tries=0 end
  if type(menu)~='table' or menu.api~=1 or type(menu.version)~='number' or menu.version<2
   or type(menu.register_binding)~='function' or type(menu.is_down)~='function'then return end
  if not registered and now>=next_try and tries<60 then
   next_try=now+1000;tries=tries+1
   local ok,value=pcall(menu.register_binding,id,'Utility Weapon',nil,{category='Utility Support Slot'})
   if ok and value==true then registered=true;emit('UTILITY_BINDING_REGISTERED MENU=true')end
  end
  if registered then return menu end
 end}
end
-- USS-SOURCE-END utility_binding_registration.lua

-- USS-SOURCE-BEGIN utility_shortcut.lua
-- The applied behavior mode controls shortcut input. The optional unbound
-- menu action registers in either mode; default mode never reads its key. Dedicated mode remains dedicated even when
-- the optional menu is missing: its INI keyboard key is always the fallback.
-- Mod Bindings Menu starts unbound and exposes action state, not mapping count.
-- Like Helmet Headlamp, the first accepted menu edge selects that input source
-- for this session. Removing/unavailable menu temporarily restores the INI key.
local function utility_shortcut(loader,ffi,kernel,emit,enabled,api,game,configuration,registration)
 configuration=configuration or utility_configuration()
 registration=registration or utility_binding_registration(emit)
 -- Cache one dedicated reader for the session, but evaluate the cached mode
 -- on each update. Re-entering dedicated mode must not recreate the INI, FFI
 -- declarations or menu binding. A held key needs a fresh release baseline.
 if type(enabled)=='function'then
  local instance,current
  return {
   poll=function(now,eligible)
    registration.poll(now) -- Discover the row even before Dedicated Key is selected.
    local selected=enabled(now)==true
    local changed=current~=nil and selected~=current
    current=selected
    if changed and instance then instance.reset()end
    if not selected then return false,changed end
    if not instance then instance=utility_shortcut(loader,ffi,kernel,emit,true,api,game,configuration,registration)end
    return instance.poll(now,eligible and not changed),changed
   end,
   configured=function()return current==true end
  }
 end
 if not enabled then return {poll=function(now)registration.poll(now);return false end,configured=function()return false end}end
 ffi.cdef[[short uss_win1_GetAsyncKeyState(int) __asm__("GetAsyncKeyState"); void *uss_win1_GetForegroundWindow(void) __asm__("GetForegroundWindow");
 uint32_t uss_win1_GetWindowThreadProcessId(void *, uint32_t *) __asm__("GetWindowThreadProcessId"); uint32_t uss_win1_GetCurrentProcessId(void) __asm__("GetCurrentProcessId");]]
 local user=ffi.load('user32')
 local pid=ffi.new('uint32_t[1]')
 local id='utility_support_slot.draw_utility'
 local key=configuration.utility_key -- Shared startup config; defaults to keyboard 6.
 local menu_identity
 local menu_last,ini_last=nil,nil
 local taken=false
 local last_ini,last_menu=-math.huge,-math.huge
 local function reset_edges()menu_last=nil;ini_last=nil end
 local function menu(now)
  local current=registration.poll(now)
  if current~=menu_identity then menu_identity=current;menu_last=nil end
  return current
 end
 local function focused()
  local window=user.uss_win1_GetForegroundWindow()
  if window==nil then return false end
  user.uss_win1_GetWindowThreadProcessId(window,pid)
  return tonumber(pid[0])==tonumber(kernel.uss_win1_GetCurrentProcessId())
 end
 local function uint(bytes,offset)
  if not bytes then return end
  local a,b,c,d=bytes:byte(offset+1,offset+4)
  return d and a+b*256+c*65536+d*16777216 or nil
 end
 -- Copied read-only layout evidence from the supplied Headlamp example, with
 -- this core's startup dependency checks. Check only on an input edge.
 -- Refuse shortcuts if UI state cannot be verified: no swaps while rebinding
 -- in the escape menu or typing chat, and no unguarded fallback after a change.
 local function ui_clear()
  local ui=api.pointer(api.read(game+0x347CE28,8))
  if not ui then return false end
  local chat=api.read(ui+0x10,8)
  local stack=api.read(ui+0x429C,24)
  if not chat or #chat~=8 or not stack or #stack~=24 then return false end
  if chat~=string.rep('\0',8)then return false end
  local depth,bottom=uint(stack,20),uint(stack,0)
  return not(depth>=1 and depth<=5 and bottom==1)
 end
 local function poll(now,eligible)
  local m=menu(now)
  if not eligible then reset_edges();return false end
  -- Read native activation state through the dependency's public API; Press,
  -- Hold and controller bindings are evaluated by the game, not reproduced here.
  local live,menu_down=false,nil
  if m then
   local ok,value=pcall(m.is_down,id)
   if ok and type(value)=='boolean'then live=true;menu_down=value end
  end
  local menu_edge=live and menu_down and menu_last==false
  menu_last=live and menu_down or nil
  -- False must remain false, not nil: release is our required edge baseline.
  if live and not menu_down then menu_last=false end
  local ini_edge=false
  if not taken or not live then
   local held=user.uss_win1_GetAsyncKeyState(key)<0
   ini_edge=held and ini_last==false;ini_last=held
  else ini_last=nil end
  if not live then menu_last=nil end
  if not menu_edge and not ini_edge then return false end
  if not focused() or not ui_clear()then reset_edges();return false end
  -- When both inputs name the same physical key, they may report one frame
  -- apart. Merge across sources for 150 ms, while preserving rapid separate
  -- presses from the same source. INI remains baseline-read when unavailable.
  if menu_edge then
   taken=true
   if now-last_ini>150 then last_menu=now;return true end
   return false
  end
  if ini_edge and now-last_menu>150 then last_ini=now;return true end
  return false
 end
 emit('UTILITY_INPUT_MODE=DEDICATED INI_VK='..key..' MENU_OPTIONAL=true')
 return {poll=poll,configured=function()return true end,
  reset=function()reset_edges();last_ini=-math.huge;last_menu=-math.huge end}
end
-- USS-SOURCE-END utility_shortcut.lua

local log,write_log,logger
local function emit(s,level,component)if logger then logger(s,level,component)end end
-- v19 can initialize after every binding/configuration addon has loaded.
-- Older API-1 loaders retain the existing immediate, load-order-safe path.
local initialized=false
local startup_mode='IMMEDIATE'
local function initialize_core()
 if initialized then return end
 initialized=true
local ok,err=pcall(function()
assert(loader and loader.api==1 and loader.open_log,'BSL_API_1_REQUIRED')
log=loader.open_log('UtilitySupportSlot.log');write_log=utility_log_writer(log)
local configuration=utility_configuration()
DEBUG=configuration.log_level=='debug'
logger=utility_logging(write_log,configuration.log_level)
for _,warning in ipairs(configuration.warnings)do emit(warning,'error','CONFIG')end
emit('CONFIG_READY LOG_LEVEL='..configuration.log_level..' INI_VK='..configuration.utility_key,'info','CONFIG')
local init_cpu=os.clock()
local api=create_readonly_api()
local game=assert(api.module('game.dll'),'NO_GAME_DLL')
local game_hash=api.module_hash(game)
-- A hash mismatch is a warning. Continue when dependency checks pass; exit
-- before gameplay/input setup only when resolution or validation fails.
local compatible,startup_addresses=check_build_compatibility(api,game,game_hash,emit)
if not compatible then
 if log then log:close();log=nil end
 return
end
-- USS-SOURCE-BEGIN inventory_publication_pins.lua
-- Instruction evidence for the reviewed BE3F40 inventory snapshot ABI and
-- selector-2 dispatch. Copied from the tested publication probe; these are
-- startup/call-boundary verification bytes, never patches or executable writes.
local function inventory_publication_pins()
    local pins = {
        {rva=0xBE3F40, hex="48895c24104889742418574881eca0000000488b05b780a5014833c44889842490000000488bf1418bf98bca418bd8e8cc5a3f008bcb4889442428c744242001000000c744242404000000e8b05a3f008bcf4889442438c744243001000000c744243404000000e8945a3f008b8c24d00000004889442448c744244001000000c744244404000000e8735a3f008b8c24d80000004889442458c744245001000000c744245404000000e8525a3f008b8c24e00000004889442468c744246001000000c744246404000000e8315a3f0048894424784c8d442420488d8424e8000000c74424700100000041b9070000004889842488000000488bd6c744247404000000b986927242c784248000000007000000c784248400000008000000e8cea3ffff488b8c24900000004833cce8fe464a014c8d9c24a0000000498b5b18498b7320498be35fc3"},
        {rva=0xFD9A40, hex="4883ec083b0d22ab4a02750c488d05f1aa17014883c408c34c8b15392549024533c048895c2410488974241848893c24458b8ab8aef100458b9ac0aef100440fafd9418d71ff4585c97435498b9ab0aef100418bbabcaef1000f1f80000000008bc6438d14184823d0488d04d38b14d33bd7740e3bd1740a41ffc0453bc172e033c0488b3c24488b742418488b5c241039080f8574ffffff8b40044805f7210a00488d0440498d04c24883c408c3"},
        {rva=0xBDE4B2, hex="4883fffe7556488b3d31ea8902488b05227f7402488bb760f801008b9fd8620100488bceff5070488b05287e7402448d43ff4533db44897c242085db488d97e86201004d8bce8bcd4c8b5038450f44c341ff5240488bcee999010000"},
        {rva=0xBDE6A7, hex="488b053a7d7402ff5078488b8c24800000004833cce8afa04a014881c498000000415f415e5f5e5d5bc3"},
    }
    for _, pin in ipairs(pins) do
        pin.bytes = pin.hex:gsub('..', function(value)
            return string.char(tonumber(value, 16))
        end)
    end
    return pins
end
-- USS-SOURCE-END inventory_publication_pins.lua
-- USS-SOURCE-BEGIN inventory_publication_dispatch_pins.lua
-- Captured dispatch bodies retained from snapshot_component_eligibility.lua.
-- These bytes identify code; they do not independently prove ABI or delivery.
local function inventory_publication_dispatch_pins()
    local function unhex(value)return(value:gsub("..",function(v)return string.char(tonumber(v,16))end))end
    local send_entries = {
        {at=0x34FF40, body=unhex('4585c00f8467030000488bc4555641544157488da828f7ffff4881ecb809000048895808488bf2488978108bd1488b3d04036c014d8be14c896820458bf84c8970d84c8b2f498bcde8533be2ff488bc84c8bf0492b8d9000000033db48c7442440b004000048b8c54eecc44eecc44e48f7e9488d8500040000895c244848c1fa05488bca488944243048c1e93f488d85000400004803d14889442438885c244c488d4c2430450fb6450de891c1f3ff41385e050f84f1000000488b8fa8000000488b01ff9090000000488b0568026c01bb10000000488b88a8000000488b01ff9090000000440fb654244c8bf8448b4424484c8b5c2438660f1f84000000000041b908000000452bc8413bd9440f42cb4585c0750b440fb6d740887c244ceb23418bc8baff000000d2e2400fb6c7d2e022c2f6d24122d2440fb6d0440ad2448854244c4503c1418bc9d3ef412bd944894424484183f80875408b4424302b4424380344244083f801730ac744244401000000eb1d837c24440075164588134c8b5c2438440fb654244c49ffc34c895c24384533c0448944244885db0f855fffffff488b3d90016c01eb10440fb654244c448b4424484c8b5c243841837e5000899df0080000765f660f1f840000000000498b46584c8d4424308b0c98498b4518488d1449498bcd488d14d0488d85f008000048894424284c89642420e83ff2ffffff85f0080000ffc3413b5e5072c1440fb654244c448b4424484c8b5c2438488b3d12016c014c8bac24f80900004585c074328b4424302b4424380344244083f801730ac744244401000000eb0f837c244400750845881348ff442438c7442448000000004883bfa80000000074624585ff0f84d3000000498bdf0f1f440000488b8fa80000004c8d8500040000448b4c2438f3410f104608410fb646044c8b11442b4c2430488b16f30f114424288844242041ff9280010000488d76084883eb010f8483000000488b3d69006c01ebaf8b4424382b442430c6442450003daf04000076074c8d742451eb1e448bc0488d9500040000488d4c24518bd8e80e76f1004c8d7424514c03f3488d4424504585ff7437442bf0498bdf660f1f440000488b8f980000004c8d442450488b16458bce488b01ff5018488d76084883eb017409488b3defff6b01ebd5488bbc24e8090000488b9c24e00900004c8bb424b00900004881c4b8090000415f415c5e5dc3')},
        {at=0xA4030, body=unhex('40534883ec30488b01488d54242041b908000000458d4128ff5030baa00f0000488b18488d055e375c01488bcb48894328ff1529263501488bc34883c4305bc3')},
        {at=0xA4070, body=unhex('4885c9742f48895c2408574883ec20488bfa488bd9ff159d253501488b074533c0488bd3488bcfff5040488b5c24304883c4205fc3')},
    }
    -- Pin the observed native creation path; its initializer name remains opaque.
    local native_creation_path=unhex('4883ec08683de6df7f4883ec084881ec0800000048891c248f04248f04244883ec0868e9feff6b4883ec08488904248f04248f04244881ec08000000687c26ff67558f04248f0424e83669a1ff68c5367f7e48891c24bb4860f77b4156564889f648c704243675727f415e41f7de4181c62d506afb4156810424dc646156582d')
    return send_entries,native_creation_path
end
-- USS-SOURCE-END inventory_publication_dispatch_pins.lua
-- USS-SOURCE-BEGIN inventory_queue_pins.lua
-- Captured engine bodies/fragments; provenance and SHA256 are in the queue review.
-- The last two pins cover only the existing-peer lookup branch, never its creator.
local function unhex(h)return(h:gsub('..',function(v)return string.char(tonumber(v,16))end))end
local function inventory_queue_pins()
return {
    {rva=0x293F50,bytes=unhex('48895c241848896c242056415541574883ec604c8db9540100004d8bd0498d770141c60700418bc7488bea2bc64c8be905b0040000413bc17305488bc6eb18458bc1498bd2488bce418bd9e8b038fd00488d0433488bf080bc24a0000000007421412bc7c744242001000000448bc84d8bc7488bd5498bcde8c393ffffe945010000418b953801000033c94889bc24800000004c89b4248800000085d274274d8b854001000066660f1f840000000000488d3c4948c1e7044903f848392f7408ffc13bca72ea33ff4c8b77204d85f60f858f000000498b4528498d5d28458d4e08488bcb458d4630488d542430ff503041b908000000488d54244041b8e0010000488bcb4c8b3049891e41c746080000000041c7460c14000000488b03ff503041b90800000041b800010000488bcb488b1049895610488d54245041c746180000000041c7461c00010000488b03ff5030488b0849894e2041c74628010000004c897720418b460c413946087317418b4618412bc703c6413b461c7708412bf7498bceeb1ef30f109424a8000000412bf78bd6498bcee845293a0084c07418488b4f20f30f109c24a8000000448bc6498bd7e839273a00488bbc24800000004c8bb424880000004c8d5c2460498b5b30498b6b38498be3415f415d5ec3')},
    {rva=0x28D390,bytes=unhex('48896c2410565741564883ec4033c04c894424204c89442428418bf144894c24304d8bf04889442434488bea8844243c488bf94183f901730f0fb64c2478c744243401000000eb0d410fb608498d40014889442428488b87b85a00004885c07409483be80f85c30000008b84248000000085c0754c01b7a8480000488d15fe833e0148895c2460448bc60fb6d9488d8f3859000001b49fb0480000e8806cfeff488d1559693c01448bc6488b14da488d8ff8580000e8666cfeff488b5c2460eb6c83f801755101b7ac4800000fb6c1488d4c242001b487b04c00000fb69771480000e879efffff8bc8448bc6488b4718486bd168488b88900000004883c10c4803d1488d8f78590000e8126cfeff488d1517833e01eb07488d1512833e01448bc6488d8f38590000e8f36bfeff488b4f10448bce4d8bc6488bd5488b01488b6c24684883c440415e5f5e48ff6018')},
    {rva=0x2AA350,bytes=unhex('48895c24084889742410574883ec20418bf9498bf0e8b6020000488bd84885c00f842101000080b88c870100000f85140100008b48708d570881e1ff7f0000b8018000006689444b788b8b58050100448b8354050100413bc8771283bb5c050100007409418bc02bc13bd0eb0f8b83500501002bc13bd07664413bd00f96c084c0755a4c8b4310488d0d52ba3c018b935c050100e8f772310083bb0888010000c6838c87010001750ac78308880100f9ffffff4533c0488bcb418d5009e82ed8ffff8b53708d42018943708bc2488b5c2430488b7424384883c4205fc3413bc8770983bb5c0501000075148b83500501002bc13bd07608898b6005010033c901935c0501008d041148038b480501004c8bc7898358050100488bd68b437089018979044883c108e8d4d3fb008b53708bc28d4a01894b70488b5c2430488b7424384883c4205fc3488b5c243033c0488b7424384883c4205fc3')},
    {rva=0x2AA620,bytes=unhex('48895c24104889742418574881ec80000000488bf2488bf94885d2742e8b89a000000033c085c974224c8b87a8000000498b1cc0483973107408ffc03bc172f0eb094885db0f8565010000')},
    {rva=0x2AA7D0,bytes=unhex('488bc34c8d9c2480000000498b5b18498b7320498be35fc3')},
}

end
-- USS-SOURCE-END inventory_queue_pins.lua
-- USS-SOURCE-BEGIN inventory_queue_route.lua
-- Replacement route predicate used inside the complete dispatch validator.
-- field/pointer retain its checked pages/read budget; target_code checks image sections.
-- Reads only: never call the peer lookup (its miss path creates transport state).
local function inventory_queue_route()
local replacement_queue_pins=inventory_queue_pins()
return function(api,field,pointer,root,registry,records,session,image)
    local function must(v,why)if not v then error(why,0)end return v end
    local u32=api.u32
    must(field(registry,16):byte(6)==0,'QUEUE_OPTIONAL_ROUTE_CHANGED')
    for _,event in ipairs({{152,0x42729286},{590,0xFE5BF7A3}})do
        local h=field(records+event[1]*104,12)
        must(u32(h,0)==event[2] and h:byte(5)==1 and h:byte(6)==1
            and u32(h,8)==0,'QUEUE_EVENT_PARAMETERS')
    end
    local transport=pointer(root+0xA8)
    must(pointer(pointer(transport)+0x180)==image.base+0x293F50,'QUEUE_SELECTED_METHOD')
    local provider=pointer(transport+0x10)
    must(pointer(pointer(provider)+0x18)==image.base+0x2AA350,'QUEUE_PROVIDER_METHOD')
    for _,pin in ipairs(replacement_queue_pins)do
        must(api.target_code(image.base+pin.rva,image,#pin.bytes)==pin.bytes,'QUEUE_CODE_PIN')
    end
    must(u32(field(session+0x162D8,4),0)==2,'QUEUE_TWO_PEERS')
    local self=field(session+0xB398,8)
    local peers=field(session+0x162E0,16);local remote=peers:sub(9,16)
    local zero=string.rep(string.char(0),8)
    must(self~=zero and peers:sub(1,8)==self and remote~=zero and remote~=self,'QUEUE_PEER_ORDER')
    local count=u32(field(provider+0xA0,4),0)
    must(count>=1 and count<=32,'QUEUE_PEER_LIMIT')
    local array=pointer(provider+0xA8);local found
    for i=0,count-1 do
        local entry=pointer(array+i*8)
        if field(entry+0x10,8)==remote then
            must(not found,'QUEUE_DUPLICATE_PEER');found=entry
        end
    end
    must(found and field(found+0x1878C,1):byte()==0,'QUEUE_EXISTING_OPEN_PEER_REQUIRED')
    -- Native append retains its own capacity/failure policy. No queue cursor,
    -- counter, allocation or lock is changed or interpreted as an acknowledgment.
    return true
end

end
-- USS-SOURCE-END inventory_queue_route.lua
-- USS-SOURCE-BEGIN inventory_slot_drop_pins.lua
-- Exact mapped bodies from the owning drop review; no native address discovery.
local function unhex(s)return(s:gsub('..',function(v)return string.char(tonumber(v,16))end))end
local function inventory_slot_drop_pins()
return {
    {rva=0x9AF090,bytes=unhex('48895c24084889742410574883ec40488b0d92769702418bf9458bc8418bf0448bc28bdae827f6ffff83ff01754c8bcb89742468e877a962004889442428448d4f01488d442468897c24204c8d442420488944243848c7c2feffffffc744242404000000b9a3f75bfe897c2430c744243404000000e826f32200488b5c2450488b7424584883c4405fc3')},
    {rva=0x9AE6E0,bytes=unhex('48895c241848896c24204889542410564883ec30443b052555ad02418be9418bd8488bf10f846c010000448b413033d2448b5138440fafd348897c2440418d78ff4585c00f84470100004c8b4928448b59340f1f400066660f1f8400000000008bc7428d0c124823c8418b04c9413bc374103bc37414ffc2413bd072e3e90f0100003bc30f8507010000418b44c90483f8ff0f84f9000000488d3c4048c1e70448037e5083fd030f849b00000083fd060f85cf0000008b470c3b05ad54ad020f84c00000008b4f143bc17531e88fb562004885c07427488bc8e8f2aab5ff83b88000000019751641b101448d45fb8bd3488bcee848090000e9880000008b4f0c33d2e8f9431a00448b470c4533c948c7442448000000008bd3488b442448488bcec6442428014889442420e830a0ffff8b153654ad028bcbe823fc19008b052954ad0289470ceb3d8b051e54ad023947087432448b47084533c948c7442448000000008bd3488b442448488bcec6442428014889442420e8e49fffff8b05ea53ad028947083b6f1c7507c7471c00000000488b7c2440488b5c2450488b6c24584883c4305ec3')},
    {rva=0x9A8840,bytes=unhex('40535641564881eca0000000488b05bd37c9014833c44889442478418bd844884c2448448bc28bf2488bd1450fb6f18bcbe8aa2716008bcbe8c314630048894424504885c00f84690f00003b1d8fb3ad020f845d0f0000488b1522e597024889ac24c000000033ed4889bc24c80000008bcd448b4228448b4a30440fafcb418d78ff4585c00f84190f00004c8b5220448b5a2c0f1f4000660f1f8400000000008bc7428d14094823d0418b04d2413bc374103bc37414ffc1413bc872e3e9e20e00003bc30f85da0e000041837cd204ff0f84ce0e00004c8b1da3e497028bcd4c89a424d0000000450fb6e64c89ac24980000004180e4014c89bc2490000000450fb6fe458b4328458b4b3041c0ef034180e701440fafcb44887c2440458d70ff4585c074284d8b5320418b7b2c0f1f00418bc6428d14094823d0418b04d23bc7746e3bc3746effc1413bc872e3b8ffffffff4c8b7424508bc8488d0449498bce48c1e0044903434048894424608b0089442458e8f808b6ff4c8be84038a8840000007437488b0d354bac020f57d240886c243841b101f30f115424308bd3896c2428896c2420e8b577a100e9dc0d00003bc37599418b44d204eb978bb88c0000000f29b4248000000085ff0f84ee000000488b0500d997024c8bb8c8020000488b05e2d89702488b4818488b8148070000418b4e0cffd0488b0ddad897024c8bf0488b5158488b0d5435ac02ffd24533c0498bd6488bc841ffd74c8b35b7d897024c8bf8493b060f8487000000488b442460488b0da7e297024d8bb6780300008b5004e88805e9ff8bc80f57f6f3480f2af1488b0d7fd89702488b5158488b0dfc34ac02ffd20f28de4c8d05c8d88901498bd7488bc841ffd64138ade0000000740a4d8bc78bd3e8d46cf1ff488b0545d897024c3b38741c488b0d61d897024533c94d8bc78bd7488b89f8100000ff9038030000440fb67c24404c8b742450418b959000000085d27439488b05f7d79702488b4818488b8150030000418b4e0cffd085c0741d488b05dbd79702418b9590000000418b4e0c4c8b401841ff9070030000418b7d5485ff74418bcee8ee116300488b0dafd797028bd7488bf04c8b41188b480c4d8b885003000041ffd185c07419488b0d8ed79702418b55544c8b41188b4e0c41ff90700300003b1d8eb0ad024c8b1dffda97027449458b43208bcd458b4b28440fafcb418d70ff4585c074324d8b5318418b7b24660f1f4400008bc6418d14094823d0418b04d23bc70f84ac0000003bc30f84ac000000ffc1413bc872dcb8ffffffff488bbc24e00000004533c9448b0551b0ad028bc8498b43384869d1ac000000488b0db1e1970248897c242844887c24208b34028bd3e84d6cf1ff488b05ded69702418b4e0c488b5018ff9268060000488b05c9d697024c8d442468c74424680000803f33d2c744246c0000803fc74424700000803f4c8b4818418b4e0c41ff51588bcbe8cf1063004885c0741df6401401741741b701eb153bc30f855bffffff418b44d204e956ffffff4532ff3b1d98afad0244887c2441745d488b05c6dd97028bcd448b4028448b4830440fafcb458d70ff4585c0743f4c8b5020448b582c0f1f8000000000418bc6418d14094823d0418b04d2413bc3740d3bc3740dffc1413bc872e2eb103bc3750c41837cd204ff0f85b60000003b1d1aafad020f84aa000000488b0575e097028bcd448b4028448b4830440fafcb458d70ff4585c074344c8b5020448b582c418bc6428d14094823d0418b04d2413bc3740d3bc3740dffc1413bc872e2eb0c3bc3750841837cd204ff7558488b05fbd897028bcd448b4030448b4838440fafcb458d70ff4585c0743a4c8b5028448b58340f1f40000f1f840000000000418bc6428d14094823d0418b04d2413bc30f849c0000003bc30f849c000000ffc1413bc872da0fb68424e8000000894424440f57f64584e475094584ff0f84c20500003b1d47aead020f84230300004c8b2d82df97028bcd458b8558400100458b8d60400100440fafcb458d70ff4585c00f84fb0200004d8b9550400100458b9d5c4001006666660f1f840000000000418bc6428d14094823d0418b04d2413bc3744c3bc37450ffc1413bc872e2e9bf0200003bc30f856bffffff41837cd204ff0f845fffffff8bd3e882a1d9ff85c00f8550ffffff8bd3e863b5d9ff85c00f8441ffffff40886c2444e943ffffff3bc30f857b02000041837cd204ff0f846f0200004c8b1d66de97028bcd458b4328458b4b30440fafcb458d78ff4585c074354d8b5320458b732c0f1f8000000000418bc7428d14094823d0418b04d2413bc60f84e40000003bc30f84e4000000ffc1413bc872da448b6424444c8b1596d797028bcd458b8238100000458b8a40100000440fafcb458d78ff4585c074374d8b9a30100000458bb23c1000000f1f00418bc7428d14094823d0418b04d3413bc60f84fe0000003bc30f84fe000000ffc1413bc872da32c04584e40f84b101000084c00f85a9010000458b85584001008bcd458b8d60400100440fafcb458d70ff4585c00f84880100004d8b9550400100458b9d5c4001000f1f840000000000418bc6428d14094823d0418b04d2413bc30f84bb0000003bc30f84bb000000ffc1413bc872dae9470100003bc30f8523ffffff41837cd204ff0f8417ffffff458b4b30458d78ff440fafcb8bcd4585c0741e418bc7418d14094823d0418b04d2413bc674353bc37435ffc1413bc872e2b8ffffffff448b6424448bc8498b4350450fb6e4488d14c9833c9005440f44e54489642444e9c1feffff3bc375d2418b44d204ebd03bc30f8509ffffff41837cd304ff0f84fdfeffff8bd3498bcae8fddff7ff84c00f84ebfeffffb001e9e6feffff3bc30f8598000000418b44d20483f8ff0f848a0000004d8ba578400100448bd84869d07003000049c1e304448bf049039570400100448bc5478b3c23458bcf448b92400300004585d274554883c2448b0a83e901740f83e902740a83e91a740583f9027504450fabc141ffc04883c268453bc272da453bf9742647890c23ba53978b78498b85684001004d8b85784001004d03c34a8b0cf08b4910e8de066300440fb67c24414584ff74693b1d0dabad020f84dd020000488b05c0db97028bcd448b4038448b4840440fafcb458d70ff4585c0743f4c8b5030448b583c90418bc6428d14094823d0418b04d2413bc3740d3bc3740dffc1413bc872e2eb163bc3751241837cd204ff740a4533c08bd3e85abedaff3b1da4aaad020f8474020000488b05d7d497028bcd448b4030448b4838440fafcb458d70ff4585c00f84f10100004c8b5028448b58340f1f4000418bc6428d14094823d0418b04d2413bc374103bc37414ffc1413bc872e2e9c20100003bc30f85ba01000041837cd204ff0f84ae0100004584ff0f8442010000488b0d69d497028bd3e8a285d9ff3b1d1caaad020f84ec010000488b05cfda97028bcd448b4038448b4840440fafcb458d70ff4585c00f84690100004c8b5030448b583c0f1f40000f1f840000000000418bc6428d14094823d0418b04d2413bc374103bc37414ffc1413bc872e2e9320100003bc30f852a01000041837cd204ff0f841e010000488b0de2d397028bd3e80ba9d9ff85c00f8f080100003b1d8da9ad024c8b1d46da97027442458b43388bcd458b4b40440fafcb458d78ff4585c0742b4d8b5330458b733c0f1f440000418bc7428d14094823d0418b04d2413bc674543bc37454ffc1413bc872e2b8ffffffff498b4b488bd0488b0cd1e84e07b6ff39a8cc0100000f8497000000488b0dfb41ac0241b10140886c24380f28d6f30f117424308bd3896c2428896c2420e87b6ea100eb6e3bc375b3418b44d204ebb14c8b1d27d397028bcd458b4330458b4b38440fafcb458d78ff4585c074364d8b5328458b73340f1f840000000000418bc7428d14094823d0418b04d2413bc60f84de0000003bc30f84de000000ffc1413bc872dab8ffffffff8bc8498b435840882c013b1d85a8ad027459488b051cda97028bcd448b4028448b4830440fafcb458d70ff4585c0743b4c8b5020448b582c0f1f4000660f1f840000000000418bc6428d14094823d0418b04d2413bc30f84800000003bc30f847c000000ffc1413bc872da4532e44532ed3b1d1ea8ad020f84560100004c8b3589d297028bcd458b4620458b4e28440fafcb458d78ff4585c00f84340100004d8b5618458b5e24418bc7428d14094823d0418b04d2413bc374633bc37467ffc1413bc872e2e9090100003bc30f8529ffffff418b44d204e924ffffff3bc3758b41837cd204ff0f847fffffff8bd3e8028af1ff84c00f8470ffffff837c24580241b4010f8465ffffff40386c24440f845affffff450fb6ece954ffffff3bc30f85ae00000041837cd204ff0f84a20000008bd3e8edbdafff84c07418440fb64424404c8bcf498bcee8789de6ff4c8b35b9d197023b1d3ba7ad02744e458b46208bcd458b4e28440fafcb418d78ff4585c074374d8b5618458b5e2466660f1f8400000000008bc7428d14094823d0418b04d2413bc30f84890000003bc30f8489000000ffc1413bc872dbb8ffffffff8bc8498b46384869d1ac0000004038ac02a8000000750d488b542450448bc6e8b20200003b1dd0a6ad020f847c000000488b05ffd497028bcd448b4028448b4830440fafcb418d78ff4585c0745e4c8b5020448b582c8bc7428d14094823d0418b04d2413bc3741f3bc3741fffc1413bc872e3eb373bc30f857effffff418b44d204e979ffffff3bc3752141837cd204ff7419488b0d9cd497028bd3e835cadcff84c074078bd3e81abadcff4c8b742460418b46043b052ba6ad027460488b0d22ce97028bd5448b4938448b5140440fafd0418d71ff4585c974424c8b59308b793c0f1f40008bce468d04124c23c1438b0cc33bcf740d3bc8740dffc2413bd172e4eb193bc8751543837cc304ff740d4533c9448bc38bd0e8b9c6ddfff6442448040f85d90000008bd34584e4740f450fb6cd450fb6c4e8fae3a100eb25488b0d813eac0241b10140886c24380f28d6f30f11742430896c2428896c2420e8036ba1003b1d7da5ad024c8b1516d797020f848b000000458b42288bcd458b4a30440fafcb418d70ff4585c074744d8b5a20418b7a2c908bc6428d14094823d0418b04d33bc7740d3bc3740dffc1413bc872e4eb4d3bc37549418b44d30483f8ff743f498b4a38488b0cc1e8a7fbb5ff448b80d40000004585c074268bd34584e47507e80f2af0ffeb18488b0d46ce9702448bcd40886c2428896c2420e8e523f0ff4c8b3526d797028bf50f28b424800000004139ae2010000076706666660f1f8400000000008bfe4803ff4183bcfe301000002a754a498bbcfe281000008bcbe8c10563004885c07436488bc8e824fbb5ff83b8800000001d75258bc5395f04740dffc04883c70883f80472f0eb118b059da4ad028947048b0594a4ad028907ffc6413bb620100000729b8bcbe854601a004c8bac24980000004c8ba424d00000004c8bbc2490000000488bac24c0000000488bbc24c8000000488b4c24784833cce86fef6d014881c4a0000000415e5e5bc3')},
    {rva=0xBC2C50,bytes=unhex('40534883ec20488b4208488d4c2438488bda8b10e8376f4100488b43188b542438448b004883c4205be942a5fbff')},
    {rva=0xB7D1C0,bytes=unhex('48894c240853554154415641574883ec304c8b35909c7a024533e4458bf88bda418bec4539a6e04c00000f869b010000488974246848897c2470660f1f4400008bc54803c04183bcc6f04c0000460f855e0100003b1d066a9002498bb4c6e84c00000f844a010000448b4630418bcc448b4e38440fafcb418d78ff4585c00f842e0100004c8b5628448b5e340f1f40008bc7428d14094823d0418b04d2413bc374103bc37414ffc1413bc872e3e9000100003bc30f85f8000000418b44d20483f8ff0f84ea000000488d3c4048c1e70448037e504183ff030f84930000004183ff060f85c00000008b470c3b059b6990020f84b10000008b4f143bc1752ee87dca45004885c07424488bc8e8e0bf98ff83b88000000019751341b101458d47fb8bd3488bcee8361ee3ffeb7c8b4f0c33d2e8ea58fdff448b470c498bc4c6442428014533c98bd34889442420488bce4c89642460e827b5e2ff8b152d6990028bcbe81a11fdff8b052069900289470ceb378b0515699002394708742c448b4708498bc4c6442428014533c98bd34889442420488bce4c89642478e8e1b4e2ff8b05e7689002894708443b7f1c75044489671cffc5413baee04c00000f827ffeffff488b7c2470488b7424684883c430415f415e415c5d5bc3')},
}

end
-- USS-SOURCE-END inventory_slot_drop_pins.lua
-- USS-SOURCE-BEGIN inventory_publication_dispatch.lua
-- Dispatch validation extracted from the proven send probe. No research controls,
-- captures or game mutators live here; this factory only checks dependency identity.
local function inventory_publication_dispatch(services,slot_drop)
    local send_entries,native_creation_path=inventory_publication_dispatch_pins()
    local module_path=services.module_path
    local queue_route
    local function unhex(value)return(value:gsub('..',function(v)return string.char(tonumber(v,16))end))end
    return function(read,keep,session,helpers,targets,auxiliary,include_slot_drop)
        local u32=services.read_u32
        local function must(value,reason)if value==nil or value==false then error(reason,0)end return value end
        local memory={pointer=services.memory.pointer}
        function memory.query(at)
            local region=services.query_dispatch(at)
            if region then
                for _,key in ipairs({'base','size','allocation','kind','state','protection'})do keep(region[key])end
            end
            return region
        end
        local function valid(at, size)
            return type(at) == 'number' and at % 1 == 0 and at >= 65536
                and type(size) == 'number' and size % 1 == 0 and size >= 1 and size <= 4096
                and at + size < 0x800000000000
        end
        local function readable(region, at, size)
            if not region or region.state ~= 0x1000 or at < region.base
                or at + size > region.base + region.size or region.protection >= 256 then return false end
            local p = region.protection % 256
            return p == 2 or p == 4 or p == 8 or p == 32 or p == 64 or p == 128
        end

            local function field(at,size)
                if not valid(at,size) or not readable(memory.query(at),at,size)then error('DISPATCH_READ_PAGE',0)end
                return read(at,size)
            end
            local function decoded_pointer(bytes)
                local value=services.memory.pointer(bytes)
                return must(valid(value,1)and value,'DISPATCH_POINTER')
            end
            local function pointer(at)
                return decoded_pointer(field(at,8))
            end
        local function executable(region, base, at, size)
            if not readable(region, at, size) or region.kind ~= 0x1000000
                or region.allocation ~= base then return false end
            local p = region.protection % 256
            return p == 32 or p == 64 or p == 128
        end
        local function filename(path)
            -- Decode only the ASCII basename; personal UTF-16 directories stay private.
            if type(path) ~= 'string' or #path < 4 or #path > 4092
                or #path % 4 ~= 0 or path:find('[^%x]') then return nil end
            local name = ''
            for pair in path:gmatch('....') do
                local code = tonumber(pair:sub(1, 2), 16) + tonumber(pair:sub(3, 4), 16) * 256
                if code == 92 or code == 47 then name = ''
                elseif code >= 32 and code <= 126 then name = name .. string.char(code)
                else name = name .. '?' end
            end
            return name:lower()
        end
        local function module(target)
            if not valid(target, 1) then return nil, 'MISSING_TARGET' end
            local region = memory.query(target)
            local base = region and region.allocation
            if not valid(base, 1) or not executable(region, base, target, 1) then
                return nil, 'NOT_EXECUTABLE_IMAGE'
            end
            -- Image headers are adjacent and read without an intervening game
            -- mutation. Share region metadata only inside this module() call,
            -- then re-query every used region before accepting the header set.
            -- Header bytes themselves are always read fresh; same_module() still
            -- compares independent before/after module snapshots at its callers.
            local header_regions={}
            local function header_field(at,size)
                if not valid(at,size)then error('DISPATCH_READ_PAGE',0)end
                local region
                for _,item in ipairs(header_regions)do
                    if readable(item.region,at,size)then region=item.region;break end
                end
                if not region then
                    region=memory.query(at)
                    if not readable(region,at,size)then error('DISPATCH_READ_PAGE',0)end
                    header_regions[#header_regions+1]={address=at,size=size,region=region}
                end
                return read(at,size)
            end
            local dos = header_field(base, 64)
            if not dos or dos:sub(1, 2) ~= 'MZ' then return nil, 'DOS_HEADER' end
            local pe = u32(dos, 0x3C)
            if pe < 64 or pe > 4096 then return nil, 'PE_OFFSET' end
            local header = header_field(base + pe, 24)
            if not header or header:sub(1, 4) ~= 'PE\0\0'
                or header:byte(5) ~= 0x64 or header:byte(6) ~= 0x86 then return nil, 'PE_HEADER' end
            local count = header:byte(7) + header:byte(8) * 256
            local optional_size = header:byte(21) + header:byte(22) * 256
            if count < 1 or count > 96 or optional_size < 64 or optional_size > 512 then
                return nil, 'PE_COUNTS'
            end
            local optional = header_field(base + pe + 24, 64)
            if not optional or optional:byte(1) ~= 0x0B or optional:byte(2) ~= 2 then
                return nil, 'PE_OPTIONAL'
            end
            local size = u32(optional, 56)
            if size ~= 60719104 or u32(header, 8) ~= 0x6AB382E4 then return nil, 'MODULE_BUILD' end
            local table_at = pe + 24 + optional_size
            if table_at + count * 40 > size then return nil, 'SECTION_TABLE_RANGE' end
            local rows = header_field(base + table_at, count * 40)
            if not rows then return nil, 'SECTION_TABLE' end
            for _,item in ipairs(header_regions)do
                local before=item.region
                local after=memory.query(item.address)
                if not readable(after,item.address,item.size)
                    or after.base~=before.base or after.size~=before.size
                    or after.allocation~=before.allocation or after.kind~=before.kind
                    or after.state~=before.state or after.protection~=before.protection then
                    error('DISPATCH_MODULE_PAGE_CHANGED',0)
                end
            end
            local path = module_path(base)
            if filename(path) ~= 'helldivers2.exe' then return nil, 'MODULE_NAME' end
            return {base = base, size = size, dos = dos, header = header, optional = optional,
                rows = rows, path = path, count = count}
        end
        local function same_module(left, right)
            return left.base == right.base and left.size == right.size and left.dos == right.dos
                and left.header == right.header and left.optional == right.optional
                and left.rows == right.rows and left.path == right.path
        end
        local function section_end(image, rva)
            if rva < 0 or rva >= image.size then return nil end
            local found
            for index = 0, image.count - 1 do
                local offset = index * 40
                local size, start = u32(image.rows, offset + 8), u32(image.rows, offset + 12)
                local flags = u32(image.rows, offset + 36)
                if size > 0 and start + size <= image.size and rva >= start and rva < start + size
                    and math.floor(flags / 0x20000000) % 2 == 1 then
                    if found then return nil end -- Ambiguous overlapping executable sections.
                    found = start + size
                end
            end
            return found
        end

        local function target_code(at,image,size)
            local rva=at-image.base
            local finish=image.base==services.game_base and image.size or section_end(image,rva)
            -- game.dll is already gated by its build and complete entry pins.
            if image.base==services.game_base then finish=image.size end
            if not finish or rva<0 or rva+size>finish or size>1024 then error('DISPATCH_CODE_SECTION',0)end
            local chunks,done={},0
            while done<size do
                local address=at+done
                local region=memory.query(address)
                if not executable(region,image.base,address,1)then error('DISPATCH_CODE_PAGE',0)end
                local n=math.min(size-done,4096-address%4096,region.base+region.size-address)
                -- The region was just queried above; no native mutation occurs
                -- before this read. Keep the full range check and the fresh
                -- after-read query, without a third query inside field().
                if not valid(address,n)or not readable(region,address,n)then error('DISPATCH_READ_PAGE',0)end
                chunks[#chunks+1]=read(address,n)
                local after=memory.query(address)
                if not executable(after,image.base,address,n)or after.base~=region.base
                    or after.size~=region.size or after.protection~=region.protection then error('DISPATCH_PAGE_CHANGED',0)end
                done=done+n
            end
            return table.concat(chunks)
        end
        local api={game=services.game_base,memory=memory,u32=u32,executable=executable,
            same_module=same_module,target_code=target_code}
        function api.module(at)
            local image,reason=module(at)
            if image then image.name=filename(image.path)end
            return must(image,reason or 'DISPATCH_MODULE')
        end
        local game_image={base=services.game_base,size=0x4744000}
        local records={
            {name='EnterCriticalSection',rva=0xA40B0,slot=0x13F66B8,bytes=unhex('48ff2501263501')},
            {name='LeaveCriticalSection',rva=0xA40C0,slot=0x13F66C0,bytes=unhex('48ff25f9253501')},
        }
        for index,item in ipairs(records) do
            local at=targets[index]
            local image=must(api.module(at),'IMPORT_THUNK_MODULE')
            if image.name~='helldivers2.exe' or at~=image.base+item.rva then error('IMPORT_THUNK_IDENTITY',0) end
            if must(api.target_code(at,image,7),'IMPORT_THUNK_BYTES')~=item.bytes then error('IMPORT_THUNK_PIN',0) end
            -- RIP after the seven-byte jump plus its signed DWORD.
            local displacement=api.u32(item.bytes,3)
            if displacement>=0x80000000 then displacement=displacement-0x100000000 end
            if item.rva+7+displacement~=item.slot then error('IMPORT_SLOT_ARITHMETIC',0) end
            local destination=must(api.memory.pointer(field(image.base+item.slot,8,image.base)),'IMPORT_SLOT_POINTER')
            -- Code addresses need not be eight-byte aligned.
            local exported=services.export(item.name)
            if not exported or exported<65536 or exported>=0x800000000000 then error('WINDOWS_EXPORT_MISSING',0) end
            if destination~=exported then error('IMPORT_EXPORT_MISMATCH',0) end
            local page=api.memory.query(destination)
            if not page or not api.executable(page,page.allocation,destination,1) then error('IMPORT_EXPORT_PAGE',0) end
            for _,key in ipairs({'base','size','allocation','kind','state','protection'}) do keep(page[key]) end
            local after=must(api.module(at),'IMPORT_THUNK_MODULE_AFTER')
            if not api.same_module(image,after) then error('IMPORT_THUNK_MODULE_CHANGED',0) end
            for _,key in ipairs({'base','size','dos','header','optional','rows','path','name'}) do keep(image[key]) end
            keep(at);keep(destination);keep(exported)
        end

        -- The existing native-owned lock/object is observed, never initialized here.
        local before=memory.query(auxiliary)
        if not readable(before,auxiliary,40)then error('AUXILIARY_SPAN',0)end
        local opaque=services.memory.read(auxiliary,40)
        if not opaque or #opaque~=40 then error('AUXILIARY_SPAN',0)end
        local after=memory.query(auxiliary)
        if not readable(after,auxiliary,40)or before.base~=after.base or before.size~=after.size
            or before.protection~=after.protection then error('AUXILIARY_CHANGED',0)end
        if field(session+0x1F86B,1):byte(1)~=1 then error('SESSION_LIFECYCLE_INACTIVE',0) end
        local image=must(api.module(targets[3]),'SEND_INTERFACE_MODULE')
        if image.name~='helldivers2.exe' or targets[3]~=image.base+0x34FF40 then error('SEND_INTERFACE_IDENTITY',0) end
        if pointer(helpers+0x60)~=image.base+0xA4030 or pointer(helpers+0x68)~=image.base+0xA4070 then error('SEND_LIFECYCLE_IDENTITY',0) end
        for _,pin in ipairs(send_entries) do
            if must(api.target_code(image.base+pin.at,image,#pin.body))~=pin.body then error('SEND_DEPENDENCY_PIN',0) end
        end
        -- Preserved argument moves before the already pinned -2 branch.
        if must(api.target_code(api.game + 0xBDE452,game_image,11))~=unhex('458bf94d8bf0488bfa8be9') then error('SEND_DISPATCH_ARGUMENT_PIN',0) end
        -- We use the game's existing object, never invoke its
        -- creator. Require the observed constructor path and tag;
        -- do not claim a named Windows initializer behind it.
        local creator=must(api.memory.pointer(field(image.base+0x13F6690,8,image.base)),'SEND_CREATION_POINTER')
        if creator~=image.base+0x2C6E642 then error('SEND_CREATION_PATH',0) end
        if must(api.target_code(creator,image,5))~=unhex('e9da412f00')
            or must(api.target_code(image.base+0x2F62821,image,128))~=native_creation_path then
            error('SEND_CREATION_PATH_PIN',0)
        end
        if pointer(auxiliary+0x28)~=image.base+0x16677B8 then error('SEND_NATIVE_OBJECT_TAG',0) end
        keep(creator)
        for _,item in ipairs({{slot=0x13F6628,name='DeleteCriticalSection'}}) do
            local destination=must(api.memory.pointer(field(image.base+item.slot,8,image.base)),'SEND_IMPORT_POINTER')
            local exported=services.export(item.name)
            if not exported or exported<65536 or exported>=0x800000000000 or destination~=exported then error('SEND_LIFECYCLE_EXPORT',0) end
            local page=api.memory.query(destination)
            if not page or not api.executable(page,page.allocation,destination,1) then error('SEND_IMPORT_PAGE',0) end
            keep(destination);keep(exported)
        end
        -- Serializer dereferences its binary-search result without
        -- a null guard. Prove this exact record/policy before call.
        local registry_root=pointer(image.base+0x1A10278,image.base)
        local registry=pointer(registry_root)
        local header=field(registry+0x88,16)
        local count=api.u32(header,0)
        if count<1 or count>1024 then error('SEND_REGISTRY_COUNT',0) end
        -- The records pointer is already inside this fresh 16-byte header.
        local records,metadata=decoded_pointer(header:sub(9,16)),pointer(registry+0x18)
        -- Membership and local-drop messages share the same dispatcher, imports,
        -- lock object and transport. In a control capture validate those once,
        -- but independently resolve and check BOTH message records/policies.
        local function message(drop_message)
            local send_key=drop_message and 0xFE5BF7A3 or 0x42729286
            local low,high,index=0,count,nil
            for step=1,11 do
                if low>=high then break end
                local candidate=math.floor((low+high)/2)
                local key=api.u32(field(records+candidate*104,4),0)
                if key==send_key then index=candidate;break
                elseif key<send_key then low=candidate+1 else high=candidate end
            end
            if index~=(drop_message and 590 or 152) then error('SEND_REGISTRY_BINDING',0) end
            local record=records+index*104
            field(record,12)
            local arguments=field(record+0x50,16)
            local argument_count=drop_message and 2 or 7
            if api.u32(arguments,0)~=argument_count then error('SEND_ARGUMENT_COUNT',0) end
            local indexes=decoded_pointer(arguments:sub(9,16))
            -- Read the complete adjacent index array once, checking its full span.
            -- Each ordinal and its policy descriptor are still checked separately.
            -- Entry capture repeats these reads; no bytes survive a native action.
            local policy_indexes=field(indexes,argument_count*4)
            for ordinal=0,argument_count-1 do
                local policy_index=api.u32(policy_indexes,ordinal*4)
                local expected_index,selector,width
                if drop_message then
                    expected_index=ordinal==1 and 164 or 256
                    selector,width=1,ordinal==1 and 32 or 15
                elseif ordinal==6 then
                    expected_index,selector,width=408,7,0
                else
                    expected_index,selector,width=256,1,15
                end
                if policy_index~=expected_index then error('SEND_POLICY_INDEX',0) end
                local policy=field(metadata+policy_index*24+12,12)
                if policy~=string.char(selector,width)..string.rep(string.char(0),10) then error('SEND_POLICY',0) end
            end
            if pointer(api.game+0x214C2E0+index*8,api.game)~=api.game+(drop_message and 0xBC2C50 or 0xBAABD0) then error('SEND_RECEIVER_BINDING',0) end
            return index
        end
        local index=message(slot_drop)
        local drop_index
        if include_slot_drop and not slot_drop then drop_index=message(true)end
        local registry_flags=field(registry,16)
        local index_bits=registry_flags:byte(14)
        if index_bits<8 or index_bits>16 or index>=2^index_bits
            or drop_index and drop_index>=2^index_bits then error('SEND_REGISTRY_INDEX_BITS',0) end
        -- The serializer chooses A8 transport when present, else98.
        -- Validate the selected object/table and both optional calls.
        local function optional_pointer(at)
            local raw=field(at,8)
            if raw==string.rep(string.char(0),8) then return 0 end
            return must(api.memory.pointer(raw),'SEND_TRANSPORT_POINTER')
        end
        local transport_a=optional_pointer(registry_root+0xA8)
        local transport_b=optional_pointer(registry_root+0x98)
        local selected=transport_a~=0 and transport_a or transport_b
        if selected<65536 or selected%8~=0 then error('SEND_TRANSPORT_MISSING',0) end
        local table_at=pointer(selected)
        local offsets={transport_a~=0 and 0x180 or 0x18}
        if registry_flags:byte(6)~=0 then
            if transport_a==0 then error('SEND_OPTIONAL_TRANSPORT_MISSING',0) end
            offsets[#offsets+1]=0x90
        end
        for _,offset in ipairs(offsets) do
            local method=must(api.memory.pointer(field(table_at+offset,8)),'SEND_TRANSPORT_METHOD')
            local page=api.memory.query(method)
            if not page or not api.executable(page,page.allocation,method,1) then error('SEND_TRANSPORT_CODE',0) end
            for _,key in ipairs({'base','size','allocation','kind','state','protection'}) do keep(page[key]) end
        end
        local after=must(api.module(targets[3]),'SEND_INTERFACE_MODULE_AFTER')
        if not api.same_module(image,after) then error('SEND_MODULE_CHANGED',0) end
        for _,key in ipairs({'base','size','dos','header','optional','rows','path','name'}) do keep(image[key]) end

        if slot_drop or include_slot_drop or services.replacement_queue_checks then
            queue_route=queue_route or inventory_queue_route()
            queue_route(api,field,pointer,registry_root,registry,records,session,image)
        end
        return true
    end
end
-- USS-SOURCE-END inventory_publication_dispatch.lua
-- USS-SOURCE-BEGIN replacement_primary_boundary.lua
-- One native replacement owns this earlier character boundary. No memory writes
-- or gameplay native calls: Core validates the work and each full publication entry.
local function replacement_primary_boundary(services,enabled)
    local work
    local allowed={REPLACEMENT_WAIT=true,REPLACEMENT_CONTROL=true,
        REPLACEMENT_RETAINED_UTILITY=true,POST_ATTACH_INCOMING=true,
        ENTRY_FORMATION=true,ENTRY_INCOMING=true}
    local boundary={}
    function boundary.begin(w)
        if enabled==false then return end -- Archived private comparison baseline.
        assert(not work,'REPLACEMENT_BOUNDARY_ALREADY_ACTIVE')
        assert(w and w.pair==services.current_pair() and w.context
            and w.primary_deadline and w.old_id and w.new_id and w.kept_id
            and w.old~=w.new and w.old~=w.kept and w.new~=w.kept
            and services.weapon_identity(w.old)==w.old_id
            and services.weapon_identity(w.new)==w.new_id
            and services.weapon_identity(w.kept)==w.kept_id
            and services.is_utility(w.old_id)==services.is_utility(w.new_id)
            and services.is_utility(w.kept_id)~=services.is_utility(w.new_id),
            'REPLACEMENT_BOUNDARY_SCOPE')
        work=w
    end
    function boundary.admit(owner,g,stage,epoch)
        local w=work
        if not w or not allowed[stage] or epoch~=w.epoch
            or services.clock()>=w.primary_deadline
            or services.current_pair()~=w.pair then return false end
        local p=w.context
        if owner.owner~=p.owner or owner.avatar~=p.avatar or owner.inventory~=p.inventory
            or owner.address~=p.address or owner.primary~=p.primary then return false end
        -- Native countdown continues. Only the observed primary-held phase2
        -- shape is added; all identity/parent/peer/code entry checks still run.
        return owner.active==1 and g.avatar==owner.avatar and g.row_avatar==owner.avatar
            and g.primary==owner.primary and g.held==owner.primary
            and g.selected==1 and g.active==1 and g.prior==3 and g.pending==1
            and g.phase==2 and g.timer_bits==0 and g.row1c>0 and g.row1c<0x7F800000
            and g.bit47==1 and g.state_byte==0 and g.global_guard==0
    end
    function boundary.close()work=nil end
    function boundary.status()return {active=work~=nil}end
    return boundary
end
-- USS-SOURCE-END replacement_primary_boundary.lua

-- USS-SOURCE-BEGIN inventory_publication.lua
-- Host/two-player behavior passed scoped checks; joined-owner integration is a candidate.
-- Automatic inventory publication, independent of the research workbench.
-- A member handle, wire ID, resource identity and selected slot are distinct.
-- Both roles use the same identity, recipient and native-entry contract.
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
        local is_host = self==host or host==zero
        -- A joined owner may publish only to its one known host. Keep actual
        -- host identity separate from eligibility used by the transaction guards.
        local joined_client = false
        if not is_host and host~=zero and uint(read(session+0x162D8,4),0)==2 then
            local peers=read(session+0x162E0,16)
            joined_client=peers:sub(1,8)==self and peers:sub(9,16)==host
        end
        local eligible = is_host or joined_client
        return mission, eligible, session, self, state, is_host, joined_client
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
            local mission,eligible,session=scope()
            return mission and eligible and uint(read(session+0x162D8,4),0)==2
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
            local mission,eligible,_,_,game_state=scope()
            if not mission or not eligible or not supported_pair(services.current_pair()) then
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
        local ok,mission,eligible=pcall(scope)
        return ok and mission and eligible or false
    end
    function publisher.replacement_primary_ready(owner)
        local previous_sequence=guard_sequence
        if not publisher.pending_formation(owner) then return false,'REPLACEMENT_SCOPE_CHANGED' end
        local ok,reason=pcall(function()
            local mission,eligible,_,_,game_state=scope()
            if not mission or not eligible then error('REPLACEMENT_SCOPE_CHANGED',0) end
            character_primary(owner,game_state,'REPLACEMENT_WAIT')
        end)
        -- Reuse this exact guard sample; diagnostics never issue another read.
        return ok,not ok and tostring(reason) or nil,
            guard_sequence>previous_sequence and copy_guard(latest_guard) or nil
    end
    local function capture(stage,include_slot_drop,recovery)
        latest_guard=nil
        reads=0;fingerprint={}
        local mission, eligible, session, self, game_state = scope()
        if not mission or not eligible then error('OWNER_MISSION_REQUIRED',0) end
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
        local ok,mission,eligible,_,_,_,host,joined_client=pcall(scope)
        if ok then update_epoch(mission) else mission=nil;host=nil;joined_client=nil end
        local formed=formed_pair(services.current_pair())
        return {replacement_primary=replacement_primary.status(),epoch=epoch,mission=mission,host=host,formed=formed,
            joined_client=joined_client,eligible=eligible,
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
    emit('READY MODE=AUTOMATIC SCOPE=HOST_OR_JOINED_TWO_PLAYERS_TYPED_SUPPORT_PAIR ENABLED='..tostring(not disabled)..' VERSION=29')
    return publisher
end
-- USS-SOURCE-END inventory_publication.lua
-- USS-SOURCE-BEGIN inventory_publication_adapter.lua
-- Windows/native adapter for automatic gameplay publication. It does not load
-- the memory kit or a research reader. The guarded factory owns entry decisions.
local function inventory_publication_services(context,kernel,held_weapon,silo_id,railgun_id)
    local ffi=context.ffi
    ffi.cdef[[
        uint32_t uss_win1_GetCurrentThreadId(void) __asm__("GetCurrentThreadId");
        void *uss_win1_GetProcAddress(void *,const char *) __asm__("GetProcAddress");
    ]]
    local region=ffi.new('UtilitySupportSlotWin1Region[1]')
    local path=ffi.new('uint16_t[1024]')
    local services={memory=context.memory,game_base=context.game_base,ffi=ffi,
        read_u32=context.read_u32,emit=context.emit,context=context,
        sample_inventory=context.sample_inventory,current_pair=context.current_pair,
        weapon_identity=context.weapon_identity,is_utility=context.is_utility,held_weapon=held_weapon,
        silo_identity=silo_id,railgun_identity=railgun_id}
    services.clock=function()return tonumber(kernel.uss_win1_GetTickCount64())end
    services.thread_id=function()return tonumber(kernel.uss_win1_GetCurrentThreadId())end
    services.query_dispatch=function(at)
        if kernel.uss_win1_VirtualQuery(ffi.cast('const void *',at),region,
            ffi.sizeof(region[0]))~=ffi.sizeof(region[0])then return nil end
        local value=region[0]
        return {base=tonumber(ffi.cast('uintptr_t',value.base)),
            allocation=tonumber(ffi.cast('uintptr_t',value.allocation)),size=tonumber(value.size),
            state=tonumber(value.state),protection=tonumber(value.protection),kind=tonumber(value.kind)}
    end
    services.module_path=function(base)
        local length=tonumber(kernel.uss_win1_GetModuleFileNameW(ffi.cast('void *',base),path,1024))
        if length<=0 or length>=1024 then return nil end
        return (ffi.string(path,length*2):gsub('.',function(byte)return string.format('%02X',byte:byte())end))
    end
    services.export=function(name)
        local system=kernel.uss_win1_GetModuleHandleA('kernel32.dll')
        local base=tonumber(ffi.cast('uintptr_t',system))
        if not base or base<65536 or base>=0x800000000000 then return nil end
        return tonumber(ffi.cast('uintptr_t',kernel.uss_win1_GetProcAddress(system,name)))
    end
    services.send=function(owner,metadata)
        local native=ffi.cast('void (*)(int64_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint64_t)',context.game_base+0xBE3F40)
        local uint=context.read_u32
        -- Preserve the exact eighth argument as uint64 cdata, never a Lua double.
        native(-2,owner.avatar,owner.primary,uint(owner.row,4),owner.support,
            uint(owner.row,12),uint(owner.row,24),metadata)
    end
    return services
end
local function inventory_publication_adapter(context,kernel,held_weapon,silo_id,railgun_id)
    return inventory_publication(inventory_publication_services(
        context,kernel,held_weapon,silo_id,railgun_id))
end
-- USS-SOURCE-END inventory_publication_adapter.lua
local ffi,bit=require('ffi'),require('bit')
 ffi.cdef[[uint64_t uss_win1_GetTickCount64(void) __asm__("GetTickCount64");]]
local kernel=ffi.load('kernel32')
local dedicated_mode=binding_mode_options(emit)
local shortcut=utility_shortcut(loader,ffi,kernel,emit,dedicated_mode,api,game,configuration)
local process=kernel.uss_win1_GetCurrentProcess()
local read_buffer=ffi.new('uint8_t[4096]')
local read_count=ffi.new('size_t[1]')
local read_calls=0
-- Read counting/profiling is development-only; release skips it.
-- Hot-path override of the initial region-checking small-read implementation.
-- Reuse a 4096-byte buffer instead of allocating one for every read. Windows
-- ReadProcessMemory plus the returned byte count detects failure/short copies;
-- this override does not perform VirtualQuery on every small read.
function api.read(address,size)
 if type(address)~='number' or address<65536 or address>=0x800000000000
  or type(size)~='number' or size<=0 or size>4096 then return nil end
 if DEBUG then read_calls=read_calls+1 end
 if kernel.uss_win1_ReadProcessMemory(process,ffi.cast('const void*',address),read_buffer,size,read_count)==0
  or tonumber(read_count[0])~=size then return nil end
 return ffi.string(read_buffer,size)
end
-- Identity/signature strings are converted from hexadecimal into raw bytes.
-- Compare the exact byte sequence; do not interpret a weapon hash as an entity.
local function unhex(s)return (s:gsub('..',function(x)return string.char(tonumber(x,16))end))end
-- Lua string indexing starts at 1, while record offsets start at 0. Decode
-- four little-endian bytes; a missing fourth byte leaves the result nil.
local function u32(s,o)
 if not s then return nil end
 local a,b,c,d=s:byte(o+1,o+4)
 return d and a+b*256+c*65536+d*16777216 or nil
end
local function ptr(a) return api.pointer(api.read(a,8)) end
-- Resolve image addresses once; live manager pointers are still read per sample.
local native_addresses,resolve_error=startup_addresses,nil
if not native_addresses then native_addresses,resolve_error=resolve_native_addresses(api,game,u32,emit)end
assert(native_addresses,resolve_error)
-- One-time code validation; the preference value is read only when an incoming
-- utility needs a pickup decision. Do not cache the boolean: menus change it live.
local read_pickup_preference=pickup_preferences(api,game)
local pickup_unknown_reason
local function utility_pickup_setting()
 local value,reason=read_pickup_preference()
 if value==nil then
  if reason~=pickup_unknown_reason then emit('PICKUP_DEFERRED SETTING_UNKNOWN='..tostring(reason))end
  pickup_unknown_reason=reason
 else pickup_unknown_reason=nil end
 return value
end
-- Lookup a mapped open-addressed game table. Capacity must be a power of two
-- for the bit mask, and probing is bounded to at most 128 entries. Returning
-- nil is preferable to scanning arbitrary memory after a malformed header.
local function lookup(a,key)
 local h=api.read(a,20);if not h then return end
 local data=api.pointer(h:sub(1,8))
 local cap,empty,mult=u32(h,8),u32(h,12),u32(h,16)
 if not data or cap==0 or cap>1048576 or bit.band(cap,cap-1)~=0 then return end
 local product=tonumber(ffi.cast('uint32_t',ffi.new('uint64_t',key)*ffi.new('uint64_t',mult)))
 for p=0,math.min(cap,128)-1 do
  local row=api.read(data+8*bit.band(product+p,cap-1),8)
  if not row then return end
  local found=u32(row,0)
  if found==key then local i=u32(row,4);return i~=0xffffffff and i or nil end
  if found==empty then return end
 end
end
local function hex(s) return (s:gsub('.',function(c)return string.format('%02x',c:byte())end)) end


-- Resolve the local inventory and verify that its owner matches the avatar.
-- Return a fresh snapshot rather than trusting handles from a previous frame.
-- The helpers use different address spaces: ptr(global location) gives a
-- manager address; lookup(table address, entity) gives that table's row index.
-- Only then can index * stride reach a record. avatar below initially holds
-- 24 copied descriptor bytes; `id` is the numeric avatar handle decoded from it.
local function sample()
 local owner,player,inv=ptr(native_addresses.owner_manager),ptr(native_addresses.player_manager),ptr(native_addresses.inventory_manager)
 if not owner or not player or not inv then return nil,'NO_MANAGERS' end
 local unit=u32(api.read(player+0x3A8,4),0)
 if not unit or unit==0 or unit==0xffffffff then return nil,'NO_LOCAL_UNIT' end
 -- Follow local unit -> owner record -> avatar -> inventory row. Each table
 -- has its own index; an owner index is not interchangeable with inventory.
 local oi=lookup(owner+0xF21A88,unit)
 if not oi or oi>1048576 then return nil,'NO_OWNER_INDEX' end
 local avatar=api.read(owner+0xF31AD8+oi*24,24)
 local id=u32(avatar,8)
 if not id or id==0 or id==0xffffffff then return nil,'NO_AVATAR' end
 local i=lookup(inv+40,id)
 if not i or i>4096 then return nil,'NO_INVENTORY_INDEX' end
 -- Both pointers belong to the same inventory header, separated by 8 bytes.
 -- Read their 24-byte span once; never reuse it across sample() calls.
 -- A failed complete-span read yields no pointers, preserving owner validation.
 local arrays=api.read(inv+64,24)
 local owners=arrays and api.pointer(arrays:sub(1,8))
 local rows=arrays and api.pointer(arrays:sub(17,24))
 local owned=owners and ptr(owners+i*8)
 if not owned or not rows or api.read(owned,24)~=avatar then return nil,'OWNER_MISMATCH' end
 local row=api.read(rows+i*48,48)
 if not row then return nil,'ROW_UNREADABLE' end
 -- Keep the raw row for later equality checks as well as decoded fields.
 -- A native operation may change a slot between two otherwise valid reads.
 return {owner=owner,avatar=id,inventory=inv,address=rows+i*48,row=row,primary=u32(row,0),secondary=u32(row,4),support=u32(row,8),backpack=u32(row,12),active=u32(row,28)}
end
-- Resolve and validate all native gameplay/refill entry points once.
local attach=ffi.cast('void (*)(void*,uint32_t,uint32_t,uint32_t)',native_addresses.attach)
local select_weapon=ffi.cast('void (*)(void*,uint32_t,uint32_t)',native_addresses.select_weapon)
local select_state=ffi.cast('void (*)(void*,uint32_t,uint32_t)',native_addresses.select_state)
local drop_entity=ffi.cast('void (*)(void*,uint32_t,uint32_t,uint8_t,void*,uint8_t)',native_addresses.drop_entity)
local death_drop_calls=0
local replacement_drop_calls=0
local SOLO_SILO_ID=unhex('aa79ca92054613fc')
local C4_ID=unhex('3d2ff521630df551')
local utility_id=SOLO_SILO_ID
-- Explicit utility class: add verified weapon identity hashes here.
-- This is an allowlist. Identity bytes are stored in the order returned by
-- the live record. Add only utilities whose identity and attachment behavior
-- have been verified, not every weapon that looks physically small.
local UTILITY_WEAPONS={[SOLO_SILO_ID]=true,[C4_ID]=true}
local function is_utility(id) return UTILITY_WEAPONS[id]==true end
-- cache contains validated identities; wanted contains only pending searches.
-- cursor spreads fallback scanning over polls instead of restarting a full
-- world-sized scan every update. scan() also prunes no-longer-relevant entries.
local avatar,owner,cache,wanted,cursor=nil,nil,{},{},0
local pair,last_support,paused,down,tick,calls=nil,nil,false,false,0,0
local publication -- Initialized after the shared context; reset at owner changes.
local next_poll,updates,polls,poll_total_ms,poll_peak_ms=0,0,0,0,0
local native_used,native_until=0,0
-- Bound attachment/drop calls within a time window; pause on runaway activity.
local function native_guard()
 local now=tonumber(kernel.uss_win1_GetTickCount64())
 if now>=native_until then native_used=0;native_until=now+10000 end
 if native_used>=64 then paused=true;emit('PAUSED NATIVE_RATE_LIMIT');return false end
 native_used=native_used+1;return true
end
local input_cache=nil
local reset_input_for_mode=false
-- Inventory active-slot state alone is insufficient: verify the held entity too.
-- This protects support swapping during fire, vehicle use and transitions.
local function held_weapon(avatar)
 local m=ptr(native_addresses.held_weapon_manager)
 local i=m and lookup(m+0x30,avatar)
 local rows=i and i<16384 and ptr(m+0x60)
 return rows and u32(api.read(rows+i*0x1D0,4),0)
end
-- Read native semantic weapon actions, retaining custom bindings. Action 24
-- selects support; action 19 participates in native weapon cycling (831316).
-- Its first byte is an edge, while +4 is the live held value. The captured
-- controller hold keeps +4 at float 1 after the edge clears. Index 206 also
-- responds to mouse fire and is deliberately no longer read by this function.
-- Arm only after a release was observed, with support already actually held.
-- Vanilla may take about 500 ms to wield primary. Keep the attempt armed past
-- 250 ms until that transition is observed; never consume it just on timeout.
local function support_action()
 if not pair or paused then input_cache=nil;return false end
 local c=input_cache
 if not c then return false end
 local m=ptr(game+0x3326D20)
 local record=m==c.manager and api.read(c.record,4)
 local row=record and u32(record,0)==pair.avatar and api.read(c.address,48)
 local flags=row and api.read(c.flag,1)
 local cycle=flags and api.read(c.flag+(19-24)*32,8)
 local hit=false
 if cycle then
  local active=u32(row,28);local support=u32(row,8)
  local now=tonumber(kernel.uss_win1_GetTickCount64())
  local pressed=flags:byte(1)==1
  -- Exact observed float encoding avoids treating arbitrary nonzero bytes,
  -- stale duration values, release events or malformed records as a hold.
  local holding=u32(cycle,4)==0x3f800000
  local need=(pressed and not c.down) or active~=c.active
   or (holding and c.held==false)
   or ((active==3 or (holding and c.armed and not c.fired)) and now>=(c.hand_due or 0))
  local actual=c.hand
  if need then actual=held_weapon(pair.avatar);c.hand_due=now+100 end
  local valid=u32(row,0)~=0 and u32(row,4)~=0
   and (support==pair.utility or support==pair.support)
  hit=pressed and not c.down and c.active==3 and active==3 and valid
   and c.hand==support and actual==support
  if holding and c.held==false then
   c.hold_since=now
   c.armed=c.active==3 and c.hand==support and valid
    and (actual==support or (active==1 and actual==u32(row,0)) or (active==2 and actual==u32(row,4)))
   c.hold_real=support;c.hold_left=pair.utility;c.hold_right=pair.support
   c.fired=false;c.transitioned=false
  end
  if holding and c.armed and not c.fired then
   if not valid or support~=c.hold_real or pair.utility~=c.hold_left
    or pair.support~=c.hold_right then c.armed=false
   else
    if (active==1 and actual==u32(row,0)) or (active==2 and actual==u32(row,4)) then c.transitioned=true end
    if now-c.hold_since>=250 and c.transitioned then
     -- Commit only with a fresh wielded-entity read. Timed sampling above is
     -- capped at 10 Hz; this event check prevents an old sample causing a swap.
     if not need then actual=held_weapon(pair.avatar);c.hand_due=now+100 end
     if actual==support or (active==1 and actual==u32(row,0)) or (active==2 and actual==u32(row,4)) then
      hit=true
      if DEBUG then emit('CONTROLLER_HOLD_TRIGGER ACTION=19 MS='..(now-c.hold_since)..' ACTIVE='..active) end
     else c.armed=false end
    end
   end
  end
  if hit then c.fired=true;c.armed=false end
  if not holding then c.armed=false;c.hold_since=nil;c.fired=false end
  c.held=holding;c.down=pressed;c.active=active;c.hand=actual
 else input_cache=nil end
 return hit
end
-- Refresh the input context from a validated inventory snapshot. Release/edge
-- history prevents a continuously held action from repeatedly swapping.
local function track_input(p)
 if not pair then input_cache=nil;return end
 if input_cache and input_cache.address==p.address then return end
 local m=ptr(game+0x3326D20);local i=m and lookup(m+0xF8,p.avatar)
 if not i or i>=8 then input_cache=nil;return end
 local record=m+0x53DCD0+i*0x1238
 if u32(api.read(record,4),0)~=p.avatar then input_cache=nil;return end
 if not input_cache or input_cache.manager~=m or input_cache.record~=record
  or input_cache.address~=p.address then
  input_cache={manager=m,record=record,address=p.address,active=p.active,hand=held_weapon(p.avatar),down=false,
   flag=m+0x150+i*0xA7AEC+0x1B68+24*32}
 end
end
local pending_utility=nil
local replacement_pending=nil
local formation_pending=nil
-- Forget pairing and cached context at avatar/world boundaries; stale entities
-- must never be reused by attachment, drop or resupply operations.
local function reset(p)
 formation_pending=nil
 if replacement_pending then emit('REPLACEMENT_DEFERRED_CANCEL REASON=RESET');replacement_pending=nil end
 if publication then publication.end_replacement();publication.reset()end
 avatar=p and p.avatar;owner=p and p.owner
 input_cache=nil;cache={};wanted={};cursor=0;pair=nil;last_support=nil;pending_utility=nil;paused=false
 if DEBUG then emit('RESET AVATAR='..tostring(avatar)) end 
end
-- Resolve and cache a verified weapon identity. Entity handles can be reused,
-- so later operations still check identity and ownership before native calls.
local function identity(entity)
 if not entity or entity==0 or entity==0xffffffff then return nil end
 local c=cache[entity]
 if c then
  -- A cached direct identity must still point to the same manager/header.
  -- Recycled entity IDs must not inherit an old weapon identity.
  if c.direct then
   if ptr(game+0x3326DC0)==c.manager and api.read(c.direct,24)==c.header then return c.identity end
  else
   local row=api.read(owner+0xF31AD8+c.index*24,12)
   if row and u32(row,8)==entity and row:sub(1,8)==c.identity then return c.identity end
  end
  cache[entity]=nil
 end
 -- Try the indexed unit/weapon manager before asking the fallback owner scan.
 -- This direct path is important for prompt attachment after pickup.
 if not wanted[entity] then
  local m=ptr(game+0x3326DC0)
  local h=m and api.read(m,72)
  local i=h and lookup(m+0x20,entity)
  local units=h and api.pointer(h:sub(57,64))
  local rows=h and api.pointer(h:sub(65,72))
  local unit=i and units and u32(h,12)<=16384 and u32(h,12)<=u32(h,0) and i<16384 and i<u32(h,12) and ptr(units+i*8)
  local header=unit and api.read(unit,24)
  local row=unit and rows and api.read(rows+i*48,20)
  if header and row and u32(header,8)==entity and u32(row,12)==2
   and ptr(game+0x3326DC0)==m and api.read(m,72)==h and api.read(unit,24)==header then
   local id=header:sub(1,8)
   cache[entity]={direct=unit,header=header,manager=m,identity=id}
   if DEBUG then emit('IDENTITY_DIRECT ENTITY='..entity..' HASH_LE='..hex(id)) end 
   return id
  end
  wanted[entity]=tonumber(kernel.uss_win1_GetTickCount64())
  if DEBUG then emit('IDENTITY_WAIT_BEGIN ENTITY='..entity..' TICK_MS='..wanted[entity]) end 
 end
 return nil
end
local function identity_needed(e,p)
 return e==p.support or (pair and (e==pair.utility or e==pair.support))
  or (pending_utility and e==pending_utility.entity) or (last_support and e==last_support.entity)
end
-- Incremental identity discovery avoids an unbounded world scan per update.
local function scan(p)
 for e in pairs(wanted) do
  if not identity_needed(e,p) then wanted[e]=nil;if DEBUG then emit('IDENTITY_SEARCH_CANCEL ENTITY='..e) end  end
 end
 for e in pairs(cache) do if not identity_needed(e,p) then cache[e]=nil end end
 if paused or not next(wanted) or not owner or ptr(native_addresses.owner_manager)~=owner then return end
 local n=math.min(512,16384-cursor)
 local b=api.read_blob(owner+0xF31AD8+cursor*24,n*24)
 if b then for j=0,n-1 do
  local e=u32(b,j*24+8)
  if e and wanted[e] then
   local id=b:sub(j*24+1,j*24+8)
   local elapsed=tonumber(kernel.uss_win1_GetTickCount64())-wanted[e]
   cache[e]={index=cursor+j,identity=id};wanted[e]=nil
   if DEBUG then emit('IDENTITY ENTITY='..e..' HASH_LE='..hex(id)..' WAIT_MS='..elapsed) end 
  end
 end end
 cursor=(cursor+n)%16384
end
-- Validate the local weapon/input context before native selection/attachment.
-- Aiming is allowed, as it is for vanilla weapon selection. The previous gate
-- checked bit 18 twice (its cached byte and source bit): both mean aiming,
-- rather than independent unsafe action states. Callers retain their held-entity
-- and native-boundary checks; removing the aim veto does not remove those.
local function can_switch(p)
 if not p or p.primary==0 or p.secondary==0 then return false end
 if p.active~=1 and p.active~=2 and p.active~=3 then return false end
 local m=ptr(game+0x3326D20);local i=m and lookup(m+0xF8,p.avatar)
 local count=m and u32(api.read(m+0x70,4),0)
 if not i or not count or count>4096 or i>=count then return false end
 local context=ptr(m+0x110+i*8)
 return context~=nil and u32(api.read(context+8,4),0)==p.avatar
end
-- Compare the stable local context around a native call. The caller separately
-- checks support/active state where that operation expects them to remain fixed.
-- Do not add support equality here blindly: swapping deliberately changes it.
local function same_context(a,b)
 return a and b and a.owner==b.owner and a.avatar==b.avatar and a.inventory==b.inventory
  and a.address==b.address and a.primary==b.primary and a.secondary==b.secondary and a.backpack==b.backpack
end
-- Selection updates both action state and inventory selection through native
-- functions, then confirms the requested slot became active.
local function choose(p,slot)
 local m=ptr(game+0x3326D20);local i=m and lookup(m+0xF8,p.avatar)
 if not i or i>4096 then return false end
 local at=m+0x53DCD0+i*0x1238
 if u32(api.read(at,24),0)~=p.avatar then return false end
 local current=sample()
 if not same_context(p,current) or current.support~=p.support then return false end
 select_state(ffi.cast('void*',at),slot,0)
 select_weapon(ffi.cast('void*',p.inventory),p.avatar,slot)
 local after=sample()
 return same_context(p,after) and after.support==p.support and after.active==slot
end
-- Read pickup interaction metadata for diagnostics/ground verification.
-- A visible weapon on the character can still have an interaction helper.
local function interaction(entity)
 local m=ptr(game+0x3326D68);local i=m and lookup(m+0x14050,entity)
 local rows=m and ptr(m+0x14070)
 local r=i and i<=1048576 and rows and api.read(rows+i*0x370,0x370)
 return r and {zone=u32(r,0xA8),gate=r:byte(0xC9+1),tail=u32(r,0x360)}
end
-- Validate canonical classes and local ownership for both pair members.
local function check_pair(p)
 return pair and pair.avatar==p.avatar and pair.owner==p.owner
  and is_utility(utility_id) and not is_utility(pair.support_id)
  and identity(pair.utility)==utility_id and identity(pair.support)==pair.support_id
end
-- Bounded HUD work at verified replacement boundaries. Intermediate snapshots
-- are debug-only; COMPLETE also owns cosmetic pickup correction in normal builds.
-- Missing/failed HUD work must never block replacement.
local function trace_replacement(stage,p,incoming,old,kept)
 if not DEBUG and stage~='COMPLETE'then return end
 p=p or sample() -- COMPLETE needs one current context even with tracing disabled.
 local callback=_G.UtilitySupportSlotContext and _G.UtilitySupportSlotContext.replacement_hud
 if not callback then return end
 local good,why=pcall(callback,stage,p,incoming,old,kept)
 if not good then emit('REPLACEMENT_HUD_CALLBACK_ERROR STAGE='..stage..' REASON='..tostring(why))end
end

local filter_restore,filter_stop
-- Use the verified native attachment/selection path for a single entity.
-- Revalidate context at the call boundary and confirm the resulting inventory.
-- This path can still produce the game's acquired notification.
-- Argument roles: p is the caller's earlier inventory snapshot; target is a
-- live entity handle; target_id is its expected eight-byte resource identity.
-- draw requests selection as well as slot restoration; label names our operation
-- for logs/HUD coordination. The fresh `current` snapshot below must still agree.
local function switch_entity(p,target,target_id,draw,label,defer_ack)
 local current=sample()
 if not same_context(p,current) or current.row~=p.row or not can_switch(current)
  or identity(target)~=target_id then return false end
 if not native_guard() then return false end
 -- Pickup-only experiment: vanilla has already replaced the held support
 -- with the utility. Restore the remembered regular member in slot 3 without
 -- inserting another primary/secondary animation. Pending supported multiplayer
 -- formation takes the primary boundary instead; final selection stays unchanged.
 -- The local-only exception requires the
 -- exact pair direction and an explicit draw request; ordinary swaps retain
 -- their verified holster path. Do not broaden it to replacements or drops.
 local pickup_restore=label=='STORE_NEW_UTILITY'
 local deferred_replacement=label=='REPLACE_RECOVER_UTILITY' or label=='REPLACE_RESTORE_ORDINARY'
  or label=='REPLACE_RECOVER_ORDINARY'
 local pair_setup=pickup_restore or label=='CAPTURE_UTILITY_FIRST' or label=='RESTORE_UTILITY_FIRST_SUPPORT' or deferred_replacement
  or label=='REPLACE_RETAIN_SPECIAL' or label=='REPLACE_KEEP_NEW_PICKUP' or label=='STORE_REPLACEMENT_UTILITY'
 local forming=pair_setup and publication and publication.pending_formation and publication.pending_formation(current)
 local preserve_support=not forming and pickup_restore and draw and current.active==3 and pair
  and pair.avatar==current.avatar and pair.owner==current.owner
  and current.support==pair.utility and target==pair.support
  and target_id==pair.support_id and pair.real==pair.support
 if pickup_restore then emit('PICKUP_RESTORE_BEGIN PRESERVE_SUPPORT='..tostring(preserve_support)
  ..' ACTIVE='..current.active..' FROM='..current.support..' TO='..target
  ..' HELD='..tostring(held_weapon(current.avatar)))end
 -- Member-changing draw commands use the reviewed primary publication boundary
 -- from secondary too. Pair setup restores its chosen selection in the caller;
 -- unrelated drawless recovery does not gain a new secondary-to-primary hop.
 local managed_swap=draw and (label=='DRAW_UTILITY' or label=='DRAW_SUPPORT')
 if (current.active==3 or ((managed_swap or forming) and current.active==2)) and not preserve_support then
  if not choose(current,1) then paused=true;emit('PAUSED HOLSTER_VERIFY_FAILED');return false end
  current=sample()
 end
 local support_before=current.support
 local replacement_action=label=='REPLACE_RETAIN_SPECIAL'or label=='REPLACE_KEEP_NEW_PICKUP'
  or label=='STORE_REPLACEMENT_UTILITY'
 if replacement_action then trace_replacement('ATTACH_BEFORE_'..label,current,support_before,0,target)end
 -- Pickup-only read-only diagnostic before our native attachment. The HUD
 -- callback catches its own errors, so tracing cannot block weapon handling.
 if pickup_restore and _G.UtilitySupportSlotContext.before_pickup_restore then _G.UtilitySupportSlotContext.before_pickup_restore(support_before,target,current)end
 if filter_restore then filter_restore() end
 -- Formation uses the ordinary member at a verified primary boundary. It is
 -- published once for this pair, rather than replayed on every swap.
 if publication and not deferred_replacement then publication.formation_boundary(current)end
 if DEBUG then emit('ATTACH_BEGIN ACTION='..label..' FROM='..support_before..' TO='..target..' BACKPACK='..current.backpack..' TICK_MS='..tonumber(kernel.uss_win1_GetTickCount64())) end 
 attach(ffi.cast('void*',current.inventory),current.avatar,target,3)
 calls=calls+1
 local after=sample()
 if not same_context(current,after) or after.support~=target then
  paused=true;emit('PAUSED ATTACH_VERIFY_FAILED');return false
 end
 local ir=interaction(target)
 if DEBUG then emit('ATTACH_END TO='..target..' GROUND_ZONE='..tostring(ir and ir.zone)..' GROUND_GATE='..tostring(ir and ir.gate)) end 
 if not ir or (ir.gate~=0 and ir.gate~=1) or (ir.zone~=0xffffffff and ir.gate~=0) then
  paused=true;emit('PAUSED TARGET_GROUND_INTERACTION_ACTIVE_OR_UNKNOWN');return false
 end
 -- Selected slot 3 does not prove its held entity changed during attachment.
 -- Always run both native selection functions for draw requests, even when
 -- the index is unchanged. This synchronizes wield/action state with inventory.
 -- Publish the incoming membership after attachment and before native draw.
 -- Guard refusal leaves the accepted local selection path intact; no retry/write.
 if publication then
  if deferred_replacement then
   local accepted,reason
   if label=='REPLACE_RECOVER_UTILITY' then accepted,reason=publication.replacement_utility_boundary(after)
   else accepted,reason=publication.boundary(after)end
   if not accepted then
    emit('REPLACEMENT_DEFERRED_REFUSED ACTION='..label..' REASON='..tostring(reason));return false
   end
  elseif draw or label=='RESTORE_UTILITY_FIRST_SUPPORT' then
   publication.boundary(after)
  elseif label=='CAPTURE_UTILITY_FIRST' then
   -- This recovered utility may have no observer parent yet. Publish it while
   -- it occupies the real support slot, before restoring/publishing ordinary.
   publication.utility_first_boundary(after)
  else publication.formation_boundary(after)end
 end
 if draw and not choose(after,3) then paused=true;emit('PAUSED DRAW_VERIFY_FAILED');return false end
 if pickup_restore then local final=sample();emit('PICKUP_RESTORE_END ACTIVE='..tostring(final and final.active)
  ..' SUPPORT='..tostring(final and final.support)..' PRESERVE_SUPPORT='..tostring(preserve_support)
  ..' HELD='..tostring(final and held_weapon(final.avatar)))end
 -- Notification acknowledgement runs only at a successful native boundary.
 -- The callback isolates errors and validates explicit pair swaps or automatic
 -- restoration. Initial utility pickup restoration acknowledges only the
 -- automatic partner restoration; rendered genuine pickup messages stay intact.
 if not defer_ack and _G.UtilitySupportSlotContext.acknowledge_inventory_change then _G.UtilitySupportSlotContext.acknowledge_inventory_change(support_before,target,label) end
 if replacement_action then trace_replacement('ATTACH_AFTER_'..label,nil,support_before,0,target)end
 return true
end

-- Replacement can keep both members attached without needing switch_entity.
-- Establish its one formation boundary before restoring the pickup's selection.
local function publish_replacement_pair(p)
 if not publication or not publication.pending_formation(p) then return p end
 if p.support~=pair.support then return p end
 if not can_switch(p) or not native_guard() then return nil end
 if p.active~=1 then
  if not choose(p,1) then return nil end
 end
 local current=sample()
 if not same_context(p,current) or current.support~=pair.support or not check_pair(current) then return nil end
 publication.formation_boundary(current)
 return current
end

-- Read attachment/parent state only after confirming the expected identity.
local function attached_entity(entity,expected)
 local m=ptr(game+0x3326DC0)
 local i=m and lookup(m+0x20,entity)
 if not i or i>16383 then return end
 local units,rows=ptr(m+0x38),ptr(m+0x40)
 local unit=units and ptr(units+i*8)
 local h=unit and api.read(unit,24)
 local r=rows and api.read(rows+i*48,48)
 if not h or not r or u32(h,8)~=entity or h:sub(1,8)~=expected then return end
 return {parent=u32(r,4),row=r}
end
-- Stored-weapon ping filtering; no executable code modification.
local filter_old,filter_armed,filter_failed=nil,false,false
local filter_api,filter_table,filter_worlds,filter_owner
local filter_buffer=ffi.new('uint32_t[1]')
local filter_original,filter_candidate=0x236122F0,0xFAEE2C05
local filter_calls=0
local filter_used,filter_until=0,0
local function filter_signed(s,o)
 local n=u32(s,o);return n>=0x80000000 and n-0x100000000 or n
end
-- Bind the proven filter API/catalog for the weapon's dedicated ping helper.
-- This does not patch executable instructions or rewrite a shared query buffer.
local function filter_init()
 local exe=api.module('helldivers2.exe')
 local a=ptr(game+0x3326338)
 local fn=a and ptr(a+0x110)
 local destroy=a and ptr(a+8)
 local property=a and ptr(a+0x100)
 if not exe or fn~=exe+0x77DA10 or destroy~=exe+0x77F4F0 or property~=exe+0x77D840 then return false end
 local d=api.read_module(destroy,0x80)
 local q=api.read_module(property,0x66)
 local sig=api.read_module(fn,0x25)
 if not d or not q or not sig or hex(sig)~='415641574883ec48448bf24c8bf9488b15238404024885d20f8453010000448b92f8000000' then return false end
 if d:sub(0x57,0x59)~=string.char(0x4C,0x8D,0x35) or d:sub(0x74,0x76)~=string.char(0x4D,0x8D,0x86) or q:sub(0x3E,0x40)~=string.char(0x48,0x8D,0x05) then return false end
 local catalog=ptr(exe+0x27C5E48)
 local h=catalog and api.read(catalog,0x110)
 if not h or u32(h,0xF8)~=104 then return false end
 local ha,ma=ptr(catalog+0x100),ptr(catalog+0x108)
 if not ha or ma~=ha+416 then return false end
 local hashes,masks=api.read(ha,416),api.read(ma,1664)
 if not hashes or not masks or u32(hashes,26*4)~=filter_original or u32(hashes,37*4)~=filter_candidate or u32(hashes,57*4)~=0x74B9D874 then return false end
 if masks:sub(26*16+1,27*16)~=string.rep(string.char(0),16) or masks:sub(37*16+1,38*16)~=string.rep(string.char(0),16) then return false end
 for i=0,103 do
  local r=masks:sub(i*16+1,(i+1)*16)
  local old=bit.band(r:byte(4),4)~=0
  local new=bit.band(r:byte(5),32)~=0
  if (old~=new)~=(i==57 or i==94) or (i==57 and (not old or new)) then return false end
 end
 filter_table=destroy+0x5D+filter_signed(d,0x59)+u32(d,0x76)
 filter_worlds=property+0x44+filter_signed(q,0x40)
 filter_api=ffi.cast('void (*)(uint32_t*,uint32_t,uint32_t)',fn)
 return true
end
-- A physics actor handle is not the weapon entity. Resolve the helper actor
-- from the verified weapon and check its body/parent data before filtering.
local function filter_body(entity,id_expected)
 local m=ptr(game+0x3326450)
 local h=m and api.read(m,0x48)
 if not h or u32(h,0x0C)>u32(h,4) or u32(h,4)>16384 then return end
 local i=lookup(m+0x20,entity)
 if not i or i>=u32(h,0x0C) then return end
 local units,rows=ptr(m+0x38),ptr(m+0x40)
 local unit=units and ptr(units+i*8)
 local header=unit and api.read(unit,16)
 local row=rows and api.read(rows+i*24,24)
 if not header or u32(header,8)~=entity or header:sub(1,8)~=id_expected or not row then return end
 local id=u32(row,0)
 local world=math.floor(id/0x40000000)
 local group=math.floor(id/0x10000000)%4
 local descriptor=filter_table+(group+world*10)*64
 local d=api.read(descriptor,0x38)
 if not d then return end
 local index=bit.band(id,u32(d,0x28))
 local layout=u32(d,0x1C)
 local stride=layout%65536
 local keyoffset=math.floor(layout/65536)%256
 local dataoffset=math.floor(layout/0x1000000)
 local data=ptr(descriptor)
 if not data or index>=u32(d,0x24) or bit.band(id,u32(d,0x34))==0 or stride<24 or stride>4096 or keyoffset+4>stride or dataoffset+24>stride then return end
 local key=api.read(data+index*stride+keyoffset,4)
 if not key or u32(key,0)~=id then return end
 local record=api.read(data+index*stride+dataoffset,24)
 local getter=ptr(filter_worlds+world*0xB0+0x10)
 local vt=getter and ptr(getter)
 local fn=vt and ptr(vt+0x70)
 local signature=fn and api.read_module(fn,20)
 if signature~=string.char(0x8B,0xC2,0x25,0xFF,0xFF,0xFF,0,0x48,0x8D,4,0x80,0x48,0xC1,0xE0,0x05,0x48,0x03,0x41,0x18,0xC3) then return end
 local body_id=record and u32(record,0x14)
 local bodies=ptr(getter+0x18)
 if not body_id or not bodies then return end
 local body_index=bit.band(body_id,0xFFFFFF)
 if body_index>=0x100000 then return end
 local body=api.read(bodies+body_index*160,112)
 if not body then return end
 return {entity=entity,id=id_expected,actor=id,body_id=body_id,packed=u32(body,0x6C)}
end

local function filter_current()
 local old=filter_old
 if not old or ptr(native_addresses.owner_manager)~=old.owner then return end
 local r=filter_body(old.entity,old.id)
 if r and r.actor==old.actor and r.body_id==old.body_id then return r end
end
-- Queue a supported engine filter command. Keep other packed filter fields
-- intact; a filter catalog index is not a raw pointer or a global ping switch.
local function filter_send(r,hash,label)
 filter_buffer[0]=r.actor
 filter_api(filter_buffer,1,hash)
 filter_calls=filter_calls+1
 if DEBUG then emit('FILTER_'..label..' ENTITY='..r.entity..' ACTOR='..r.actor..' HASH='..hash..' CALL='..filter_calls) end 
end
filter_restore=function()
 if not filter_old then return end
 local r=filter_current()
 if not r then if DEBUG then emit('FILTER_FORGET STALE_ENTITY_OR_WORLD') end ;filter_old=nil;filter_armed=false;return end
 local index=bit.band(r.packed,127)
 filter_old.restoring=true
 if index==26 then
  if filter_old.confirmed or filter_old.restore_sent then if DEBUG then emit('FILTER_RESTORE_CONFIRMED ENTITY='..r.entity) end ;filter_old=nil end
 elseif index==37 then
  if not filter_old.restore_sent then
   filter_old.restore_sent=true;filter_old.deadline=tonumber(kernel.uss_win1_GetTickCount64())+1000
   filter_send(r,filter_original,'RESTORE_QUEUED')
  end
 else if DEBUG then emit('FILTER_FORGET EXTERNAL_FILTER_CHANGE') end ;filter_old=nil;filter_armed=false;filter_failed=true end
end
-- Exclude only the virtually stored weapon's verified ping helper. Restore
-- filtering when ownership/storage changes, and re-resolve recreated helpers.
local function filter_sync(p)
 if not pair and not filter_old then return end
 p=p or sample()
 local desired,id
 if not filter_failed and not paused and p and pair and check_pair(p) and p.primary~=0 and p.secondary~=0 then
  if not filter_api and not filter_init() then filter_failed=true;if DEBUG then emit('FILTER_TEST_REJECTED ENGINE_OR_CATALOG') end ;return end
  filter_armed=true;filter_owner=p.owner
  if p.support==pair.utility then desired,id=pair.support,pair.support_id
  elseif p.support==pair.support then desired,id=pair.utility,utility_id end
  local held=desired and attached_entity(desired,id)
  if not held or held.parent~=p.avatar then desired=nil end
 else filter_armed=false end
 -- Commands are queued, so confirmation/restoration can require later polls.
 -- Track the same helper instance and a deadline; discard stale world handles
 -- instead of restoring into a replacement object.
 if filter_old then
  local r=filter_current()
  if not r then filter_restore();return end
  local index=bit.band(r.packed,127)
  if filter_old.restoring or desired~=filter_old.entity then
   filter_restore()
  elseif index==37 then
   if not filter_old.confirmed then filter_old.confirmed=true;if DEBUG then emit('FILTER_APPLY_CONFIRMED ENTITY='..r.entity) end  end
  elseif index~=26 then filter_old=nil;filter_armed=false;filter_failed=true;if DEBUG then emit('FILTER_FORGET EXTERNAL_FILTER_CHANGE') end  end
  if not filter_failed and filter_old and tonumber(kernel.uss_win1_GetTickCount64())>filter_old.deadline and (filter_old.restoring or not filter_old.confirmed) then
   filter_failed=true;filter_armed=false;if DEBUG then emit('FILTER_TEST_TIMEOUT') end ;filter_restore()
  end
 end
 if desired and not filter_old and not filter_failed then
  local r=filter_body(desired,id)
  if not r then return end
  if bit.band(r.packed,127)~=26 then filter_failed=true;filter_armed=false;if DEBUG then emit('FILTER_TEST_REJECTED WEAPON_FILTER') end ;return end
  local now=tonumber(kernel.uss_win1_GetTickCount64())
  if now>=filter_until then filter_used=0;filter_until=now+10000 end
  if filter_used>=32 then filter_failed=true;filter_armed=false;if DEBUG then emit('FILTER_RATE_LIMIT') end ;return end
  filter_used=filter_used+1
  r.owner=p.owner;r.deadline=tonumber(kernel.uss_win1_GetTickCount64())+1000
  filter_old=r
  filter_send(r,filter_candidate,'APPLY_QUEUED')
 end
end
filter_stop=function()
 filter_armed=false;filter_failed=true;filter_restore()
 emit('FILTER_TEST_END CALLS='..filter_calls..' RESTORE_PENDING='..tostring(filter_old~=nil))
end
emit('FILTER_TEST_READY VERSION=0.7.1 AUTOMATIC CANDIDATE=37')

-- Let vanilla detach the normal inventory first, then drop the stored member
-- once. Require empty inventory and fresh owner/parent checks before the call.
-- This makes both weapons recoverable without duplicating the normal death drop.
local function death_release(p)
 if not pair or pair.drop_attempted then return end
 pair.drop_attempted=true
 local real=pair.real
 if real~=pair.support and real~=pair.utility then if DEBUG then emit('DEATH_DROP_SKIP UNKNOWN_REAL_SLOT') end ;return end
 local stored=real==pair.support and pair.utility or pair.support
 local stored_id=stored==pair.utility and utility_id or pair.support_id
 local real_id=real==pair.utility and utility_id or pair.support_id
 local attached=attached_entity(stored,stored_id)
 local dropped=attached_entity(real,real_id)
 if not attached or attached.parent~=pair.avatar then if DEBUG then emit('DEATH_DROP_SKIP STORED_ENTITY_OR_PARENT_CHANGED') end ;return end
 if not dropped or dropped.parent~=0 then if DEBUG then emit('DEATH_DROP_SKIP NORMAL_SUPPORT_NOT_DETACHED') end ;return end
 local fresh=sample()
 if not fresh or not same_context(p,fresh) or fresh.row~=p.row
  or fresh.avatar~=pair.avatar or fresh.owner~=pair.owner
  or ptr(native_addresses.owner_manager)~=pair.owner or ptr(native_addresses.inventory_manager)~=fresh.inventory
  or fresh.primary~=0 or fresh.secondary~=0 or fresh.support~=0 or fresh.backpack~=0 then
  if DEBUG then emit('DEATH_DROP_SKIP CONTEXT_CHANGED') end ;return
 end
 filter_restore()
 if not native_guard() then return end
 if DEBUG then emit('DEATH_DROP_BEGIN AVATAR='..pair.avatar..' STORED='..stored..' NORMAL_DROPPED='..real) end 
 death_drop_calls=death_drop_calls+1
 drop_entity(ffi.cast('void*',fresh.inventory),pair.avatar,stored,0,nil,1)
 local after=attached_entity(stored,stored_id)
 local g=interaction(stored)
 local latest=sample()
 if DEBUG then emit('DEATH_DROP_END ENTITY='..stored..' PARENT='..tostring(after and after.parent)
  ..' GROUND_ZONE='..tostring(g and g.zone)..' GROUND_GATE='..tostring(g and g.gate)
  ..' INVENTORY_UNCHANGED='..tostring(latest and same_context(fresh,latest) and latest.row==fresh.row)) end 
end


-- Incoming weapons replace their own class: new utility replaces utility;
-- new ordinary replaces ordinary. Keep the opposite-class member even if
-- vanilla briefly dropped it during pickup. Never pair two ordinary weapons
-- or two utilities. Utility selection follows the live pickup preference,
-- with the explicit exception for replacing a utility already in hand.
-- USS-SOURCE-BEGIN primary_readiness_trace.lua
-- Bounded diagnostics for one existing primary wait. No memory reads, native calls or gameplay guards.
-- tick_ms comes from the guard's monotonic clock; milliseconds are local only.
local function primary_readiness_trace(start_ms,origin)
    local rows,conditions={},{}
    local names={'AVATAR','CHAR_SELECTED','INVENTORY_SELECTED','HELD_PRIMARY',
        'ROW1C','GLOBAL_GUARD','STATE_BYTE','TRANSITION'}
    local count,max_gap,previous_tick,first_ready,truncated=0,0,nil,nil,false
    local last_refused='NONE'
    local trace={}
    function trace.sample(g)
        local elapsed=g.tick_ms-start_ms
        local settled=g.phase==0 and g.timer_bits==0 and g.bit47==0
        local holstering=g.phase==1 and (g.prior==2 or g.prior==3)
            and g.pending==1 and g.bit47==1 and g.timer_bits>0 and g.timer_bits<0x7F800000
        -- Mirror the named conditions for explanation only. g.ready, produced
        -- by the original guard, remains the sole gameplay admission decision.
        local passed={g.row_avatar==g.avatar,g.selected==1,g.active==1,
            g.held==g.primary,g.row1c==0,g.global_guard==0,g.state_byte==0,
            settled or holstering}
        local blocked={}
        for i,name in ipairs(names)do
            local c=conditions[name] or {};conditions[name]=c
            if not passed[i]then
                blocked[#blocked+1]=name;c.last_blocked=elapsed;c.clear_after_last=nil
            elseif c.last_blocked and not c.clear_after_last then c.clear_after_last=elapsed end
        end
        local blockers=#blocked>0 and table.concat(blocked,',')or 'NONE'
        if not g.ready then last_refused=blockers
        elseif not first_ready then first_ready=elapsed end
        if previous_tick then max_gap=math.max(max_gap,g.tick_ms-previous_tick)end
        previous_tick=g.tick_ms;count=count+1
        local row={elapsed=elapsed,guard=g,blockers=blockers}
        -- 81 rows cover a nominal25ms/2000ms wait. Retain the latest sample if
        -- unexpected repeated sampling reaches the cap; never grow indefinitely.
        if #rows<81 then rows[#rows+1]=row else rows[81]=row;truncated=true end
    end
    function trace.finish(outcome)
        local result={'REPLACEMENT_PRIMARY_TIMING OUTCOME='..outcome..' SAMPLES='..count
            ..' ORIGIN='..(origin or 'UNSPECIFIED')
            ..' FIRST_SAMPLE_MS='..tostring(rows[1] and rows[1].elapsed)
            ..' FIRST_READY_MS='..tostring(first_ready)..' MAX_SAMPLE_GAP_MS='..max_gap
            ..' LAST_REFUSED_BLOCKERS='..last_refused..' TRUNCATED='..tostring(truncated)
            ..' NOMINAL_INTERVAL_MS=25 SAMPLE_LIMIT=81'}
        for _,row in ipairs(rows)do
            local g=row.guard
            local fields={'MS='..row.elapsed,'READY='..tostring(g.ready),'BLOCKERS='..row.blockers}
            for _,key in ipairs({'avatar','row_avatar','primary','active','held','selected','prior','phase','pending',
                'timer_bits','row1c','bit47','state_byte','global_guard','early_replacement'})do
                fields[#fields+1]=key:upper()..'='..tostring(g[key])
            end
            result[#result+1]='REPLACEMENT_PRIMARY_SAMPLE '..table.concat(fields,' ')
        end
        for _,name in ipairs(names)do
            local c=conditions[name]
            if c and c.last_blocked then
                result[#result+1]='REPLACEMENT_PRIMARY_CONDITION NAME='..name
                    ..' LAST_BLOCKED_MS='..c.last_blocked
                    ..' CLEAR_AFTER_LAST_MS='..tostring(c.clear_after_last)
            end
        end
        return result
    end
    return trace
end
-- USS-SOURCE-END primary_readiness_trace.lua

-- USS-SOURCE-BEGIN replacement_transaction.lua
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
        must(state.mission and (state.host or state.joined_client) and state.formed and not state.disabled
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
            epoch=state.epoch,peers=peers(),host_role=state.host}
    end
    function s.seed(p)
        -- Seed only reads the intended replacement. begin() freshly checks the
        -- route before its filter restoration/primary selection/native preparation.
        local state=publication.status()
        must(state.mission and (state.host or state.joined_client) and state.formed and state.targets
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
            epoch=state.epoch,peers=peers(),host_role=state.host}
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
        must(state.host==w.host_role,'CONTROL_OWNER_ROLE_CHANGED')
        must(state.mission and (state.host or state.joined_client) and state.epoch==w.epoch
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
-- USS-SOURCE-END replacement_transaction.lua

-- USS-SOURCE-BEGIN formation_transaction.lua
-- Pair setup and joined-client draws may wait across updates for primary readiness.
-- Keep the original operation order and publish the actual incoming member before
-- drawing. A refusal before entry can wait; an uncertain/failed native entry cannot.
local function queue_formation(p,order,selected,pickup_setting,draw_target,draw_id)
 if not publication then return false end
 local utility_exit=order=='UTILITY_EXIT'
 local managed_draw=order=='CLIENT_DRAW'
 local state=publication.status()
 if utility_exit or managed_draw then
  -- This is an existing pair whose local support membership would otherwise
  -- change without an incoming publication. Unsupported scopes retain Core's
  -- original local restoration; do not invent a new network contract here.
  if not state.mission or not (state.host or state.joined_client) or not state.formed
   or state.disabled or state.suspended then return false end
  if managed_draw then
   if not state.joined_client or selected~=3 then return false end
   local expected=draw_target==pair.utility and utility_id
    or draw_target==pair.support and pair.support_id
   if not expected or draw_id~=expected then return false end
  end
 elseif not publication.pending_formation(p)then return false end
 local utility_first=order=='UTILITY_FIRST'
 local steps=managed_draw and {
  {target=draw_target,id=draw_id,label=draw_target==pair.utility and 'DRAW_UTILITY' or 'DRAW_SUPPORT',stage='POST_ATTACH_INCOMING'},
 } or utility_exit and {
  {target=pair.support,id=pair.support_id,label='STORE_UTILITY_ON_WEAPON_CHANGE',stage='POST_ATTACH_INCOMING'},
 } or utility_first and {
  {target=pair.utility,id=utility_id,label='CAPTURE_UTILITY_FIRST',stage='UTILITY_FIRST_CAPTURE'},
  {target=pair.support,id=pair.support_id,label='RESTORE_UTILITY_FIRST_SUPPORT',stage='POST_ATTACH_INCOMING'},
 } or {{target=pair.support,id=pair.support_id,label='STORE_NEW_UTILITY',stage='POST_ATTACH_INCOMING'}}
 if not managed_draw and not utility_exit and not utility_first and pickup_setting then
  steps[#steps+1]={target=pair.utility,id=utility_id,label='DRAW_UTILITY',stage='POST_ATTACH_INCOMING'}
 end
 formation_pending={pair=pair,context=p,epoch=state.epoch,host_role=state.host,utility_id=utility_id,
  ordinary_id=pair.support_id,support=p.support,steps=steps,index=1,selected=selected,origin_selected=p.active,
  deadline=tonumber(kernel.uss_win1_GetTickCount64())+2000,order=order,pickup=pickup_setting,utility_exit=utility_exit,managed_draw=managed_draw}
 -- The pair remains owned by this work record until completion or cancellation.
 pending_utility=nil
 emit('FORMATION_DEFERRED_BEGIN ORDER='..order..' SELECTED='..selected)
 return true
end
-- Drop/consumption has already restored the survivor locally. Hold only this
-- finite operation across updates; the actual Core pair is cleared immediately.
local function queue_lone_publication(p,target,id,outgoing,outgoing_id)
 if not publication then return end
 local recovery=publication.begin_lone(p,target,id,outgoing,outgoing_id)
 if not recovery then return end -- Solo/unsupported scope keeps local behavior.
 formation_pending={lone=recovery,context=p,selected=p.active,
  deadline=tonumber(kernel.uss_win1_GetTickCount64())+2000}
 emit('LONE_PUBLICATION_BEGIN TARGET='..target..' OUTGOING='..outgoing)
end
local function service_lone_publication(p,now,input,work)
 local ok,why=pcall(function()
  local recovery=work.lone
  if now>=work.deadline or input or paused or down or pair
   or not publication.replacement_scope_ready(recovery.epoch)
   or not same_context(work.context,p) or not can_switch(p)
   or p.primary~=work.context.primary or p.secondary~=work.context.secondary
   or p.support~=recovery.target or identity(recovery.target)~=recovery.id then
   error('LONE_CONTEXT_OR_INPUT_CHANGED',0)
  end
  local survivor=attached_entity(recovery.target,recovery.id)
  if not survivor or survivor.parent~=p.avatar then error('LONE_SURVIVOR_PARENT_CHANGED',0)end
  if not work.primary_requested then
   if p.active~=work.selected then error('LONE_SELECTION_CHANGED',0)end
   if p.active~=1 and not choose(p,1)then error('LONE_PRIMARY_SELECTION_FAILED',0)end
   work.primary_requested=true;p=sample()
  end
  if not p or p.active~=1 then error('LONE_SELECTION_CHANGED',0)end
  local ready,reason=publication.lone_primary_ready(p,recovery)
  if not ready then
   if reason=='PRIMARY_BOUNDARY_NOT_READY' then return end
   error(reason,0)
  end
  local published,reason=publication.lone_boundary(p,recovery)
  if not published then error(reason,0)end
  -- Selection is restored only after a returned send and a fresh local check.
  local after=sample()
  if not after or not same_context(work.context,after) or after.support~=recovery.target
   or after.active~=1 or after.primary~=work.context.primary
   or after.secondary~=work.context.secondary or not can_switch(after) then error('LONE_FINAL_CONTEXT_CHANGED',0)end
  if work.selected~=1 and not choose(after,work.selected)then error('LONE_FINAL_SELECTION_FAILED',0)end
  formation_pending=nil
  emit('LONE_PUBLICATION_COMPLETE TARGET='..recovery.target..' RESTORED_SELECTED='..work.selected)
 end)
 if not ok then
  formation_pending=nil;paused=true
  emit('LONE_PUBLICATION_CANCEL REASON='..tostring(why)..' RETRY=false')
 end
 return true
end
local function service_formation(p,now,input)
 local work=formation_pending
 if not work then return false end
 if work.lone then return service_lone_publication(p,now,input,work)end
 local function cancel(reason)
  formation_pending=nil
  if pair==work.pair then paused=true end
  emit('FORMATION_DEFERRED_CANCEL REASON='..tostring(reason))
 end
 local ok,why=pcall(function()
  local state=publication.status()
  if work.utility_exit and input then error('INPUT_DURING_UTILITY_EXIT',0)end
  if work.managed_draw and input then error('INPUT_DURING_CLIENT_DRAW',0)end
  if now>=work.deadline then error('TIMEOUT',0)end
  if not state.mission or not (state.host or state.joined_client) or state.host~=work.host_role or state.epoch~=work.epoch
   or state.disabled or state.suspended or paused or down or pair~=work.pair
   or not same_context(work.context,p) or not can_switch(p)
   or utility_id~=work.utility_id or pair.support_id~=work.ordinary_id
   or identity(pair.utility)~=work.utility_id or identity(pair.support)~=work.ordinary_id
   or p.support~=work.support then error('CONTEXT_OR_PICKUP_CHANGED',0)end
  if (work.utility_exit or work.managed_draw) and (p.primary~=work.context.primary or p.secondary~=work.context.secondary
   or(not work.primary_requested and p.active~=(work.origin_selected or work.selected)))then
   error('UTILITY_EXIT_SELECTION_CHANGED',0)
  end
  for _,member in ipairs({{pair.utility,work.utility_id},{pair.support,work.ordinary_id}})do
   local item=attached_entity(member[1],member[2])
   if not item or (item.parent~=0 and item.parent~=p.avatar)
    or((work.utility_exit or work.managed_draw) and item.parent~=p.avatar)then error('PARENT_CHANGED',0)end
  end
  if not work.primary_requested then
   -- Request once. Never force primary again after intervening player input.
   if p.active~=1 and not choose(p,1)then error('PRIMARY_SELECTION_FAILED',0)end
   work.primary_requested=true
   p=sample()
  end
  if not p or p.active~=1 then error('SELECTION_CHANGED',0)end
  local ready,reason=publication.formation_primary_ready(p)
  if not ready then
   if reason=='PRIMARY_BOUNDARY_NOT_READY' then return end
   error(reason,0)
  end
  local step=work.steps[work.index]
  if not work.attached then
   -- Consume attachment before entry; failures cancel, never retry it.
   work.attached=true
   work.outgoing=p.support
   if not switch_entity(p,step.target,step.id,false,step.label,work.managed_draw)then error('ATTACH_FAILED',0)end
   work.support=step.target
  end
  local after=sample()
  if not after or not same_context(work.context,after) or after.support~=step.target
   or after.active~=1 then error('ATTACH_CONTEXT_CHANGED',0)end
  -- switch_entity may already have published. boundary deduplicates a returned
  -- member and refuses uncertain entry; it only waits on a pre-entry guard refusal.
  local published,reason=publication.boundary(after,false,step.stage)
  if not published then
   if reason=='PRIMARY_BOUNDARY_NOT_READY' then return end
   error(reason,0)
  end
  work.index=work.index+1;work.attached=false
  if work.index<=#work.steps then return end
  if not check_pair(after)then error('FINAL_PAIR_CHANGED',0)end
  if after.active~=work.selected and not choose(after,work.selected)then error('FINAL_SELECTION_FAILED',0)end
  pair.real=step.target;pair.selected=work.selected
  pair.utility_drawn=step.target==pair.utility and work.selected==3
  last_support={entity=pair.support,id=pair.support_id};pending_utility=nil
  formation_pending=nil
  if work.managed_draw then
   -- The attachment ran while primary was selected. Acknowledge only after
   -- the final draw; the HUD callback freshly checks selection and pair identity.
   local acknowledge=_G.UtilitySupportSlotContext.acknowledge_inventory_change
   if acknowledge then acknowledge(work.outgoing,step.target,step.label)end
   emit('CLIENT_DRAW_COMPLETE TARGET='..step.target..' ACTIVE='..work.selected)
   return
  end
  if work.utility_exit then
   emit('UTILITY_EXIT_COMPLETE SUPPORT='..pair.support..' RESTORED_SELECTED='..work.selected)
   return
  end
  emit('PAIR_READY ORDER='..work.order..' UTILITY='..pair.utility..' SUPPORT='..pair.support
   ..' SWITCH_WEAPON_ON_PICKUP='..tostring(work.pickup))
 end)
 if not ok then cancel(why)end
 return true
end
-- USS-SOURCE-END formation_transaction.lua

local function replace_support(p)
 filter_restore()
 local new_id=identity(p.support)
 if not new_id or not can_switch(p) then return end
 -- Read before any replacement/drop writes, including the drawn exception.
 -- A regular incoming weapon keeps vanilla selection behavior unchanged.
 local pickup_setting
 if is_utility(new_id)then
  pickup_setting=utility_pickup_setting()
  if pickup_setting==nil then return end
 end
 local previous_active=pair.selected or p.active
 local real=pair.real
 if real~=pair.utility and real~=pair.support then
  paused=true;emit('PAUSED REPLACEMENT_LAST_REAL_UNKNOWN');return
 end
 if not is_utility(utility_id) or is_utility(pair.support_id) then
  paused=true;emit('PAUSED REPLACEMENT_INVALID_CLASSES');return
 end
 -- Classify the incoming identity, then retain the opposite-class member.
 -- The old normal member may already have been detached by vanilla pickup.
 local kept=is_utility(new_id) and pair.support or pair.utility
 local old=kept==pair.utility and pair.support or pair.utility
 local kept_id=kept==pair.utility and utility_id or pair.support_id
 local old_id=old==pair.utility and utility_id or pair.support_id
 if is_utility(kept_id)==is_utility(new_id) then
  paused=true;emit('PAUSED REPLACEMENT_SAME_CLASS');return
 end
 local k=attached_entity(kept,kept_id)
 local o=attached_entity(old,old_id)
 local n=attached_entity(p.support,new_id)
 if not k or (k.parent~=p.avatar and k.parent~=0) or not o
  or (o.parent~=0 and o.parent~=p.avatar) or not n or n.parent~=p.avatar
  or (kept~=real and k.parent~=p.avatar) then
  paused=true;emit('PAUSED REPLACEMENT_ENTITY_OR_PARENT_CHANGED');return
 end
 local released=real==kept and k or o
 if released.parent==p.avatar then return end
 local fresh=sample()
 if not fresh or not same_context(p,fresh) or fresh.row~=p.row
  or fresh.avatar~=pair.avatar or fresh.owner~=pair.owner
  or ptr(native_addresses.owner_manager)~=pair.owner or ptr(native_addresses.inventory_manager)~=fresh.inventory then
  paused=true;emit('PAUSED REPLACEMENT_CONTEXT_CHANGED');return
 end
 -- Keep the old pair roles until its outgoing member has been published
 -- as real support and dropped natively. The former local virtual drop here
 -- left that member attached on the observer.
 local state=publication and publication.status()
 if state and state.mission and (state.host or state.joined_client) and state.formed
  and real==kept and k.parent==0 and o.parent==p.avatar then
  -- This shape needs native publication of the old virtual member before drop.
  -- An unrecognized selection must not fall back to its known local-only cleanup.
  if not replacement_origin(p,pair,kept)then
   paused=true;emit('PAUSED REPLACEMENT_ORIGIN_NOT_READY');return
  end
  local services=replacement_transaction_services(true)
  local started=services.clock()
  local ok,work=pcall(function()
   local w=services.seed(fresh);services.begin(w,fresh);publication.begin_replacement(w);return w
  end)
  if not ok then paused=true;emit('PAUSED REPLACEMENT_NATIVE_PREPARE_FAILED REASON='..tostring(work));return end
  local now=services.clock()
  replacement_pending={native_transaction=true,pair=pair,work=work,services=services,
   started=started,prepare_ms=now-started,next_check=now+25,deadline=work.primary_deadline}
  emit('REPLACEMENT_NATIVE_BEGIN OLD='..old..' KEPT='..kept..' INCOMING='..p.support
   ..' WAIT_MS=2000 CHECK_MS=25')
  return
 end
 trace_replacement('BEGIN',fresh,p.support,old,kept)
 -- If the member being replaced is still virtual/attached, vanilla did not
 -- drop it. Drop it explicitly and verify detachment before rewriting roles.
 if o.parent==p.avatar then
  if not native_guard() then return end
  replacement_drop_calls=replacement_drop_calls+1
  drop_entity(ffi.cast('void*',fresh.inventory),p.avatar,old,0,nil,1)
  local after=attached_entity(old,old_id);local verify=sample()
  if not after or after.parent~=0 or not verify or not same_context(fresh,verify) or verify.row~=fresh.row then
   paused=true;emit('PAUSED REPLACEMENT_DROP_VERIFY_FAILED');return
  end
 end
 trace_replacement('AFTER_OLD_DROP',nil,p.support,old,kept)
 if publication then publication.reset()end
 local utility_was_drawn=real==pair.utility and pair.utility_drawn==true
 local draw_utility=is_utility(new_id) and (pickup_setting or utility_was_drawn)
 local held=p.active==3
 if is_utility(new_id)then held=draw_utility end
 if is_utility(new_id) then
  pair.utility=p.support;utility_id=new_id
  pair.support=kept;pair.support_id=kept_id
 else
  pair.utility=kept;utility_id=kept_id
  pair.support=p.support;pair.support_id=new_id
 end
 pair.real=p.support
 pair.drop_attempted=nil;pair.death_since=nil
 last_support={entity=pair.support,id=pair.support_id}
 -- If vanilla dropped the member we intended to retain, reattach that exact
 -- entity, then restore the newly picked weapon as the normal-slot member.
 -- A drawn incoming utility needs ordinary formation before its own incoming
 -- publication. Reuse the same retain/restore path even if ordinary stayed owned.
 local publish_drawn_utility=draw_utility and publication and publication.pending_formation(p)
 -- Only the measured ordinary-replacement recovery needs this deferred path.
 -- Vanilla dropped the retained utility, so publishing ordinary alone omits it.
 -- Preserve the pickup's selection now; do not draw again before publication.
 if not is_utility(new_id) and k.parent==0 and publication and publication.pending_formation(p) then
  local current=sample()
  if not same_context(p,current) or current.row~=p.row or not native_guard()
   or (current.active~=1 and not choose(current,1)) then
   paused=true;emit('PAUSED REPLACEMENT_PRIMARY_PREPARE_FAILED');return
  end
  local now=tonumber(kernel.uss_win1_GetTickCount64())
  replacement_pending={pair=pair,context=p,utility=kept,utility_id=kept_id,
   ordinary=p.support,ordinary_id=new_id,old=old,old_id=old_id,
   selected=p.active,epoch=publication.status().epoch,started=now,next_check=now+25,deadline=now+2000}
  pair.utility_drawn=false
  emit('REPLACEMENT_DEFERRED_BEGIN OLD='..old..' UTILITY='..kept..' ORDINARY='..p.support
   ..' RESTORE_SELECTED='..p.active..' WAIT_MS=2000 CHECK_MS=25')
  return
 end
 if k.parent==0 or publish_drawn_utility then
  fresh=sample()
  if not fresh or not switch_entity(fresh,kept,kept_id,false,'REPLACE_RETAIN_SPECIAL') then
   paused=true;emit('PAUSED REPLACEMENT_SPECIAL_ATTACH_FAILED');return
  end
  fresh=sample()
  if not fresh or not switch_entity(fresh,p.support,new_id,held,'REPLACE_KEEP_NEW_PICKUP') then
   paused=true;emit('PAUSED REPLACEMENT_NEW_ATTACH_FAILED');return
  end
 end
 trace_replacement('AFTER_RETAIN_ATTACH',nil,p.support,old,kept)
 local expected_real=p.support
 if is_utility(new_id) and not draw_utility then
  fresh=sample()
  if not fresh or not switch_entity(fresh,pair.support,pair.support_id,previous_active==3,'STORE_REPLACEMENT_UTILITY')then
   paused=true;emit('PAUSED UTILITY_REPLACEMENT_STORE_FAILED');return
  end
  expected_real=pair.support;pair.real=expected_real
 end
 -- No restores the previous selected weapon. Yes (or replacement of a
 -- drawn utility) explicitly selects the incoming utility in the native slot.
 local selected=is_utility(new_id) and (draw_utility and 3 or previous_active) or p.active
 local final=sample()
 if final and publication then final=publish_replacement_pair(final)end
 if not final then paused=true;emit('PAUSED REPLACEMENT_PUBLICATION_BOUNDARY_FAILED');return end
 if final and (final.active~=selected or (is_utility(new_id) and draw_utility))then
  if not choose(final,selected)then paused=true;emit('PAUSED REPLACEMENT_SELECTION_FAILED');return end
  final=sample()
 end
 pair.real=expected_real;pair.selected=selected
 pair.utility_drawn=expected_real==pair.utility and selected==3
 if not final or not same_context(p,final) or final.support~=expected_real or not check_pair(final) then
  paused=true;emit('PAUSED REPLACEMENT_PAIR_VERIFY_FAILED');return
 end
 trace_replacement('COMPLETE',final,p.support,old,kept)
 if DEBUG then emit('REPLACE_TYPED DROPPED='..old..' RETAINED='..kept..' NEW_REAL='..p.support) end 
end


-- Readiness polls every25ms until the unchanged2000ms deadline. Once ready,
-- run one guarded stage per update; no extra fixed delay between stages.
-- Consume each stage index BEFORE native entry. Cancellation discards the rest;
-- never restart a partially completed or uncertain transaction from stage one.
-- Diagnostics are optional and cannot change gameplay or retry decisions.
local function record_primary_wait(work,guard,outcome)
 if not DEBUG then return end
 local ok=pcall(function()
  if not work.readiness_trace then
   local started=work.work and work.work.primary_requested_at or work.started
   work.readiness_trace=primary_readiness_trace(started,
    work.work and 'PRIMARY_REQUEST' or 'WAIT_SCHEDULED')
  end
  if guard then work.readiness_trace.sample(guard)end
  if outcome then
   for _,line in ipairs(work.readiness_trace.finish(outcome))do emit(line)end
   work.readiness_trace=nil
  end
 end)
 if not ok then work.readiness_trace=nil end
end
local replacement_stages={
 {phase='WAIT_PRIMARY',method='recover'},
 {phase='UTILITY',method='represent'},
 {phase='OLD',method='drop'},
 {phase='DROPPED',method='finish'},
}
local function service_replacement(p,now,input)
 local work=replacement_pending
 if not work then return false end
 local function cancel(reason)
  replacement_pending=nil
  if pair==work.pair then paused=true end
  publication.end_replacement()
  emit('REPLACEMENT_DEFERRED_CANCEL REASON='..reason)
  record_primary_wait(work,nil,'CANCEL_'..tostring(reason))
 end
 if work.native_transaction and input then cancel('INPUT_DURING_REPLACEMENT');return true end
 if now<work.next_check then return true end
 if now>=work.deadline then cancel('TIMEOUT');return true end
 if work.native_transaction then
  local services,w=work.services,work.work
  local ok,reason=pcall(function()
   if not work.stage_index then
    work.next_check=now+25 -- Only primary readiness waits use the25ms schedule.
    services.validate(w,p,'WAIT_PRIMARY')
    local ready,why,guard=services.ready(p)
    record_primary_wait(work,guard)
    if not ready then
     if why~='PRIMARY_BOUNDARY_NOT_READY' then error(why,0)end
     return
    end
    work.stage_index=1
    if DEBUG then work.ready_at=services.clock();work.parts={}end
   end
   if work.last_stage_tick==tick then return end
   local index=work.stage_index
   local stage=assert(replacement_stages[index],'REPLACEMENT_STAGE_INDEX')
   -- Both selection and all expected parents/identities are rechecked after
   -- the intervening engine update. finish also validates after its route check.
   if stage.method~='finish' then services.validate(w,services.sample(),stage.phase)end
   work.last_stage_tick=tick
   work.stage_index=index+1 -- Consumed even if native entry/return becomes uncertain.
   work.next_check=now -- Next update, not another25ms delay.
   local start=DEBUG and services.clock()
   services[stage.method](w)
   if DEBUG then work.parts[#work.parts+1]=stage.method:upper()..'_MS='..(services.clock()-start)end
   if index==#replacement_stages then
    replacement_pending=nil
    if DEBUG then emit('REPLACEMENT_NATIVE_COMPLETE PREPARE_MS='..work.prepare_ms
     ..' PRIMARY_WAIT_MS='..(work.ready_at-work.started-work.prepare_ms)
     ..' '..table.concat(work.parts,' ')..' TOTAL_MS='..(services.clock()-work.started))end
    record_primary_wait(work,nil,'COMPLETE')
    publication.end_replacement()
   end
  end)
  if not ok then cancel(tostring(reason))end
  return true
 end
 work.next_check=now+25
 local state=publication.status()
 if not state.mission or not (state.host or state.joined_client) or state.epoch~=work.epoch then
  cancel('MISSION_OR_AUTHORITY_CHANGED');return true
 end
 if paused or down or pair~=work.pair or not same_context(work.context,p)
  or p.support~=work.ordinary or p.active~=1 or not can_switch(p)
  or pair.utility~=work.utility or pair.support~=work.ordinary
  or identity(work.utility)~=work.utility_id or identity(work.ordinary)~=work.ordinary_id then
  cancel('CONTEXT_OR_SELECTION_CHANGED');return true
 end
 local kept=attached_entity(work.utility,work.utility_id)
 local incoming=attached_entity(work.ordinary,work.ordinary_id)
 local old=attached_entity(work.old,work.old_id)
 if not kept or kept.parent~=0 or not incoming or incoming.parent~=p.avatar
  or not old or old.parent~=0 then cancel('PARENTS_CHANGED');return true end
 local ready,reason,guard=publication.replacement_primary_ready(p)
 record_primary_wait(work,guard)
 if not ready then
  if reason~='PRIMARY_BOUNDARY_NOT_READY' then cancel(tostring(reason))end
  return true
 end
 replacement_pending=nil
 emit('REPLACEMENT_DEFERRED_READY UTILITY='..work.utility..' ORDINARY='..work.ordinary)
 if not switch_entity(p,work.utility,work.utility_id,false,'REPLACE_RECOVER_UTILITY') then
  paused=true;emit('PAUSED REPLACEMENT_DEFERRED_UTILITY_FAILED');return true
 end
 local current=sample()
 if not current or not switch_entity(current,work.ordinary,work.ordinary_id,false,'REPLACE_RESTORE_ORDINARY') then
  paused=true;emit('PAUSED REPLACEMENT_DEFERRED_ORDINARY_FAILED');return true
 end
 local final=sample()
 if not final or not same_context(work.context,final) or final.support~=work.ordinary
  or not check_pair(final) or (final.active~=work.selected and not choose(final,work.selected)) then
  paused=true;emit('PAUSED REPLACEMENT_DEFERRED_SELECTION_FAILED');return true
 end
 pair.real=work.ordinary;pair.selected=work.selected;pair.utility_drawn=false
 trace_replacement('COMPLETE',nil,work.ordinary,work.old,work.utility)
 emit('REPLACE_TYPED DROPPED='..work.old..' RETAINED='..work.utility..' NEW_REAL='..work.ordinary)
 emit('REPLACEMENT_DEFERRED_COMPLETE RESTORED_SELECTED='..work.selected)
 record_primary_wait(work,nil,'COMPLETE')
 return true
end

-- Shared bridge to resupply: expose only the existing core helpers/context.
-- fields names the inventory snapshot fields; the installer supports either script load order.
-- UtilitySupportSlotUpdate is called by this core, so the addon needs no independent update hook.
-- USS-SOURCE-BEGIN inventory_observation.lua
-- Share only an observational inventory snapshot within one addon callback.
-- Never seed from the earlier core sample: selection/attachment may have changed
-- it. Never cache ammo, source policy or a native operation's verification read.
local function inventory_observation(sample)
    -- These locals belong to this factory instance. The returned functions
    -- are closures: they share the locals without exposing them through _G.
    -- `read` records an attempted sample, including a nil result. Retrying a
    -- failed read inside the same callback could mix different game states.
    local active, read, cached = false, false, nil
    return {
        begin = function()
            active = true;
            read = false;
            cached = nil
        end,
        sample = function()
            -- Outside the bracketed callback, always obtain a fresh snapshot.
            if not active then return sample() end
            if not read then
                cached = sample();
                read = true
            end
            return cached
        end,
        finish = function()
            -- Release the table reference; do not retain it across updates.
            active = false;
            read = false;
            cached = nil
        end
    }
end
-- USS-SOURCE-END inventory_observation.lua
local observation=inventory_observation(sample)
_G.UtilitySupportSlotContext={
 native_addresses=native_addresses,
 debug=DEBUG,
 fields={avatar='avatar',address='address',support='support',backpack='backpack',support_id='support_id'},
 memory=api,
 game_base=game,
 ffi=ffi,
 sample_inventory=sample,
 observe_inventory=observation.sample,
 current_pair=function()return not paused and not down and pair end,
 weapon_identity=identity,
 is_utility=is_utility,
 attached_entity=attached_entity,
 emit=emit,
 read_u32=u32,
 lookup=lookup,
 validate_pair=check_pair,
}
-- Automatic publication is part of Core; tools/readers are optional observers.
local publication_ok,publication_result=pcall(inventory_publication_adapter,
 _G.UtilitySupportSlotContext,kernel,held_weapon,SOLO_SILO_ID,unhex('609eb048dc0b9d2e'))
if publication_ok then
 publication=publication_result
 _G.UtilitySupportSlotContext.inventory_publication=publication
else emit('MP_PUBLICATION DISABLED REASON='..tostring(publication_result))end
if _G.UtilitySupportSlotInstall then _G.UtilitySupportSlotInstall(_G.UtilitySupportSlotContext)end
local previous,shutdown=_G.update,_G.shutdown
-- Allocate this helper once, preserving nils and multiple callback returns.
local function pack_returns(...)return {n=select('#',...),...}end
-- Chain the previous update and preserve all return values. Input is checked
-- each update for responsiveness; heavier inventory work is throttled to 25 ms.
-- Resupply/classification have their own bounded schedules on this same hook.
_G.update=function(...)
 local result
 if previous then result=pack_returns(previous(...))end
 tick=tick+1
 updates=updates+1
 local started=tonumber(kernel.uss_win1_GetTickCount64())
 local did_poll=false
 local poll_sample,poll_calls
 local success,why=pcall(function()
  local dedicated,mode_changed=shortcut.poll(started,not paused and not down)
  -- Dedicated mode ignores support gestures while ordinary support is in the
  -- real slot. Skip the native input reads instead of reading then discarding.
  local support_suspended=shortcut.configured() and pair and pair.real~=pair.utility
  -- Invalidate suspended history. When support controls resume, track_input()
  -- rebuilds the context below and primes it to require an observed release.
  -- This also prevents a mode change from completing an earlier held gesture.
  if mode_changed or support_suspended then
   input_cache=nil;reset_input_for_mode=true
  end
  local hit=not mode_changed and not support_suspended and support_action()
  hit=hit or dedicated
  -- An intentional input edge can bypass the poll delay. Otherwise skip the
  -- heavy path until due; this is throttling, not sleeping the game thread.
  local replacement_advancing=replacement_pending and replacement_pending.native_transaction
   and replacement_pending.stage_index
  if not hit and not replacement_advancing and started<next_poll then return end
  next_poll=started+25
  did_poll=true;polls=polls+1
  local p=sample()
  poll_sample=p;poll_calls=calls+death_drop_calls+replacement_drop_calls
  if not p then
   if avatar then reset(nil) end
   return
  end
  if avatar~=p.avatar or owner~=p.owner then reset(p) end
  if service_replacement(p,started,hit) then return end
  if service_formation(p,started,hit) then return end
  if publication then publication.poll(p)end
  -- Do not rebuild an input cache on every inventory poll while suspended.
  if not support_suspended then track_input(p) end
  if reset_input_for_mode and input_cache then
   -- Require observed releases under the new mode. Rebuilding the cache alone
   -- would prime down=false, which could treat an existing press as a new edge.
   input_cache.down=true;input_cache.held=true
   input_cache.armed=false;input_cache.fired=true
   reset_input_for_mode=false
  end
  scan(p)
  if paused then
   if hit then emit('SUPPORT_ACTION_BLOCKED PAUSED SUPPORT='..p.support..' ACTIVE='..p.active..' STORED_UTILITY='..tostring(pair and pair.utility)) end
   return
  end
  -- A lone utility uses vanilla slot 3. The dedicated key selects that slot
  -- without forming a pair, attaching again, or consuming the normal binding.
  -- Eligibility is decided from the fresh snapshot on an input edge, so an
  -- ordinary support weapon, empty slot or death transition cannot be drawn.
  if dedicated and not pair and p.support~=0 and is_utility(identity(p.support)) then
   if p.primary==0 or p.secondary==0 or not can_switch(p) then return end
   local selected=p.active==1 and p.primary or p.active==2 and p.secondary or p.active==3 and p.support
   if not selected or held_weapon(p.avatar)~=selected then return end
   if p.active~=3 and native_guard() then
    if not choose(p,3) then paused=true;emit('PAUSED DRAW_SINGLE_UTILITY_SELECT_VERIFY_FAILED')
    elseif DEBUG then emit('DRAW_SINGLE_UTILITY SUPPORT='..p.support) end
   end
   return
  end
  if pair then
   -- Standard weapons disappearing is a death/invalid-inventory signal. Wait
   -- briefly for vanilla to clear all slots before attempting the virtual drop.
   if p.primary==0 or p.secondary==0 then
    if p.primary==0 and p.secondary==0 then
     pair.death_since=pair.death_since or started
     if p.support==0 and p.backpack==0 then death_release(p)
     elseif started-pair.death_since<500 then return
     else if DEBUG then emit('DEATH_DROP_SKIP INVENTORY_DID_NOT_EMPTY') end  end
    else if DEBUG then emit('DEATH_DROP_SKIP PARTIAL_STANDARD_INVENTORY') end  end
    reset(p);if DEBUG then emit('PAIR_CLEARED NO_STANDARD_WEAPONS') end ;return
   end
   pair.death_since=nil
   if p.support==pair.support or p.support==pair.utility then
    pair.real=p.support;pair.utility_drawn=p.support==pair.utility and p.active==3;pair.selected=p.active
   end
   -- A manual drop or consumed item emptied the real support slot. Restore
   -- only the other, still-attached member; never reacquire the dropped one.
   if p.support==0 then
    if p.active~=1 and p.active~=2 then return end
    local dropped=pair.real
    if dropped~=pair.utility and dropped~=pair.support then
     paused=true;emit('PAUSED EMPTY_SLOT_LAST_REAL_UNKNOWN');return
    end
    local target=dropped==pair.utility and pair.support or pair.utility
    local target_id=target==pair.utility and utility_id or pair.support_id
    local remaining=attached_entity(target,target_id)
    if not remaining or remaining.parent~=p.avatar then
     paused=true;emit('PAUSED EMPTY_SLOT_REMAINING_NOT_ATTACHED');return
    end
    local dropped_id=dropped==pair.utility and utility_id or pair.support_id
    if identity(dropped)==dropped_id then
     local released=attached_entity(dropped,dropped_id)
     if released and released.parent==p.avatar then return end
     if not released or released.parent~=0 then
      paused=true;emit('PAUSED EMPTY_SLOT_DROPPED_PARENT_UNKNOWN');return
     end
    end
    if switch_entity(p,target,target_id,false,'RESTORE_REMAINING_AFTER_DROP_OR_USE') then
     queue_lone_publication(p,target,target_id,dropped,dropped_id)
     if DEBUG then emit('PAIR_RELEASED DROPPED_OR_USED='..dropped..' REMAINING='..target) end 
     pair=nil;last_support=nil;pending_utility=nil
     if is_utility(target_id) then
      utility_id=target_id
      pending_utility={entity=target,avatar=p.avatar,owner=p.owner}
     else last_support={entity=target,id=target_id} end
    end
    return
   end
   -- A third support entity entering the slot means pickup/replacement, not
   -- an ordinary swap between the two known members.
   if p.support~=pair.support and p.support~=pair.utility then
    replace_support(p);return
   end
   if not check_pair(p) then
    if p.support==pair.support then pair=nil;if DEBUG then emit('PAIR_CLEARED UTILITY_GONE') end 
    else paused=true;emit('PAUSED STORED_ENTITY_CHANGED') end
    return
   end
   if shortcut.configured() and not hit and p.support==pair.utility
    and (p.active==1 or p.active==2)then
    local actual=held_weapon(p.avatar)
    local chosen=p.active==1 and p.primary or p.secondary
    if actual==chosen and can_switch(p)then
     -- Publish the same membership change that Core makes locally. Wait on
     -- the existing guarded primary boundary and restore the chosen slot;
     -- a formed pair must not make this an initial-formation-only no-op.
     if not queue_formation(p,'UTILITY_EXIT',p.active,nil)then
      switch_entity(p,pair.support,pair.support_id,false,'STORE_UTILITY_ON_WEAPON_CHANGE')
     end
    end
    return
   end
   -- At this point the pair is validated and no recovery/replacement is due.
   -- An accepted support gesture can safely target the opposite member.
   if hit then
    if dedicated then
     local actual=held_weapon(p.avatar)
     local selected=p.active==1 and p.primary or p.active==2 and p.secondary or p.support
     if actual~=selected then return end
    end
    if not can_switch(p) then if DEBUG then emit('SUPPORT_ACTION_BLOCKED INVALID_WEAPON_OR_INPUT_CONTEXT') end ;return end
    -- Dedicated input is a draw command, not a toggle. Repeated presses with
    -- utility already wielded do nothing: no reattachment or notification work.
    if dedicated and p.support==pair.utility then
     if p.active~=3 then
      -- Utility can still be in slot 3 while a primary/secondary transition is
      -- settling. Select it directly rather than attaching the same entity again.
      if native_guard() and not choose(p,3) then
       paused=true;emit('PAUSED DRAW_UTILITY_SELECT_VERIFY_FAILED')
      end
     end
     return
    end
    local target=pair.utility
    if not dedicated and p.support==pair.utility then target=pair.support end
    local id=target==pair.utility and utility_id or pair.support_id
    if not queue_formation(p,'CLIENT_DRAW',3,nil,target,id)then
     switch_entity(p,target,id,true,target==pair.utility and 'DRAW_UTILITY' or 'DRAW_SUPPORT')
    end
   end
   return
  end
  if pending_utility and (p.support==0 or p.primary==0 or p.secondary==0) then
   pending_utility=nil;if DEBUG then emit('UTILITY_FIRST_CANCEL EMPTY_INVENTORY_OR_MANUAL_DROP') end 
  end
  if not is_utility(utility_id) then utility_id=SOLO_SILO_ID end
  if p.support==0 then last_support=nil end
  local id=identity(p.support)
  -- A different utility replaces the pending utility before a pair exists.
  -- Clear the old pending reference so two utilities cannot form a pair.
  if pending_utility and id and is_utility(id) and p.support~=pending_utility.entity then
   if DEBUG then emit('UTILITY_REPLACED OLD='..pending_utility.entity..' NEW='..p.support) end 
   pending_utility=nil;last_support=nil
  end
  -- Utility-first: the new ordinary weapon may have displaced the utility.
  -- Reattach the remembered utility, then restore the new ordinary member.
  if pending_utility and id and not is_utility(id) then
   if pending_utility.avatar~=p.avatar or pending_utility.owner~=p.owner
    or identity(pending_utility.entity)~=utility_id then
    pending_utility=nil;if DEBUG then emit('UTILITY_FIRST_CANCEL ENTITY_OR_CONTEXT_CHANGED') end 
   elseif can_switch(p) then
    local owned_support=attached_entity(p.support,id)
    if not owned_support or owned_support.parent~=p.avatar then pending_utility=nil;last_support=nil;return end
    local utility=pending_utility.entity
    local held=p.active==3
    local selected_before=p.active
    pair={avatar=p.avatar,owner=p.owner,utility=utility,support=p.support,support_id=id,real=p.support}
    if queue_formation(p,'UTILITY_FIRST',selected_before,nil)then return end
    local restore_selected=publication and publication.pending_formation and publication.pending_formation(p)
    if not switch_entity(p,utility,utility_id,false,'CAPTURE_UTILITY_FIRST') then
     paused=true;emit('PAUSED UTILITY_FIRST_CAPTURE_FAILED');return
    end
    local after=sample()
    if not after or not switch_entity(after,pair.support,id,held,'RESTORE_UTILITY_FIRST_SUPPORT') then
     paused=true;emit('PAUSED UTILITY_FIRST_RESTORE_FAILED');return
    end
    local final=sample()
    if restore_selected and (not final or not same_context(p,final) or final.support~=pair.support)then
     paused=true;emit('PAUSED UTILITY_FIRST_FINAL_CONTEXT_CHANGED');return
    end
    if restore_selected and not held and final.active~=selected_before then
     if not choose(final,selected_before)then paused=true;emit('PAUSED UTILITY_FIRST_SELECTION_RESTORE_FAILED');return end
    end
    last_support={entity=pair.support,id=id};pending_utility=nil
    emit('PAIR_READY ORDER=UTILITY_FIRST UTILITY='..pair.utility..' SUPPORT='..pair.support)
    return
   else
    if hit then if DEBUG then emit('UTILITY_FIRST_WAIT INVALID_WEAPON_OR_INPUT_CONTEXT') end  end
    return
   end
  end
  if is_utility(id) then utility_id=id end
  -- Ordinary-first: utility pickup may have displaced the remembered gun.
  -- Capture that gun, then follow the live Switch Weapon On Pickup setting.
  if id==utility_id and last_support and not is_utility(last_support.id) and identity(last_support.entity)==last_support.id then
   local owned_support=attached_entity(p.support,id)
   if not owned_support or owned_support.parent~=p.avatar then last_support=nil;pending_utility=nil;return end
   if not can_switch(p) then return end
   local pickup_setting=utility_pickup_setting()
   if pickup_setting==nil then return end
   pair={avatar=p.avatar,owner=p.owner,utility=p.support,support=last_support.entity,support_id=last_support.id,real=last_support.entity}
   local picked=p.support
   local previous_active=last_support.active or p.active
   if queue_formation(p,'ORDINARY_FIRST',pickup_setting and 3 or previous_active,pickup_setting)then return end
   if switch_entity(p,pair.support,pair.support_id,not pickup_setting and previous_active==3,'STORE_NEW_UTILITY') then
    local ready=sample()
    pair.real=pair.support
    if pickup_setting then
     -- The utility was genuinely picked up. Keep the preserved gun attached,
     -- then restore/draw the utility using the normal verified swap boundary.
     if not ready or not switch_entity(ready,pair.utility,utility_id,true,'DRAW_UTILITY')then
      paused=true;emit('PAUSED PICKUP_UTILITY_DRAW_FAILED');return
     end
     pair.real=pair.utility;pair.selected=3;pair.utility_drawn=true
    else
     if not ready then paused=true;emit('PAUSED PICKUP_SELECTION_UNAVAILABLE');return end
     if ready.active~=previous_active and not choose(ready,previous_active)then
      paused=true;emit('PAUSED PICKUP_SELECTION_RESTORE_FAILED');return
     end
     pair.selected=previous_active;pair.utility_drawn=false
    end
    pending_utility=nil
    emit('PAIR_READY UTILITY='..picked..' SUPPORT='..pair.support..' SWITCH_WEAPON_ON_PICKUP='..tostring(pickup_setting))
   else paused=true;emit('PAUSED CAPTURE_FAILED') end
  elseif id==utility_id then
   if not pending_utility or pending_utility.entity~=p.support then
    local owned_support=attached_entity(p.support,id)
    if not owned_support or owned_support.parent~=p.avatar then return end
    pending_utility={entity=p.support,avatar=p.avatar,owner=p.owner}
    if DEBUG then emit('UTILITY_FIRST_PENDING ENTITY='..p.support) end 
   end
  elseif id and id~=utility_id then last_support={entity=p.support,id=id,active=p.active} end
  if hit and not pair then if DEBUG then emit('SUPPORT_ACTION NO_STORED_UTILITY') end  end
 end)
 if not success then formation_pending=nil;replacement_pending=nil;if publication then publication.end_replacement()end;paused=true;emit('PAUSED LUA_ERROR='..tostring(why)) end
 -- If a native action occurred, the earlier inventory snapshot can be stale.
 -- Pass it to filter_sync only when attachment/drop counters stayed unchanged.
 -- Resupply runs from this hook rather than registering its own update chain.
 if did_poll then
  if not replacement_pending and not formation_pending and _G.UtilitySupportSlotUpdate then
   -- Lazy observation sharing is scoped to this call only. Native boundary
   -- verification still calls sample_inventory and bypasses this cache.
   observation.begin()
   local addon_ok,addon_error=pcall(_G.UtilitySupportSlotUpdate,started)
   observation.finish() -- Clear even after an addon error; never cross updates.
   if not addon_ok then emit('RESUPPLY_UPDATE_ERROR='..tostring(addon_error))end
  end
  local filter_ok,filter_error=true,nil
  if not replacement_pending and not formation_pending then
   filter_ok,filter_error=pcall(filter_sync,calls+death_drop_calls+replacement_drop_calls==poll_calls and poll_sample or nil)
  end
  if not filter_ok then filter_failed=true;filter_armed=false;emit('FILTER_TEST_ERROR='..tostring(filter_error));pcall(filter_stop) end
  if DEBUG then local elapsed=tonumber(kernel.uss_win1_GetTickCount64())-started
  poll_total_ms=poll_total_ms+elapsed
  if elapsed>poll_peak_ms then poll_peak_ms=elapsed end end
 end
 -- Preserve nils and multiple returns from another mod's previous hook.
 -- Returning just result[1] would silently change that hook contract.
 if result then return unpack(result,1,result.n)end
end
-- Restore filters, disconnect shared helpers, close the log, and chain shutdown.
_G.shutdown=function(...)
 _G.UtilitySupportSlotUpdate=nil;_G.UtilitySupportSlotContext=nil
 local clean,why=pcall(filter_stop)
 if not clean then emit('FILTER_SHUTDOWN_ERROR='..tostring(why))end
 emit('SESSION_END DEBUG='..tostring(DEBUG)..' REPLACEMENT_DROP_CALLS='..replacement_drop_calls..' DEATH_DROP_CALLS='..death_drop_calls..' CALLS='..calls..' STORED_UTILITY='..tostring(pair and pair.utility)..' UPDATES='..updates..' POLLS='..polls..' POLL_TOTAL_MS='..poll_total_ms..' POLL_PEAK_MS='..poll_peak_ms..' READ_CALLS='..read_calls)
 if log then log:close();log=nil end
 if shutdown then return shutdown(...) end
end
emit('READY DEBUG='..tostring(DEBUG)..' VERSION=0.7.1 INPUT=SUPPORT_ACTION_24 SCOPE=SOLO_OR_GUARDED_TWO_PLAYERS STARTUP_MODE='..startup_mode..' INIT_CPU_MS='..string.format('%.2f',(os.clock()-init_cpu)*1000))
end)
if not ok then
if not logger and loader and loader.open_log then log=log or loader.open_log('UtilitySupportSlot.log');write_log=write_log or utility_log_writer(log);logger=utility_logging(write_log,'error') end
emit('INIT_ERROR='..tostring(err));if log then log:close();log=nil end
end
end
if loader and loader.capabilities and loader.capabilities.after_startup
 and type(loader.after_startup)=='function'then
 startup_mode='AFTER_STARTUP'
 local accepted=loader.after_startup(initialize_core)
 if accepted~=true then startup_mode='IMMEDIATE_FALLBACK';initialize_core()end
else initialize_core()end
