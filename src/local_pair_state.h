/* Local ownership receipt, deliberately separate from network publication.
 * Created from current exact identities/parents only; no effect or SDK access. */
#ifndef USS_LOCAL_PAIR_STATE_H
#define USS_LOCAL_PAIR_STATE_H
#include "core_observation.h"
typedef struct UssLocalPairState {
    unsigned valid;UssPairOwner owner;uint32_t member;
} UssLocalPairState;
static inline int uss_local_pair_owned(const UssLocalPairState* state,const UssPairOwner* owner)
{
    return state && state->valid && pair_owner_ready(owner,owner?owner->thread:0) &&
        pair_owner_local_context(&owner->context) && state->owner.thread==owner->thread &&
        state->owner.epoch==owner->epoch && state->owner.context_generation==owner->context_generation &&
        state->owner.pair_generation==owner->pair_generation && state->owner.guard_generation==owner->guard_generation &&
        pair_owner_context_equal(&state->owner.context,&owner->context) &&
        pair_member_equal(&state->owner.utility,&owner->utility) && pair_member_equal(&state->owner.ordinary,&owner->ordinary);
}
/* A partial replacement shape does not overwrite the last verified ownership.
 * It remains bound to the old pair until explicit handover/invalidation. */
static inline int uss_local_pair_accept(UssLocalPairState* state,const UssPairOwner* owner,const UssCoreObservation* current)
{
    if(!state || !owner || !current || !current->valid || !current->mission || !current->pair_matched ||
        !pair_owner_ready(owner,owner->thread) || !pair_owner_local_context(&owner->context) ||
        !pair_owner_context_equal(&owner->context,&current->context) ||
        current->expected_epoch!=owner->epoch || current->expected_context!=owner->context_generation ||
        current->expected_pair!=owner->pair_generation || current->expected_guard!=owner->guard_generation ||
        current->utility.status!=USS_MEMBER_OBSERVED || current->ordinary.status!=USS_MEMBER_OBSERVED ||
        current->utility.parent!=owner->context.local.avatar || current->ordinary.parent!=owner->context.local.avatar ||
        current->utility.classification!=USS_UTILITY_SUPPORT || current->ordinary.classification!=USS_ORDINARY_SUPPORT ||
        current->utility.entity!=owner->utility.entity || memcmp(current->utility.descriptor,owner->utility.identity,8) ||
        current->ordinary.entity!=owner->ordinary.entity || memcmp(current->ordinary.descriptor,owner->ordinary.identity,8) ||
        !current->utility.parent_matches || !current->ordinary.parent_matches ||
        !current->utility.interaction_admissible || !current->ordinary.interaction_admissible ||
        (current->inventory.slots[2]!=owner->utility.entity && current->inventory.slots[2]!=owner->ordinary.entity))return 0;
    state->valid=1;state->owner=*owner;state->member=current->inventory.slots[2];return 1;
}
static inline void uss_local_pair_invalidate(UssLocalPairState* state)
{if(state)memset(state,0,sizeof(*state));}
#endif
