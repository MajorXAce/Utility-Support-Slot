#include <assert.h>
#include <stdio.h>
#include "../src/uss_menu_settings_controller.h"
static unsigned consumed,saves;static int consume_ok=1,save_ok=1;static UssSettings saved;
static int consume(void* user){(void)user;++consumed;return consume_ok;}
static int save(void* user,const UssSettings* settings){(void)user;++saves;saved=*settings;return save_ok;}
int main(void){
    UssSettingsState settings;UssMenuSettingsController menu={0};UssDedicatedCaptureSample sample={0};
    uss_settings_init(&settings,NULL);menu.consume=consume;menu.save=save;
    sample.host=1;sample.epoch=2;sample.now_ms=100;sample.valid=sample.foreground=1;sample.page=1;
    assert(uss_menu_settings_mode(&menu,&settings,USS_BIND_DEDICATED));
    sample.keys[1]=1;assert(uss_menu_settings_begin(&menu,&settings,&sample));
    assert(!uss_menu_settings_apply(&menu,&settings,0) && !saves);
    assert(!uss_menu_settings_capture(&menu,&settings,&sample));sample.keys[1]=0;
    assert(!uss_menu_settings_capture(&menu,&settings,&sample));sample.keys[75]=1;
    assert(!uss_menu_settings_capture(&menu,&settings,&sample));sample.keys[75]=0;
    assert(uss_menu_settings_capture(&menu,&settings,&sample)==USS_DEDICATED_QUEUED);
    assert(!uss_menu_settings_apply(&menu,&settings,1) && !saves);
    assert(uss_menu_settings_apply(&menu,&settings,0)==1 && saves==1 && consumed==5);
    assert(saved.utility_key==75 && saved.mode==USS_BIND_DEDICATED && settings.release_required);
    assert(!uss_menu_settings_apply(&menu,&settings,0) && saves==1);
    assert(uss_menu_settings_mode(&menu,&settings,USS_BIND_DEFAULT));save_ok=0;
    assert(uss_menu_settings_apply(&menu,&settings,0)==2 && menu.save_failed && !menu.stopped && saves==2);
    assert(settings.current.mode==USS_BIND_DEFAULT && !uss_menu_settings_apply(&menu,&settings,0) && saves==2);
    /* An explicit second assignment still works after failure; idle never retries. */
    sample.page=USS_DEDICATED_ANY;sample.keys[75]=0;
    assert(uss_menu_settings_begin(&menu,&settings,&sample));
    assert(uss_menu_settings_capture(&menu,&settings,&sample)==USS_DEDICATED_WAIT);
    sample.keys[76]=1;assert(uss_menu_settings_capture(&menu,&settings,&sample)==USS_DEDICATED_WAIT);
    sample.keys[76]=0;assert(uss_menu_settings_capture(&menu,&settings,&sample)==USS_DEDICATED_QUEUED);
    save_ok=1;assert(uss_menu_settings_apply(&menu,&settings,0)==1 && !menu.save_failed && saves==3);
    assert(settings.current.utility_key==76 && menu.save_attempts==3);
    settings.external=1;
    assert(!uss_menu_settings_begin(&menu,&settings,&sample) && !uss_menu_settings_mode(&menu,&settings,2));
    settings.external=0;consume_ok=0;
    assert(!uss_menu_settings_begin(&menu,&settings,&sample) && menu.stopped && menu.capture.phase==USS_DEDICATED_IDLE);
    puts("USS menu settings: capture consumption, pending mode preservation, idle apply, one save, failed-save retention and provider refusal PASS");return 0;
}
