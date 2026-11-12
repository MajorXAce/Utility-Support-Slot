#include "../src/uss_native_menu_lifetime.h"
#define USS_MENU_LIFECYCLE_TEST_MAIN lifecycle_tests
#include "uss_native_menu_lifecycle_harness.c"
static UssMenuLifetime first,second;
int main(void){
    unsigned char inert[216]={0};const char* retained;unsigned before;
    assert(lifecycle_tests()==0);setup_lifecycle();uss_menu_put_word(inert,0,139);before=effects;lifecycle.io.native=owner.entries;
    assert(uss_menu_lifetime_prepare(&first,&owner.observer,&owner.entry_reader,&lifecycle.io,&fixture_settings,inert,sizeof(inert),L"build/lifetime-settings.json"));
    assert(effects==before && first.owner.rows==&first.rows && first.lifecycle.owner==&first.owner && first.frame.lifecycle==&first.lifecycle);
    retained=first.rows.rows[0].title;assert(!strcmp(retained,"Binding mode"));
    assert(!uss_menu_lifetime_prepare(&first,&owner.observer,&owner.entry_reader,&lifecycle.io,&fixture_settings,inert,sizeof(inert),L"build/lifetime-settings.json"));
    assert(!uss_menu_lifetime_retire(&first));
    uss_menu_put_word(ui,0x429c+20,0); /* Positive closed Escape stack. */
    assert(uss_menu_lifetime_retire(&first) && first.retired && !strcmp(retained,"Binding mode"));
    assert(!uss_menu_lifetime_retire(&first));
    setup_lifecycle();lifecycle.io.native=owner.entries;
    assert(uss_menu_lifetime_prepare(&second,&owner.observer,&owner.entry_reader,&lifecycle.io,&fixture_settings,inert,sizeof(inert),L"build/lifetime-settings.json"));
    assert(second.rows.rows[0].title!=retained && !strcmp(retained,"Binding mode"));
    puts("USS screen lifetime: double-observed vanilla bootstrap, no construction effects, positive-close retirement and retained distinct storage PASS");return 0;
}
