/* Single active menu provider. pure policy, no I/O, game thread only.
 *
 * USS can hear from two optional menu sources: the C Megapack provider (existing, negotiated API) and the Lua companion mailbox. Exactly ONE may drive Binding Mode and the
 * Utility Weapon input at a time, and registrations must never be duplicated into the same logical provider. Rules, in order:
 *   1. A usable C sample wins when nobody owns the menu (the existing, accepted path is preferred).
 *   2. Once a source owns the menu it keeps it while its lease holds: no flip-flopping on every frame when both are healthy. A different provider id would force a release anyway.
 *   3. The owner is dropped only when the handover policy reports the lease gone (not on a single missing frame).
 *   4. After loss, selection starts again from rule 1.
 * "Usable" means uss_menu_provider_valid(): registered options and bindings, ready, valid input, valid mode. A half-ready source is never selected, so partial registration leaves the
 * standalone gameplay/settings values in charge; separate presence policy decides whether native settings UI may append. The arbiter only SELECTS which copied sample reaches uss_menu_handover_step; it does not interpret or modify sample content. */
#ifndef USS_MENU_ARBITER_H
#define USS_MENU_ARBITER_H
#include "uss_lua_menu_mailbox.h"

typedef enum UssMenuSource { USS_SOURCE_NONE = 0, USS_SOURCE_C = 1, USS_SOURCE_LUA = 2 } UssMenuSource;
typedef struct UssMenuArbiter {
    unsigned owner;               /* UssMenuSource currently driving the handover */
    unsigned suppressed_lua;      /* Lua samples set aside because C owns the menu (diagnostic counter) */
    unsigned suppressed_c;        /* usable C samples set aside because Lua owns it */
    unsigned switches;            /* completed ownership changes, for diagnostics */
} UssMenuArbiter;
typedef struct UssMenuChoice {
    const UssMenuProviderSample* sample;   /* what to pass to uss_menu_handover_step; NULL = "nothing new this frame" (lease keeps running) */
    unsigned source;                       /* UssMenuSource that produced it, or NONE */
    unsigned changed;                      /* ownership changed this frame */
} UssMenuChoice;

/* c_sample / lua_sample: the copied samples of THIS frame, or NULL if that source produced nothing new. A source that tried and failed must pass a zeroed (invalid) sample so the
 * revoke-at-once rule applies. handover_usable: UssMenuHandover.usable BEFORE this frame's step. */
static inline UssMenuChoice uss_menu_arbiter_choose(UssMenuArbiter* a, const UssMenuProviderSample* c_sample, const UssMenuProviderSample* lua_sample, unsigned handover_usable)
{
    UssMenuChoice choice = {0, USS_SOURCE_NONE, 0}; unsigned before, c_ok, lua_ok;
    if (!a) return choice;
    before = a->owner;
    c_ok = c_sample && uss_menu_provider_valid(c_sample);
    lua_ok = lua_sample && uss_menu_provider_valid(lua_sample);
    /* Rule 3: an owner is released only after the handover lease is gone AND it produced nothing usable now. */
    if (a->owner && !handover_usable && !(a->owner == USS_SOURCE_C ? c_ok : lua_ok)) a->owner = USS_SOURCE_NONE;
    if (a->owner == USS_SOURCE_NONE) a->owner = c_ok ? USS_SOURCE_C : lua_ok ? USS_SOURCE_LUA : USS_SOURCE_NONE;   /* rules 1 and 4 */
    if (a->owner == USS_SOURCE_C) { choice.sample = c_sample; if (lua_ok) ++a->suppressed_lua; }
    else if (a->owner == USS_SOURCE_LUA) { choice.sample = lua_sample; if (c_ok) ++a->suppressed_c; }
    else {
        /* Nobody owns the menu. Pass an invalid C sample through (the existing revoke-at-once rule) but never a stale Lua one; a Lua failure sample passes too. */
        choice.sample = (c_sample && !c_ok) ? c_sample : (lua_sample && !lua_ok) ? lua_sample : c_sample ? c_sample : NULL;
    }
    choice.source = a->owner; choice.changed = a->owner != before;
    if (choice.changed) ++a->switches;
    return choice;
}
static inline const char* uss_menu_source_name(unsigned source)
{
    return source == USS_SOURCE_C ? "C" : source == USS_SOURCE_LUA ? "LUA" : "NONE";
}
#endif
