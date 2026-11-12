/* Native GAMEPLAY selector rows, following the 0BSD original menu's builder.
 * Storage belongs to the session, never the stack: widgets retain text/descriptor
 * pointers even when hidden. Caller supplies validated native entries and host.
 * No installation, registration or calls happen merely by constructing this data. */
#ifndef USS_NATIVE_MENU_ROWS_H
#define USS_NATIVE_MENU_ROWS_H
#include "uss_native_menu_observation.h"
#include "uss_native_menu_entries.h"
#include "uss_native_assignment_display.h"
#include <math.h>

enum {USS_MENU_TEXT_CAP=128};
typedef struct UssMenuSelectorSpec {
    const char* title;
    unsigned count,selected;
    const char* choices[USS_SELECTOR_CHOICES_MAX];
} UssMenuSelectorSpec;
typedef struct UssMenuSelectorStorage {
    UssOptionDescriptor descriptor;
    char title[USS_MENU_TEXT_CAP],choices[USS_SELECTOR_CHOICES_MAX][USS_MENU_TEXT_CAP];
    unsigned count,selected;
} UssMenuSelectorStorage;
typedef struct UssMenuRows {
    UssMenuHostSnapshot host;
    UssMenuSelectorStorage rows[USS_MENU_OWN_ROWS];
    _Alignas(16) float zero_rect[4];
    UssAssignmentDisplay assignment;
    unsigned assignment_press_latched;
    unsigned count,prepared,entered,built,stopped,effects;
} UssMenuRows;
typedef struct UssMenuRowsIo {
    void* user;
    /* Revalidate current host, foreground/language/ownership and native pins.
     * During construction the page is partial: never require completed rows. */
    int (*guard)(void*,const UssMenuHostSnapshot*,unsigned built);
    int (*read)(void*,uintptr_t,void*,size_t);
    int (*write)(void*,uintptr_t,const void*,size_t); /* Exact restricted field write. */
    UssNativeMenuEntries native;
} UssMenuRowsIo;
static inline int uss_menu_copy_text(char* out,const char* text)
{
    size_t i;if(!text)return 0;
    for(i=0;i<USS_MENU_TEXT_CAP;++i){out[i]=text[i];if(!text[i])return i!=0;}
    return 0;
}
static inline int uss_menu_rows_prepare(UssMenuRows* state,const unsigned char* inert,
    size_t length,const UssMenuSelectorSpec* specs,unsigned count)
{
    UssMenuRows prepared={0};unsigned i,j;uint32_t labels[USS_SELECTOR_CHOICES_MAX];
    if(!state || state->entered || !specs || !count || count>USS_MENU_OWN_ROWS)return 0;
    for(j=0;j<USS_SELECTOR_CHOICES_MAX;++j)labels[j]=UINT32_C(0xc67c7faf);
    for(i=0;i<count;++i){
        UssMenuSelectorStorage* row=&prepared.rows[i];
        if(specs[i].count<2 || specs[i].count>USS_SELECTOR_CHOICES_MAX || specs[i].selected>=specs[i].count ||
            !uss_menu_copy_text(row->title,specs[i].title) ||
            !uss_menu_selector(&row->descriptor,inert,length,UINT32_C(0xc67c7faf),labels,specs[i].count))return 0;
        for(j=0;j<specs[i].count;++j)if(!uss_menu_copy_text(row->choices[j],specs[i].choices[j]))return 0;
        row->count=specs[i].count;row->selected=specs[i].selected;
    }
    prepared.count=count;prepared.prepared=1;*state=prepared;return 1;
}
static inline int uss_menu_rows_guard(UssMenuRows* s,const UssMenuRowsIo* io)
{
    if(!s || !io || !io->guard || !io->read || !io->write || s->stopped ||
        !io->guard(io->user,&s->host,s->built)){if(s)s->stopped=1;return 0;}
    return 1;
}
typedef struct UssMenuAssignmentGuard {UssMenuRows* rows;const UssMenuRowsIo* io;} UssMenuAssignmentGuard;
static int uss_menu_assignment_guard(void* user,uintptr_t parent)
{
    UssMenuAssignmentGuard* bridge=(UssMenuAssignmentGuard*)user;
    return bridge && parent==bridge->rows->host.content+144000+USS_OPTION_ROW_BYTES &&
        uss_menu_rows_guard(bridge->rows,bridge->io);
}
static inline UssAssignmentDisplayIo uss_menu_assignment_io(UssMenuAssignmentGuard* bridge)
{
    UssAssignmentDisplayIo io={bridge,uss_menu_assignment_guard,{0}};
    io.native=bridge->io->native;return io;
}
/* Prepare the mixed two-row page without native effects. A zero choice count
 * denotes the assignment action; ordinary selectors retain their old contract. */
