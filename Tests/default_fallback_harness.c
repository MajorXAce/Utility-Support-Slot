/* Actual normal callback/settings/handover with authored memory, no file IO. */
#define main existing_normal_fixture_not_run
#include "normal_menu_presence_harness.c"
#undef main
#include <stdarg.h>
static unsigned fallback_checks,fallback_logs,sample_start,sample_reason,sample_recovery;
static char last_fallback[1024];
#define VERIFY(x) do{++fallback_checks;if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static void capture_log(HfMod* mod,const char* format,...)
{char line[1024];va_list args;(void)mod;va_start(args,format);vsnprintf(line,sizeof line,format,args);va_end(args);
 if(strstr(line,"USS Default gameplay fallback")){++fallback_logs;memcpy(last_fallback,line,strlen(line)+1);assert(!strstr(line,"CAPACITY") && !strstr(line,"0x") && !strstr(line,":\\"));
 if(!sample_start && strstr(line,"fallback start ")){sample_start=1;printf("SAMPLE_START %s\n",line);}
 if(!sample_reason && strstr(line,"fallback reason-change ")){sample_reason=1;printf("SAMPLE_REASON %s\n",line);}
 if(!sample_recovery && strstr(line,"fallback recovery ")){sample_recovery=1;printf("SAMPLE_RECOVERY %s\n",line);}
}}
static void saved_setup(unsigned mode)
{UssSettings saved=uss_settings_defaults();prepare();saved.mode=mode;saved.utility_key=0x4b;saved.utility_controller=0x1000;
 uss_settings_init(&normal.app.settings,&saved);fake_api.log=capture_log;fallback_logs=0;last_fallback[0]=0;fallback_held=1;}
static void frame(unsigned busy){normal_menu_frame(&normal,0.016f,&normal.app.settings,busy);++fixture_now;}
static void refused(void){normal.menu_stopped=1;normal.menu_stop_reason="FRAME_REFUSED";}
static void raw_context(UssCoreObservation* o,UssPairOwner* owner,UssPlayerInputSample* raw)
{memset(o,0,sizeof *o);memset(owner,0,sizeof *owner);memset(raw,0,sizeof *raw);o->valid=o->mission=o->flags.known=o->flags.eligible=1;
 o->inventory.slots[0]=10;o->inventory.slots[1]=20;o->inventory.slots[2]=30;o->inventory.active_selector=3;o->inventory.held=30;
 owner->epoch=owner->context_generation=owner->pair_generation=owner->guard_generation=1;owner->pair_present=1;owner->utility.entity=40;owner->ordinary.entity=30;
 raw->known=raw->foreground=raw->ui_clear=1;raw->manager=77;}
static int c_ready=1,c_option_ok=1,c_binding_ok=1;
static int c_is_ready(void){return c_ready;}
static int c_option(const char* id,const UssVpmOption* spec,const char** error){(void)id;(void)spec;(void)error;return c_option_ok;}
static int c_binding(const char* id,const UssVpmBinding* spec,const char** error){(void)id;(void)spec;(void)error;return c_binding_ok;}
static int c_get(const char* id,double* value){(void)id;*value=2;return 1;}
static int c_down(const char* id){(void)id;return 0;}
static UssVpmOptionsApi c_options={1,56,112,64,c_option,c_get,NULL,NULL,c_is_ready};
static UssVpmBindingsApi c_bindings={1,56,388,0,c_binding,c_down,NULL,NULL,c_is_ready};
static void c_bind(void){assert(uss_vpm_menus_bind(&normal.external_menus.menus,&c_options,&c_bindings,9,2));}
static int input_cases(void)
{UssCoreObservation o;UssPairOwner owner;UssPlayerInputSample raw;UssPlayerInput input={0};UssPlayerGesture g;UssSettingsState* settings=&normal.app.settings;
 raw_context(&o,&owner,&raw);raw.support_pressed=1;g=uss_player_input_poll(&input,settings,&o,&owner,&raw,1,0);VERIFY(g.known && !g.hit && settings->mode_release_required);
 raw.support_pressed=0;(void)uss_player_input_poll(&input,settings,&o,&owner,&raw,2,0);(void)uss_player_input_poll(&input,settings,&o,&owner,&raw,3,0);
 raw.support_pressed=1;g=uss_player_input_poll(&input,settings,&o,&owner,&raw,4,0);VERIFY(g.known && g.hit && !g.dedicated);
 raw.support_pressed=0;(void)uss_player_input_poll(&input,settings,&o,&owner,&raw,5,0);raw.cycle_held=0x3f800000;
 g=uss_player_input_poll(&input,settings,&o,&owner,&raw,10,0);VERIFY(!g.hit);
 o.inventory.active_selector=1;o.inventory.held=10;g=uss_player_input_poll(&input,settings,&o,&owner,&raw,270,0);VERIFY(g.hit && !g.dedicated);
 raw.known=0;g=uss_player_input_poll(&input,settings,&o,&owner,&raw,280,0);VERIFY(!g.known && !g.hit);return 0;}
