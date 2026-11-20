/* Optional Lua menu companion -> C USS mailbox.
 *
 * What it is: a one-slot, copy-only mailbox. A small Lua addon registers USS's Binding Mode option and Utility Weapon binding with the REAL Lua providers
 * (ModOptionsMenu / ModBindingsMenu), then every few hundred milliseconds posts one fixed-size, versioned record here through ONE exported C function.
 * What it is not: C never calls into Lua, no Lua table or pointer is kept, no lock is held across any provider or native call, and nothing here touches the game
 * or the UI. The post may arrive on whatever thread Lua runs on; the game thread consumes copies in uss_lua_mailbox_take().
 *
 * Trust model: the record is untrusted input. Every field is validated; a malformed post can never become a pressed action, and it revokes a held lease at once
 * (the same rule the C provider path follows). A replayed or older record is ignored and cannot refresh the lease: only a NEW accepted sequence counts as a heartbeat.
 * "instance" identifies one Lua companion run (a restart or reload picks a new one) so a replacement is told apart from a replay. Sequence restarts at 1 for a new instance.
 * The output is the existing provider-neutral UssMenuProviderSample, so the proven handover policy (lease, release-before-resume, mode change rules) is reused, not copied. */
#ifndef USS_LUA_MENU_MAILBOX_H
#define USS_LUA_MENU_MAILBOX_H
#include <windows.h>
#include "uss_menu_handover.h"

enum { USS_LUA_MENU_PROTOCOL = 2, USS_LUA_MENU_MAGIC = 0x424D5355u /* 'USMB' */ };
#define USS_LUA_MENU_OPTIONS_PRESENT 1u
/* Fixed layout, only 32/64-bit integers, so LuaJIT's ffi.cdef can declare it exactly. All booleans are 0 or 1. Protocol 2 flags bit 0 identifies a supported Lua ModOptionsMenu contract; other bits are rejected. generation is nonzero and changes when provider tables change. Protocol 1 is rejected. */
typedef struct UssLuaMenuPost {
    uint32_t magic, protocol, size, flags;
    uint64_t instance, sequence;
    uint32_t options_registered, bindings_registered, ready, mode, input_valid, utility_down, action_blocked, generation;
} UssLuaMenuPost;
_Static_assert(sizeof(UssLuaMenuPost) == 64, "Mailbox record layout is part of the Lua companion contract");

typedef enum UssLuaPostResult {
    USS_LUA_POST_ACCEPTED = 0, USS_LUA_POST_NULL, USS_LUA_POST_BAD_SIZE, USS_LUA_POST_BAD_MAGIC, USS_LUA_POST_BAD_PROTOCOL, USS_LUA_POST_BAD_FLAGS,
    USS_LUA_POST_BAD_FIELDS, USS_LUA_POST_REPLAY, USS_LUA_POST_RESULT_COUNT
} UssLuaPostResult;
static inline const char* uss_lua_post_result_name(unsigned r)
{
    static const char* const names[USS_LUA_POST_RESULT_COUNT] = {"ACCEPTED", "NULL", "BAD_SIZE", "BAD_MAGIC", "BAD_PROTOCOL", "BAD_FLAGS", "BAD_FIELDS", "REPLAY"};
    return r < USS_LUA_POST_RESULT_COUNT ? names[r] : "UNKNOWN";
}
/* Provider ids from Lua carry the top bit so they can never equal a C provider id (a user-mode module handle never has bit 63 set). */
#define USS_LUA_PROVIDER_BIT UINT64_C(0x8000000000000000)

#ifndef USS_LUA_RECEIPT_CLOCK
#define USS_LUA_RECEIPT_CLOCK() GetTickCount64()
#endif

