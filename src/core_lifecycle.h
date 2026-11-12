/* Explicit Core lifecycle decisions. No polling, callbacks or gameplay effects.
 * Evidence is supplied by validated current readers; proposed members are not
 * evidence that attachment/publication completed. Zero-initialize once. */
#ifndef USS_CORE_LIFECYCLE_H
#define USS_CORE_LIFECYCLE_H
#include "core_observation.h"
typedef enum UssLifecyclePhase {
    USS_LIFE_EMPTY=0,USS_LIFE_PENDING,USS_LIFE_FORMED,USS_LIFE_REPLACING,USS_LIFE_PAUSED,USS_LIFE_RESTORING
} UssLifecyclePhase;
typedef struct UssLifecyclePlan {
    UssPairMember utility,ordinary,released;
    UssPairOrder order;
    uint32_t selected;
    unsigned switch_on_pickup;
    uint64_t epoch,context_generation,pair_generation,guard_generation;
} UssLifecyclePlan;
typedef struct UssCoreLifecycle {
    UssPairOwner owner;
    UssLifecyclePhase phase;
    UssLifecyclePlan plan;
    uint64_t generation;
} UssCoreLifecycle;

/* Other policy flags belong to the caller's actual Core/input state. Never infer
 * those from a pair, and never accept caller-proposed formed/pending flags. */
static UssCoreFlags uss_lifecycle_flags(const UssCoreLifecycle* life,const UssCoreFlags* policy)
{
    UssCoreFlags flags={0};if(policy)flags=*policy;
    flags.formed=life && life->owner.pair_present && (life->phase==USS_LIFE_FORMED || life->phase==USS_LIFE_RESTORING);
    flags.formation_pending=life && life->owner.pair_present &&
        (life->phase==USS_LIFE_PENDING || life->phase==USS_LIFE_REPLACING);
    if(life && life->phase==USS_LIFE_PAUSED)flags.paused=1;
    if(life && life->owner.disabled)flags.disabled=1;
    return flags;
}
static int lifecycle_current(const UssCoreLifecycle* life,unsigned thread,const UssCoreObservation* current)
{
    return life && current && current->valid && current->mission && life->owner.ready &&
        !life->owner.disabled && life->owner.thread==thread && current->thread==thread &&
        current->expected_epoch==life->owner.epoch && current->expected_context==life->owner.context_generation &&
        current->expected_pair==life->owner.pair_generation && current->expected_guard==life->owner.guard_generation &&
        current->expected_pair_present==life->owner.pair_present &&
        pair_owner_context_equal(&current->context,&life->owner.context) &&
        current->inventory.slots[0]!=0 && current->inventory.slots[1]!=0 &&
        current->inventory.active_selector>=1 && current->inventory.active_selector<=3 &&
        current->flags.known && current->flags.eligible && !current->flags.disabled &&
        !current->flags.suspended && !current->flags.paused && !current->flags.down && !current->flags.input;
}
static int lifecycle_member(const UssMemberEvidence* evidence,UssSupportClass kind,UssPairMember* member)
{
    if(!evidence || evidence->status!=USS_MEMBER_OBSERVED || !publication_id(evidence->entity) ||
        evidence->kind!=2 || evidence->classification!=kind ||
        uss_classify_support(evidence->descriptor,2)!=kind || u32(evidence->descriptor+8)!=evidence->entity ||
        u32(evidence->interaction_descriptor+8)!=evidence->entity ||
        memcmp(evidence->descriptor,evidence->interaction_descriptor,8))return 0;
    member->entity=evidence->entity;memcpy(member->identity,evidence->descriptor,8);return 1;
}
static void lifecycle_stamp(UssCoreLifecycle* life)
{
    life->generation=life->owner.pair_generation;
    life->plan.epoch=life->owner.epoch;life->plan.context_generation=life->owner.context_generation;
    life->plan.pair_generation=life->owner.pair_generation;life->plan.guard_generation=life->owner.guard_generation;
}
static int lifecycle_plan_current(const UssCoreLifecycle* life)
{
    return life->plan.epoch==life->owner.epoch && life->plan.context_generation==life->owner.context_generation &&
        life->plan.pair_generation==life->owner.pair_generation && life->plan.guard_generation==life->owner.guard_generation;
}
/* Apply the reader result first. Unknown keeps membership/history; a confirmed
 * context/mission reset clears it. Interrupted pending effects never auto-retry. */
