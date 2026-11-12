#include <assert.h>
#include <stdio.h>
#include "../src/uss_native_menu_host.h"
static UssMenuHostSnapshot snapshot;
static unsigned calls,actions[16],fail_at,uncertain_at,replace_at;
static UssMenuHostSnapshot observe(void* user){(void)user;return snapshot;}
static unsigned effect(void* user,unsigned action,const UssMenuHostSnapshot* expected,uint32_t value)
{
    (void)user;assert(uss_menu_same_host(expected,&snapshot));actions[calls++]=action;
    if(calls==fail_at)return USS_MENU_EFFECT_NOT_ENTERED;
    if(calls==uncertain_at)return USS_MENU_EFFECT_UNCERTAIN;
    if(action==USS_MENU_BUILD_USS){snapshot.ours_intact=1;snapshot.vanilla_intact=0;}
    if(action==USS_MENU_RESTORE_CATEGORY){assert(value==4);snapshot.ours_intact=0;snapshot.vanilla_intact=1;}
    if(calls==replace_at)++snapshot.epoch;
    return USS_MENU_EFFECT_DONE;
}
static UssMenuHostServices fixture(UssMenuHost* host)
{
    UssMenuHostServices io={NULL,observe,effect};memset(host,0,sizeof(*host));memset(&snapshot,0,sizeof(snapshot));
    snapshot.screen=100;snapshot.content=200;snapshot.input=300;snapshot.container=400;snapshot.epoch=1;
    snapshot.valid=snapshot.escape_present=snapshot.topmost=snapshot.vanilla_intact=1;snapshot.category=4;
    calls=fail_at=uncertain_at=replace_at=0;memset(actions,0,sizeof(actions));return io;
}
int main(void)
{
    UssMenuHost host;UssMenuHostServices io;unsigned i,before;
    io=fixture(&host);snapshot.category=8;assert(uss_menu_host_begin(&host,&io) && host.saved_category==0);
    io=fixture(&host);assert(uss_menu_host_begin(&host,&io) && calls==4);
    assert(actions[0]==USS_MENU_SHOW_OPTIONS && actions[1]==USS_MENU_LAYOUT_VANILLA && actions[3]==USS_MENU_BUILD_USS);
    assert(uss_menu_host_before_update(&host,&io));assert(!uss_menu_host_before_update(&host,&io));
    assert(uss_menu_host_after_update(&host,&io,3));assert(uss_menu_host_close(&host,&io) && host.phase==USS_MENU_CLOSED);
    assert(actions[calls-1]==USS_MENU_RESTORE_CATEGORY && snapshot.vanilla_intact);
    for(i=1;i<=4;++i){
        io=fixture(&host);fail_at=i;assert(!uss_menu_host_begin(&host,&io) && calls==i && host.phase==USS_MENU_STOPPED);
        before=calls;assert(!uss_menu_host_begin(&host,&io) && calls==before);
        io=fixture(&host);uncertain_at=i;assert(!uss_menu_host_begin(&host,&io) && calls==i && host.uncertain);
        io=fixture(&host);replace_at=i;assert(!uss_menu_host_begin(&host,&io) && calls==i && host.phase==USS_MENU_STOPPED);
    }
    io=fixture(&host);snapshot.foreign_owner=1;assert(!uss_menu_host_begin(&host,&io) && !calls);
    io=fixture(&host);assert(uss_menu_host_begin(&host,&io));snapshot.topmost=0;before=calls;
    assert(!uss_menu_host_before_update(&host,&io) && calls==before && host.phase==USS_MENU_OWNED);
    snapshot.topmost=1;snapshot.foreign_owner=1;assert(!uss_menu_host_before_update(&host,&io) && calls==before && host.phase==USS_MENU_STOPPED);
    io=fixture(&host);assert(uss_menu_host_begin(&host,&io));snapshot.ours_intact=0;before=calls;
    assert(!uss_menu_host_close(&host,&io) && calls==before); /* Never rebuild foreign/unowned rows. */
    io=fixture(&host);assert(uss_menu_host_begin(&host,&io));++snapshot.epoch;before=calls;
    assert(!uss_menu_host_close(&host,&io) && calls==before);
    for(i=5;i<=7;++i){io=fixture(&host);assert(uss_menu_host_begin(&host,&io));uncertain_at=i;
        assert(!uss_menu_host_close(&host,&io) && calls==i && host.uncertain);}
    puts("USS host: native update phases, layout-before-hide, restore, every open/close effect refusal, screen replacement and foreign ownership PASS");return 0;
}