static inline int uss_menu_rows_prepare_assignment(UssMenuRows* rows,const unsigned char* inert,
    size_t length,unsigned mode,const char* caption)
{
    UssMenuSelectorSpec specs[2]={{"Binding mode",2,0,{"Default","Dedicated"}},
        {"Utility Weapon",2,0,{"unused","unused"}}};
    if(mode<1 || mode>2 || !uss_assignment_caption_length(caption))return 0;
    specs[0].selected=mode-1;
    if(!uss_menu_rows_prepare(rows,inert,length,specs,2) ||
        !uss_menu_assignment_descriptor(&rows->rows[1].descriptor,inert,length,UINT32_C(0xc67c7faf)))return 0;
    rows->rows[1].count=0;
    return uss_menu_copy_text(rows->rows[1].choices[0],caption);
}
/* Each void native call is consumed before entry. If a subsequent readback
 * refuses, the page stops; no rebuilding a potentially half-mutated widget. */
#define USS_MENU_ROWS_CALL(s,io,call) do { \
    if(!uss_menu_rows_guard((s),(io)))return 0; \
    ++(s)->effects;(call); \
} while(0)
static inline int uss_menu_rows_write(UssMenuRows* s,const UssMenuRowsIo* io,uintptr_t at,const void* data,size_t length)
{
    unsigned char actual[4];
    if(length!=4 || !uss_menu_rows_guard(s,io))return 0;
    ++s->effects;
    if(!io->write(io->user,at,data,length) || !io->read(io->user,at,actual,length) || memcmp(actual,data,length)){
        s->stopped=1;return 0;
    }
    return uss_menu_rows_guard(s,io);
}
static inline int uss_menu_rows_text(UssMenuRows* s,const UssMenuRowsIo* io,uintptr_t widget,const char* text)
{
    USS_MENU_ROWS_CALL(s,io,io->native.set_label(widget,UINT32_C(0xc67c7faf)));
    USS_MENU_ROWS_CALL(s,io,io->native.set_string_arg(widget,UINT32_C(0xab2a7b35),text));return 1;
}
static inline void uss_menu_rows_expected(const UssMenuRows* s,UssMenuExpectedRows* expected)
{
    unsigned i;expected->count=s->count;
    for(i=0;i<s->count;++i){expected->row_text[i]=(uintptr_t)s->rows[i].title;
        expected->choices[i]=s->rows[i].count;expected->first_choice[i]=UINT32_C(0xc67c7faf);
        if(!s->rows[i].count)expected->assignment_display[i]=(uintptr_t)s->assignment.widget;}
}
static inline int uss_menu_rows_build(UssMenuRows* s,const UssMenuRowsIo* io,const UssMenuHostSnapshot* host,UssMenuVec2 pivot)
{
    uintptr_t panel,rows,settings,row;unsigned i;uint32_t selected,count;float zero=0,height;
    if(!s || !io || !host || !s->prepared || s->entered || !uss_menu_same_host(host,host) ||
        !uss_menu_pointer((uintptr_t)host->content) || host->content>UINTPTR_MAX-1320000 ||
        host->container!=host->content+1150848+544 || !host->escape_present || !host->topmost ||
        host->language_changing || host->foreign_owner || host->category!=0 ||
        !isfinite(pivot.x) || !isfinite(pivot.y) || pivot.x<0 || pivot.x>1 || pivot.y<0 || pivot.y>1 || !io->native.row_reset ||
        !io->native.row_release || !io->native.row_init || !io->native.add_child || !io->native.set_label ||
        !io->native.set_string_arg || !io->native.set_choice || !io->native.finish_simple)return 0;
    for(i=0;i<s->count;++i)if(!s->rows[i].count &&
        (s->count!=2 || i!=1 || !io->native.assignment_display_init || !io->native.assignment_text ||
         !io->native.assignment_layout || !io->native.set_opacity))return 0;
    s->host=*host;panel=(uintptr_t)host->content+1150848;
    if(!uss_menu_rows_guard(s,io) || !io->read(io->user,panel+2816,&rows,8) || rows!=host->content+144000 ||
        !io->read(io->user,panel+2856,&settings,8) || !io->read(io->user,panel+2872,&selected,4))return 0;
    if(!uss_menu_pointer(settings))settings=0; /* Original native builder's NULL fallback. */
    s->entered=1;
    for(i=USS_OPTION_ROWS_MAX;i>0;--i){
        row=rows+(uintptr_t)(i-1)*USS_OPTION_ROW_BYTES;
        USS_MENU_ROWS_CALL(s,io,io->native.row_reset(row,s->zero_rect,s->zero_rect));
        USS_MENU_ROWS_CALL(s,io,io->native.row_release(row));
    }
    for(i=0;i<s->count;++i){
        UssMenuSelectorStorage* option=&s->rows[i];UssMenuVec2 position={0,-64.0f*i};
        row=rows+(uintptr_t)i*USS_OPTION_ROW_BYTES;
        USS_MENU_ROWS_CALL(s,io,io->native.row_init(row,position,pivot,pivot,10,settings,option->descriptor.bytes,1));
        USS_MENU_ROWS_CALL(s,io,io->native.add_child(panel+544,row));
        if(!uss_menu_rows_text(s,io,row+3992,option->title))return 0;
        if(!option->count){
            UssMenuAssignmentGuard bridge={s,io};UssAssignmentDisplayIo display_io=uss_menu_assignment_io(&bridge);
            if(i!=1 || !uss_assignment_display_build(&s->assignment,&display_io,row,option->choices[0])){
                s->stopped=1;return 0;}
        }else{
            USS_MENU_ROWS_CALL(s,io,io->native.set_choice(row+16016,option->selected));
            if(!uss_menu_rows_text(s,io,row+16832,option->choices[option->selected]))return 0;
        }
    }
    count=s->count;height=64.0f*count;
    if(!uss_menu_rows_write(s,io,panel+2832,&count,4) || !uss_menu_rows_write(s,io,panel+2836,&zero,4) ||
        !uss_menu_rows_write(s,io,panel+2840,&zero,4) || !uss_menu_rows_write(s,io,panel+2844,&height,4))return 0;
    if(selected>=count){selected=0;if(!uss_menu_rows_write(s,io,panel+2872,&selected,4))return 0;}
    USS_MENU_ROWS_CALL(s,io,io->native.finish_simple(panel));
    s->built=1;return uss_menu_rows_guard(s,io);
}
/* Restore a capture selector after its command has been consumed. Native
 * widgets retain our choice strings; never rebuild their backing storage. */
