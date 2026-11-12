/* Native USS tab/category presentation. Original menu layout/sequence: 0BSD
 * CowboyBingus ModOptionsMenu widgets.lua/view.lua. Session-owned storage only.
 * Typed entries must already be resolved; guard revalidates pins and host at
 * every call/write, including intermediate states owned by this operation. */
#ifndef USS_NATIVE_MENU_CHROME_H
#define USS_NATIVE_MENU_CHROME_H
#include "uss_native_menu_rows.h"
static const uint32_t uss_menu_vanilla_tabs[3]={0xd876b36e,0x78934e12,0x8c02bd80};
static const uint32_t uss_menu_vanilla_categories[9]={0x396f27d7,0x196248d0,0xa522d2d0,0x15cecac1,0x9a688238,0x07e3b100,0x5db3b764,0xa7c345ef,0x9bb47b92};
typedef struct UssMenuChrome {
    UssMenuHostSnapshot host;
    char tab_title[USS_MENU_TEXT_CAP],category_title[USS_MENU_TEXT_CAP];
    uint32_t tabs[8];
    uint32_t tab_index;
    unsigned char saved_visible[9];
    unsigned visibility_saved;
    unsigned prepared,tab_entered,tab_ready,categories_owned,stopped,effects;
} UssMenuChrome;
static inline int uss_menu_chrome_prepare(UssMenuChrome* s,const UssMenuHostSnapshot* host,const char* tab,const char* category)
{
    UssMenuChrome next={0};
    if(!s || s->tab_entered || !host || !uss_menu_same_host(host,host) ||
        !uss_menu_pointer((uintptr_t)host->screen) || host->screen>UINTPTR_MAX-5000000 ||
        host->content!=host->screen+3010456 || host->container!=host->content+1150848+544 ||
        !host->escape_present || !host->topmost || host->language_changing || host->foreign_owner ||
        !uss_menu_copy_text(next.tab_title,tab) || !uss_menu_copy_text(next.category_title,category))return 0;
    next.host=*host;memcpy(next.tabs,uss_menu_vanilla_tabs,sizeof(uss_menu_vanilla_tabs));
    next.tab_index=UINT32_MAX;next.prepared=1;*s=next;return 1;
}
static inline int uss_menu_chrome_guard(UssMenuChrome* s,const UssMenuRowsIo* io)
{
    if(!s || !io || !s->prepared || s->stopped || !io->guard || !io->read || !io->write ||
        !io->guard(io->user,&s->host,0)){if(s)s->stopped=1;return 0;}return 1;
}
#define USS_CHROME_CALL(s,io,expression) do {if(!uss_menu_chrome_guard(s,io))return 0; \
    ++(s)->effects;(expression);} while(0)
static inline int uss_menu_chrome_write(UssMenuChrome* s,const UssMenuRowsIo* io,uintptr_t at,const void* value,size_t length)
{
    unsigned char actual[4];if((length!=1 && length!=4) || !uss_menu_chrome_guard(s,io))return 0;
    ++s->effects;
    if(!io->write(io->user,at,value,length) || !io->read(io->user,at,actual,length) || memcmp(value,actual,length)){
        s->stopped=1;return 0;}return uss_menu_chrome_guard(s,io);
}
static inline int uss_menu_chrome_text(UssMenuChrome* s,const UssMenuRowsIo* io,uintptr_t widget,const char* text)
{
    USS_CHROME_CALL(s,io,io->native.set_label(widget,UINT32_C(0xc67c7faf)));
    USS_CHROME_CALL(s,io,io->native.set_string_arg(widget,UINT32_C(0xab2a7b35),text));return 1;
}
/* Append one USS tab to the current native list (capacity8). Existing IDs
 * stay unchanged, so native set_label preserves their title arguments. */
