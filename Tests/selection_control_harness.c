#ifdef NDEBUG
#error Controller checks require active assertions
#endif
#define USS_SELECTION_TEST_MAIN selection_regression_checks
#include "guarded_selection_harness.c"
#undef USS_SELECTION_TEST_MAIN
#include "../src/selection_control.h"
#include "../src/selection_entry_pins.h"
static uint64_t control_now;
static uint32_t control_thread_id,control_game_state;
static uint32_t test_game_state(HfMod* mod){assert(mod==MOD);return control_game_state;}
static UssControlToken request_token,arm_token,written_ready;
static int request_present,arm_present,write_ok,bind_ok;
static unsigned binds,ready_writes,bind_change_origin;
static uint64_t test_clock(void* user){(void)user;return control_now;}
static uint32_t test_thread(void* user){(void)user;return control_thread_id;}
static int test_read(void* user,unsigned phase,UssControlToken* control_token)
{
    int present=phase?arm_present:request_present;(void)user;
    *control_token=phase?arm_token:request_token;return present;
}
static int test_write(void* user,const UssControlToken* control_token)
{(void)user;written_ready=*control_token;++ready_writes;return write_ok;}
static int test_bind(void* user,const HfApi* api,const UssActionState* snapshot,UssNativeCalls* targets)
{(void)user;assert(api==&fake && snapshot->valid);++binds;*targets=selection_targets();if(bind_change_origin){put32(fixture(0x70000300,48)+28,2);put32(fixture(0x70000500,4),102);}return bind_ok;}
static void control_fixture(UssSelectionControl* control,UssControlServices* services)
{
    (void)prepare_selection();
    put32(fixture(0x70000300,48)+8,0); /* Control has no managed support pair. */
    memset(control,0,sizeof(*control));memset(services,0,sizeof(*services));
    control->session.magic=USS_CONTROL_MAGIC;control->session.version=USS_CONTROL_VERSION;
    control->session.pid=77;control->session.process_started=123456;
    control->session.nonce=UINT64_C(0xfedcba9876543211);control->started_ms=100;
    request_token=control->session;memset(&arm_token,0,sizeof(arm_token));
    request_present=arm_present=0;write_ok=bind_ok=1;binds=ready_writes=bind_change_origin=0;
    control_now=100;control_thread_id=55;control_game_state=HF_STATE_MISSION;fake.game_state=test_game_state;
    services->clock_ms=test_clock;services->thread_id=test_thread;
    services->read_token=test_read;services->write_ready=test_write;services->bind_selection=test_bind;
}
static void prepare_request(UssSelectionControl* control,UssControlServices* services)
{
    request_present=1;uss_selection_control_tick(control,&fake,MOD,services);
    assert(control->phase==1 && ready_writes==1 && !selection_count && !binds);
    assert(written_ready.avatar==22 && written_ready.local_unit==11);
    arm_token=written_ready;arm_present=1;control_now+=250;
}
#ifndef USS_CONTROL_TEST_MAIN
#define USS_CONTROL_TEST_MAIN main
#endif
int USS_CONTROL_TEST_MAIN(void)
{
    UssSelectionControl control;UssControlServices services;unsigned i,prior_reads;
    Reader reader={0};UssSelectionAddresses addresses;
    assert(selection_regression_checks()==0);
    control_fixture(&control,&services);
    for(i=0;i<4;++i){uss_selection_control_tick(&control,&fake,MOD,&services);control_now+=250;}
    assert(!reads && !selection_count && !ready_writes); /* Idle never scans game memory. */
    prepare_request(&control,&services);
    uss_selection_control_tick(&control,&fake,MOD,&services);
    assert(control.phase==2 && selection_count==2 && binds==1);
    assert(strstr(last_log,"index_verified=1 held_matches=1"));
    prior_reads=reads;uss_selection_control_tick(&control,&fake,MOD,&services);
    assert(reads==prior_reads && selection_count==2);
    for(i=0;i<7;++i) {
        control_fixture(&control,&services);prepare_request(&control,&services);
        switch(i){
        case 0:arm_token.pid++;break;case 1:arm_token.process_started++;break;
        case 2:arm_token.nonce++;break;case 3:arm_token.avatar++;break;
        case 4:arm_token.local_unit++;break;case 5:arm_token.reserved++;break;
        case 6:arm_present=-1;break;
        }
        uss_selection_control_tick(&control,&fake,MOD,&services);
        assert(control.phase==2 && !selection_count && !binds);
    }
    control_fixture(&control,&services);request_present=1;request_token.pid++;
    uss_selection_control_tick(&control,&fake,MOD,&services);assert(control.phase==2 && !reads && !selection_count);
    control_fixture(&control,&services);request_present=1;control_game_state=HF_STATE_SHIP;
    uss_selection_control_tick(&control,&fake,MOD,&services);assert(control.phase==2 && !reads && !selection_count);
    control_fixture(&control,&services);request_present=1;initial_role=HF_SESSION_HOST;
    uss_selection_control_tick(&control,&fake,MOD,&services);assert(control.phase==2 && !reads && !selection_count);
    control_fixture(&control,&services);control_now+=1800001;
    uss_selection_control_tick(&control,&fake,MOD,&services);assert(control.phase==2 && !reads && !selection_count);
    control_fixture(&control,&services);prepare_request(&control,&services);control_now+=30001;
    uss_selection_control_tick(&control,&fake,MOD,&services);assert(control.phase==2 && !selection_count);
    control_fixture(&control,&services);prepare_request(&control,&services);control_thread_id++;
    uss_selection_control_tick(&control,&fake,MOD,&services);assert(control.phase==2 && !selection_count);
    control_fixture(&control,&services);prepare_request(&control,&services);bind_ok=0;
    uss_selection_control_tick(&control,&fake,MOD,&services);assert(control.phase==2 && !selection_count);
    control_fixture(&control,&services);prepare_request(&control,&services);
    put32(fixture(0x70000300,48)+28,2);put32(fixture(0x70000500,4),102);
    uss_selection_control_tick(&control,&fake,MOD,&services);assert(control.phase==2 && !selection_count);
    control_fixture(&control,&services);prepare_request(&control,&services);bind_change_origin=1;
    uss_selection_control_tick(&control,&fake,MOD,&services);assert(control.phase==2 && !selection_count && binds==1);
    control_fixture(&control,&services);write_ok=0;request_present=1;
    uss_selection_control_tick(&control,&fake,MOD,&services);assert(control.phase==2 && !selection_count);
    control_fixture(&control,&services);prepare_request(&control,&services);selection_mode=0;
    uss_selection_control_tick(&control,&fake,MOD,&services);
    assert(control.phase==2 && selection_count==2 && strstr(last_log,"index_verified=0"));
    /* Pin validation is exercised without calling the returned synthetic addresses. */
    setup();reader.api=&fake;
    {unsigned char state[64],weapon[64];FILE* f=fopen("local-evidence/hf-uss-027/pin-fixture.bin","rb");
     assert(f && fread(state,1,64,f)==64 && fread(weapon,1,64,f)==64);fclose(f);
     memcpy(segment(BASE+0xA93E90,64),state,64);memcpy(segment(BASE+0x9AAA50,64),weapon,64);}
    assert(uss_selection_entry_pins(&reader,BASE,0x4000000,&addresses));
    assert(addresses.state==BASE+0xA93E90 && addresses.weapon==BASE+0x9AAA50);
    fixture(BASE+0xA93E90,64)[0]^=1;
    assert(!uss_selection_entry_pins(&reader,BASE,0x4000000,&addresses) && !addresses.state && !addresses.weapon);
    fixture(BASE+0xA93E90,64)[0]^=1; /* Restore first entry; fail the second exact request partially. */
    fail_at=reads+2;
    assert(!uss_selection_entry_pins(&reader,BASE,0x4000000,&addresses) && !addresses.state && !addresses.weapon);
    puts("selection control: explicit request/character arm, expiry/thread/context/binder refusal, single effect, exact pins PASS");
    return 0;
}
