#ifdef NDEBUG
#error Action-state checks require active assertions
#endif
#define main inventory_regression_checks
#include "inventory_harness.c"
#undef main
#include "../src/action_state.h"
#define ACTION_MANAGER ((uintptr_t)0x80000000)
static unsigned action_change,action_hits;
static int action_read(uintptr_t address,void* output,size_t bytes)
{
    int status=fake_read(address,output,bytes);
    uintptr_t target=action_change==1 ? ACTION_MANAGER+0x53DCD0 : ACTION_MANAGER+0x70;
    if (!status && action_change && address==target && ++action_hits==2) {
        if(action_change==1) put32((unsigned char*)output+4,99);
        else put32(output,2);
    }
    return status;
}
static void action_fixture(void)
{
    static const unsigned char pin[]={0x4c,0x8b,0x35,0x4c,0x2e,0x89,0x02};
    setup();action_change=action_hits=0;fake.read=action_read;
    memcpy(segment(BASE+0xA93ECD,7),pin,7);
    putptr(segment(BASE+0x3326D20,8),ACTION_MANAGER);
    put32(segment(ACTION_MANAGER+0x70,4),1);
    table(ACTION_MANAGER+0xF8,0x80001000,22);
    putptr(segment(ACTION_MANAGER+0x110,8),0x80002000);
    put32(segment(0x80002008,4),22);
    put32(segment(ACTION_MANAGER+0x53DCD0,24),22);
}
static UssActionState observe_action(void)
{return uss_observe_action_state(&fake,MOD,&last_reader);}
static void invalid_action(UssActionState result)
{assert(!result.valid && !result.record && !result.manager && !result.context);}
#ifndef USS_ACTION_TEST_MAIN
#define USS_ACTION_TEST_MAIN main
#endif
int USS_ACTION_TEST_MAIN(void)
{
    UssActionState result;unsigned total,i;
    assert(inventory_regression_checks()==0);
    action_fixture();result=observe_action();
    assert(result.valid && !result.switch_ready && result.avatar==22);
    assert(result.record==ACTION_MANAGER+0x53DCD0 && result.index==0 && result.count==1);
    total=reads;assert(total<=2165 && last_reader.bytes<=20000);
    action_fixture();put32(fixture(0x70000300,48)+4,101);
    put32(fixture(0x70000300,48)+28,1);
    result=observe_action();assert(result.valid && result.switch_ready);
    /* Owner/inventory indices are0; action table index1 must stay distinct. */
    action_fixture();put32(fixture(ACTION_MANAGER+0x70,4),2);
    put32(fixture(0x80001000,8)+4,1);
    putptr(segment(ACTION_MANAGER+0x118,8),0x80002000);
    put32(segment(ACTION_MANAGER+0x53DCD0+0x1238,24),22);
    result=observe_action();assert(result.valid && result.index==1 && result.record==ACTION_MANAGER+0x53DCD0+0x1238);
    for(i=1;i<=total;++i){action_fixture();fail_at=i;invalid_action(observe_action());}
    action_fixture();fixture(BASE+0xA93ECD,7)[0]^=1;invalid_action(observe_action());
    action_fixture();put32(fixture(ACTION_MANAGER+0x70,4),4097);invalid_action(observe_action());
    action_fixture();put32(fixture(0x80001000,8)+4,1);invalid_action(observe_action());
    action_fixture();put32(fixture(0x80002008,4),23);invalid_action(observe_action());
    action_fixture();put32(fixture(ACTION_MANAGER+0x53DCD0,24),23);invalid_action(observe_action());
    action_fixture();change_at=total;invalid_action(observe_action());
    action_fixture();action_change=1;invalid_action(observe_action());
    assert(!strcmp(last_reader.failure,"ACTION_CONTEXT_CHANGED"));
    action_fixture();action_change=2;invalid_action(observe_action());
    assert(!strcmp(last_reader.failure,"ACTION_CONTEXT_CHANGED"));
    action_fixture();module_change=1;invalid_action(observe_action());
    action_fixture();change_session=1;invalid_action(observe_action());
    action_fixture();change_session=2;invalid_action(observe_action());
    action_fixture();initial_role=HF_SESSION_HOST;invalid_action(observe_action());assert(!reads);
    invalid_action(uss_observe_action_state(NULL,MOD,NULL));
    puts("action-state resolver: fixed pin, independent index, record/context identity, every read failure PASS");
    return 0;
}
