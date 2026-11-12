#define USS_FORMATION_TEST_MAIN formation_runtime_regression_checks
#include "formation_runtime_harness.c"
static UssFormationNative fixture_native;
static unsigned select_fail_after_entry,send_fail_after_entry;
static const unsigned char fixture_select_state[64]={0x4c,0x8b,0xdc,0x55,0x57,0x48,0x8b,0xec,0x48,0x83,0xec,0x78,0x48,0x8b,0x05,0x6d,0x81,0xba,0x01,0x48,0x33,0xc4,0x48,0x89,0x45,0xe0,0x8b,0x01,0x48,0x8b,0xf9,0x49,0x89,0x5b,0x10,0x33,0xdb,0x3b,0x05,0x65,0xfd,0x9e,0x02,0x49,0x89,0x73,0x18,0x8b,0xf2,0x4d,0x89,0x63,0x20,0x4d,0x89,0x6b,0xe8,0x4d,0x89,0x73,0xe0,0x4c,0x8b,0x35};
static const unsigned char fixture_select_weapon[64]={0x48,0x8b,0xc4,0x44,0x89,0x40,0x18,0x53,0x55,0x41,0x55,0x48,0x81,0xec,0x80,0x00,0x00,0x00,0x3b,0x15,0xb8,0x91,0xad,0x02,0x41,0x8b,0xe8,0x8b,0xda,0x4c,0x8b,0xe9,0x0f,0x84,0x94,0x06,0x00,0x00,0x44,0x8b,0x49,0x30,0x44,0x8b,0x51,0x38,0x48,0x89,0x70,0x08,0x4c,0x89,0x70,0xd0,0x45,0x33,0xf6,0x44,0x0f,0xaf,0xd3,0x41,0x8d,0x71};

