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
