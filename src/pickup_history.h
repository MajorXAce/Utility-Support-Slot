/* Pure observed-pickup decisions. Keeps identity/intent, never cached evidence.
 * Core still owns fresh reads, preference/input/death state and every effect. */
#ifndef USS_PICKUP_HISTORY_H
#define USS_PICKUP_HISTORY_H
#include "candidate_lifecycle.h"
typedef enum UssPickupKind {
    USS_PICKUP_UNKNOWN=0,USS_PICKUP_REMEMBERED,USS_PICKUP_NEED_READ,USS_PICKUP_PAIR_PENDING,
    USS_PICKUP_MANAGED,USS_PICKUP_REPLACEMENT_INTENT,USS_PICKUP_WAIT_WORK,
    USS_PICKUP_RECOVERY_REQUIRED,USS_PICKUP_RESET,USS_PICKUP_REFUSED,USS_PICKUP_WAIT_POLICY
} UssPickupKind;
typedef struct UssPickupPolicy {unsigned known,switch_on_pickup;} UssPickupPolicy;
typedef struct UssPickupHistory {
    unsigned bound,remembered,disabled,pair_present;
    UssPairOwnerContext context;
    uint64_t epoch,context_generation,pair_generation,serial,last_serial,proposal_guard;
    UssPairMember last,proposed;
    UssSupportClass last_kind;
    uint32_t selected;
} UssPickupHistory;
typedef struct UssPickupDecision {
    UssPickupKind kind;
    UssCoreCandidateNames needed;
    UssPairMember incoming,kept,released;
    UssPairOwnerContext context;
    uint32_t selected;
    unsigned restore_kept;
    uint64_t serial,epoch,context_generation,pair_generation,guard_generation;
} UssPickupDecision;
static void pickup_forget(UssPickupHistory* history)
{
    history->remembered=0;memset(&history->last,0,sizeof(history->last));history->last_kind=USS_NOT_SUPPORT;
}
static void pickup_clear(UssPickupHistory* history)
{
    uint64_t serial=history->serial;unsigned disabled=history->disabled;
    memset(history,0,sizeof(*history));history->serial=serial;history->disabled=disabled;
}
/* Filter role order from ONE snapshot; missing evidence requests a later fresh
 * bundle explicitly. It never fills a missing member from remembered read data. */
