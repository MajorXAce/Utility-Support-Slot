#include <assert.h>
#include <stdio.h>
#include "../src/normal_gameplay.c"
static unsigned logs,module_calls;
static void own_log(HfMod* mod,const char* format,...){assert(mod==(HfMod*)1);assert(format);++logs;}
static const char* own_build(void){assert(0);return NULL;}
static uintptr_t own_module(const char* name,size_t* size){assert(!strcmp(name,"game.dll"));++module_calls;*size=0;return 0;}
static unsigned provider_ready=1,provider_options,provider_bindings;
static double provider_mode=2;
static int provider_is_ready(void){return (int)provider_ready;}
static int provider_option(const char* id,const UssVpmOption* option,const char** error)
{(void)error;assert(!strcmp(id,USS_VPM_MODE_ID));assert(option->default_value==1);++provider_options;return 1;}
static int provider_binding(const char* id,const UssVpmBinding* binding,const char** error)
{(void)error;assert(!strcmp(id,USS_VPM_BINDING_ID) && !binding->slot);++provider_bindings;return 1;}
static int provider_get(const char* id,double* value){assert(!strcmp(id,USS_VPM_MODE_ID));*value=provider_mode;return 1;}
static int provider_down(const char* id){assert(!strcmp(id,USS_VPM_BINDING_ID));return 0;}
static void external_integration(HfApi* api)
{
    UssNormalSession state={0};UssMenuSessionNode open_screen={0};uint64_t revision;
    UssVpmOptionsApi options={1,56,112,64,provider_option,provider_get,NULL,NULL,provider_is_ready};
    UssVpmBindingsApi bindings={1,56,388,0,provider_binding,provider_down,NULL,NULL,provider_is_ready};
    state.app.native.api=api;state.app.native.mod=(HfMod*)1;
    uss_settings_init(&state.app.settings,NULL);
    assert(uss_vpm_menus_bind(&state.external_menus.menus,&options,&bindings,1,1));
    state.menus.current=&open_screen;
    normal_external_menus(&state,&state.app.settings,0,1);
    assert(!state.app.settings.external && state.app.settings.current.mode==1); /* live widgets retained */
    state.menus.current=NULL; /* own fake closed-screen retirement boundary */
    normal_external_menus(&state,&state.app.settings,1,2);assert(!state.app.settings.external);
    normal_external_menus(&state,&state.app.settings,0,3);
    assert(state.app.settings.external && state.app.settings.current.mode==2);
    assert(state.standalone_settings.mode==1 && state.menus.current==NULL);
    revision=state.app.settings.revision;
    normal_external_menus(&state,&state.app.settings,0,4);assert(state.app.settings.revision==revision);
    provider_mode=1;normal_external_menus(&state,&state.app.settings,1,5);assert(state.app.settings.current.mode==2);
    normal_external_menus(&state,&state.app.settings,0,6);assert(state.app.settings.current.mode==1);
    provider_ready=0;normal_external_menus(&state,&state.app.settings,1,7);assert(state.app.settings.external && !state.external_down);
    normal_external_menus(&state,&state.app.settings,0,8);
    assert(!state.app.settings.external && !state.app.settings.provider && state.app.settings.current.mode==1);
    provider_ready=1;normal_external_menus(&state,&state.app.settings,0,9);assert(state.app.settings.external);
    assert(provider_options==1 && provider_bindings==1 && state.standalone_settings.mode==1);
}
int main(void){HfApi api={0};api.version=12;api.size=sizeof(api);api.log=own_log;api.build_id=own_build;
 assert(hellforge_mod_init(&api,(HfMod*)1)==0 && logs==1 && !normal.app.attempted && !normal.menus.retained);
 assert(hellforge_mod_init(&api,(HfMod*)1)==1 && logs==1);
 api.module_base=own_module;normal.app.native.api=&api;normal.app.native.mod=(HfMod*)1;
 uss_settings_init(&normal.app.settings,NULL);logs=0;
 {unsigned i;for(i=0;i<20;++i){normal.next_menu_check=0;normal_menu_frame(&normal,0.016f,&normal.app.settings,0);}}
 assert(logs==2 && normal.bootstrap_reports==1 && module_calls==20);
 assert(!strcmp(normal.bootstrap_checkpoint,"ARGUMENTS") && normal.bootstrap_status==1 && !normal.menus.retained);
 /* Reports software transitions once, never reads code or performs effects.
  * A new pause is still visible after the progress-log quota is exhausted. */
 normal.app.initialized=1;logs=0;normal_gameplay_report(&normal);assert(logs==1);
 normal_gameplay_report(&normal);assert(logs==1 && module_calls==20);
 normal.app.recovery.reason="RECOVERY_STARTED";normal_gameplay_report(&normal);assert(logs==2);
 normal.app.recovery.work.phase=USS_RECOVERY_ATTACH;normal_gameplay_report(&normal);assert(logs==3);
 normal_gameplay_report(&normal);assert(logs==3 && module_calls==20);
 normal.app.loop.refill.pickups.reason="PICKUP_ACTION_UNAVAILABLE";normal_gameplay_report(&normal);assert(logs==4);
 ++normal.app.loop.refill.transaction.calls;normal_gameplay_report(&normal);assert(logs==5);
 normal_gameplay_report(&normal);assert(logs==5 && module_calls==20);logs=1;
 normal.gameplay_reports=128;normal.app.player.reason="INPUT_CONTEXT_UNAVAILABLE";
 normal_gameplay_report(&normal);assert(logs==1);
 normal.app.loop.handling_paused=1;normal.app.automatic.reason="PLAYER_INTERRUPTED_WORK";
 normal_gameplay_report(&normal);assert(logs==2);normal_gameplay_report(&normal);assert(logs==2);
 normal.app.loop.handling_paused=0;normal_gameplay_report(&normal);assert(logs==2);
 normal.app.loop.handling_paused=1;normal_gameplay_report(&normal);assert(logs==3 && module_calls==20);
 external_integration(&api);
 puts("USS normal entry: inactive default performs no build/game/input/file/registration work; repeated init refused PASS");return 0;}
