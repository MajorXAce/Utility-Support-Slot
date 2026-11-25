/* Normal-loop recovery routing. Idle observations only remember the actual real
 * member; the extra reader runs only for a failed Core/empty slot or active work. */
#ifndef USS_RECOVERY_RUNTIME_H
#define USS_RECOVERY_RUNTIME_H
#include "uss_recovery_native.h"
#include "uss_member_identity_source.h"
typedef struct UssRecoveryRuntime {
    UssRecoveryWork work;UssRecoveryNative native;UssAutomaticController* automatic;
    UssRecoveryWork wait_proof;
    UssIdentityCache identity;
    uint64_t watched_pair,wait_started;unsigned waiting;uint32_t real;const char* reason;
    /* Stored-member-lost confirmation: the entity under test, consecutive clean absences, first absence time. */
    uint32_t lost_entity;unsigned lost_count;uint64_t lost_since;
    const char* declined;unsigned declined_logs; /* last logged reason the stored-member check declined, and how many lines so far */
    unsigned stop_logs; /* lines written by the recovery-stop attribution below (log-only, capped) */
} UssRecoveryRuntime;
/* Software-only policy deferral, not effect admission. The exact existing
 * owner/member operation and actual native counter must still be untouched.
 * Fresh physical ownership/readiness and input are checked when policy allows
 * the scheduled recovery reader to run again. No guard is rebased or reset. */
static inline int recovery_pre_native_owned(UssRecoveryRuntime* recovery)
{
    UssRecoveryWork* work;UssFormationNative* native;UssFormationRuntime* runtime;UssPairOwner* owner;
    const UssPairMember* outgoing;const UssPairMember* survivor;
    if(!recovery || !(native=recovery->native.formation) || !(runtime=native->runtime) || !recovery->automatic ||
        recovery->automatic->formation!=runtime || recovery->automatic->stopped)return 0;
    work=recovery->waiting?&recovery->wait_proof:&recovery->work;owner=&runtime->life.owner;
    if((recovery->waiting?work->phase!=USS_RECOVERY_IDLE:work->phase!=USS_RECOVERY_ATTACH) || work->consumed || work->entry_barrier || !work->native_entries_known ||
        native->effects!=work->native_entries_start || runtime->busy || runtime->publication.busy ||
        (runtime->work.phase!=USS_PAIR_IDLE && runtime->work.phase!=USS_PAIR_COMPLETE) ||
        (recovery->automatic->replacement.phase!=USS_REPLACE_IDLE && recovery->automatic->replacement.phase!=USS_REPLACE_COMPLETE) ||
        runtime->life.phase!=USS_LIFE_FORMED || !pair_owner_ready(owner,owner->thread) ||
        !recovery_lifetime(work,owner) || runtime->thread!=owner->thread || !recovery->native.reader.platform.thread_id ||
        recovery->native.reader.platform.thread_id(recovery->native.reader.platform.user)!=owner->thread ||
        recovery->native.reader.api!=native->api || recovery->native.reader.mod!=native->mod ||
        recovery->native.reader.policy!=&runtime->policy || recovery->watched_pair!=owner->pair_generation ||
        !pair_member_equal(&work->owner.utility,&owner->utility) || !pair_member_equal(&work->owner.ordinary,&owner->ordinary) ||
        work->local_only!=owner->context.local_only)return 0;
    if(recovery->real==owner->utility.entity){outgoing=&owner->utility;survivor=&owner->ordinary;}
    else if(recovery->real==owner->ordinary.entity){outgoing=&owner->ordinary;survivor=&owner->utility;}
    else return 0;
    if(!pair_member_equal(&work->outgoing,outgoing) || !pair_member_equal(&work->survivor,survivor) ||
        (!work->local_only && !recovery_publication_owned(&runtime->publication,owner)))return 0;
    return 1;
}
static inline int uss_recovery_defer_pre_native(UssRecoveryRuntime* recovery)
{
    if(!recovery_pre_native_owned(recovery))return 0;
    if(recovery->waiting)recovery->wait_proof.policy_deferred=1;
    else recovery->work.policy_deferred=1;
    return 1;
}
/* Check the stored member only while the actual real member remains carried in
 * native slot2. Two coherent lookup absences separated by400ms may clear an
 * uncorroborated utility when ordinary support is real. Exact retained identity
 * vetoes that clearing without granting native use. If utility is real and its
 * ordinary partner cannot be corroborated, preserve the pair and stop handling,
 * matching the normal Lua role policy. Unknown reads never prove destruction. */
