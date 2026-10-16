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