static inline int uss_menu_chrome_add_tab(UssMenuChrome* s,const UssMenuRowsIo* io)
{
    uintptr_t bar;uint32_t count,current,selected_state=3;unsigned char active=1;
    if(!s || !io || !s->prepared || s->tab_entered || !io->native.set_tab_labels ||
        !io->native.set_label || !io->native.set_string_arg || !uss_menu_chrome_guard(s,io))return 0;
    bar=(uintptr_t)s->host.screen+1248;
    if(!io->read(io->user,bar+57448,&count,4) || count<3 || count>=8 ||
        !io->read(io->user,bar+57452,&current,4) || current>=count ||
        !io->read(io->user,bar+57320,s->tabs,count*4))return 0;
    s->tabs[count]=UINT32_C(0xc67c7faf);s->tab_index=count;s->tab_entered=1;
    USS_CHROME_CALL(s,io,io->native.set_tab_labels(bar,s->tabs,(int)count+1));
    if(!uss_menu_chrome_write(s,io,bar+3400*current+11004,&selected_state,4) ||
        !uss_menu_chrome_write(s,io,bar+3400*current+11021,&active,1) ||
        !uss_menu_chrome_text(s,io,bar+8296+3400*s->tab_index,s->tab_title))return 0;
    if(!io->read(io->user,bar+57448,&count,4) || count!=s->tab_index+1){s->stopped=1;return 0;}
    s->tab_ready=1;return uss_menu_chrome_guard(s,io);
}
static inline int uss_menu_chrome_categories(UssMenuChrome* s,const UssMenuRowsIo* io,unsigned restore)
{
    unsigned i;uintptr_t button;uint32_t flags;
    if(!s || !io || !s->tab_ready || restore>1 || (restore?!s->categories_owned:s->categories_owned) ||
        !io->native.set_visible)return 0;
    if(!restore){
        /* Capture visibility before the first effect. Labels/arguments stay untouched. */
        for(i=0;i<9;++i){
            button=(uintptr_t)s->host.content+816+14920*i;
            if(!uss_menu_chrome_guard(s,io) || !io->read(io->user,button,&flags,4))return 0;
            s->saved_visible[i]=(flags&0x10u)!=0;
        }
        s->visibility_saved=1;
    }else if(!s->visibility_saved)return 0;
    for(i=0;i<9;++i){
        button=(uintptr_t)s->host.content+816+14920*i;
        USS_CHROME_CALL(s,io,io->native.set_visible(button,restore?s->saved_visible[i]:0));
    }
    s->categories_owned=!restore;return uss_menu_chrome_guard(s,io);
}
/* Native category selection does not maintain PREVIOUS_CATEGORY itself. To
 * force a rebuild of the same panel, take the original alternate-panel route. */
static inline int uss_menu_chrome_select_category(UssMenuChrome* s,const UssMenuRowsIo* io,uint32_t target)
{
    uint32_t shown,other;
    if(!s || !io || !s->tab_ready || target>=8 || !io->native.select_category || !uss_menu_chrome_guard(s,io))return 0;
    if(!io->read(io->user,(uintptr_t)s->host.content+1318488,&shown,4) || shown>=9)return 0;
    if(shown==target){
        other=target==0?3:0;
        if(!uss_menu_chrome_write(s,io,(uintptr_t)s->host.content+1318492,&shown,4))return 0;
        USS_CHROME_CALL(s,io,io->native.select_category(s->host.content,(int)other));shown=other;
    }
    if(!uss_menu_chrome_write(s,io,(uintptr_t)s->host.content+1318492,&shown,4))return 0;
    USS_CHROME_CALL(s,io,io->native.select_category(s->host.content,(int)target));
    if(!io->read(io->user,(uintptr_t)s->host.content+1318488,&shown,4) || shown!=target){s->stopped=1;return 0;}
    return uss_menu_chrome_guard(s,io);
}
/* The actual tab bar stays USS, but SHOWN_CONTENT remains OPTIONS for native
 * input. Original ModOptionsMenu step runs the visual pass itself once per tick.
 * Never write SHOWN_CONTENT=3: that bypasses the native Options input route. */
static inline int uss_menu_chrome_present(UssMenuChrome* s,const UssMenuRowsIo* io,unsigned uss,float dt)
{
    uint32_t tab=2;
    if(!s || !io || !s->tab_ready || !s->categories_owned || uss>1 || !isfinite(dt) || dt<0 || dt>1 ||
        !io->native.set_content_hidden || !io->native.options_visual)return 0;
    if(!uss_menu_chrome_write(s,io,(uintptr_t)s->host.screen+8,&tab,4))return 0;
    USS_CHROME_CALL(s,io,io->native.set_content_hidden(s->host.content,0,1));
    if(uss){USS_CHROME_CALL(s,io,io->native.options_visual(s->host.content,dt));}
    return uss_menu_chrome_guard(s,io);
}
#undef USS_CHROME_CALL
#endif
