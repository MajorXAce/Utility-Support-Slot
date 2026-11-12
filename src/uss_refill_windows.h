/* Concrete executable-page reader for fixed-build refill resolution.
 * No page protection changes, hooks, callback registration or native invocation. */
#ifndef USS_REFILL_WINDOWS_H
#define USS_REFILL_WINDOWS_H
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "uss_refill_core.h"
#include "uss_refill_entries.h"
static inline int uss_refill_windows_code(void* user,uintptr_t at,void* out,size_t length)
{
    UssRefillCore* core=(UssRefillCore*)user;MEMORY_BASIC_INFORMATION before={0},after={0};DWORD protection;
    if(!uss_refill_core_epoch(core) || !out || !length || length>32 || at<core->base ||
        at-core->base>core->module_size || length>core->module_size-(at-core->base) ||
        VirtualQuery((const void*)at,&before,sizeof(before))!=sizeof(before))return 0;
    protection=before.Protect;
    if(before.State!=MEM_COMMIT || before.Type!=MEM_IMAGE || (uintptr_t)before.AllocationBase!=core->base ||
        !(protection==PAGE_EXECUTE || protection==PAGE_EXECUTE_READ || protection==PAGE_EXECUTE_READWRITE || protection==PAGE_EXECUTE_WRITECOPY) ||
        at<(uintptr_t)before.BaseAddress || (uintptr_t)before.BaseAddress>UINTPTR_MAX-before.RegionSize ||
        length>(uintptr_t)before.BaseAddress+before.RegionSize-at || !uss_refill_core_memory(core,at,out,length))return 0;
    return VirtualQuery((const void*)at,&after,sizeof(after))==sizeof(after) &&
        !memcmp(&before,&after,sizeof(before)) && uss_refill_core_epoch(core);
}
static inline UssRefillEntryIo uss_refill_windows_entries(UssRefillCore* core)
{
    UssRefillEntryIo io={0};if(core){io.user=core;io.base=core->base;io.size=core->module_size;
        io.context=uss_refill_core_epoch;io.code=uss_refill_windows_code;io.data=uss_refill_core_memory;}return io;
}
#endif
