/* Owned Core lifetime, analogous to Lua's pair table identity.
 * No game reads, callback registration, discovery, native target or effect. */
#ifndef USS_PAIR_OWNER_H
#define USS_PAIR_OWNER_H
#include "pair_transaction.h"
#include "publication_state.h"

typedef struct UssPairOwnerContext {
    UssPairContext local;
    uintptr_t session,mapping;
    unsigned role,session_version,local_only; /* 1 host, 2 joined; observed separately, never guessed. */
    size_t peer_size;
    unsigned char peers[USS_PUBLICATION_PEER_BYTES]; /* canonical validated bytes */
} UssPairOwnerContext;

typedef struct UssPairOwner {
    unsigned thread,disabled,mission,ready,context_valid,pair_present;
    uint64_t epoch,context_generation,pair_generation,guard_generation;
    UssPairOwnerContext context;
    UssPairMember utility,ordinary;
} UssPairOwner;

static int pair_owner_thread(UssPairOwner* owner,unsigned thread)
{
    if(!owner || owner->disabled || !thread)return 0;
    if(owner->thread && owner->thread!=thread){owner->disabled=1;owner->ready=0;return 0;}
    owner->thread=thread;return 1;
}
static void pair_owner_forget_pair(UssPairOwner* owner)
{
    owner->pair_present=0;
    memset(&owner->utility,0,sizeof(owner->utility));memset(&owner->ordinary,0,sizeof(owner->ordinary));
    /* Generations are lifetime counters, not handles; never reset on removal. */
}
/* Canonical peer schema: self[8], host[8], sorted members[8 each].
 * A local context has one actual self peer; absent native host is allowed. */
static inline int pair_owner_local_context(const UssPairOwnerContext* context)
{
    unsigned char zero[8]={0};
    return context && context->local_only==1 && context->role<=1 && context->peer_size==24 &&
        memcmp(context->peers,zero,8) && !memcmp(context->peers,context->peers+16,8) &&
        (!memcmp(context->peers+8,zero,8) || !memcmp(context->peers,context->peers+8,8));
}
static int pair_owner_context_valid(const UssPairOwnerContext* context)
{
    return context && context->local.owner && context->local.inventory && context->local.row_address &&
        publication_id(context->local.avatar) && publication_id(context->local.primary) &&
        context->session && context->mapping && (context->local_only?pair_owner_local_context(context):(context->role==1 || context->role==2)) &&
        context->peer_size && context->peer_size<=USS_PUBLICATION_PEER_BYTES;
}
/* Scope/selection fields can change while the same actor still owns its pair.
 * Finite work must invalidate on ANY context change, but forgetting membership
 * is reserved for a different local actor/world. Never equate a row index with
 * avatar identity: all four ownership fields and session/mapping must match. */
static inline int pair_owner_same_actor(const UssPairOwnerContext* a,const UssPairOwnerContext* b)
{
    return a && b && a->local.owner==b->local.owner && a->local.inventory==b->local.inventory &&
        a->local.row_address==b->local.row_address && a->local.avatar==b->local.avatar &&
        a->session==b->session && a->mapping==b->mapping;
}
static int pair_owner_context_equal(const UssPairOwnerContext* a,const UssPairOwnerContext* b)
{
    return pair_context_equal(&a->local,&b->local) && a->session==b->session && a->mapping==b->mapping &&
        a->local_only==b->local_only && a->role==b->role && a->session_version==b->session_version && a->peer_size==b->peer_size && !memcmp(a->peers,b->peers,a->peer_size);
}
static void pair_owner_interrupt(UssPairOwner* owner)
{
    if(owner->ready) {
        if(owner->guard_generation==UINT64_MAX)owner->disabled=1;
        else ++owner->guard_generation;
    }
    owner->ready=0;
}
/* The eventual Core/event adapter supplies validated observations. Unknown is NOT
 * mission exit: suppress outputs but retain history; do not infer a new pair.
 * Confirmed actor/world changes forget the old pair. Same-actor scope changes
 * retain membership but invalidate all old operation/publication generations. */
static int uss_pair_owner_update(UssPairOwner* owner,unsigned thread,unsigned known,
    unsigned mission,const UssPairOwnerContext* context)
{
    UssPairOwner candidate;
    if(!pair_owner_thread(owner,thread))return 0;
    if(!known){pair_owner_interrupt(owner);return 0;}
    if(!mission) {
        pair_owner_interrupt(owner);owner->mission=owner->context_valid=0;pair_owner_forget_pair(owner);
        memset(&owner->context,0,sizeof(owner->context));return 1;
    }
    if(!pair_owner_context_valid(context)){pair_owner_interrupt(owner);return 0;}
    /* Normal observations do not copy the owned peer buffer or recreate a pair. */
    if(owner->mission && owner->context_valid && pair_owner_context_equal(&owner->context,context)) {
        owner->ready=1;return 1;
    }
    candidate=*owner;
    if(!candidate.mission) {
        if(candidate.epoch==UINT64_MAX)goto exhausted;
        ++candidate.epoch;
    }
    if(!candidate.mission || !candidate.context_valid || !pair_owner_context_equal(&candidate.context,context)) {
        if(candidate.context_generation==UINT64_MAX || candidate.guard_generation==UINT64_MAX)goto exhausted;
        ++candidate.context_generation;++candidate.guard_generation;
        if(!candidate.mission || !candidate.context_valid || !pair_owner_same_actor(&candidate.context,context))
            pair_owner_forget_pair(&candidate);
        candidate.context=*context;
        memset(candidate.context.peers+context->peer_size,0,sizeof(candidate.context.peers)-context->peer_size);
    }
    candidate.ready=candidate.mission=candidate.context_valid=1;*owner=candidate;return 1;
exhausted:
    owner->disabled=1;owner->ready=0;return 0;
}
/* Call only when Core actually creates/replaces its pair object, not every poll.
 * expected=0 creates an absent pair; replacement needs its current generation.
 * Even identical reused handles receive a new lifetime on explicit replacement. */
