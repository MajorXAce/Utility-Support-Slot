/* Typed MAG/ROUNDS entry forwarding from the Lua resupply addon. No resolution
 * or unchecked callback registration. Native handler owns capacity/rounding. */
#ifndef USS_REFILL_NATIVE_CALLS_H
#define USS_REFILL_NATIVE_CALLS_H
#include "uss_refill_transaction.h"
typedef void (*UssRefillFn)(void*,uint32_t,float);
typedef struct UssRefillNativeCalls {UssRefillFn mag,rounds;} UssRefillNativeCalls;
static inline unsigned uss_refill_native_call(const UssRefillNativeCalls* calls,const UssRefillTarget* target,float scale)
{
    UssRefillFn call;
    if(!calls || !uss_refill_target_valid(target) || !uss_refill_scale_valid(scale) || !target->direct_amount)return USS_REFILL_CALL_NOT_ENTERED;
    call=target->kind==USS_REFILL_MAG?calls->mag:calls->rounds;if(!call)return USS_REFILL_CALL_NOT_ENTERED;
    /* Lua calls native(nil, entity, scale). Manager identity validates the
     * component snapshot; it is not the first native argument. */
    call(NULL,target->target,scale);
    return USS_REFILL_CALL_RETURNED; /* Return is not positive-gain confirmation. */
}
#endif
