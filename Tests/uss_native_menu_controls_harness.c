#include "../src/uss_native_menu_controls.h"
#define USS_MENU_LIFECYCLE_TEST_MAIN lifecycle_tests
#include "uss_native_menu_lifecycle_harness.c"
static unsigned control_consumes,control_saves;
static int control_consume(void* user){(void)user;++control_consumes;return 1;}
static int control_save(void* user,const UssSettings* value){(void)user;assert(value->utility_key==75);++control_saves;return 1;}
int main(void)
{
    unsigned char inert[216]={0};unsigned values[3]={1,1,0};
    UssMenuSettingsController controller={0};UssDedicatedCaptureSample sample={0};
    assert(lifecycle_tests()==0);setup_lifecycle();
    uss_menu_put_word(inert,0,139);
    assert(uss_menu_controls_prepare(&own_rows,inert,sizeof(inert),&fixture_settings));
    assert(uss_menu_lifecycle_open(&lifecycle));
    controller.consume=control_consume;controller.save=control_save;
    sample.host=owner.identity.screen;sample.epoch=2;sample.now_ms=100;sample.valid=sample.foreground=1;sample.page=USS_DEDICATED_ANY;sample.keys[1]=1;
    assert(uss_menu_controls_update(&own_rows,&lifecycle.io,&controller,&fixture_settings,values,&sample));
    assert(own_rows.rows[1].count==0 && controller.capture.phase==USS_DEDICATED_ARMING);
    values[1]=0;sample.keys[1]=0;
    assert(uss_menu_controls_update(&own_rows,&lifecycle.io,&controller,&fixture_settings,values,&sample));
    sample.keys[75]=1;assert(uss_menu_controls_update(&own_rows,&lifecycle.io,&controller,&fixture_settings,values,&sample));
    sample.keys[75]=0;assert(uss_menu_controls_update(&own_rows,&lifecycle.io,&controller,&fixture_settings,values,&sample));
    assert(uss_menu_settings_apply(&controller,&fixture_settings,0)==1 && control_saves==1 && control_consumes==4);
    assert(fixture_settings.current.mode==USS_BIND_DEDICATED && fixture_settings.current.utility_key==75);
    values[1]=1;sample.page=USS_DEDICATED_KEYBOARD;
    assert(!uss_menu_controls_update(&own_rows,&lifecycle.io,&controller,&fixture_settings,values,&sample));
    assert(uss_menu_controls_present(&own_rows,&lifecycle.io,&controller,&fixture_settings));
    assert(!strcmp(own_rows.assignment.caption,"K"));
    /* A controller assignment replaces the keyboard and is presented before idle save. */
    sample.page=USS_DEDICATED_ANY;sample.controller=0;
    assert(uss_menu_controls_update(&own_rows,&lifecycle.io,&controller,&fixture_settings,values,&sample));
    values[1]=0;
    assert(uss_menu_controls_update(&own_rows,&lifecycle.io,&controller,&fixture_settings,values,&sample));
    sample.controller=4096;
    assert(uss_menu_controls_update(&own_rows,&lifecycle.io,&controller,&fixture_settings,values,&sample));
    sample.controller=0;
    assert(uss_menu_controls_update(&own_rows,&lifecycle.io,&controller,&fixture_settings,values,&sample));
    assert(fixture_settings.pending_present && !fixture_settings.pending.utility_key && fixture_settings.pending.utility_controller==4096);
    assert(uss_menu_controls_present(&own_rows,&lifecycle.io,&controller,&fixture_settings));
    assert(!strcmp(own_rows.assignment.caption,"Controller A"));
    assert(uss_menu_lifecycle_close(&lifecycle));
    assert(!own_rows.assignment.entered); /* Positive native teardown permits a fresh view. */
    assert(uss_menu_lifecycle_open(&lifecycle) && own_rows.assignment.attached);
    puts("USS native controls: real row construction/reset, activator release, capture, mode and saved idle apply PASS");return 0;
}
