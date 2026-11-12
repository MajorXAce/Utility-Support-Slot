#ifdef NDEBUG
#error Candidate assertions required
#endif
#define USS_CORE_TEST_MAIN observation_regression_checks
#include "core_observation_harness.c"
#include "../src/candidate_lifecycle.h"
static UssCoreLifecycle candidate_life;
static UssCoreCandidateNames request;
static UssCoreFlags candidate_flags;
static unsigned candidate_change,candidate_hits,candidate_started,candidate_request_change;
static uint32_t candidate_thread(void* user)
{uint32_t result=core_thread(user);if(core_flag_change && core_threads==2)candidate_flags.paused^=1;
 if(candidate_request_change && core_threads==2)request.members[0].identity[0]^=1;return result;}
static UssPairMember candidate_id(uint32_t id)
{
    UssPairMember result={0};result.entity=id;
    memcpy(result.identity,fixture(id==103?0x70002900:id==104?0x70003900:0x70001900,24),8);return result;
}
static int changing_candidate_read(uintptr_t address,void* output,size_t size)
{
    int status=fake_read(address,output,size);
    if(!status && address==BASE+0x978AC0)candidate_started=1;
    if(!status && candidate_change && candidate_started && address==0x70000760 && ++candidate_hits==2)put32((unsigned char*)output+4,0);
    return status;
}
static UssCoreCandidates candidates(void)
{
    UssCorePlatform platform={0};candidate_flags=uss_lifecycle_flags(&candidate_life,&core_flags);
    platform.thread_id=candidate_thread;
    return uss_core_observe_candidates(&fake,MOD,&candidate_life.owner,&candidate_flags,&platform,&request,&last_reader);
}
static void candidate_fixture_role(unsigned owned,unsigned role)
{
    UssCoreObservation current;UssCorePlatform platform={0};UssCoreFlags flags;
    UssPairObservation observation;UssPublicationView view;UssPublicationOwner row;UssPairMember names[2];
    core_fixture();memset(&candidate_life,0,sizeof(candidate_life));candidate_change=candidate_hits=candidate_started=candidate_request_change=0;
    initial_role=role;if(role==HF_SESSION_JOINER)fixture(SESSION+0xB3A8,8)[0]=2;
    if(!owned){put32(fixture(0x70000300,48)+8,103);put32(fixture(0x70000700,144)+48+4,0);}
    platform.thread_id=core_thread;flags=uss_lifecycle_flags(&candidate_life,&core_flags);
    current=uss_core_observe(&fake,MOD,&candidate_life.owner,&flags,&platform,NULL);
    assert(current.valid && uss_lifecycle_observe(&candidate_life,7,&current,&observation,&view,&row));
    names[0]=candidate_id(103);names[1]=candidate_id(102);
    if(owned){assert(uss_pair_owner_set_pair(&candidate_life.owner,7,0,&names[0],&names[1]));
        candidate_life.generation=candidate_life.owner.pair_generation;candidate_life.phase=USS_LIFE_FORMED;}
    assert(uss_core_candidate_names(&candidate_life.owner,names,2,&request));
    fake.read=changing_candidate_read;reads=module_calls=session_calls=core_threads=0;
}
static void candidate_fixture(unsigned owned){candidate_fixture_role(owned,HF_SESSION_HOST);}
static void extra_candidate(void)
{
    unsigned char* b;const unsigned char c4[8]={0x3d,0x2f,0xf5,0x21,0x63,0x0d,0xf5,0x51};
    b=fixture(0x60000000,72);put32(b,4);put32(b+12,4);
    b=fixture(0x70000800,32);put32(b,104);put32(b+4,3);
    putptr(fixture(0x70000600,32)+24,0x70003900);
    b=segment(0x70003900,24);memcpy(b,c4,8);put32(b+8,104);
    b=fixture(0x70000700,192);put32(b+144+4,22);put32(b+144+12,2);
    b=fixture(0x90000100,32);put32(b,104);put32(b+4,2);
    putptr(fixture(0x90000200,24)+16,0x70003900);
    b=fixture(0x90001000,3*0x370);put32(b+2*0x370+0xA8,UINT32_MAX);put32(b+2*0x370+0x360,7);
}
#ifndef USS_CANDIDATE_TEST_MAIN
#define USS_CANDIDATE_TEST_MAIN main
#endif
int USS_CANDIDATE_TEST_MAIN(void)
{
    UssCoreCandidates bundle,stale;UssPairMember names[3];UssCoreCandidateNames bad;
    unsigned total,i;uint64_t generation;UssCorePlatform platform={0};UssCoreFlags flags;
    /* These operations belong to the lifecycle fixture; keep this header user narrow. */
    (void)uss_lifecycle_complete;(void)uss_lifecycle_cancel;(void)uss_lifecycle_remove;
    assert(observation_regression_checks()==0);
    candidate_fixture(0);bundle=candidates();assert(bundle.valid && bundle.current.valid && !bundle.current.pair_matched);
    assert(bundle.members[0].entity==103 && bundle.members[1].entity==102 && bundle.members[1].parent==0);
    total=reads;assert(total==last_reader.calls && total<=2165 && last_reader.bytes<=20000);
    printf("Candidate pre-pair baseline: %u reads / %u bytes\n",total,last_reader.bytes);
    assert(!query_calls && !input_calls && !candidate_life.owner.pair_present);
    assert(uss_lifecycle_create_candidates(&candidate_life,7,&bundle,USS_PAIR_ORDINARY_FIRST,1,0));
    assert(candidate_life.phase==USS_LIFE_PENDING && !uss_lifecycle_flags(&candidate_life,&core_flags).formed);
    assert(!uss_lifecycle_create_candidates(&candidate_life,7,&bundle,USS_PAIR_ORDINARY_FIRST,1,0));
    for(i=1;i<=total;++i){candidate_fixture(0);fail_at=i;bundle=candidates();assert(!bundle.valid && !bundle.current.valid);
        assert(!candidate_life.owner.pair_present);}
    candidate_fixture(0);candidate_change=1;assert(!candidates().valid);
    candidate_fixture(0);fixture(0x70002900,24)[0]^=1;assert(!candidates().valid);
    candidate_fixture(0);putptr(fixture(BASE+0x346BF98,8),MAPPING+8);assert(!candidates().valid);
    candidate_fixture(0);core_thread_change=1;assert(!candidates().valid);
    candidate_fixture(0);core_flag_change=1;assert(!candidates().valid);
    candidate_fixture(0);candidate_request_change=1;assert(!candidates().valid);
    candidate_fixture_role(0,HF_SESSION_JOINER);assert(candidates().valid);
    candidate_fixture(0);change_session=1;assert(!candidates().valid);
    candidate_fixture(0);request.guard_generation++;assert(!candidates().valid && !reads);
    candidate_fixture(0);names[0]=candidate_id(103);names[1]=names[0];
    assert(!uss_core_candidate_names(&candidate_life.owner,names,2,&bad));
    assert(!uss_core_candidate_names(&candidate_life.owner,names,0,&bad));
    assert(!uss_core_candidate_names(&candidate_life.owner,names,4,&bad));
    candidate_fixture(0);core_state=HF_STATE_SHIP;put32(fixture(0xA0000000+0xAC21C,4),3);
    bundle=candidates();assert(!bundle.valid && bundle.current.valid && !bundle.current.mission);
    candidate_fixture(0);initial_role=HF_SESSION_SOLO;assert(!candidates().valid);
    candidate_fixture(1);bundle=candidates();assert(bundle.valid && bundle.current.pair_matched);
    assert(bundle.members[0].entity==bundle.current.utility.entity && reads==306);
    stale=bundle;generation=candidate_life.owner.pair_generation;
    assert(uss_pair_owner_clear_pair(&candidate_life.owner,7,generation));
    assert(uss_pair_owner_set_pair(&candidate_life.owner,7,0,&request.members[0],&request.members[1]));
    assert(!candidate_bundle_current(&candidate_life,&stale));
    candidate_fixture(1);extra_candidate();put32(fixture(0x70000300,48)+8,104);
    names[0]=candidate_id(104);assert(uss_core_candidate_names(&candidate_life.owner,names,1,&request));
    bundle=candidates();assert(bundle.valid && uss_lifecycle_replace_begin_candidates(&candidate_life,7,&bundle,1,0));
    names[0]=candidate_id(104);names[1]=candidate_id(102);names[2]=candidate_id(103);
    assert(uss_core_candidate_names(&candidate_life.owner,names,3,&request));
    put32(fixture(0x70000700,192)+96+4,0);bundle=candidates();assert(bundle.valid && bundle.members[2].parent==0);
    printf("Candidate replacement bundle: %u reads / %u bytes\n",last_reader.calls,last_reader.bytes);
    assert(last_reader.calls<=2165 && last_reader.bytes<=20000);
    assert(uss_lifecycle_replace_commit_candidates(&candidate_life,7,&bundle));
    assert(candidate_life.owner.utility.entity==104 && candidate_life.phase==USS_LIFE_PENDING);
    total=last_reader.calls;
    for(i=1;i<=total;++i){
        candidate_fixture(1);extra_candidate();put32(fixture(0x70000300,48)+8,104);
        names[0]=candidate_id(104);names[1]=candidate_id(102);names[2]=candidate_id(103);
        assert(uss_core_candidate_names(&candidate_life.owner,names,3,&request));
        put32(fixture(0x70000700,192)+96+4,0);fail_at=i;
        bundle=candidates();assert(!bundle.valid && !bundle.current.valid);
    }
    /* No implicit capture/request is available when the explicit names are absent. */
    platform.thread_id=core_thread;flags=uss_lifecycle_flags(&candidate_life,&core_flags);
    bundle=uss_core_observe_candidates(&fake,MOD,&candidate_life.owner,&flags,&platform,NULL,NULL);assert(!bundle.valid);
    puts("Named candidates: coherent pre-pair/replacement reads, partial failures, lifetime/context guards and explicit lifecycle consumption PASS");return 0;
}
