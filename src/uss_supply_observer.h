/* Own Supply Pack inference joins charge spend and local resource gains.
 * This is event evidence, never a refill effect or an enemy/testing aid. */
#ifndef USS_SUPPLY_OBSERVER_H
#define USS_SUPPLY_OBSERVER_H
#include "uss_refill_components.h"
typedef struct UssSupplySnapshot {
    UssRefillTarget target;UssRefillComponent pack;
    uint32_t primary,secondary,backpack,signals[5];unsigned signal_mask;
} UssSupplySnapshot;
typedef struct UssSupplyObserver {
    UssSupplySnapshot previous,base,recent;
    uint64_t last_ms,spent_ms,gain_ms;uint32_t spent;
    unsigned known,pending,gain_known;
} UssSupplyObserver;
static inline int uss_supply_valid(const UssSupplySnapshot* s)
{
    unsigned i;
    static const unsigned char pack_id[8]={0x58,0x9a,0x23,0x09,0x71,0xa4,0xf9,0x4e};
    if(!s)return 0;
    for(i=0;i<5;++i)if((s->signal_mask&(1u<<i)) && s->signals[i]>(i<3?1000u:100u))return 0;
    return uss_refill_target_valid(&s->target) && s->primary && s->primary!=UINT32_MAX &&
        s->secondary && s->secondary!=UINT32_MAX && s->backpack && s->backpack!=UINT32_MAX &&
        s->pack.entity==s->backpack && s->pack.kind==3 && s->pack.epoch==s->target.epoch &&
        s->pack.manager && s->pack.unit && s->pack.index<4096 && s->pack.reserve<=1000 &&
        !memcmp(s->pack.identity,pack_id,8) && !(s->signal_mask&~31u);
}
static inline int uss_supply_same(const UssSupplySnapshot* a,const UssSupplySnapshot* b)
{
    return uss_supply_valid(a) && uss_supply_valid(b) && uss_refill_target_same(&a->target,&b->target,1) &&
        a->primary==b->primary && a->secondary==b->secondary && a->backpack==b->backpack &&
        a->pack.manager==b->pack.manager && a->pack.unit==b->pack.unit && a->pack.index==b->pack.index &&
        !memcmp(a->pack.identity,b->pack.identity,8);
}
static inline int uss_supply_gain(const UssSupplySnapshot* a,const UssSupplySnapshot* b)
{
    unsigned i,mask=a->signal_mask&b->signal_mask;
    for(i=0;i<5;++i)if((mask&(1u<<i)) && b->signals[i]>a->signals[i])return 1;
    return 0;
}
/* Shared serial belongs to BOTH ground and pack producers. Caller serializes
 * producer calls on the game thread; independent serial counters would collide.
 * Unknown action history cancels inference, rather than implying no pickup. */
static inline int uss_supply_observe(UssSupplyObserver* state,const UssSupplySnapshot* current,
    uint64_t now,unsigned ground_history_known,unsigned ground_near,uint64_t* serial,UssRefillEvidence* output)
{
    UssSupplySnapshot old;UssRefillEvidence event={0};unsigned had;
    if(!state || !serial || !output)return 0;
    if(!uss_supply_valid(current) || !ground_history_known || ground_near ||
        (state->known && now<state->last_ms) || now>UINT64_MAX-500){memset(state,0,sizeof(*state));return 0;}
    old=state->previous;had=state->known;state->previous=*current;state->known=1;state->last_ms=now;
    if(!had || !uss_supply_same(&old,current)){state->pending=state->gain_known=0;return 0;}
    if(uss_supply_gain(&old,current)){state->recent=old;state->gain_ms=now;state->gain_known=1;}
    if(old.pack.reserve>0 && current->pack.reserve==old.pack.reserve-1){
        state->base=old;
        if(state->gain_known && now>=state->gain_ms && now-state->gain_ms<=300 &&
            uss_supply_same(&state->recent,current))state->base=state->recent;
        state->spent=current->pack.reserve;state->spent_ms=now;state->pending=1;
    }else if(old.pack.reserve!=current->pack.reserve)state->pending=0;
    if(!state->pending)return 0;
    if(!uss_supply_same(&state->base,current) || current->pack.reserve!=state->spent ||
        now<state->spent_ms || now-state->spent_ms>500){state->pending=0;return 0;}
    if(now-state->spent_ms<200 || !uss_supply_gain(&state->base,current))return 0;
    state->pending=state->gain_known=0; /* Consume before handing evidence to mutation path. */
    if(*serial==UINT64_MAX)return 0;
    event.serial=++*serial;event.target=current->target;event.time_ms=event.policy_time_ms=now;
    event.route=USS_REFILL_OWN_PACK;event.pack_manager=current->pack.manager;event.pack_unit=current->pack.unit;
    event.pack_entity=current->pack.entity;event.charges_before=state->spent+1;event.charges_after=state->spent;
    event.pack_spent_ms=state->spent_ms;event.resource_gain=1;
    event.completed=event.source_verified=event.policy_verified=1;
    *output=event;return 1;
}
#endif
