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
