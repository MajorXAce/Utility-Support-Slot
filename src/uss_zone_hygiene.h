/* Pickup-prompt hygiene for carried pair members.
 *
 * Observed (live, solo, LTM batches 8-10): a weapon USS keeps and re-attaches after the game dropped it can come back
 * CARRIED (parent = local avatar) with its ground pickup zone still OPEN (zone handle 1..0xFFFF at interaction row +0xA8)
 * and the settled state at +0x360 equal to 2 ("can be picked up"): the state of a weapon lying on the ground, and the
 * pickup prompt was showing. Carried weapons normally have closed zones (handle -1). The game's own zone-close call closes it.
 * Hypothesis (not proven): the drop opens the zone and USS's native re-attach never closes it.
 *
 * This module closes such a zone on a member of the CURRENT PAIR only: owned (parent == local avatar), identity equal to
 * the owner record, classification as recorded, zone open and settled == 2, observed in an accepted coherent observation
 * and re-proven at the mutation boundary. It never touches a weapon that is not a pair member, never writes game rows
 * itself (the game's function does), is bounded (2 attempts per member, 1 s apart, 64 calls per session) and gives up with
 * one log line instead of fighting another mod that keeps reopening the zone. Dropping the weapon later reopens its ground
 * zone as usual. Not tested in multiplayer. Separate from stored-weapon ping suppression, which is not handled here. */
#ifndef USS_ZONE_HYGIENE_H
#define USS_ZONE_HYGIENE_H
#include "core_observation.h"
#include "core_lifecycle.h"
#include "native_calls.h"

#define USS_ZONE_CLOSE_RVA 0x978610u        /* game.dll + RVA: ZONE_CLOSE(events manager, entity, zone, open) */
#define USS_ZONE_GROUND 1u                  /* zone 1 = the ground pickup */
#define USS_ZONE_SETTLED_PICKABLE 2u
#define USS_ZONE_HYGIENE_ATTEMPT_CAP 2u
#define USS_ZONE_HYGIENE_RETRY_MS 1000u
#define USS_ZONE_HYGIENE_SESSION_CAP 64u
#define USS_ZONE_PROLOGUE_BYTES 24u

/* function = pinned game address; events = interaction manager; the callee is the game's own code. */
typedef int (*UssZoneCloseInvoke)(void* user,uintptr_t function,uintptr_t events,uint32_t entity,uint32_t zone);
typedef struct UssZoneHygieneSlot { uint32_t entity; unsigned attempts; uint64_t last_ms; unsigned gave_up; } UssZoneHygieneSlot;
typedef struct UssZoneHygiene {
    uint64_t epoch,context_generation,pair_generation;
    UssZoneHygieneSlot slot[2];
    unsigned calls_total;
    UssZoneCloseInvoke invoke;void* invoke_user;
} UssZoneHygiene;
typedef struct UssZoneCloseRequest { uintptr_t function,events;uint32_t entity,zone_handle; } UssZoneCloseRequest;

static inline int uss_zone_open_handle(uint32_t zone){return zone>=1u && zone<0x10000u;}
/* The prompting state for one pair member, from evidence already read. */
static inline int uss_zone_member_prompting(const UssMemberEvidence* m,const UssPairMember* want,UssSupportClass cls)
{
    return m && want && m->status==USS_MEMBER_OBSERVED && m->entity==want->entity &&
        !memcmp(m->descriptor,want->identity,8) && m->parent_matches && m->classification==cls &&
        uss_zone_open_handle(m->zone) && m->tail==USS_ZONE_SETTLED_PICKABLE;
}
/* Mutation-boundary proof. Re-reads the inventory and the member and requires the SAME state that justified the effect;
 * checks the build and the ZONE_CLOSE prologue (24 bytes, the rip-relative displacement is a wildcard). Reads only. */
