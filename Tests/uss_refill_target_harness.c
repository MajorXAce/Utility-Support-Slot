/* Reuse the tested native-layout fixture, then connect actual target reads. */
#define main amount_fixture_checks
#include "uss_refill_amount_harness.c"
#undef main
#include "../src/uss_refill_target.h"
static UssRefillPairView pair;static unsigned observations,mutate;
static int observe_pair(void* p,UssRefillPairView* out)
{(void)p;++observations;*out=pair;if(observations==2 && mutate)++out->pair_generation;return 1;}
static void pair_fixture(void)
{
    static const unsigned char utility[8]={0xaa,0x79,0xca,0x92,0x05,0x46,0x13,0xfc};
    amount_fixture(1,0);memset(&pair,0,sizeof(pair));observations=mutate=0;
    pair.epoch=pair.context_generation=pair.pair_generation=pair.guard_generation=1;
    pair.owner=20;pair.inventory=21;pair.avatar=2;pair.primary=10;pair.secondary=11;
    pair.support=pair.utility=4;pair.ordinary=5;pair.utility_parent=pair.ordinary_parent=2;
    memcpy(pair.utility_identity,utility,8);pair.ordinary_identity[0]=7;
    pair.valid=pair.mission=pair.ready=pair.formed=1;
}
int main(void)
{
    UssRefillMemoryIo memory={NULL,0x10000000,read_memory,context};UssRefillTargetIo io={NULL,observe_pair,&memory};
    UssRefillTarget target,copy;unsigned i;assert(!amount_fixture_checks());
    pair_fixture();assert(uss_refill_target_read(&io,&target) && target.target==5 && target.direct_amount==3 && target.pair_generation==1);
    copy=target;++copy.pair_generation;assert(!uss_refill_target_same(&target,&copy,1));
    for(i=0;i<8;++i){pair_fixture();
        if(i==0)pair.support=5;if(i==1)pair.ordinary_parent=0;if(i==2)pair.busy=1;if(i==3)pair.down=1;
        if(i==4)pair.primary=0;if(i==5)pair.pair_generation=0;if(i==6)mutate=1;if(i==7)pair.ordinary_identity[0]=8;
        memset(&target,0xa5,sizeof(target));copy=target;assert(!uss_refill_target_read(&io,&target) && !memcmp(&target,&copy,sizeof(target)));}
    pair_fixture();{static const unsigned char expendable[8]={0xd3,0x01,0x69,0xed,0xa0,0x2f,0x93,0x80};
        memcpy(pair.ordinary_identity,expendable,8);assert(!uss_refill_pair_eligible(&pair));}
    /* A run-time clone of an expendable (for example EAT-77, a copy of the EAT-17) is excluded too; others still pass. */
    pair_fixture();{unsigned char clone[8]={0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88};
        uss_derived_support_reset();
        memcpy(pair.ordinary_identity,clone,8);assert(uss_refill_pair_eligible(&pair));
        assert(uss_derived_support_add(clone,USS_SUPPORT_TYPE_EXPENDABLE) && !uss_refill_pair_eligible(&pair));
        uss_derived_support_reset();assert(uss_refill_pair_eligible(&pair));}
    puts("USS refill target: concrete ammo/settings, pair lifetime, attachment, busy/down, active ordinary and expendable exclusion PASS");return 0;
}
