/* Pickup-prompt hygiene (uss_zone_hygiene.h) over owned buffers, no game access.
 * The game's ZONE_CLOSE is replaced by a recording fake; the module itself must never write game memory. */
#define USS_CORE_TEST_MAIN original_core_checks
#include "core_observation_harness.c"
#include "../src/core_lifecycle.h"
#include "../src/uss_zone_hygiene.h"

static unsigned zc_calls,zc_zone;static uintptr_t zc_function,zc_events;static uint32_t zc_entity;static int zc_result=1;
static int fake_zone_close(void* user,uintptr_t function,uintptr_t events,uint32_t entity,uint32_t zone)
{assert(user==(void*)&zc_calls);++zc_calls;zc_function=function;zc_events=events;zc_entity=entity;zc_zone=zone;return zc_result;}
static unsigned nat_calls,nat_zone,nat_open;static uint32_t nat_entity;static void* nat_events;
static void fake_native(void* events,uint32_t entity,uint32_t zone,uint8_t open)
{++nat_calls;nat_events=events;nat_entity=entity;nat_zone=zone;nat_open=open;}
/* Read wrapper: on read number wrap_at (counting from wrap_reads=0) the returned 8 bytes become another pointer value. */
static unsigned wrap_at,wrap_reads;static int (*real_read)(uintptr_t,void*,size_t);
static int wrapped_read(uintptr_t a,void* out,size_t n)
{int r=real_read(a,out,n);if(++wrap_reads==wrap_at && !r && n==8){uintptr_t other=0x90000100;memcpy(out,&other,sizeof(other));}return r;}
static UssCoreLifecycle zlife;static UssZoneHygiene zh;
/* Prologue with a non-zero rip displacement: the displacement bytes are a wildcard. */
static const unsigned char zprologue[24]={0x41,0x56,0x41,0x57,0x48,0x83,0xec,0x28,0x3b,0x15,0xDE,0xAD,0xBE,0xEF,
    0x45,0x0f,0xb6,0xf1,0x45,0x8b,0xf8,0x4c,0x8b,0xd1};
