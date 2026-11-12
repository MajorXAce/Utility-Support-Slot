/* Read-only adapter: complete local inventory -> typed support observation. */
#ifndef USS_SUPPORT_OBSERVATION_H
#define USS_SUPPORT_OBSERVATION_H
#include "../ResearchSource/inventory_reader.h"
#include "weapon_policy.h"

typedef enum UssSupportStatus {
    USS_SUPPORT_INVALID = 0,
    USS_SUPPORT_ABSENT,
    USS_SUPPORT_UNSUPPORTED,
    USS_SUPPORT_UTILITY,
    USS_SUPPORT_ORDINARY
} UssSupportStatus;

typedef struct UssSupportObservation {
    UssSupportStatus status;
    uint32_t entity, observed_kind;
    unsigned char identity[8];
} UssSupportObservation;

/* Internal conversion. Input must be a complete, successful inventory sample.
 * Descriptor agreement is rechecked here to protect the policy boundary. */
static UssSupportObservation support_from_inventory(const Inventory* inventory)
{
    UssSupportObservation result = {0};
    UssSupportClass classification;
    uint32_t entity;
    if (!inventory) return result;
    entity = inventory->slots[2];
    if (entity != u32(inventory->row+8)) return result;
    if (!valid_id(entity)) { result.status = USS_SUPPORT_ABSENT; return result; }
    if (entity != u32(inventory->identities[2]+8)) return result;
    result.entity = entity;
    result.observed_kind = inventory->observed_kinds[2];
    memcpy(result.identity, inventory->identities[2], 8);
    classification = uss_classify_support(result.identity, result.observed_kind);
    result.status = classification == USS_UTILITY_SUPPORT ? USS_SUPPORT_UTILITY :
                    classification == USS_ORDINARY_SUPPORT ? USS_SUPPORT_ORDINARY :
                    USS_SUPPORT_UNSUPPORTED;
    return result;
}

/* No cached target or fallback scan. Same SOLO/session checks as the diagnostic.
 * Invalid results contain no candidate; diagnostics never confer gameplay approval. */
static UssSupportObservation uss_observe_support(const HfApi* api, HfMod* mod,
                                                Reader* diagnostics)
{
    UssSupportObservation result = {0};
    Reader reader = {0};
    Inventory inventory;
    const char* build;
    HfSession before = {0}, after = {0};
    reader.api = api;
    reader.slot = -1;
    if (!api || !mod || !uss_sdk_compatible(api) ||
        !api->read || !api->module_base || !api->session || !api->build_id) {
        reader.failure = "API_CONTRACT";
    } else if (!(build = api->build_id()) || strcmp(build, "25480438")) {
        reader.failure = "BUILD_CONTRACT";
    } else {
        api->session(mod, &before);
        if (before.role != HF_SESSION_SOLO) reader.failure = "SESSION_ROLE";
        else {
            int complete = inventory_sample(&reader, &inventory);
            api->session(mod, &after);
            if (before.role != after.role || before.version != after.version)
                reader.failure = "SESSION_CHANGED";
            else if (complete) result = support_from_inventory(&inventory);
        }
    }
    if (diagnostics) *diagnostics = reader;
    return result;
}
#endif
