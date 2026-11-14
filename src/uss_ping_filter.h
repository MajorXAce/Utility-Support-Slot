/* Stored-weapon ping suppression: a port of the Lua version's filter test (Source/utility_support_slot.lua, filter_*).
 *
 * The weapon a pair keeps STORED is still carried by the character, so other players' pings could target it. The game's
 * own physics gives every weapon a dedicated "ping helper" body with a collision filter; an engine command switches that
 * filter between catalog index 26 (normal) and 37 (the same filter without pings). This module only decides WHEN to ask
 * for the switch and when to put it back; the engine call and every layout read live in uss_ping_filter_native.h and are
 * reached through UssPingFilterIo, so this state machine is testable without a game.
 *
 * Behaviour kept from the Lua version: only the stored weapon is touched; the command is only queued (confirmation is
 * read back on later polls); the helper instance is tracked so a replaced helper is forgotten and never "restored" into
 * a different object; restore is queued when the stored weapon changes, the pair ends or the state is not managed; a 1 s
 * deadline for each queued command; at most 32 applies per 10 s; and a permanent failure latch (until a new actor) for an
 * unverified engine/catalog, an unexpected filter value or a timeout. Pickup-prompt handling is separate (uss_zone_hygiene.h). */
#ifndef USS_PING_FILTER_H
#define USS_PING_FILTER_H
#include <stdint.h>
#include <string.h>

#define USS_PING_NORMAL_INDEX 26u            /* catalog index of the weapon helper's normal filter */
#define USS_PING_EXCLUDED_INDEX 37u          /* the same filter with pings excluded */
#define USS_PING_NORMAL_HASH 0x236122F0u     /* catalog hash for index 26 */
#define USS_PING_EXCLUDED_HASH 0xFAEE2C05u   /* catalog hash for index 37 */
#define USS_PING_DEADLINE_MS 1000u
#define USS_PING_WINDOW_MS 10000u
#define USS_PING_WINDOW_CAP 32u

/* What a fresh read found for one weapon's ping helper. `packed` holds the body's filter data; its low 7 bits are the
 * catalog index. `actor` is the physics actor id (not the weapon entity). */
typedef struct UssPingHelper { uint32_t entity,actor,body_id,packed;unsigned char identity[8]; } UssPingHelper;
/* The weapon that SHOULD be excluded now: the carried, stored member of a coherent formed pair, with both other slots
 * filled. valid==0 means nothing should be excluded (restore whatever was). */
typedef struct UssPingDesired { unsigned valid;uint32_t entity;unsigned char identity[8];uintptr_t owner; } UssPingDesired;
typedef struct UssPingFilterIo {
    void* user;
    /* One-time proof of the engine function, its code and the filter catalog; 1 when the filter may be used. */
    int (*ready)(void*);
    /* Fresh read-only resolution of the weapon's helper (identity, layout and body checks inside); 1 on success. */
    int (*resolve)(void*,uint32_t entity,const unsigned char identity[8],UssPingHelper* out);
    /* Queue the engine's filter command for one actor (count 1); 1 when the call was entered. Never retried here. */
    int (*send)(void*,uint32_t actor,uint32_t hash);
    /* Optional, rare events: APPLY_QUEUED, APPLY_CONFIRMED, RESTORE_QUEUED, RESTORE_CONFIRMED, FORGET_*, FAILED_*. */
    void (*note)(void*,const char* event,uint32_t entity,uint32_t value);
} UssPingFilterIo;
typedef struct UssPingFilter {
    UssPingFilterIo io;
    unsigned failed,checked,ready;
    struct { unsigned valid;UssPingHelper helper;uintptr_t owner;uint64_t deadline;unsigned confirmed,restoring,restore_sent; } old;
    uint64_t window_until;unsigned window_used,calls;
} UssPingFilter;

static inline void uss_ping_note(const UssPingFilter* f,const char* event,uint32_t entity,uint32_t value)
{if(f->io.note)f->io.note(f->io.user,event,entity,value);}
static inline void uss_ping_filter_init(UssPingFilter* f,const UssPingFilterIo* io)
{memset(f,0,sizeof(*f));if(io)f->io=*io;}
/* A new actor (death, mission change): the helpers of the old one no longer exist. The failure latch is cleared too. */
static inline void uss_ping_filter_forget(UssPingFilter* f)
{UssPingFilterIo io=f->io;memset(f,0,sizeof(*f));f->io=io;}
static inline unsigned uss_ping_filter_active(const UssPingFilter* f){return f && f->old.valid;}

