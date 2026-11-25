/* Drop/use/death recovery with qualified pre-entry waits. This owns an operation record from an actual
 * published pair; it never fabricates a pair after the native slot is emptied. */
#ifndef USS_RECOVERY_H
#define USS_RECOVERY_H
#include "uss_automatic_controller.h"

typedef enum UssRecoveryPhase {
    USS_RECOVERY_IDLE=0,USS_RECOVERY_ATTACH,USS_RECOVERY_PRIMARY,USS_RECOVERY_SEND,
    USS_RECOVERY_SELECTION,USS_RECOVERY_DEATH_WAIT,USS_RECOVERY_COMPLETE,USS_RECOVERY_CANCELLED
} UssRecoveryPhase;
typedef enum UssRecoveryEffect {
    USS_RECOVERY_RESTORE=1,USS_RECOVERY_SELECT_PRIMARY,USS_RECOVERY_PUBLISH,
    USS_RECOVERY_SELECT_FINAL,USS_RECOVERY_DROP_STORED
} UssRecoveryEffect;
/* Why an observation was refused, when the reader knows. Set ONLY on a refused (invalid) snapshot, from facts it had already
 * established; a valid snapshot keeps 0, so snapshot comparisons are unchanged. TRANSIENT means "could not read or coherently
 * confirm" (the only kind a not-yet-effected recovery may wait out); the others are definitive and are never waited out. */
enum { USS_RECOVERY_REFUSAL_TRANSIENT=0,USS_RECOVERY_REFUSAL_SURVIVOR_ABSENT,USS_RECOVERY_REFUSAL_CONTEXT_CHANGED,USS_RECOVERY_REFUSAL_CONTRACT };
/* Unknown-history callers retain the finite legacy wait. The actual runtime may
 * suspend before entry while its saved counter and ownership remain unchanged;
 * every later phase keeps its own finite window after the restore. */
enum { USS_RECOVERY_RESTORE_WAIT_MS=10000,USS_RECOVERY_PHASE_WINDOW_MS=2000 };
typedef struct UssRecoverySnapshot {
    unsigned valid,death,all_empty,outgoing_absent;Inventory inventory;
    UssMemberEvidence survivor,outgoing;UssPrimaryReadiness primary;
    unsigned refusal;
} UssRecoverySnapshot;
typedef struct UssRecoveryWork {
    UssRecoveryPhase phase;UssPairOwner owner;UssPairMember survivor,outgoing;
    uint32_t selected;unsigned consumed,local_only;uint64_t started,deadline;
    const char* reason;
    /* Set by recovery_effect when the restore was refused before ANY native entry (and its consumed bit was given back), and the
     * earliest tick at which the pre-effect wait may try the restore again. */
    unsigned refused_before_entry;uint64_t retry_after;
    /* Actual runtime counter at begin. Unknown history never authorizes policy
     * deferral. The barrier is set before a service call and cleared only after
     * a failed first restore proves that no native entry occurred. */
    unsigned native_entries_known,native_entries_start,entry_barrier,policy_deferred;
} UssRecoveryWork;
typedef struct UssRecoveryIo {
    void* user;int (*observe)(void*,const UssRecoveryWork*,UssRecoverySnapshot*);
    /* Return1 only after returned effect AND fresh verified readback. Never WAIT.
     * Callback must revalidate the supplied snapshot/entry at its own boundary. */
    int (*effect)(void*,const UssRecoveryWork*,const UssRecoverySnapshot*,UssRecoveryEffect,uint32_t);
    /* Optional: the service's cumulative count of native calls it has ENTERED. If it did not move across a refused restore, no
     * native call happened and nothing is uncertain. Without it every refusal stays consumed, exactly as before. */
    unsigned (*entries)(void*);
} UssRecoveryIo;
enum { USS_RECOVERY_RETRY_MS=100 };
static inline unsigned recovery_cancel(UssRecoveryWork* work,const char* reason)
{work->phase=USS_RECOVERY_CANCELLED;work->reason=reason;return USS_CONTROL_STOP;}
/* Structural pre-entry state. Runtime ownership and the actual saved entry
 * counter are checked separately before extending a wait or admitting a call. */
