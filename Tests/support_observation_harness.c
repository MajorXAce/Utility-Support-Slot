#ifdef NDEBUG
#error Observation checks require active assertions
#endif
/* Reuse the actual reader fake-SDK graph and its negative cases, not a mock reader. */
#define main inventory_regression_checks
#include "inventory_harness.c"
#undef main
#include "../src/support_observation.h"

static const char* other_build(void) { return "unsupported"; }
static void support_fixture(const unsigned char identity[8], uint32_t kind)
{
    distinct_slots();
    memcpy(fixture(0x70002900,24),identity,8);
    put32(fixture(0x70000700,144)+96+12,kind);
}
static UssSupportObservation observe(void)
{
    return uss_observe_support(&fake,MOD,&last_reader);
}
static void assert_invalid(UssSupportObservation result)
{
    static const unsigned char zero[8]={0};
    assert(result.status==USS_SUPPORT_INVALID && !result.entity && !result.observed_kind);
    assert(!memcmp(result.identity,zero,8));
}
int main(void)
{
    const unsigned char silo[8]={0xaa,0x79,0xca,0x92,0x05,0x46,0x13,0xfc};
    const unsigned char c4[8]={0x3d,0x2f,0xf5,0x21,0x63,0x0d,0xf5,0x51};
    const unsigned char ordinary[8]={1,2,3,4,5,6,7,8};
    UssSupportObservation result; Inventory inventory; unsigned total,i;
    assert(inventory_regression_checks()==0);
    setup();result=observe();assert(result.status==USS_SUPPORT_ABSENT && !result.entity);
    setup();put32(fixture(0x70000300,48)+8,UINT32_MAX);
    result=observe();assert(result.status==USS_SUPPORT_ABSENT);
    support_fixture(silo,2);result=observe();
    assert(result.status==USS_SUPPORT_UTILITY && result.entity==103 && result.observed_kind==2);
    assert(!memcmp(result.identity,silo,8)); total=reads;
    support_fixture(c4,2);result=observe();assert(result.status==USS_SUPPORT_UTILITY);
    support_fixture(ordinary,2);result=observe();assert(result.status==USS_SUPPORT_ORDINARY);
    support_fixture(silo,1);result=observe();
    assert(result.status==USS_SUPPORT_UNSUPPORTED && result.entity==103);
    support_fixture(silo,2);put32(fixture(0x70002900,24)+8,999);
    assert_invalid(observe());
    /* Failed reads, including partial writes, must not yield a candidate. */
    for(i=1;i<=total;++i){support_fixture(silo,2);fail_at=i;assert_invalid(observe());}
    support_fixture(silo,2);module_change=1;assert_invalid(observe());
    assert(last_reader.final_reasons==4);
    support_fixture(silo,2);change_session=1;assert_invalid(observe());
    assert(!strcmp(last_reader.failure,"SESSION_CHANGED"));
    support_fixture(silo,2);change_session=2;assert_invalid(observe());
    support_fixture(silo,2);initial_role=HF_SESSION_HOST;assert_invalid(observe());assert(!reads);
    support_fixture(silo,2);fake.size=(uint32_t)USS_REQUIRED_API_BYTES-1;assert_invalid(observe());assert(!reads);
    support_fixture(silo,2);fake.build_id=other_build;assert_invalid(observe());assert(!reads);
    assert_invalid(uss_observe_support(NULL,MOD,NULL));
    /* Adapter defenses independently reject an inconsistent copied candidate. */
    support_fixture(silo,2);assert(sample(&inventory));
    inventory.identities[2][8]^=1;assert_invalid(support_from_inventory(&inventory));
    inventory.identities[2][8]^=1;inventory.slots[2]++;
    assert_invalid(support_from_inventory(&inventory));
    puts("typed support observation: actual reader, absent/unsupported/classes, identity/context/read failures PASS");
    return 0;
}

