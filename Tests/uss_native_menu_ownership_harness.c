#include "../src/uss_native_menu_ownership.h"
#define USS_MENU_OBSERVATION_TEST_MAIN observation_tests
#include "uss_native_menu_observation_harness.c"
static UssMenuChrome chrome;
static UssMenuRows own_rows;
static UssMenuOwnership owner;
static unsigned bad_pin;
static int code_read(void* user,uintptr_t at,unsigned char* out,size_t length)
{
    unsigned i;(void)user;if(bad_pin)return 0;
    for(i=0;i<USS_MENU_ENTRY_COUNT;++i)if(at==0x10000000+uss_menu_entry_pins[i].rva && length==uss_menu_entry_pins[i].length){
        memcpy(out,uss_menu_entry_pins[i].bytes,length);return 1;}
    return 0;
}
static void start(void)
{
    UssMenuHostSnapshot initial={0};
    UssMenuObservationIo io={NULL,0x10000000,sizeof(module),scope,language,read_memory};
    bad_pin=0;fixture();memset(&chrome,0,sizeof(chrome));memset(&own_rows,0,sizeof(own_rows));memset(&owner,0,sizeof(owner));
    assert(uss_menu_observe_host(&io,NULL,&initial,NULL));
    assert(uss_menu_chrome_prepare(&chrome,&initial,"USS","Utility Support Slot"));
    owner.observer=io;owner.identity=initial;owner.chrome=&chrome;owner.rows=&own_rows;owner.stage=USS_MENU_PROOF_VANILLA;
    owner.entry_reader.user=NULL;owner.entry_reader.context=scope;owner.entry_reader.read_executable=code_read;
    assert(uss_native_menu_resolve(&owner.entry_reader,io.base,io.module_size,&owner.entries));
}
#ifndef USS_MENU_OWNERSHIP_TEST_MAIN
#define USS_MENU_OWNERSHIP_TEST_MAIN main
#endif
int USS_MENU_OWNERSHIP_TEST_MAIN(void)
{
    UssMenuHostSnapshot current;size_t content=3010456,tab=1248+8296+3400*3,row=content+144000;
    assert(observation_tests()==0);start();assert(uss_menu_ownership_guard(&owner,&owner.identity));
    assert(!uss_menu_ownership_stage(&owner,USS_MENU_PROOF_OWNED));
    assert(uss_menu_ownership_stage(&owner,USS_MENU_PROOF_TAB));
    uss_menu_put_word(screen,1248+57448,4);uss_menu_put_word(screen,tab+272,0xc67c7faf);screen[tab+616]=0;
    assert(uss_menu_ownership_observe(&owner,&current)); /* Only our nominated empty template. */
    text_identity(tab,(uintptr_t)chrome.tab_title);chrome.tab_ready=1;
    assert(uss_menu_ownership_stage(&owner,USS_MENU_PROOF_CATEGORY));
    uss_menu_put_word(screen,content+816+1928+272,0xc67c7faf);screen[content+816+1928+616]=0;
    assert(uss_menu_ownership_observe(&owner,&current));
    text_identity(content+816+1928,(uintptr_t)chrome.category_title);chrome.categories_owned=1;
    assert(uss_menu_ownership_stage(&owner,USS_MENU_PROOF_ROWS));assert(uss_menu_ownership_observe(&owner,&current));
    own_rows.count=1;own_rows.rows[0].count=2;own_rows.built=1;
    put_pointer(screen+content+1150848+2816,0x22000000+row);uss_menu_put_word(screen,content+1150848+2832,1);
    text_identity(row+3992,(uintptr_t)own_rows.rows[0].title);
    uss_menu_put_word(screen,row+31428,139);uss_menu_put_word(screen,row+29072,2);uss_menu_put_word(screen,row+29080,0xc67c7faf);
    assert(uss_menu_ownership_stage(&owner,USS_MENU_PROOF_OWNED));assert(uss_menu_ownership_observe(&owner,&current) && current.ours_intact);
    assert(!uss_menu_ownership_stage(&owner,USS_MENU_PROOF_ROWS));
    assert(uss_menu_ownership_stage(&owner,USS_MENU_PROOF_RESTORE));
    screen[content+816+1928+616]=0;assert(uss_menu_ownership_observe(&owner,&current));
    uss_menu_put_word(screen,content+816+1928+272,uss_menu_vanilla_categories[0]);chrome.categories_owned=0;
    assert(uss_menu_ownership_stage(&owner,USS_MENU_PROOF_VANILLA));assert(uss_menu_ownership_observe(&owner,&current) && current.vanilla_intact);
    assert(uss_menu_ownership_stage(&owner,USS_MENU_PROOF_CATEGORY)); /* Reopen retained tab. */
    text_identity(content+816+1928,0xdead0000);
    assert(uss_menu_ownership_observe(&owner,&current)); /* Unrelated category title is permitted. */
    text_identity(tab,0xdead0000);assert(!uss_menu_ownership_observe(&owner,&current) && owner.stopped);
    start();assert(uss_menu_ownership_stage(&owner,USS_MENU_PROOF_TAB));
    uss_menu_put_word(screen,1248+57448,4);text_identity(tab,0xdead0000);
    assert(uss_menu_ownership_observe(&owner,&current)); /* Foreign extra tab is not USS ownership. */
    start();uss_menu_put_word(screen,content+816+14920+1928+272,123);
    assert(uss_menu_ownership_observe(&owner,&current));
    start();bad_pin=1;assert(!uss_menu_ownership_observe(&owner,&current));
    start();change_scope=1;assert(!uss_menu_ownership_observe(&owner,&current));
    puts("USS menu ownership: bounded real graph, exact partial text states, retained-tab lifecycle, foreign widgets and no recovery from lost proof PASS");return 0;
}
