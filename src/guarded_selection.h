/* Offline guarded selection seam. No real game targets are bound here. */
#ifndef USS_GUARDED_SELECTION_H
#define USS_GUARDED_SELECTION_H
#include "action_state.h"
#include "native_calls.h"

typedef struct UssSelectionAttempt { unsigned consumed; } UssSelectionAttempt;
typedef enum UssSelectionOutcome {
    USS_SELECTION_NOT_ENTERED=0,
    USS_SELECTION_RETURNED_UNVERIFIED,
    USS_SELECTION_INDEX_VERIFIED
} UssSelectionOutcome;
typedef struct UssSelectionResult {
    UssSelectionOutcome outcome;
    unsigned entered, calls_returned, held_matches;
    const char* reason;
    unsigned reads, bytes;
} UssSelectionResult;

/* Lua same_context() plus copied target identity, module/session/action lifetime.
 * Active/held/action phase may change as selection proceeds; they are not owners. */
static int same_selection_context(const UssActionState* a,const UssActionState* b)
{
    const Inventory *x=&a->inventory,*y=&b->inventory;
    return a->valid && b->valid && a->manager==b->manager && a->record==b->record &&
        a->context==b->context && a->index==b->index && a->count==b->count &&
        a->game_base==b->game_base && a->game_size==b->game_size &&
        a->session_role==b->session_role && a->session_version==b->session_version &&
        x->avatar==y->avatar && x->local_unit==y->local_unit &&
        x->row_address==y->row_address && !memcmp(x->roots,y->roots,sizeof(x->roots)) &&
        !memcmp(x->avatar_header,y->avatar_header,sizeof(x->avatar_header)) &&
        !memcmp(x->slots,y->slots,sizeof(x->slots)) &&
        !memcmp(x->identities,y->identities,4*sizeof(x->identities[0])) &&
        !memcmp(x->observed_kinds,y->observed_kinds,4*sizeof(x->observed_kinds[0])) &&
        !memcmp(x->native_kinds,y->native_kinds,4*sizeof(x->native_kinds[0]));
}

static UssSelectionResult select_guarded_internal(const HfApi* api,HfMod* mod,
    const UssNativeCalls* calls,const UssActionState* expected,uint32_t slot,
    UssSelectionAttempt* selection_attempt,unsigned primary_control)
{
    UssSelectionResult result={0};
    UssActionState before,after;
    UssNativeCalls targets;
    Reader budget={0};
    result.reason="INPUT";
    if (!selection_attempt || selection_attempt->consumed || !expected || !expected->valid ||
        !calls || !calls->select_state || !calls->select_weapon || slot<1 || slot>3)
        return result;
    /* Copy callbacks before entry. Attempt ownership is single-threaded here;
     * no game-thread/ABI/pin authority is inferred from supplied pointers. */
    targets=*calls;
    before=action_state_sample(api,mod,&budget);
    if (!before.valid || !same_selection_context(expected,&before) || !before.switch_ready ||
        !valid_id(before.inventory.slots[slot-1]) ||
        (primary_control && (before.inventory.active_selector!=1 ||
            before.inventory.held!=before.inventory.slots[0] || valid_id(before.inventory.slots[2])))) {
        result.reason=budget.failure ? budget.failure : "PREFLIGHT_CHANGED_OR_NOT_READY";
        goto done;
    }
    selection_attempt->consumed=1; /* No retry through this selection_attempt, even after readback failure. */
    result.entered=1;
    if (uss_select_slot(&targets,(void*)before.record,(void*)before.inventory.roots[2],
                        before.avatar,slot)!=USS_CALL_RETURNED) {
        result.reason="ENTRY_UNCONFIRMED";goto done;
    }
    result.calls_returned=1;result.outcome=USS_SELECTION_RETURNED_UNVERIFIED;
    after=action_state_sample(api,mod,&budget);
    if (!after.valid || !same_selection_context(&before,&after) ||
        after.inventory.active_selector!=slot) {
        result.reason=budget.failure ? budget.failure : "POST_SELECTION_UNVERIFIED";
        goto done;
    }
    result.outcome=USS_SELECTION_INDEX_VERIFIED;
    result.held_matches=after.inventory.held==after.inventory.slots[slot-1];
    result.reason="INDEX_VERIFIED";
done:
    result.reads=budget.calls;result.bytes=budget.bytes;
    return result;
}
static inline UssSelectionResult uss_select_guarded(const HfApi* api,HfMod* mod,
    const UssNativeCalls* calls,const UssActionState* expected,uint32_t slot,
    UssSelectionAttempt* selection_attempt)
{
    return select_guarded_internal(api,mod,calls,expected,slot,selection_attempt,0);
}
/* Fixed diagnostic: require the primary origin again at the actual entry preflight. */
static inline UssSelectionResult uss_select_primary_control(const HfApi* api,HfMod* mod,
    const UssNativeCalls* calls,const UssActionState* expected,UssSelectionAttempt* selection_attempt)
{
    return select_guarded_internal(api,mod,calls,expected,2,selection_attempt,1);
}
#endif
