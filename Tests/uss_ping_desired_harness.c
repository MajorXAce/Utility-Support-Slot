/* Which weapon is excluded from pings (uss_ping_desired.h): only the carried, stored member of a coherent formed pair.
 * Hand-built observations; no game access. */
#ifdef NDEBUG
#error Ping desired checks require active assertions
#endif
#include "../src/uss_ping_desired.h"
#include <assert.h>
#include <stdio.h>

static const unsigned char SILO[8]={0xAA,0x79,0xCA,0x92,0x05,0x46,0x13,0xFC},OTHER[8]={9,8,7,6,5,4,3,2};
static UssCoreLifecycle life;static UssCoreObservation cur;
static void scene(unsigned real_is_utility)
{
    memset(&life,0,sizeof(life));memset(&cur,0,sizeof(cur));
    life.phase=USS_LIFE_FORMED;life.owner.pair_present=1;
    life.owner.utility.entity=103;memcpy(life.owner.utility.identity,SILO,8);
    life.owner.ordinary.entity=102;memcpy(life.owner.ordinary.identity,OTHER,8);
    cur.valid=cur.mission=cur.pair_matched=1;cur.context.local.owner=77;
    cur.inventory.slots[0]=201;cur.inventory.slots[1]=202;cur.inventory.slots[2]=real_is_utility?103:102;
    cur.utility.status=cur.ordinary.status=USS_MEMBER_OBSERVED;
    cur.utility.entity=103;cur.ordinary.entity=102;cur.utility.parent_matches=cur.ordinary.parent_matches=1;
    memcpy(cur.utility.descriptor,SILO,8);memcpy(cur.ordinary.descriptor,OTHER,8);
}
int main(void)
{
    UssPingDesired d;
    (void)uss_core_observe;(void)uss_core_apply;(void)inventory_sample;
    (void)uss_lifecycle_flags;(void)uss_lifecycle_observe;(void)uss_lifecycle_create;
    (void)uss_lifecycle_replace_begin;(void)uss_lifecycle_replace_commit;(void)uss_lifecycle_complete;
    /* Utility held in slot 2: the ORDINARY member is stored and excluded. */
    scene(1);assert(uss_ping_desired_from(&life,&cur,0,&d) && d.valid && d.entity==102 && !memcmp(d.identity,OTHER,8) && d.owner==77);
    /* Ordinary held: the UTILITY (Silo remote) is stored and excluded. */
    scene(0);assert(uss_ping_desired_from(&life,&cur,0,&d) && d.valid && d.entity==103 && !memcmp(d.identity,SILO,8) && d.owner==77);
    /* Every gate individually. */
    scene(0);cur.valid=0;assert(!uss_ping_desired_from(&life,&cur,0,&d) && !d.valid);
    scene(0);cur.mission=0;assert(!uss_ping_desired_from(&life,&cur,0,&d));
    scene(0);cur.pair_matched=0;assert(!uss_ping_desired_from(&life,&cur,0,&d));
    scene(0);life.owner.pair_present=0;assert(!uss_ping_desired_from(&life,&cur,0,&d));
    scene(0);life.phase=USS_LIFE_REPLACING;assert(!uss_ping_desired_from(&life,&cur,0,&d));
    scene(0);life.phase=USS_LIFE_PAUSED;assert(!uss_ping_desired_from(&life,&cur,0,&d));
    scene(0);life.phase=USS_LIFE_RESTORING;assert(!uss_ping_desired_from(&life,&cur,0,&d));
    scene(0);life.phase=USS_LIFE_PENDING;assert(!uss_ping_desired_from(&life,&cur,0,&d));
    scene(0);cur.flags.paused=1;assert(!uss_ping_desired_from(&life,&cur,0,&d));
    scene(0);cur.flags.disabled=1;assert(!uss_ping_desired_from(&life,&cur,0,&d));
    scene(0);cur.flags.suspended=1;assert(!uss_ping_desired_from(&life,&cur,0,&d));
    scene(0);assert(!uss_ping_desired_from(&life,&cur,1,&d));                      /* the loop is handling a pause */
    scene(0);cur.inventory.slots[0]=0;assert(!uss_ping_desired_from(&life,&cur,0,&d));   /* primary missing */
    scene(0);cur.inventory.slots[1]=0;assert(!uss_ping_desired_from(&life,&cur,0,&d));   /* secondary missing */
    scene(0);cur.inventory.slots[2]=555;assert(!uss_ping_desired_from(&life,&cur,0,&d)); /* slot 2 holds neither member */
    scene(0);cur.inventory.slots[2]=0;assert(!uss_ping_desired_from(&life,&cur,0,&d));
    /* The stored member must be observed, be the recorded entity, carried by the avatar and have the recorded identity. */
    scene(0);cur.utility.status=USS_MEMBER_EMPTY;assert(!uss_ping_desired_from(&life,&cur,0,&d));
    scene(0);cur.utility.status=USS_MEMBER_UNKNOWN;assert(!uss_ping_desired_from(&life,&cur,0,&d));
    scene(0);cur.utility.entity=999;assert(!uss_ping_desired_from(&life,&cur,0,&d));
    scene(0);cur.utility.parent_matches=0;assert(!uss_ping_desired_from(&life,&cur,0,&d));
    scene(0);cur.utility.descriptor[3]^=1;assert(!uss_ping_desired_from(&life,&cur,0,&d));
    /* Evidence of the REAL member does not matter for this decision (only the stored one is touched). */
    scene(0);cur.ordinary.status=USS_MEMBER_EMPTY;assert(uss_ping_desired_from(&life,&cur,0,&d) && d.entity==103);
    /* Bad arguments. */
    scene(0);assert(!uss_ping_desired_from(NULL,&cur,0,&d) && !uss_ping_desired_from(&life,NULL,0,&d) && !uss_ping_desired_from(&life,&cur,0,NULL));
    puts("USS ping desired: stored member only, every pair/policy/slot/evidence gate PASS");
    return 0;
}
