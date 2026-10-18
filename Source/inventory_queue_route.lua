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
    local roster=inventory_session_peers(field,u32,session)
    must(roster.count>1,'QUEUE_MULTIPLAYER_REQUIRED')
    local required,found={},{}
    for _,id in ipairs(roster.remotes)do required[id]=true end
    local count=u32(field(provider+0xA0,4),0)
    must(count>=1 and count<=32,'QUEUE_PEER_LIMIT')
    local array=pointer(provider+0xA8)
    for i=0,count-1 do
        local entry=pointer(array+i*8)
        local id=field(entry+0x10,8)
        if required[id]then
            must(not found[id],'QUEUE_DUPLICATE_PEER')
            must(field(entry+0x1878C,1):byte()==0,'QUEUE_EXISTING_OPEN_PEER_REQUIRED')
            found[id]=true
        end
    end
    for _,id in ipairs(roster.remotes)do
        must(found[id],'QUEUE_EXISTING_OPEN_PEER_REQUIRED')
    end
    -- Native append retains its own capacity/failure policy. No queue cursor,
    -- counter, allocation or lock is changed or interpreted as an acknowledgment.
    return true
end

end
