/* Copy-only Lua-provider observations for the C menu receiver.
 * Both the optional Lua adapter and native Lua client publish the same fixed
 * protocol record. No Lua pointer or provider call crosses this lock. Latest
 * heartbeats coalesce; an opted-in receiver can retain eight input transitions
 * in order, bound to its actor/settings scope and trusted receipt clock.
 * Malformed input revokes immediately; replay cannot renew the lease. The
 * existing provider-neutral handover retains lease and release-before-resume
 * rules. This transport never queues or retries a native gameplay effect. */
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
    /* Opt-in semantic transitions, not native commands. Scope is receiver-owned.
     * Heartbeats coalesce; eight transitions retain original sequence/receipt.
     * Overflow revokes instead of replaying an arbitrary subset of presses. */
    struct { UssLuaMenuPost post;uint64_t received_ms; } edges[8];
    unsigned edge_head,edge_count,edge_enabled,edge_baseline;
    uint64_t edge_scope[6];unsigned edge_queued,edge_taken,edge_canceled,edge_overflow;
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
/* Receiver binds queued observations to the same actor/settings/provider scope.
 * Menu/focus/release/stopped boundaries disable and discard before consumption. */
static inline void uss_lua_mailbox_scope(UssLuaMenuMailbox* box,const uint64_t scope[6],unsigned enabled)
{
 AcquireSRWLockExclusive(&box->lock);
 if(!enabled || !box->edge_enabled || memcmp(box->edge_scope,scope,sizeof box->edge_scope)){
  box->edge_canceled+=box->edge_count;box->edge_head=box->edge_count=0;box->edge_baseline=0;
 }
 memcpy(box->edge_scope,scope,sizeof box->edge_scope);box->edge_enabled=enabled;
 if(enabled && box->slot.ready && box->slot.input_valid && !box->slot.action_blocked && !box->slot.utility_down)box->edge_baseline=1;
 ReleaseSRWLockExclusive(&box->lock);
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
        if(local.instance!=box->last_instance || local.generation!=box->slot.generation || local.mode!=box->slot.mode || !local.ready || !local.input_valid || local.action_blocked){
            box->edge_canceled+=box->edge_count;box->edge_head=box->edge_count=0;box->edge_baseline=0;
        }else if(box->edge_enabled && box->edge_baseline && local.utility_down!=box->slot.utility_down){
            if(box->edge_count==8){box->edge_canceled+=box->edge_count;box->edge_head=box->edge_count=0;box->edge_baseline=0;box->edge_enabled=0;box->revoke=1;++box->edge_overflow;}
            else {unsigned at=(box->edge_head+box->edge_count)%8;box->edges[at].post=local;box->edges[at].received_ms=USS_LUA_RECEIPT_CLOCK();++box->edge_count;++box->edge_queued;}
        }
        if(box->edge_enabled && local.ready && local.input_valid && !local.action_blocked && !local.utility_down)box->edge_baseline=1;
        box->invalidated = 0; box->slot = local; box->received_ms = USS_LUA_RECEIPT_CLOCK(); box->fresh = 1; box->last_instance = local.instance; box->last_sequence = local.sequence;
    } else if (result != USS_LUA_POST_REPLAY) { box->edge_canceled+=box->edge_count;box->edge_count=box->edge_head=box->edge_baseline=0;box->revoke = 1; box->last_rejected = result; box->fresh = 0; box->invalidated = 1; }   /* malformed: revoke; a replay only has no effect */
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
    if(!revoke && box->edge_enabled && box->edge_count){
        unsigned at=box->edge_head;local=box->edges[at].post;fresh=1;
        if(receipt_ms)*receipt_ms=box->edges[at].received_ms;
        box->edge_head=(at+1)%8;--box->edge_count;++box->edge_taken;
        if(local.sequence==box->slot.sequence)box->fresh=0;
    }else if (revoke) box->revoke = 0; else box->fresh = 0;   /* a good record posted after a malformed one is still delivered on the next take */
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
