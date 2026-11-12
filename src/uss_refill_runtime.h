/* Production service wiring. Instantiated by the eventual normal mod callback;
 * writing this adapter does not install/register/execute it. */
#ifndef USS_REFILL_RUNTIME_H
#define USS_REFILL_RUNTIME_H
#include "uss_refill_windows.h"
#include "uss_refill_driver.h"
typedef struct UssRefillRuntime {
    UssRefillCore* core;UssRefillEntries entries;uint64_t now;
} UssRefillRuntime;
static inline UssRefillTarget uss_refill_runtime_target(void* user)
{
    UssRefillRuntime* runtime=(UssRefillRuntime*)user;UssRefillTarget result={0};
    UssRefillMemoryIo memory;UssRefillTargetIo io={0};
    if(!runtime || !runtime->core)return result;
    memory=uss_refill_core_io(runtime->core);io.user=runtime->core;io.observe=uss_refill_core_pair;io.memory=&memory;
    if(!uss_refill_target_read(&io,&result))memset(&result,0,sizeof(result));return result;
}
static inline int uss_refill_runtime_verify(void* user,const UssRefillEvidence* event,const UssRefillTarget* target)
{
    UssRefillRuntime* runtime=(UssRefillRuntime*)user;UssRefillEntryIo reader;
    if(!runtime || !runtime->core || !event || !uss_refill_target_same(&event->target,target,1))return 0;
    reader=uss_refill_windows_entries(runtime->core);
    if(!uss_refill_entries_read(&reader,&runtime->entries) || runtime->entries.epoch!=target->epoch)return 0;
    if(event->route==USS_REFILL_OWN_PACK)return runtime->entries.pack_valid && uss_refill_core_verify_pack(runtime->core,event,runtime->now);
    /* Ground source may have been consumed/destroyed; the one-shot completion
     * producer owns its matched fresh snapshot, like Lua's source cache. */
    return event->route==USS_REFILL_GROUND;
}
static inline unsigned uss_refill_runtime_call(void* user,const UssRefillTarget* target,float scale)
{
    UssRefillRuntime* runtime=(UssRefillRuntime*)user;UssRefillTarget fresh;
    if(!runtime || !runtime->core || runtime->entries.epoch!=target->epoch || runtime->entries.base!=runtime->core->base)
        return USS_REFILL_CALL_NOT_ENTERED;
    fresh=uss_refill_runtime_target(user);
    if(!uss_refill_target_same(target,&fresh,1))return USS_REFILL_CALL_NOT_ENTERED;
    return uss_refill_native_call(&runtime->entries.calls,&fresh,scale);
}
static inline UssRefillServices uss_refill_runtime_services(UssRefillRuntime* runtime)
{
    UssRefillServices services={runtime,uss_refill_runtime_target,uss_refill_runtime_verify,uss_refill_runtime_call};return services;
}
#endif
