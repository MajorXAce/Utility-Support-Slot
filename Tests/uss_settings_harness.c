#include <assert.h>
#include "../src/uss_settings.h"
int main(void)
{
    UssSettings saved=uss_settings_defaults(),decoded=saved,changed=saved;UssSettingsState state;
    UssMenuConnection menu={0};char json[512];int n=uss_settings_encode(&saved,json,sizeof(json));
    assert(n>0 && uss_settings_decode(json,(size_t)n,&decoded) && uss_settings_equal(&saved,&decoded));
    changed.schema=2;assert(!uss_settings_valid(&changed));changed=saved;changed.utility_key=0x77;assert(!uss_settings_valid(&changed));
    changed=saved;changed.menu_key=changed.utility_key;assert(!uss_settings_valid(&changed));
    changed=saved;changed.utility_controller=3;assert(!uss_settings_valid(&changed));
    changed.utility_controller=0x400;assert(!uss_settings_valid(&changed));
    changed.utility_controller=USS_BIND_LEFT_TRIGGER;assert(uss_settings_valid(&changed));
    assert(!uss_settings_decode("{}",2,&decoded));assert(uss_settings_equal(&decoded,&saved));
    {const char* bad="{\"schema\":1,\"schema\":1,\"mode\":1,\"utility_key\":54,\"utility_controller\":0,\"menu_key\":118}";
        assert(!uss_settings_decode(bad,strlen(bad),&decoded));}
    assert(!uss_settings_decode(json,(size_t)n-2,&decoded));
    /* Prefix truncation, embedded NUL and trailing garbage never commit a value. */
    {int i;for(i=0;i<n-2;++i)assert(!uss_settings_decode(json,(size_t)i,&decoded));}
    json[n]='x';assert(!uss_settings_decode(json,(size_t)n+1,&decoded));json[n]='\0';
    uss_settings_init(&state,&saved);changed=saved;changed.mode=USS_BIND_DEDICATED;
    assert(uss_settings_queue(&state,&changed));assert(!uss_settings_commit(&state,1));
    assert(state.current.mode==USS_BIND_DEFAULT && state.pending_present);
    assert(uss_settings_commit(&state,0) && state.current.mode==USS_BIND_DEDICATED && state.revision==2);
    assert(!uss_settings_input_ready(&state,1,0,0,0,1));assert(!uss_settings_input_ready(&state,1,0,0,0,0));
    assert(uss_settings_input_ready(&state,1,0,0,0,1));
    assert(!uss_settings_input_ready(&state,1,1,0,0,0));assert(!uss_settings_input_ready(&state,1,0,0,0,1));
    assert(!uss_settings_input_ready(&state,1,0,0,0,0));assert(uss_settings_input_ready(&state,1,0,0,0,1));
    menu.provider=7;menu.applied=saved;menu.options_registered=menu.subscribed=menu.values_valid=1;
    assert(!uss_settings_connect(&state,&menu) && !state.external); /* binding registration missing */
    menu.bindings_registered=1;assert(uss_settings_connect(&state,&menu));assert(uss_settings_commit(&state,0));
    changed=saved;changed.utility_key=0x37;
    assert(!uss_settings_external_change(&state,8,&changed));assert(uss_settings_external_change(&state,7,&changed));
    assert(!uss_settings_commit(&state,1));assert(uss_settings_commit(&state,0));
    assert(!uss_settings_connect(&state,NULL) && !state.external && state.current.utility_key==0x37);
    assert(!uss_settings_external_change(&state,7,&saved));assert(uss_settings_connect(&state,&menu));
    puts("USS settings: bounded JSON, deferred commits, partial/replaced menus, stale callbacks and release gating PASS");return 0;
}
