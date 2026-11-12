#include "../src/uss_native_menu_lifecycle.h"
#include "../src/uss_native_menu_sdk.h"
#define USS_MENU_OWNERSHIP_TEST_MAIN ownership_tests
#include "uss_native_menu_ownership_harness.c"
static UssMenuLifecycle lifecycle;
static unsigned effects,fail_effect,cleanup_calls,uncertain_cleanup,description_hides;
static UssSettingsState fixture_settings;
static unsigned takeover_during_cleanup;
static unsigned char* mapped(uintptr_t at,size_t length)
{
    assert(at>=0x22000000 && at-0x22000000<=sizeof(screen) && length<=sizeof(screen)-(at-0x22000000));
    return screen+(at-0x22000000);
}
static void entered(void){++effects;if(effects==fail_effect)change_scope=1;}
static void native_tabs(uint64_t widget,const uint32_t* labels,int count)
{
    assert(widget==0x22000000+1248 && count==4);
    memcpy(mapped(widget+57320,16),labels,16);uss_menu_put_word(mapped(widget,60000),57448,4);
    uss_menu_put_word(screen,1248+8296+3400*3+272,0xc67c7faf);entered();
}
static void native_label(uint64_t widget,uint32_t value)
{uss_menu_put_word(mapped(widget,620),272,value);entered();}
static void native_text(uint64_t widget,uint32_t key,const char* value)
{
    unsigned char* w=mapped(widget,620);uintptr_t pointer=(uintptr_t)value;
    assert(key==0xab2a7b35);uss_menu_put_word(w,280,key);uss_menu_put_word(w,284,1);
    memcpy(w+288,&pointer,8);w[616]=1;entered();
}
static uint8_t native_clear(uint64_t at){mapped(at-272,620)[616]=0;entered();return 1;}
static void native_visible(uint64_t at,uint8_t value){assert(mapped(at,1) && value<=1);entered();}
static void native_select(uint64_t content_address,int category)
{
    assert(content_address==0x22000000+3010456 && category>=0 && category<8);
    if(own_rows.assignment.attached)memset(own_rows.assignment.widget,0,sizeof(own_rows.assignment.widget));
    uss_menu_put_word(mapped(content_address,1320000),1318488,(uint32_t)category);entered();
}
static void native_hidden(uint64_t at,uint8_t hidden,uint8_t immediate)
{assert(at==0x22000000+3010456 && !hidden && immediate);entered();}
static void native_hide_description(uint64_t at,uint8_t hide,uint8_t animate)
{assert(at==0x22000000+3010456+1315800 && hide==1 && animate==0);++description_hides;if(takeover_during_cleanup)fixture_settings.external=1;entered();}
static void native_visual(uint64_t at,float dt){assert(at==0x22000000+3010456 && dt>=0);entered();}
static void native_reset(uint64_t at,const float* a,const float* b)
{assert(mapped(at,31464) && !((uintptr_t)a&15) && a==b);entered();}
static void native_release(uint64_t at){assert(mapped(at,31464));entered();}
static void native_init(uint64_t at,UssMenuVec2 position,UssMenuVec2 a,UssMenuVec2 b,int kind,uint64_t native_settings,const void* descriptor,uint8_t enabled)
{
    const unsigned char* d=(const unsigned char*)descriptor;unsigned char* row=mapped(at,31464);unsigned i,count=uss_menu_word(d,64);
    assert(position.y<=0 && a.x==b.x && kind==10 && !native_settings && enabled);
    memset(row,0,31464);uss_menu_put_word(row,0x7ac0,uss_menu_word(d,8));uss_menu_put_word(row,31428,139);uss_menu_put_word(row,29072,count);
    for(i=0;i<count;++i)uss_menu_put_word(row,29080+4*i,uss_menu_word(d,68+4*i));entered();
}
static void native_child(uint64_t parent,uint64_t child)
{
    if(parent==0x22000000+3010456+1150848+544){assert(mapped(child,31464));}
    else{assert(parent==0x22000000+3010456+144000+USS_OPTION_ROW_BYTES && child==(uintptr_t)own_rows.assignment.widget);
        uintptr_t previous;memcpy(&previous,mapped(parent+0xe0,8),8);
        memcpy(own_rows.assignment.widget+0xe8,&previous,8);
        memcpy(own_rows.assignment.widget+0xf0,&parent,8);memcpy(mapped(parent+0xe0,8),&child,8);}
    entered();
}
static void native_display_init(uint64_t at,UssMenuVec2 a,UssMenuVec2 b,UssMenuVec2 c,int layer)
{
    uint64_t identity=UINT64_MAX;(void)a;(void)b;(void)c;assert(layer==12 && at==(uintptr_t)own_rows.assignment.widget);
    retained_display=(unsigned char*)(uintptr_t)at;retained_display_size=USS_ASSIGNMENT_DISPLAY_BYTES;
    memcpy(retained_display+USS_ASSIGNMENT_IDENTITY,&identity,8);entered();
}
static uint8_t native_direct_text(uint64_t at,const char* text)
{assert(at==(uintptr_t)retained_display+USS_ASSIGNMENT_TEXT);memcpy(retained_display+USS_ASSIGNMENT_TEXT_BUFFER,text,strlen(text)+1);entered();return 0;}
static void native_display_layout(uint64_t at){assert(at==(uintptr_t)retained_display);entered();}
static void native_display_opacity(uint64_t at,float value)
{assert(value==1.0f && (at==(uintptr_t)retained_display+USS_ASSIGNMENT_TEXT || at==(uintptr_t)retained_display+0x268));entered();}
static void native_choice(uint64_t at,uint32_t choice)
{uss_menu_put_word(mapped(at-16016,31464),29068,choice);entered();}
static void native_finish(uint64_t at){assert(at==0x22000000+3010456+1150848);entered();}
static int guarded(void* user,const UssMenuHostSnapshot* expected,unsigned built)
{(void)user;(void)built;return uss_menu_ownership_guard(&owner,expected);}
static int write_field(void* user,uintptr_t at,const void* data,size_t length)
{
    (void)user;assert(uss_menu_sdk_field(&owner.identity,at,length));
    memcpy(mapped(at,length),data,length);entered();return 1;
}
static unsigned cleanup(void* user)
{(void)user;++cleanup_calls;return uncertain_cleanup?USS_MENU_EFFECT_UNCERTAIN:USS_MENU_EFFECT_DONE;}
static void setup_lifecycle(void)
{
    unsigned char inert[216]={0};uintptr_t rows=0x22000000+3010456+144000;
    UssMenuSelectorSpec specs[2]={{"Binding mode",2,0,{"Default","Dedicated"}},{"Input",2,1,{"Keyboard","Controller"}}};
    start();memset(&lifecycle,0,sizeof(lifecycle));effects=fail_effect=cleanup_calls=uncertain_cleanup=description_hides=takeover_during_cleanup=0;
    uss_settings_init(&fixture_settings,NULL);
    uss_menu_put_word(inert,0,139);assert(uss_menu_rows_prepare(&own_rows,inert,216,specs,2));
    put_pointer(screen+3010456+1150848+2816,rows);
    lifecycle.initialized=1;lifecycle.owner=&owner;lifecycle.pivot.x=lifecycle.pivot.y=0.5f;lifecycle.cleanup=cleanup;
    lifecycle.io.guard=guarded;lifecycle.io.read=read_memory;lifecycle.io.write=write_field;
    lifecycle.io.native.set_tab_labels=native_tabs;lifecycle.io.native.set_label=native_label;
    lifecycle.io.native.set_string_arg=native_text;lifecycle.io.native.clear_args=native_clear;
    lifecycle.io.native.set_visible=native_visible;lifecycle.io.native.select_category=native_select;
    lifecycle.io.native.set_content_hidden=native_hidden;lifecycle.io.native.options_visual=native_visual;
    lifecycle.io.native.row_reset=native_reset;lifecycle.io.native.row_release=native_release;
    lifecycle.io.native.row_init=native_init;lifecycle.io.native.add_child=native_child;
    lifecycle.io.native.assignment_display_init=native_display_init;lifecycle.io.native.assignment_text=native_direct_text;
    lifecycle.io.native.assignment_layout=native_display_layout;lifecycle.io.native.set_opacity=native_display_opacity;
    lifecycle.io.native.hide_description=native_hide_description;
    lifecycle.io.native.set_choice=native_choice;lifecycle.io.native.finish_simple=native_finish;
}
static int full_cycle(void)
{
    return uss_menu_lifecycle_open(&lifecycle) && uss_menu_lifecycle_before_update(&lifecycle) &&
        uss_menu_lifecycle_after_update(&lifecycle,0.016f) && uss_menu_lifecycle_close(&lifecycle);
}
#ifndef USS_MENU_LIFECYCLE_TEST_MAIN
#define USS_MENU_LIFECYCLE_TEST_MAIN main
#endif
int USS_MENU_LIFECYCLE_TEST_MAIN(void)
{
    unsigned total,before,i,at[7],values[3]={99,99,99};const char* retained;
    assert(ownership_tests()==0);setup_lifecycle();assert(full_cycle());total=effects;
    assert(cleanup_calls==1 && lifecycle.host.phase==USS_MENU_CLOSED && owner.stage==USS_MENU_PROOF_VANILLA);
    retained=own_rows.rows[0].title;assert(uss_menu_lifecycle_open(&lifecycle));assert(own_rows.rows[0].title==retained);
    assert(uss_menu_lifecycle_close(&lifecycle));assert(cleanup_calls==2);
    setup_lifecycle();assert(uss_menu_lifecycle_open(&lifecycle));
    uss_menu_put_word(ui,0x429c+4,2);uss_menu_put_word(ui,0x429c+20,2);before=effects;
    assert(!uss_menu_lifecycle_before_update(&lifecycle) && !owner.stopped && effects==before);
    uss_menu_put_word(ui,0x429c+20,1);assert(uss_menu_lifecycle_before_update(&lifecycle));
    assert(!uss_menu_lifecycle_before_update(&lifecycle));assert(uss_menu_lifecycle_after_update(&lifecycle,0));
    assert(!uss_menu_lifecycle_after_update(&lifecycle,0));assert(uss_menu_lifecycle_close(&lifecycle));
    at[0]=1;at[1]=2;at[2]=20;at[3]=64;at[4]=80;at[5]=total-1;at[6]=total;
    for(i=0;i<7;++i){setup_lifecycle();fail_effect=at[i];assert(!full_cycle());before=effects;
        assert(!uss_menu_lifecycle_open(&lifecycle) && effects==before);}
    setup_lifecycle();lifecycle.cleanup=NULL;assert(!uss_menu_lifecycle_open(&lifecycle) && !effects);
    setup_lifecycle();assert(uss_menu_lifecycle_open(&lifecycle));uncertain_cleanup=1;
    assert(!uss_menu_lifecycle_close(&lifecycle) && lifecycle.host.uncertain && lifecycle.host.phase==USS_MENU_STOPPED && owner.stopped);
    setup_lifecycle();assert(uss_menu_lifecycle_open(&lifecycle));
    text_identity(3010456+144000+3992,0xdead0000);before=effects;
    assert(!uss_menu_lifecycle_before_update(&lifecycle) && effects==before && owner.stopped);
    setup_lifecycle();assert(uss_menu_lifecycle_bind_native_cleanup(&lifecycle,&fixture_settings));
    assert(lifecycle.pivot.x==0.5f && lifecycle.pivot.y==1 && !effects);
    fixture_settings.pending=fixture_settings.current;fixture_settings.pending.mode=USS_BIND_DEDICATED;fixture_settings.pending_present=1;
    screen[3010456+1319449]=1;screen[3010456+1315800+936+616]=1;screen[3010456+1315800+1632+616]=1;
    assert(full_cycle());assert(!fixture_settings.pending_present && fixture_settings.current.mode==USS_BIND_DEFAULT && fixture_settings.revision==1);
    assert(description_hides==1 && !screen[3010456+1319449] && !screen[3010456+1315800+936+616] && !screen[3010456+1315800+1632+616]);
    assert(uss_menu_word(screen,3010456+1318500)==156 && uss_menu_word(screen,3010456+1318476)==156);
    for(i=1;i<=6;++i){setup_lifecycle();assert(uss_menu_lifecycle_bind_native_cleanup(&lifecycle,&fixture_settings));
        assert(uss_menu_lifecycle_open(&lifecycle));fail_effect=effects+i;
        assert(!uss_menu_lifecycle_close(&lifecycle) && owner.stopped);before=effects;
        assert(!uss_menu_lifecycle_close(&lifecycle) && effects==before);}
    setup_lifecycle();assert(uss_menu_lifecycle_bind_native_cleanup(&lifecycle,&fixture_settings));assert(uss_menu_lifecycle_open(&lifecycle));
    takeover_during_cleanup=1;before=effects;
    assert(!uss_menu_lifecycle_close(&lifecycle) && fixture_settings.external && owner.stopped && effects==before+2);
    setup_lifecycle();fixture_settings.external=1;assert(!uss_menu_lifecycle_bind_native_cleanup(&lifecycle,&fixture_settings));
    setup_lifecycle();assert(uss_menu_lifecycle_bind_native_cleanup(&lifecycle,&fixture_settings));assert(uss_menu_lifecycle_open(&lifecycle));
    fixture_settings.external=1;fixture_settings.pending_present=1;before=effects;
    assert(!uss_menu_lifecycle_close(&lifecycle) && fixture_settings.pending_present && effects==before);
    setup_lifecycle();assert(uss_menu_lifecycle_bind_native_cleanup(&lifecycle,&fixture_settings));
    assert(uss_menu_lifecycle_step(&lifecycle,0.016f,values,3)==USS_MENU_STEP_TAB_ADDED);
    before=effects;assert(uss_menu_lifecycle_step(&lifecycle,0.016f,values,3)==USS_MENU_STEP_IDLE && effects==before);
    uss_menu_put_word(screen,1248+57452,3);
    assert(uss_menu_lifecycle_step(&lifecycle,0.016f,values,3)==USS_MENU_STEP_OPENED && uss_menu_word(screen,8)==2);
    uss_menu_put_word(screen,3010456+144000+29068,1);
    assert(uss_menu_lifecycle_step(&lifecycle,0.016f,values,3)==USS_MENU_STEP_UPDATED && values[0]==1 && uss_menu_word(screen,8)==2);
    uss_menu_put_word(ui,0x429c+4,2);uss_menu_put_word(ui,0x429c+20,2);before=effects;
    assert(uss_menu_lifecycle_step(&lifecycle,0.016f,values,3)==USS_MENU_STEP_SUSPENDED && effects==before && !owner.stopped);
    uss_menu_put_word(ui,0x429c+20,1);uss_menu_put_word(screen,1248+57452,2);
    assert(uss_menu_lifecycle_step(&lifecycle,0.016f,values,3)==USS_MENU_STEP_CLOSED);
    uss_menu_put_word(screen,1248+57452,3);
    assert(uss_menu_lifecycle_step(&lifecycle,0.016f,values,3)==USS_MENU_STEP_OPENED);
    uss_menu_put_word(ui,0x429c+20,0);fixture_settings.pending_present=1;before=effects;
    assert(uss_menu_lifecycle_step(&lifecycle,0.016f,values,3)==USS_MENU_STEP_ENDED && !fixture_settings.pending_present && effects==before);
    assert(uss_menu_lifecycle_step(&lifecycle,0.016f,values,3)==USS_MENU_STEP_REFUSED && effects==before);
    puts("USS integrated native menu lifecycle: actual ownership graph plus typed tab/row effects, update pairing, cleanup, close/reopen and fault boundaries PASS");return 0;
}