typedef struct UssLuaMenuMailbox {
    SRWLOCK lock;                 /* guards only the fields below; guards the record and receiver-clock snapshot; never held across provider or gameplay calls */
    UssLuaMenuPost slot;          /* last ACCEPTED record */
    unsigned invalidated;         /* last ordered post was malformed, even after its notification is consumed */
    unsigned fresh;               /* an accepted record has not been taken yet */
    unsigned revoke;              /* a malformed post arrived: next take reports an invalid sample once */
    unsigned counts[USS_LUA_POST_RESULT_COUNT];
    unsigned last_rejected;       /* result of the most recent malformed post (a replay is not a rejection) */
    uint64_t last_instance, last_sequence;
    uint64_t received_ms; /* Trusted receiver clock, captured under the publication lock. */
} UssLuaMenuMailbox;
static inline void uss_lua_mailbox_init(UssLuaMenuMailbox* box)
{
    memset(box, 0, sizeof *box); InitializeSRWLock(&box->lock);
}
static inline int uss_lua_post_fields_valid(const UssLuaMenuPost* p)
{
    return p->instance && p->sequence && p->options_registered <= 1 && p->bindings_registered <= 1 && p->ready <= 1 && p->input_valid <= 1 &&
        p->utility_down <= 1 && p->action_blocked <= 1 && !(p->action_blocked && p->utility_down) && p->generation != 0 && (!(p->options_registered || p->ready) || (p->flags & USS_LUA_MENU_OPTIONS_PRESENT)) &&
        (p->mode == USS_BIND_DEFAULT || p->mode == USS_BIND_DEDICATED);
}
/* Any thread. `size` is what the caller says it wrote; nothing past it is read. Returns a UssLuaPostResult. */
static inline unsigned uss_lua_mailbox_post(UssLuaMenuMailbox* box, const void* data, uint32_t size)
{
    UssLuaMenuPost local; unsigned result;
    memset(&local, 0, sizeof local);
    if (!box) return USS_LUA_POST_NULL;
    if (!data) result = USS_LUA_POST_NULL;
    else if (size != sizeof local) result = USS_LUA_POST_BAD_SIZE;
    else {
        memcpy(&local, data, sizeof local);          /* private copy: later checks cannot be raced by the caller changing its buffer */
        if (local.magic != USS_LUA_MENU_MAGIC) result = USS_LUA_POST_BAD_MAGIC;
        else if (local.protocol != USS_LUA_MENU_PROTOCOL || local.size != sizeof local) result = USS_LUA_POST_BAD_PROTOCOL;
        else if (local.flags & ~USS_LUA_MENU_OPTIONS_PRESENT) result = USS_LUA_POST_BAD_FLAGS;
        else if (!uss_lua_post_fields_valid(&local)) result = USS_LUA_POST_BAD_FIELDS;
        else result = USS_LUA_POST_ACCEPTED;
    }
    AcquireSRWLockExclusive(&box->lock);
    if (result == USS_LUA_POST_ACCEPTED && local.instance == box->last_instance && (local.sequence <= box->last_sequence || local.generation < box->slot.generation)) result = USS_LUA_POST_REPLAY;
    if (result == USS_LUA_POST_ACCEPTED) {
        /* A changed table generation cannot inherit a held binding from the prior table. */
        if (local.instance == box->last_instance && local.generation != box->slot.generation) box->revoke = 1;
        box->invalidated = 0; box->slot = local; box->received_ms = USS_LUA_RECEIPT_CLOCK(); box->fresh = 1; box->last_instance = local.instance; box->last_sequence = local.sequence;
    } else if (result != USS_LUA_POST_REPLAY) { box->revoke = 1; box->last_rejected = result; box->fresh = 0; box->invalidated = 1; }   /* malformed: revoke; a replay only has no effect */
    ++box->counts[result];
    ReleaseSRWLockExclusive(&box->lock);
    return result;
}
/* Counters for diagnostics (any thread; a brief lock, no calls inside it). */
static inline void uss_lua_mailbox_stats(UssLuaMenuMailbox* box, unsigned* accepted, unsigned* rejected, unsigned* last_rejected)
{
    unsigned r, bad = 0;
    AcquireSRWLockExclusive(&box->lock);
    if (accepted) *accepted = box->counts[USS_LUA_POST_ACCEPTED];
    for (r = 0; r < USS_LUA_POST_RESULT_COUNT; ++r) if (r != USS_LUA_POST_ACCEPTED && r != USS_LUA_POST_REPLAY) bad += box->counts[r];
    if (rejected) *rejected = bad;
    if (last_rejected) *last_rejected = box->last_rejected;
    ReleaseSRWLockExclusive(&box->lock);
}
/* Owning game thread. Returns 1 and fills *out with a NEW sample, or 0 when nothing new arrived. When a malformed post was seen, returns 1 with an INVALID sample (protocol 0)
 * once, so the handover policy revokes at once. `*invalid` says which. Arrival time is captured by the C post routine, never taken from Lua. */
static inline int uss_lua_mailbox_take_receipt(UssLuaMenuMailbox* box, UssMenuProviderSample* out, unsigned* invalid, uint64_t* receipt_ms)
{
    UssLuaMenuPost local; unsigned fresh, revoke;
    if (!box || !out) return 0;
    if (invalid) *invalid = 0;
    AcquireSRWLockExclusive(&box->lock);
    local = box->slot; fresh = box->fresh; revoke = box->revoke;
    if (receipt_ms) *receipt_ms = box->received_ms;
    if (revoke) box->revoke = 0; else box->fresh = 0;   /* a good record posted after a malformed one is still delivered on the next take */
    ReleaseSRWLockExclusive(&box->lock);
    memset(out, 0, sizeof *out);
    if (revoke) { if (invalid) *invalid = 1; return 1; }
    if (!fresh) return 0;
    out->protocol = USS_MENU_HANDOVER_PROTOCOL; out->size = sizeof *out;
    out->provider = local.instance | USS_LUA_PROVIDER_BIT; out->sequence = local.sequence;
    out->options_registered = local.options_registered; out->bindings_registered = local.bindings_registered; out->ready = local.ready;
    out->mode = local.mode; out->input_valid = local.input_valid; out->utility_down = local.utility_down; out->action_blocked = local.action_blocked;
    return 1;
}
static inline int uss_lua_mailbox_take(UssLuaMenuMailbox* box, UssMenuProviderSample* out, unsigned* invalid)
{ return uss_lua_mailbox_take_receipt(box, out, invalid, NULL); }
#endif
