#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../src/uss_native_menu_observation.h"
static unsigned char module[0x3480000],ui[0x4400],menu[256],screen[5000000];
static unsigned reads,fail_at,change_scope,scopes;
static unsigned char* retained_display;static size_t retained_display_size;
static int language(void* user){(void)user;return 0;}
static uint64_t scope(void* user){(void)user;return ++scopes>1 && change_scope?2:1;}
static int read_memory(void* user,uintptr_t at,void* out,size_t size)
{
    const struct {uintptr_t at;unsigned char* data;size_t size;} ranges[]={{0x10000000,module,sizeof(module)},{0x20000000,ui,sizeof(ui)},{0x21000000,menu,sizeof(menu)},{0x22000000,screen,sizeof(screen)}};
    unsigned i;(void)user;if(++reads==fail_at)return 0;
    for(i=0;i<4;++i)if(at>=ranges[i].at && at-ranges[i].at<=ranges[i].size && size<=ranges[i].size-(at-ranges[i].at)){
        memcpy(out,ranges[i].data+(at-ranges[i].at),size);return 1;}
    if(retained_display && at>=(uintptr_t)retained_display && at-(uintptr_t)retained_display<=retained_display_size &&
        size<=retained_display_size-(at-(uintptr_t)retained_display)){
        memcpy(out,(const void*)at,size);return 1;}
    return 0;
}
static void put_pointer(unsigned char* at,uintptr_t value){memcpy(at,&value,8);}
static void fixture(void)
{
    static const uint32_t labels[9]={0x396f27d7,0x196248d0,0xa522d2d0,0x15cecac1,0x9a688238,0x07e3b100,0x5db3b764,0xa7c345ef,0x9bb47b92};unsigned i;
    memset(module,0,sizeof(module));memset(ui,0,sizeof(ui));memset(menu,0,sizeof(menu));memset(screen,0,sizeof(screen));
    put_pointer(module+0x347ce28,0x20000000);put_pointer(module+0x347ce38,0x21000000);put_pointer(module+0x347cf18,0x23000000);put_pointer(menu+200,0x22000000);
    uss_menu_put_word(ui,0x429c,1);uss_menu_put_word(ui,0x429c+20,1);uss_menu_put_word(screen,1248+57448,3);
    for(i=0;i<9;++i)uss_menu_put_word(screen,3010456+816+14920*i+1928+272,labels[i]);
    {static const uint32_t tabs[3]={0xd876b36e,0x78934e12,0x8c02bd80};
        memcpy(screen+1248+57320,tabs,sizeof(tabs));}
    reads=fail_at=change_scope=scopes=0;retained_display=NULL;retained_display_size=0;
}
static void text_identity(size_t widget,uintptr_t text)
{
    uss_menu_put_word(screen,widget+272,0xc67c7faf);screen[widget+616]=1;
    uss_menu_put_word(screen,widget+280,0xab2a7b35);uss_menu_put_word(screen,widget+284,1);put_pointer(screen+widget+288,text);
}
#ifndef USS_MENU_OBSERVATION_TEST_MAIN
#define USS_MENU_OBSERVATION_TEST_MAIN main
#endif
int USS_MENU_OBSERVATION_TEST_MAIN(void)
{
    UssMenuObservationIo io={NULL,0x10000000,sizeof(module),scope,language,read_memory};UssMenuHostSnapshot snapshot={0},before;
    UssMenuReadBudget budget;unsigned baseline,i;
    fixture();assert(uss_menu_observe_host(&io,NULL,&snapshot,&budget));
    assert(snapshot.valid && snapshot.escape_present && snapshot.topmost && snapshot.vanilla_intact && !snapshot.foreign_owner);
    assert(snapshot.container==0x22000000+3010456+1150848+544 && budget.calls<=256 && budget.bytes<=8192);
    baseline=reads;before=snapshot;
    for(i=1;i<=baseline;++i){fixture();fail_at=i;assert(!uss_menu_observe_host(&io,NULL,&snapshot,&budget));assert(!memcmp(&snapshot,&before,sizeof(before)));}
    fixture();uss_menu_put_word(screen,1248+57320,123);
    assert(uss_menu_observe_host(&io,NULL,&snapshot,&budget) && !snapshot.foreign_owner);
    fixture();change_scope=1;assert(!uss_menu_observe_host(&io,NULL,&snapshot,&budget));
    fixture();uss_menu_put_word(screen,1248+57448,4);assert(uss_menu_observe_host(&io,NULL,&snapshot,&budget) && !snapshot.foreign_owner && snapshot.vanilla_intact);
    fixture();uss_menu_put_word(ui,0x429c+20,0);assert(uss_menu_observe_host(&io,NULL,&snapshot,&budget) && !snapshot.escape_present && reads==2);
    fixture();uss_menu_put_word(ui,0x429c+20,6);assert(!uss_menu_observe_host(&io,NULL,&snapshot,&budget));
    {
        UssMenuExpectedRows owned={0};size_t content=3010456,row=content+144000;
        fixture();owned.count=0;owned.tab_text=0x24000000;owned.category_text=0x24001000;owned.row_text[0]=0x24002000;
        owned.choices[0]=2;owned.first_choice[0]=100;
        uss_menu_put_word(screen,1248+57448,4);text_identity(1248+8296+3400*3,owned.tab_text);
        assert(uss_menu_observe_host(&io,&owned,&snapshot,&budget) && snapshot.vanilla_intact && !snapshot.foreign_owner);
        owned.count=1;text_identity(content+816+1928,owned.category_text);text_identity(row+3992,owned.row_text[0]);
        put_pointer(screen+content+1150848+2816,0x22000000+row);uss_menu_put_word(screen,content+1150848+2832,1);
        uss_menu_put_word(screen,row+31428,139);uss_menu_put_word(screen,row+29072,2);uss_menu_put_word(screen,row+29080,100);
        assert(uss_menu_observe_host(&io,&owned,&snapshot,&budget) && snapshot.ours_intact && !snapshot.foreign_owner);
        uss_menu_put_word(screen,row+29080,101);assert(uss_menu_observe_host(&io,&owned,&snapshot,&budget) && !snapshot.ours_intact);
        assert(!strcmp(budget.checkpoint,"ROW_FIRST_CHOICE_CHANGED") && budget.index==0 && budget.actual==101 && budget.expected==100);
        uss_menu_put_word(screen,content+1150848+2832,0);
        assert(uss_menu_observe_host(&io,&owned,&snapshot,&budget) && !snapshot.ours_intact);
        assert(!strcmp(budget.checkpoint,"ROWS_COUNT_CHANGED") && budget.actual==0 && budget.expected==1);
        uss_menu_put_word(screen,content+1150848+2832,1);uss_menu_put_word(screen,row+29080,100);
        uss_menu_put_word(screen,row+31428,140);
        assert(uss_menu_observe_host(&io,&owned,&snapshot,&budget) && !snapshot.ours_intact);
        assert(!strcmp(budget.checkpoint,"ROW_SETTING_CHANGED") && budget.actual==140 && budget.expected==139);
        uss_menu_put_word(screen,row+31428,139);text_identity(row+3992,0x24009900);
        assert(uss_menu_observe_host(&io,&owned,&snapshot,&budget) && !snapshot.ours_intact);
        assert(!strcmp(budget.checkpoint,"ROW_TITLE_CHANGED"));
        io.language_state=NULL;assert(uss_menu_observe_host(&io,&owned,&snapshot,&budget) && snapshot.language_changing);
        io.language_state=language;
    }
    {UssMenuExpectedRows own={0};
     fixture();own.tab_text=0x24003000;own.require_tab=1;
     uss_menu_put_word(screen,1248+57448,7);
     uss_menu_put_word(screen,1248+57452,6);
     text_identity(1248+8296+3400*6,own.tab_text);
     uss_menu_put_word(screen,3010456+816+1928+272,12345);
     assert(uss_menu_observe_host(&io,&own,&snapshot,&budget) && !snapshot.foreign_owner && snapshot.uss_tab==6);
     /* Relocate the retained USS title while preserving independent other tabs. */
     memset(screen+1248+8296+3400*6,0,620);
     text_identity(1248+8296+3400*4,own.tab_text);
     assert(uss_menu_observe_host(&io,&own,&snapshot,&budget) && snapshot.uss_tab==4 && !snapshot.foreign_owner);
     text_identity(1248+8296+3400*5,own.tab_text);
     assert(uss_menu_observe_host(&io,&own,&snapshot,&budget) && snapshot.foreign_owner);
     memset(screen+1248+8296+3400*4,0,620);memset(screen+1248+8296+3400*5,0,620);
     assert(uss_menu_observe_host(&io,&own,&snapshot,&budget) && snapshot.foreign_owner);
    }
    puts("USS native host observation: exact bounded graph, all read faults, foreign menu, closed fast path and scope changes PASS");return 0;
}
