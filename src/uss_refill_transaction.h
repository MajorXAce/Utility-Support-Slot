/* Stored ordinary-weapon refill policy/transaction, translated from the Lua
 * resupply addon and source-completion filter. No observer grants evidence merely
 * because ammo changed. Production services must produce fresh verified snapshots. */
#ifndef USS_REFILL_TRANSACTION_H
#define USS_REFILL_TRANSACTION_H
#include <stdint.h>
#include <string.h>
#include <float.h>

enum {USS_REFILL_MAG=1,USS_REFILL_ROUNDS=2};
enum {USS_REFILL_GROUND=1,USS_REFILL_OWN_PACK=2};
enum {USS_REFILL_SKIPPED=0,USS_REFILL_CONFIRMED,USS_REFILL_FULL,USS_REFILL_STOPPED};
enum {USS_REFILL_CALL_NOT_ENTERED=0,USS_REFILL_CALL_RETURNED,USS_REFILL_CALL_UNCERTAIN};
typedef struct UssRefillTarget {
    uint64_t epoch,owner,inventory,ammo_manager,ammo_unit;
    uint64_t context_generation,pair_generation,guard_generation;
    uint32_t primary,secondary,backpack;
    uint32_t avatar,target,real_support,kind,index,reserve,direct_amount;
    unsigned char identity[8];
    unsigned valid,eligible,stored,non_expendable,ordinary;
} UssRefillTarget;
typedef struct UssRefillEvidence {
    UssRefillTarget target;
    uint64_t serial,time_ms,policy_time_ms;
    uint32_t route,source,zone,source_kind;
    unsigned char source_identity[8];
    float source_scale;
    unsigned completed,source_verified,policy_verified;
    /* Own Supply Pack needs SAME validated bag owner/unit, charge spend AND
     * local resource gain; overlapping ground activity disqualifies inference. */
    uint64_t pack_manager,pack_unit,pack_spent_ms;
    uint32_t pack_entity,charges_before,charges_after;
    unsigned resource_gain,ground_activity_near;
} UssRefillEvidence;
/* Optional UI observer payload. Created only after a confirmed positive gain. */
typedef struct UssRefillNotice {
    uint64_t serial;uint32_t route;UssRefillTarget before,after;
} UssRefillNotice;
typedef void (*UssRefillObserver)(void*,const UssRefillNotice*);
typedef struct UssRefillState {
    uint64_t epoch,last_serial,calls;
    unsigned busy,disabled;UssRefillNotice notice;
    const char* reason;
} UssRefillState;
typedef struct UssRefillServices {
    void* user;
    UssRefillTarget (*observe)(void*);
    int (*verify_evidence)(void*,const UssRefillEvidence*,const UssRefillTarget*);
    unsigned (*refill)(void*,const UssRefillTarget*,float);
} UssRefillServices;
static inline int uss_refill_target_valid(const UssRefillTarget* t)
{
    static const unsigned char zero[8]={0};
    return t && t->valid && t->eligible && t->stored && t->non_expendable && t->ordinary &&
        t->epoch && t->owner && t->inventory && t->ammo_manager && t->ammo_unit && t->avatar &&
        t->target && t->target!=UINT32_MAX && t->target!=t->real_support && t->real_support && t->real_support!=UINT32_MAX &&
        (t->kind==USS_REFILL_MAG || t->kind==USS_REFILL_ROUNDS) && t->index<1048576 && t->reserve<=1000 &&
        t->direct_amount<=1000 && memcmp(t->identity,zero,8);
}
static inline int uss_refill_target_same(const UssRefillTarget* a,const UssRefillTarget* b,unsigned reserve)
{
    return uss_refill_target_valid(a) && uss_refill_target_valid(b) && a->epoch==b->epoch && a->owner==b->owner &&
        a->context_generation==b->context_generation && a->pair_generation==b->pair_generation &&
        a->guard_generation==b->guard_generation && a->primary==b->primary && a->secondary==b->secondary &&
        a->backpack==b->backpack && a->inventory==b->inventory && a->ammo_manager==b->ammo_manager && a->ammo_unit==b->ammo_unit &&
        a->avatar==b->avatar && a->target==b->target && a->real_support==b->real_support && a->kind==b->kind &&
        a->index==b->index && a->direct_amount==b->direct_amount && !memcmp(a->identity,b->identity,8) &&
        (!reserve || a->reserve==b->reserve);
}
static inline int uss_refill_scale_valid(float scale)
{uint32_t bits;memcpy(&bits,&scale,4);return scale>=FLT_MIN && (bits&0x7f800000u)!=0x7f800000u;}
static inline int uss_refill_policy(const UssRefillEvidence* e,uint64_t now,float* scale)
{
    if(!e || !scale || !e->serial || !e->completed || !e->source_verified || !e->policy_verified ||
        !uss_refill_target_valid(&e->target) || !e->target.direct_amount || e->time_ms>now || now-e->time_ms>500 ||
        e->policy_time_ms>e->time_ms || e->time_ms-e->policy_time_ms>1500)return 0;
    if(e->route==USS_REFILL_GROUND){
        static const unsigned char zero[8]={0};
        if(!e->source || e->source==UINT32_MAX || !memcmp(e->source_identity,zero,8))return 0;
        if(e->source_kind==4){*scale=0.5f;return 1;}
        if((e->source_kind==7 || e->source_kind==8 || e->source_kind==9) && uss_refill_scale_valid(e->source_scale)){
            *scale=e->source_scale;return 1;}
        return 0; /* Dedicated grenade/stim pickups never refill this weapon. */
    }
    if(e->route!=USS_REFILL_OWN_PACK || !e->pack_entity || !e->pack_manager || !e->pack_unit ||
        !e->charges_before || e->charges_before-e->charges_after!=1 || e->charges_after>=e->charges_before ||
        e->pack_spent_ms>now || now-e->pack_spent_ms<200 || now-e->pack_spent_ms>500 || !e->resource_gain || e->ground_activity_near ||
        !e->target.direct_amount)return 0;
    *scale=1.0f;return 1; /* Native setting amount controls the actual grant. */
}
static inline unsigned uss_refill_apply(UssRefillState* state,const UssRefillServices* io,const UssRefillEvidence* evidence,uint64_t now)
{
    UssRefillTarget before,after;float scale;unsigned result;
    if(!state || !io || !io->observe || !io->verify_evidence || !io->refill || !evidence || state->busy || state->disabled)return USS_REFILL_SKIPPED;
    if((state->epoch && state->epoch!=evidence->target.epoch) || evidence->serial<=state->last_serial || !uss_refill_policy(evidence,now,&scale))return USS_REFILL_SKIPPED;
    if(!state->epoch)state->epoch=evidence->target.epoch;
    state->busy=1;state->last_serial=evidence->serial; /* Spend before rechecks/call, no reentrant/repeated grant. */
    before=io->observe(io->user);
    if(!uss_refill_target_same(&evidence->target,&before,1) || !io->verify_evidence(io->user,evidence,&before)){
        state->reason="REFILL_CONTEXT_CHANGED_BEFORE_ENTRY";state->busy=0;return USS_REFILL_SKIPPED;}
    ++state->calls;result=io->refill(io->user,&before,scale);
    if(result!=USS_REFILL_CALL_RETURNED){state->disabled=1;state->busy=0;
        state->reason=result==USS_REFILL_CALL_NOT_ENTERED?"REFILL_NOT_ENTERED":"REFILL_ENTRY_UNCERTAIN";return USS_REFILL_STOPPED;}
    after=io->observe(io->user);state->busy=0;
    if(!uss_refill_target_same(&before,&after,0) || after.reserve<before.reserve || after.reserve-before.reserve>200){
        state->disabled=1;state->reason="REFILL_READBACK_CHANGED";return USS_REFILL_STOPPED;}
    if(after.reserve>before.reserve){state->notice.serial=evidence->serial;state->notice.route=evidence->route;
        state->notice.before=before;state->notice.after=after;}
    state->reason=after.reserve>before.reserve?"REFILL_CONFIRMED":"REFILL_FULL";
    return after.reserve>before.reserve?USS_REFILL_CONFIRMED:USS_REFILL_FULL;
}
/* Consume observer delivery separately; it cannot roll back or repeat refill. */
static inline int uss_refill_notify(const UssRefillState* state,unsigned result,uint64_t* notified,void* user,UssRefillObserver observer)
{
    const UssRefillNotice* notice;
    if(!state || !notified || !observer || state->busy || state->disabled || result!=USS_REFILL_CONFIRMED)return 0;
    notice=&state->notice;
    if(!notice->serial || notice->serial<=*notified || notice->serial!=state->last_serial || notice->before.epoch!=state->epoch ||
        !uss_refill_target_same(&notice->before,&notice->after,0) || notice->after.reserve<=notice->before.reserve ||
        notice->after.reserve-notice->before.reserve>200)return 0;
    *notified=notice->serial;observer(user,notice);return 1;
}
#endif
