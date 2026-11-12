#ifdef NDEBUG
#error Core observation checks require active assertions
#endif
#define USS_CAPTURE_TEST_MAIN producer_regression_checks
#include "publication_capture_harness.c"
#include "../src/core_observation.h"
static UssPairOwner core_owner;
static UssCoreFlags core_flags;
static unsigned core_state,core_threads,core_thread_change,core_flag_change;
static uint32_t core_game_state(HfMod* mod){assert(mod==MOD);return core_state;}
static uint32_t core_thread(void* user)
{(void)user;++core_threads;if(core_flag_change && core_threads==2)core_flags.paused^=1;
 return core_thread_change && core_threads==2?8:7;}
static UssCoreObservation core_read(void)
{UssCorePlatform p={0};p.thread_id=core_thread;return uss_core_observe(&fake,MOD,&core_owner,&core_flags,&p,&last_reader);}
static int core_apply(UssCoreObservation* current)
{UssPairObservation pair;UssPublicationView view;UssPublicationOwner row;
 return uss_core_apply(&core_owner,7,current,&pair,&view,&row);}
static void core_fixture(void)
{
    static const unsigned char pin[32]={0x40,0x53,0x41,0x54,0x41,0x57,0x48,0x83,0xec,0x20,0x3b,0x15,0x50,0xb1,0xb0,0x02,0x45,0x0f,0xb6,0xf9,0x48,0x8b,0x1d,0x8d,0xe2,0x9a,0x02,0x45,0x8b,0xe0,0x0f,0x84};
    const uint32_t keys[2]={103,102},indexes[2]={0,1};unsigned char* b;unsigned i;
    capture_fixture();memset(&core_owner,0,sizeof(core_owner));memset(&core_flags,0,sizeof(core_flags));
    core_flags.known=core_flags.eligible=core_flags.formed=1;
    core_state=HF_STATE_MISSION;core_threads=core_thread_change=core_flag_change=0;fake.game_state=core_game_state;
    memcpy(segment(BASE+0x978AC0,32),pin,32);ptr_segment(BASE+0x3326D68,0x90000000);
    hash_rows(0x90014050,0x90000100,4,keys,indexes,2);
    b=fixture(0x90014050,40);putptr(b+24,0x90000200);putptr(b+32,0x90001000);
    b=segment(0x90000200,16);putptr(b,0x70002900);putptr(b+8,0x70001900);
    b=segment(0x90001000,2*0x370);
    for(i=0;i<2;++i){put32(b+i*0x370+0xA8,UINT32_MAX);put32(b+i*0x370+0x360,7);}
}
static void core_pair(void)
{
    UssCoreObservation current;UssPairMember utility={0},ordinary={0};
    current=core_read();assert(current.valid && !current.pair_matched);assert(core_apply(&current));
    assert(core_owner.ready && !core_owner.pair_present);
    utility.entity=103;ordinary.entity=102;
    memcpy(utility.identity,capture_input.utility_identity,8);memcpy(ordinary.identity,capture_input.ordinary_identity,8);
    assert(uss_pair_owner_set_pair(&core_owner,7,0,&utility,&ordinary));
    reads=module_calls=session_calls=core_threads=0;
}
#ifndef USS_CORE_TEST_MAIN
#define USS_CORE_TEST_MAIN main
#endif
int USS_CORE_TEST_MAIN(void)
{
    UssCoreObservation current,stale;UssPairObservation pair;UssPublicationView view;UssPublicationOwner row;
    uint64_t generation,guard;unsigned total,i;unsigned char* b;
    /* Existing standalone coordinator behavior has its own HF041 fixture. */
    (void)uss_pair_begin;(void)uss_pair_tick;
    assert(producer_regression_checks()==0);
    core_fixture();core_pair();current=core_read();
    if(!current.valid)fprintf(stderr,"Core baseline rejected: %s\n",last_reader.failure);
    assert(current.valid && current.pair_matched && current.primary.valid && current.primary.ready);
    total=reads;assert(last_reader.calls==total && total<=2165 && last_reader.bytes<=20000);
    printf("Core owned-pair observation: %u reads / %u bytes\n",total,last_reader.bytes);
    assert(!query_calls && !input_calls);
    assert(uss_core_apply(&core_owner,7,&current,&pair,&view,&row));
    assert(pair.valid && pair.pair_generation==core_owner.pair_generation && view.supported_pair);
    generation=core_owner.pair_generation;guard=core_owner.guard_generation;
    for(i=1;i<=total;++i){core_fixture();core_pair();fail_at=i;current=core_read();assert(!current.valid);
        assert(!core_apply(&current));assert(core_owner.pair_present && !core_owner.ready);
        assert(core_owner.pair_generation==generation && core_owner.guard_generation==guard+1);}
    core_fixture();core_pair();fail_at=1;current=core_read();assert(!core_apply(&current));
    fail_at=0;current=core_read();assert(current.valid && core_apply(&current));
    assert(core_owner.pair_generation==generation && core_owner.guard_generation==guard+1);
    core_fixture();core_pair();b=fixture(SESSION+0x162E0,16);b[0]=2;b[8]=1;
    current=core_read();assert(current.valid && current.pair_matched);
    core_fixture();initial_role=HF_SESSION_JOINER;fixture(SESSION+0xB3A8,8)[0]=2;core_pair();
    current=core_read();assert(current.valid && current.pair_matched && core_apply(&current));
    core_fixture();core_pair();initial_role=HF_SESSION_JOINER;fixture(SESSION+0xB3A8,8)[0]=2;
    current=core_read();assert(current.valid && !current.pair_matched);assert(core_apply(&current));assert(core_owner.pair_present);
    current=core_read();assert(current.valid && current.pair_matched && core_apply(&current));
    core_fixture();core_pair();fixture(SESSION+0xB3A8,8)[0]=2;assert(!core_read().valid);
    core_fixture();core_pair();put32(fixture(SESSION+0x162D8,4),1);
    current=core_read();assert(current.valid && current.context.local_only && core_apply(&current));
    current=core_read();assert(current.pair_matched && core_apply(&current));
    core_fixture();core_pair();fixture(SESSION+0x162E0,16)[8]=1;assert(!core_read().valid);
    core_fixture();core_pair();fixture(SESSION+0xB398,8)[0]=3;assert(!core_read().valid);
    core_fixture();core_pair();core_state=HF_STATE_NONE;current=core_read();assert(!current.valid);
    assert(!core_apply(&current) && core_owner.pair_present);
    core_fixture();core_pair();core_state=HF_STATE_SHIP;assert(!core_read().valid);
    put32(fixture(0xA0000000+0xAC21C,4),3);current=core_read();assert(current.valid && !current.mission);
    assert(core_apply(&current) && !core_owner.pair_present && !core_owner.mission);
    core_fixture();core_pair();fake.version=HF_MOD_SDK_VERSION+1;fake.size=(uint32_t)sizeof(HfApi)+8;
    current=core_read();assert(current.valid && current.pair_matched);
    core_fixture();core_pair();fake.version=11;fake.size=552;assert(core_read().valid);
    core_fixture();core_pair();fake.size=551;assert(!core_read().valid);
    core_fixture();core_pair();core_flags.known=0;assert(!core_read().valid);
    core_fixture();core_pair();core_flags.paused=core_flags.input=1;current=core_read();assert(current.valid);
    assert(uss_core_apply(&core_owner,7,&current,&pair,&view,&row));assert(pair.paused && pair.input);
    core_fixture();core_pair();put32(fixture(0x70000300,48)+28,3);put32(fixture(0x70000500,4),102);
    put32(fixture(ACTION_MANAGER+0x53DCD0,32)+4,3);current=core_read();assert(current.valid && !current.primary.ready);
    core_fixture();core_pair();put32(fixture(0x70000700,144)+96+4,0);
    current=core_read();assert(current.valid && !current.utility.parent_matches && current.utility.parent==0);
    core_fixture();core_pair();fixture(0x70002900,24)[0]^=1;assert(!core_read().valid);
    core_fixture();core_pair();change_session=1;assert(!core_read().valid);
    core_fixture();core_pair();module_change=1;assert(!core_read().valid);
    core_fixture();core_pair();core_thread_change=1;assert(!core_read().valid);
    core_fixture();core_pair();core_flag_change=1;assert(!core_read().valid);
    core_fixture();core_pair();stale=core_read();assert(stale.valid);
    assert(uss_pair_owner_clear_pair(&core_owner,7,core_owner.pair_generation));assert(!core_apply(&stale));assert(!core_owner.pair_present);
    core_fixture();core_pair();b=fixture(SESSION+0x162E0,32);memset(b,0,32);b[0]=4;b[8]=2;b[16]=1;b[24]=3;
    put32(fixture(SESSION+0x162D8,4),4);current=core_read();assert(current.valid && !current.pair_matched);
    assert(core_apply(&current) && core_owner.pair_present);
    current=core_read();assert(current.valid && current.pair_matched);
    core_fixture();core_pair();assert(!uss_observe_action_state(&fake,MOD,NULL).valid);
    assert(!uss_observe_primary_readiness(&fake,MOD,&capture_input.owner,NULL).valid);
    assert(uss_observe_pair_member(&fake,MOD,&capture_input.owner,103,capture_input.utility_identity,NULL).status!=USS_MEMBER_OBSERVED);
    puts("Core adapter: explicit lifecycle, unknown/exit distinction, stale snapshots, peers, idle readiness and partial-read refusals PASS");
    return 0;
}

