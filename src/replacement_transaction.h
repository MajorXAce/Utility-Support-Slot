/* Finite typed replacement, translated from the existing Lua native transaction.
 * All effects are abstract services. No native address or production callback. */
#ifndef USS_REPLACEMENT_TRANSACTION_H
#define USS_REPLACEMENT_TRANSACTION_H
#include "pickup_history.h"
#include "local_pair_state.h"
typedef enum UssReplacementPhase {USS_REPLACE_IDLE=0,USS_REPLACE_ACTIVE,USS_REPLACE_COMPLETE,USS_REPLACE_CANCELLED} UssReplacementPhase;
typedef enum UssReplacementStage {
    USS_REPLACE_PREPARE=0,USS_REPLACE_RECOVER,USS_REPLACE_REPRESENT,USS_REPLACE_DROP,
    USS_REPLACE_HANDOVER,USS_REPLACE_RESTORE_NEW,USS_REPLACE_PROMOTE,
    USS_REPLACE_RETAINED_START,USS_REPLACE_RETAINED_TICK,USS_REPLACE_SELECT_FINAL,USS_REPLACE_VERIFY
} UssReplacementStage;
typedef struct UssReplacementOrigin {
    unsigned known,utility_drawn;
    uint32_t real,selected;
    uint64_t epoch,context_generation,pair_generation,guard_generation;
} UssReplacementOrigin;
typedef struct UssReplacementSnapshot {
    /* Explicit order: incoming, retained opposite class, released same class.
     * All three and current pair must come from ONE coherent reader interval. */
    UssCoreCandidates bundle;
    unsigned empty_valid;
    uint32_t native_empty; /* Fresh game empty-slot sentinel; not guessed by Core. */
} UssReplacementSnapshot;
typedef struct UssReplacementWork {
    UssReplacementPhase phase;UssReplacementStage stage;
    UssPickupDecision intent;UssReplacementOrigin origin;
    UssPairMember old_utility,old_ordinary;
    uint64_t started_ms,deadline_ms,primary_deadline,next_ms,last_tick,new_generation;
    unsigned has_tick,primary_requested,consumed_mask,admitted,local_only;
    uint32_t real,selected;unsigned utility_drawn;
    UssPairTransaction retained;
    const char* reason;
} UssReplacementWork;
typedef struct UssReplacementServices {
    void* user;
    UssReplacementSnapshot (*observe)(void*,const UssReplacementWork*);
    UssPairServiceResult (*select_slot)(void*,const UssReplacementSnapshot*,uint32_t);
    UssPairServiceResult (*primary_ready)(void*,const UssReplacementWork*,const UssReplacementSnapshot*,uint64_t);
    /* Complete guarded switch_entity: fresh context/pins, one attachment plus
     * required publication, post-read. STOP on uncertainty, never WAIT/retry. */
    UssPairServiceResult (*switch_member)(void*,const UssReplacementSnapshot*,const UssPairMember*,UssPublicationStage);
    /* Two matching fresh native-drop control captures, mapping/peer/policy/pins,
     * consume before native local slot drop and verify detached-old/empty slot.
     * This pure coordinator does NOT substitute the inventory sender for drop. */
    UssPairServiceResult (*drop_slot)(void*,const UssReplacementSnapshot*);
    UssLocalPairState* local; /* NULL for the network adapter. */
    UssPairServices retained; /* Reuse shared formation/draw implementation. */
} UssReplacementServices;
static int replacement_services_valid(const UssReplacementServices* s)
{return s && s->observe && s->select_slot && s->primary_ready && s->switch_member && s->drop_slot && pair_services_valid(&s->retained);}
static int replacement_record_matches(const UssCoreLifecycle* life,const UssPublicationState* publication)
{
    UssPublicationKey key={0};const UssPairOwner* owner;
    if(!life || !publication || life->owner.context.local_only || !life->owner.context.peer_size || life->owner.context.peer_size>USS_PUBLICATION_PEER_BYTES)return 0;
    owner=&life->owner;
    key.pair_generation=owner->pair_generation;key.context_generation=owner->context_generation;
    key.session=owner->context.session;key.mapping=owner->context.mapping;key.owner=owner->context.local.owner;
    key.inventory=owner->context.local.inventory;key.avatar=owner->context.local.avatar;
    key.utility=owner->utility.entity;key.ordinary=owner->ordinary.entity;
    memcpy(key.utility_identity,owner->utility.identity,8);memcpy(key.ordinary_identity,owner->ordinary.identity,8);
    key.peer_size=owner->context.peer_size;memcpy(key.peers,owner->context.peers,key.peer_size);
    return publication->formed && publication->record_present && publication_key_equal(&key,&publication->record);
}
static inline int replacement_owned(const UssCoreLifecycle* life,const UssPublicationState* publication,
    const UssReplacementServices* services,uint32_t member)
{
    if(life->owner.context.local_only)return uss_local_pair_owned(services->local,&life->owner) &&
        (!member || services->local->member==member);
    return replacement_record_matches(life,publication) && (!member || publication->member==member);
}
static void replacement_cancel(UssReplacementWork* work,UssCoreLifecycle* life,const char* reason)
{
    uint64_t generation=work->new_generation?work->new_generation:work->intent.pair_generation;
    work->phase=USS_REPLACE_CANCELLED;work->reason=reason;
    /* Do not pause a different/new mission or pair on behalf of stale work. */
    if(life && life->owner.pair_present && life->owner.epoch==work->intent.epoch && life->owner.pair_generation==generation &&
        pair_owner_context_equal(&life->owner.context,&work->intent.context)){
        life->phase=USS_LIFE_PAUSED;pair_owner_interrupt(&life->owner);
    }
}
static int replacement_read(const UssReplacementServices* s,UssCoreLifecycle* life,unsigned thread,
    const UssReplacementWork* work,UssReplacementSnapshot* snapshot,UssPairObservation* pair,UssPublicationView* view)
{
    UssPublicationOwner row;
    *snapshot=s->observe(s->user,work);
    return uss_lifecycle_observe(life,thread,&snapshot->bundle.current,pair,view,&row) &&
        candidate_bundle_current(life,&snapshot->bundle) && snapshot->bundle.names.count==3;
}
static int replacement_guard(const UssReplacementWork* w,const UssCoreLifecycle* life,unsigned thread,
    const UssPickupHistory* history,const UssReplacementSnapshot* snapshot)
{
    const UssCoreCandidates* b=&snapshot->bundle;const UssCoreObservation* current=&b->current;
    UssPairMember incoming,kept,old;UssSupportClass kind=uss_classify_support(w->intent.incoming.identity,2);
    uint64_t generation=w->new_generation?w->new_generation:w->intent.pair_generation;
    uint32_t avatar=current->context.local.avatar,expected;
    UssLifecyclePhase phase=w->stage<=USS_REPLACE_HANDOVER?USS_LIFE_REPLACING:
        w->stage<=USS_REPLACE_PROMOTE?USS_LIFE_PENDING:USS_LIFE_RESTORING;
    if(w->local_only!=life->owner.context.local_only || (w->admitted && life->phase!=phase))return 0;
    if(w->new_generation &&
        (!pair_member_equal(&life->owner.utility,kind==USS_UTILITY_SUPPORT?&w->intent.incoming:&w->intent.kept) ||
         !pair_member_equal(&life->owner.ordinary,kind==USS_ORDINARY_SUPPORT?&w->intent.incoming:&w->intent.kept)))return 0;
    if(!pair_member_equal(&w->intent.released,kind==USS_UTILITY_SUPPORT?&w->old_utility:&w->old_ordinary) ||
        !pair_member_equal(&w->intent.kept,kind==USS_UTILITY_SUPPORT?&w->old_ordinary:&w->old_utility) ||
        (!w->new_generation && (!pair_member_equal(&life->owner.utility,&w->old_utility) || !pair_member_equal(&life->owner.ordinary,&w->old_ordinary))) ||
        !candidate_bundle_current(life,b) || !lifecycle_current(life,thread,current) || !current->pair_matched ||
        !history || history->disabled || history->last_serial!=w->intent.serial ||
        !pair_member_equal(&history->proposed,&w->intent.incoming) ||
        life->owner.epoch!=w->intent.epoch || life->owner.context_generation!=w->intent.context_generation ||
        life->owner.pair_generation!=generation || life->owner.guard_generation!=w->intent.guard_generation ||
        !pair_owner_context_equal(&life->owner.context,&w->intent.context) ||
        !lifecycle_member(&b->members[0],kind,&incoming) ||
        !lifecycle_member(&b->members[1],kind==USS_UTILITY_SUPPORT?USS_ORDINARY_SUPPORT:USS_UTILITY_SUPPORT,&kept) ||
        !lifecycle_member(&b->members[2],kind,&old) ||
        !pair_member_equal(&incoming,&w->intent.incoming) || !pair_member_equal(&kept,&w->intent.kept) ||
        !pair_member_equal(&old,&w->intent.released) || b->members[0].parent!=avatar)return 0;
    if(w->stage!=USS_REPLACE_PREPARE && w->stage!=USS_REPLACE_RETAINED_TICK && w->stage!=USS_REPLACE_VERIFY &&
        current->inventory.active_selector!=1)return 0;
    if(w->stage==USS_REPLACE_PREPARE && current->inventory.active_selector!=w->selected)return 0;
    if(b->members[1].parent!=((w->stage==USS_REPLACE_PREPARE || w->stage==USS_REPLACE_RECOVER)?0:avatar) ||
        b->members[2].parent!=(w->stage<USS_REPLACE_HANDOVER?avatar:0))return 0;
    if(w->stage==USS_REPLACE_PREPARE || w->stage==USS_REPLACE_RECOVER)expected=incoming.entity;
    else if(w->stage==USS_REPLACE_REPRESENT)expected=kept.entity;
    else if(w->stage==USS_REPLACE_DROP)expected=old.entity;
    else if(w->stage==USS_REPLACE_HANDOVER || w->stage==USS_REPLACE_RESTORE_NEW){
        if(!snapshot->empty_valid || publication_id(snapshot->native_empty))return 0;
        expected=snapshot->native_empty;
    }else if(w->stage==USS_REPLACE_RETAINED_TICK){
        return current->inventory.slots[2]==incoming.entity || current->inventory.slots[2]==kept.entity;
    }else expected=w->stage==USS_REPLACE_VERIFY && w->intent.restore_kept?kept.entity:incoming.entity;
    return current->inventory.slots[2]==expected;
}
/* Scoped Lua phase2 predicate, pure interpretation of already-owned fields.
 * Production capture/drop services must independently support the same guarded
 * boundary before it can authorize real entry. Public readiness is unchanged. */
