/* Concrete automatic origin/adapter binding; no diagnostic ticket or command.
 * Effect invoker remains explicit, validated by existing replacement services. */
#ifndef USS_AUTOMATIC_NATIVE_H
#define USS_AUTOMATIC_NATIVE_H
#include "uss_automatic_controller.h"
#include "formation_native.h"
#include "replacement_adapter.h"
#include "local_replacement_adapter.h"
typedef struct UssAutomaticNative {
    UssAutomaticController* controller;UssFormationNative* formation;
    UssReplacementOriginEvidence origin;UssReplacementAdapter adapter;
    UssLocalReplacementAdapter local;
    void* policy_user;int (*pickup_policy)(void*,UssPickupPolicy*);
    void* invoke_user;
    int (*invoke)(void*,UssReplacementEffect,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uint32_t,uint32_t,const UssInventoryMessage*);
} UssAutomaticNative;
static inline UssReplacementProducer automatic_native_producer(UssAutomaticNative* native)
{
    UssReplacementProducer p={0};UssFormationRuntime* runtime=native->formation->runtime;
    p.api=native->formation->api;p.mod=native->formation->mod;p.platform=native->formation->platform;
    p.life=&runtime->life;p.history=&runtime->history;p.flags=&runtime->policy;p.work=&native->controller->replacement;
    p.origin=&native->origin;p.clock_ms=runtime->io.clock_ms;p.user=runtime->io.user;return p;
}
static inline int automatic_native_policy(void* user,UssPickupPolicy* out)
{
    UssAutomaticNative* native=(UssAutomaticNative*)user;
    return native->pickup_policy && native->pickup_policy(native->policy_user,out);
}
static inline int automatic_native_origin(void* user,UssReplacementOrigin* out)
{
    UssAutomaticNative* native=(UssAutomaticNative*)user;UssReplacementProducer producer=automatic_native_producer(native);
    memset(&native->origin,0,sizeof(native->origin));
    if(native->formation->runtime->life.owner.context.local_only){
        UssFormationRuntime* r=native->formation->runtime;UssPairOwner* owner=&r->life.owner;UssReplacementOrigin result={0};
        if(!formation_refresh(r) || !lifecycle_current(&r->life,r->thread,&r->latest) ||
            r->life.phase!=USS_LIFE_FORMED || !uss_local_pair_owned(&r->local,owner))return 0;
        result.known=1;result.real=r->latest.inventory.slots[2];result.selected=r->latest.inventory.active_selector;
        result.utility_drawn=result.selected==3 && result.real==owner->utility.entity;
        result.epoch=owner->epoch;result.context_generation=owner->context_generation;
        result.pair_generation=owner->pair_generation;result.guard_generation=owner->guard_generation;*out=result;return 1;
    }
    if(!uss_replacement_read_origin(&producer,&native->origin))return 0;
    *out=native->origin.origin;return 1;
}
static inline int automatic_native_prepare(void* user,UssReplacementWork* work,
    const UssReplacementOrigin* origin,UssReplacementServices* out)
{
    UssAutomaticNative* native=(UssAutomaticNative*)user;UssReplacementProducer producer=automatic_native_producer(native);
    if(work!=&native->controller->replacement)return 0;
    if(native->formation->runtime->life.owner.context.local_only){
        if(!uss_local_replacement_init(&native->local,native->formation,work,origin,native->invoke,native->invoke_user))return 0;
        *out=uss_local_replacement_services(&native->local);return 1;
    }
    if(!native->origin.valid ||
        memcmp(origin,&native->origin.origin,sizeof(*origin)) ||
        !uss_replacement_adapter_init(&native->adapter,&producer,&native->formation->runtime->life,
            &native->formation->runtime->publication,native->invoke,native->invoke_user))return 0;
    *out=uss_replacement_adapter_services(&native->adapter);return 1;
}
static inline int uss_automatic_native_init(UssAutomaticNative* native,UssAutomaticController* controller,
    UssFormationNative* formation,int (*policy)(void*,UssPickupPolicy*),void* policy_user,
    int (*invoke)(void*,UssReplacementEffect,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uint32_t,uint32_t,const UssInventoryMessage*),void* invoke_user)
{
    UssAutomaticIo io={0};UssCorePlatform platform={0};
    if(!native || !controller || !formation || !formation->runtime || !policy || !invoke)return 0;
    memset(native,0,sizeof(*native));native->controller=controller;native->formation=formation;native->pickup_policy=policy;
    native->policy_user=policy_user;native->invoke=invoke;native->invoke_user=invoke_user;
    io.user=native;io.pickup_policy=automatic_native_policy;io.origin=automatic_native_origin;io.prepare_replace=automatic_native_prepare;
    platform.user=formation->platform.user;platform.thread_id=formation->platform.thread_id;
    return uss_automatic_init(controller,formation->runtime,formation->api,formation->mod,&platform,&io);
}
#endif
