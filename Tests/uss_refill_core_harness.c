#define USS_LIFECYCLE_TEST_MAIN lifecycle_regression_checks
#include "core_lifecycle_harness.c"
#include "../src/uss_refill_runtime.h"
static UssRefillCore core;
static void refill_core_fixture(void)
{
    unsigned char* bytes;core_fixture();memset(&life,0,sizeof(life));
    /* A real secondary descriptor, distinct from both pair members. */
    put32(fixture(0x70000300,48)+4,104);put32(fixture(0x70000300,48)+8,103);
    bytes=fixture(0x60000000,72);put32(bytes,4);put32(bytes+12,4);
    {unsigned j;bytes=fixture(0x70000800,64);memset(bytes,0xff,64);
        for(j=0;j<4;++j){unsigned slot=(101+j)&7;put32(bytes+slot*8,101+j);put32(bytes+slot*8+4,j);}}
    put32(fixture(0x60000000,72)+40,8);putptr(fixture(0x70000600,32)+24,0x70003900);
    bytes=segment(0x70003900,24);put32(bytes,1001);put32(bytes+8,104);
    bytes=fixture(0x70000700,192);put32(bytes+144+4,22);put32(bytes+144+12,1);
    /* Backpack is a distinct, inventory-observed weapon descriptor too. */
    put32(fixture(0x70000300,48)+12,105);
    bytes=fixture(0x60000000,72);put32(bytes,5);put32(bytes+12,5);
    bytes=fixture(0x70000800,64);put32(bytes+8,105);put32(bytes+12,4);
    putptr(fixture(0x70000600,40)+32,0x70004900);
    bytes=segment(0x70004900,24);memcpy(bytes,(unsigned char[]){0x58,0x9a,0x23,0x09,0x71,0xa4,0xf9,0x4e},8);put32(bytes+8,105);
    bytes=fixture(0x70000700,240);put32(bytes+192+4,22);put32(bytes+192+12,3);
    lifecycle_snapshot=life_read();if(!lifecycle_snapshot.valid)fprintf(stderr,"new fixture %s\n",last_reader.failure);
    assert(lifecycle_snapshot.valid && life_apply(&lifecycle_snapshot));lifecycle_snapshot=life_read();
    utility_evidence=named(103);ordinary_evidence=named(102);ordinary_evidence.parent=0;
    create(USS_PAIR_ORDINARY_FIRST,0);finish();
    memset(&core,0,sizeof(core));core.api=&fake;core.mod=MOD;core.life=&life;core.policy=&core_flags;
    core.platform.thread_id=core_thread;core.base=BASE;core.module_size=0x4000000;
    /* Supply component and stored/local magazine counters/settings. */
    ptr_segment(BASE+0x33265e8,0xd0000000);bytes=segment(0xd0000000,88);
    putptr(bytes+0x20,0xd0001000);put32(bytes+0x28,4);put32(bytes+0x2c,UINT32_MAX);put32(bytes+0x30,1);
    putptr(bytes+0x38,0xd0002000);putptr(bytes+0x50,0xd0003000);
    bytes=segment(0xd0001000,32);memset(bytes,0xff,32);put32(bytes+8,105);put32(bytes+12,0);
    ptr_segment(0xd0002000,0x70004900);bytes=segment(0xd0003000,8);put32(bytes,3);put32(bytes+4,17);
    ptr_segment(BASE+0x3326648,0xd1000000);bytes=segment(0xd1000000,176);
    putptr(bytes+0x20,0xd1001000);put32(bytes+0x28,8);put32(bytes+0x2c,UINT32_MAX);put32(bytes+0x30,1);
    putptr(bytes+0x38,0xd1002000);putptr(bytes+0x50,0xd1003000);
    putptr(bytes+0x60,0xd1004000);put32(bytes+0x68,4);put32(bytes+0x6c,UINT32_MAX);put32(bytes+0x70,1);putptr(bytes+0xa0,0xd1005000);
    bytes=segment(0xd1001000,64);memset(bytes,0xff,64);
    put32(bytes+6*8,102);put32(bytes+6*8+4,0);put32(bytes+5*8,101);put32(bytes+5*8+4,1);put32(bytes,104);put32(bytes+4,2);
    bytes=segment(0xd1002000,24);putptr(bytes,0x70001900);putptr(bytes+8,0x70000900);putptr(bytes+16,0x70003900);
    bytes=segment(0xd1003000,36);put32(bytes,10);put32(bytes+12,2);put32(bytes+24,2);
    bytes=segment(0xd1004000,32);memset(bytes,0xff,32);put32(bytes+16,102);put32(bytes+20,0);
    word_segment(0xd1005090,2);
    /* Full inventory header mirrors its existing subrange segments. */
    bytes=fixture(0x40000040,32);putptr(bytes+24,0xd2000000);
    bytes=segment(0x40000000,96);memcpy(bytes+40,fixture(0x40000028,20),20);memcpy(bytes+64,fixture(0x40000040,32),32);
    bytes=segment(0xd2000000,8);put32(bytes,2);put32(bytes+4,4);
    reads=module_calls=core_threads=0;
}
#ifndef USS_REFILL_CORE_TEST_MAIN
#define USS_REFILL_CORE_TEST_MAIN main
#endif
int USS_REFILL_CORE_TEST_MAIN(void)
{
    UssRefillPairView out,before;UssRefillMemoryIo io;unsigned i,total;unsigned char bytes[4];
    (void)lifecycle_regression_checks;
    refill_core_fixture();assert(uss_refill_core_pair(&core,&out) && out.ordinary==102 && out.primary==101 && out.secondary==104);
    {UssRefillTarget target;UssSupplySnapshot supply;unsigned have;UssCoreObservation current=life_read();
        assert(uss_refill_core_sample(&core,&current,&target,&supply,&have));
        assert(!strcmp(core.reason,"REFILL_SAMPLE_READY"));
        /* Missing amount is a settings failure, distinct from pair admission. */
        put32(fixture(0xd1005090,4),1001);
        assert(!uss_refill_core_sample(&core,&current,&target,&supply,&have));
        assert(!strcmp(core.reason,"REFILL_AMOUNT_UNKNOWN"));
        put32(fixture(0xd1005090,4),2);
        put32(fixture(0x70000300,48)+8,102);current=life_read();
        assert(!uss_refill_core_sample(&core,&current,&target,&supply,&have));
        assert(!strcmp(core.reason,"REFILL_PAIR_NOT_STORED"));
        refill_core_fixture();assert(uss_refill_core_pair(&core,&out));
    }
    total=reads;assert(total>0);printf("Core pair snapshot fixture reads: %u\n",total);io=uss_refill_core_io(&core);
    assert(io.context(io.user)==life.owner.epoch && io.read(io.user,0x70000300,bytes,4) && refill_u32(bytes)==101);
    fail_at=reads+1;assert(!io.read(io.user,0x70000300,bytes,4));fail_at=0;
    for(i=0;i<7;++i){refill_core_fixture();
        if(i==0)life.phase=USS_LIFE_PENDING;if(i==1)core_flags.down=1;if(i==2)core_flags.known=0;
        if(i==3)life.owner.ready=0;if(i==4)++life.generation;if(i==5)core_flags.input=1;if(i==6)core_thread_change=1;
        memset(&out,0xa5,sizeof(out));before=out;assert(!uss_refill_core_pair(&core,&out) && !memcmp(&out,&before,sizeof(out)));}
    refill_core_fixture();put32(fixture(0x70000700,240)+48+4,0);
    assert(!uss_refill_core_pair(&core,&out));
    refill_core_fixture();{UssSupplySnapshot supply;UssSupplyObserver observer={0};UssRefillEvidence event;
        uint64_t serial=0;assert(uss_refill_core_supply(&core,&supply) && supply.pack.reserve==3 && supply.signal_mask==27);
        assert(!uss_supply_observe(&observer,&supply,1000,1,0,&serial,&event));
        put32(fixture(0xd0003000,8),2);put32(fixture(0xd2000000,8),3);
        assert(uss_refill_core_supply(&core,&supply));assert(!uss_supply_observe(&observer,&supply,1025,1,0,&serial,&event));
        assert(uss_refill_core_supply(&core,&supply));assert(uss_supply_observe(&observer,&supply,1225,1,0,&serial,&event));
        assert(uss_refill_core_verify_pack(&core,&event,1225));
        assert(!uss_refill_core_verify_pack(&core,&event,1726));
        {uint64_t manager=event.pack_manager;event.pack_manager=manager+8;
            assert(!uss_refill_core_verify_pack(&core,&event,1225));event.pack_manager=manager;}
        {unsigned char* descriptor=segment(0xd0004000,24);memcpy(descriptor,fixture(0x70004900,24),24);
            putptr(fixture(0xd0002000,8),0xd0004000);
            assert(!uss_refill_core_verify_pack(&core,&event,1225));putptr(fixture(0xd0002000,8),0x70004900);}
        put32(fixture(0xd0003000,8),1);assert(!uss_refill_core_verify_pack(&core,&event,1225));}
    refill_core_fixture();core_flags.paused=1;{UssSupplySnapshot supply;assert(!uss_refill_core_supply(&core,&supply));}
    assert(!uss_refill_core_verify_pack(&core,NULL,1000));
    refill_core_fixture();{UssRefillEntryIo entries=uss_refill_windows_entries(&core);UssRefillEntries output;
        assert(!uss_refill_entries_read(&entries,&output));
        {UssRefillRuntime runtime={0};UssRefillServices services;UssRefillTarget target;
            runtime.core=&core;runtime.now=1000;services=uss_refill_runtime_services(&runtime);
            target=services.observe(services.user);assert(uss_refill_target_valid(&target));
            assert(!services.verify_evidence(services.user,NULL,&target));
            assert(services.refill(services.user,&target,0.5f)==USS_REFILL_CALL_NOT_ENTERED);
        }} /* No real image at synthetic base. */
    puts("USS concrete Core refill: lifecycle_snapshot lifecycle/member reads, real secondary, admission/context/attachment refusal and SDK zero-success conversion PASS");return 0;
}
