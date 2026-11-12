/* Pure lifetime/boundary port. Services here have no SDK or native binding.
 * Owner must serialize calls on one callback thread and retain this state for
 * the entire publisher lifetime. Zero-init once; reset does NOT clear disable. */
#ifndef USS_PUBLICATION_STATE_H
#define USS_PUBLICATION_STATE_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>

enum { USS_PUBLICATION_PEER_BYTES=1024, USS_PUBLICATION_FINGERPRINT_BYTES=32768 };
typedef struct UssPublicationKey {
    uint64_t pair_generation,context_generation;
    uintptr_t session,mapping,owner,inventory;
    uint32_t avatar,utility,ordinary;
    unsigned char utility_identity[8],ordinary_identity[8];
    size_t peer_size;
    unsigned char peers[USS_PUBLICATION_PEER_BYTES];
} UssPublicationKey;
typedef struct UssPublicationOwner {
    uintptr_t owner,inventory,address;
    uint32_t avatar;
    unsigned char row[48];
} UssPublicationOwner;
typedef struct UssPublicationView {
    unsigned valid,mission,pair_present,supported_pair;
    UssPublicationKey key;
} UssPublicationView;
typedef struct UssPublicationCapture {
    unsigned valid;
    UssPublicationKey key;
    UssPublicationOwner owner;
    uint32_t thread,wires[4]; /* avatar,primary,utility,ordinary */
    size_t fingerprint_size;
    unsigned char fingerprint[USS_PUBLICATION_FINGERPRINT_BYTES];
} UssPublicationCapture;
typedef enum UssPublicationStage {
    USS_PUBLICATION_PRE_ATTACH=1,USS_PUBLICATION_POST_ATTACH,
    USS_PUBLICATION_UTILITY_FIRST,USS_PUBLICATION_POLL,USS_PUBLICATION_REPLACEMENT_UTILITY,
    USS_PUBLICATION_ENTRY_FORMATION,USS_PUBLICATION_ENTRY_INCOMING
} UssPublicationStage;
typedef enum UssPublicationKind {USS_PUBLICATION_FORMATION=1,USS_PUBLICATION_INCOMING} UssPublicationKind;
typedef enum UssPublicationSend {
    USS_PUBLICATION_SEND_NOT_ENTERED=0,USS_PUBLICATION_SEND_RETURNED,USS_PUBLICATION_SEND_UNCERTAIN
} UssPublicationSend;
typedef enum UssPublicationOutcome {
    USS_PUBLICATION_REFUSED=0,USS_PUBLICATION_UNCHANGED,USS_PUBLICATION_RETURNED,
    USS_PUBLICATION_UNCERTAIN,USS_PUBLICATION_NOT_ENTERED
} UssPublicationOutcome;
typedef enum UssPublicationReason {
    USS_PUB_OK=0,USS_PUB_CONTRACT,USS_PUB_DISABLED,USS_PUB_SUSPENDED,USS_PUB_REENTRANT,
    USS_PUB_NOT_APPLICABLE,USS_PUB_CAPTURE,USS_PUB_CALLER_CHANGED,USS_PUB_ENTRY_CHANGED,
    USS_PUB_PENDING_ORDINARY,USS_PUB_ALREADY_ATTEMPTED,USS_PUB_RATE_LIMIT,
    USS_PUB_CLOCK_RANGE,USS_PUB_THROTTLED,USS_PUB_SCOPE,USS_PUB_SEND_FAILED
} UssPublicationReason;
typedef struct UssPublicationResult {
    UssPublicationOutcome outcome;
    UssPublicationReason reason;
} UssPublicationResult;
typedef struct UssPublicationState {
    unsigned record_present,formed,formation_attempted,disabled,suspended,busy,last_mission;
    uint32_t member,game_thread,used;
    uint64_t epoch,next_poll,rate_until,attempted,returned;
    UssPublicationKey record;
    uint32_t formation_wires[4];
} UssPublicationState;
typedef struct UssPublicationServices {
    void* user;
    uint64_t (*clock_ms)(void*);
    UssPublicationView (*view)(void*);
    /* Must supply a fully owned bounded capture after ALL Lua capture checks:
     * scope/authority/epoch, pair identities/parents, primary readiness, peer
     * identity, handle/wire roundtrips, component/relationship eligibility,
     * dispatch/native pins, full row and context rechecks, callback thread.
     * fingerprint is exact canonical dependency bytes, NOT a lossy hash.
     * Production implementation is not supplied by this pure state machine. */
    int (*capture)(void*,UssPublicationStage,UssPublicationCapture*);
    /* Own effect seam. Production must never install a raw unchecked sender.
     * An invocation consumes an attempt even if the service proves no entry.
     * RETURNED says nothing about delivery/application on another client. */
    UssPublicationSend (*send)(void*,UssPublicationKind,const UssPublicationCapture*,uint64_t metadata);
} UssPublicationServices;
static uint32_t publication_u32(const void* bytes)
{uint32_t value;memcpy(&value,bytes,4);return value;}
static int publication_id(uint32_t value)
{return value && value!=UINT32_MAX;}
static int publication_key_valid(const UssPublicationKey* key)
{
    return key->pair_generation && key->context_generation && key->session && key->mapping &&
        key->owner && key->inventory && publication_id(key->avatar) && publication_id(key->utility) &&
        publication_id(key->ordinary) && key->avatar!=key->utility && key->avatar!=key->ordinary &&
        key->utility!=key->ordinary && key->peer_size && key->peer_size<=USS_PUBLICATION_PEER_BYTES;
}
static int publication_key_equal(const UssPublicationKey* a,const UssPublicationKey* b)
{
    return a->pair_generation==b->pair_generation && a->context_generation==b->context_generation &&
        a->session==b->session && a->mapping==b->mapping && a->owner==b->owner &&
        a->inventory==b->inventory && a->avatar==b->avatar && a->utility==b->utility &&
        a->ordinary==b->ordinary && !memcmp(a->utility_identity,b->utility_identity,8) &&
        !memcmp(a->ordinary_identity,b->ordinary_identity,8) && a->peer_size==b->peer_size &&
        a->peer_size<=USS_PUBLICATION_PEER_BYTES && !memcmp(a->peers,b->peers,a->peer_size);
}
static int publication_owner_equal(const UssPublicationOwner* a,const UssPublicationOwner* b)
{
    return a->owner==b->owner && a->inventory==b->inventory && a->address==b->address &&
        a->avatar==b->avatar && !memcmp(a->row,b->row,48);
}
static int publication_capture_valid(const UssPublicationState* state,const UssPublicationCapture* capture)
{
    uint32_t member=publication_u32(capture->owner.row+8);
    return capture->valid && publication_key_valid(&capture->key) && capture->thread &&
        (!state->game_thread || capture->thread==state->game_thread) &&
        capture->fingerprint_size && capture->fingerprint_size<=USS_PUBLICATION_FINGERPRINT_BYTES &&
        capture->owner.owner==capture->key.owner && capture->owner.inventory==capture->key.inventory &&
        capture->owner.avatar==capture->key.avatar && capture->owner.address &&
        publication_u32(capture->owner.row+28)==1 &&
        (member==capture->key.utility || member==capture->key.ordinary);
}
static UssPublicationResult publication_result(UssPublicationOutcome outcome,UssPublicationReason reason)
{UssPublicationResult result={outcome,reason};return result;}
static void publication_clear_record(UssPublicationState* state)
{
    state->record_present=state->formed=state->formation_attempted=state->member=0;
    memset(&state->record,0,sizeof(state->record));
    memset(state->formation_wires,0,sizeof(state->formation_wires));
}
static int publication_epoch(UssPublicationState* state,unsigned mission)
{
    if (mission && !state->last_mission) {
        if (state->epoch==UINT64_MAX) {state->disabled=1;return 0;}
        ++state->epoch;publication_clear_record(state);
    }
    if (!mission) publication_clear_record(state);
    state->last_mission=!!mission;return 1;
}
static int publication_services_valid(const UssPublicationServices* services)
{return services && services->clock_ms && services->view && services->capture && services->send;}