int main(void)
{UssSettings saved,draft;UssSettingsState before;char encoded[512],after[512];unsigned count,down;UssCoreObservation o;UssPairOwner owner;UssPlayerInputSample raw;UssPlayerInput input={0};UssPlayerGesture g;
 saved_setup(2);saved=normal.app.settings.current;draft=saved;draft.utility_key=0x4d;VERIFY(uss_settings_queue(&normal.app.settings,&draft));before=normal.app.settings;VERIFY(uss_settings_encode(&saved,encoded,sizeof encoded)>0);
 normal.diag.lines=48;normal.diag.capped=1;post(1,0,0,1);frame(0);
 VERIFY(normal.app.settings.default_fallback && uss_settings_effective_mode(&normal.app.settings)==USS_BIND_DEFAULT);
 VERIFY(normal.menu_ui.deferred && !normal.handover.usable && !normal.menus.current);
 VERIFY(uss_settings_equal(&normal.app.settings.current,&saved) && normal.app.settings.pending_present && uss_settings_equal(&normal.app.settings.pending,&before.pending));
 VERIFY(uss_settings_encode(&normal.app.settings.current,after,sizeof after)>0 && !strcmp(after,encoded));
 VERIFY(fallback_logs==1 && strstr(last_fallback,"start prior=2 configured=2 effective=1") && strstr(last_fallback,"PROVIDER_PRESENCE_DEFERRED"));
 VERIFY(normal_dedicated(&normal,&normal.app.settings.current,&down) && !down);VERIFY(!input_cases());
 count=fallback_logs;normal.app.player.pending.present=1;frame(0);VERIFY(fallback_logs==count && normal.app.player.pending.present);refused();frame(0);VERIFY(fallback_logs==count+1 && strstr(last_fallback,"reason-change") && strstr(last_fallback,"FRAME_REFUSED"));
 post(1,1,0,2);frame(0);VERIFY(!normal.app.settings.default_fallback && uss_settings_effective_mode(&normal.app.settings)==2 && normal.handover.usable);
 VERIFY(strstr(last_fallback,"recovery") && strstr(last_fallback,"effective=2") && normal.app.settings.pending_present);
 /* A closed/not-yet-open own menu is a valid route, without needing native UI construction. */
 saved_setup(2);uss_menu_put_word(stack_memory,0x429c+20,0);frame(0);VERIFY(!normal.app.settings.default_fallback && uss_settings_effective_mode(&normal.app.settings)==2 && !fallback_logs);
 /* Neither saved Default nor missing-save defaults become a dedicated fallback. */
 saved_setup(1);refused();frame(0);VERIFY(normal.app.settings.default_fallback && uss_settings_effective_mode(&normal.app.settings)==1 && normal.app.settings.current.mode==1);
 prepare();fake_api.log=capture_log;refused();frame(0);VERIFY(normal.app.settings.default_fallback && normal.app.settings.current.mode==1);
 /* Active transaction: suppress new commands, retain effective authority until safe boundary. */
 saved_setup(2);refused();normal.app.player.pending.present=1;frame(1);
 VERIFY(!normal.app.settings.default_fallback && normal.app.settings.mode_change_pending && !normal.app.player.pending.present && normal.app.settings.release_required);
 raw_context(&o,&owner,&raw);raw.dedicated_down=1;g=uss_player_input_poll(&input,&normal.app.settings,&o,&owner,&raw,1,USS_INPUT_WORK_QUEUE_DRAW);VERIFY(g.known && !g.hit && !g.queue_requested && !g.interrupted);
 frame(0);VERIFY(normal.app.settings.default_fallback && !normal.app.settings.mode_change_pending && normal.app.settings.current.mode==2);
 /* Restore own authority on positive safe availability, not merely external disappearance. */
 normal.menu_stopped=0;uss_menu_put_word(stack_memory,0x429c+20,0);frame(1);VERIFY(normal.app.settings.default_fallback && normal.app.settings.mode_change_pending);
 frame(0);VERIFY(!normal.app.settings.default_fallback && normal.app.settings.mode_release_required && normal.app.settings.current.mode==2);
 memset(&input,0,sizeof input);raw_context(&o,&owner,&raw);raw.dedicated_down=1;g=uss_player_input_poll(&input,&normal.app.settings,&o,&owner,&raw,2,0);VERIFY(!g.hit && normal.app.settings.mode_release_required);
 raw.dedicated_down=0;(void)uss_player_input_poll(&input,&normal.app.settings,&o,&owner,&raw,3,0);(void)uss_player_input_poll(&input,&normal.app.settings,&o,&owner,&raw,4,0);
 raw.dedicated_down=1;g=uss_player_input_poll(&input,&normal.app.settings,&o,&owner,&raw,5,0);VERIFY(g.hit && g.dedicated);
 /* Provider loss/withdrawal while a prior standalone lifetime remains unsafe. */
 saved_setup(2);refused();post(1,1,0,1);frame(0);VERIFY(!normal.app.settings.default_fallback && normal.handover.usable);
 post(1,0,0,2);frame(0);VERIFY(normal.app.settings.default_fallback && !normal.handover.usable);
 normal.menu_ui.close_required=1;frame(0);VERIFY(normal.app.settings.default_fallback); /* no unsafe UI takeover */
 /* Only bindings available + refused own route is no configuration route. */
 saved_setup(2);refused();{UssLuaMenuPost p={0};p.magic=USS_LUA_MENU_MAGIC;p.protocol=2;p.size=sizeof p;p.instance=p.sequence=p.generation=1;p.mode=2;p.bindings_registered=1;VERIFY(!uss_lua_mailbox_post(&normal.lua_mailbox,&p,sizeof p));}frame(0);VERIFY(normal.app.settings.default_fallback);
 /* Supported C readiness/partial registration remain separate from UI presence. */
 saved_setup(2);c_ready=0;c_option_ok=c_binding_ok=1;c_bind();frame(0);
 VERIFY(normal.app.settings.default_fallback && normal.menu_ui.c_options_present && !normal.handover.usable && strstr(last_fallback,"WAIT_READY"));
 c_ready=1;frame(0);VERIFY(!normal.app.settings.default_fallback && normal.handover.usable && uss_settings_effective_mode(&normal.app.settings)==2 && normal.menu_ui.deferred);
 saved_setup(2);c_option_ok=0;c_binding_ok=1;c_bind();frame(0);
 VERIFY(normal.app.settings.default_fallback && !normal.external_menus.menus.option_registered && normal.external_menus.menus.binding_registered && strstr(last_fallback,"REGISTRATION_REFUSED"));
 saved_setup(2);normal.external_menus.options_present=1;frame(0);VERIFY(normal.app.settings.default_fallback && normal.menu_ui.c_options_present);
 /* Menu/input suppression alone does not make healthy configuration unavailable. */
 saved_setup(2);post(1,1,0,1);frame(0);
 {UssLuaMenuPost p=normal.lua_mailbox.slot;p.sequence=2;p.action_blocked=1;VERIFY(!uss_lua_mailbox_post(&normal.lua_mailbox,&p,sizeof p));}frame(0);
 VERIFY(normal.handover.usable && !normal.app.settings.default_fallback && normal.app.settings.current.mode==2);
 /* Authority transition discards unread scoped observations, not only player commands. */
 saved_setup(2);refused();post(1,1,0,1);
 {uint64_t scope[6]={1,1,1,1,1,1};uss_lua_mailbox_scope(&normal.lua_mailbox,scope,1);post(1,1,1,2);post(1,1,0,3);VERIFY(normal.lua_mailbox.edge_count==2);}
 normal_mode_authority(&normal,&normal.app.settings,0);VERIFY(normal.app.settings.default_fallback && !normal.lua_mailbox.edge_count && !normal.lua_mailbox.edge_enabled);
 /* Reserved fallback budget: cap notification once, recovery metadata once. */
 {UssMenuDiagnostics d={0};UssMenuFallbackDiag e={0};unsigned i;d.lines=48;d.capped=1;fallback_logs=0;e.active=1;e.configured=2;e.effective=1;strcpy_s(e.standalone,sizeof e.standalone,"FRAME_REFUSED");strcpy_s(e.c_status,sizeof e.c_status,"REGISTRATION_REFUSED");
  for(i=0;i<40;i++){e.c_options=i%2;uss_menu_diag_fallback(&d,normal_diag_log,&normal,&e);}VERIFY(fallback_logs==17 && d.fallback_capped);
  e.active=0;e.effective=2;uss_menu_diag_fallback(&d,normal_diag_log,&normal,&e);VERIFY(fallback_logs==18 && strstr(last_fallback,"recovery") && strstr(last_fallback,"effective=2"));uss_menu_diag_fallback(&d,normal_diag_log,&normal,&e);VERIFY(fallback_logs==18);}
 printf("DEFAULT_FALLBACK_CHECKS=%u\n",fallback_checks);puts("PASS actual callback/configuration/handover/input and reserved normal-user logs; no save or provider mapping writes");return 0;
}
