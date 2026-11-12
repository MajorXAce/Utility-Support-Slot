/* One serialized update consumer for ground and Pack evidence.
 * Caller supplies its current validated target/optional Pack snapshot; this
 * driver does not start a second full-Core polling loop or register callbacks. */
#ifndef USS_REFILL_DRIVER_H
#define USS_REFILL_DRIVER_H
#include "uss_pickup_completion.h"
#include "uss_supply_observer.h"
typedef struct UssRefillDriver {
    UssPickupCompletions pickups;UssSupplyObserver supply;UssRefillState transaction;
    uint64_t next_poll,last_ms;unsigned clock_known,busy;const char* reason;
} UssRefillDriver;
static inline void uss_refill_driver_clear(UssRefillDriver* state)
{
    uss_pickup_completion_clear(&state->pickups);memset(&state->supply,0,sizeof(state->supply));state->next_poll=0;
}
static inline unsigned uss_refill_driver_tick(UssRefillDriver* state,const UssRefillMemoryIo* memory,
    const UssRefillServices* services,const UssRefillTarget* target,const UssSupplySnapshot* supply,uint64_t now)
{
    UssRefillEvidence event;unsigned result=USS_REFILL_SKIPPED;
    if(!state || !memory || !services || state->busy)return USS_REFILL_SKIPPED;
    if(state->transaction.disabled || state->pickups.disabled)return USS_REFILL_STOPPED;
    if(!uss_refill_target_valid(target) || !target->direct_amount || now>UINT64_MAX-250){uss_refill_driver_clear(state);state->reason="REFILL_DRIVER_TARGET_OR_ZERO_AMOUNT";return USS_REFILL_SKIPPED;}
    if(state->clock_known && now<state->last_ms)uss_refill_driver_clear(state);
    state->last_ms=now;state->clock_known=1;
    if(!uss_pickup_completion_bind(&state->pickups,target))return USS_REFILL_SKIPPED;
    if(now<state->next_poll)return USS_REFILL_SKIPPED;
    state->next_poll=now+25;state->busy=1;
    /* A new confirmed mission may admit new events. Never clear a disabled
     * transaction after an uncertain call/readback, or reset the shared serial. */
    if(state->transaction.epoch!=target->epoch)state->transaction.epoch=target->epoch;
    state->reason="REFILL_WAIT_COMPLETION";
    if(uss_pickup_completion_poll(&state->pickups,memory,target,now,&event)){
        memset(&state->supply,0,sizeof(state->supply));
        result=uss_refill_apply(&state->transaction,services,&event,now);
        state->reason=state->transaction.reason?state->transaction.reason:"REFILL_TRANSACTION_SKIPPED";
    }else if(supply && uss_refill_target_same(target,&supply->target,1)){
        if(uss_supply_observe(&state->supply,supply,now,
            state->pickups.bound && state->pickups.have_previous,
            (unsigned)uss_pickup_activity_near(&state->pickups,now),&state->pickups.serial,&event))
            result=uss_refill_apply(&state->transaction,services,&event,now);
    }else memset(&state->supply,0,sizeof(state->supply));
    state->busy=0;return result;
}
#endif
