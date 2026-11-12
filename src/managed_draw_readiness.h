/* Explicit early-primary contract for one managed network draw. This is not
 * general readiness, initial formation, replacement or native-send authority. */
#ifndef USS_MANAGED_DRAW_READINESS_H
#define USS_MANAGED_DRAW_READINESS_H
#include "primary_readiness.h"
#include "pair_transaction.h"
static inline int uss_managed_draw_primary(const UssPairTransaction* work,const Inventory* owner,
    const UssPrimaryReadiness* primary,uint64_t now)
{
    const UssPairObservation* origin;const unsigned char* row;uint32_t block;
    if(!work || !owner || !primary || !primary->valid || work->phase!=USS_PAIR_ACTIVE ||
        work->order!=USS_PAIR_DRAW || work->step_count!=1 || work->index>1 || work->selected!=3 ||
        now<work->started_ms || now>=work->deadline_ms)return 0;
    origin=&work->origin;row=primary->fields.row;block=u32(row+28);
    if(!pair_member_equal(&work->steps[0].member,&origin->utility) &&
        !pair_member_equal(&work->steps[0].member,&origin->ordinary))return 0;
    return pair_scope(origin) && origin->formed && !origin->formation_pending && !origin->local_only &&
        origin->context.owner==owner->roots[0] && origin->context.inventory==owner->roots[2] &&
        origin->context.row_address==owner->row_address && origin->context.avatar==owner->avatar &&
        origin->context.primary==owner->slots[0] && origin->context.secondary==owner->slots[1] &&
        origin->context.backpack==owner->slots[3] && owner->slots[2]==work->expected_support &&
        (owner->slots[2]==origin->utility.entity || owner->slots[2]==origin->ordinary.entity) &&
        owner->active_selector==1 && owner->held==owner->slots[0] &&
        u32(row)==owner->avatar && u32(row+4)==1 && u32(row+8)==3 && u32(row+16)==2 &&
        u32(row+20)==1 && !u32(row+24) && block>0 && block<0x7f800000 &&
        ((u32(primary->fields.flags+12)>>15)&1) && !primary->fields.global_guard && !primary->fields.state_byte;
}
#endif
