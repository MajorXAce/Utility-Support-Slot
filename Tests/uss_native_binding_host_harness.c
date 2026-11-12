#include <assert.h>
#include <stdio.h>
#include "../src/uss_native_binding_host.h"
static unsigned char ui[0x4400],screen[3000000];
static unsigned reads,fail_at,scopes,change_scope,bad_code;
static uint64_t scope(void* user){(void)user;return ++scopes>1 && change_scope?2:1;}
static int language(void* user){(void)user;return 0;}
static int read_memory(void* user,uintptr_t at,void* out,size_t length)
{
    uintptr_t pointer;(void)user;if(++reads==fail_at)return 0;
    if(length==8){
        if(at==0x10000000+0x347ce28)pointer=0x20000000;
        else if(at==0x10000000+0x347ce38)pointer=0x21000000;
        else if(at==0x10000000+0x347cf18)pointer=0x23000000;
        else if(at==0x21000000+208)pointer=0x22000000;
        else goto buffers;
        memcpy(out,&pointer,8);return 1;
    }
buffers:
    if(at>=0x20000000 && at-0x20000000<=sizeof(ui) && length<=sizeof(ui)-(at-0x20000000)){
        memcpy(out,ui+at-0x20000000,length);return 1;}
    if(at>=0x22000000 && at-0x22000000<=sizeof(screen) && length<=sizeof(screen)-(at-0x22000000)){
        memcpy(out,screen+at-0x22000000,length);return 1;}return 0;
}
static int code_read(void* user,uintptr_t at,unsigned char* out,size_t length)
{
    unsigned i;(void)user;
    for(i=0;i<2;++i){
        if(at==0x10000000+uss_binding_host_pins[i].rva && length==uss_binding_host_pins[i].length){
            memcpy(out,uss_binding_host_pins[i].bytes,length);if(bad_code)out[0]^=1;return 1;
        }
    }
    return 0;
}
static UssMenuObservationIo fixture(void)
{
    UssMenuObservationIo io={NULL,0x10000000,0x4000000,scope,language,read_memory};
    memset(ui,0,sizeof(ui));memset(screen,0,sizeof(screen));reads=fail_at=scopes=change_scope=bad_code=0;
    uss_menu_put_word(ui,0x429c,1);uss_menu_put_word(ui,0x429c+4,26);uss_menu_put_word(ui,0x429c+20,2);
    uss_menu_put_word(screen,1248+57448,4);uss_menu_put_word(screen,8,3);
    uss_menu_put_word(screen,338344+2411916,2);
    uss_menu_put_word(screen,338344+7856+24752,4);uss_menu_put_word(screen,338344+7856+4264+272,100);
    uss_menu_put_word(screen,338344+7856+24784+24760,12*65536+1);return io;
}
int main(void)
{
    UssMenuObservationIo io;UssMenuEntryReader reader={NULL,scope,code_read};UssNativeBindingEntries entries={0},entry_before;
    UssNativeBindingHost result={0},before;UssMenuReadBudget budget;
    UssNativeBindingRow layout[2]={{13,0,100},{12,1,0}},maximum[USS_BINDING_ROWS_MAX];unsigned total,i;
    io=fixture();assert(uss_native_binding_host_observe(&io,layout,2,&result,&budget));
    assert(result.valid && result.open && result.topmost && result.rows_match && result.listing==0x22000000+338344 && !result.controller_page);
    before=result;total=reads;
    for(i=1;i<=total;++i){io=fixture();fail_at=i;assert(!uss_native_binding_host_observe(&io,layout,2,&result,&budget) && !memcmp(&result,&before,sizeof(result)));}
    io=fixture();change_scope=1;assert(!uss_native_binding_host_observe(&io,layout,2,&result,&budget));
    io=fixture();uss_menu_put_word(screen,338344+2411904,1);assert(uss_native_binding_host_observe(&io,layout,2,&result,&budget) && result.controller_page);
    io=fixture();uss_menu_put_word(screen,338344+7856+24784+24760,123);assert(uss_native_binding_host_observe(&io,layout,2,&result,&budget) && !result.rows_match);
    io=fixture();uss_menu_put_word(ui,0x429c+4,1);assert(uss_native_binding_host_observe(&io,layout,2,&result,&budget) && !result.open && reads==2);
    io=fixture();uss_menu_put_word(ui,0x429c+8,2);uss_menu_put_word(ui,0x429c+20,3);
    assert(uss_native_binding_host_observe(&io,layout,2,&result,&budget) && result.open && !result.topmost);
    io=fixture();layout[0].group=12;assert(!uss_native_binding_host_observe(&io,layout,2,&result,&budget) && !reads);layout[0].group=13;
    io=fixture();uss_menu_put_word(screen,338344+2411916,USS_BINDING_ROWS_MAX);
    for(i=0;i<USS_BINDING_ROWS_MAX;++i){size_t row=338344+7856+(size_t)i*USS_BINDING_ROW_BYTES;
        maximum[i].group=13;maximum[i].action=0;maximum[i].label=100;
        uss_menu_put_word(screen,row+24752,4);uss_menu_put_word(screen,row+4264+272,100);}
    assert(uss_native_binding_host_observe(&io,maximum,USS_BINDING_ROWS_MAX,&result,&budget) && result.rows_match && budget.calls<=256 && budget.bytes<=8192);
    io.language_state=NULL;assert(uss_native_binding_host_observe(&io,NULL,0,&result,&budget) && result.language_changing);
    io=fixture();assert(uss_native_binding_entries(&reader,io.base,io.module_size,&entries));entry_before=entries;
    bad_code=1;assert(!uss_native_binding_entries(&reader,io.base,io.module_size,&entries) && !memcmp(&entries,&entry_before,sizeof(entries)));
    io=fixture();change_scope=1;assert(!uss_native_binding_entries(&reader,io.base,io.module_size,&entries));
    puts("USS native binding host: separate screen/list/row ABI, bounded ownership graph, all read faults, device/foreign/covered handling and exact entry pins PASS");return 0;
}
