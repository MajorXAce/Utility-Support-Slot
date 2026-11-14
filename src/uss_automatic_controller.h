/* Automatic pickup/formation/replacement scheduling, independent of command
 * files and operator-selected test windows. Required effect services still own
 * their full current native/ABI/publication validation. */
#ifndef USS_AUTOMATIC_CONTROLLER_H
#define USS_AUTOMATIC_CONTROLLER_H
#include "uss_gameplay_loop.h"
#include "formation_runtime.h"
#include "replacement_transaction.h"
#include "uss_active_replacement.h"
typedef struct UssAutomaticIo {
    void* user;
    int (*pickup_policy)(void*,UssPickupPolicy*);
    /* Called only BEFORE pickup, while current support belongs to the old pair.
     * Native adapter must retain its full origin/mapping evidence internally. */
    int (*origin)(void*,UssReplacementOrigin*);
    /* Fresh adapter/services per admitted intent; no reuse of previous consumed
     * entry bits or descriptor anchors. Called once, never retried. */
    int (*prepare_replace)(void*,UssReplacementWork*,const UssReplacementOrigin*,UssReplacementServices*);
    /* Optional pickup-prompt hygiene for carried pair members (uss_zone_hygiene.h). Returns 1 after a native call. */
    unsigned (*zone_hygiene)(void*,UssCoreLifecycle*,const UssCoreObservation*,uint64_t);
} UssAutomaticIo;
typedef struct UssAutomaticController {
    UssFormationRuntime* formation;const HfApi* api;HfMod* mod;UssCorePlatform platform;UssAutomaticIo io;
    UssReplacementOrigin origin;UssReplacementWork replacement;UssReplacementServices services;
    unsigned origin_valid,stopped;uint64_t origin_ms;UssPickupKind decision;const char* reason;
    uintptr_t refusal_signature; /* last logged candidate refusal; avoids repeating one log line */
} UssAutomaticController;
static inline unsigned automatic_stop(UssAutomaticController* c,const char* reason)
{c->stopped=1;c->origin_valid=0;c->reason=reason;return USS_CONTROL_STOP;}
static inline int automatic_origin_matches(const UssReplacementOrigin* o,const UssCoreLifecycle* life,
    const UssCoreObservation* current)
{
    return o->known && o->epoch==life->owner.epoch && o->context_generation==life->owner.context_generation &&
        o->pair_generation==life->owner.pair_generation && o->guard_generation==life->owner.guard_generation &&
        o->real==current->inventory.slots[2] && o->selected==current->inventory.active_selector &&
        o->utility_drawn==(unsigned)(o->selected==3 && o->real==life->owner.utility.entity);
}
static inline unsigned uss_automatic_control(void* user,UssCoreLifecycle* life,
    const UssCoreObservation* current,uint64_t now)
{
    UssAutomaticController* c=(UssAutomaticController*)user;UssFormationRuntime* r;
    UssPairMember incoming;UssCoreCandidateNames names;UssCoreCandidates bundle;UssCoreFlags flags;
    UssPickupPolicy policy={0};UssPickupDecision decision={0};unsigned pass,policy_attempted=0;
    Reader refusal={0};
    if(!c || !c->formation)return USS_CONTROL_STOP;r=c->formation;
    if(c->stopped || life!=&r->life)return automatic_stop(c,"CONTROLLER_OWNER_CHANGED");
    if(!current || !lifecycle_current(life,r->thread,current)){
        c->origin_valid=0;c->reason="CURRENT_CONTEXT_UNAVAILABLE";return USS_CONTROL_WORK;}
    /* Normal policy belongs to the owner loop, never a standing-test constant. */
    r->policy=current->flags;
    if(!uss_local_pair_owned(&r->local,&life->owner))uss_local_pair_invalidate(&r->local);
    if(current->context.local_only)uss_local_pair_accept(&r->local,&life->owner,current);
    if(c->replacement.phase==USS_REPLACE_ACTIVE){
        UssReplacementPhase phase=uss_replacement_tick(&c->replacement,life,r->thread,&r->history,&r->publication,&c->services,now,now);
        c->origin_valid=0;if(phase==USS_REPLACE_CANCELLED)return automatic_stop(c,c->replacement.reason);
        c->reason="REPLACEMENT_WORK";return USS_CONTROL_WORK;
    }
    if(r->work.phase==USS_PAIR_ACTIVE){
        UssPairPhase phase=uss_formation_tick(r);c->origin_valid=0;
        if(phase==USS_PAIR_CANCELLED)return automatic_stop(c,r->reason);
        c->reason="FORMATION_WORK";return USS_CONTROL_WORK;
    }
    if(life->phase==USS_LIFE_PAUSED)return automatic_stop(c,"LIFECYCLE_PAUSED");
    incoming.entity=current->inventory.slots[2];memcpy(incoming.identity,current->inventory.identities[2],8);
    if(!publication_id(incoming.entity)){
        c->origin_valid=0;if(!life->owner.pair_present)pickup_forget(&r->history);
        c->reason="RECOVERY_REQUIRED";return USS_CONTROL_WORK;
    }
    if(life->owner.pair_present && c->origin_valid && incoming.entity!=life->owner.utility.entity && incoming.entity!=life->owner.ordinary.entity){
        const UssMemberEvidence* old=current->support_class==USS_UTILITY_SUPPORT?&current->utility:&current->ordinary;
        if(c->origin.real==old->entity && old->parent==current->inventory.avatar){
            /* Vanilla release may settle after the incoming slot changes.
             * Wait before creating/consuming an intent; no native effect retries. */
            if(now<c->origin_ms || now-c->origin_ms>500)return automatic_stop(c,"NATIVE_RELEASE_TIMEOUT");
            c->reason="WAIT_NATIVE_RELEASE";return USS_CONTROL_WORK;
        }
    }
    if(!uss_core_candidate_names(&life->owner,&incoming,1,&names))return automatic_stop(c,"CANDIDATE_NAMES_REFUSED");
    for(pass=0;pass<2;++pass){
        flags=uss_lifecycle_flags(life,&r->policy);
        if(pass==0 && current->pair_matched && names.count==1 &&
            (incoming.entity==life->owner.utility.entity || incoming.entity==life->owner.ordinary.entity)){
            /* Normal update already verified both owned members coherently. */
            memset(&bundle,0,sizeof(bundle));bundle.valid=1;bundle.current=*current;bundle.names=names;
            bundle.members[0]=incoming.entity==life->owner.utility.entity?current->utility:current->ordinary;
        }else bundle=uss_core_observe_candidates(c->api,c->mod,&life->owner,&flags,&c->platform,&names,&refusal);
        if(!bundle.valid){
            /* Log-only: which reader gate refused this candidate. Repeated identical refusals log once. */
            uintptr_t signature=(uintptr_t)refusal.failure*31u+(uintptr_t)refusal.stage*17u+incoming.entity+refusal.diag_native_kind*7u+refusal.diag_effective_kind+(uintptr_t)refusal.diag_fallback*13u;
            if(c->api && c->api->log && signature!=c->refusal_signature){
                uint64_t identity;memcpy(&identity,incoming.identity,8);
                c->api->log(c->mod,"USS candidate refused failure=%s stage=%s slot=%d entity=%u identity=%016llx native_kind=%u effective_kind=%u support_proof=%s",
                    refusal.failure?refusal.failure:"NONE",refusal.stage?refusal.stage:"NONE",refusal.slot,
                    (unsigned)incoming.entity,(unsigned long long)identity,(unsigned)refusal.diag_native_kind,(unsigned)refusal.diag_effective_kind,
                    refusal.diag_fallback?refusal.diag_fallback:"NOT_RUN");
                c->refusal_signature=signature;
            }
            c->origin_valid=0;c->reason="CANDIDATE_READ_UNAVAILABLE";return USS_CONTROL_WORK;}
        decision=uss_pickup_step(&r->history,life,r->thread,&bundle,&policy);
        /* The real preference is read only when the pickup decision needs it.
         * Idle managed pairs and utility-first formation do not poll settings.
         * WAIT_POLICY consumes no intent/effect; recompute on the same coherent
         * bundle after one bounded preference read, never after a native call. */
        if(decision.kind==USS_PICKUP_WAIT_POLICY && !policy_attempted){
            policy_attempted=1;
            if(c->io.pickup_policy && c->io.pickup_policy(c->io.user,&policy) && policy.known)
                decision=uss_pickup_step(&r->history,life,r->thread,&bundle,&policy);
        }
        if(decision.kind!=USS_PICKUP_NEED_READ)break;
        names=decision.needed;
    }
    c->decision=decision.kind;
    if(decision.kind==USS_PICKUP_PAIR_PENDING){
        c->origin_valid=0;
        if(!uss_formation_start(r,life->plan.order,NULL))return automatic_stop(c,"FORMATION_START_REFUSED");
        c->reason="FORMATION_STARTED";return USS_CONTROL_WORK;
    }
    if(decision.kind==USS_PICKUP_REPLACEMENT_INTENT){
        UssReplacementOrigin origin=c->origin;unsigned valid=c->origin_valid;c->origin_valid=0;
        if(!valid || now<c->origin_ms || now-c->origin_ms>500)
            return automatic_stop(c,"PRE_PICKUP_ORIGIN_UNAVAILABLE");
        if(origin.real==decision.released.entity){
            UssPairMember members[3];UssCoreCandidates fresh;UssCoreCandidateNames required;
            unsigned utility=uss_classify_support(decision.incoming.identity,2)==USS_UTILITY_SUPPORT;
            members[0]=utility?decision.incoming:decision.kept;members[1]=utility?decision.kept:decision.incoming;members[2]=decision.released;
            if(!uss_core_candidate_names(&life->owner,members,3,&required))return automatic_stop(c,"ACTIVE_REPLACEMENT_NAMES");
            flags=uss_lifecycle_flags(life,&r->policy);
            fresh=uss_core_observe_candidates(c->api,c->mod,&life->owner,&flags,&c->platform,&required,NULL);
            if(!(life->owner.context.local_only?
                uss_active_replacement_prepare_local(life,r->thread,&r->history,&decision,&origin,&fresh,&r->publication,&r->local):
                uss_active_replacement_prepare(life,r->thread,&r->history,&decision,&origin,&fresh,&r->publication)) ||
                !uss_formation_start(r,life->plan.order,NULL))return automatic_stop(c,"ACTIVE_REPLACEMENT_REFUSED");
            c->reason="ACTIVE_REPLACEMENT_FORMATION";return USS_CONTROL_WORK;
        }
        if(!c->io.prepare_replace)return automatic_stop(c,"REPLACEMENT_SERVICES_UNAVAILABLE");
        if(c->replacement.phase!=USS_REPLACE_IDLE && c->replacement.phase!=USS_REPLACE_COMPLETE)
            return automatic_stop(c,"REPLACEMENT_STATE_UNAVAILABLE");
        memset(&c->replacement,0,sizeof(c->replacement));memset(&c->services,0,sizeof(c->services));
        if(!c->io.prepare_replace(c->io.user,&c->replacement,&origin,&c->services) ||
            !uss_replacement_begin(&c->replacement,life,r->thread,&r->history,&decision,&origin,&r->publication,&c->services,now))
            return automatic_stop(c,"REPLACEMENT_START_REFUSED");
        c->reason="REPLACEMENT_STARTED";return USS_CONTROL_WORK;
    }
    if(decision.kind==USS_PICKUP_MANAGED){
        UssReplacementOrigin origin={0};
        /* Idle managed pair only: no formation, replacement or recovery is active on this path. */
        if(c->io.zone_hygiene && bundle.current.valid && c->io.zone_hygiene(c->io.user,life,&bundle.current,now)){
            c->origin_valid=0;c->reason="ZONE_HYGIENE";return USS_CONTROL_WORK;}
        /* Bounded observation reuse only; replacement entry still checks fresh
         * mapping/context. Selection/lifetime changes bypass the100ms cadence. */
        if(c->origin_valid && now>=c->origin_ms && now-c->origin_ms<100 &&
            automatic_origin_matches(&c->origin,life,&bundle.current)){
            c->reason="MANAGED_PAIR";return USS_CONTROL_IDLE;}
        c->origin_valid=0;
        if(c->io.origin && c->io.origin(c->io.user,&origin) && automatic_origin_matches(&origin,life,&bundle.current)){
            c->origin=origin;c->origin_valid=1;c->origin_ms=now;}
        c->reason="MANAGED_PAIR";return USS_CONTROL_IDLE;
    }
    c->origin_valid=0;c->reason="PICKUP_WAIT";
    return decision.kind==USS_PICKUP_REMEMBERED?USS_CONTROL_IDLE:USS_CONTROL_WORK;
}
static inline int uss_automatic_init(UssAutomaticController* c,UssFormationRuntime* formation,
    const HfApi* api,HfMod* mod,const UssCorePlatform* platform,const UssAutomaticIo* io)
{
    if(!c || !formation || !uss_sdk_compatible(api) || !mod || !platform || !platform->thread_id || !io || !io->pickup_policy)return 0;
    memset(c,0,sizeof(*c));c->formation=formation;c->api=api;c->mod=mod;c->platform=*platform;c->io=*io;c->reason="WAITING";return 1;
}
#endif
