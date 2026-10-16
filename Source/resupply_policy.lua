-- Read-only counterparts of the captured source-property and ammo-setting
-- lookups. They inspect native policy; they do not grant ammo or alter slots.
-- This factory is embedded before the refill addon and shared with its filter.
-- `c` is the mod's shared context, not a game-provided API object.
-- Short names below follow the surrounding native readers: api = checked memory
-- reader, game = module base, u32 = unsigned 32-bit decoder, m = manager pointer,
-- h = copied header, i/index = component-array position. They are not entity IDs.
-- source_scale reads a source multiplier; direct_amount reads a weapon setting.
-- Neither computes the final ammo gain: the game's refill function applies it.
local function resupply_policy(c)
 local api,game,u32=c.memory,c.game_base,c.read_u32
 local function ptr(a)return api.pointer(api.read(a,8))end
 -- Decode IEEE-754 binary32 from copied little-endian bytes. Lua numbers are
 -- doubles here; manually decoding avoids dereferencing native memory with FFI.
 -- Exponent zero includes subnormal values; 255 is NaN/infinity and is refused.
 local function float(bytes)
  local n=bytes and #bytes==4 and u32(bytes,0)
  if not n then return end
  local sign=n>=2147483648 and -1 or 1
  local exponent=math.floor(n/8388608)%256
  local fraction=n%8388608
  if exponent==255 then return end -- Reject NaN/Infinity before a native call.
  if exponent==0 then return sign*fraction*2^-149 end
  return sign*(1+fraction/8388608)*2^(exponent-127)
 end
 -- Captured open-addressed table: header [pointer, capacity, empty key,
 -- multiplier], followed by 8-byte [entity key, component index] buckets.
 -- Our probe bound caps work; exhausting it means UNKNOWN, not absent.
 -- Only encountering the table's empty sentinel proves an absent component.
 local function checked_hash(at,key)
  local h=api.read(at,20)
  if not h or #h~=20 then return nil,'HASH_READ' end
  local cap,empty,mult=u32(h,8),u32(h,12),u32(h,16)
  if not cap or cap<1 or cap>1048576 then return nil,'HASH_CAPACITY' end
  local power=cap
  while power>1 and power%2==0 do power=power/2 end
  if power~=1 then return nil,'HASH_CAPACITY' end
  local rows=api.pointer(h:sub(1,8))
  if not rows then return nil,'HASH_ARRAY' end
  -- Reduce factors before multiplying: all arithmetic is exact in Lua's
  -- double, and matches the native low-bit mask for a power-of-two capacity.
  local first=((key%cap)*(mult%cap))%cap
  for p=0,math.min(cap,128)-1 do
   local atrow=rows+((first+p)%cap)*8
   local row=api.read(atrow,8)
   if not row or #row~=8 then return nil,'HASH_ROW' end
   local found=u32(row,0)
   if found==key or found==empty then
    if api.read(at,20)~=h or api.read(atrow,8)~=row then return nil,'HASH_CHANGED' end
    if found~=key or u32(row,4)==0xffffffff then return nil,'ABSENT' end
    return u32(row,4)
   end
  end
  return nil,'HASH_PROBE_BOUND'
 end
 -- Property 9 is an observed numeric ID used by the native supply caller.
 -- We name it a source scale because of that use; this is not an official
 -- documented property name. The override belongs to the pickup entity.
 -- Header pointer fields use Lua string positions 49..56 / 65..72, which
 -- correspond to zero-based byte offsets +0x30 / +0x40 in native memory.
 local function source_scale(entity)
  local m=ptr(game+0x3326AC0)
  local h=m and api.read(m,80)
  if not h or #h~=80 then return nil,'PROPERTY_MANAGER' end
  local index,why=checked_hash(m+0x18,entity)
  if not index then
   -- A proved absent component takes the same default as native A079B0.
   -- Unreadable, changed or exhausted hashes must never become a default.
   if why~='ABSENT' then return nil,why end
   if ptr(game+0x3326AC0)~=m or api.read(m,80)~=h then return nil,'PROPERTY_CHANGED' end
   return float(api.read(game+0x21417B4,4)),'PROPERTY_9_DEFAULT'
  end
  if index>=16384 then return nil,'PROPERTY_INDEX' end
  local units=api.pointer(h:sub(49,56));local rows=api.pointer(h:sub(65,72))
  local unit=units and ptr(units+index*8)
  local descriptor=unit and api.read(unit,24)
  local address=rows and rows+index*0x38
  local row=address and api.read(address,0x38)
  if not row or #row~=0x38 or not descriptor or #descriptor~=24
   or u32(descriptor,8)~=entity then return nil,'PROPERTY_OWNER' end
  local n=u32(row,0)
  if not n or n>6 then return nil,'PROPERTY_COUNT' end
  local scale,origin=float(api.read(game+0x21417B4,4)),'PROPERTY_9_DEFAULT'
  -- Native A07A62 copies the value-array header (+1C) to stack+20;
  -- A07AC1 reads values at stack+24, so row values begin at +20, NOT +1C.
  -- +1C is the array count: interpreting integer 1 as float yielded a
  -- subnormal multiplier and incorrectly forced the native minimum-one gain.
  -- IDs begin at +4. A zero property ID terminates the native search.
  for j=0,n-1 do
   local id=u32(row,4+j*4)
   if id==0 then break end
   if id==9 then
    local values=u32(row,0x1C)
    if not values or values>6 or j>=values then return nil,'PROPERTY_VALUE_COUNT' end
    scale=float(row:sub(0x20+j*4+1,0x24+j*4));origin='PROPERTY_9_OVERRIDE';break
   end
  end
  if ptr(game+0x3326AC0)~=m or api.read(m,80)~=h
   or ptr(units+index*8)~=unit or api.read(unit,24)~=descriptor
   or api.read(address,0x38)~=row or checked_hash(m+0x18,entity)~=index then return nil,'PROPERTY_CHANGED' end
  return scale,origin
 end
 local function ground(kind,entity)
  if kind==4 then return 0.5,'NATIVE_HALF_SHARE' end
  if kind==7 or kind==8 or kind==9 then
   local scale,why=source_scale(entity)
   -- Verified supply boxes can use 9999: let native refill clamp to capacity.
   -- Do not impose a percentage-like ceiling on this native multiplier.
   if not scale or scale<2^-126 then return nil,'INVALID_SCALE_'..tostring(why) end
   return scale,why
  end
  return nil,'NON_AMMO_PICKUP'
 end
 -- A resource identity is eight raw bytes identifying a weapon definition.
 -- Resolve its base ammo settings when no live per-entity override exists.
 -- cap/bias/stride describe two captured table layouts, not balancing values.
 -- Folding bytes modulo cap avoids converting a 64-bit ID to an inexact double.
 local function resource_settings(id,kind)
  local owner=ptr(game+0x346BF98)
  local offset,cap,bias,stride=0xF124A0,0x21C,0x36,0xA0
  if kind=='ROUNDS' then offset,cap,bias,stride=0xF12820,50,0x320/0x88,0x88 end
  local tableptr=owner and ptr(owner+offset)
  if not tableptr or not id or #id~=8 then return nil,'AMMO_RESOURCE_TABLE' end
  local first=0
  for j=8,1,-1 do first=(first*256+id:byte(j))%cap end
  for p=0,math.min(cap,128)-1 do
   local at=tableptr+((first+p)%cap)*16
   local row=api.read(at,16)
   if not row or #row~=16 then return nil,'AMMO_RESOURCE_BUCKET' end
   if row:sub(1,8)==id then
    local i=u32(row,8)
    if not i or i>=1048576 then return nil,'AMMO_RESOURCE_INDEX' end
    local address=kind=='ROUNDS' and tableptr+0x320+i*stride or tableptr+(i+bias)*stride
    if ptr(game+0x346BF98)~=owner or ptr(owner+offset)~=tableptr
     or api.read(at,16)~=row then return nil,'AMMO_RESOURCE_CHANGED' end
    return address
   end
   if row:sub(1,8)==string.rep('\0',8)then return nil,'AMMO_RESOURCE_ABSENT' end
  end
  return nil,'AMMO_RESOURCE_BOUND'
 end
 -- p is a validated stored-weapon snapshot. MAG and ROUNDS select distinct
 -- native component layouts; the names describe storage, not weapon categories.
 -- A zero direct supply amount is meaningful: backpack-fed weapons must use
 -- their backpack/reload path. It is not permission to invent a minimum grant.
 local function direct_amount(p)
  local rounds=p.kind=='ROUNDS'
  local unit_offset,hash_offset,rows_offset,stride,amount_offset=0x38,0x60,0xA0,0xA0,0x90
  local global=0x3326648
  if rounds then unit_offset,hash_offset,rows_offset,stride,amount_offset,global=0x40,0x68,0xA8,0x88,0x54,0x3326CF0 end
  local units=ptr(p.manager+unit_offset)
  local unit=units and ptr(units+p.index*8)
  local descriptor=unit and api.read(unit,24)
  if not descriptor or #descriptor~=24 or u32(descriptor,8)~=p.target
   or descriptor:sub(1,8)~=p.id then return nil,'AMMO_SETTING_OWNER' end
  -- Live per-entity settings override the base resource, exactly as in the
  -- game's 4F3740/4FDD20 getters. Missing override is a valid resource path.
  local i,why=checked_hash(p.manager+hash_offset,p.target)
  local hash_why=why
  local address,settings_rows
  if i then
   if i>=16384 then return nil,'AMMO_SETTING_INDEX' end
   local rows=ptr(p.manager+rows_offset)
   settings_rows=rows;address=rows and rows+i*stride
  elseif why=='ABSENT' then address,why=resource_settings(p.id,p.kind)
  else return nil,why end
  local bytes=address and api.read(address+amount_offset,4)
  local amount=bytes and #bytes==4 and u32(bytes,0)
  if not amount or amount>1000 then return nil,why or 'AMMO_SETTING_AMOUNT' end
  if ptr(game+global)~=p.manager or c.lookup(p.manager+(rounds and 0x28 or 0x20),p.target)~=p.index
   or ptr(p.manager+unit_offset)~=units or ptr(units+p.index*8)~=unit
   or api.read(unit,24)~=descriptor or api.read(address+amount_offset,4)~=bytes then return nil,'AMMO_SETTING_CHANGED' end
  local again,again_why=checked_hash(p.manager+hash_offset,p.target)
  if again~=i or again_why~=hash_why then return nil,'AMMO_SETTING_HASH_CHANGED' end
  if i then
   if ptr(p.manager+rows_offset)~=settings_rows then return nil,'AMMO_SETTING_ARRAY_CHANGED' end
  else
   local current=resource_settings(p.id,p.kind)
   if current~=address then return nil,'AMMO_RESOURCE_CHANGED' end
  end
  return amount
 end
 return {ground=ground,direct_amount=direct_amount}
end
