#ifdef NDEBUG
#error Guarded selection checks require active assertions
#endif
#define USS_ACTION_TEST_MAIN action_regression_checks
#include "action_state_harness.c"
#undef USS_ACTION_TEST_MAIN
#define main native_regression_checks
#include "native_calls_harness.c"
#undef main
#include "../src/guarded_selection.h"

static unsigned selection_mode,selection_count,selection_events[2];
static void selection_state(void* context,uint32_t slot,uint32_t zero)
{
    assert((uintptr_t)context==ACTION_MANAGER+0x53DCD0 && zero==0);
    assert(selection_count==0);selection_events[selection_count++]=1;
    put32(fixture(ACTION_MANAGER+0x53DCD0,24)+4,slot);
}
static void selection_weapon(void* inventory,uint32_t avatar,uint32_t slot)
{
    assert((uintptr_t)inventory==0x40000000 && avatar==22);
    assert(selection_count==1);selection_events[selection_count++]=2;
    if(selection_mode==0) return; /* Calls returned, inventory did not select. */
    put32(fixture(0x70000300,48)+28,slot);
    put32(fixture(0x70000500,4),selection_mode==2?101:u32(fixture(0x70000300,48)+4*(slot-1)));
    if(selection_mode==3) fail_at=reads+1;
    if(selection_mode==4) fixture(0x70002900,24)[0]^=1; /* Same handle, new type. */
    if(selection_mode==5) {
        memcpy(segment(0x90001000,48),fixture(0x70000300,48),48);
        putptr(fixture(0x40000000+64,24)+16,0x90001000);
    }
    if(selection_mode==6) change_session=1;
}
static UssNativeCalls selection_targets(void)
{
    UssNativeCalls targets={0};
    targets.select_state=selection_state;targets.select_weapon=selection_weapon;
    return targets;
}
static UssActionState prepare_selection(void)
{
    UssActionState expected;
    action_fixture();
    /* Distinct primary/secondary/support entities reveal stale held state. */
    {
        unsigned char* bytes;unsigned k;
        bytes=fixture(0x70000300,48);put32(bytes+4,102);put32(bytes+8,103);put32(bytes+28,1);
        bytes=fixture(0x60000000,72);put32(bytes,3);put32(bytes+12,3);put32(bytes+40,4);
        bytes=fixture(0x70000800,32);memset(bytes,0xFF,32);
        for(k=0;k<3;++k){put32(bytes+8*(k+1),101+k);put32(bytes+8*(k+1)+4,k);}
        bytes=fixture(0x70000600,24);
        for(k=0;k<3;++k)putptr(bytes+8*k,0x70000900+(uintptr_t)k*0x1000);
        bytes=fixture(0x70000700,144);memset(bytes,0,144);
        put32(bytes+12,1);put32(bytes+48+12,3);put32(bytes+96+12,2);
        bytes=segment(0x70001900,24);put32(bytes,998);put32(bytes+8,102);
        bytes=segment(0x70002900,24);put32(bytes,999);put32(bytes+8,103);
    }
    expected=observe_action();assert(expected.valid && expected.switch_ready);
    reads=module_calls=session_calls=action_hits=0;
    selection_count=0;selection_mode=1;
    return expected;
}
#ifndef USS_SELECTION_TEST_MAIN
#define USS_SELECTION_TEST_MAIN main
#endif
int USS_SELECTION_TEST_MAIN(void)
{
    UssActionState expected;
    UssNativeCalls targets=selection_targets(),missing;
    UssSelectionAttempt selection_attempt;
    UssSelectionResult result;
    unsigned slot,i,total,entered_reads;
    assert(action_regression_checks()==0);
    assert(native_regression_checks()==0);
    for(slot=1;slot<=3;++slot) {
        expected=prepare_selection();selection_attempt.consumed=0;
        result=uss_select_guarded(&fake,MOD,&targets,&expected,slot,&selection_attempt);
        assert(result.outcome==USS_SELECTION_INDEX_VERIFIED && result.held_matches);
        assert(result.entered && result.calls_returned && selection_attempt.consumed);
        assert(selection_count==2 && selection_events[0]==1 && selection_events[1]==2);
        assert(result.reads==reads && reads<=2165 && result.bytes<=20000);
        total=reads;
    }
    for(i=0;i<=6;++i) if(i!=1) {
        expected=prepare_selection();selection_attempt.consumed=0;selection_mode=i;
        result=uss_select_guarded(&fake,MOD,&targets,&expected,3,&selection_attempt);
        assert(selection_count==2 && selection_attempt.consumed && result.entered && result.calls_returned);
        if(i==2) assert(result.outcome==USS_SELECTION_INDEX_VERIFIED && !result.held_matches);
        else assert(result.outcome==USS_SELECTION_RETURNED_UNVERIFIED);
        entered_reads=reads;
        result=uss_select_guarded(&fake,MOD,&targets,&expected,3,&selection_attempt);
        assert(result.outcome==USS_SELECTION_NOT_ENTERED && reads==entered_reads && selection_count==2);
    }
    /* Every exact read failure must prevent entry or leave returned/unverified.
     * A failed post-read never causes another call or a compensating selection. */
    for(i=1;i<=total;++i) {
        expected=prepare_selection();selection_attempt.consumed=0;fail_at=i;
        result=uss_select_guarded(&fake,MOD,&targets,&expected,3,&selection_attempt);
        assert(result.outcome!=USS_SELECTION_INDEX_VERIFIED);
        assert(selection_count==0 || selection_count==2);
        assert(selection_attempt.consumed==(unsigned)(selection_count==2));
    }
    expected=prepare_selection();selection_attempt.consumed=0;expected.inventory.row_address+=48;
    result=uss_select_guarded(&fake,MOD,&targets,&expected,3,&selection_attempt);
    assert(!result.entered && !selection_attempt.consumed && !selection_count);
    expected=prepare_selection();selection_attempt.consumed=0;expected.session_version++;
    result=uss_select_guarded(&fake,MOD,&targets,&expected,3,&selection_attempt);assert(!result.entered && !selection_count);
    expected=prepare_selection();selection_attempt.consumed=0;put32(fixture(0x70000300,48)+8,0);
    result=uss_select_guarded(&fake,MOD,&targets,&expected,3,&selection_attempt);assert(!result.entered && !selection_count);
    expected=prepare_selection();selection_attempt.consumed=0;
    missing=targets;missing.select_weapon=NULL;
    result=uss_select_guarded(&fake,MOD,&missing,&expected,3,&selection_attempt);
    assert(!result.entered && !reads && !selection_count && !selection_attempt.consumed);
    result=uss_select_guarded(&fake,MOD,&targets,&expected,4,&selection_attempt);
    assert(!result.entered && !reads && !selection_count);
    /* One shared budget, not a fresh allowance for each phase. */
    {Reader budget={0};budget.calls=2165;assert(!action_state_sample(&fake,MOD,&budget).valid);assert(budget.calls==2165);}
    puts("guarded selection: context/row identity, dual-call order, selected vs held, no retry/rollback, all read failures PASS");
    return 0;
}