static int fixture_invoke(void* user,UssFormationEffect effect,uintptr_t first,uintptr_t second,
    uintptr_t record,uintptr_t inventory,uint32_t avatar,uint32_t value,const UssInventoryMessage* message)
{
    (void)user;assert(avatar==22 || effect==USS_EFFECT_SEND);
    if(effect==USS_EFFECT_SELECT){if(select_fail_after_entry)return 0;assert(first==BASE+0xA93E90 && second==BASE+0x9AAA50 && record && inventory);
        return runtime_select(NULL,&fixture_runtime.latest,value)==USS_PAIR_SERVICE_DONE;}
    if(effect==USS_EFFECT_ATTACH){UssPairMember member=candidate_id(value);assert(first==BASE+0x9AA290 && inventory);
        return runtime_attach(NULL,&fixture_runtime.latest,&member)==USS_PAIR_SERVICE_DONE;}
    assert(effect==USS_EFFECT_SEND && first==BASE+0xBE3F40 && message);
    assert(message->avatar==22 && message->primary==101 && (message->support==102 || message->support==103));
    ++sends;return !send_fail_after_entry;
}
static void native_fixture_role(unsigned role)
{
    UssFormationIo io;
    select_fail_after_entry=send_fail_after_entry=0;runtime_fixture(0,0,role);memset(&fixture_native,0,sizeof(fixture_native));
    memcpy(segment(BASE+0xA93E90,64),fixture_select_state,64);
    memcpy(segment(BASE+0x9AAA50,64),fixture_select_weapon,64);
    memcpy(segment(BASE+0x9AA290,64),uss_attachment_entry_pin,64);
    fixture_native.api=&fake;fixture_native.mod=MOD;fixture_native.platform=producer().platform;fixture_native.runtime=&fixture_runtime;fixture_native.invoke=fixture_invoke;
    io=uss_formation_native_io(&fixture_native,runtime_clock);fixture_runtime.io=io;
}
static void native_fixture(void){native_fixture_role(HF_SESSION_HOST);}
#ifndef USS_NATIVE_TEST_MAIN
#define USS_NATIVE_TEST_MAIN main
#endif
int USS_NATIVE_TEST_MAIN(void)
{
    UssPairMember target;unsigned before;
    assert(formation_runtime_regression_checks()==0);
    native_fixture();assert(uss_formation_start(&fixture_runtime,USS_PAIR_ORDINARY_FIRST,NULL));finish_work();
    target=fixture_runtime.life.owner.utility;assert(uss_formation_start(&fixture_runtime,USS_PAIR_DRAW,&target));finish_work();
    assert(fixture_runtime.work.real==103 && selections==1 && attachments==2 && sends==2 && fixture_native.effects==5);
    /* Scoped controller phase2 draw: actual captures, attachment, sender and
     * final selection all run through the production adapters on OWN memory. */
    for(unsigned role=HF_SESSION_HOST;role<=HF_SESSION_JOINER;++role)for(unsigned ordinary=0;ordinary<2;++ordinary){
        UssCaptureProducer producer_input;UssPublicationCapture capture;UssPairTransaction saved;
        UssCoreObservation denied;unsigned effects;
        native_fixture_role(role);assert(uss_formation_start(&fixture_runtime,USS_PAIR_ORDINARY_FIRST,NULL));finish_work();
        if(ordinary){target=fixture_runtime.life.owner.utility;assert(uss_formation_start(&fixture_runtime,USS_PAIR_DRAW,&target));finish_work();}
        target=ordinary?fixture_runtime.life.owner.ordinary:fixture_runtime.life.owner.utility;
        assert(runtime_select(NULL,&fixture_runtime.latest,1)==USS_PAIR_SERVICE_DONE);
        {unsigned char* row=fixture(ACTION_MANAGER+0x53dcd0,32);
         put32(row+8,3);put32(row+16,2);put32(row+20,1);put32(row+24,0);put32(row+28,0x3e99999a);
         put32(fixture(ACTION_MANAGER+0x53e880,24)+12,0x8000);}
        assert(uss_formation_start(&fixture_runtime,USS_PAIR_DRAW,&target));effects=fixture_native.effects;
        assert(!fixture_runtime.controller_draw);
        assert(uss_formation_tick(&fixture_runtime)==USS_PAIR_ACTIVE && fixture_native.effects==effects);
        fixture_runtime.controller_draw=1;assert(formation_refresh(&fixture_runtime));
        assert(formation_controller_draw_ready(&fixture_runtime,&fixture_runtime.latest));
        producer_input=producer();producer_input.user=&fixture_runtime;producer_input.input=formation_native_input;
        assert(!uss_capture_publication(&producer_input,&capture,NULL)); /* Default reader remains strict. */
        assert(uss_capture_managed_draw_publication(&producer_input,&fixture_runtime.work,now_ms,&capture,NULL));
        assert(!uss_capture_managed_draw_publication(&producer_input,&fixture_runtime.work,fixture_runtime.work.deadline_ms,&capture,NULL));
        saved=fixture_runtime.work;fixture_runtime.work.order=USS_PAIR_RETAINED;
        assert(!formation_controller_draw_ready(&fixture_runtime,&fixture_runtime.latest));fixture_runtime.work=saved;
        fixture_runtime.publication.formed=0;assert(!formation_controller_draw_ready(&fixture_runtime,&fixture_runtime.latest));fixture_runtime.publication.formed=1;
        denied=fixture_runtime.latest;denied.primary.fields.global_guard=1;assert(!formation_controller_draw_ready(&fixture_runtime,&denied));
        denied=fixture_runtime.latest;denied.inventory.held=target.entity;assert(!formation_controller_draw_ready(&fixture_runtime,&denied));
        denied=fixture_runtime.latest;denied.utility.parent_matches=0;assert(!formation_controller_draw_ready(&fixture_runtime,&denied));
        denied=fixture_runtime.latest;put32(denied.primary.fields.row+16,3);assert(!formation_controller_draw_ready(&fixture_runtime,&denied));
        ++fixture_runtime.view.key.context_generation;
        assert(!formation_controller_draw_ready(&fixture_runtime,&fixture_runtime.latest));
        --fixture_runtime.view.key.context_generation;
        if(role==HF_SESSION_HOST && !ordinary){
            fixture(BASE+0xbe3f40,sizeof(capture_pin_0))[0]^=1;
            assert(uss_formation_tick(&fixture_runtime)==USS_PAIR_ACTIVE && fixture_native.effects==effects+1);
            now_ms+=25;assert(uss_formation_tick(&fixture_runtime)==USS_PAIR_ACTIVE && fixture_native.effects==effects+1);
            fixture(BASE+0xbe3f40,sizeof(capture_pin_0))[0]^=1; /* A pre-entry capture wait never repeats consumed attachment. */

        }
        {UssPairPhase phase=uss_formation_tick(&fixture_runtime);
         if(phase!=USS_PAIR_COMPLETE)fprintf(stderr,"early role=%u ordinary=%u phase=%u reason=%s native=%s formed=%u disabled=%u effects=%u ready=%u expected=%u real=%u\n",role,ordinary,phase,fixture_runtime.work.reason,fixture_native.reason,fixture_runtime.publication.formed,fixture_runtime.publication.disabled,fixture_native.effects,formation_controller_draw_ready(&fixture_runtime,&fixture_runtime.latest),fixture_runtime.work.expected_support,fixture_runtime.latest.inventory.slots[2]);
         assert(phase==USS_PAIR_COMPLETE);}
        assert(fixture_runtime.work.real==target.entity && fixture_runtime.latest.inventory.active_selector==3 &&
            u32(fixture(ACTION_MANAGER+0x53dcd0,32)+28)==0x3e99999a && fixture_native.effects==effects+3);
        effects=fixture_native.effects;assert(uss_formation_tick(&fixture_runtime)==USS_PAIR_COMPLETE && fixture_native.effects==effects);
    }
    /* An earlier send whose entered result is uncertain remains terminal. */
    native_fixture();assert(uss_formation_start(&fixture_runtime,USS_PAIR_ORDINARY_FIRST,NULL));finish_work();
    target=fixture_runtime.life.owner.utility;
    {unsigned char* row=fixture(ACTION_MANAGER+0x53dcd0,32);
     put32(row+8,3);put32(row+16,2);put32(row+20,1);put32(row+28,0x3e99999a);
     put32(fixture(ACTION_MANAGER+0x53e880,24)+12,0x8000);}
    assert(uss_formation_start(&fixture_runtime,USS_PAIR_DRAW,&target));fixture_runtime.controller_draw=1;
    before=fixture_native.effects;send_fail_after_entry=1;
    assert(uss_formation_tick(&fixture_runtime)==USS_PAIR_CANCELLED && fixture_native.effects==before+2 && fixture_runtime.publication.disabled);
    assert(uss_formation_tick(&fixture_runtime)==USS_PAIR_CANCELLED && fixture_native.effects==before+2);
    /* Restore the settled scene for the established keyboard/readiness checks. */
    native_fixture();assert(uss_formation_start(&fixture_runtime,USS_PAIR_ORDINARY_FIRST,NULL));finish_work();
    target=fixture_runtime.life.owner.utility;assert(uss_formation_start(&fixture_runtime,USS_PAIR_DRAW,&target));finish_work();
    /* A transient known readiness blocker is a pre-entry WAIT, not a consumed
     * selection. Clearing it permits exactly the normal fresh selection path. */
    before=fixture_native.effects;target=fixture_runtime.life.owner.ordinary;
    put32(fixture(ACTION_MANAGER+0x53dcd0,32)+28,1);
    assert(uss_formation_start(&fixture_runtime,USS_PAIR_DRAW,&target));
    assert(uss_formation_tick(&fixture_runtime)==USS_PAIR_ACTIVE && !fixture_runtime.work.primary_requested);
    assert(!strcmp(fixture_native.reason,"SELECT_NOT_READY_BEFORE_ENTRY") && fixture_native.effects==before);
    now_ms+=25;assert(uss_formation_tick(&fixture_runtime)==USS_PAIR_ACTIVE && fixture_native.effects==before);
    put32(fixture(ACTION_MANAGER+0x53dcd0,32)+28,0);finish_work();
    assert(fixture_runtime.work.real==102 && fixture_runtime.work.primary_requested);
    /* A selection that was entered but not confirmed remains terminal: no
     * readiness retry can call it again. */
    target=fixture_runtime.life.owner.utility;assert(uss_formation_start(&fixture_runtime,USS_PAIR_DRAW,&target));
    select_fail_after_entry=1;before=fixture_native.effects;
    assert(uss_formation_tick(&fixture_runtime)==USS_PAIR_CANCELLED && fixture_native.effects==before+1);
    assert(!strcmp(fixture_native.reason,"SELECT_ENTRY_UNCONFIRMED"));
    assert(uss_formation_tick(&fixture_runtime)==USS_PAIR_CANCELLED && fixture_native.effects==before+1);
    /* Exact entry mismatch stops before our invoker; never calls game memory. */
    native_fixture();fixture(BASE+0x9AA290,64)[0]^=1;
    assert(uss_formation_start(&fixture_runtime,USS_PAIR_ORDINARY_FIRST,NULL));
    assert(uss_formation_tick(&fixture_runtime)==USS_PAIR_CANCELLED && !fixture_native.effects && !attachments && !sends);
    native_fixture();assert(uss_formation_start(&fixture_runtime,USS_PAIR_ORDINARY_FIRST,NULL));finish_work();
    before=fixture_native.effects;fixture(BASE+0xA93E90,64)[0]^=1;target=fixture_runtime.life.owner.utility;
    assert(uss_formation_start(&fixture_runtime,USS_PAIR_DRAW,&target));
    /* Primary already held, so first draw does not need primary selection; the
     * final draw selection refuses the corrupt entry after attachment/publication. */
    assert(uss_formation_tick(&fixture_runtime)==USS_PAIR_CANCELLED && fixture_native.effects==before+2);
    puts("Native adapter: real reader/capture integration and own invoker; corrupt attach/select pins refuse PASS");
    return 0;
}
