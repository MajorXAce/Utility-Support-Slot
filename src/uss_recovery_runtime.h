/* Normal-loop recovery routing. Idle observations only remember the actual real
 * member; the extra reader runs only for a failed Core/empty slot or active work. */
#ifndef USS_RECOVERY_RUNTIME_H
#define USS_RECOVERY_RUNTIME_H
#include "uss_recovery_native.h"
typedef struct UssRecoveryRuntime {
    UssRecoveryWork work;UssRecoveryNative native;UssAutomaticController* automatic;
    uint64_t watched_pair,wait_started;unsigned waiting;uint32_t real;const char* reason;
    /* Stored-member-lost confirmation: the entity under test, consecutive clean absences, first absence time. */
    uint32_t lost_entity;unsigned lost_count;uint64_t lost_since;
    const char* declined;unsigned declined_logs; /* last logged reason the stored-member check declined, and how many lines so far */
} UssRecoveryRuntime;
/* The STORED member of an idle pair vanished from the game (for example the Solo Silo was destroyed and the game deleted its
 * remote). Observed live: the weapon-manager row and the WeaponLinker entry of the stored remote were gone while the real
 * member stayed healthy in slot 2; the pair still named the deleted entity, so ordinary observation failed and recovery
 * (which assumes the REAL member left) timed out and stopped, leaving a pair that could never be used until death.
 * Here the pair is cleared with NO native effect, only when the stored member is CLEANLY absent (the weapon lookup reached an
 * empty bucket, never a failed read) in two coherent snapshots (the second at least USS_STORED_LOST_MIN_MS after the first), and the real member is
 * still present in slot 2 and carried by the local avatar. Death, an active replacement/formation, a failed or changing read,
 * or a missing real member never take this path. */
#define USS_STORED_LOST_MIN_MS 400u
enum { USS_STORED_LOST_NO=0,USS_STORED_LOST_WAIT,USS_STORED_LOST_CLEARED,USS_STORED_LOST_STOP };
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
    if(recovery->lost_entity!=stored->entity || !recovery->lost_count || now<recovery->lost_since){
        recovery->lost_entity=stored->entity;recovery->lost_count=1;recovery->lost_since=now;return USS_STORED_LOST_WAIT;
    }
    /* The first clean absence only starts the clock (it returned above); clearing needs a LATER clean pass. */
    if(now-recovery->lost_since<USS_STORED_LOST_MIN_MS)return USS_STORED_LOST_WAIT;
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
            "USS stored member %u no longer exists; pair cleared, real member %u stays in the support slot",
            (unsigned)stored_entity,(unsigned)real_entity);
    return USS_STORED_LOST_CLEARED;
}
static inline unsigned uss_recovery_control(UssRecoveryRuntime* recovery,const UssCoreObservation* current,uint64_t now)
{
    UssFormationRuntime* runtime=recovery->native.formation->runtime;UssPairOwner* owner=&runtime->life.owner;
    UssRecoveryIo io=uss_recovery_native_io(&recovery->native);UssRecoveryWork candidate={0};UssRecoverySnapshot snapshot;
    if(current->valid && !current->mission){
        recovery->work.phase=USS_RECOVERY_IDLE;recovery->real=0;recovery->watched_pair=0;recovery->reason="RECOVERY_SESSION_ENDED";return USS_CONTROL_IDLE;
    }
    if(recovery->work.phase!=USS_RECOVERY_IDLE && recovery->work.phase!=USS_RECOVERY_COMPLETE){
        unsigned result=uss_recovery_tick(&recovery->work,&runtime->life,&runtime->publication,&runtime->history,&io,now);
        recovery->reason=recovery->work.reason;return result;
    }
    if(!owner->pair_present)return USS_CONTROL_IDLE;
    if(current->valid && current->pair_matched &&
        (current->inventory.slots[2]==owner->utility.entity || current->inventory.slots[2]==owner->ordinary.entity)){
        recovery->waiting=0;recovery->lost_count=0;recovery->declined=NULL;recovery->watched_pair=owner->pair_generation;recovery->real=current->inventory.slots[2];return USS_CONTROL_IDLE;
    }
    if(current->valid && current->inventory.slots[2]!=0 && publication_id(current->inventory.slots[0]) &&
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
    if(!recovery->waiting){recovery->waiting=1;recovery->wait_started=now;}
    if(now<recovery->wait_started || now-recovery->wait_started>=2000){
        recovery->reason="RECOVERY_PREFLIGHT_TIMEOUT";return USS_CONTROL_STOP;
    }
    candidate.owner=*owner;candidate.outgoing=recovery->real==owner->utility.entity?owner->utility:owner->ordinary;
    candidate.survivor=recovery->real==owner->utility.entity?owner->ordinary:owner->utility;
    if(!uss_recovery_observe(&recovery->native.reader,&candidate,&snapshot)){
        /* A failed normal Core observation can be unrelated to recovery. It is
         * still unknown: retain membership, never infer death/consumption. */
        recovery->reason="RECOVERY_OBSERVATION_UNKNOWN";return USS_CONTROL_WORK;
    }
    if(!snapshot.death && snapshot.inventory.slots[2]!=0)return USS_CONTROL_IDLE;
    if(!snapshot.death && !snapshot.outgoing_absent && snapshot.outgoing.parent==snapshot.inventory.avatar){
        recovery->reason="WAIT_NATIVE_DROP_RELEASE";
        return now-recovery->wait_started<500?USS_CONTROL_WORK:USS_CONTROL_STOP;
    }
    recovery->waiting=0;
    if(!(owner->context.local_only?
        uss_recovery_begin_local(&recovery->work,owner,&runtime->local,recovery->real,&snapshot,now):
        uss_recovery_begin(&recovery->work,owner,&runtime->publication,recovery->real,&snapshot,now))){
        recovery->reason="RECOVERY_BEGIN_REFUSED";return USS_CONTROL_STOP;
    }
    uss_local_pair_invalidate(&runtime->local);
    recovery->automatic->origin_valid=0;recovery->reason=recovery->work.reason;
    return USS_CONTROL_WORK;
}
#endif