/* Put the original filter back for the helper being tracked. `r` is its FRESH resolution, or NULL when it cannot be read. */
static inline unsigned uss_ping_restore(UssPingFilter* f,const UssPingHelper* r,uint64_t now)
{
    unsigned sent=0;
    if(!f->old.valid)return 0;
    if(!r){uss_ping_note(f,"FORGET_STALE_HELPER",f->old.helper.entity,0);f->old.valid=0;return 0;}
    f->old.restoring=1;
    switch(r->packed&127u){
    case USS_PING_NORMAL_INDEX:
        if(f->old.confirmed || f->old.restore_sent){uss_ping_note(f,"RESTORE_CONFIRMED",r->entity,0);f->old.valid=0;}
        break;
    case USS_PING_EXCLUDED_INDEX:
        if(!f->old.restore_sent){
            f->old.restore_sent=1;f->old.deadline=now+USS_PING_DEADLINE_MS;
            if(f->io.send && f->io.send(f->io.user,r->actor,USS_PING_NORMAL_HASH)){++f->calls;sent=1;}
            uss_ping_note(f,"RESTORE_QUEUED",r->entity,r->actor);
        }
        break;
    default: /* somebody else changed the filter: leave it alone and stop */
        uss_ping_note(f,"FORGET_EXTERNAL_CHANGE",r->entity,r->packed&127u);
        f->old.valid=0;f->failed=1;break;
    }
    return sent;
}
/* One pass. `current_owner` is the present owner token (a different one means the tracked helper belongs to a retired
 * actor: forget it, send nothing). Returns 1 when an engine command was sent. */
static inline unsigned uss_ping_filter_step(UssPingFilter* f,const UssPingDesired* desired,uintptr_t current_owner,uint64_t now)
{
    UssPingHelper r;unsigned have,sent=0;
    if(!f || !f->io.resolve || !f->io.send)return 0;
    if(!f->old.valid && (!desired || !desired->valid))return 0;
    if(f->old.valid && f->old.owner!=current_owner){uss_ping_note(f,"FORGET_OWNER_CHANGED",f->old.helper.entity,0);f->old.valid=0;}
    if(desired && desired->valid && !f->failed){
        if(!f->checked){f->checked=1;f->ready=f->io.ready?(unsigned)(f->io.ready(f->io.user)!=0):0;}
        if(!f->ready){f->failed=1;uss_ping_note(f,"FAILED_ENGINE_OR_CATALOG",desired->entity,0);return 0;}
    }
    if(f->old.valid){
        have=f->io.resolve(f->io.user,f->old.helper.entity,f->old.helper.identity,&r)!=0 &&
            r.actor==f->old.helper.actor && r.body_id==f->old.helper.body_id;
        if(!have)return uss_ping_restore(f,NULL,now);
        if(f->old.restoring || !desired || !desired->valid || desired->entity!=f->old.helper.entity)sent=uss_ping_restore(f,&r,now);
        else if((r.packed&127u)==USS_PING_EXCLUDED_INDEX){
            if(!f->old.confirmed){f->old.confirmed=1;uss_ping_note(f,"APPLY_CONFIRMED",r.entity,0);}
        }else if((r.packed&127u)!=USS_PING_NORMAL_INDEX){
            f->old.valid=0;f->failed=1;uss_ping_note(f,"FORGET_EXTERNAL_CHANGE",r.entity,r.packed&127u);
        }
        if(!f->failed && f->old.valid && now>f->old.deadline && (f->old.restoring || !f->old.confirmed)){
            f->failed=1;uss_ping_note(f,"FAILED_TIMEOUT",f->old.helper.entity,0);
            sent+=uss_ping_restore(f,&r,now);
        }
    }
    if(desired && desired->valid && !f->old.valid && !f->failed){
        if(!f->io.resolve(f->io.user,desired->entity,desired->identity,&r))return sent;
        if((r.packed&127u)!=USS_PING_NORMAL_INDEX){f->failed=1;uss_ping_note(f,"FAILED_UNEXPECTED_FILTER",desired->entity,r.packed&127u);return sent;}
        if(now>=f->window_until){f->window_used=0;f->window_until=now+USS_PING_WINDOW_MS;}
        if(f->window_used>=USS_PING_WINDOW_CAP){f->failed=1;uss_ping_note(f,"FAILED_RATE_LIMIT",desired->entity,0);return sent;}
        ++f->window_used;
        r.entity=desired->entity;memcpy(r.identity,desired->identity,8);
        f->old.valid=1;f->old.helper=r;f->old.owner=desired->owner;f->old.deadline=now+USS_PING_DEADLINE_MS;
        f->old.confirmed=f->old.restoring=f->old.restore_sent=0;
        if(f->io.send(f->io.user,r.actor,USS_PING_EXCLUDED_HASH)){++f->calls;++sent;}
        uss_ping_note(f,"APPLY_QUEUED",desired->entity,r.actor);
    }
    return sent;
}
#endif
