/* Normal-loop recovery routing. Idle observations only remember the actual real
 * member; the extra reader runs only for a failed Core/empty slot or active work. */
#ifndef USS_RECOVERY_RUNTIME_H
#define USS_RECOVERY_RUNTIME_H
#include "uss_recovery_native.h"
typedef struct UssRecoveryRuntime {
    UssRecoveryWork work;UssRecoveryNative native;UssAutomaticController* automatic;
    uint64_t watched_pair,wait_started;unsigned waiting;uint32_t real;const char* reason;
} UssRecoveryRuntime;
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
        recovery->waiting=0;recovery->watched_pair=owner->pair_generation;recovery->real=current->inventory.slots[2];return USS_CONTROL_IDLE;
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
