/* Lua replacement route where vanilla pickup already released the old real
 * member. No native drop, slot writes, or weakened displaced-retained guard.
 * Bundle order: new utility, new ordinary, released old member. */
#ifndef USS_ACTIVE_REPLACEMENT_H
#define USS_ACTIVE_REPLACEMENT_H
#include "replacement_transaction.h"
static inline int active_replacement_prepare(UssCoreLifecycle* life,unsigned thread,
    const UssPickupHistory* history,const UssPickupDecision* intent,const UssReplacementOrigin* origin,
    const UssCoreCandidates* bundle,UssPublicationState* publication,UssLocalPairState* local)
{
    const UssMemberEvidence *incoming,*kept,*old;unsigned utility,draw;uint32_t selected;
    if(!life || !history || !intent || !origin || !bundle || !publication || !origin->known ||
        !uss_pickup_intent_current(history,life,intent) || !candidate_bundle_current(life,bundle) || bundle->names.count!=3 ||
        !lifecycle_current(life,thread,&bundle->current) || !bundle->current.pair_matched ||
        origin->epoch!=intent->epoch || origin->context_generation!=intent->context_generation ||
        origin->pair_generation!=intent->pair_generation || origin->guard_generation!=intent->guard_generation ||
        origin->real!=intent->released.entity || origin->selected<1 || origin->selected>3 ||
        origin->utility_drawn!=(unsigned)(origin->selected==3 && origin->real==life->owner.utility.entity) ||
        intent->restore_kept>1 || publication->disabled || publication->suspended || publication->busy ||
        !(life->owner.context.local_only?uss_local_pair_owned(local,&life->owner):replacement_record_matches(life,publication)))return 0;
    utility=uss_classify_support(intent->incoming.identity,2)==USS_UTILITY_SUPPORT;
    incoming=&bundle->members[utility?0:1];kept=&bundle->members[utility?1:0];old=&bundle->members[2];
    if(!pair_member_equal(&bundle->names.members[utility?0:1],&intent->incoming) ||
        !pair_member_equal(&bundle->names.members[utility?1:0],&intent->kept) ||
        !pair_member_equal(&bundle->names.members[2],&intent->released) ||
        incoming->parent!=bundle->current.inventory.avatar || kept->parent!=bundle->current.inventory.avatar || old->parent!=0)return 0;
    draw=utility && (!intent->restore_kept || origin->utility_drawn);
    selected=utility?(draw?3:origin->selected):bundle->current.inventory.active_selector;
    if(!uss_lifecycle_replace_begin(life,thread,&bundle->current,incoming,selected,draw))return 0;
    if(!uss_publication_reset(publication))return 0;
    uss_local_pair_invalidate(local);
    return uss_lifecycle_replace_commit_candidates(life,thread,bundle);
}
static inline int uss_active_replacement_prepare(UssCoreLifecycle* life,unsigned thread,
    const UssPickupHistory* history,const UssPickupDecision* intent,const UssReplacementOrigin* origin,
    const UssCoreCandidates* bundle,UssPublicationState* publication)
{return active_replacement_prepare(life,thread,history,intent,origin,bundle,publication,NULL);}
static inline int uss_active_replacement_prepare_local(UssCoreLifecycle* life,unsigned thread,
    const UssPickupHistory* history,const UssPickupDecision* intent,const UssReplacementOrigin* origin,
    const UssCoreCandidates* bundle,UssPublicationState* publication,UssLocalPairState* local)
{return life && life->owner.context.local_only && active_replacement_prepare(life,thread,history,intent,origin,bundle,publication,local);}
#endif
