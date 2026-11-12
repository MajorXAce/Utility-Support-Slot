/* Actual Core/lifecycle -> refill producer. Read-only, serialized callback use.
 * Admission comes from existing owned lifecycle and concrete member reads. */
#ifndef USS_REFILL_CORE_H
#define USS_REFILL_CORE_H
#include "core_lifecycle.h"
#include "uss_refill_target.h"
#include "uss_supply_reader.h"
typedef struct UssRefillCore {
    const HfApi* api;HfMod* mod;const UssCoreLifecycle* life;
    const UssCoreFlags* policy;UssCorePlatform platform;
    uintptr_t base;size_t module_size;
    UssCoreObservation last;const char* reason;
} UssRefillCore;
static inline uint64_t uss_refill_core_epoch(void* user)
{
    UssRefillCore* c=(UssRefillCore*)user;size_t size=0;const char* build;unsigned thread;
    if(!c || !c->life || !c->policy || !c->platform.thread_id || !c->api || !c->mod ||
        !uss_sdk_compatible(c->api) || !c->api->read || !c->api->module_base || !c->api->build_id ||
        !c->policy->known || !c->policy->eligible || c->policy->disabled || c->policy->suspended ||
        c->policy->paused || c->policy->down || c->policy->input || c->life->phase!=USS_LIFE_FORMED ||
        c->life->generation!=c->life->owner.pair_generation)return 0;
    thread=c->platform.thread_id(c->platform.user);
    if(!pair_owner_ready(&c->life->owner,thread) || c->base<0x10000 ||
        c->module_size<0x3484570 || c->module_size>0x20000000 || c->base>UINTPTR_MAX-c->module_size)return 0;
    build=c->api->build_id();
    if(!build || strcmp(build,"25480438") || c->api->module_base("game.dll",&size)!=c->base || size!=c->module_size)return 0;
    return c->life->owner.epoch;
}
static inline int uss_refill_core_memory(void* user,uintptr_t at,void* out,size_t length)
{
    UssRefillCore* c=(UssRefillCore*)user;
    return uss_refill_core_epoch(c) && out && length && length<=512 && at>=0x10000 &&
        at<UINT64_C(0x800000000000) && length<=UINT64_C(0x800000000000)-at && c->api->read(at,out,length)==0;
}
static inline int uss_refill_core_view(UssRefillCore* c,const UssCoreObservation* snapshot,UssRefillPairView* output)
{
    UssCoreFlags flags;UssCoreObservation current;
    UssRefillPairView view={0};const UssPairOwner* owner;UssPairMember utility,ordinary;
    if(!c)return 0;
    c->reason="REFILL_CORE_SCOPE";
    if(!output || !snapshot || !uss_refill_core_epoch(c))return 0;
    owner=&c->life->owner;flags=uss_lifecycle_flags(c->life,c->policy);
    current=*snapshot;
    c->reason="REFILL_FLAGS_CHANGED";
    if(memcmp(&current.flags,&flags,sizeof(flags)))return 0;
    c->reason="REFILL_PAIR_CONTEXT";
    if(!lifecycle_current(c->life,owner->thread,&current) || !current.pair_matched || !current.flags.formed ||
        current.flags.formation_pending || !lifecycle_member(&current.utility,USS_UTILITY_SUPPORT,&utility) ||
        !lifecycle_member(&current.ordinary,USS_ORDINARY_SUPPORT,&ordinary) ||
        !pair_member_equal(&utility,&owner->utility) || !pair_member_equal(&ordinary,&owner->ordinary))return 0;
    view.epoch=owner->epoch;view.context_generation=owner->context_generation;
    view.pair_generation=owner->pair_generation;view.guard_generation=owner->guard_generation;
    view.owner=current.context.local.owner;view.inventory=current.context.local.inventory;
    view.avatar=current.inventory.avatar;view.primary=current.inventory.slots[0];view.secondary=current.inventory.slots[1];
    view.backpack=current.inventory.slots[3];view.support=current.inventory.slots[2];
    view.utility=utility.entity;view.ordinary=ordinary.entity;
    view.utility_parent=current.utility.parent;view.ordinary_parent=current.ordinary.parent;
    memcpy(view.utility_identity,utility.identity,8);memcpy(view.ordinary_identity,ordinary.identity,8);
    view.valid=current.valid;view.mission=current.mission;view.ready=owner->ready;
    view.formed=current.flags.formed;view.busy=current.flags.formation_pending;view.down=current.flags.down;
    c->reason="REFILL_PAIR_NOT_STORED";
    if(!uss_refill_pair_eligible(&view) || uss_refill_core_epoch(c)!=view.epoch)return 0;
    c->last=current;*output=view;return 1;
}
static inline int uss_refill_core_pair(void* user,UssRefillPairView* output)
{
    UssRefillCore* c=(UssRefillCore*)user;UssCoreFlags flags;UssCoreObservation current;
    if(!output || !uss_refill_core_epoch(c))return 0;
    flags=uss_lifecycle_flags(c->life,c->policy);
    current=uss_core_observe(c->api,c->mod,&c->life->owner,&flags,&c->platform,NULL);
    return uss_refill_core_view(c,&current,output);
}
static inline UssRefillMemoryIo uss_refill_core_io(UssRefillCore* core)
{
    UssRefillMemoryIo io={0};if(core){io.user=core;io.base=core->base;io.read=uss_refill_core_memory;io.context=uss_refill_core_epoch;}return io;
}
static inline int uss_refill_core_supply(UssRefillCore* core,UssSupplySnapshot* output)
{
    UssRefillMemoryIo memory=uss_refill_core_io(core);UssRefillTargetIo io={0};
    UssRefillTarget target;UssSupplySnapshot supply;UssRefillPairView after;Inventory inventory;
    unsigned char identities[3][8];unsigned i;
    if(!output)return 0;
    io.user=core;io.observe=uss_refill_core_pair;io.memory=&memory;
    if(!uss_refill_target_read(&io,&target))return 0;
    inventory=core->last.inventory;
    for(i=0;i<3;++i)memcpy(identities[i],inventory.identities[i],8);
    if(!uss_supply_read(&memory,&target,inventory.row_address,identities,&supply) ||
        !uss_refill_core_pair(core,&after) || after.epoch!=target.epoch ||
        after.context_generation!=target.context_generation || after.pair_generation!=target.pair_generation ||
        after.guard_generation!=target.guard_generation || after.owner!=target.owner || after.inventory!=target.inventory ||
        after.avatar!=target.avatar || after.ordinary!=target.target || after.support!=target.real_support ||
        after.primary!=target.primary || after.secondary!=target.secondary || after.backpack!=target.backpack ||
        memcmp(after.ordinary_identity,target.identity,8))return 0;
    *output=supply;return 1;
}
static inline int uss_refill_core_verify_pack(UssRefillCore* core,const UssRefillEvidence* event,uint64_t now)
{
    UssSupplySnapshot fresh;float scale;
    return event && event->route==USS_REFILL_OWN_PACK && uss_refill_policy(event,now,&scale) &&
        uss_refill_core_supply(core,&fresh) && uss_refill_target_same(&fresh.target,&event->target,1) &&
        fresh.pack.manager==event->pack_manager && fresh.pack.unit==event->pack_unit &&
        fresh.pack.entity==event->pack_entity && fresh.pack.reserve==event->charges_after;
}
/* Borrow only a fresh snapshot from this synchronous gameplay update. No cache
 * survives this call; the native services still use full fresh boundary readers. */
static inline int uss_refill_core_sample(UssRefillCore* core,const UssCoreObservation* current,
    UssRefillTarget* target,UssSupplySnapshot* supply,unsigned* have_supply)
{
    UssRefillPairView view,after;UssRefillMemoryIo memory=uss_refill_core_io(core);
    UssRefillTarget result;UssSupplySnapshot bag={0};unsigned char identities[3][8];unsigned i,have=0;
    if(!core || !target || !have_supply)return 0;
    if(!uss_refill_core_view(core,current,&view) ||
        !uss_refill_target_from_pair_diagnose(&memory,&view,&result,&core->reason))return 0;
    if(supply && result.backpack && result.backpack!=UINT32_MAX){
        for(i=0;i<3;++i)memcpy(identities[i],current->inventory.identities[i],8);
        have=(unsigned)uss_supply_read(&memory,&result,current->inventory.row_address,identities,&bag);
    }
    if(!uss_refill_core_view(core,current,&after) || !uss_refill_pair_same(&view,&after))return 0;
    core->reason="REFILL_SAMPLE_READY";
    *target=result;*have_supply=have;if(have)*supply=bag;return 1;
}
#endif
