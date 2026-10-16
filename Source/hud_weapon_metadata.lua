-- Read-only counterparts of the HUD's weapon resource/icon lookups. No native
-- UI functions are called. This factory is embedded in the HUD addon below.
-- Results are scoped to this HUD root and at most the two current pair IDs.
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