static inline int uss_menu_rows_select(UssMenuRows* s,const UssMenuRowsIo* io,unsigned index,unsigned choice)
{
    uintptr_t row;uint32_t actual;
    if(!s || !s->built || index>=s->count || choice>=s->rows[index].count ||
        !io || !io->native.set_choice || !uss_menu_rows_guard(s,io))return 0;
    row=(uintptr_t)s->host.content+144000+(uintptr_t)index*USS_OPTION_ROW_BYTES;
    ++s->effects;io->native.set_choice(row+16016,choice);
    if(!uss_menu_rows_guard(s,io) || !io->read(io->user,row+29068,&actual,4) || actual!=choice ||
        !uss_menu_rows_text(s,io,row+16832,s->rows[index].choices[choice])){s->stopped=1;return 0;}
    s->rows[index].selected=choice;return uss_menu_rows_guard(s,io);
}
/* Poll only while the host owns this page. All values are read before committing
 * changes, so an invalid later row cannot leak a partially accepted settings edit.
 * Native text refresh precedes committing the returned selection snapshot. */
static inline int uss_menu_rows_poll(UssMenuRows* s,const UssMenuRowsIo* io,unsigned* values,unsigned capacity)
{
    unsigned i,press_read=0;uint32_t read_values[USS_MENU_OWN_ROWS];uintptr_t row;
    if(!s || !s->built || !values || capacity<s->count || !uss_menu_rows_guard(s,io))return 0;
    for(i=0;i<s->count;++i){
        row=(uintptr_t)s->host.content+144000+(uintptr_t)i*USS_OPTION_ROW_BYTES;
        if(!s->rows[i].count){
            unsigned char selected,press;
            if(!io->read(io->user,row+0x3e85,&selected,1) || !io->read(io->user,row+0x7aca,&press,1) ||
                selected>1 || press>1){s->stopped=1;return 0;}
            read_values[i]=selected && press && !s->assignment_press_latched;
            /* Commit the latch only after every row and final host proof. */
            press_read=press;
        }else if(!io->read(io->user,row+29068,&read_values[i],4) || read_values[i]>=s->rows[i].count){s->stopped=1;return 0;}
    }
    if(!uss_menu_rows_guard(s,io))return 0;
    for(i=0;i<s->count;++i)if(s->rows[i].count && read_values[i]!=s->rows[i].selected){
        row=(uintptr_t)s->host.content+144000+(uintptr_t)i*USS_OPTION_ROW_BYTES;
        if(!uss_menu_rows_text(s,io,row+16832,s->rows[i].choices[read_values[i]]))return 0;
    }
    if(!uss_menu_rows_guard(s,io))return 0;
    if(s->count==2 && !s->rows[1].count)s->assignment_press_latched=press_read;
    for(i=0;i<s->count;++i){if(s->rows[i].count)s->rows[i].selected=read_values[i];values[i]=read_values[i];}return 1;
}
static inline int uss_menu_rows_assignment_caption(UssMenuRows* rows,const UssMenuRowsIo* io,const char* caption)
{
    UssMenuAssignmentGuard bridge={rows,io};UssAssignmentDisplayIo display_io;
    if(!rows || !rows->built || rows->count!=2 || rows->rows[1].count)return 0;
    display_io=uss_menu_assignment_io(&bridge);
    if(!uss_assignment_display_caption(&rows->assignment,&display_io,caption)){rows->stopped=1;return 0;}return 1;
}
#undef USS_MENU_ROWS_CALL
#endif