static int uss_lifecycle_observe(UssCoreLifecycle* life,unsigned thread,const UssCoreObservation* current,
    UssPairObservation* observation,UssPublicationView* view,UssPublicationOwner* row)
{
    int accepted;if(!life)return 0;
    accepted=uss_core_apply(&life->owner,thread,current,observation,view,row);
    if(!life->owner.pair_present) {
        life->phase=USS_LIFE_EMPTY;memset(&life->plan,0,sizeof(life->plan));life->generation=life->owner.pair_generation;
    } else if(life->generation!=life->owner.pair_generation ||
        ((life->phase==USS_LIFE_PENDING || life->phase==USS_LIFE_REPLACING || life->phase==USS_LIFE_RESTORING) && !lifecycle_plan_current(life))) {
        life->phase=USS_LIFE_PAUSED;
        if(observation){observation->paused=1;observation->formed=0;observation->formation_pending=0;}
    }
    return accepted;
}
/* Explicit create event, after Core detected either pickup order. Merely observing
 * an unchanged slot never calls this. Parents may be avatar or displaced zero;
 * the currently picked member must actually be attached to this avatar. */
static int uss_lifecycle_create(UssCoreLifecycle* life,unsigned thread,const UssCoreObservation* current,
    const UssMemberEvidence* utility,const UssMemberEvidence* ordinary,UssPairOrder order,
    uint32_t selected,unsigned switch_on_pickup)
{
    UssLifecyclePlan plan={0};const UssMemberEvidence* picked;
    if(!lifecycle_current(life,thread,current) || life->phase!=USS_LIFE_EMPTY || life->owner.pair_present ||
        selected<1 || selected>3 || switch_on_pickup>1 ||
        !lifecycle_member(utility,USS_UTILITY_SUPPORT,&plan.utility) ||
        !lifecycle_member(ordinary,USS_ORDINARY_SUPPORT,&plan.ordinary))return 0;
    if(order!=USS_PAIR_ORDINARY_FIRST && order!=USS_PAIR_UTILITY_FIRST)return 0;
    picked=order==USS_PAIR_ORDINARY_FIRST?utility:ordinary;
    if(current->inventory.slots[2]!=picked->entity || picked->parent!=current->context.local.avatar ||
        (utility->parent && utility->parent!=current->context.local.avatar) ||
        (ordinary->parent && ordinary->parent!=current->context.local.avatar) ||
        memcmp(current->inventory.identities[2],picked->descriptor,8) ||
        (order==USS_PAIR_ORDINARY_FIRST && switch_on_pickup && selected!=3))return 0;
    if(!uss_pair_owner_set_pair(&life->owner,thread,0,&plan.utility,&plan.ordinary))return 0;
    plan.order=order;plan.selected=selected;plan.switch_on_pickup=switch_on_pickup;
    life->plan=plan;life->phase=USS_LIFE_PENDING;lifecycle_stamp(life);return 1;
}
/* Replacement is a proposal until the old same-type member has been released.
 * Do not mint a new generation before its effects have a validated post-read. */
static int uss_lifecycle_replace_begin(UssCoreLifecycle* life,unsigned thread,const UssCoreObservation* current,
    const UssMemberEvidence* incoming,uint32_t selected,unsigned switch_on_pickup)
{
    UssPairMember member={0};UssLifecyclePlan plan={0};UssSupportClass kind;
    if(!lifecycle_current(life,thread,current) || life->phase!=USS_LIFE_FORMED || !current->pair_matched ||
        selected<1 || selected>3 || switch_on_pickup>1 || (switch_on_pickup && selected!=3) || !incoming)return 0;
    kind=incoming->classification;
    if((kind!=USS_UTILITY_SUPPORT && kind!=USS_ORDINARY_SUPPORT) || !lifecycle_member(incoming,kind,&member) ||
        incoming->parent!=current->context.local.avatar || current->inventory.slots[2]!=member.entity ||
        memcmp(current->inventory.identities[2],member.identity,8) ||
        member.entity==life->owner.utility.entity || member.entity==life->owner.ordinary.entity)return 0;
    plan.utility=life->owner.utility;plan.ordinary=life->owner.ordinary;
    if(kind==USS_UTILITY_SUPPORT){plan.released=plan.utility;plan.utility=member;plan.order=USS_PAIR_ORDINARY_FIRST;}
    else {plan.released=plan.ordinary;plan.ordinary=member;plan.order=USS_PAIR_UTILITY_FIRST;}
    plan.selected=selected;plan.switch_on_pickup=switch_on_pickup;
    life->plan=plan;life->phase=USS_LIFE_REPLACING;lifecycle_stamp(life);return 1;
}
static int lifecycle_replace_commit(UssCoreLifecycle* life,unsigned thread,const UssCoreObservation* current,
    const UssMemberEvidence* utility,const UssMemberEvidence* ordinary,const UssMemberEvidence* released,
    unsigned after_drop,uint32_t native_empty)
{
    UssPairMember u={0},o={0},old={0};UssSupportClass old_kind;const UssPairMember* incoming;
    if(!lifecycle_current(life,thread,current) || life->phase!=USS_LIFE_REPLACING || !lifecycle_plan_current(life) ||
        !lifecycle_member(utility,USS_UTILITY_SUPPORT,&u) || !lifecycle_member(ordinary,USS_ORDINARY_SUPPORT,&o) ||
        !pair_member_equal(&u,&life->plan.utility) || !pair_member_equal(&o,&life->plan.ordinary) ||
        utility->parent!=current->context.local.avatar || ordinary->parent!=current->context.local.avatar)return 0;
    incoming=life->plan.order==USS_PAIR_ORDINARY_FIRST?&life->plan.utility:&life->plan.ordinary;
    if(after_drop){
        if(publication_id(native_empty) || current->inventory.slots[2]!=native_empty || current->inventory.active_selector!=1)return 0;
    }else if(current->inventory.slots[2]!=incoming->entity ||
        memcmp(current->inventory.identities[2],incoming->identity,8))return 0;
    old_kind=uss_classify_support(life->plan.released.identity,2);
    if(!lifecycle_member(released,old_kind,&old) || !pair_member_equal(&old,&life->plan.released) || released->parent!=0)return 0;
    if(!uss_pair_owner_set_pair(&life->owner,thread,life->plan.pair_generation,&u,&o))return 0;
    /* Membership changed; attachment/publication completion is still separate. */
    life->phase=USS_LIFE_PENDING;lifecycle_stamp(life);return 1;
}
/* Legacy attached-incoming admission stays available for its existing callers. */
static int uss_lifecycle_replace_commit(UssCoreLifecycle* life,unsigned thread,const UssCoreObservation* current,
    const UssMemberEvidence* utility,const UssMemberEvidence* ordinary,const UssMemberEvidence* released)
{return lifecycle_replace_commit(life,thread,current,utility,ordinary,released,0,0);}
/* Lua handover occurs AFTER verified native drop, BEFORE incoming restore/send.
 * Caller must provide the freshly observed native empty-slot sentinel from the
 * SAME operation. This pure interface does not read that global or grant entry. */