static UssPublicationResult publication_boundary_internal(UssPublicationState* state,
    const UssPublicationServices* services,const UssPublicationOwner* owner,
    unsigned formation_only,UssPublicationStage stage)
{
    UssPublicationView view;
    UssPublicationCapture initial={0},entry={0};
    UssPublicationKind kind;
    UssPublicationSend sent;
    uint64_t now,metadata;
    uint32_t member;
    if (state->disabled) return publication_result(USS_PUBLICATION_REFUSED,USS_PUB_DISABLED);
    if (state->suspended) return publication_result(USS_PUBLICATION_REFUSED,USS_PUB_SUSPENDED);
    view=services->view(services->user);
    if (!view.valid || !view.mission || !view.pair_present || !view.supported_pair || !owner ||
        publication_u32(owner->row+28)!=1 || !publication_key_valid(&view.key))
        return publication_result(USS_PUBLICATION_REFUSED,USS_PUB_NOT_APPLICABLE);
    if (!services->capture(services->user,stage,&initial) || !publication_capture_valid(state,&initial))
        return publication_result(USS_PUBLICATION_REFUSED,USS_PUB_CAPTURE);
    if (!publication_key_equal(&view.key,&initial.key) || !publication_owner_equal(owner,&initial.owner))
        return publication_result(USS_PUBLICATION_REFUSED,USS_PUB_CALLER_CHANGED);
    if (!publication_epoch(state,1)) return publication_result(USS_PUBLICATION_REFUSED,USS_PUB_CLOCK_RANGE);
    if (!state->record_present || !publication_key_equal(&state->record,&initial.key)) {
        publication_clear_record(state);state->record=initial.key;state->record_present=1;
    }
    member=publication_u32(initial.owner.row+8);
    if (!state->formed) {
        if (member!=initial.key.ordinary && !((stage==USS_PUBLICATION_UTILITY_FIRST || stage==USS_PUBLICATION_REPLACEMENT_UTILITY) && member==initial.key.utility))
            return publication_result(USS_PUBLICATION_REFUSED,USS_PUB_PENDING_ORDINARY);
        if (state->formation_attempted) return publication_result(USS_PUBLICATION_REFUSED,USS_PUB_ALREADY_ATTEMPTED);
        kind=USS_PUBLICATION_FORMATION;
    } else {
        if (formation_only || member==state->member)
            return publication_result(USS_PUBLICATION_UNCHANGED,USS_PUB_OK);
        kind=USS_PUBLICATION_INCOMING;
    }
    now=services->clock_ms(services->user);
    if (now>=state->rate_until) {
        if (now>UINT64_MAX-10000) return publication_result(USS_PUBLICATION_REFUSED,USS_PUB_CLOCK_RANGE);
        state->used=0;state->rate_until=now+10000;
    }
    if (state->used>=64) return publication_result(USS_PUBLICATION_REFUSED,USS_PUB_RATE_LIMIT);
    if (!services->capture(services->user,kind==USS_PUBLICATION_FORMATION ?
        USS_PUBLICATION_ENTRY_FORMATION : USS_PUBLICATION_ENTRY_INCOMING,&entry) ||
        !publication_capture_valid(state,&entry))
        return publication_result(USS_PUBLICATION_REFUSED,USS_PUB_CAPTURE);
    if (!publication_key_equal(&initial.key,&entry.key) ||
        !publication_owner_equal(&initial.owner,&entry.owner) || initial.thread!=entry.thread ||
        initial.fingerprint_size!=entry.fingerprint_size ||
        memcmp(initial.fingerprint,entry.fingerprint,initial.fingerprint_size) ||
        memcmp(initial.wires,entry.wires,sizeof(initial.wires)))
        return publication_result(USS_PUBLICATION_REFUSED,USS_PUB_ENTRY_CHANGED);
    memcpy(&metadata,entry.owner.row+40,8); /* exact uint64, never a floating-point number */
    ++state->used;++state->attempted;state->game_thread=entry.thread;
    if (kind==USS_PUBLICATION_FORMATION) state->formation_attempted=1;
    sent=services->send(services->user,kind,&entry,metadata);
    if (sent!=USS_PUBLICATION_SEND_RETURNED) {
        state->disabled=1;
        return publication_result(sent==USS_PUBLICATION_SEND_NOT_ENTERED ?
            USS_PUBLICATION_NOT_ENTERED : USS_PUBLICATION_UNCERTAIN,USS_PUB_SEND_FAILED);
    }
    ++state->returned;state->formed=1;state->member=publication_u32(entry.owner.row+8);
    if (kind==USS_PUBLICATION_FORMATION) memcpy(state->formation_wires,entry.wires,sizeof(entry.wires));
    return publication_result(USS_PUBLICATION_RETURNED,USS_PUB_OK);
}
static inline UssPublicationResult uss_publication_boundary(UssPublicationState* state,
    const UssPublicationServices* services,const UssPublicationOwner* owner,
    unsigned formation_only,UssPublicationStage stage)
{
    UssPublicationResult result;
    if (!state || !publication_services_valid(services) || stage<USS_PUBLICATION_PRE_ATTACH ||
        stage>USS_PUBLICATION_REPLACEMENT_UTILITY) return publication_result(USS_PUBLICATION_REFUSED,USS_PUB_CONTRACT);
    if (state->busy) return publication_result(USS_PUBLICATION_REFUSED,USS_PUB_REENTRANT);
    state->busy=1;result=publication_boundary_internal(state,services,owner,formation_only,stage);
    state->busy=0;return result;
}
static inline UssPublicationResult uss_publication_poll(UssPublicationState* state,
    const UssPublicationServices* services,const UssPublicationOwner* owner)
{
    UssPublicationResult result;
    UssPublicationView view;
    uint64_t now;
    if (!state || !publication_services_valid(services)) return publication_result(USS_PUBLICATION_REFUSED,USS_PUB_CONTRACT);
    if (state->busy) return publication_result(USS_PUBLICATION_REFUSED,USS_PUB_REENTRANT);
    if (state->disabled || state->suspended) return publication_result(USS_PUBLICATION_REFUSED,
        state->disabled ? USS_PUB_DISABLED : USS_PUB_SUSPENDED);
    state->busy=1;now=services->clock_ms(services->user);
    if (now<state->next_poll) {state->busy=0;return publication_result(USS_PUBLICATION_REFUSED,USS_PUB_THROTTLED);}
    if (now>UINT64_MAX-250) {state->busy=0;return publication_result(USS_PUBLICATION_REFUSED,USS_PUB_CLOCK_RANGE);}
    state->next_poll=now+250;
    view=services->view(services->user);
    if (!view.valid) result=publication_result(USS_PUBLICATION_REFUSED,USS_PUB_SCOPE);
    else if (!publication_epoch(state,view.mission)) result=publication_result(USS_PUBLICATION_REFUSED,USS_PUB_CLOCK_RANGE);
    else if (!view.mission || !owner || !view.pair_present)
        result=publication_result(USS_PUBLICATION_REFUSED,USS_PUB_NOT_APPLICABLE);
    else if (state->record_present && state->formed && view.supported_pair &&
        view.key.pair_generation==state->record.pair_generation &&
        view.key.context_generation==state->record.context_generation &&
        view.key.owner==owner->owner && view.key.avatar==owner->avatar &&
        view.key.owner==state->record.owner && view.key.avatar==state->record.avatar &&
        view.key.inventory==owner->inventory && view.key.inventory==state->record.inventory &&
        view.key.utility==state->record.utility && view.key.ordinary==state->record.ordinary)
        result=publication_result(USS_PUBLICATION_UNCHANGED,USS_PUB_OK);
    else result=publication_boundary_internal(state,services,owner,0,USS_PUBLICATION_POLL);
    state->busy=0;return result;
}
static inline int uss_publication_reset(UssPublicationState* state)
{
    if (!state || state->busy) return 0;
    /* Preserve disable, suspension, pinned thread, epoch, rate and counters. */
    publication_clear_record(state);state->next_poll=0;return 1;
}
static inline int uss_publication_suspend(UssPublicationState* state)
{if (!state || state->busy) return 0;state->suspended=1;return 1;}
#endif
