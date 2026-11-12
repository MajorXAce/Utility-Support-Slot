/* Consume only one owned coherent candidate bundle. No cached member mixing,
 * pair inference, automatic observation refresh or effect execution. */
#ifndef USS_CANDIDATE_LIFECYCLE_H
#define USS_CANDIDATE_LIFECYCLE_H
#include "core_lifecycle.h"
static inline int candidate_bundle_current(const UssCoreLifecycle* life,const UssCoreCandidates* bundle)
{
    unsigned i;
    if(!life || !bundle || !bundle->valid || !bundle->current.valid || !bundle->current.mission ||
        !core_candidate_names_valid(&life->owner,&bundle->names) ||
        !pair_owner_context_equal(&bundle->current.context,&bundle->names.context) ||
        bundle->current.expected_epoch!=bundle->names.epoch ||
        bundle->current.expected_context!=bundle->names.context_generation ||
        bundle->current.expected_pair!=bundle->names.pair_generation ||
        bundle->current.expected_guard!=bundle->names.guard_generation ||
        bundle->current.expected_pair_present!=bundle->names.pair_present)return 0;
    for(i=0;i<bundle->names.count;++i)
        if(bundle->members[i].status!=USS_MEMBER_OBSERVED || bundle->members[i].entity!=bundle->names.members[i].entity ||
            memcmp(bundle->members[i].descriptor,bundle->names.members[i].identity,8))return 0;
    return 1;
}
/* Fixed role ordering: utility, ordinary. Naming remains an explicit Core event. */
static inline int uss_lifecycle_create_candidates(UssCoreLifecycle* life,unsigned thread,const UssCoreCandidates* bundle,
    UssPairOrder order,uint32_t selected,unsigned switch_on_pickup)
{
    return candidate_bundle_current(life,bundle) && bundle->names.count==2 &&
        uss_lifecycle_create(life,thread,&bundle->current,&bundle->members[0],&bundle->members[1],order,selected,switch_on_pickup);
}
/* One explicitly named incoming candidate; current owned members come from the
 * same observer interval, not a separate earlier pair reading. */
static inline int uss_lifecycle_replace_begin_candidates(UssCoreLifecycle* life,unsigned thread,
    const UssCoreCandidates* bundle,uint32_t selected,unsigned switch_on_pickup)
{
    return candidate_bundle_current(life,bundle) && bundle->names.count==1 &&
        uss_lifecycle_replace_begin(life,thread,&bundle->current,&bundle->members[0],selected,switch_on_pickup);
}
/* Fixed role ordering: new utility, new ordinary, released old same-type member. */
static inline int uss_lifecycle_replace_commit_candidates(UssCoreLifecycle* life,unsigned thread,const UssCoreCandidates* bundle)
{
    return candidate_bundle_current(life,bundle) && bundle->names.count==3 &&
        uss_lifecycle_replace_commit(life,thread,&bundle->current,&bundle->members[0],&bundle->members[1],&bundle->members[2]);
}
#endif
