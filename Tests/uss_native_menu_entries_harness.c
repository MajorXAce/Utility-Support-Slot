#include <assert.h>
#include <stdio.h>
#include "../src/uss_native_menu_entries.h"
static unsigned reads,fail_at,corrupt_at,contexts,change_context;
static uint64_t scope(void* user){(void)user;++contexts;return change_context && contexts>1?2:1;}
static int read_entry(void* user,uintptr_t address,unsigned char* out,size_t size)
{
    unsigned i;(void)user;++reads;if(reads==fail_at)return 0;
    for(i=0;i<USS_MENU_ENTRY_COUNT;++i)if(address==0x10000+uss_menu_entry_pins[i].rva){
        assert(size==uss_menu_entry_pins[i].length);memcpy(out,uss_menu_entry_pins[i].bytes,size);
        if(reads==corrupt_at)out[0]^=1;return 1;}
    return 0;
}
int main(void)
{
    UssMenuEntryReader io={NULL,scope,read_entry};UssNativeMenuEntries entries,previous;unsigned i;
    memset(&entries,0,sizeof(entries));assert(uss_native_menu_resolve(&io,0x10000,0x4000000,&entries));
    assert(reads==USS_MENU_ENTRY_COUNT && entries.row_init && entries.row_release && entries.options_visual &&
        entries.assignment_display_init && entries.assignment_text && entries.assignment_layout && entries.assignment_highlight);
    previous=entries;
    for(i=1;i<=USS_MENU_ENTRY_COUNT;++i){
        reads=contexts=0;fail_at=i;assert(!uss_native_menu_resolve(&io,0x10000,0x4000000,&entries));
        assert(reads==i && !memcmp(&entries,&previous,sizeof(entries)));
        reads=contexts=fail_at=0;corrupt_at=i;assert(!uss_native_menu_resolve(&io,0x10000,0x4000000,&entries));
        assert(reads==i && !memcmp(&entries,&previous,sizeof(entries)));corrupt_at=0;
    }
    reads=contexts=0;change_context=1;assert(!uss_native_menu_resolve(&io,0x10000,0x4000000,&entries));
    assert(!memcmp(&entries,&previous,sizeof(entries)));change_context=0;
    assert(!uss_native_menu_resolve(&io,UINTPTR_MAX-1,0x4000000,&entries));
    assert(!uss_native_menu_resolve(&io,0x10000,16,&entries));
    puts("USS native menu entries: all retained menu/display pins, every read/corruption fault, scope change, bounds and commit-only table PASS");return 0;
}