#define USS_STORED_LOST_MIN_MS 400u
enum { USS_STORED_LOST_NO=0,USS_STORED_LOST_WAIT,USS_STORED_LOST_CLEARED,USS_STORED_LOST_STOP,USS_STORED_LOST_RETAINED };
/* Why the check declined, logged when the reason changes (at most 16 lines per runtime), so a refusal is never silent.
 * Reasons are fixed strings; no entity data is logged. */
static inline unsigned recovery_stored_lost_decline(UssRecoveryRuntime* recovery,const char* why)
{
    if(recovery->declined!=why && recovery->declined_logs<16){
        recovery->declined=why;++recovery->declined_logs;
        if(recovery->native.reader.api && recovery->native.reader.api->log && recovery->native.reader.mod)
            recovery->native.reader.api->log(recovery->native.reader.mod,"USS stored-member check declined: %s",why);
    }
    return USS_STORED_LOST_NO;
}
static inline unsigned uss_recovery_stored_lost(UssRecoveryRuntime* recovery,uint64_t now)
{
    UssFormationRuntime* runtime=recovery->native.formation->runtime;UssPairOwner* owner=&runtime->life.owner;
    UssRecoveryWork candidate={0};UssRecoverySnapshot snapshot;const UssPairMember* real;const UssPairMember* stored;
    uint32_t stored_entity,real_entity;
    if(!owner->pair_present)return USS_STORED_LOST_NO;
    if(runtime->life.phase!=USS_LIFE_FORMED)return recovery_stored_lost_decline(recovery,"LIFECYCLE_NOT_FORMED");
    if(recovery->real==owner->utility.entity){real=&owner->utility;stored=&owner->ordinary;}
    else if(recovery->real==owner->ordinary.entity){real=&owner->ordinary;stored=&owner->utility;}
    else return recovery_stored_lost_decline(recovery,"REAL_MEMBER_UNKNOWN");
    {const UssCoreFlags* policy=recovery->native.reader.policy; /* the recovery observation refuses these states */
     if(!policy || !policy->known || !policy->eligible || policy->disabled || policy->suspended || policy->paused || policy->input){
        recovery->lost_count=0;return recovery_stored_lost_decline(recovery,"POLICY_NOT_READY_PAUSED_OR_INPUT");}}
    candidate.owner=*owner;candidate.survivor=*real;candidate.outgoing=*stored; /* roles: the REAL member survives, the STORED one is under test */
    if(!uss_recovery_observe(&recovery->native.reader,&candidate,&snapshot)){
        recovery->lost_count=0;return recovery_stored_lost_decline(recovery,"OBSERVATION_UNKNOWN");
    }
    if(snapshot.death){recovery->lost_count=0;return recovery_stored_lost_decline(recovery,"DEATH_CONTEXT");}
    if(!snapshot.outgoing_absent){recovery->lost_count=0;return recovery_stored_lost_decline(recovery,"STORED_MEMBER_PRESENT");}
    if(snapshot.inventory.slots[2]!=real->entity){recovery->lost_count=0;return recovery_stored_lost_decline(recovery,"REAL_NOT_IN_SUPPORT_SLOT");}
    if(snapshot.survivor.parent!=snapshot.inventory.avatar){recovery->lost_count=0;return recovery_stored_lost_decline(recovery,"REAL_NOT_CARRIED");}
    /* Qualified current absence may disagree with the retained identity
     * sources. Reuse this inventory; cached identity grants no native effect.
     * A retained result must not fall into the survivor-restoration preflight. */
    if(!identity_owner_matches(&candidate.owner,owner) ||
        (recovery->waiting && !recovery_pre_native_owned(recovery))){
        recovery->reason="STORED_LOSS_AUTHORITY_CHANGED";return USS_STORED_LOST_STOP;
    }
    {UssIdentityResult identity=uss_identity_query(&recovery->identity,recovery->native.reader.api,
        recovery->native.reader.mod,&recovery->native.reader.platform,owner,&snapshot.inventory,stored);
     if(!identity_owner_matches(&candidate.owner,owner)){
        recovery->reason="STORED_LOSS_AUTHORITY_CHANGED";return USS_STORED_LOST_STOP;
     }
     if(identity.status==USS_IDENTITY_PRESENT){
        recovery->lost_count=0;recovery->waiting=0;
        recovery->reason="STORED_IDENTITY_RETAINED";return USS_STORED_LOST_RETAINED;
     }}
    recovery->reason="STORED_ABSENCE_UNCORROBORATED";
    if(recovery->lost_entity!=stored->entity || !recovery->lost_count || now<recovery->lost_since){
        recovery->lost_entity=stored->entity;recovery->lost_count=1;recovery->lost_since=now;return USS_STORED_LOST_WAIT;
    }
    /* The first clean absence only starts the clock (it returned above); clearing needs a LATER clean pass. */
    if(now-recovery->lost_since<USS_STORED_LOST_MIN_MS)return USS_STORED_LOST_WAIT;
    if(real->entity==owner->utility.entity){
        recovery->reason="STORED_ENTITY_CHANGED";return USS_STORED_LOST_STOP;
    }
    stored_entity=stored->entity;real_entity=real->entity; /* clear_pair zeroes the owner record these point into */
    if(!uss_pair_owner_clear_pair(owner,owner->thread,owner->pair_generation)){
        recovery->reason="STORED_LOST_PAIR_CLEAR_FAILED";return USS_STORED_LOST_STOP;
    }
    runtime->life.phase=USS_LIFE_EMPTY;pickup_clear(&runtime->history);uss_publication_reset(&runtime->publication);
    uss_local_pair_invalidate(&runtime->local);
    recovery->automatic->origin_valid=0;recovery->real=0;recovery->watched_pair=0;recovery->waiting=0;recovery->lost_count=0;
    recovery->reason="STORED_MEMBER_LOST_PAIR_CLEARED";
    if(recovery->native.reader.api && recovery->native.reader.api->log && recovery->native.reader.mod)
        recovery->native.reader.api->log(recovery->native.reader.mod,
            "USS stored member %u uncorroborated after coherent lookup absence; pair cleared, real member %u stays in the support slot",
            (unsigned)stored_entity,(unsigned)real_entity);
    return USS_STORED_LOST_CLEARED;
}
static inline unsigned recovery_control_step(UssRecoveryRuntime* recovery,const UssCoreObservation* current,uint64_t now)
{
    UssFormationRuntime* runtime=recovery->native.formation->runtime;UssPairOwner* owner=&runtime->life.owner;
    UssRecoveryIo io=uss_recovery_native_io(&recovery->native);UssRecoveryWork candidate={0};UssRecoverySnapshot snapshot;
    if(current->valid && !current->mission){
        uss_identity_forget(&recovery->identity);recovery->work.phase=USS_RECOVERY_IDLE;recovery->real=0;recovery->watched_pair=0;recovery->reason="RECOVERY_SESSION_ENDED";return USS_CONTROL_IDLE;
    }
    if(recovery->work.phase!=USS_RECOVERY_IDLE && recovery->work.phase!=USS_RECOVERY_COMPLETE){
        if(recovery->work.phase==USS_RECOVERY_ATTACH && !recovery_pre_native_owned(recovery)){
            unsigned stopped=recovery_cancel(&recovery->work,"RECOVERY_DEFERRED_AUTHORITY_CHANGED");
            recovery->reason=recovery->work.reason;return stopped;
        }
        unsigned result=uss_recovery_tick(&recovery->work,&runtime->life,&runtime->publication,&runtime->history,&io,now);
        recovery->reason=recovery->work.reason;return result;
    }
    if(!owner->pair_present){uss_identity_forget(&recovery->identity);return USS_CONTROL_IDLE;}
    if(current->valid && current->pair_matched &&
        (current->inventory.slots[2]==owner->utility.entity || current->inventory.slots[2]==owner->ordinary.entity)){
        if(recovery->waiting && (current->inventory.slots[2]!=recovery->real || !recovery_pre_native_owned(recovery))){
            recovery->reason="RECOVERY_PREFLIGHT_REAL_CHANGED";return USS_CONTROL_STOP;
        }
        (void)uss_identity_seed(&recovery->identity,recovery->native.reader.api,recovery->native.reader.mod,
            &recovery->native.reader.platform,owner,current);
        recovery->waiting=0;recovery->lost_count=0;recovery->declined=NULL;recovery->watched_pair=owner->pair_generation;recovery->real=current->inventory.slots[2];return USS_CONTROL_IDLE;
    }
    if(!recovery->waiting && current->valid && current->inventory.slots[2]!=0 && publication_id(current->inventory.slots[0]) &&
        publication_id(current->inventory.slots[1]))return USS_CONTROL_IDLE;
    /* Replacement owns the intentional empty slot after its verified native
     * drop. Let its next stage perform the fresh handover/restore checks;
     * generic drop/use recovery must not pause or take over that transaction.
     * Unknown context and every other empty-slot interruption still stop. */
    {const UssReplacementWork* replacement=&recovery->automatic->replacement;
        if(replacement->phase==USS_REPLACE_ACTIVE && replacement->admitted &&
            (replacement->consumed_mask&8) &&
            (replacement->stage==USS_REPLACE_HANDOVER || replacement->stage==USS_REPLACE_RESTORE_NEW) &&
            current->valid && current->mission && current->inventory.slots[2]==0 &&
            lifecycle_current(&runtime->life,runtime->thread,current)){
            recovery->reason="REPLACEMENT_OWNS_EMPTY_SLOT";return USS_CONTROL_IDLE;
        }
    }
    if(recovery->automatic->replacement.phase==USS_REPLACE_ACTIVE || runtime->work.phase==USS_PAIR_ACTIVE){
        recovery->reason="RECOVERY_INTERRUPTED_NATIVE_WORK";return USS_CONTROL_STOP;
    }
    if(recovery->watched_pair!=owner->pair_generation ||
        (recovery->real!=owner->utility.entity && recovery->real!=owner->ordinary.entity)){
        recovery->reason="RECOVERY_LAST_REAL_UNKNOWN";return USS_CONTROL_STOP;
    }
    {unsigned lost=uss_recovery_stored_lost(recovery,now);
        if(lost==USS_STORED_LOST_STOP)return USS_CONTROL_STOP;
        if(lost!=USS_STORED_LOST_NO)return USS_CONTROL_WORK; /* confirming or just cleared: poll again soon */}
    if(!recovery->waiting){
        recovery->waiting=1;recovery->wait_started=now;
        memset(&recovery->wait_proof,0,sizeof(recovery->wait_proof));
        recovery->wait_proof.owner=*owner;recovery->wait_proof.local_only=owner->context.local_only;
        recovery->wait_proof.outgoing=recovery->real==owner->utility.entity?owner->utility:owner->ordinary;
        recovery->wait_proof.survivor=recovery->real==owner->utility.entity?owner->ordinary:owner->utility;
        if(io.entries){recovery->wait_proof.native_entries_start=io.entries(io.user);recovery->wait_proof.native_entries_known=1;}
    }
    /* Retain the first lifetime and actual entry baseline across scheduled
     * observations. Elapsed time is not evidence of destruction or release. */
    if(now<recovery->wait_started || !recovery_pre_native_owned(recovery)){
        recovery->reason="RECOVERY_PREFLIGHT_AUTHORITY_CHANGED";return USS_CONTROL_STOP;
    }
    candidate=recovery->wait_proof;
    if(!uss_recovery_observe(&recovery->native.reader,&candidate,&snapshot)){
        if(snapshot.refusal!=USS_RECOVERY_REFUSAL_TRANSIENT){
            recovery->reason="RECOVERY_PREFLIGHT_LIFETIME_REFUSED";return USS_CONTROL_STOP;
        }
        recovery->reason="RECOVERY_OBSERVATION_UNKNOWN";return USS_CONTROL_WORK;
    }
    if(!snapshot.death && snapshot.inventory.slots[2]!=0){
        /* A transient Core read can initiate preflight while the original real
         * member is still in its native slot. Its coherent return needs no
         * restoration and must not be mistaken for another pickup. */
        if(snapshot.inventory.slots[2]==recovery->real && !snapshot.outgoing_absent &&
            snapshot.outgoing.parent==snapshot.inventory.avatar && snapshot.survivor.parent==snapshot.inventory.avatar){
            recovery->waiting=0;recovery->reason="RECOVERY_NATIVE_REAL_PRESENT";return USS_CONTROL_WORK;
        }
        recovery->reason="RECOVERY_PREFLIGHT_SUPPORT_CHANGED";return USS_CONTROL_STOP;
    }
    if(!snapshot.death && (snapshot.survivor.parent!=snapshot.inventory.avatar ||
        (!snapshot.outgoing_absent && snapshot.outgoing.parent!=0 && snapshot.outgoing.parent!=snapshot.inventory.avatar))){
        recovery->reason="RECOVERY_PREFLIGHT_PARENT_CHANGED";return USS_CONTROL_STOP;
    }
    if(!snapshot.death && !snapshot.outgoing_absent && snapshot.outgoing.parent==snapshot.inventory.avatar){
        recovery->reason="WAIT_NATIVE_DROP_RELEASE";return USS_CONTROL_WORK;
    }
    if(!(owner->context.local_only?
        uss_recovery_begin_local(&recovery->work,owner,&runtime->local,recovery->real,&snapshot,now):
        uss_recovery_begin(&recovery->work,owner,&runtime->publication,recovery->real,&snapshot,now))){
        recovery->reason="RECOVERY_BEGIN_REFUSED";return USS_CONTROL_STOP;
    }
    recovery->work.native_entries_start=candidate.native_entries_start;
    recovery->work.native_entries_known=candidate.native_entries_known;
    /* All extended waits require the same neutral-input and settled-selection
     * admission as a policy deferral, before a first native effect. */
    recovery->work.policy_deferred=1;recovery->waiting=0;
    uss_local_pair_invalidate(&runtime->local);
    recovery->automatic->origin_valid=0;recovery->reason=recovery->work.reason;
    return USS_CONTROL_WORK;
}
static inline const char* recovery_phase_text(UssRecoveryPhase phase)
{
    switch(phase){case USS_RECOVERY_IDLE:return "IDLE";case USS_RECOVERY_ATTACH:return "ATTACH";case USS_RECOVERY_PRIMARY:return "PRIMARY";
        case USS_RECOVERY_SEND:return "SEND";case USS_RECOVERY_SELECTION:return "SELECTION";case USS_RECOVERY_DEATH_WAIT:return "DEATH_WAIT";
        case USS_RECOVERY_COMPLETE:return "COMPLETE";case USS_RECOVERY_CANCELLED:return "CANCELLED";}
    return "UNKNOWN";
}
/* Recovery STOP is terminal for this actor's handling (the loop stays paused), so it must never be silent. This wrapper only
 * reads the work record the stop already filled in: it adds no game read and changes no decision. The first lines per
 * session (capped) name the reason, the phase, which native effects had been consumed (bit n = effect n) and how long the
 * recovery had been running, which is what separates "gave up before touching anything" from "an effect was already entered". */
