/* Own graph memory only. No callback registration, process reads or native call. */
#ifndef USS_REPLACEMENT_REUSE_CAPTURE
#define USS_CAPTURE_TEST_MAIN capture_regression_checks
#include "publication_capture_harness.c"
#endif
#include "../src/replacement_dispatch.h"
static unsigned char* control_segment(uintptr_t at,size_t n)
{
    unsigned i;for(i=0;i<used;++i)if(segments[i].at==at){assert(n<=USS_FIXTURE_BYTES);segments[i].n=n;return segments[i].data;}
    return segment(at,n);
}
static void control_word(uintptr_t at,uint32_t value){put32(control_segment(at,4),value);}
static void control_dependencies(void)
{
    unsigned i,j;unsigned char* b;
    put32(fixture(0xA7100088,16),591);fixture(0xA7100000,16)[13]=10;
    /* Only visited search buckets need storage; keys retain sorted ordering. */
    for(j=0;j<2;++j) {
        uint32_t low=0,high=591,target=j?590:152;
        while(low<high) {
            uint32_t mid=(low+high)/2;
            if(mid==152)control_word(0xA7600000+(uintptr_t)mid*104,0x42729286);
            else if(mid==590)control_word(0xA7600000+(uintptr_t)mid*104,UINT32_C(0xFE5BF7A3));
            else control_word(0xA7600000+(uintptr_t)mid*104,mid<152?0:UINT32_C(0x80000000));
            if(mid==target)break;
            if(mid<target)low=mid+1;else high=mid;
        }
    }
    b=control_segment(0xA7600000+152*104,12);put32(b,0x42729286);b[4]=b[5]=1;
    b=control_segment(0xA7600000+590*104,12);put32(b,UINT32_C(0xFE5BF7A3));b[4]=b[5]=1;
    b=segment(0xA7600000+590*104+0x50,16);put32(b,2);putptr(b+8,0xA7410000);
    b=segment(0xA7410000,8);put32(b,256);put32(b+4,164);
    b=segment(0xA7300000+164*24+12,12);b[0]=1;b[1]=32;
    ptr_segment(BASE+0x214C2E0+590*8,BASE+0xBC2C50);
    ptr_segment(0xA7200010,0xA7700000);ptr_segment(0xA7700000,0xA7710000);
    ptr_segment(0xA7710018,EXE+0x2AA350);
    word_segment(0xA77000A0,1);ptr_segment(0xA77000A8,0xA7720000);
    ptr_segment(0xA7720000,0xA7730000);b=segment(0xA7730010,8);b[0]=2;
    segment(0xA7730000+0x1878C,1);
    for(j=0;j<2;++j) {
        const UssCapturePin* pins=j?uss_queue_control_pins:uss_drop_control_pins;
        unsigned count=j?(unsigned)(sizeof(uss_queue_control_pins)/sizeof(pins[0])):
                         (unsigned)(sizeof(uss_drop_control_pins)/sizeof(pins[0]));
        for(i=0;i<count;++i)memcpy(control_segment((j?EXE:BASE)+pins[i].rva,pins[i].size),pins[i].bytes,pins[i].size);
    }
    reads=query_calls=0;
}
static void control_fixture(void){capture_fixture();control_dependencies();}
static unsigned char control_proof[32768];
static Reader control_budget;
static int control_observe(void)
{
    UssCaptureProducer p=producer();UssCaptureReader reader={0};int result;
    reader.memory.api=&fake;reader.memory.slot=-1;reader.memory.stage="REPLACEMENT_CONTROL";
    reader.memory.evidence=control_proof;reader.memory.evidence_capacity=sizeof(control_proof);
    reader.platform=&p.platform;reader.game=BASE;reader.game_size=0x4000000;
    result=uss_capture_replacement_dispatch(&reader,SESSION);
    control_budget=reader.memory;control_budget.evidence=NULL;return result;
}
#ifndef USS_DISPATCH_TEST_MAIN
#define USS_DISPATCH_TEST_MAIN main
#endif
int USS_DISPATCH_TEST_MAIN(void)
{
    unsigned i,total,queries,j;UssPublicationCapture publication;
    assert(capture_regression_checks()==0);
    control_fixture();assert(control_observe());total=reads;queries=query_calls;
    printf("replacement dispatch baseline reads=%u bytes=%llu queries=%u evidence=%llu\n",total,
        (unsigned long long)control_budget.bytes,queries,(unsigned long long)control_budget.evidence_size);
    assert(control_budget.calls<2165 && control_budget.bytes<20000);
    for(i=1;i<=total;++i){control_fixture();fail_at=i;assert(!control_observe());}
    for(i=1;i<=queries;++i){control_fixture();query_fail=i;assert(!control_observe());}
    /* All body fragments are checked, not just a function prologue. */
    for(j=0;j<2;++j) {
        const UssCapturePin* pins=j?uss_queue_control_pins:uss_drop_control_pins;
        unsigned count=j?(unsigned)(sizeof(uss_queue_control_pins)/sizeof(pins[0])):
                         (unsigned)(sizeof(uss_drop_control_pins)/sizeof(pins[0]));
        for(i=0;i<count;++i){control_fixture();fixture((j?EXE:BASE)+pins[i].rva,pins[i].size)[pins[i].size-1]^=1;assert(!control_observe());}
    }
    control_fixture();fixture(0xA7600000+152*104,12)[4]=0;assert(!control_observe());
    control_fixture();fixture(0xA7600000+590*104,12)[5]=0;assert(!control_observe());
    control_fixture();put32(fixture(0xA7600000+590*104,12)+8,1);assert(!control_observe());
    control_fixture();put32(fixture(0xA7600000+590*104+0x50,16),3);assert(!control_observe());
    control_fixture();put32(fixture(0xA7410000,8)+4,256);assert(!control_observe());
    control_fixture();fixture(0xA7300000+164*24+12,12)[1]=15;assert(!control_observe());
    control_fixture();fixture(0xA7300000+164*24+12,12)[11]=1;assert(!control_observe());
    control_fixture();putptr(fixture(BASE+0x214C2E0+590*8,8),BASE+0xBAABD0);assert(!control_observe());
    control_fixture();fixture(0xA7100000,16)[13]=9;assert(!control_observe());
    control_fixture();fixture(0xA7100000,16)[5]=1;assert(!control_observe());
    control_fixture();putptr(fixture(0xA70000A8,8),0);assert(!control_observe());
    control_fixture();putptr(fixture(0xA7710018,8),EXE+0x293F50);assert(!control_observe());
    control_fixture();put32(fixture(0xA77000A0,4),33);assert(!control_observe());
    control_fixture();put32(fixture(0xA77000A0,4),0);assert(!control_observe());
    control_fixture();fixture(0xA7730010,8)[0]=3;assert(!control_observe());
    control_fixture();*fixture(0xA7730000+0x1878C,1)=1;assert(!control_observe());
    control_fixture();put32(fixture(0xA77000A0,4),2);
    ptr_segment(0xA7720008,0xA7730000);assert(!control_observe()); /* duplicate remote */
    control_fixture();put32(fixture(SESSION+0x162D8,4),3);
    fixture(SESSION+0x162E0,24)[16]=3;assert(!control_observe()); /* missing remote */
    control_fixture();initial_role=HF_SESSION_JOINER;fixture(SESSION+0xB398,8)[0]=2;
    fixture(0xA7730010,8)[0]=1;assert(control_observe()); /* remote is host */
    control_fixture();put32(fixture(SESSION+0x162D8,4),1);assert(!control_observe());
    /* Four peers and unrelated existing transports are valid within the bound. */
    control_fixture();put32(fixture(SESSION+0x162D8,4),4);
    fixture(SESSION+0x162E0,32)[16]=3;fixture(SESSION+0x162E0,32)[24]=4;
    put32(fixture(0xA77000A0,4),4);fixture(0xA7720000,32);
    for(i=1;i<4;++i) {
        uintptr_t entry=0xA7800000+(uintptr_t)i*0x20000;
        putptr(fixture(0xA7720000,32)+i*8,entry);
        segment(entry+0x10,8)[0]=(unsigned char)(i<3?i+2:9);segment(entry+0x1878C,1);
    }
    assert(control_observe());
    control_fixture();put32(fixture(0xA77000A0,4),32);fixture(0xA7720000,256);
    for(i=1;i<32;++i) {
        uintptr_t entry=0xA7800000+(uintptr_t)i*0x20000;
        putptr(fixture(0xA7720000,256)+i*8,entry);segment(entry+0x10,8)[0]=9;
    }
    assert(control_observe());
    /* Ordinary capture never acquires this new restriction. */
    capture_fixture();assert(observe(&publication));
    puts("replacement dispatch: both policies, existing-peer queue, code/page identity and every partial read/query refusal PASS");
    return 0;
}
