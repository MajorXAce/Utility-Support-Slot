#include <assert.h>
#include <stdio.h>
#include "../src/uss_native_menu_chrome.h"
static _Alignas(16) unsigned char screen[5000000];
static UssMenuChrome chrome;
static UssMenuHostSnapshot host;
static unsigned calls,guards,fail_guard,fail_write,selects,visuals;
static uintptr_t bar,content;
static int read_memory(void* user,uintptr_t at,void* out,size_t length)
{
    (void)user;if(at<(uintptr_t)screen || at>(uintptr_t)screen+sizeof(screen) ||
        length>(uintptr_t)screen+sizeof(screen)-at)return 0;
    memcpy(out,(const void*)at,length);return 1;
}
static int write_memory(void* user,uintptr_t at,const void* data,size_t length)
{
    unsigned char actual[4];(void)user;
    assert((length==4 && ((at>=bar+11004 && at<=bar+3400*7+11004 && (at-bar-11004)%3400==0) || at==content+1318492 || at==(uintptr_t)screen+8)) ||
        (length==1 && at>=bar+11021 && at<=bar+3400*7+11021 && (at-bar-11021)%3400==0));++calls;
    if(fail_write)return 0;assert(read_memory(NULL,at,actual,length));memcpy((void*)at,data,length);return 1;
}
static int guard(void* user,const UssMenuHostSnapshot* expected,unsigned built)
{
    (void)user;(void)built;++guards;
    return guards!=fail_guard && uss_menu_same_host(expected,&host) && !host.foreign_owner &&
        host.topmost && !host.language_changing;
}
static void tabs(uint64_t widget,const uint32_t* labels,int count)
{
    assert(widget==bar && labels==chrome.tabs && count>=4 && count<=8);
    memcpy((void*)(bar+57320),labels,(size_t)count*4);uss_menu_put_word((unsigned char*)bar,57448,(uint32_t)count);++calls;
}
static void label(uint64_t widget,uint32_t value)
{uss_menu_put_word((unsigned char*)(uintptr_t)widget,272,value);++calls;}
static void text(uint64_t widget,uint32_t key,const char* value)
{
    uintptr_t address=(uintptr_t)value;
    assert(key==UINT32_C(0xab2a7b35) && (value==chrome.tab_title || value==chrome.category_title));
    memcpy((void*)(uintptr_t)(widget+288),&address,8);++calls;
}
static uint8_t clear(uint64_t widget){assert(widget>=content);++calls;return 1;}
static void visible(uint64_t widget,uint8_t value)
{uint32_t flags;assert(widget>=content+816 && value<=1);memcpy(&flags,(void*)(uintptr_t)widget,4);flags=(flags&~0x10u)|(value?0x10u:0);memcpy((void*)(uintptr_t)widget,&flags,4);++calls;}
static void select_category(uint64_t widget,int index)
{assert(widget==content && index>=0 && index<8);uss_menu_put_word((unsigned char*)content,1318488,(uint32_t)index);++selects;++calls;}
static void hidden(uint64_t widget,uint8_t a,uint8_t b)
{assert(widget==content && !a && b==1);++calls;}
static void visual(uint64_t widget,float dt)
{assert(widget==content && dt>=0 && dt<=1);++visuals;++calls;}
static UssMenuRowsIo setup(void)
{
    UssMenuRowsIo io={0};unsigned i;
    memset(screen,0,sizeof(screen));memset(&chrome,0,sizeof(chrome));memset(&host,0,sizeof(host));
    calls=guards=fail_guard=fail_write=selects=visuals=0;
    bar=(uintptr_t)screen+1248;content=(uintptr_t)screen+3010456;
    host.screen=(uintptr_t)screen;host.content=content;host.container=content+1150848+544;
    host.input=0x20000;host.epoch=1;host.valid=host.escape_present=host.topmost=host.vanilla_intact=1;
    memcpy((void*)(bar+57320),uss_menu_vanilla_tabs,12);uss_menu_put_word((unsigned char*)bar,57448,3);
    for(i=0;i<9;++i)uss_menu_put_word((unsigned char*)content,816+14920*i+1928+272,uss_menu_vanilla_categories[i]);
    assert(uss_menu_chrome_prepare(&chrome,&host,"USS","Utility Support Slot"));
    io.guard=guard;io.read=read_memory;io.write=write_memory;io.native.set_tab_labels=tabs;
    io.native.set_label=label;io.native.set_string_arg=text;io.native.clear_args=clear;io.native.set_visible=visible;
    io.native.select_category=select_category;io.native.set_content_hidden=hidden;io.native.options_visual=visual;return io;
}
static int cycle(const UssMenuRowsIo* io)
{
    return uss_menu_chrome_add_tab(&chrome,io) && uss_menu_chrome_categories(&chrome,io,0) &&
        uss_menu_chrome_select_category(&chrome,io,0) && uss_menu_chrome_present(&chrome,io,0,0) &&
        uss_menu_chrome_present(&chrome,io,1,0.016f) && uss_menu_chrome_categories(&chrome,io,1) &&
        uss_menu_chrome_select_category(&chrome,io,0);
}
int main(void)
{
    UssMenuRowsIo io;unsigned i,total,before;uintptr_t text_address;
    io=setup();assert(cycle(&io));assert(uss_menu_word(screen,8)==2);assert(selects==4 && visuals==1 && chrome.tab_ready && !chrome.categories_owned);
    assert(uss_menu_word((unsigned char*)bar,57448)==4);
    memcpy(&text_address,(void*)(bar+8296+3400*3+288),8);assert(text_address==(uintptr_t)chrome.tab_title);
    for(i=0;i<9;++i)assert(uss_menu_word((unsigned char*)content,816+14920*i+1928+272)==uss_menu_vanilla_categories[i]);
    assert(!uss_menu_chrome_add_tab(&chrome,&io));
    assert(!uss_menu_chrome_prepare(&chrome,&host,"changed","changed"));
    assert(uss_menu_chrome_categories(&chrome,&io,0)); /* Reopen without adding another tab. */
    io=setup();assert(cycle(&io));total=guards;
    for(i=1;i<=total;++i){
        io=setup();fail_guard=i;assert(!cycle(&io));before=calls;
        assert(!uss_menu_chrome_categories(&chrome,&io,0) && calls==before);
        assert(!uss_menu_chrome_add_tab(&chrome,&io) && calls==before);
    }
    io=setup();uss_menu_put_word((unsigned char*)bar,57448,8);assert(!uss_menu_chrome_add_tab(&chrome,&io) && !calls);
    io=setup();uss_menu_put_word((unsigned char*)bar,57320,123);assert(uss_menu_chrome_add_tab(&chrome,&io));
    /* Generic preexisting menus: preserve label IDs, title args and category
     * labels; restore each original visibility instead of assuming vanilla. */
    io=setup();uss_menu_put_word((unsigned char*)bar,57448,6);
    uss_menu_put_word((unsigned char*)bar,57452,4);
    for(i=0;i<6;++i)uss_menu_put_word((unsigned char*)bar,57320+4*i,900+i);
    for(i=0;i<9;++i){
        uss_menu_put_word((unsigned char*)content,816+14920*i+1928+272,1000+i);
        uss_menu_put_word((unsigned char*)content,816+14920*i,(i&1)?0x10:0);
    }
    {uintptr_t original_text=0x55550000;
     memcpy((void*)(bar+8296+3400*3+288),&original_text,8);
     assert(cycle(&io) && chrome.tab_index==6 && uss_menu_word((unsigned char*)bar,57448)==7);
     memcpy(&text_address,(void*)(bar+8296+3400*3+288),8);assert(text_address==original_text);}
    for(i=0;i<6;++i)assert(uss_menu_word((unsigned char*)bar,57320+4*i)==900+i);
    for(i=0;i<9;++i){
        assert(uss_menu_word((unsigned char*)content,816+14920*i+1928+272)==1000+i);
        assert((uss_menu_word((unsigned char*)content,816+14920*i)&0x10)==((i&1)?0x10u:0));
    }
    io=setup();fail_write=1;assert(!uss_menu_chrome_add_tab(&chrome,&io) && chrome.stopped);
    io=setup();++host.epoch;assert(!uss_menu_chrome_add_tab(&chrome,&io) && !calls);
    io=setup();host.foreign_owner=1;assert(!uss_menu_chrome_add_tab(&chrome,&io) && !calls);
    puts("USS native tab/categories: retained labels, vanilla restoration, same-panel rebuild, reopen and all guard refusals PASS");return 0;
}
