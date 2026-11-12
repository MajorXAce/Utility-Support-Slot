/* Genuine negative record -> native expiry -> second selection, OWN memory only. */
#define USS_HUD_PRESENTATION_TEST_MAIN drop_presentation_regressions
#include "uss_hud_presentation_harness.c"
static unsigned queue_fault;
static int drop_fixture_read(uintptr_t at,void* output,size_t length)
{
    if(queue_fault && at==(uintptr_t)hud_allocation+0x394cd8){
        if((queue_fault==1 && (hud.drop.consumed&1) && hud_word_at(20)==UINT32_MAX) ||
            (queue_fault==2 && (hud.drop.consumed&2) && u32(hud_allocation+0x394b44)==0) ||
            (queue_fault==4 && (hud.drop.consumed&4) && hud_word_at(20)==UINT32_MAX))return 1;
    }
    return presentation_fixture_read(at,output,length);
}
static void drop_tick(void){replacement_now+=100;uss_hud_runtime_poll(&hud,replacement_now);}
static void drop_scene(unsigned consumed,unsigned baseline)
{
    presentation_scene();(void)drop_presentation_regressions;uss_hud_runtime_cancel(&hud);
    solo_draw(&automatic_runtime.life.owner.utility);automatic_formation.invoke=recovery_own_effect;
    assert(recovery_update()==USS_CONTROL_IDLE);empty_support(consumed,1);hud_record(103,0,UINT32_MAX);
    (void)recovery_update();finish_recovery();uss_hud_runtime_restored(&hud,&recovery.work,replacement_now);
    assert(hud.drop.active && hud.handoff.active);
    queue_fault=0;fake.read=drop_fixture_read;presentation_widget(0); /* Wrong ordinary text on first Silo removal. */
    put32(hud_allocation+0x394fe0,UINT32_MAX);put32(hud_allocation+0x394fec,UINT32_MAX);put32(hud_allocation+0x394fe8,103);
    hud_word_at_set(28,102);hud_word_at_set(24,1);
    if(baseline){drop_tick();assert(hud.drop.baseline && !hud.handoff.active);
        assert(present_calls==(consumed?0u:5u));
        if(!consumed)assert(u32((void*)(present_widget()+0x1cc0+0x110))==202);
    }
}
static void second_drop(void)
{
    put32(fixture(0x70000300,48)+8,0);put32(fixture(0x70000700,240)+48+4,0);set_selected(1);
    hud_word_at_set(16,0);hud_word_at_set(24,0);hud_word_at_set(20,UINT32_MAX-1);hud_word_at_set(28,102);hud_word_at_set(32,102);
    put32(hud_allocation+0x394fe0,UINT32_MAX-1);put32(hud_allocation+0x394fec,UINT32_MAX-1);put32(hud_allocation+0x394b44,0x40a00000);
}
static void normalized_quantities(void)
{put32(hud_allocation+0x394fe0,UINT32_MAX);put32(hud_allocation+0x394fec,UINT32_MAX);}
static void native_first_end(void)
{hud_word_at_set(20,0);put32(hud_allocation+0x394fdc,UINT32_MAX);hud_allocation[0x394ff0]=0;}
int main(void)
{
    unsigned consumed,before,mode;unsigned char old_record[40],wanted[40];uint32_t minus_one=UINT32_MAX;
    hud_allocation=VirtualAlloc(NULL,0x396000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);assert(hud_allocation);
    for(consumed=0;consumed<2;++consumed){
        drop_scene(consumed,1);second_drop();
        memcpy(old_record,hud_allocation+0x394cd8,40);memcpy(wanted,old_record,40);memcpy(wanted+20,&minus_one,4);
        drop_tick();assert(hud.drop.active && hud.drop.stage==1 && hud.drop.consumed==1);
        assert(!memcmp(wanted,hud_allocation+0x394cd8,40)); /* Only -2 -> first -1. */
        normalized_quantities();drop_tick();assert(hud.drop.consumed==3 && u32(hud_allocation+0x394b44)==0 && hud_word_at(20)==UINT32_MAX);
        native_first_end();drop_tick();assert(hud.drop.stage==2 && hud.drop.consumed==7 && hud_word_at(20)==UINT32_MAX);
        put32(hud_allocation+0x394fdc,10);put32(hud_allocation+0x394fe8,102);normalized_quantities();
        memcpy(&expected_name,weapon_assets+0x17c*0x38+0x30,8);drop_tick();
        assert(!hud.drop.active && !hud.drop.disabled && hud_word_at(20)==UINT32_MAX);
        assert(present_calls==(consumed?0u:10u));
        if(!consumed)assert(u32((void*)(present_widget()+0x1cc0+0x110))==201);
        before=hud_reads;drop_tick();assert(hud_reads==before); /* No idle observer. */
    }
    /* Without the earlier active first-drop baseline, a coalesced -2 is not ours. */
    drop_scene(0,0);second_drop();memcpy(old_record,hud_allocation+0x394cd8,40);drop_tick();
    assert(!hud.drop.baseline && !hud.drop.consumed && !memcmp(old_record,hud_allocation+0x394cd8,40));
    /* Foreign pending quantities, new support, unknown lifetime, or an expiry
     * cancel the queue; none can cause submission into a newer message. */
    for(mode=0;mode<4;++mode){
        drop_scene(0,1);second_drop();drop_tick();assert(hud.drop.stage==1);
        if(mode==0)hud_word_at_set(20,1);if(mode==1)put32(fixture(0x70000300,48)+8,104);
        if(mode==2)++automatic_runtime.life.owner.guard_generation;if(mode==3)replacement_now+=20001;
        memcpy(old_record,hud_allocation+0x394cd8,40);drop_tick();
        assert(!hud.drop.active && !(hud.drop.consumed&4) && !memcmp(old_record,hud_allocation+0x394cd8,40));
    }
    /* Each of the three OS writes is consumed before entry; a failed readback
     * disables only the queue, preserves actual effects, and is never retried. */
    for(mode=1;mode<=4;mode*=2){
        drop_scene(0,1);second_drop();if(mode==1)queue_fault=mode;drop_tick();
        if(mode>=2){normalized_quantities();if(mode==2)queue_fault=mode;drop_tick();}
        if(mode==4){native_first_end();queue_fault=mode;drop_tick();}
        assert(hud.drop.disabled && !hud.drop.active && (hud.drop.consumed&mode) && !hud.ack.disabled && !automatic_controller.stopped);
        before=hud.drop.consumed;queue_fault=0;memcpy(old_record,hud_allocation+0x394cd8,40);drop_tick();
        assert(hud.drop.consumed==before && !memcmp(old_record,hud_allocation+0x394cd8,40));
        uss_hud_runtime_retire(&hud);assert(hud.drop.disabled);
    }
    assert(VirtualFree(hud_allocation,0,MEM_RELEASE));
    puts("USS two-drop HUD: own memory/native setters, observed first baseline, exact -2 split, hold-zero native expiry, one idle-only second -1, later selection, consumed fallback, guards and three-write no-retry PASS");return 0;
}
