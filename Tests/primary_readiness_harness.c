#ifdef NDEBUG
#error Primary readiness checks require active assertions
#endif
#ifndef USS_FIXTURE_SEGMENTS
#define USS_FIXTURE_SEGMENTS 48
#endif
#define USS_ACTION_TEST_MAIN action_regression_checks
#include "action_state_harness.c"
#include "../src/primary_readiness.h"
#define GAME_STATE ((uintptr_t)0xA0000000)
static Inventory primary_owner;
static unsigned primary_change,primary_hits;
static int primary_read(uintptr_t address,void* output,size_t bytes)
{
    int status=fake_read(address,output,bytes);
    uintptr_t target=primary_change==1 ? ACTION_MANAGER+0x53DCD0 :
                     primary_change==2 ? ACTION_MANAGER+0x53E880 :
                     primary_change==3 ? BASE+0x3326340 : GAME_STATE+0xAC218;
    if (!status && primary_change && address==target &&
        (primary_change!=1 || bytes==32) && ++primary_hits==2) {
        if(primary_change==1) put32((unsigned char*)output+28,1);
        else if(primary_change==2) ((unsigned char*)output)[0]^=1;
        else if(primary_change==3) putptr(output,GAME_STATE+0x1000);
        else put32(output,1);
    }
    return status;
}
static void primary_refresh(void)
{
    assert(sample(&primary_owner));
    reads=module_calls=session_calls=0;memset(&last_reader,0,sizeof(last_reader));
}
static void primary_fixture(void)
{
    static const unsigned char pin[30]={
        0x48,0x8b,0x0d,0x15,0x23,0x1e,0x03,0x48,0xbf,0xf3,0x9d,0xe1,0x38,0x8c,0x41,
        0xe2,0x12,0x48,0x81,0xc1,0x58,0xfa,0x01,0x00,0x48,0x8b,0xd7,0x41,0xb1,0x01};
    unsigned char* row;
    action_fixture();fake.read=primary_read;primary_change=primary_hits=0;
    put32(fixture(0x70000300,48)+4,101);put32(fixture(0x70000300,48)+28,1);
    row=fixture(ACTION_MANAGER+0x53DCD0,32);put32(row+4,1);
    memcpy(segment(BASE+0x144024,30),pin,30);
    putptr(segment(BASE+0x3326340,8),GAME_STATE);
    put32(segment(GAME_STATE+0xAC218,4),0);
    segment(ACTION_MANAGER+0x53E880,24);
    segment(ACTION_MANAGER+0x53EAE0,1);
    /* Historical wrong-base regression: this unrelated address is NONZERO. */
    put32(segment(BASE+0xAC218,4),92799782);
    primary_refresh();
}
static UssPrimaryReadiness observe_primary(void)
{return uss_observe_primary_readiness(&fake,MOD,&primary_owner,&last_reader);}
static void unknown_primary(UssPrimaryReadiness result)
{UssPrimaryReadiness empty={0};assert(!memcmp(&empty,&result,sizeof(result)));}
static void not_ready(UssPrimaryReadiness result)
{assert(result.valid && !result.ready);}
static void predicate_cases(void)
{
    /* Literal expected outcomes for the two accepted shapes and nearby rejects. */
    static const struct {uint32_t phase,prior,pending,timer,bit;unsigned ready;} cases[]={
        {0,0,0,0,0,1},{0,3,1,0,0,1},{0,0,0,1,0,0},{0,0,0,0,1,0},
        {1,2,1,1,1,1},{1,3,1,0x3f800000,1,1},{1,2,1,0x7f7fffff,1,1},
        {1,1,1,1,1,0},{1,0,1,1,1,0},{1,2,0,1,1,0},{1,2,2,1,1,0},
        {1,2,1,0,1,0},{1,2,1,1,0,0},{1,2,1,0x7f800000,1,0},
        {1,2,1,0x7fc00000,1,0},{1,2,1,0xbf800000,1,0},{2,3,1,1,1,0},
        {3,2,1,1,1,0}
    };
    unsigned i;
    for(i=0;i<sizeof(cases)/sizeof(cases[0]);++i) {
        UssPrimaryReadiness result={0};Inventory owner={0};
        owner.avatar=22;owner.slots[0]=101;owner.active_selector=1;owner.held=101;
        put32(result.fields.row,22);put32(result.fields.row+4,1);
        put32(result.fields.row+8,cases[i].prior);put32(result.fields.row+16,cases[i].phase);
        put32(result.fields.row+20,cases[i].pending);put32(result.fields.row+24,cases[i].timer);
        put32(result.fields.flags+12,cases[i].bit<<15);
        primary_predicate(&result,&owner);assert(result.ready==cases[i].ready);
    }
}
#ifndef USS_PRIMARY_TEST_MAIN
#define USS_PRIMARY_TEST_MAIN main
#endif
int USS_PRIMARY_TEST_MAIN(void)
{
    UssPrimaryReadiness result;unsigned total,i;unsigned char* row;
    assert(action_regression_checks()==0);predicate_cases();
    primary_fixture();result=observe_primary();
    assert(result.valid && result.ready && result.settled && !result.holstering);
    assert(result.fields.action.switch_ready && result.fields.game_state==GAME_STATE);
    total=reads;printf("primary readiness baseline: %u reads / %u bytes\n",total,last_reader.bytes);
    assert(total==last_reader.calls && total<=2165 && last_reader.bytes<=20000);
    for(i=1;i<=total;++i){primary_fixture();fail_at=i;unknown_primary(observe_primary());}
    /* Action index1 must never be replaced with owner/inventory index0. */
    primary_fixture();put32(fixture(ACTION_MANAGER+0x70,4),2);
    put32(fixture(0x80001000,8)+4,1);
    putptr(segment(ACTION_MANAGER+0x118,8),0x80002000);
    row=segment(ACTION_MANAGER+0x53DCD0+0x1238,32);put32(row,22);put32(row+4,1);
    segment(ACTION_MANAGER+0x53E880+0x1238,24);
    segment(ACTION_MANAGER+0x53EAE0+0x1238,1);
    result=observe_primary();assert(result.valid && result.ready && result.fields.action.index==1);
    primary_fixture();last_reader.calls=10;last_reader.bytes=100;
    result=observe_primary();assert(result.valid && result.ready && last_reader.calls==total+10);
    assert(last_reader.bytes==4891);
    /* can_switch can stay true while publication readiness is false. */
    primary_fixture();put32(fixture(ACTION_MANAGER+0x53DCD0,32)+4,2);
    result=observe_primary();not_ready(result);assert(result.fields.action.switch_ready);
    primary_fixture();put32(fixture(ACTION_MANAGER+0x53DCD0,32)+28,1);not_ready(observe_primary());
    primary_fixture();put32(fixture(GAME_STATE+0xAC218,4),1);not_ready(observe_primary());
    primary_fixture();*fixture(ACTION_MANAGER+0x53EAE0,1)=1;not_ready(observe_primary());
    primary_fixture();put32(fixture(ACTION_MANAGER+0x53E880,24)+12,1u<<15);not_ready(observe_primary());
    primary_fixture();put32(fixture(0x70000300,48)+28,2);primary_refresh();not_ready(observe_primary());
    primary_fixture();put32(fixture(0x70000500,4),0);primary_refresh();not_ready(observe_primary());
    primary_fixture();put32(fixture(0x70000300,48),0);primary_refresh();not_ready(observe_primary());
    primary_fixture();row=fixture(ACTION_MANAGER+0x53DCD0,32);
    put32(row+8,3);put32(row+16,1);put32(row+20,1);put32(row+24,0x3f800000);
    put32(fixture(ACTION_MANAGER+0x53E880,24)+12,1u<<15);
    result=observe_primary();assert(result.valid && result.ready && result.holstering && !result.settled);
    for(i=1;i<=4;++i){primary_fixture();primary_change=i;unknown_primary(observe_primary());}
    primary_fixture();fixture(BASE+0x144024,30)[0]^=1;unknown_primary(observe_primary());
    primary_fixture();fixture(BASE+0xA93ECD,7)[0]^=1;unknown_primary(observe_primary());
    primary_fixture();put32(fixture(ACTION_MANAGER+0x53DCD0,32),23);unknown_primary(observe_primary());
    primary_fixture();put32(fixture(0x80002008,4),23);unknown_primary(observe_primary());
    primary_fixture();putptr(fixture(BASE+0x3326340,8),0);unknown_primary(observe_primary());
    primary_fixture();primary_owner.avatar=23;unknown_primary(observe_primary());
    primary_fixture();module_change=1;unknown_primary(observe_primary());
    primary_fixture();module_change=2;unknown_primary(observe_primary());
    primary_fixture();change_session=1;unknown_primary(observe_primary());
    primary_fixture();change_session=2;unknown_primary(observe_primary());
    primary_fixture();initial_role=HF_SESSION_HOST;unknown_primary(observe_primary());assert(!reads);
    primary_fixture();last_reader.bytes=19999;unknown_primary(observe_primary());assert(reads<=1);
    primary_fixture();last_reader.bytes=20001;unknown_primary(observe_primary());assert(!reads);
    primary_fixture();last_reader.calls=2165;unknown_primary(observe_primary());assert(!reads);
    primary_fixture();fake.version=8;unknown_primary(observe_primary());assert(!reads);
    primary_fixture();fake.size=0;unknown_primary(observe_primary());assert(!reads);
    primary_fixture();fake.read=NULL;unknown_primary(observe_primary());assert(!reads);
    unknown_primary(uss_observe_primary_readiness(NULL,MOD,&primary_owner,NULL));
    puts("primary readiness: settled/holstering, correct object guard, context stability and every partial failure PASS");
    return 0;
}
