#include "../src/uss_gameplay_application.h"
#define USS_PLAYER_TEST_MAIN player_control_regressions
#include "uss_player_control_harness.c"
static UssGameplayApplication application;static HfTickCallback app_callback;static void* app_user;
static unsigned app_registrations,app_registration_ok,app_effects;
static int register_application(HfMod* mod,HfTickCallback callback,void* user)
{
    assert(mod==MOD);++app_registrations;app_callback=callback;app_user=user;
    callback(mod,0,user);assert(!application.initialized && !application.loop.updates);return (int)app_registration_ok;
}
static int application_own_formation(void* user,UssFormationEffect effect,uintptr_t first,uintptr_t second,
    uintptr_t record,uintptr_t inventory,uint32_t avatar,uint32_t value,const UssInventoryMessage* message)
{(void)user;(void)effect;(void)first;(void)second;(void)record;(void)inventory;(void)avatar;(void)value;(void)message;++app_effects;assert(0);return 0;}
static int application_own_replacement(void* user,UssReplacementEffect effect,uintptr_t first,uintptr_t second,
    uintptr_t record,uintptr_t inventory,uint32_t avatar,uint32_t value,const UssInventoryMessage* message)
{(void)user;(void)effect;(void)first;(void)second;(void)record;(void)inventory;(void)avatar;(void)value;(void)message;++app_effects;assert(0);return 0;}
static int application_own_death_drop(void* user,uintptr_t entry,uintptr_t inventory,uint32_t avatar,uint32_t entity)
{(void)user;(void)entry;(void)inventory;(void)avatar;(void)entity;++app_effects;assert(0);return 0;}
static int application_own_presentation(void* user,UssHudPresentationEffect effect,uintptr_t entry,uintptr_t node,
    uint64_t value,uint32_t* width,uint32_t* height)
{(void)user;(void)effect;(void)entry;(void)node;(void)value;(void)width;(void)height;assert(0);return 0;}
static unsigned app_menu_frames,app_menu_busy;
static void own_menu_frame(void* user,float dt,UssSettingsState* state,unsigned busy)
{(void)user;(void)dt;assert(state==&application.settings);++app_menu_frames;app_menu_busy=busy;}
/* A callback can arm HUD work after the loop captured its initial time. */
static int application_later_hud_event(void* user,const UssCoreObservation* current,UssPlayerInputSample* raw)
{
    (void)user;(void)current;(void)raw;
    ++now_ms;application.hud.popup.active=1;application.hud.popup.started=now_ms;
    application.hud.popup.next=now_ms+25;
    return 0; /* No input/native operation; only a new cosmetic event exists. */
}
static UssGameplayEnvironment application_fixture(void)
{
    UssGameplayEnvironment environment={0};
    player_fixture(); /* Use the actual reader graph, not a synthetic flags producer. */
    memset(&application,0,sizeof(application));app_registrations=app_effects=app_menu_frames=0;app_registration_ok=1;
    environment.menu_frame=own_menu_frame;fake.on_game_thread=register_application;environment.platform=producer().platform;
    environment.hud_presentation=application_own_presentation;environment.clock_ms=runtime_clock;environment.foreground=own_foreground;environment.dedicated=own_dedicated;
    environment.drop_stored=application_own_death_drop;environment.formation_effect=application_own_formation;environment.replacement_effect=application_own_replacement;
    return environment;
}
#ifndef USS_APPLICATION_TEST_MAIN
#define USS_APPLICATION_TEST_MAIN main
#endif
int USS_APPLICATION_TEST_MAIN(void)
{
    UssGameplayEnvironment environment;uint64_t updates;unsigned before;
    (void)player_control_regressions;
    environment=application_fixture();assert(uss_gameplay_application_register(&application,&fake,MOD,&environment,NULL));
    assert(app_registrations==1 && !application.initialized && !app_menu_frames);
    foreground_ok=0;app_callback(MOD,0,app_user);assert(!application.initialized && !application.loop.updates && !app_effects);
    assert(app_menu_frames==1 && !app_menu_busy);
    application.formation.publication.busy=1;application_menu_frame(&application,0);assert(app_menu_busy);
    application.formation.publication.busy=0;application_menu_frame(&application,0);assert(!app_menu_busy);foreground_ok=1;now_ms+=100;app_callback(MOD,0,app_user);
    assert(application.initialized && application.loop.updates==1 && !app_effects);
    assert(application.hud.presentation.invoke==application_own_presentation && application.loop.io.refilled==application_refilled);
    assert(application.native.acknowledge==uss_hud_runtime_ack && application.formation.io.acknowledge==formation_native_acknowledge);
    assert(application.player.automatic==&application.automatic && application.replacement.formation==&application.native);
    assert(application.loop.life==&application.formation.life && application.formation.thread==7);
    /* No pair was inherited from the diagnostic fixture's software state. The
     * real slot is just remembered from the new application's own observation. */
    /* Applying the first observation establishes a new context generation.
     * Admission uses the next fresh observation stamped with that generation. */
    now_ms+=25;app_callback(MOD,0,app_user);
    assert(!application.formation.life.owner.pair_present && application.formation.history.remembered);
    assert(!application.input.pickup_pins_checked); /* No setting work for unchanged ordinary support. */
    {uint64_t callback_started=now_ms;UssCoreObservation current={0};
     application.player.sample=application_later_hud_event;
     (void)application_control(&application,&application.formation.life,&current,callback_started);
     assert(application.hud.popup.active && !application.hud.popup.checks);
     application.hud.popup.active=0;application.player.sample=uss_player_sdk_sample;}

    assert(!uss_gameplay_application_register(&application,&fake,MOD,&environment,NULL) && app_registrations==1);
    updates=application.loop.updates;uss_gameplay_loop_stop(&application.loop);before=reads;
    now_ms+=100;app_callback(MOD,0,app_user);assert(application.loop.updates==updates && reads==before);
    environment=application_fixture();app_registration_ok=0;
    assert(!uss_gameplay_application_register(&application,&fake,MOD,&environment,NULL));
    app_callback(MOD,0,app_user);assert(!application.initialized && !application.loop.updates && !app_effects);
    puts("USS normal application: single fake-SDK registration, lazy real policy initialization, owned controllers, no diagnostic state, stop/failure inert PASS");return 0;
}
