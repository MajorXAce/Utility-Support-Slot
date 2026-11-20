/* One frame of external-menu selection on the owning game thread. Thin composition of three existing/new parts so the call site stays one line:
 *   1. the C provider's copied sample (or a zeroed sample if it failed this frame: the existing revoke-at-once rule),
 *   2. the Lua companion mailbox (a NEW copied sample, a revoke sample after a malformed post, or nothing),
 *   3. the arbiter (exactly one owner) feeding the UNCHANGED provider-neutral handover policy.
 * Nothing here knows about native widgets: yielding or retaking the standalone menu stays the existing normal_external_menus / menu-lifecycle job, which needs the
 * returned view.external to be 0/1 exactly as before. */
#ifndef USS_MENU_EXTERNAL_H
#define USS_MENU_EXTERNAL_H
#include "uss_menu_arbiter.h"

static inline UssMenuHandoverView uss_menu_external_step(UssMenuHandover* handover, UssMenuArbiter* arbiter, UssLuaMenuMailbox* mailbox,
    const UssMenuProviderSample* c_sample, uint64_t now_ms, unsigned transaction_active, unsigned input_blocked,
    const UssSettings* standalone, unsigned local_input_down, UssMenuChoice* choice_out)
{
    UssMenuProviderSample lua; UssMenuHandoverView view; uint64_t receipt_ms = 0;
    unsigned invalid = 0; int have = mailbox ? uss_lua_mailbox_take_receipt(mailbox, &lua, &invalid, &receipt_ms) : 0;
    if (have && !invalid && (now_ms < receipt_ms || now_ms - receipt_ms > USS_MENU_HANDOVER_TIMEOUT_MS)) {
        memset(&lua, 0, sizeof lua); invalid = 1;
    }
    UssMenuChoice choice = uss_menu_arbiter_choose(arbiter, c_sample, have ? &lua : NULL, handover ? handover->usable : 0);
    /* Outward metadata never retains this function's stack-local Lua copy. */
    if (choice_out) { *choice_out = choice; choice_out->sample = NULL; }
    view = uss_menu_handover_step(handover, choice.sample, now_ms, transaction_active, input_blocked, standalone, local_input_down);
    if (handover && handover->usable && choice.sample == &lua && !invalid && handover->sequence == lua.sequence)
        handover->received_ms = receipt_ms;
    return view;
}
/* The selected usable source supplies input; C discovery alone is not its authority. */
static inline unsigned uss_menu_external_down(const UssMenuHandover* handover,unsigned settings_external,const UssMenuHandoverView* view)
{ return handover && view && settings_external && handover->usable && view->external ? view->utility_down : 0; }
#endif