static int pickup_subset(const UssCoreLifecycle* life,const UssCoreCandidates* source,
    const UssPairMember* wanted,unsigned count,UssCoreCandidates* selected,UssPickupDecision* decision)
{
    unsigned i,j;
    memset(selected,0,sizeof(*selected));
    if(!uss_core_candidate_names(&life->owner,wanted,count,&decision->needed))return 0;
    selected->current=source->current;selected->names=decision->needed;
    for(i=0;i<count;++i){
        for(j=0;j<source->names.count;++j)if(pair_member_equal(&wanted[i],&source->names.members[j]))break;
        if(j==source->names.count){decision->kind=USS_PICKUP_NEED_READ;return 0;}
        selected->members[i]=source->members[j];
    }
    selected->valid=1;memset(&decision->needed,0,sizeof(decision->needed));return 1;
}
static UssPickupDecision uss_pickup_step(UssPickupHistory* history,UssCoreLifecycle* life,unsigned thread,
    const UssCoreCandidates* bundle,const UssPickupPolicy* policy)
{
    UssPickupDecision decision={0};UssCoreCandidates selected={0};UssPairMember incoming={0},wanted[2];
    UssMemberEvidence* evidence;UssSupportClass kind;UssPairObservation observation;
    UssPublicationView view;UssPublicationOwner row;const UssCoreObservation* current;
    UssPairOrder order;uint32_t restore;
    if(!history || !life || !bundle)return decision;
    current=&bundle->current;
    if(!uss_lifecycle_observe(life,thread,current,&observation,&view,&row))return decision;
    if(!current->mission){pickup_clear(history);decision.kind=USS_PICKUP_RESET;return decision;}
    if(history->disabled){decision.kind=USS_PICKUP_REFUSED;return decision;}
    if(!history->bound || history->epoch!=life->owner.epoch ||
        history->context_generation!=life->owner.context_generation ||
        history->pair_generation!=life->owner.pair_generation || history->pair_present!=life->owner.pair_present ||
        !pair_owner_context_equal(&history->context,&life->owner.context)){
        pickup_clear(history);history->bound=1;history->context=life->owner.context;
        history->epoch=life->owner.epoch;history->context_generation=life->owner.context_generation;
        history->pair_generation=life->owner.pair_generation;history->pair_present=life->owner.pair_present;
    }
    if(!bundle->valid || !candidate_bundle_current(life,bundle))return decision;
    if(current->flags.down || !current->inventory.slots[0] || !current->inventory.slots[1])pickup_forget(history);
    if(!lifecycle_current(life,thread,current)){decision.kind=USS_PICKUP_REFUSED;return decision;}
    incoming.entity=current->inventory.slots[2];memcpy(incoming.identity,current->inventory.identities[2],8);
    if(life->owner.pair_present){
        pickup_forget(history);
        if(life->phase!=USS_LIFE_FORMED){decision.kind=USS_PICKUP_WAIT_WORK;return decision;}
        if(!publication_id(incoming.entity)){decision.kind=USS_PICKUP_RECOVERY_REQUIRED;return decision;}
        if(incoming.entity==life->owner.utility.entity || incoming.entity==life->owner.ordinary.entity){
            history->last_serial=0;decision.kind=USS_PICKUP_MANAGED;return decision;
        }
    } else if(!publication_id(incoming.entity)){
        pickup_forget(history);decision.kind=USS_PICKUP_RESET;return decision;
    }
    kind=current->support_class;
    if(kind!=USS_UTILITY_SUPPORT && kind!=USS_ORDINARY_SUPPORT){pickup_forget(history);decision.kind=USS_PICKUP_REFUSED;return decision;}
    if(!pickup_subset(life,bundle,&incoming,1,&selected,&decision))return decision;
    evidence=&selected.members[0];
    if(evidence->parent!=current->context.local.avatar){decision.kind=USS_PICKUP_REFUSED;return decision;}
    if(life->owner.pair_present){
        if(!policy || !policy->known || policy->switch_on_pickup>1){decision.kind=USS_PICKUP_WAIT_POLICY;return decision;}
        if(history->last_serial && pair_member_equal(&history->proposed,&incoming) && history->proposal_guard==life->owner.guard_generation){
            decision.kind=USS_PICKUP_WAIT_WORK;return decision;
        }
        if(history->serial==UINT64_MAX){history->disabled=1;decision.kind=USS_PICKUP_REFUSED;return decision;}
        history->last_serial=++history->serial;history->proposed=incoming;history->proposal_guard=life->owner.guard_generation;
        decision.kind=USS_PICKUP_REPLACEMENT_INTENT;decision.serial=history->last_serial;decision.incoming=incoming;
        decision.released=kind==USS_UTILITY_SUPPORT?life->owner.utility:life->owner.ordinary;
        decision.kept=kind==USS_UTILITY_SUPPORT?life->owner.ordinary:life->owner.utility;
        decision.selected=current->inventory.active_selector;decision.restore_kept=!policy->switch_on_pickup;
        decision.context=life->owner.context;decision.epoch=life->owner.epoch;decision.context_generation=life->owner.context_generation;
        decision.pair_generation=life->owner.pair_generation;decision.guard_generation=life->owner.guard_generation;
        /* Proposal only. Replacement origin/selection must still be checked by
         * the future replacement service before entry, just as Lua does. */
        return decision;
    }
    if(!history->remembered || history->last_kind==kind || history->last.entity==incoming.entity){
        history->remembered=1;history->last=incoming;history->last_kind=kind;
        history->selected=current->inventory.active_selector;decision.kind=USS_PICKUP_REMEMBERED;return decision;
    }
    order=kind==USS_UTILITY_SUPPORT?USS_PAIR_ORDINARY_FIRST:USS_PAIR_UTILITY_FIRST;
    if(order==USS_PAIR_ORDINARY_FIRST && (!policy || !policy->known || policy->switch_on_pickup>1)){
        decision.kind=USS_PICKUP_WAIT_POLICY;return decision;
    }
    wanted[0]=kind==USS_UTILITY_SUPPORT?incoming:history->last;
    wanted[1]=kind==USS_ORDINARY_SUPPORT?incoming:history->last;
    /* Utility-first restores the observed incoming selection; preference is not
     * consulted by that Lua route. Its unused ordinary-first flag stays zero. */
    restore=order==USS_PAIR_ORDINARY_FIRST?(policy->switch_on_pickup?3:history->selected):current->inventory.active_selector;
    if(!pickup_subset(life,bundle,wanted,2,&selected,&decision))return decision;
    if(!uss_lifecycle_create_candidates(life,thread,&selected,order,restore,
        order==USS_PAIR_ORDINARY_FIRST?policy->switch_on_pickup:0)){
        decision.kind=USS_PICKUP_REFUSED;return decision;
    }
    pickup_forget(history);decision.kind=USS_PICKUP_PAIR_PENDING;decision.selected=restore;return decision;
}
/* A later service may test that its proposal was not superseded. This check is
 * NOT native-entry authority; it still needs fresh target/origin/effect guards. */
static inline int uss_pickup_intent_current(const UssPickupHistory* history,const UssCoreLifecycle* life,
    const UssPickupDecision* decision)
{
    return history && !history->disabled && life && decision && decision->kind==USS_PICKUP_REPLACEMENT_INTENT &&
        decision->serial && decision->serial==history->last_serial && pair_member_equal(&decision->incoming,&history->proposed) &&
        pair_owner_context_equal(&decision->context,&life->owner.context) &&
        life->phase==USS_LIFE_FORMED && pair_owner_ready(&life->owner,life->owner.thread) &&
        decision->epoch==life->owner.epoch && decision->context_generation==life->owner.context_generation &&
        decision->pair_generation==life->owner.pair_generation && decision->guard_generation==life->owner.guard_generation;
}
#endif
