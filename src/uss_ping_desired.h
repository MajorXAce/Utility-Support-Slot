/* Which weapon should be excluded from pings right now: the STORED member of a coherent, formed, unpaused pair, and nothing
 * otherwise (Lua: pair and check_pair(p) and primary/secondary present and not paused, then the member that is not the real
 * support entry, still carried by the avatar). Pure decision over an observation already read; no reads, no effects. */
#ifndef USS_PING_DESIRED_H
#define USS_PING_DESIRED_H
#include "core_lifecycle.h"
#include "uss_ping_filter.h"

static inline int uss_ping_desired_from(const UssCoreLifecycle* life,const UssCoreObservation* current,unsigned handling_paused,
    UssPingDesired* out)
{
    const UssPairOwner* owner;const UssMemberEvidence* stored;const UssPairMember* member;
    if(!out)return 0;
    memset(out,0,sizeof(*out));
    if(!life || !current || handling_paused)return 0;
    owner=&life->owner;
    if(!current->valid || !current->mission || !current->pair_matched || !owner->pair_present || life->phase!=USS_LIFE_FORMED ||
        current->flags.paused || current->flags.disabled || current->flags.suspended ||
        !publication_id(current->inventory.slots[0]) || !publication_id(current->inventory.slots[1]))return 0;
    if(current->inventory.slots[2]==owner->utility.entity){stored=&current->ordinary;member=&owner->ordinary;}
    else if(current->inventory.slots[2]==owner->ordinary.entity){stored=&current->utility;member=&owner->utility;}
    else return 0;
    if(stored->status!=USS_MEMBER_OBSERVED || stored->entity!=member->entity || !stored->parent_matches ||
        memcmp(stored->descriptor,member->identity,8))return 0;
    out->valid=1;out->entity=member->entity;memcpy(out->identity,member->identity,8);out->owner=current->context.local.owner;
    return 1;
}
#endif