static int uss_zone_hygiene_prepare(const HfApi* api,HfMod* mod,const UssCoreObservation* current,
    const UssMemberEvidence* seen,const UssPairMember* want,UssSupportClass cls,UssZoneCloseRequest* out)
{
    static const unsigned char prologue[USS_ZONE_PROLOGUE_BYTES]={0x41,0x56,0x41,0x57,0x48,0x83,0xec,0x28,0x3b,0x15,0,0,0,0,
        0x45,0x0f,0xb6,0xf1,0x45,0x8b,0xf8,0x4c,0x8b,0xd1};
    Reader reader={0};Inventory fresh;UssMemberEvidence member;HfSession start={0},end={0};
    unsigned char bytes[USS_ZONE_PROLOGUE_BYTES];uintptr_t base,events=0;size_t size=0;const char* build;unsigned i;
    if(out)memset(out,0,sizeof(*out));
    if(!out || !current || !seen || !want || !uss_sdk_compatible(api) || !mod || !api->read || !api->module_base ||
        !api->build_id || !api->session)return 0;
    build=api->build_id();if(!build || strcmp(build,"25480438"))return 0;
    api->session(mod,&start);
    base=api->module_base("game.dll",&size);
    if(!base || size<=USS_ZONE_CLOSE_RVA+USS_ZONE_PROLOGUE_BYTES || size<0x3326DC8 || size>0x20000000 || !address_ok(base,size))return 0;
    reader.api=api;reader.slot=-1;reader.stage="ZONE_HYGIENE_PIN";
    if(!get(&reader,base+USS_ZONE_CLOSE_RVA,bytes,sizeof(bytes)))return 0;
    for(i=0;i<sizeof(prologue);++i)if(!(i>=10u && i<14u) && bytes[i]!=prologue[i])return 0;
    reader.stage="ZONE_HYGIENE_CONTEXT";
    if(!inventory_sample(&reader,&fresh) || memcmp(&fresh,&current->inventory,sizeof(fresh)))return 0;
    if(!member_fields(&reader,base,&fresh,want->entity,want->identity,&member) || reader.failure)return 0;
    if(!uss_zone_member_prompting(&member,want,cls) || member.interaction_manager!=seen->interaction_manager)return 0;
    if(!get_ptr(&reader,base+0x3326D68,&events) || events!=member.interaction_manager)return 0;
    api->session(mod,&end);
    if(start.role!=end.role || start.version!=end.version)return 0;
    out->function=base+USS_ZONE_CLOSE_RVA;out->events=events;out->entity=want->entity;out->zone_handle=member.zone;
    return 1;
}
/* One bounded pass over the two pair members. Returns 1 when a native call was made (caller re-polls soon to read it back). */
static inline unsigned uss_zone_hygiene_step(UssZoneHygiene* h,const UssCoreLifecycle* life,const UssCoreObservation* current,
    const HfApi* api,HfMod* mod,uint64_t now)
{
    const UssPairOwner* owner;unsigned i;
    if(!h || !h->invoke || !life || !current || !current->valid || !current->pair_matched || life->phase!=USS_LIFE_FORMED)return 0;
    owner=&life->owner;
    if(!owner->pair_present)return 0;
    if(h->epoch!=owner->epoch || h->context_generation!=owner->context_generation || h->pair_generation!=owner->pair_generation){
        memset(h->slot,0,sizeof(h->slot));
        h->epoch=owner->epoch;h->context_generation=owner->context_generation;h->pair_generation=owner->pair_generation;
    }
    for(i=0;i<2;++i){
        const UssMemberEvidence* seen=i?&current->ordinary:&current->utility;
        const UssPairMember* want=i?&owner->ordinary:&owner->utility;
        const UssSupportClass cls=i?USS_ORDINARY_SUPPORT:USS_UTILITY_SUPPORT;
        UssZoneHygieneSlot* slot=&h->slot[i];UssZoneCloseRequest request;int called;
        if(slot->entity!=want->entity){memset(slot,0,sizeof(*slot));slot->entity=want->entity;}
        if(!uss_zone_member_prompting(seen,want,cls)){
            if(seen->status==USS_MEMBER_OBSERVED && seen->entity==want->entity && !uss_zone_open_handle(seen->zone)){
                slot->attempts=0;slot->gave_up=0;} /* closed again (or never open): forget earlier attempts */
            continue;
        }
        if(slot->gave_up)continue;
        if(slot->attempts>=USS_ZONE_HYGIENE_ATTEMPT_CAP){
            slot->gave_up=1;
            if(api && api->log && mod)api->log(mod,"USS zone hygiene gave up entity=%u zone=%u attempts=%u",
                (unsigned)want->entity,(unsigned)seen->zone,slot->attempts);
            continue;
        }
        if(slot->attempts && (now<slot->last_ms || now-slot->last_ms<USS_ZONE_HYGIENE_RETRY_MS))continue;
        if(h->calls_total>=USS_ZONE_HYGIENE_SESSION_CAP)continue;
        /* Count the attempt first so a refusal can never turn into per-tick work. */
        ++slot->attempts;slot->last_ms=now;
        if(!uss_zone_hygiene_prepare(api,mod,current,seen,want,cls,&request))continue;
        called=h->invoke(h->invoke_user,request.function,request.events,request.entity,USS_ZONE_GROUND);
        ++h->calls_total;
        if(api && api->log && mod)api->log(mod,"USS zone hygiene closed pickup zone entity=%u handle=%u attempt=%u result=%d",
            (unsigned)request.entity,(unsigned)request.zone_handle,slot->attempts,called);
        return 1;
    }
    return 0;
}
/* Production forwarder: the game's own ZONE_CLOSE(events, entity, zone, 0). Typed forwarding only. */
static inline int uss_zone_close_invoke(void* user,uintptr_t function,uintptr_t events,uint32_t entity,uint32_t zone)
{
    UssNativeCalls calls={0};
    (void)user;calls.zone_close=(UssZoneCloseFn)function;
    return uss_close_pickup_zone(&calls,(void*)events,entity,zone)==USS_CALL_RETURNED;
}
#endif
