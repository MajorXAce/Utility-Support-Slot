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
