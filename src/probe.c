/* Authored offline draft. No inventory/native-memory access. */
#include "sdk_compat.h"
#include <string.h>
#include <stdatomic.h>

/* A later reviewed build must explicitly enable this. Never saved as session state. */
#ifndef HF_USS_ENABLE_PROBE
#define HF_USS_ENABLE_PROBE 0
#endif

static const HfApi* sdk;
/* Publish immutable context before registration; the loader and game thread may
 * overlap. Atomic budget also makes a reported registration failure safe. */
static atomic_uint remaining;
static HfSession initial_session;

static void probe_tick(HfMod* mod, float dt, void* user)
{
    HfSession current = {0};
    unsigned budget = atomic_load_explicit(&remaining, memory_order_acquire);
    (void)dt;
    (void)user;
    do {
        if (budget == 0) return;
    } while (!atomic_compare_exchange_weak_explicit(&remaining, &budget, budget - 1,
                                                  memory_order_acq_rel, memory_order_acquire));
    sdk->session(mod, &current);
    if (current.role != HF_SESSION_SOLO ||
        current.version != initial_session.version) {
        atomic_store_explicit(&remaining, 0, memory_order_release);
        sdk->log(mod, "HF-USS: context changed; stopped, no retry");
        return;
    }
    sdk->log(mod, "HF-USS: game-thread callback; remaining=%u; inventory unavailable",
             budget - 1);
}

HF_MOD_EXPORT int hellforge_mod_init(const HfApi* api, HfMod* mod)
{
    /* size guards our required prefix; version guards its minimum ABI revision.
     * As in Lua, test availability before use; C additionally needs byte bounds. */
    if (!api || !mod || !uss_sdk_compatible(api)) return 1;
    if (!api->log || !api->build_id) return 1;
    if (!HF_USS_ENABLE_PROBE) {
        api->log(mod, "HF-USS: inactive offline draft");
        return 0;
    }
    if (!api->session || !api->on_game_thread) return 1;
    {
        const char* build = api->build_id();
        if (!build || strcmp(build, "25480438") != 0) return 1;
    }
    api->session(mod, &initial_session);
    if (initial_session.role != HF_SESSION_SOLO) return 1;
    sdk = api;
    atomic_store_explicit(&remaining, 3, memory_order_release);
    api->log(mod, "HF-USS: SDK=%u size=%u build=25480438 session-version=%u",
             api->version, api->size, initial_session.version);
    if (!api->on_game_thread(mod, probe_tick, NULL)) {
        atomic_store_explicit(&remaining, 0, memory_order_release);
        api->log(mod, "HF-USS: registration reported failure; stopped/no retry; staying loaded");
        return 0;
    }
    /* After successful registration never return failure: callback lifetime has
     * no documented cancellation. Storage remains valid for the whole DLL life. */
    return 0;
}
