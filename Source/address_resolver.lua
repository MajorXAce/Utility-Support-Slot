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
