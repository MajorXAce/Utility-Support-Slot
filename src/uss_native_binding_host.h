/* Separate native binding-screen contract, from 0BSD ModBindingsMenu.
 * Binding rows are NOT Options selectors. Observation/resolution only; no call,
 * list rebuild, slot reservation or mapping/default writes are performed here. */
#ifndef USS_NATIVE_BINDING_HOST_H
#define USS_NATIVE_BINDING_HOST_H
#include "uss_native_menu_observation.h"
#include "uss_native_menu_entries.h"
typedef void (*UssNativeBindingBuild)(uintptr_t,const UssNativeBindingRow*);
typedef void (*UssNativeBindingReset)(uintptr_t,int);
typedef struct UssNativeBindingEntries {UssNativeBindingBuild build;UssNativeBindingReset reset;} UssNativeBindingEntries;
static const UssMenuEntryPin uss_binding_host_pins[]={
    {0x1812940,13,{0x48,0x8b,0xc4,0x53,0x41,0x56,0x48,0x81,0xec,0xd8,0x00,0x00,0x00}},
    {0x1813dd0,16,{0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9,0x85,0xd2,0x74,0x30,0x83,0xea,0x01}}
};
static inline int uss_native_binding_entries(const UssMenuEntryReader* reader,uintptr_t base,size_t size,UssNativeBindingEntries* out)
{
    UssNativeBindingEntries entries;uint64_t epoch;unsigned i;unsigned char bytes[16];
    if(!reader || !reader->context || !reader->read_executable || !out || !uss_menu_pointer(base) ||
        !size || base>UINTPTR_MAX-size)return 0;
    epoch=reader->context(reader->user);if(!epoch)return 0;
    for(i=0;i<2;++i){const UssMenuEntryPin* pin=&uss_binding_host_pins[i];
        if(pin->rva>size || pin->length>size-pin->rva ||
            !reader->read_executable(reader->user,base+pin->rva,bytes,pin->length) || memcmp(bytes,pin->bytes,pin->length))return 0;}
    if(reader->context(reader->user)!=epoch)return 0;
    entries.build=(UssNativeBindingBuild)(base+0x1812940);entries.reset=(UssNativeBindingReset)(base+0x1813dd0);
    *out=entries;return 1;
}
typedef struct UssNativeBindingHost {
    uint64_t epoch;uintptr_t screen,listing,input;
    uint32_t selected_tab,tab_count,row_count;
    unsigned valid,open,topmost,language_changing,controller_page,rows_match;
} UssNativeBindingHost;
static inline int uss_native_binding_layout_valid(const UssNativeBindingRow* rows,unsigned count)
{
    unsigned i;if(!rows || !count || count>USS_BINDING_ROWS_MAX)return 0;
    for(i=0;i<count;++i){
        if(rows[i].label){if(rows[i].group!=13 || rows[i].action)return 0;}
        else if(rows[i].group>=13 || rows[i].action>=97)return 0;
    }
    return 1;
}
static inline int uss_native_binding_host_observe(const UssMenuObservationIo* io,const UssNativeBindingRow* expected,
    unsigned count,UssNativeBindingHost* output,UssMenuReadBudget* statistics)
{
    UssMenuReadBudget b={0};UssNativeBindingHost result={0};uint64_t epoch;
    uintptr_t ui,menu,input,screen,again;uint32_t stack[6],again_stack[6],page,value;unsigned i,open=0;
    if(!io || !io->context || !io->read || !output || !uss_menu_pointer(io->base) ||
        io->module_size<0x347cf20 || io->base>UINTPTR_MAX-io->module_size ||
        (count && !uss_native_binding_layout_valid(expected,count)) || (!count && expected))return 0;
    b.io=io;epoch=io->context(io->user);if(!epoch)goto refused;
    if(!uss_menu_ptr(&b,io->base+0x347ce28,&ui) || !uss_menu_read(&b,ui+0x429c,stack,24) || stack[5]>5)goto refused;
    for(i=0;i<stack[5];++i)if(stack[i]==26)open=1;
    if(!open){result.valid=1;result.epoch=epoch;goto commit;}
    if(!uss_menu_ptr(&b,io->base+0x347ce38,&menu) || !uss_menu_ptr(&b,menu+208,&screen) ||
        screen>UINTPTR_MAX-3000000 || !uss_menu_ptr(&b,io->base+0x347cf18,&input))goto refused;
    result.epoch=epoch;result.screen=screen;result.listing=screen+338344;result.input=input;
    result.open=1;result.topmost=stack[stack[5]-1]==26;
    result.language_changing=!io->language_state || io->language_state(io->user)!=0;
    if(!uss_menu_u32(&b,screen+8,&result.selected_tab) || !uss_menu_u32(&b,screen+1248+57448,&result.tab_count) ||
        result.tab_count<3 || result.tab_count>8 || result.selected_tab>=result.tab_count ||
        !uss_menu_u32(&b,result.listing+2411904,&page) ||
        !uss_menu_u32(&b,result.listing+2411916,&result.row_count) || result.row_count>USS_BINDING_ROWS_MAX)goto refused;
    result.controller_page=page!=0;result.rows_match=count && result.row_count==count;
    for(i=0;result.rows_match && i<count;++i){
        uintptr_t row=result.listing+7856+(uintptr_t)i*USS_BINDING_ROW_BYTES;
        if(expected[i].label){
            if(!uss_menu_u32(&b,row+24752,&value))goto refused;
            if(value!=4){result.rows_match=0;break;}
            if(!uss_menu_u32(&b,row+4264+272,&value))goto refused;
            if(value!=expected[i].label)result.rows_match=0;
        }else{
            if(!uss_menu_u32(&b,row+24760,&value))goto refused;
            if(value!=expected[i].group*65536u+expected[i].action)result.rows_match=0;
        }
    }
    if(!uss_menu_ptr(&b,io->base+0x347ce28,&again) || again!=ui ||
        !uss_menu_read(&b,ui+0x429c,again_stack,24) || memcmp(stack,again_stack,24) ||
        !uss_menu_ptr(&b,io->base+0x347ce38,&again) || again!=menu ||
        !uss_menu_ptr(&b,menu+208,&again) || again!=screen ||
        !uss_menu_ptr(&b,io->base+0x347cf18,&again) || again!=input)goto refused;
    result.valid=1;
commit:
    if(io->context(io->user)!=epoch)goto refused;
    *output=result;if(statistics)*statistics=b;return 1;
refused:
    if(statistics)*statistics=b;return 0; /* Preserve caller output; no inferred native ownership. */
}
#endif