static inline int uss_lifecycle_replace_handover(UssCoreLifecycle* life,unsigned thread,const UssCoreObservation* current,
    const UssMemberEvidence* utility,const UssMemberEvidence* ordinary,const UssMemberEvidence* released,
    unsigned empty_valid,uint32_t native_empty)
{return empty_valid && lifecycle_replace_commit(life,thread,current,utility,ordinary,released,1,native_empty);}
/* Existing coordinator owns completion. This is not a boolean "formed" setter.
 * Replacement services must eventually provide an equivalent reviewed completion;
 * this slice does not invent or execute the missing replacement transaction. */
static int uss_lifecycle_complete(UssCoreLifecycle* life,unsigned thread,const UssCoreObservation* current,
    const UssPairTransaction* work)
{
    if(!lifecycle_current(life,thread,current) || life->phase!=USS_LIFE_PENDING || !lifecycle_plan_current(life) ||
        !current->pair_matched || !work || work->phase!=USS_PAIR_COMPLETE || work->order!=life->plan.order ||
        work->origin.epoch!=life->owner.epoch || work->origin.pair_generation!=life->owner.pair_generation ||
        work->origin.guard_generation!=life->owner.guard_generation ||
        !pair_context_equal(&work->origin.context,&life->owner.context.local) ||
        !pair_member_equal(&work->origin.utility,&life->owner.utility) ||
        !pair_member_equal(&work->origin.ordinary,&life->owner.ordinary) ||
        current->utility.parent!=current->context.local.avatar || current->ordinary.parent!=current->context.local.avatar ||
        current->inventory.slots[2]!=work->real || current->inventory.active_selector!=work->selected ||
        work->selected!=life->plan.selected ||
        (work->real!=life->owner.utility.entity && work->real!=life->owner.ordinary.entity))return 0;
    life->phase=USS_LIFE_FORMED;return 1;
}
/* Consume failed/uncertain work without pretending completed effects rolled back. */
static inline int uss_lifecycle_cancel(UssCoreLifecycle* life,unsigned thread,uint64_t expected)
{
    if(!life || !pair_owner_thread(&life->owner,thread) || !life->owner.pair_present ||
        expected!=life->owner.pair_generation ||
        (life->phase!=USS_LIFE_PENDING && life->phase!=USS_LIFE_REPLACING && life->phase!=USS_LIFE_RESTORING))return 0;
    life->phase=USS_LIFE_PAUSED;pair_owner_interrupt(&life->owner);return 1;
}
/* Explicit removal/reset only after the owning Core route decides to forget it;
 * does not drop/reacquire any entity, roll back effects or reset monotonic ids. */
static inline int uss_lifecycle_remove(UssCoreLifecycle* life,unsigned thread,uint64_t expected)
{
    if(!life || !uss_pair_owner_clear_pair(&life->owner,thread,expected))return 0;
    life->phase=USS_LIFE_EMPTY;memset(&life->plan,0,sizeof(life->plan));return 1;
}
#endif