static inline int uss_replacement_primary_ready(const UssReplacementWork* work,const UssReplacementSnapshot* snapshot,uint64_t now)
{
    const UssCoreObservation* c;const UssPrimaryReadiness* r;
    if(!work || work->phase!=USS_REPLACE_ACTIVE || !snapshot || now>=work->primary_deadline)return 0;
    c=&snapshot->bundle.current;r=&c->primary;
    const unsigned char* row=r->fields.row;uint32_t remaining=u32(row+28);
    if(!r->valid)return 0;
    if(r->ready)return 1;
    return work && now<work->primary_deadline && c->inventory.active_selector==1 &&
        c->inventory.held==c->inventory.slots[0] && u32(row)==c->inventory.avatar && u32(row+4)==1 &&
        u32(row+8)==3 && u32(row+20)==1 && u32(row+16)==2 && u32(row+24)==0 &&
        remaining>0 && remaining<0x7F800000 && r->bit47 &&
        r->fields.state_byte==0 && r->fields.global_guard==0;
}
static inline int uss_replacement_begin(UssReplacementWork* work,UssCoreLifecycle* life,unsigned thread,
    const UssPickupHistory* history,const UssPickupDecision* intent,const UssReplacementOrigin* origin,
    UssPublicationState* publication,const UssReplacementServices* services,uint64_t now)
{
    UssReplacementWork candidate={0};UssReplacementSnapshot snapshot;UssPairObservation pair;UssPublicationView view;
    unsigned standard;uint32_t expected;const UssCoreObservation* current;
    if(!work || work->phase!=USS_REPLACE_IDLE || !life || !intent || !origin || !origin->known || origin->utility_drawn>1 ||
        !replacement_services_valid(services) || !publication || publication->disabled || publication->suspended ||
        publication->busy || now>UINT64_MAX-2000 || !uss_pickup_intent_current(history,life,intent) ||
        origin->epoch!=intent->epoch || origin->context_generation!=intent->context_generation ||
        origin->pair_generation!=intent->pair_generation || origin->guard_generation!=intent->guard_generation ||
        origin->selected<1 || origin->selected>3 || origin->real!=intent->kept.entity || intent->restore_kept>1)return 0;
    candidate.local_only=life->owner.context.local_only;candidate.intent=*intent;candidate.origin=*origin;candidate.old_utility=life->owner.utility;candidate.old_ordinary=life->owner.ordinary;
    candidate.selected=intent->selected;candidate.phase=USS_REPLACE_ACTIVE;candidate.reason="ACTIVE";
    candidate.started_ms=now;candidate.deadline_ms=candidate.primary_deadline=now+2000;
    if(!replacement_read(services,life,thread,&candidate,&snapshot,&pair,&view) || !replacement_guard(&candidate,life,thread,history,&snapshot))return 0;
    current=&snapshot.bundle.current;
    standard=(origin->selected==1 || origin->selected==2) && !origin->utility_drawn &&
        (current->inventory.active_selector==origin->selected || current->inventory.active_selector==3);
    if(standard){expected=intent->restore_kept?origin->selected:3;if(current->inventory.active_selector!=expected)return 0;}
    else if(current->inventory.active_selector!=3 || origin->utility_drawn!=(unsigned)(intent->kept.entity==life->owner.utility.entity))return 0;
    if(!replacement_owned(life,publication,services,0))return 0;
    if(!uss_lifecycle_replace_begin(life,thread,current,&snapshot.bundle.members[0],candidate.selected,!intent->restore_kept))return 0;
    candidate.admitted=1;
    if(!uss_publication_reset(publication)){replacement_cancel(&candidate,life,"PUBLICATION_RESET_FAILED");*work=candidate;return 0;}
    *work=candidate;return 1;
}
static inline UssReplacementPhase uss_replacement_tick(UssReplacementWork* work,UssCoreLifecycle* life,unsigned thread,
    const UssPickupHistory* history,UssPublicationState* publication,const UssReplacementServices* services,uint64_t now,uint64_t tick)
{
    UssReplacementSnapshot snapshot;UssPairObservation pair;UssPublicationView view;
    UssPairServiceResult result;const UssMemberEvidence *utility,*ordinary;UssPairMember u,o;
    UssPairPhase retained;UssPublicationStage stage;
    if(!work)return USS_REPLACE_IDLE;
    if(work->phase!=USS_REPLACE_ACTIVE)return work->phase;
    if(!life || !history || !replacement_services_valid(services) || !publication || publication->disabled || publication->suspended ||
        now<work->started_ms || now>=work->deadline_ms || (work->has_tick && tick<work->last_tick)){
        replacement_cancel(work,life,"TIME_OR_SERVICES_CHANGED");return work->phase;}
    if((work->has_tick && tick==work->last_tick) || now<work->next_ms)return work->phase;
    work->has_tick=1;work->last_tick=tick;
    if(!replacement_read(services,life,thread,work,&snapshot,&pair,&view) ||
        !replacement_guard(work,life,thread,history,&snapshot)){replacement_cancel(work,life,"CONTEXT_PICKUP_OR_PARENTS_CHANGED");return work->phase;}
    if((work->stage==USS_REPLACE_REPRESENT || work->stage==USS_REPLACE_DROP) &&
        !replacement_owned(life,publication,services,work->stage==USS_REPLACE_REPRESENT?work->intent.kept.entity:work->intent.released.entity)){
        replacement_cancel(work,life,"OUTGOING_PUBLICATION_MISSING");return work->phase;
    }
    if(work->stage==USS_REPLACE_PREPARE){
        work->primary_requested=1;work->stage=USS_REPLACE_RECOVER;work->next_ms=now+25;
        if(snapshot.bundle.current.inventory.active_selector!=1){work->consumed_mask|=1;
            if(services->select_slot(services->user,&snapshot,1)!=USS_PAIR_SERVICE_DONE)replacement_cancel(work,life,"PRIMARY_SELECTION_UNCERTAIN");}
        return work->phase;
    }
    if(work->stage==USS_REPLACE_RECOVER){
        result=services->primary_ready(services->user,work,&snapshot,now);
        if(result==USS_PAIR_SERVICE_WAIT){work->next_ms=now+25;return work->phase;}
        if(result!=USS_PAIR_SERVICE_DONE){replacement_cancel(work,life,"PRIMARY_READINESS_FAILED");return work->phase;}
        work->stage=USS_REPLACE_REPRESENT;work->consumed_mask|=2;
        stage=uss_classify_support(work->intent.kept.identity,2)==USS_UTILITY_SUPPORT?USS_PUBLICATION_REPLACEMENT_UTILITY:USS_PUBLICATION_POST_ATTACH;
        if(services->switch_member(services->user,&snapshot,&work->intent.kept,stage)!=USS_PAIR_SERVICE_DONE)replacement_cancel(work,life,"RETAINED_RECOVERY_UNCERTAIN");
    }else if(work->stage==USS_REPLACE_REPRESENT){
        work->stage=USS_REPLACE_DROP;work->consumed_mask|=4;
        if(services->switch_member(services->user,&snapshot,&work->intent.released,USS_PUBLICATION_POST_ATTACH)!=USS_PAIR_SERVICE_DONE)replacement_cancel(work,life,"OUTGOING_REPRESENT_UNCERTAIN");
    }else if(work->stage==USS_REPLACE_DROP){
        work->stage=USS_REPLACE_HANDOVER;work->consumed_mask|=8;
        if(services->drop_slot(services->user,&snapshot)!=USS_PAIR_SERVICE_DONE)replacement_cancel(work,life,"NATIVE_DROP_UNCERTAIN");
    }else if(work->stage==USS_REPLACE_HANDOVER){
        utility=uss_classify_support(work->intent.incoming.identity,2)==USS_UTILITY_SUPPORT?&snapshot.bundle.members[0]:&snapshot.bundle.members[1];
        ordinary=utility==&snapshot.bundle.members[0]?&snapshot.bundle.members[1]:&snapshot.bundle.members[0];
        work->consumed_mask|=16;
        if(!uss_lifecycle_replace_handover(life,thread,&snapshot.bundle.current,utility,ordinary,&snapshot.bundle.members[2],snapshot.empty_valid,snapshot.native_empty)){
            replacement_cancel(work,life,"MEMBERSHIP_HANDOVER_FAILED");return work->phase;}
        uss_local_pair_invalidate(services->local);
        work->new_generation=life->owner.pair_generation;work->stage=USS_REPLACE_RESTORE_NEW;
        if(!uss_publication_reset(publication))replacement_cancel(work,life,"NEW_PUBLICATION_RESET_FAILED");
    }else if(work->stage==USS_REPLACE_RESTORE_NEW){
        work->stage=USS_REPLACE_PROMOTE;work->consumed_mask|=32;
        stage=uss_classify_support(work->intent.incoming.identity,2)==USS_UTILITY_SUPPORT?USS_PUBLICATION_REPLACEMENT_UTILITY:USS_PUBLICATION_POST_ATTACH;
        if(services->switch_member(services->user,&snapshot,&work->intent.incoming,stage)!=USS_PAIR_SERVICE_DONE)replacement_cancel(work,life,"INCOMING_RESTORE_UNCERTAIN");
    }else if(work->stage==USS_REPLACE_PROMOTE){
        /* Newly attached roles are structurally formed and have an actual returned
         * publication. Keep lifecycle RESTORING until requested selection finishes;
         * pickup history blocks new decisions in that phase. */
        if(!replacement_owned(life,publication,services,work->intent.incoming.entity) ||
            (!work->local_only && (!view.supported_pair || !publication_key_equal(&view.key,&publication->record)))){
            replacement_cancel(work,life,"NEW_PAIR_PUBLICATION_MISSING");return work->phase;}
        life->phase=USS_LIFE_RESTORING;
        work->stage=work->intent.restore_kept?USS_REPLACE_RETAINED_START:USS_REPLACE_SELECT_FINAL;
    }else if(work->stage==USS_REPLACE_RETAINED_START){
        if(!uss_pair_begin(&work->retained,&services->retained,&pair,USS_PAIR_RETAINED,work->selected,0,&work->intent.kept,now)){
            replacement_cancel(work,life,"RETAINED_QUEUE_FAILED");return work->phase;}
        work->deadline_ms=work->retained.deadline_ms;work->stage=USS_REPLACE_RETAINED_TICK;
    }else if(work->stage==USS_REPLACE_RETAINED_TICK){
        retained=uss_pair_tick(&work->retained,&services->retained,now);
        if(retained==USS_PAIR_CANCELLED)replacement_cancel(work,life,"RETAINED_TRANSACTION_CANCELLED");
        else if(retained==USS_PAIR_COMPLETE)work->stage=USS_REPLACE_VERIFY;
    }else if(work->stage==USS_REPLACE_SELECT_FINAL){
        work->stage=USS_REPLACE_VERIFY;work->consumed_mask|=64;
        if(snapshot.bundle.current.inventory.active_selector!=work->selected &&
            services->select_slot(services->user,&snapshot,work->selected)!=USS_PAIR_SERVICE_DONE)replacement_cancel(work,life,"FINAL_SELECTION_UNCERTAIN");
    }else if(work->stage==USS_REPLACE_VERIFY){
        u=life->owner.utility;o=life->owner.ordinary;
        if(life->phase!=USS_LIFE_RESTORING || !lifecycle_plan_current(life) ||
            !pair_member_equal(&u,&life->plan.utility) || !pair_member_equal(&o,&life->plan.ordinary) ||
            snapshot.bundle.current.inventory.active_selector!=work->selected ||
            !replacement_owned(life,publication,services,snapshot.bundle.current.inventory.slots[2]) ||
            (!work->local_only && (!view.supported_pair || !publication_key_equal(&view.key,&publication->record))) ||
            (work->intent.restore_kept && (work->retained.phase!=USS_PAIR_COMPLETE || work->retained.real!=work->intent.kept.entity))){
            replacement_cancel(work,life,"FINAL_REPLACEMENT_CHANGED");return work->phase;}
        work->real=snapshot.bundle.current.inventory.slots[2];work->utility_drawn=work->real==u.entity && work->selected==3;
        life->phase=USS_LIFE_FORMED;work->phase=USS_REPLACE_COMPLETE;work->reason="COMPLETE";
    }else replacement_cancel(work,life,"UNKNOWN_STAGE");
    return work->phase;
}
#endif
