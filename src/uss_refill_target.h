/* Connect validated Core/member observations to concrete ammo/settings readers.
 * observe() must supply current owned context and freshly verified attachments;
 * this adapter never turns missing Core evidence into valid flags. */
#ifndef USS_REFILL_TARGET_H
#define USS_REFILL_TARGET_H
#include "uss_refill_amount.h"
#include "weapon_policy.h"
typedef struct UssRefillPairView {
    uint64_t epoch,context_generation,pair_generation,guard_generation;
    uintptr_t owner,inventory;
    uint32_t avatar,primary,secondary,backpack,support,utility,ordinary,utility_parent,ordinary_parent;
    unsigned char utility_identity[8],ordinary_identity[8];
    unsigned valid,mission,ready,formed,busy,down;
} UssRefillPairView;
typedef struct UssRefillTargetIo {
    void* user;int (*observe)(void*,UssRefillPairView*);
    const UssRefillMemoryIo* memory;
} UssRefillTargetIo;
static inline int uss_refill_pair_eligible(const UssRefillPairView* p)
{
    static const unsigned char zero[8]={0};
    if(!p || !p->valid || !p->mission || !p->ready || !p->formed || p->busy || p->down ||
        !p->epoch || !p->context_generation || !p->pair_generation || !p->guard_generation ||
        !p->owner || !p->inventory || !p->avatar || p->avatar==UINT32_MAX ||
        !p->primary || p->primary==UINT32_MAX || !p->secondary || p->secondary==UINT32_MAX ||
        !p->utility || p->utility==UINT32_MAX || !p->ordinary || p->ordinary==UINT32_MAX ||
        p->utility==p->ordinary || p->utility==p->avatar || p->ordinary==p->avatar ||
        p->support!=p->utility || p->utility_parent!=p->avatar || p->ordinary_parent!=p->avatar ||
        !memcmp(p->ordinary_identity,zero,8) ||
        uss_classify_support(p->utility_identity,2)!=USS_UTILITY_SUPPORT ||
        uss_classify_support(p->ordinary_identity,2)!=USS_ORDINARY_SUPPORT)return 0;
    /* Single-use weapons are never refilled: built-in ids plus run-time clones of them (weapon_policy.h). */
    if(uss_is_expendable_support(p->ordinary_identity))return 0;
    return 1;
}
static inline int uss_refill_pair_same(const UssRefillPairView* a,const UssRefillPairView* b)
{
    return uss_refill_pair_eligible(a) && uss_refill_pair_eligible(b) &&
        a->epoch==b->epoch && a->context_generation==b->context_generation &&
        a->pair_generation==b->pair_generation && a->guard_generation==b->guard_generation &&
        a->owner==b->owner && a->inventory==b->inventory && a->avatar==b->avatar &&
        a->primary==b->primary && a->secondary==b->secondary && a->backpack==b->backpack &&
        a->support==b->support && a->utility==b->utility && a->ordinary==b->ordinary &&
        !memcmp(a->utility_identity,b->utility_identity,8) && !memcmp(a->ordinary_identity,b->ordinary_identity,8);
}
/* Observation-only construction from a Core view owned by this update.
 * Mutation services use target_read below, which obtains fresh pair evidence. */
static inline int uss_refill_target_from_pair_diagnose(const UssRefillMemoryIo* memory,
    const UssRefillPairView* before,UssRefillTarget* output,const char** reason)
{
    UssRefillComponent ammo;UssRefillTarget target={0};uint32_t amount;
    int component;
    if(reason)*reason="REFILL_PAIR_INELIGIBLE";
    if(!output || !uss_refill_pair_eligible(before))return 0;
    component=uss_refill_ammo_read(memory,before->ordinary,before->ordinary_identity,&ammo);
    if(reason)*reason=component==0?"REFILL_AMMO_ABSENT":"REFILL_AMMO_UNKNOWN";
    if(component!=1)return 0;
    if(reason)*reason="REFILL_AMMO_EPOCH";
    if(ammo.epoch!=before->epoch)return 0;
    if(reason)*reason="REFILL_AMOUNT_UNKNOWN";
    if(!uss_refill_direct_amount(memory,&ammo,&amount))return 0;
    target.epoch=before->epoch;target.owner=before->owner;target.inventory=before->inventory;
    target.context_generation=before->context_generation;target.pair_generation=before->pair_generation;
    target.guard_generation=before->guard_generation;target.primary=before->primary;target.secondary=before->secondary;target.backpack=before->backpack;
    target.ammo_manager=ammo.manager;target.ammo_unit=ammo.unit;target.avatar=before->avatar;
    target.target=before->ordinary;target.real_support=before->support;target.kind=ammo.kind;target.index=ammo.index;
    target.reserve=ammo.reserve;target.direct_amount=amount;memcpy(target.identity,ammo.identity,8);
    target.valid=target.eligible=target.stored=target.non_expendable=target.ordinary=1;
    if(reason)*reason="REFILL_TARGET_INVALID";
    if(!uss_refill_target_valid(&target))return 0;
    if(reason)*reason="REFILL_TARGET_READY";
    *output=target;return 1;
}
static inline int uss_refill_target_from_pair(const UssRefillMemoryIo* memory,
    const UssRefillPairView* before,UssRefillTarget* output)
{return uss_refill_target_from_pair_diagnose(memory,before,output,NULL);}
static inline int uss_refill_target_read(const UssRefillTargetIo* io,UssRefillTarget* output)
{
    UssRefillPairView before={0},after={0};UssRefillTarget target;
    if(!io || !io->observe || !io->memory || !output || !io->observe(io->user,&before) ||
        !uss_refill_target_from_pair(io->memory,&before,&target) ||
        !io->observe(io->user,&after) || !uss_refill_pair_same(&before,&after))return 0;
    *output=target;return 1;
}
#endif
