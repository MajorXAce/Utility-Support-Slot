/* Actual HUD context/readers and OWN private memory; no game process access. */
#include "../src/uss_gameplay_application.h"
#define USS_SOLO_TEST_MAIN hud_solo_regressions
#include "uss_solo_harness.c"
#define HUD_WEAPONS ((uintptr_t)0xd5000000)
#define HUD_ICONS ((uintptr_t)0xd5100000)
#define HUD_DISPLAY ((uintptr_t)0xd5200000)
#define HUD_COMPONENT ((uintptr_t)0xd5300000)
static UssHudRuntime hud;
static unsigned char* hud_allocation;
static unsigned char weapon_assets[0x6000],icon_assets[0x200];
static uintptr_t hud_registry;static unsigned hud_reads,hud_table_reads,hud_override,fail_hud_postwrite,unknown_icon_lookup;
static uintptr_t hud_projectile_table;static uint32_t hud_display_icon,hud_display_type;
static uint32_t hud_word_at(unsigned offset){return u32(hud_allocation+0x394cd8+offset);}
static void hud_word_at_set(unsigned offset,uint32_t value){put32(hud_allocation+0x394cd8+offset,value);}
static int hud_fixture_read(uintptr_t at,void* output,size_t length)
{
    ++hud_reads;
    if(unknown_icon_lookup && at==HUD_COMPONENT+0x20)return 1;
    if(at==BASE+0x346d538 && length==8){uintptr_t root=(uintptr_t)hud_allocation;memcpy(output,&root,8);return 0;}
    if(at>=(uintptr_t)hud_allocation && at-(uintptr_t)hud_allocation<=0x395000 && length<=0x395000-(at-(uintptr_t)hud_allocation)){
        if(fail_hud_postwrite && at==(uintptr_t)hud_allocation+0x394cd8 && hud_word_at(4)==202)return 1;
        memcpy(output,(const void*)at,length);return 0;}
    if(at==MAPPING+0xf126d8 && length==8){uintptr_t root=HUD_WEAPONS;memcpy(output,&root,8);return 0;}
    if(at==MAPPING+0xf129f8 && length==8){uintptr_t root=HUD_ICONS;memcpy(output,&root,8);return 0;}
    if(at>=HUD_WEAPONS && at-HUD_WEAPONS<=sizeof(weapon_assets) && length<=sizeof(weapon_assets)-(at-HUD_WEAPONS)){
        if(length>56)++hud_table_reads;memcpy(output,weapon_assets+(at-HUD_WEAPONS),length);return 0;}
    if(at>=HUD_ICONS && at-HUD_ICONS<=sizeof(icon_assets) && length<=sizeof(icon_assets)-(at-HUD_ICONS)){
        memcpy(output,icon_assets+(at-HUD_ICONS),length);return 0;}
    if(at==hud_registry+0x1c14 && length==4){memcpy(output,&hud_override,4);return 0;}
    if(at==hud_registry+0x1c18 && length==16){unsigned char entry[16]={0};putptr(entry,HUD_COMPONENT);put32(entry+8,100);memcpy(output,entry,16);return 0;}
    if(at==hud_projectile_table && length==8){memset(output,0,8);return 0;}
    if(at==hud_projectile_table+7*8 && length==8){uintptr_t root=HUD_DISPLAY;memcpy(output,&root,8);return 0;}
    if(at==HUD_DISPLAY && length==4){memcpy(output,&hud_display_type,4);return 0;}
    if(at==HUD_DISPLAY+4 && length==4){memcpy(output,&hud_display_icon,4);return 0;}
    return fake_read(at,output,length);
}
static void hud_record(uint32_t entity,uint32_t count,uint32_t pending)
{
    unsigned index=entity==103?1:0;
    memset(hud_allocation+0x394cd8,0,40);hud_word_at_set(0,10);
    memcpy(hud_allocation+0x394cdc,weapon_assets+(0x17c+index)*0x38+8,4);
    memcpy(hud_allocation+0x394ce0,weapon_assets+(0x17c+index)*0x38+0x30,8);
    hud_word_at_set(16,count);hud_word_at_set(20,pending);hud_word_at_set(24,count);hud_word_at_set(28,entity);hud_word_at_set(32,entity);
    memset(hud_allocation+0x394fdc,0,24);put32(hud_allocation+0x394fdc,10);
}
static void hud_scene(void)
{
    unsigned i;const uint32_t keys[]={103},indexes[]={0};
    solo_scene(HF_SESSION_SOLO,0,0,1);(void)hud_solo_regressions;
    memset(hud_allocation,0,0x395000);memset(weapon_assets,0,sizeof(weapon_assets));memset(icon_assets,0,sizeof(icon_assets));
    for(i=0;i<3;++i){uint32_t entity=102+i;unsigned char* row=weapon_assets+(0x17c+i)*0x38;
        const unsigned char* identity=entity==102?automatic_runtime.life.owner.ordinary.identity:
            entity==103?automatic_runtime.life.owner.utility.identity:fixture(0x70003900,24);
        memcpy(weapon_assets+(0x532-3+i)*16,identity,8);put32(weapon_assets+(0x532-3+i)*16+8,i);
        put32(row+8,201+i);row[0x30]=(unsigned char)(31+i);row[0x37]=0xf1; /* High-bit opaque name. */
    }
    assert(fake_read(BASE+0x3326e68,&hud_registry,8)==0);
    hash_rows(HUD_COMPONENT+0x20,HUD_COMPONENT+0x100,4,keys,indexes,1);
    ptr_segment(HUD_COMPONENT+0x38,HUD_COMPONENT+0x200);ptr_segment(HUD_COMPONENT+0x200,HUD_COMPONENT+0x300);
    {unsigned char* descriptor=segment(HUD_COMPONENT+0x300,24);descriptor[0]=55;put32(descriptor+8,103);
        memcpy(icon_assets,descriptor,8);put32(icon_assets+8,0);put32(icon_assets+256,7);}
    for(i=0;i<sizeof(uss_hud_pins)/sizeof(uss_hud_pins[0]);++i)
        memcpy(segment(BASE+uss_hud_pins[i].rva,uss_hud_pins[i].size),uss_hud_pins[i].bytes,uss_hud_pins[i].size);
    hud_projectile_table=BASE+0x37c7670;hud_display_icon=777;hud_display_type=7;
    hud_reads=hud_table_reads=hud_override=fail_hud_postwrite=unknown_icon_lookup=0;fake.read=hud_fixture_read;
    uss_hud_runtime_init(&hud,&automatic_formation);automatic_formation.acknowledge=uss_hud_runtime_ack;
    automatic_formation.acknowledge_user=&hud;automatic_runtime.io=uss_formation_native_io(&automatic_formation,replacement_clock_fixture);
    hud_record(102,1,0);
}
#ifndef USS_HUD_RUNTIME_TEST_MAIN
#define USS_HUD_RUNTIME_TEST_MAIN main
#endif
int USS_HUD_RUNTIME_TEST_MAIN(void)
{
    unsigned before,consumed;uint64_t writes;unsigned char prior[40],metadata[12];UssPairMember allowed[2];
    hud_allocation=VirtualAlloc(NULL,0x395000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);assert(hud_allocation);
    hud_scene();before=hud_reads;uss_hud_runtime_poll(&hud,replacement_now);assert(hud_reads==before); /* Idle has no reads. */
    solo_draw(&automatic_runtime.life.owner.utility);
    assert(hud.last_result==USS_HUD_ACKED && hud.ack.writes==2 && hud_word_at(28)==102 && hud_word_at(32)==103 && hud_word_at(20)==0);
    assert(hud_word_at(4)==202 && hud_allocation[0x394ce7]==0xf1);
    allowed[0]=automatic_runtime.life.owner.utility;allowed[1]=automatic_runtime.life.owner.ordinary;
    before=hud_table_reads;assert(uss_hud_metadata(&hud.metadata,&fake,BASE,(uintptr_t)hud_allocation,allowed,&allowed[0],metadata));
    assert(hud_table_reads==before && u32(metadata)==202); /* Cache still verifies live unit/name/icon roots. */
    /* Existing component override and absent override use distinct native paths. */
    hud_scene();hud_override=1;solo_draw(&automatic_runtime.life.owner.utility);assert(hud_word_at(4)==777);
    allowed[0]=automatic_runtime.life.owner.utility;allowed[1]=automatic_runtime.life.owner.ordinary;
    assert(uss_hud_metadata(&hud.metadata,&fake,BASE,(uintptr_t)hud_allocation,allowed,&allowed[1],metadata) && u32(metadata)==201);
    /* Both reviewed instruction targets move together. Old fixed table is
     * unreadable: resolving the new override therefore proves actual use. */
    hud_projectile_table=BASE+0x9000000;hud_display_icon=778;
    {int32_t a=(int32_t)(hud_projectile_table-(BASE+0x5f576a+7));
     int32_t b=(int32_t)(hud_projectile_table-(BASE+0xb5c43b+15));
     memcpy(fixture(BASE+0x5f576a,7)+3,&a,4);memcpy(fixture(BASE+0xb5c43b,26)+11,&b,4);}
    assert(uss_hud_metadata(&hud.metadata,&fake,BASE,(uintptr_t)hud_allocation,allowed,&allowed[0],metadata) && u32(metadata)==778);
    {Reader reader={0};reader.api=&fake;assert(uss_hud_validate_pins(&reader,BASE));}
    /* Same table, changed row contents must not reuse the old override cache. */
    hud_display_icon=779;
    assert(uss_hud_metadata(&hud.metadata,&fake,BASE,(uintptr_t)hud_allocation,allowed,&allowed[0],metadata) && u32(metadata)==779);
    hud_display_type=8;
    assert(!uss_hud_metadata(&hud.metadata,&fake,BASE,(uintptr_t)hud_allocation,allowed,&allowed[0],metadata));
    hud_display_type=7;
    fixture(BASE+0x5f576a,7)[3]^=8;
    assert(!uss_hud_metadata(&hud.metadata,&fake,BASE,(uintptr_t)hud_allocation,allowed,&allowed[0],metadata));
    fixture(BASE+0x5f576a,7)[3]^=8;
    fixture(BASE+0x5f576a,7)[0]^=1;
    {Reader reader={0};reader.api=&fake;assert(!uss_hud_validate_pins(&reader,BASE));}
    fixture(BASE+0x5f576a,7)[0]^=1;
    /* A changed cached name must resolve again; opaque high bits survive. */
    weapon_assets[(0x17c+1)*0x38+0x30]=99;
    before=hud_table_reads;assert(uss_hud_metadata(&hud.metadata,&fake,BASE,(uintptr_t)hud_allocation,allowed,&allowed[0],metadata));
    assert(hud_table_reads>before && metadata[4]==99 && metadata[11]==0xf1);
    /* Unknown component lookup is not evidence that the default icon applies. */
    hud_scene();hud_override=1;unknown_icon_lookup=1;solo_draw(&automatic_runtime.life.owner.utility);assert(!hud.ack.writes);
    /* Dedicated primary/secondary exit now emits the background completion event. */
    hud_scene();solo_draw(&automatic_runtime.life.owner.utility);hud_record(103,1,0);
    assert(uss_formation_store(&automatic_runtime,&automatic_runtime.life.owner.ordinary,2));solo_finish();
    assert(hud.last_result==USS_HUD_ACKED && hud.event.kind==USS_HUD_BACKGROUND && hud_word_at(32)==102 && hud_word_at(4)==201);
    /* Queued acquisition/resource/foreign records are untouched. */
    for(consumed=0;consumed<3;++consumed){
        hud_scene();solo_draw(&automatic_runtime.life.owner.utility);hud_record(102,1,consumed==0?2:0);
        if(consumed==1)hud_word_at_set(0,1);if(consumed==2)hud_word_at_set(32,999);
        memcpy(prior,hud_allocation+0x394cd8,40);writes=hud.ack.writes;
        uss_hud_runtime_ack(&hud,102,103,USS_PAIR_DRAW_UTILITY);
        assert(hud.ack.writes==writes && !memcmp(prior,hud_allocation+0x394cd8,40));
    }
    /* Drop and consumption both retain exact outgoing -1 display, then hand it
     * over only after native count/history and dirty state settle on survivor. */
    for(consumed=0;consumed<2;++consumed){
        hud_scene();solo_draw(&automatic_runtime.life.owner.utility);automatic_formation.invoke=recovery_own_effect;
        assert(recovery_update()==USS_CONTROL_IDLE);empty_support(consumed,1);hud_record(103,0,UINT32_MAX);
        assert(recovery_update()==USS_CONTROL_WORK);finish_recovery();
        uss_hud_runtime_restored(&hud,&recovery.work,replacement_now);
        if(hud.last_result!=USS_HUD_ACKED || !hud.handoff.active)fprintf(stderr,"restore consumed=%u last=%u disabled=%u reason=%s ack=%s phase=%u count=%u pending=%u prev=%u current=%u writes=%llu\n",consumed,hud.last_result,hud.ack.disabled,hud.reason?hud.reason:"none",hud.ack.reason?hud.ack.reason:"none",recovery.work.phase,hud_word_at(16),hud_word_at(20),hud_word_at(32),hud_word_at(28),(unsigned long long)hud.ack.writes);
        assert(hud.last_result==USS_HUD_ACKED && hud.handoff.active && hud_word_at(4)==202 && hud_word_at(16)==1 &&
            hud_word_at(24)==0 && hud_word_at(28)==103 && hud_word_at(32)==102 && hud_word_at(20)==UINT32_MAX);
        writes=hud.ack.writes;replacement_now+=50;uss_hud_runtime_poll(&hud,replacement_now);assert(hud.ack.writes==writes);
        hud_word_at_set(24,1);hud_word_at_set(28,102);hud_allocation[0x394ff0]=1;
        replacement_now+=50;uss_hud_runtime_poll(&hud,replacement_now);assert(hud.ack.writes==writes && hud.handoff.active);
        hud_allocation[0x394ff0]=0;replacement_now+=50;uss_hud_runtime_poll(&hud,replacement_now);
        assert(!hud.handoff.active && hud_word_at(4)==201 && hud_word_at(20)==UINT32_MAX && hud.ack.writes==writes+1);
        uss_hud_runtime_poll(&hud,replacement_now+8001); /* Event-owned drop observer also expires. */
        before=hud_reads;uss_hud_runtime_poll(&hud,replacement_now+8051);assert(hud_reads==before);
    }
    /* Acknowledge in the attachment callback, while lone publication is still
     * pending. Same guarded record handling, without waiting three more ticks. */
    hud_scene();solo_draw(&automatic_runtime.life.owner.utility);automatic_formation.invoke=recovery_own_effect;
    assert(recovery_update()==USS_CONTROL_IDLE);empty_support(0,1);hud_record(103,0,UINT32_MAX);
    assert(recovery_update()==USS_CONTROL_WORK && recovery.work.phase==USS_RECOVERY_ATTACH);
    replacement_now+=25;assert(recovery_update()==USS_CONTROL_WORK && recovery.work.phase==USS_RECOVERY_PRIMARY);
    assert(!automatic_runtime.life.owner.pair_present);
    uss_hud_runtime_restored(&hud,&recovery.work,replacement_now);
    assert(hud.last_result==USS_HUD_ACKED && hud.handoff.active && hud_word_at(32)==102);
    writes=hud.ack.writes;finish_recovery();assert(hud.ack.writes==writes);
    /* A new positive message cancels handoff without relabeling it. */
    hud_scene();solo_draw(&automatic_runtime.life.owner.utility);automatic_formation.invoke=recovery_own_effect;
    assert(recovery_update()==USS_CONTROL_IDLE);empty_support(0,1);hud_record(103,0,UINT32_MAX);
    (void)recovery_update();finish_recovery();uss_hud_runtime_restored(&hud,&recovery.work,replacement_now);
    hud_word_at_set(20,1);memcpy(prior,hud_allocation+0x394cd8,40);replacement_now+=50;uss_hud_runtime_poll(&hud,replacement_now);
    assert(!hud.handoff.active && !memcmp(prior,hud_allocation+0x394cd8,40));
    /* Handoff expiry and scope change retire read-only work without writes. */
    for(consumed=0;consumed<2;++consumed){
        hud_scene();solo_draw(&automatic_runtime.life.owner.utility);automatic_formation.invoke=recovery_own_effect;
        assert(recovery_update()==USS_CONTROL_IDLE);empty_support(0,1);hud_record(103,0,UINT32_MAX);
        (void)recovery_update();finish_recovery();uss_hud_runtime_restored(&hud,&recovery.work,replacement_now);
        writes=hud.ack.writes;
        if(consumed)++automatic_runtime.life.owner.guard_generation;
        uss_hud_runtime_poll(&hud,replacement_now+(consumed?50:6501));assert(!hud.handoff.active && hud.ack.writes==writes);
    }
    /* Replacement completion takes a fresh Core view, never its stale formation
     * mirror. This same callback is wired for both automatic replacement routes. */
    hud_scene();solo_incoming(1,1,0,0);hud_record(103,1,0);
    /* Silo103 was replaced by104, so history must name the actual new utility. */
    hud_word_at_set(28,104);hud_word_at_set(32,104);
    automatic_runtime.latest.inventory.slots[2]=999; /* Deliberately stale mirror only. */
    uss_hud_runtime_completed_pair(&hud);
    assert(hud.last_result==USS_HUD_ACKED && hud.event.target==102 && hud_word_at(32)==102);
    /* Cosmetic uncertainty does not pause or retry gameplay. */
    hud_scene();fail_hud_postwrite=1;solo_draw(&automatic_runtime.life.owner.utility);
    assert(hud.ack.disabled && automatic_runtime.life.phase==USS_LIFE_FORMED && hud_word_at(4)==202);
    writes=hud.ack.writes;fail_hud_postwrite=0;solo_draw(&automatic_runtime.life.owner.ordinary);assert(hud.ack.writes==writes);
    uss_hud_runtime_retire(&hud);assert(hud.ack.disabled && !hud.handoff.active);
    /* Entry mismatches disable only this HUD path. */
    hud_scene();fixture(BASE+uss_hud_pins[0].rva,uss_hud_pins[0].size)[0]^=1;solo_draw(&automatic_runtime.life.owner.utility);
    assert(!hud.pins_valid && !hud.ack.writes && automatic_runtime.life.phase==USS_LIFE_FORMED);
    assert(!strcmp(hud.pin_diagnostic.stage,"HUD_PIN_MISMATCH") && hud.pin_diagnostic.index==0 &&
        hud.pin_diagnostic.rva==uss_hud_pins[0].rva && hud.pin_diagnostic.offset==0 &&
        hud.pin_diagnostic.expected==uss_hud_pins[0].bytes[0] && hud.pin_diagnostic.actual==(uss_hud_pins[0].bytes[0]^1u));
    assert(VirtualFree(hud_allocation,0,MEM_RELEASE));
    puts("USS connected HUD: actual Core/metadata/private writes; native icon paths, opaque names, draw/background, removal/consumption handoff, queue preservation, idle/no-retry isolation PASS");return 0;
}
