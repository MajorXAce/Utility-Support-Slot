/* Full native HUD reader/sequence using only OWN functions and allocations. */
#include "../src/uss_hud_presentation.h"
#define USS_HUD_RUNTIME_TEST_MAIN presentation_hud_regressions
#include "uss_hud_runtime_harness.c"
#define PRESENT_IMAGE ((uintptr_t)0xe7000000)
#define PRESENT_ENGINE ((uintptr_t)0xd6000000)
#define PRESENT_CONTEXT ((uintptr_t)0xd6100000)
#define PRESENT_DISPATCH ((uintptr_t)0xd6200000)
static unsigned char present_image[8192];
static unsigned present_calls,present_kinds[16],present_fail,present_no_effect,present_change,present_bad_page,present_invalid_dimensions;
static unsigned sized_width,sized_height;static uint64_t expected_name;
static uintptr_t present_widget(void){return (uintptr_t)hud_allocation+0x392828;}
static int presentation_fixture_read(uintptr_t at,void* output,size_t length)
{
    if(at>=PRESENT_IMAGE && at-PRESENT_IMAGE<=sizeof(present_image) && length<=sizeof(present_image)-(at-PRESENT_IMAGE)){
        memcpy(output,present_image+(at-PRESENT_IMAGE),length);return 0;}
    return hud_fixture_read(at,output,length);
}
static int presentation_fixture_query(void* user,uintptr_t at,UssCapturePage* output)
{
    if(at>=PRESENT_IMAGE && at<PRESENT_IMAGE+sizeof(present_image)){
        memset(output,0,sizeof(*output));output->base=output->allocation=PRESENT_IMAGE;output->size=sizeof(present_image);
        output->kind=present_bad_page?0x20000:0x1000000;output->state=0x1000;output->protection=32;return 1;}
    if(at>=(uintptr_t)hud_allocation && at<(uintptr_t)hud_allocation+0x396000)return capture_windows_query(user,at,output);
    return page_snapshot(user,at,output);
}
static void present_own_measure(uint64_t name,uint32_t* width,uint32_t* height,void* dispatch)
{
    assert(name==expected_name && (uintptr_t)dispatch==PRESENT_DISPATCH);*width=present_invalid_dimensions?0:160;*height=64;
    if(present_change)put32(hud_allocation+0x392828+0x27cc,2);
}
static void present_own_icon(void* node,uint32_t icon)
{assert((uintptr_t)node==present_widget()+0x1cc0);if(!present_no_effect)memcpy((unsigned char*)node+0x110,&icon,4);}
static void present_own_size(void* node,uint64_t packed)
{
    float dimensions[2];assert((uintptr_t)node==present_widget()+0x13a8);memcpy(dimensions,&packed,8);
    assert(dimensions[0]==80.0f && dimensions[1]==32.0f);sized_width=(unsigned)dimensions[0];sized_height=(unsigned)dimensions[1];
}
static void present_own_name(void* node,uint64_t format,uint64_t name,uint8_t zero)
{
    assert((uintptr_t)node==present_widget()+0x13a8 && format==UINT64_C(0xd08fc1346df3d6fc) && name==expected_name && !zero);
    if(!present_no_effect)memcpy((unsigned char*)node+0x150,&name,8);
}
static void present_own_layout(void* widget){assert((uintptr_t)widget==present_widget());}
static int presentation_fixture_invoke(void* user,UssHudPresentationEffect effect,uintptr_t entry,uintptr_t node,
    uint64_t value,uint32_t* width,uint32_t* height)
{
    uintptr_t own=0;unsigned ordinal;(void)user;assert(present_calls<16);ordinal=++present_calls;present_kinds[ordinal-1]=(unsigned)effect;
    if(ordinal==present_fail)return 0;
    if(effect==USS_HUD_MEASURE){assert(entry==PRESENT_IMAGE+0x1000);own=(uintptr_t)present_own_measure;}
    else if(effect==USS_HUD_SET_ICON){assert(entry==BASE+0x143bf90);own=(uintptr_t)present_own_icon;}
    else if(effect==USS_HUD_SET_SIZE){assert(entry==BASE+0x1447160);own=(uintptr_t)present_own_size;}
    else if(effect==USS_HUD_SET_NAME){assert(entry==BASE+0x1450230);own=(uintptr_t)present_own_name;}
    else {assert(effect==USS_HUD_LAYOUT && entry==BASE+0x198c430);own=(uintptr_t)present_own_layout;}
    return uss_hud_presentation_invoke(NULL,effect,own,node,value,width,height);
}
static void presentation_widget(unsigned utility)
{
    unsigned char* text=(unsigned char*)present_widget()+0x13a8;unsigned char* icon=(unsigned char*)present_widget()+0x1cc0;
    unsigned char* resource=weapon_assets+(0x17c+utility)*0x38;
    put32(text,3u<<18);putptr(text+0xf8,PRESENT_CONTEXT);memcpy(text+0x150,resource+0x30,8);
    put32(icon,7u<<18);memcpy(icon+0x110,resource+8,4);put32((unsigned char*)present_widget()+0x27cc,1);
    put32(hud_allocation+0x394b44,0x3f800000);put32(hud_allocation+0x394fe0,1);
    put32(hud_allocation+0x394fe8,utility?103:102);put32(hud_allocation+0x394fec,1);
    hud_allocation[0x394ff0]=0;
}
static void presentation_scene(void)
{
    unsigned i;hud_scene();(void)presentation_hud_regressions;
    memset(present_image,0,sizeof(present_image));memcpy(present_image,"MZ",2);put32(present_image+0x3c,0x80);
    memcpy(present_image+0x80,"PE\0\0",4);present_image[0x84]=0x64;present_image[0x85]=0x86;present_image[0x86]=1;
    present_image[0x94]=0xe0;present_image[0x98]=0x0b;present_image[0x99]=2;put32(present_image+0x98+56,sizeof(present_image));
    put32(present_image+0x178+8,0x1000);put32(present_image+0x178+12,0x1000);put32(present_image+0x178+36,0x20000000);present_image[0x1000]=0xc3;
    putptr(fixture(BASE+USS_HUD_ENGINE_RVA,8),PRESENT_ENGINE);
    ptr_segment(PRESENT_ENGINE+0x28,PRESENT_ENGINE+0x1000);ptr_segment(PRESENT_ENGINE+0xd0,PRESENT_DISPATCH);
    ptr_segment(PRESENT_DISPATCH+0x338,PRESENT_IMAGE+0x1000);ptr_segment(PRESENT_CONTEXT+8,PRESENT_CONTEXT+0x1000);
    for(i=0;i<5;++i)memcpy(segment(BASE+uss_hud_presentation_pins[i].rva,uss_hud_presentation_pins[i].size),
        uss_hud_presentation_pins[i].bytes,uss_hud_presentation_pins[i].size);
    fake.read=presentation_fixture_read;automatic_formation.platform.query=presentation_fixture_query;
    hud.presentation.invoke=presentation_fixture_invoke;present_calls=present_fail=present_no_effect=present_change=present_bad_page=present_invalid_dimensions=0;
    sized_width=sized_height=0;memset(present_kinds,0,sizeof(present_kinds));
    hud_record(102,1,1);presentation_widget(0);memcpy(&expected_name,weapon_assets+(0x17c+1)*0x38+0x30,8);
    uss_hud_runtime_completed_pair(&hud);assert(hud.popup.active); /* Confirmed initial utility acquisition, final inventory. */
}
#ifndef USS_HUD_PRESENTATION_TEST_MAIN
#define USS_HUD_PRESENTATION_TEST_MAIN main
#endif
int USS_HUD_PRESENTATION_TEST_MAIN(void)
{
    unsigned i,mode,before;unsigned char prior[40];UssHudPopupSnapshot snapshot;UssHudPopupRecheck check;
    hud_allocation=VirtualAlloc(NULL,0x396000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);assert(hud_allocation);
    presentation_scene();memcpy(prior,hud_allocation+0x394cd8,40);replacement_now+=25;uss_hud_runtime_poll(&hud,replacement_now);
    assert(!hud.popup.active && present_calls==5 && sized_width==80 && sized_height==32 && !hud.presentation.disabled);
    for(i=0;i<5;++i)assert(present_kinds[i]==i+1);
    assert(u32((void*)(present_widget()+0x1cc0+0x110))==202 && !memcmp((void*)(present_widget()+0x13a8+0x150),&expected_name,8));
    assert(!memcmp(prior,hud_allocation+0x394cd8,40)); /* Genuine +1/history/cache remain intact. */
    before=hud_reads;uss_hud_runtime_poll(&hud,replacement_now+25);assert(hud_reads==before && present_calls==5);
    /* A completed pickup owns stable slots/identities, not a frozen held
     * weapon while its animation settles. Lua's popup context allows this. */
    presentation_scene();
    put32(fixture(0x70000500,4),102); /* Primary selected, previous support still held. */
    {UssCoreObservation anchor=formation_native_observe(&automatic_formation,&automatic_runtime.life.owner,&automatic_runtime.policy);
     assert(anchor.valid && anchor.inventory.held==102);
     hud_popup_arm(&hud,&anchor,102,103,0,replacement_now);assert(hud.popup.active);}
    put32(fixture(0x70000500,4),101); /* Primary finishes drawing before HUD settles. */
    replacement_now+=25;uss_hud_runtime_poll(&hud,replacement_now);
    assert(present_calls==5 && !hud.popup.active && hud_word_at(20)==1);
    /* The exception admits only settling onto the already selected slot.
     * New selection, identity, slot, metadata or owner changes still refuse. */
    {Inventory anchor=hud.popup.inventory,changed;
     for(mode=0;mode<8;++mode){
      changed=anchor;
      if(mode==0)changed.active_selector=2;
      if(mode==1)changed.slots[2]=999;
      if(mode==2)changed.identities[2][0]^=1;
      if(mode==3)changed.row_address+=48;
      if(mode==4)changed.row[40]^=1;
      if(mode==5)changed.avatar+=1;
      if(mode==6)changed.held=999;
      if(mode==7)changed.identities[4][0]^=1;
      assert(!hud_popup_inventory_same(&anchor,&changed));
     }}
    /* An actual backwards clock still refuses; only the caller's stale sample
     * is corrected. No new read or native effect on this refusal path. */
    presentation_scene();before=hud_reads;uss_hud_runtime_poll(&hud,replacement_now-1);
    assert(!hud.popup.active && !present_calls && hud_reads==before && !strcmp(hud.reason,"PICKUP_CLOCK_BACKWARDS"));
    /* Dirty rendering can settle before the bounded event expires. */
    presentation_scene();hud_allocation[0x394ff0]=1;replacement_now+=25;uss_hud_runtime_poll(&hud,replacement_now);assert(!present_calls && hud.popup.active);
    hud_allocation[0x394ff0]=0;replacement_now+=25;uss_hud_runtime_poll(&hud,replacement_now);assert(present_calls==5);
    /* Code/owning image/section/node-kind guards all refuse before any entry. */
    for(mode=0;mode<5;++mode){
        presentation_scene();if(mode==0)fixture(BASE+0x1450230,25)[0]^=1;
        if(mode==1)present_bad_page=1;if(mode==2)present_image[0x85]=0;
        if(mode==3)put32(present_image+0x178+36,0);if(mode==4)put32((void*)(present_widget()+0x13a8),2u<<18);
        replacement_now+=25;uss_hud_runtime_poll(&hud,replacement_now);assert(!present_calls && !hud.popup.active);
    }
    /* Measurement is consumed even if it changes context or returns bad sizes. */
    for(mode=0;mode<2;++mode){
        presentation_scene();if(mode)present_invalid_dimensions=1;else present_change=1;
        replacement_now+=25;uss_hud_runtime_poll(&hud,replacement_now);assert(present_calls==1 && !hud.popup.active);
        assert(hud_popup_capture(&hud,&snapshot));check.runtime=&hud;check.expected=&snapshot;
        assert(uss_hud_present(&hud.presentation,snapshot.serial,(uintptr_t)snapshot.hud.root,snapshot.expected,snapshot.desired,hud_popup_unchanged,&check)==USS_HUD_PRESENT_SKIP);
        assert(present_calls==1);
    }
    for(i=1;i<=5;++i){
        presentation_scene();present_fail=i;replacement_now+=25;uss_hud_runtime_poll(&hud,replacement_now);
        assert(present_calls==i && hud.presentation.disabled && !hud.ack.disabled && !automatic_controller.stopped);
        replacement_now+=25;uss_hud_runtime_poll(&hud,replacement_now);assert(present_calls==i);
    }
    presentation_scene();present_no_effect=1;replacement_now+=25;uss_hud_runtime_poll(&hud,replacement_now);
    assert(present_calls==5 && hud.presentation.disabled); /* Returned setter with wrong readback never retries. */
    /* New/foreign quantities and expired ownership cannot correct another popup. */
    for(mode=0;mode<4;++mode){
        presentation_scene();if(mode==0)hud_word_at_set(20,2);if(mode==1)++automatic_runtime.life.owner.guard_generation;
        if(mode==2)hud_word_at_set(28,999);
        replacement_now+=mode==3?1501:25;uss_hud_runtime_poll(&hud,replacement_now);assert(!present_calls);
    }
    /* Ordinary acquisition with pickup No: utility was restored, so the wrong
     * visible utility+1 is corrected to the actual new ordinary member. */
    presentation_scene();hud.popup.active=0;solo_incoming(0,3,0,0);
    hud_record(103,1,0);presentation_widget(1);memcpy(&expected_name,weapon_assets+0x17c*0x38+0x30,8);
    uss_hud_runtime_completed_pair(&hud);assert(hud.popup.active && hud.popup.wait_pending && hud.popup.presented==104);
    replacement_now+=25;uss_hud_runtime_poll(&hud,replacement_now);assert(hud.popup.active && !present_calls);
    hud_word_at_set(20,1);replacement_now+=25;uss_hud_runtime_poll(&hud,replacement_now);
    assert(present_calls==5 && u32((void*)(present_widget()+0x1cc0+0x110))==201 && hud_word_at(20)==1);
    /* Reported ordinary replacement while primary/secondary remains selected:
     * Silo was an internal restoration, not the real acquisition. */
    for(unsigned selected=1;selected<=2;++selected)for(unsigned active=0;active<2;++active){
        presentation_scene();hud.popup.active=0;solo_incoming(0,selected,0,active);
        hud_record(103,1,1);presentation_widget(1);
        memcpy(&expected_name,weapon_assets+0x17c*0x38+0x30,8);
        uss_hud_runtime_completed_pair(&hud);
        assert(hud.popup.active && hud.popup.presented==104 && hud.popup.displayed==103);
        memcpy(prior,hud_allocation+0x394cd8,40);
        replacement_now+=25;uss_hud_runtime_poll(&hud,replacement_now);
        assert(present_calls==5 && !hud.popup.active && hud_word_at(20)==1 &&
            u32((void*)(present_widget()+0x1cc0+0x110))==201);
        assert(!memcmp(prior,hud_allocation+0x394cd8,40));
    }
    /* A genuine ordinary pickup name or a different queued quantity is not
     * relabeled; an old weapon still parented is not a verified replacement. */
    for(mode=0;mode<3;++mode){
        presentation_scene();hud.popup.active=0;solo_incoming(0,1,0,1);
        hud_record(mode==0?102:103,1,mode==1?2:1);presentation_widget(mode==0?0:1);
        if(mode==0){hud_word_at_set(28,104);hud_word_at_set(32,104);put32(hud_allocation+0x394fe8,104);}
        if(mode==2)put32(fixture(0x70000700,240)+48+4,22);
        memcpy(prior,hud_allocation+0x394cd8,40);
        uss_hud_runtime_completed_pair(&hud);replacement_now+=25;uss_hud_runtime_poll(&hud,replacement_now);
        assert(!present_calls && !memcmp(prior,hud_allocation+0x394cd8,40));
    }
    /* Actually acquiring Silo keeps its genuine utility notification. */
    presentation_scene();hud_record(103,1,1);presentation_widget(1);
    memcpy(prior,hud_allocation+0x394cd8,40);replacement_now+=25;uss_hud_runtime_poll(&hud,replacement_now);
    assert(!present_calls && !memcmp(prior,hud_allocation+0x394cd8,40));
    uss_hud_runtime_retire(&hud);assert(!hud.popup.active && !hud.handoff.active);
    assert(VirtualFree(hud_allocation,0,MEM_RELEASE));
    puts("USS HUD native presentation: OWN Win64 measure/icon/packed-size/name/layout; actual widget readback, external PE owner, confirmed pickup windows, unchanged quantities/history, guards and no-retry PASS");return 0;
}
