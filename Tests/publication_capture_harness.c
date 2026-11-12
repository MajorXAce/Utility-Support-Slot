#ifdef NDEBUG
#error Capture checks require active assertions
#endif
#define USS_FIXTURE_SEGMENTS 256
#define USS_FIXTURE_BYTES 4096
#define USS_ACTION_TEST_MAIN action_regression_checks
#include "action_state_harness.c"
#include "../src/publication_capture.h"
#define EXE ((uintptr_t)0xB0000000)
#define SESSION ((uintptr_t)0xA2000000)
#define MAPPING ((uintptr_t)0xA3000000)
static UssCaptureInput capture_input;
static unsigned input_calls,input_change,query_calls,query_fail,path_fail,export_fail,thread_change,query_change;
static uintptr_t module_capture(const char* name,size_t* size)
{if(!strcmp(name,"helldivers2.exe")){*size=60719104;return EXE;}return module(name,size);}
static int input_snapshot(void* user,UssCaptureInput* out)
{(void)user;*out=capture_input;if(++input_calls>1 && input_change)++out->pair_generation;return 1;}
static int page_snapshot(void* user,uintptr_t address,UssCapturePage* out)
{
    (void)user;++query_calls;if(query_fail && query_calls==query_fail)return 0;
    memset(out,0,sizeof(*out));out->state=0x1000;
    if(address>=EXE && address<EXE+60719104){out->base=out->allocation=EXE;out->size=60719104;out->kind=0x1000000;out->protection=32;}
    else if(address>=BASE && address<BASE+0x4000000){out->base=out->allocation=BASE;out->size=0x4000000;out->kind=0x1000000;out->protection=32;}
    else if(address>=0xE0000000 && address<0xE0100000){out->base=out->allocation=0xE0000000;out->size=0x100000;out->kind=0x1000000;out->protection=32;}
    else {out->base=out->allocation=address&~(uintptr_t)0xfff;out->size=4096;out->kind=0x20000;out->protection=4;}
    if(query_change && query_calls==query_change)out->size-=8;
    return 1;
}
static int module_path_snapshot(void* user,uintptr_t base,uint16_t* out,size_t capacity,size_t* length)
{const char* path="C:\\Fixture\\helldivers2.exe";size_t i;(void)user;assert(base==EXE && capacity>=32);if(path_fail)return 0;*length=strlen(path);for(i=0;i<*length;++i)out[i]=(uint16_t)path[i];return 1;}
static uintptr_t export_snapshot(void* user,const char* name)
{(void)user;if(export_fail)return 0;if(!strcmp(name,"EnterCriticalSection"))return 0xE0001000;if(!strcmp(name,"LeaveCriticalSection"))return 0xE0002000;assert(!strcmp(name,"DeleteCriticalSection"));return 0xE0003000;}
static uint32_t thread_snapshot(void* user){(void)user;return thread_change?8:7;}
static void ptr_segment(uintptr_t at,uintptr_t value){putptr(segment(at,8),value);}
static void word_segment(uintptr_t at,uint32_t value){put32(segment(at,4),value);}
static void hash_rows(uintptr_t at,uintptr_t data,unsigned capacity,const uint32_t* keys,const uint32_t* indexes,unsigned count)
{
    unsigned char* header=segment(at,20);unsigned char* rows=segment(data,capacity*8);unsigned i;
    putptr(header,data);put32(header+8,capacity);put32(header+12,UINT32_MAX);put32(header+16,1);memset(rows,0xff,capacity*8);
    for(i=0;i<count;++i){unsigned k=keys[i]&(capacity-1);while(u32(rows+k*8)!=UINT32_MAX)k=(k+1)&(capacity-1);put32(rows+k*8,keys[i]);put32(rows+k*8+4,indexes[i]);}
}
static UssCaptureProducer producer(void)
{
    UssCaptureProducer result={0};result.api=&fake;result.mod=MOD;result.input=input_snapshot;
    result.platform.query=page_snapshot;result.platform.path=module_path_snapshot;
    result.platform.export_address=export_snapshot;result.platform.thread_id=thread_snapshot;return result;
}
static void capture_fixture(void)
{
    static const unsigned char silo[8]={0xaa,0x79,0xca,0x92,0x05,0x46,0x13,0xfc};
    static const unsigned char state_pin[7]={0x48,0x8b,0x0d,0x15,0x23,0x1e,0x03};
    static const uint32_t handles[4]={22,101,103,102},positions[4]={3,2,0,1},wires[4]={50,51,52,53};
    unsigned i;unsigned char* b;uintptr_t descriptors[4]={0x20000000+0xF31AD8,0x70000900,0x70002900,0x70001900};
    distinct_slots();fake.module_base=module_capture;initial_role=HF_SESSION_HOST;
    input_calls=input_change=query_calls=query_fail=path_fail=export_fail=thread_change=query_change=0;
    memcpy(segment(BASE+0xA93ECD,7),(unsigned char[]){0x4c,0x8b,0x35,0x4c,0x2e,0x89,0x02},7);
    ptr_segment(BASE+0x3326D20,ACTION_MANAGER);word_segment(ACTION_MANAGER+0x70,1);
    table(ACTION_MANAGER+0xF8,0x80001000,22);ptr_segment(ACTION_MANAGER+0x110,0x80002000);
    word_segment(0x80002008,22);b=segment(ACTION_MANAGER+0x53DCD0,32);put32(b,22);put32(b+4,1);
    segment(ACTION_MANAGER+0x53E880,24);segment(ACTION_MANAGER+0x53EAE0,1);
    memcpy(segment(BASE+0x144024,7),state_pin,7);ptr_segment(BASE+0x3326340,0xA0000000);
    word_segment(0xA0000000+0xAC218,0);word_segment(0xA0000000+0xAC21C,4);
    ptr_segment(BASE+0x33266A0,0xA1000000);word_segment(0xA1000008,1);ptr_segment(BASE+0x347CEF0,SESSION);
    word_segment(SESSION+0x162D8,2);b=segment(SESSION+0xB398,8);b[0]=1;
    b=segment(SESSION+0xB3A8,8);b[0]=1;b=segment(SESSION+0x162E0,16);b[0]=1;b[8]=2;
    b=fixture(0x70000300,48);put32(b+4,UINT32_MAX);put32(b+8,102);put32(b+12,UINT32_MAX);put32(b+24,UINT32_MAX);put32(b+28,1);
    b=fixture(0x70000700,144);put32(b+4,22);put32(b+48+4,22);put32(b+96+4,22);put32(b+48+12,2);put32(b+96+12,2);
    memcpy(fixture(0x70002900,24),silo,8);
    for(i=0;i<4;++i){b=fixture(descriptors[i],24);put32(b+16,wires[i]);put32(b+20,1);}
    ptr_segment(BASE+0x346BF98,MAPPING);
    hash_rows(MAPPING+0xF1AEB0,0xA3010000,8,handles,positions,4);
    hash_rows(MAPPING+0xF22EC8,0xA3020000,8,wires,positions,4);
    for(i=0;i<4;++i)memcpy(segment(MAPPING+0xF32F18+(uintptr_t)positions[i]*24,24),fixture(descriptors[i],24),24);
    ptr_segment(BASE+0x3326E68,0x91000000);word_segment(0x910006C0,1);
    b=segment(0x910006C8,16);putptr(b,0x91001000);put32(b+8,141);segment(0x91001020,20);
    ptr_segment(BASE+0x3326698,0x92000000);
    {const uint32_t keys[]={101,103,102},indexes[]={2,0,1};hash_rows(0x92000018,0x92000100,4,keys,indexes,3);}
    ptr_segment(0x92000038,0x92001000);for(i=0;i<3;++i)word_segment(0x92001000+(uintptr_t)i*172,1);
    word_segment(BASE+0x348456C,UINT32_MAX);word_segment(BASE+0x2154544,32767);
    ptr_segment(BASE+0x33263E8,0xA4000000);ptr_segment(BASE+0x3326308,0xA5000000);ptr_segment(0xA5000038,0xA5001000);
    b=segment(0xA4000060,32);putptr(b,EXE+0xA4030);putptr(b+8,EXE+0xA4070);putptr(b+16,EXE+0xA40B0);putptr(b+24,EXE+0xA40C0);
    ptr_segment(0xA5001040,EXE+0x34FF40);ptr_segment(SESSION+0x1F860,0xA6000000);*segment(SESSION+0x1F86B,1)=1;
    segment(0xA6000000,40);ptr_segment(0xA6000028,EXE+0x16677B8);
    b=segment(EXE,64);memcpy(b,"MZ",2);put32(b+0x3C,0x80);
    b=segment(EXE+0x80,24);memcpy(b,"PE\0\0\x64\x86",6);b[6]=1;put32(b+8,0x6AB382E4);b[20]=0xf0;
    b=segment(EXE+0x98,64);b[0]=0x0b;b[1]=2;put32(b+56,60719104);
    b=segment(EXE+0x188,40);put32(b+8,60719104-0x1000);put32(b+12,0x1000);put32(b+36,0x20000000);
    memcpy(segment(EXE+0xA40B0,7),(unsigned char[]){0x48,0xff,0x25,1,0x26,0x35,1},7);
    memcpy(segment(EXE+0xA40C0,7),(unsigned char[]){0x48,0xff,0x25,0xf9,0x25,0x35,1},7);
    ptr_segment(EXE+0x13F66B8,0xE0001000);ptr_segment(EXE+0x13F66C0,0xE0002000);ptr_segment(EXE+0x13F6628,0xE0003000);
    for(i=0;i<sizeof(capture_pins)/sizeof(capture_pins[0]);++i){const UssCapturePin* pin=&capture_pins[i];memcpy(segment((pin->exe?EXE:BASE)+pin->rva,pin->size),pin->bytes,pin->size);}
    memcpy(segment(BASE+0xBDE452,11),(unsigned char[]){0x45,0x8b,0xf9,0x4d,0x8b,0xf0,0x48,0x8b,0xfa,0x8b,0xe9},11);
    ptr_segment(EXE+0x13F6690,EXE+0x2C6E642);memcpy(segment(EXE+0x2C6E642,5),(unsigned char[]){0xe9,0xda,0x41,0x2f,0},5);
    ptr_segment(EXE+0x1A10278,0xA7000000);ptr_segment(0xA7000000,0xA7100000);
    b=segment(0xA7100000,16);b[13]=8;ptr_segment(0xA7100018,0xA7300000);
    b=segment(0xA7100088,16);put32(b,153);putptr(b+8,0xA7600000);
    {const unsigned candidates[]={76,115,134,144,149,151};for(i=0;i<6;++i)word_segment(0xA7600000+(uintptr_t)candidates[i]*104,0);}
    b=segment(0xA7600000+152*104,12);put32(b,0x42729286);
    b=segment(0xA7600000+152*104+0x50,16);put32(b,7);putptr(b+8,0xA7400000);
    b=segment(0xA7400000,28);for(i=0;i<7;++i)put32(b+i*4,i==6?408:256);
    b=segment(0xA7300000+256*24+12,12);b[0]=1;b[1]=15;b=segment(0xA7300000+408*24+12,12);b[0]=7;
    ptr_segment(BASE+0x214C2E0+152*8,BASE+0xBAABD0);
    ptr_segment(0xA70000A8,0xA7200000);ptr_segment(0xA7000098,0);ptr_segment(0xA7200000,0xA7500000);
    ptr_segment(0xA7500180,EXE+0x293F50);
    memset(&capture_input,0,sizeof(capture_input));capture_input.pair_generation=capture_input.context_generation=1;
    capture_input.utility=103;capture_input.ordinary=102;capture_input.callback_thread=7;
    memcpy(capture_input.utility_identity,silo,8);memcpy(capture_input.ordinary_identity,fixture(0x70001900,24),8);
    assert(sample(&capture_input.owner));reads=module_calls=session_calls=0;memset(&last_reader,0,sizeof(last_reader));
}
static int observe(UssPublicationCapture* output){UssCaptureProducer p=producer();return uss_capture_publication(&p,output,&last_reader);}
typedef struct CaptureIntegration {
    UssPublicationState state;
    UssPublicationCapture reference;
    Reader budget;
    unsigned captures,sends,reorder_entry;
} CaptureIntegration;
static uint64_t integration_clock(void* user){(void)user;return 100;}
static UssPublicationView integration_view(void* user)
{CaptureIntegration* test=user;UssPublicationView out={0};out.valid=out.mission=out.pair_present=out.supported_pair=1;out.key=test->reference.key;return out;}
static int integration_capture(void* user,UssPublicationStage stage,UssPublicationCapture* out)
{
    CaptureIntegration* test=user;UssCaptureProducer p=producer();(void)stage;
    if(++test->captures==2 && test->reorder_entry) {
        fixture(SESSION+0x162E0,16)[0]=2;fixture(SESSION+0x162E0,16)[8]=1;
    }
    return uss_capture_publication(&p,out,&test->budget);
}
static UssPublicationSend integration_send(void* user,UssPublicationKind kind,const UssPublicationCapture* captured,uint64_t metadata)
{
    CaptureIntegration* test=user;uint64_t expected;
    assert(kind==USS_PUBLICATION_FORMATION && test->state.formation_attempted && test->state.attempted==1);
    assert(captured->valid);memcpy(&expected,capture_input.owner.row+40,8);assert(metadata==expected);
    ++test->sends;return USS_PUBLICATION_SEND_RETURNED;
}
static void integration_checks(void)
{
    CaptureIntegration test={0};UssPublicationServices services={0};UssPublicationResult result;
    capture_fixture();assert(observe(&test.reference));
    services.user=&test;services.clock_ms=integration_clock;services.view=integration_view;
    services.capture=integration_capture;services.send=integration_send;
    result=uss_publication_boundary(&test.state,&services,&test.reference.owner,1,USS_PUBLICATION_PRE_ATTACH);
    assert(result.outcome==USS_PUBLICATION_RETURNED && test.captures==2 && test.sends==1);
    printf("two real read captures/fake send PASS: reads=%u bytes=%u\n",test.budget.calls,test.budget.bytes);
    capture_fixture();memset(&test,0,sizeof(test));assert(observe(&test.reference));test.reorder_entry=1;
    result=uss_publication_boundary(&test.state,&services,&test.reference.owner,1,USS_PUBLICATION_PRE_ATTACH);
    assert(result.outcome==USS_PUBLICATION_REFUSED && result.reason==USS_PUB_ENTRY_CHANGED && !test.sends && !test.state.attempted);
}
static void rejected_capture(void)
{
    UssPublicationCapture output;static const UssPublicationCapture empty={0};
    memset(&output,0xCC,sizeof(output));assert(!observe(&output));
    assert(!memcmp(&output,&empty,sizeof(output)) && !last_reader.evidence);
}
#ifndef USS_CAPTURE_TEST_MAIN
#define USS_CAPTURE_TEST_MAIN main
#endif
int USS_CAPTURE_TEST_MAIN(void)
{
    UssPublicationCapture output,baseline;unsigned total,queries,i;
    assert(action_regression_checks()==0);
    capture_fixture();assert(observe(&baseline));total=reads;queries=query_calls;
    assert(baseline.valid && baseline.key.avatar==22 && baseline.key.utility==103 && baseline.key.ordinary==102);
    assert(baseline.wires[0]==50 && baseline.wires[1]==51 && baseline.wires[2]==52 && baseline.wires[3]==53);
    assert(!last_reader.evidence && last_reader.calls==total && total<=2165 && last_reader.bytes<=20000);
    printf("capture baseline PASS: reads=%u bytes=%u fingerprint=%zu queries=%u\n",last_reader.calls,last_reader.bytes,baseline.fingerprint_size,query_calls);
    for(i=1;i<=total;++i){capture_fixture();fail_at=i;rejected_capture();}
    for(i=1;i<=queries;++i){capture_fixture();query_fail=i;rejected_capture();}
    capture_fixture();fixture(EXE+0x80,24)[6]=96;fixture(EXE+0x188,96*40);
    assert(observe(&output));printf("96-section proof PASS: bytes=%u fingerprint=%zu\n",last_reader.bytes,output.fingerprint_size);
    capture_fixture();putptr(fixture(0xA70000A8,8),0);putptr(fixture(0xA7000098,8),0xA7200000);
    ptr_segment(0xA7500018,EXE+0x293F50);assert(observe(&output));
    capture_fixture();fixture(0xA7100000,16)[5]=1;ptr_segment(0xA7500090,EXE+0x34FF40);assert(observe(&output));
    capture_fixture();{uint64_t metadata=UINT64_C(0xfedcba9876543210);memcpy(fixture(0x70000300,48)+40,&metadata,8);
        assert(sample(&capture_input.owner));reads=module_calls=session_calls=0;
        assert(observe(&output) && !memcmp(output.owner.row+40,&metadata,8));}
    capture_fixture();{
        unsigned char descriptor[24];memcpy(descriptor,fixture(MAPPING+0xF32F18+3*24,24),24);
        memcpy(segment(MAPPING+0xF32F18+4*24,24),descriptor,24);
        /* Reverse avatar index4 differs from forward3 and owner index0. */
        put32(fixture(0xA3020000,64)+(50&7)*8+4,4);assert(observe(&output));
    }
    capture_fixture();initial_role=HF_SESSION_JOINER;fixture(SESSION+0xB3A8,8)[0]=2;
    assert(observe(&output) && output.key.peers[8]==2);
    capture_fixture();
    put32(fixture(SESSION+0x162D8,4),4);fixture(SESSION+0x162E0,32)[16]=3;fixture(SESSION+0x162E0,32)[24]=4;
    assert(observe(&output) && output.key.peer_size==48);
    capture_fixture();fixture(SESSION+0x162E0,16)[0]=2;fixture(SESSION+0x162E0,16)[8]=1;
    assert(observe(&output) && output.key.peer_size==baseline.key.peer_size &&
        !memcmp(output.key.peers,baseline.key.peers,baseline.key.peer_size));
    capture_fixture();initial_role=HF_SESSION_JOINER;rejected_capture();
    capture_fixture();initial_role=HF_SESSION_SOLO;rejected_capture();
    capture_fixture();put32(fixture(SESSION+0x162D8,4),1);rejected_capture();
    capture_fixture();put32(fixture(SESSION+0x162D8,4),5);rejected_capture();
    capture_fixture();memset(fixture(SESSION+0xB398,8),0,8);rejected_capture();
    capture_fixture();fixture(SESSION+0xB398,8)[0]=3;rejected_capture();
    capture_fixture();fixture(SESSION+0xB3A8,8)[0]=3;rejected_capture();
    capture_fixture();fixture(SESSION+0x162E0,16)[8]=1;rejected_capture();
    capture_fixture();query_change=2;rejected_capture();
    capture_fixture();input_change=1;rejected_capture();
    capture_fixture();path_fail=1;rejected_capture();
    capture_fixture();export_fail=1;rejected_capture();
    capture_fixture();thread_change=1;rejected_capture();
    capture_fixture();change_session=1;rejected_capture();
    capture_fixture();module_change=1;rejected_capture();
    capture_fixture();put32(fixture(0xA0000000+0xAC218,4),1);rejected_capture();
    capture_fixture();put32(fixture(0xA0000000+0xAC21C,4),0);rejected_capture();
    capture_fixture();put32(fixture(0x70000700,144)+96+4,0);rejected_capture();
    capture_fixture();put32(fixture(MAPPING+0xF32F18,24)+8,104);rejected_capture();
    capture_fixture();put32(fixture(MAPPING+0xF32F18,24)+16,32767);rejected_capture();
    capture_fixture();fixture(MAPPING+0xF32F18,24)[20]=0;rejected_capture();
    capture_fixture();put32(fixture(0x910006C0,4),33);rejected_capture();
    capture_fixture();put32(fixture(0x91001020,20)+8,3);rejected_capture();
    capture_fixture();{const uint32_t keys[]={103},index[]={0};
        /* A valid but present component is ineligible. */
        unsigned char* header=fixture(0x91001020,20);putptr(header,0x93000000);
        put32(header+8,1);put32(header+12,UINT32_MAX);put32(header+16,1);
        put32(segment(0x93000000,8),keys[0]);put32(fixture(0x93000000,8)+4,index[0]);}
    rejected_capture();
    capture_fixture();{
        unsigned char* header=fixture(0x91001020,20);unsigned char* rows=segment(0x93000000,64*8);
        putptr(header,0x93000000);put32(header+8,64);put32(header+12,UINT32_MAX);put32(header+16,1);
        for(i=0;i<64;++i){put32(rows+i*8,999);put32(rows+i*8+4,0);}
        rejected_capture(); /* Collision-limit exhaustion is unknown, not absent. */
    }
    capture_fixture();put32(fixture(0x92001000,4),0);rejected_capture();
    capture_fixture();put32(fixture(BASE+0x2154544,4),0);rejected_capture();
    capture_fixture();fixture(EXE+0x80,24)[8]^=1;rejected_capture();
    capture_fixture();fixture(EXE+0x188,40)[36]=0;fixture(EXE+0x188,40)[39]=0;rejected_capture();
    capture_fixture();{
        unsigned char* sections=fixture(EXE+0x188,80);fixture(EXE+0x80,24)[6]=2;
        put32(sections+40+8,1);put32(sections+40+12,0xA40B0);put32(sections+40+36,0x20000000);
        rejected_capture(); /* Overlap is ambiguous even if only one span fits the body. */
    }
    capture_fixture();fixture(EXE+0x34FF40,sizeof(capture_pin_4))[0]^=1;rejected_capture();
    capture_fixture();putptr(fixture(EXE+0x13F66B8,8),0xE0003000);rejected_capture();
    capture_fixture();put32(fixture(0xA7400000,28),257);rejected_capture();
    capture_fixture();fixture(0xA7300000+256*24+12,12)[1]=32;rejected_capture();
    capture_fixture();*fixture(SESSION+0x1F86B,1)=0;rejected_capture();
    capture_fixture();capture_input.replacement_queue_checks=1;rejected_capture();
    capture_fixture();capture_input.pair_generation=0;rejected_capture();
    capture_fixture();last_reader.calls=2165;rejected_capture();assert(!reads);
    capture_fixture();last_reader.bytes=20001;rejected_capture();assert(!reads);
    /* Lossless trace framing refuses before exposing partial decoded output. */
    {Reader reader={0};unsigned char proof[16],value[20];
        capture_fixture();reader.api=&fake;reader.evidence=proof;reader.evidence_capacity=sizeof(proof);
        memset(value,0xCC,sizeof(value));assert(!get(&reader,0x60000000,value,20));
        for(i=0;i<sizeof(value);++i)assert(value[i]==0xCC);
        assert(!strcmp(reader.failure,"EVIDENCE_BOUNDS"));}
    /* The concrete Windows provider compiles, but no real OS/game observation is called. */
    {UssCapturePlatform platform=uss_capture_windows_platform();assert(platform.query && platform.path && platform.export_address && platform.thread_id);}
    integration_checks();
    puts("publication capture: actual bounded reads, host/joiner roster fixtures, mappings, parents, dispatch pins/policies, every read/query failure PASS");
    return 0;
}