static int uss_pair_owner_set_pair(UssPairOwner* owner,unsigned thread,uint64_t expected,
    const UssPairMember* utility,const UssPairMember* ordinary)
{
    UssPairMember u,o;unsigned char zero[8]={0};
    if(!pair_owner_thread(owner,thread) || !owner->ready || !owner->context_valid || !owner->mission ||
        !utility || !ordinary || (owner->pair_present ? expected!=owner->pair_generation : expected!=0))return 0;
    u=*utility;o=*ordinary;
    if(!publication_id(u.entity) || !publication_id(o.entity) || u.entity==o.entity ||
        u.entity==owner->context.local.avatar || o.entity==owner->context.local.avatar ||
        uss_classify_support(u.identity,2)!=USS_UTILITY_SUPPORT ||
        uss_classify_support(o.identity,2)!=USS_ORDINARY_SUPPORT || !memcmp(o.identity,zero,8))return 0;
    if(owner->pair_generation==UINT64_MAX){owner->disabled=1;owner->ready=0;return 0;}
    ++owner->pair_generation;owner->utility=u;owner->ordinary=o;owner->pair_present=1;return 1;
}
static int uss_pair_owner_clear_pair(UssPairOwner* owner,unsigned thread,uint64_t expected)
{
    if(!pair_owner_thread(owner,thread) || !owner->pair_present || expected!=owner->pair_generation)return 0;
    pair_owner_forget_pair(owner);return 1;
}
static int pair_owner_ready(const UssPairOwner* owner,unsigned thread)
{
    return owner && !owner->disabled && owner->thread==thread && owner->ready && owner->mission &&
        owner->context_valid && owner->pair_present && owner->epoch &&
        owner->context_generation && owner->pair_generation && owner->guard_generation;
}
/* Preserve all supplied flags; a stamp is not can_switch/native authority. */
static int uss_pair_owner_observation(const UssPairOwner* owner,unsigned thread,
    const UssPairObservation* current,UssPairObservation* output)
{
    UssPairObservation result={0};
    if(current){result=*current;current=&result;}
    if(output)memset(output,0,sizeof(*output));
    if(!output || !current || !pair_owner_ready(owner,thread) || !current->valid || !current->mission ||
        !current->members_valid || current->local_only!=owner->context.local_only || current->host!=(unsigned)(owner->context.role==1) ||
        current->joined_client!=(unsigned)(owner->context.role==2) ||
        !pair_context_equal(&current->context,&owner->context.local) ||
        !pair_member_equal(&current->utility,&owner->utility) || !pair_member_equal(&current->ordinary,&owner->ordinary))return 0;
    result=*current;result.epoch=owner->epoch;result.pair_generation=owner->pair_generation;
    result.guard_generation=owner->guard_generation;
    *output=result;return 1;
}
/* Bind publisher keys to the same owned lifetime, after supplied identity/owner
 * checks. Existing real capture must STILL validate all dependencies at entry. */
static int uss_pair_owner_publication(const UssPairOwner* owner,unsigned thread,unsigned role,
    const UssPublicationOwner* current,UssPublicationKey* key)
{
    if(!pair_owner_ready(owner,thread) || owner->context.local_only || !current || !key || role!=owner->context.role ||
        current->owner!=owner->context.local.owner || current->inventory!=owner->context.local.inventory ||
        current->address!=owner->context.local.row_address || current->avatar!=owner->context.local.avatar ||
        publication_u32(current->row)!=owner->context.local.primary ||
        publication_u32(current->row+4)!=owner->context.local.secondary ||
        publication_u32(current->row+12)!=owner->context.local.backpack ||
        (publication_u32(current->row+8)!=owner->utility.entity && publication_u32(current->row+8)!=owner->ordinary.entity) ||
        key->session!=owner->context.session || key->mapping!=owner->context.mapping ||
        key->owner!=current->owner || key->inventory!=current->inventory || key->avatar!=current->avatar ||
        key->utility!=owner->utility.entity || key->ordinary!=owner->ordinary.entity ||
        memcmp(key->utility_identity,owner->utility.identity,8) || memcmp(key->ordinary_identity,owner->ordinary.identity,8) ||
        key->peer_size!=owner->context.peer_size || memcmp(key->peers,owner->context.peers,key->peer_size))return 0;
    key->context_generation=owner->context_generation;key->pair_generation=owner->pair_generation;return 1;
}
#endif