/* Entity 103 (utility, Silo identity) is row index 0, entity 102 (ordinary) is row index 1 of the interaction rows. */
static unsigned char* zrow(uint32_t entity){return fixture(0x90001000,2*0x370)+(entity==102?1:0)*0x370;}
static void open_zone(uint32_t entity){put32(zrow(entity)+0xA8,154);put32(zrow(entity)+0x360,2);}
static void close_zone(uint32_t entity){put32(zrow(entity)+0xA8,UINT32_MAX);put32(zrow(entity)+0x360,0);}
static void zone_fixture(void)
{
    core_fixture();core_pair();
    memcpy(segment(BASE+USS_ZONE_CLOSE_RVA,24),zprologue,24);
    memset(&zlife,0,sizeof(zlife));zlife.owner=core_owner;zlife.phase=USS_LIFE_FORMED;
    memset(&zh,0,sizeof(zh));zh.invoke=fake_zone_close;zh.invoke_user=&zc_calls;
    zc_calls=0;zc_result=1;logs=0;
}
static unsigned step(const UssCoreObservation* current,uint64_t now)
{return uss_zone_hygiene_step(&zh,&zlife,current,&fake,MOD,now);}
int main(void)
{
    UssCoreObservation current,bad;unsigned char before[2*0x370],weapons[96];
    (void)uss_lifecycle_flags;(void)uss_lifecycle_observe;(void)uss_lifecycle_create;
    (void)uss_lifecycle_replace_begin;(void)uss_lifecycle_replace_commit;(void)uss_lifecycle_complete;
    assert(original_core_checks()==0);

    /* Closed zones: nothing to do, no call, no log. */
    zone_fixture();current=core_read();assert(current.valid && current.pair_matched);
    assert(!step(&current,1000) && !zc_calls && !logs);

    /* Open ground zone + settled 2 on the carried ordinary member: exactly one call, with the exact arguments. */
    zone_fixture();open_zone(102);current=core_read();
    assert(current.valid && current.pair_matched && current.ordinary.zone==154 && current.ordinary.tail==2);
    memcpy(before,zrow(103),2*0x370);memcpy(weapons,fixture(0x70000700,144)+48,sizeof(weapons));
    assert(step(&current,1000) && zc_calls==1);
    assert(zc_function==BASE+USS_ZONE_CLOSE_RVA && zc_events==0x90000000 && zc_entity==102 && zc_zone==USS_ZONE_GROUND);
    assert(logs==1 && strstr(last_log,"closed pickup zone entity=102 handle=154 attempt=1"));
    /* The module itself writes nothing to the game (the fake does not either). */
    assert(!memcmp(before,zrow(103),2*0x370) && !memcmp(weapons,fixture(0x70000700,144)+48,sizeof(weapons)));

    /* Bounded: wait between attempts, two attempts, then give up with one log line; no more calls. */
    assert(!step(&current,1500) && zc_calls==1);              /* retry interval not elapsed */
    assert(step(&current,2000) && zc_calls==2);              /* second and last attempt */
    assert(!step(&current,3000) && zc_calls==2 && strstr(last_log,"gave up entity=102"));
    {unsigned seen=logs;assert(!step(&current,4000) && !step(&current,5000) && logs==seen && zc_calls==2);}
    /* A new pair generation starts over. */
    zlife.owner.pair_generation+=1;assert(step(&current,6000) && zc_calls==3);
    /* Once the zone reads closed again, earlier attempts are forgotten and a later reopening is handled. */
    zone_fixture();open_zone(102);current=core_read();assert(step(&current,1000) && zc_calls==1);
    close_zone(102);current=core_read();assert(!step(&current,1100) && zc_calls==1);
    open_zone(102);current=core_read();assert(step(&current,1200) && zc_calls==2);

    /* The utility member is handled the same way; both open: one call per step, utility first. */
    zone_fixture();open_zone(103);current=core_read();
    assert(step(&current,1000) && zc_calls==1 && zc_entity==103);
    zone_fixture();open_zone(103);open_zone(102);current=core_read();
    assert(step(&current,1000) && zc_entity==103 && zc_calls==1);
    close_zone(103);current=core_read();assert(step(&current,1100) && zc_entity==102 && zc_calls==2);

    /* Only the exact prompting state: open handle AND settled 2. */
    zone_fixture();put32(zrow(102)+0xA8,154);put32(zrow(102)+0x360,0);current=core_read();assert(!step(&current,1000) && !zc_calls);
    zone_fixture();put32(zrow(102)+0xA8,UINT32_MAX);put32(zrow(102)+0x360,2);current=core_read();assert(!step(&current,1000) && !zc_calls);
    zone_fixture();put32(zrow(102)+0xA8,0x10000);put32(zrow(102)+0x360,2);current=core_read();assert(!step(&current,1000) && !zc_calls);
    zone_fixture();put32(zrow(102)+0xA8,0);put32(zrow(102)+0x360,2);current=core_read();assert(!step(&current,1000) && !zc_calls);

    /* Evidence gates: ownership, identity, classification, status, pair match, lifecycle phase, validity. */
    zone_fixture();open_zone(102);current=core_read();assert(current.valid && current.pair_matched);
    bad=current;bad.ordinary.parent_matches=0;assert(!step(&bad,1000) && !zc_calls);
    bad=current;bad.ordinary.descriptor[0]^=1;assert(!step(&bad,1000) && !zc_calls);
    bad=current;bad.ordinary.classification=USS_UTILITY_SUPPORT;assert(!step(&bad,1000) && !zc_calls);
    bad=current;bad.ordinary.status=USS_MEMBER_EMPTY;assert(!step(&bad,1000) && !zc_calls);
    bad=current;bad.ordinary.entity=999;assert(!step(&bad,1000) && !zc_calls);
    bad=current;bad.pair_matched=0;assert(!step(&bad,1000) && !zc_calls);
    bad=current;bad.valid=0;assert(!step(&bad,1000) && !zc_calls);
    zlife.phase=USS_LIFE_REPLACING;assert(!step(&current,1000) && !zc_calls);
    zlife.phase=USS_LIFE_PAUSED;assert(!step(&current,1000) && !zc_calls);
    zlife.phase=USS_LIFE_FORMED;zlife.owner.pair_present=0;assert(!step(&current,1000) && !zc_calls);
    zlife.owner.pair_present=1;assert(step(&current,1000) && zc_calls==1);
    /* Disabled (no invoker), and the session cap. */
    zone_fixture();open_zone(102);current=core_read();zh.invoke=NULL;assert(!step(&current,1000) && !zc_calls);
    zone_fixture();open_zone(102);current=core_read();zh.calls_total=USS_ZONE_HYGIENE_SESSION_CAP;assert(!step(&current,1000) && !zc_calls);
    zone_fixture();open_zone(102);current=core_read();assert(!uss_zone_hygiene_step(NULL,&zlife,&current,&fake,MOD,1000));
    assert(!uss_zone_hygiene_step(&zh,NULL,&current,&fake,MOD,1000) && !uss_zone_hygiene_step(&zh,&zlife,NULL,&fake,MOD,1000));

    /* Mutation-boundary refusals: the attempt is consumed, the game function is never called. */
    zone_fixture();open_zone(102);current=core_read();
    fixture(BASE+USS_ZONE_CLOSE_RVA,24)[0]^=1;                       /* wrong prologue byte */
    assert(!step(&current,1000) && !zc_calls && zh.slot[1].attempts==1);
    zone_fixture();open_zone(102);current=core_read();
    fixture(BASE+USS_ZONE_CLOSE_RVA,24)[20]^=1;                      /* last prologue byte */
    assert(!step(&current,1000) && !zc_calls);
    zone_fixture();open_zone(102);current=core_read();
    close_zone(102);                                                  /* zone closed before the boundary re-read */
    assert(!step(&current,1000) && !zc_calls);
    zone_fixture();open_zone(102);current=core_read();
    put32(fixture(0x70000300,48)+28,3);                               /* selection changed since the observation */
    assert(!step(&current,1000) && !zc_calls);
    zone_fixture();open_zone(102);current=core_read();
    fake.build_id=NULL;assert(!step(&current,1000) && !zc_calls);fake.build_id=build_id;
    zone_fixture();open_zone(102);current=core_read();
    fake.module_base=NULL;assert(!step(&current,1000) && !zc_calls);fake.module_base=module_capture;
    zone_fixture();open_zone(102);current=core_read();
    fail_at=reads+1;assert(!step(&current,1000) && !zc_calls);fail_at=0;   /* the next read fails at the boundary */
    zone_fixture();open_zone(102);current=core_read();
    putptr(fixture(BASE+0x3326D68,8),0x90000100);                    /* interaction manager moved */
    assert(!step(&current,1000) && !zc_calls);
    putptr(fixture(BASE+0x3326D68,8),0x90000000);
    /* The manager pointer changes BETWEEN the member read and the final re-read: refused. First count the boundary reads of a
     * normal pass, then change the last one (the final 8-byte pointer read). */
    zone_fixture();open_zone(102);current=core_read();
    real_read=fake.read;fake.read=wrapped_read;wrap_at=0;wrap_reads=0;
    assert(step(&current,1000) && zc_calls==1);
    {unsigned boundary_reads=wrap_reads;
     zone_fixture();open_zone(102);current=core_read();
     real_read=fake.read;fake.read=wrapped_read;wrap_at=boundary_reads;wrap_reads=0;
     assert(!step(&current,1000) && !zc_calls);}
    fake.read=real_read;

    /* The production forwarder passes the exact arguments and closes (open = 0); anything missing is refused. */
    {static int marker;
     nat_calls=0;
     assert(uss_zone_close_invoke(NULL,(uintptr_t)fake_native,(uintptr_t)&marker,102,1));
     assert(nat_calls==1 && nat_events==&marker && nat_entity==102 && nat_zone==1 && nat_open==0);
     assert(!uss_zone_close_invoke(NULL,0,(uintptr_t)&marker,102,1));
     assert(!uss_zone_close_invoke(NULL,(uintptr_t)fake_native,0,102,1));
     assert(!uss_zone_close_invoke(NULL,(uintptr_t)fake_native,(uintptr_t)&marker,0,1));
     assert(nat_calls==1);}
    puts("USS zone hygiene: exact prompting state only, owned pair members, bounded attempts/give-up, boundary re-proof, no writes PASS");
    return 0;
}
