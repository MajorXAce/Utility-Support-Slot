/* Shared typed invocation for diagnostic and normal replacement services. */
#ifndef USS_REPLACEMENT_INVOKE_H
#define USS_REPLACEMENT_INVOKE_H
#include "replacement_adapter.h"
static inline int replacement_runtime_invoke(void* user,UssReplacementEffect effect,uintptr_t first,uintptr_t second,
    uintptr_t record,uintptr_t inventory,uint32_t avatar,uint32_t value,const UssInventoryMessage* message)
{
    UssNativeCalls calls={0};(void)user;
    if(effect==USS_REPLACEMENT_SELECT){calls.select_state=(UssSelectStateFn)first;calls.select_weapon=(UssSelectWeaponFn)second;
        return uss_select_slot(&calls,(void*)record,(void*)inventory,avatar,value)==USS_CALL_RETURNED;}
    if(effect==USS_REPLACEMENT_ATTACH){calls.attach=(UssAttachFn)first;
        return uss_attach_support(&calls,(void*)inventory,avatar,value)==USS_CALL_RETURNED;}
    if(effect==USS_REPLACEMENT_DROP){calls.drop_slot=(UssDropSlotFn)first;
        return value==3 && uss_drop_support_slot(&calls,(void*)inventory,avatar)==USS_CALL_RETURNED;}
    if(effect==USS_REPLACEMENT_SEND){calls.send_inventory=(UssSendInventoryFn)first;
        return uss_send_inventory(&calls,message)==USS_CALL_RETURNED;}
    return 0;
}
#endif
