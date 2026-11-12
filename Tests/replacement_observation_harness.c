#define USS_REPLACEMENT_TEST_MAIN replacement_regression_checks
#include "replacement_transaction_harness.c"
#define USS_REPLACEMENT_REUSE_CAPTURE 1
#define USS_DISPATCH_TEST_MAIN dispatch_regression_checks
#define capture_regression_checks producer_regression_checks
#include "replacement_dispatch_harness.c"
#undef capture_regression_checks
#include "../src/replacement_observation.h"
static UssReplacementOriginEvidence observed_origin;
static UssReplacementProducer observation_producer;
static UssReplacementControl observation;
static unsigned observation_clock_calls,observation_mutate;
static uint64_t observation_clock(void* user)
{
    (void)user;++observation_clock_calls;
    if(observation_clock_calls==2 && observation_mutate==1)++replacement_work.intent.serial;
    if(observation_clock_calls==3 && observation_mutate==3)fixture(0xA6000000,40)[0]^=1;
    return replacement_now+(observation_clock_calls==2 && observation_mutate==2?2000:0);
}
static void observation_fixture(unsigned utility,unsigned pickup,unsigned selected,unsigned role)
{
    unsigned char* row;UssCaptureProducer base=producer();
    replacement_fixture(utility,pickup,selected,role);
    memset(&observation_producer,0,sizeof(observation_producer));
    observation_producer.api=&fake;observation_producer.mod=MOD;observation_producer.platform=base.platform;
    observation_producer.life=&candidate_life;observation_producer.history=&hf048_history;
    observation_producer.flags=&core_flags;observation_producer.work=&replacement_work;
    observation_producer.origin=&observed_origin;observation_producer.clock_ms=observation_clock;
    observation_clock_calls=observation_mutate=0;
    /* Obtain origin through the actual producer while old members are attached. */
    row=fixture(0x70000300,48);put32(row+8,replacement_origin.real);set_selected(selected);
    put32(fixture(0x70000700,240)+4+(replacement_origin.real==102?48:96),22);
    assert(uss_replacement_read_origin(&observation_producer,&observed_origin));
    assert(!memcmp(&observed_origin.origin,&replacement_origin,sizeof(replacement_origin)));
    put32(fixture(0x70000700,240)+4+(replacement_origin.real==102?48:96),0);
    put32(row+8,104);set_selected(pickup?3:selected);
    assert(begin_replacement());
    /* Map the incoming unit and keep native descriptor equality exact. */
    {const uint32_t handles[]={22,101,103,102,104,105},positions[]={3,2,0,1,4,5},wires[]={50,51,52,53,54,55};
        unsigned i;uintptr_t descriptor[]={0x20000000+0xF31AD8,0x70000900,0x70002900,0x70001900,0x70003900,0x70004900};
        hash_rows(MAPPING+0xF1AEB0,0xA3010000,8,handles,positions,6);
        /* Existing fixture rows win lookup: update rather than append shadows. */
        {unsigned char* h=fixture(MAPPING+0xF1AEB0,20);unsigned char* data=fixture(0xA3010000,64);
            (void)h;memset(data,0xff,64);for(i=0;i<6;++i){unsigned k=handles[i]&7;while(u32(data+k*8)!=UINT32_MAX)k=(k+1)&7;put32(data+k*8,handles[i]);put32(data+k*8+4,positions[i]);}}
        {unsigned char* data=fixture(0xA3020000,64);memset(data,0xff,64);for(i=0;i<6;++i){unsigned k=wires[i]&7;while(u32(data+k*8)!=UINT32_MAX)k=(k+1)&7;put32(data+k*8,wires[i]);put32(data+k*8+4,positions[i]);}}
        for(i=0;i<6;++i){unsigned char* d=fixture(descriptor[i],24);put32(d+16,wires[i]);put32(d+20,1);
            memcpy(control_segment(MAPPING+0xF32F18+(uintptr_t)positions[i]*24,24),d,24);}
    }
    {const uint32_t handles[]={101,102,103,104},indexes[]={0,1,2,3};unsigned i;
        unsigned char* header=fixture(0x92000018,20);unsigned char* data=fixture(0x92000100,64);
        put32(header+8,8);memset(data,0xff,64);
        for(i=0;i<4;++i){unsigned k=handles[i]&7;put32(data+k*8,handles[i]);put32(data+k*8+4,indexes[i]);}
        for(i=0;i<4;++i)control_word(0x92001000+(uintptr_t)i*172,1);
    }
    reads=0;memset(&last_reader,0,sizeof(last_reader));
}
static int read_observation(unsigned entry)
{return uss_replacement_capture(&observation_producer,entry,&observation,&last_reader);}
static void advance_to(UssReplacementStage target)
{
    unsigned i;for(i=0;i<20 && replacement_work.stage!=target;++i){replacement_now+=25;tick_replacement(replacement_now);}
    assert(replacement_work.phase==USS_REPLACE_ACTIVE && replacement_work.stage==target);
}
static void entry_fixture(void)
{
    observation_fixture(1,0,1,HF_SESSION_HOST);advance_to(USS_REPLACE_DROP);control_dependencies();
    observation_clock_calls=reads=query_calls=0;
}
#ifndef USS_REPLACEMENT_OBSERVATION_TEST_MAIN
#define USS_REPLACEMENT_OBSERVATION_TEST_MAIN main
#endif
int USS_REPLACEMENT_OBSERVATION_TEST_MAIN(void)
{
    UssReplacementEntryEvidence evidence;UssReplacementControl post;
    unsigned total,i,queries,utility,pickup,selected,role;assert(replacement_regression_checks()==0);assert(dispatch_regression_checks()==0);
    observation_fixture(1,0,1,HF_SESSION_HOST);assert(read_observation(0));total=reads;
    printf("replacement observation reads=%u bytes=%llu evidence=%llu\n",total,
        (unsigned long long)last_reader.bytes,(unsigned long long)observation.fingerprint_size);
    assert(observation.valid && observation.snapshot.empty_valid && observation.snapshot.native_empty==UINT32_MAX);
    for(i=1;i<=total;++i){observation_fixture(1,0,1,HF_SESSION_HOST);fail_at=i;assert(!read_observation(0) && !observation.valid);}
    observation_fixture(1,0,1,HF_SESSION_HOST);replacement_work.admitted=0;candidate_life.phase=USS_LIFE_FORMED;
    assert(read_observation(0));assert(!read_observation(1)); /* Begin may read, never enter native. */
    observation_fixture(1,0,1,HF_SESSION_HOST);observed_origin.origin.real=104;assert(!read_observation(0));
    observation_fixture(1,0,1,HF_SESSION_HOST);observed_origin.origin.selected=2;assert(!read_observation(0));
    observation_fixture(1,0,1,HF_SESSION_HOST);++observed_origin.wires[2];assert(!read_observation(0));
    observation_fixture(1,0,1,HF_SESSION_HOST);observation_mutate=1;assert(!read_observation(0));
    observation_fixture(1,0,1,HF_SESSION_HOST);observation_mutate=2;assert(!read_observation(0));
    observation_fixture(1,0,1,HF_SESSION_HOST);core_flags.input=1;assert(!read_observation(0));
    observation_fixture(1,0,1,HF_SESSION_HOST);put32(fixture(BASE+0x348456C,4),123);assert(!read_observation(0));
    observation_fixture(1,0,1,HF_SESSION_HOST);put32(fixture(MAPPING+0xF32F18+4*24,24)+16,32767);assert(!read_observation(0));
    observation_fixture(1,0,1,HF_SESSION_JOINER);assert(read_observation(0));
    observation_fixture(0,1,2,HF_SESSION_HOST);assert(read_observation(0));
    observation_fixture(1,0,1,HF_SESSION_HOST);advance_to(USS_REPLACE_DROP);assert(read_observation(0));
    control_dependencies();observation_clock_calls=0;{int ok=read_observation(1);if(!ok)fprintf(stderr,"entry failure=%s stage=%s reads=%llu bytes=%llu\n",last_reader.failure,last_reader.stage,(unsigned long long)last_reader.calls,(unsigned long long)last_reader.bytes);assert(ok);}
    printf("replacement entry reads=%llu bytes=%llu evidence=%llu\n",(unsigned long long)last_reader.calls,
        (unsigned long long)last_reader.bytes,(unsigned long long)observation.fingerprint_size);
    entry_fixture();assert(read_observation(1));total=reads;queries=query_calls;
    printf("entry complete core=%llu/%llu control=%llu/%llu trace=%llu+%llu\n",(unsigned long long)observation.core_calls,(unsigned long long)observation.core_bytes,
        (unsigned long long)observation.control_calls,(unsigned long long)observation.control_bytes,
        (unsigned long long)observation.fingerprint_size,(unsigned long long)observation.dispatch_fingerprint_size);
    assert(observation.core_bytes<=20000 && observation.control_bytes<=20000 &&
        observation.fingerprint_size<=32768 && observation.dispatch_fingerprint_size<=32768);
    for(i=1;i<=total;++i){entry_fixture();fail_at=i;assert(!read_observation(1) && !observation.valid);}
    for(i=1;i<=queries;++i){entry_fixture();query_fail=i;assert(!read_observation(1) && !observation.valid);}
    entry_fixture();assert(uss_replacement_capture_entry(&observation_producer,&evidence));
    assert(uss_replacement_consume_entry(&observation_producer,&evidence));
    assert(!uss_replacement_consume_entry(&observation_producer,&evidence));
    assert(!uss_replacement_read_origin(&observation_producer,&observed_origin));
    observed_origin=evidence.captured.origin; /* Refused origin read clears output. */
    assert(!uss_replacement_verify_drop(&observation_producer,&evidence,&post)); /* no effect yet */
    put32(fixture(0x70000300,48)+8,UINT32_MAX);put32(fixture(0x70000700,240)+96+4,0);
    assert(uss_replacement_verify_drop(&observation_producer,&evidence,&post));
    assert(post.snapshot.native_empty==UINT32_MAX && post.snapshot.bundle.members[2].parent==0);
    assert(replacement_work.stage==USS_REPLACE_DROP && !replacement_work.new_generation);
    ++candidate_life.owner.pair_generation;assert(!uss_replacement_verify_drop(&observation_producer,&evidence,&post));
    entry_fixture();assert(uss_replacement_capture_entry(&observation_producer,&evidence));
    core_flags.eligible=0;assert(!uss_replacement_consume_entry(&observation_producer,&evidence));
    entry_fixture();assert(uss_replacement_capture_entry(&observation_producer,&evidence));
    ++replacement_work.intent.serial;assert(!uss_replacement_consume_entry(&observation_producer,&evidence) && evidence.consumed);
    entry_fixture();observation_mutate=3;assert(!uss_replacement_capture_entry(&observation_producer,&evidence) && !evidence.valid);
    /* Phase2 belongs only to this current replacement and primary deadline. */
    entry_fixture();{unsigned char* row=fixture(ACTION_MANAGER+0x53DCD0,32);
        put32(row+8,3);put32(row+16,2);put32(row+20,1);put32(row+24,0);put32(row+28,0x3F800000);
        put32(fixture(ACTION_MANAGER+0x53E880,24)+12,0x8000);}
    assert(read_observation(1) && !observation.snapshot.bundle.current.primary.ready);
    {UssPublicationCapture ordinary;UssCaptureProducer strict=producer();Reader budget={0};
        assert(sample(&capture_input.owner));assert(!uss_capture_publication(&strict,&ordinary,&budget));}
    replacement_now=replacement_work.primary_deadline;assert(!read_observation(1));
    entry_fixture();replacement_work.phase=USS_REPLACE_CANCELLED;assert(!read_observation(1));
    /* Actual old/new membership handover and all replacement variants. */
    for(utility=0;utility<2;++utility)for(pickup=0;pickup<2;++pickup)for(selected=1;selected<=3;++selected)
    for(role=HF_SESSION_HOST;role<=HF_SESSION_JOINER;++role){
        observation_fixture(utility,pickup,selected,role);assert(read_observation(0));
        advance_to(USS_REPLACE_HANDOVER);observation_clock_calls=0;assert(read_observation(0));
        assert(observation.snapshot.bundle.current.inventory.slots[2]==observation.snapshot.native_empty);
        advance_to(USS_REPLACE_RESTORE_NEW);observation_clock_calls=0;assert(read_observation(0));
        assert(observation.work.new_generation && observation.snapshot.bundle.names.pair_generation==replacement_work.new_generation);
        control_dependencies();observation_clock_calls=0;assert(read_observation(1));
    }
    puts("replacement observation: real origin, scoped phase2, two full matching captures, one-shot evidence and fresh post-drop/handover checks PASS");
    return 0;
}