static inline unsigned uss_recovery_control(UssRecoveryRuntime* recovery,const UssCoreObservation* current,uint64_t now)
{
    unsigned result=recovery_control_step(recovery,current,now);
    if(result==USS_CONTROL_STOP && recovery->stop_logs<4 && recovery->native.reader.api && recovery->native.reader.api->log &&
        recovery->native.reader.mod){
        const UssRecoveryWork* work=&recovery->work;
        uint64_t since=work->phase==USS_RECOVERY_IDLE?recovery->wait_started:work->started;
        ++recovery->stop_logs;
        recovery->native.reader.api->log(recovery->native.reader.mod,
            "USS recovery stopped: reason=%s phase=%s consumed=0x%x elapsed_ms=%llu detail=%s",
            recovery->reason?recovery->reason:"NONE",recovery_phase_text(work->phase),work->consumed,
            (unsigned long long)(now>=since?now-since:0),
            recovery->native.formation && recovery->native.formation->reason?recovery->native.formation->reason:"NONE");
    }
    return result;
}

/* Explicit read-only diagnostic entry. It obtains the existing qualified recovery
 * snapshot; it neither runs per frame nor changes any stored-loss decision. */
static inline UssIdentityResult uss_recovery_identity_probe(UssRecoveryRuntime* recovery,const UssPairMember* member)
{
    UssIdentityResult result={0};UssRecoveryWork request={0};UssRecoverySnapshot snapshot;UssPairOwner* owner;
    if(!recovery || !recovery->native.formation || !recovery->native.formation->runtime)return result;
    owner=&recovery->native.formation->runtime->life.owner;
    if(!owner->pair_present){uss_identity_forget(&recovery->identity);return result;}
    if(!recovery->identity.valid || !member || (!pair_member_equal(member,&owner->utility) &&
        !pair_member_equal(member,&owner->ordinary)))return result;
    request.owner=*owner;
    if(recovery->real==owner->utility.entity){request.survivor=owner->utility;request.outgoing=owner->ordinary;}
    else if(recovery->real==owner->ordinary.entity){request.survivor=owner->ordinary;request.outgoing=owner->utility;}
    else return result;
    if(!uss_recovery_observe(&recovery->native.reader,&request,&snapshot) || snapshot.death)return result;
    return uss_identity_query(&recovery->identity,recovery->native.reader.api,recovery->native.reader.mod,
        &recovery->native.reader.platform,owner,&snapshot.inventory,member);
}

#endif