static inline int recovery_pre_effect_wait(const UssRecoveryWork* work)
{return work->phase==USS_RECOVERY_ATTACH && !work->entry_barrier && !(work->consumed&(1u<<(unsigned)USS_RECOVERY_RESTORE));}
static inline uint64_t recovery_deadline(const UssRecoveryWork* work)
{return recovery_pre_effect_wait(work)?work->started+USS_RECOVERY_RESTORE_WAIT_MS:work->deadline;}
static inline int recovery_lifetime(const UssRecoveryWork* work,const UssPairOwner* owner)
{
    return owner && !owner->disabled && owner->thread==work->owner.thread && owner->epoch==work->owner.epoch &&
        owner->context_generation==work->owner.context_generation && owner->pair_generation==work->owner.pair_generation &&
        owner->guard_generation==work->owner.guard_generation && pair_owner_context_equal(&owner->context,&work->owner.context);
}
static inline int recovery_effect(UssRecoveryWork* work,const UssRecoveryIo* io,const UssRecoverySnapshot* snapshot,
    UssRecoveryEffect effect,uint32_t slot)
{
    unsigned bit=1u<<(unsigned)effect,before=io->entries?io->entries(io->user):0,prior_barrier=work->entry_barrier;int result;
    if(effect==USS_RECOVERY_RESTORE && (work->entry_barrier ||
        (work->native_entries_known && (!io->entries || before!=work->native_entries_start)))){
        work->refused_before_entry=0;return 0;
    }
    if((work->consumed&bit) || !io->effect)return 0;
    work->consumed|=bit;work->refused_before_entry=0;work->entry_barrier=1;
    result=io->effect(io->user,work,snapshot,effect,slot);
    /* Only the restore, and only while recovery is still before its first effect: a refusal that entered no native call is not
     * an attempted restore (the same rule the readiness wait already follows), so the bit is given back. */
    if(!result && effect==USS_RECOVERY_RESTORE && io->entries && !prior_barrier && work->native_entries_known &&
        before==work->native_entries_start && io->entries(io->user)==before){
        work->consumed&=~bit;work->entry_barrier=0;work->refused_before_entry=1;}
    return result;
}
static inline int recovery_publication_owned(const UssPublicationState* publication,const UssPairOwner* owner)
{
    UssPublicationKey key={0};
    if(!publication || !owner || owner->context.local_only || publication->disabled || publication->suspended || !publication->record_present || !publication->formed)return 0;
    key.pair_generation=owner->pair_generation;key.context_generation=owner->context_generation;
    key.owner=owner->context.local.owner;key.inventory=owner->context.local.inventory;key.avatar=owner->context.local.avatar;
    key.session=owner->context.session;key.mapping=owner->context.mapping;key.utility=owner->utility.entity;key.ordinary=owner->ordinary.entity;
    memcpy(key.utility_identity,owner->utility.identity,8);memcpy(key.ordinary_identity,owner->ordinary.identity,8);
    key.peer_size=owner->context.peer_size;memcpy(key.peers,owner->context.peers,key.peer_size);
    return publication_key_equal(&publication->record,&key);
}
static inline int recovery_begin(UssRecoveryWork* work,const UssPairOwner* owner,
    const UssPublicationState* publication,const UssLocalPairState* local,uint32_t real,const UssRecoverySnapshot* snapshot,uint64_t now)
{
    UssRecoveryWork result={0};
    if(!work || !pair_owner_ready(owner,owner?owner->thread:0) || !snapshot || !snapshot->valid ||
        !(owner->context.local_only?uss_local_pair_owned(local,owner):recovery_publication_owned(publication,owner)) ||
        (real!=owner->utility.entity && real!=owner->ordinary.entity) || now>UINT64_MAX-USS_RECOVERY_RESTORE_WAIT_MS)return 0;
    result.local_only=owner->context.local_only;result.owner=*owner;result.outgoing=real==owner->utility.entity?owner->utility:owner->ordinary;
    result.survivor=real==owner->utility.entity?owner->ordinary:owner->utility;
    result.started=now;result.deadline=now+2000;result.selected=snapshot->inventory.active_selector;
    result.phase=snapshot->death?USS_RECOVERY_DEATH_WAIT:USS_RECOVERY_ATTACH;
    result.reason="RECOVERY_STARTED";*work=result;return 1;
}
static inline int uss_recovery_begin(UssRecoveryWork* work,const UssPairOwner* owner,
    const UssPublicationState* publication,uint32_t real,const UssRecoverySnapshot* snapshot,uint64_t now)
{return recovery_begin(work,owner,publication,NULL,real,snapshot,now);}
static inline int uss_recovery_begin_local(UssRecoveryWork* work,const UssPairOwner* owner,
    const UssLocalPairState* local,uint32_t real,const UssRecoverySnapshot* snapshot,uint64_t now)
{return owner && owner->context.local_only && recovery_begin(work,owner,NULL,local,real,snapshot,now);}
static inline unsigned uss_recovery_tick(UssRecoveryWork* work,UssCoreLifecycle* life,
    UssPublicationState* publication,UssPickupHistory* history,const UssRecoveryIo* io,uint64_t now)
{
    UssRecoverySnapshot snapshot={0};
    if(!work || !life || !publication || !history || !io || !io->observe)return USS_CONTROL_STOP;
    if(work->phase==USS_RECOVERY_IDLE || work->phase==USS_RECOVERY_COMPLETE)return USS_CONTROL_IDLE;
    if(work->phase==USS_RECOVERY_CANCELLED)return USS_CONTROL_STOP;
    if(now<work->started || !recovery_lifetime(work,&life->owner))return recovery_cancel(work,"RECOVERY_LIFETIME_CHANGED");
    if(work->phase==USS_RECOVERY_ATTACH && work->native_entries_known &&
        (!io->entries || io->entries(io->user)!=work->native_entries_start || work->entry_barrier || work->consumed))
        return recovery_cancel(work,"RECOVERY_ENTRY_HISTORY_CHANGED");
    if(!(recovery_pre_effect_wait(work) && work->native_entries_known) && now>=recovery_deadline(work))
        return recovery_cancel(work,recovery_pre_effect_wait(work)?"RECOVERY_RESTORE_WAIT_EXPIRED":"RECOVERY_PHASE_TIMEOUT");
    if(!io->observe(io->user,work,&snapshot) || !snapshot.valid){
        /* Before any effect a transient read failure is a wait, not a verdict. A definitive refusal is never waited out. */
        if(recovery_pre_effect_wait(work) && snapshot.refusal==USS_RECOVERY_REFUSAL_TRANSIENT){work->reason="WAIT_RECOVERY_READ_UNKNOWN";return USS_CONTROL_WORK;}
        return recovery_cancel(work,snapshot.refusal==USS_RECOVERY_REFUSAL_SURVIVOR_ABSENT?"RECOVERY_SURVIVOR_GONE":
            snapshot.refusal==USS_RECOVERY_REFUSAL_CONTEXT_CHANGED?"RECOVERY_CONTEXT_CHANGED":
            snapshot.refusal==USS_RECOVERY_REFUSAL_CONTRACT?"RECOVERY_READ_CONTRACT":"RECOVERY_READ_UNKNOWN");
    }
    if(work->phase==USS_RECOVERY_DEATH_WAIT){
        if(!snapshot.death)return recovery_cancel(work,"DEATH_CONTEXT_CHANGED");
        if(!snapshot.all_empty){
            if(now-work->started<500){work->reason="WAIT_NATIVE_DEATH_EMPTY";return USS_CONTROL_WORK;}
            work->reason="DEATH_NATIVE_DID_NOT_EMPTY";
        }else{
            if(snapshot.outgoing_absent || snapshot.outgoing.parent!=0 || snapshot.survivor.parent!=snapshot.inventory.avatar)
                return recovery_cancel(work,"DEATH_RELEASE_PARENT_UNKNOWN");
            if(!recovery_effect(work,io,&snapshot,USS_RECOVERY_DROP_STORED,0))return recovery_cancel(work,"DEATH_DROP_FAILED");
            work->reason="DEATH_STORED_RELEASED";
        }
        if(!uss_pair_owner_clear_pair(&life->owner,life->owner.thread,work->owner.pair_generation))
            return recovery_cancel(work,"DEATH_PAIR_CLEAR_FAILED");
        life->phase=USS_LIFE_EMPTY;pickup_clear(history);uss_publication_reset(publication);
        work->phase=USS_RECOVERY_COMPLETE;return USS_CONTROL_WORK;
    }
    if(snapshot.death || snapshot.survivor.parent!=snapshot.inventory.avatar ||
        (!snapshot.outgoing_absent && snapshot.outgoing.parent!=0))return recovery_cancel(work,"SURVIVOR_OR_OUTGOING_CHANGED");
    if(work->phase==USS_RECOVERY_ATTACH){
        if(snapshot.inventory.slots[2]!=0)return recovery_cancel(work,"RECOVERY_EMPTY_SELECTION_CHANGED");
        /* Vanilla may not have left the emptied support slot yet (a knockdown can delay it). Nothing is selected or attached by
         * us until it does; wait for it, as Lua does, while the pre-entry proof remains current. */
        if(snapshot.inventory.active_selector!=1 && snapshot.inventory.active_selector!=2){work->reason="WAIT_RECOVERY_SELECTION";return USS_CONTROL_WORK;}
        /* A transient native selection block is not an attempted restore.
         * Observe again at the scheduled cadence, before consuming effects. */
        if(snapshot.primary.valid && (snapshot.primary.fields.global_guard ||
            snapshot.primary.fields.state_byte || u32(snapshot.primary.fields.row+28))){
            work->reason="WAIT_RECOVERY_RESTORE_READY";return USS_CONTROL_WORK;
        }
        /* The current coherent choice supplies the final return selection.
         * This is not a lifetime generation: it may change before first entry.
         * Later phases keep this value once the restore service has entered. */
        work->selected=snapshot.inventory.active_selector;
        if(now<work->retry_after){work->reason="WAIT_RECOVERY_RESTORE_RETRY";return USS_CONTROL_WORK;}
        if(!recovery_effect(work,io,&snapshot,USS_RECOVERY_RESTORE,0)){
            if(work->refused_before_entry){ /* nothing was entered: wait and try again, at a bounded rate while the pre-entry proof remains current */
                work->refused_before_entry=0;work->retry_after=now+USS_RECOVERY_RETRY_MS;
                work->reason="WAIT_RECOVERY_RESTORE_REFUSED_BEFORE_ENTRY";return USS_CONTROL_WORK;}
            return recovery_cancel(work,"SURVIVOR_RESTORE_FAILED");
        }
        if(!uss_pair_owner_clear_pair(&life->owner,life->owner.thread,work->owner.pair_generation))
            return recovery_cancel(work,"RECOVERY_PAIR_CLEAR_FAILED");
        life->phase=USS_LIFE_EMPTY;pickup_clear(history);work->phase=USS_RECOVERY_PRIMARY;
        /* Each later phase gets its own window from the consumed restore, however long the pre-effect wait lasted. */
        work->deadline=now+USS_RECOVERY_PHASE_WINDOW_MS;
        work->reason="SURVIVOR_RESTORED_PAIR_CLEARED";return USS_CONTROL_WORK;
    }
    if(snapshot.inventory.slots[2]!=work->survivor.entity)return recovery_cancel(work,"SURVIVOR_SLOT_CHANGED");
    if(work->phase==USS_RECOVERY_PRIMARY){
        if(snapshot.inventory.active_selector!=1 && snapshot.inventory.active_selector!=work->selected)return recovery_cancel(work,"RECOVERY_PLAYER_SELECTION_CHANGED");
        if(snapshot.inventory.active_selector!=1 && !recovery_effect(work,io,&snapshot,USS_RECOVERY_SELECT_PRIMARY,1))
            return recovery_cancel(work,"RECOVERY_PRIMARY_FAILED");
        work->phase=USS_RECOVERY_SEND;return USS_CONTROL_WORK;
    }
    if(snapshot.inventory.active_selector!=1)
        return recovery_cancel(work,"RECOVERY_PRIMARY_CHANGED");
    if(work->phase==USS_RECOVERY_SEND){
        if(!snapshot.primary.valid)return recovery_cancel(work,"RECOVERY_PRIMARY_UNKNOWN");
        if(!snapshot.primary.ready){work->reason="WAIT_RECOVERY_PRIMARY";return USS_CONTROL_WORK;}
        if(!work->local_only && !recovery_effect(work,io,&snapshot,USS_RECOVERY_PUBLISH,0))return recovery_cancel(work,"LONE_PUBLICATION_FAILED");
        work->phase=USS_RECOVERY_SELECTION;return USS_CONTROL_WORK;
    }
    if(work->phase!=USS_RECOVERY_SELECTION || snapshot.inventory.held!=snapshot.inventory.slots[0])return recovery_cancel(work,"RECOVERY_PHASE_OR_HELD_INVALID");
    if(work->selected!=1 && !recovery_effect(work,io,&snapshot,USS_RECOVERY_SELECT_FINAL,work->selected))
        return recovery_cancel(work,"RECOVERY_SELECTION_FAILED");
    uss_publication_reset(publication);work->phase=USS_RECOVERY_COMPLETE;work->reason="LONE_RECOVERY_COMPLETE";
    return USS_CONTROL_WORK;
}
#endif
