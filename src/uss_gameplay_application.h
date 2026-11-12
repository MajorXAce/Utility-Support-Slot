/* Session-stable normal gameplay assembly. One persistent SDK callback, no
 * diagnostic tickets, standing setup flags, hotkeys or operator command files.
 * Caller supplies reviewed effect invokers and loads the menu-owned settings.
 * This assembly is not an installation or permission to run it in a game. */
#ifndef USS_GAMEPLAY_APPLICATION_H
#define USS_GAMEPLAY_APPLICATION_H
#include "uss_player_input_sdk.h"
#include "uss_automatic_native.h"
#include "uss_recovery_runtime.h"
#include "uss_hud_runtime.h"

typedef struct UssGameplayEnvironment {
    UssCapturePlatform platform;void* user;uint64_t (*clock_ms)(void*);
    int (*foreground)(void*);int (*dedicated)(void*,const UssSettings*,unsigned*);
    int (*formation_effect)(void*,UssFormationEffect,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uint32_t,uint32_t,const UssInventoryMessage*);
    int (*replacement_effect)(void*,UssReplacementEffect,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uint32_t,uint32_t,const UssInventoryMessage*);
    void* effect_user;
    int (*drop_stored)(void*,uintptr_t,uintptr_t,uint32_t,uint32_t);
    UssRefillObserver refill_notice; /* Same optional positive-gain observer as Lua. */
    UssHudPresentationInvoke hud_presentation; /* Optional, isolated cosmetic binding. */
    void (*menu_frame)(void*,float,UssSettingsState*,unsigned); /* Session UI; independently validates its native host. */
} UssGameplayEnvironment;
typedef struct UssGameplayApplication {
    UssGameplayLoop loop;UssFormationRuntime formation;UssFormationNative native;
    UssAutomaticController automatic;UssAutomaticNative replacement;UssPlayerControl player;
    UssHudRuntime hud;UssRecoveryRuntime recovery;UssPlayerInputSdk input;UssSettingsState settings;UssGameplayEnvironment environment;
    unsigned attempted,initialized;const char* reason;
} UssGameplayApplication;
static void application_menu_frame(void* user,float dt)
{
    UssGameplayApplication* app=user;
    unsigned busy=app->formation.busy || app->formation.publication.busy ||
        app->formation.work.phase==USS_PAIR_ACTIVE || app->automatic.replacement.phase==USS_REPLACE_ACTIVE ||
        (app->recovery.work.phase!=USS_RECOVERY_IDLE && app->recovery.work.phase!=USS_RECOVERY_COMPLETE);
    if(uss_player_sdk_managed_pending(&app->input,app->player.input.support_down,app->player.input.cycle_down))
        uss_gameplay_loop_wake(&app->loop);
    if(app->environment.menu_frame)app->environment.menu_frame(app->environment.user,dt,&app->settings,busy);
}
static uint64_t application_clock(void* user)
{UssGameplayApplication* app=user;return app->environment.clock_ms(app->environment.user);}
static unsigned application_thread(void* user)
{UssGameplayApplication* app=user;return app->environment.platform.thread_id(app->environment.platform.user);}
static uint64_t application_native_clock(void* user)
{UssFormationNative* native=user;UssGameplayApplication* app=native->invoke_user;return application_clock(app);}
static int application_formation_effect(void* user,UssFormationEffect effect,uintptr_t first,uintptr_t second,
    uintptr_t record,uintptr_t inventory,uint32_t avatar,uint32_t value,const UssInventoryMessage* message)
{
    UssGameplayApplication* app=user;
    return app->environment.formation_effect(app->environment.effect_user,effect,first,second,record,inventory,avatar,value,message);
}
static int application_policy(void* user,UssCoreFlags* flags)
{
    UssGameplayApplication* app=user;UssFormationIo io;unsigned thread;
    if(!app->initialized){
        /* Initialization occurs on the SDK's actual game callback. Read-only
         * policy failures may be sampled again; no effect was attempted. */
        thread=application_thread(app);app->formation.thread=thread;
        if(!uss_player_sdk_policy(&app->input,flags)){app->reason="WAIT_PLAYER_POLICY";return 0;}
        io=uss_formation_native_io(&app->native,application_native_clock);
        if(!uss_formation_runtime_init(&app->formation,&io,thread,flags) ||
            !uss_automatic_native_init(&app->replacement,&app->automatic,&app->native,uss_player_sdk_pickup,&app->input,
                app->environment.replacement_effect,app->environment.effect_user)){
            app->reason="APPLICATION_INITIALIZATION_FAILED";uss_gameplay_loop_stop(&app->loop);return 0;
        }
        app->initialized=1;app->reason="NORMAL_GAMEPLAY_INITIALIZED";return 1;
    }
    return uss_player_sdk_policy(&app->input,flags);
}
/* Only confirmed exit/new actor retires consumed work. Preserve settings,
 * process counters and publisher disable/suspension after uncertain sends. */
static int application_retire_actor(UssGameplayApplication* app)
{
    UssFormationRuntime* runtime=&app->formation;
    if(runtime->busy || runtime->publication.busy)return 0;
    memset(&runtime->work,0,sizeof(runtime->work));uss_local_pair_invalidate(&runtime->local);
    uss_hud_runtime_retire(&app->hud);
    memset(&runtime->life.plan,0,sizeof(runtime->life.plan));runtime->life.phase=USS_LIFE_EMPTY;
    pickup_clear(&runtime->history);uss_publication_reset(&runtime->publication);
    memset(&app->recovery.work,0,sizeof(app->recovery.work));app->recovery.real=0;app->recovery.watched_pair=0;app->recovery.waiting=0;
    uss_player_input_reset(&app->player.input,&app->settings);
    memset(&app->player.pending,0,sizeof(app->player.pending));
    if(!uss_automatic_native_init(&app->replacement,&app->automatic,&app->native,uss_player_sdk_pickup,&app->input,
        app->environment.replacement_effect,app->environment.effect_user))return 0;
    app->loop.handling_paused=0;app->reason="ACTOR_WORK_RETIRED";return 1;
}
static unsigned application_lifecycle(void* user,const UssCoreObservation* current,uint64_t now)
{
    UssGameplayApplication* app=user;UssPairOwner* owner=&app->formation.life.owner;
    if(!app->initialized || !current->valid)return USS_CONTROL_IDLE;
    if(owner->mission && (!current->mission || (owner->context_valid &&
        !pair_owner_same_actor(&owner->context,&current->context)))){
        if(!application_retire_actor(app))return USS_CONTROL_STOP;
        if(app->native.api->log)app->native.api->log(app->native.mod,
            "USS lifecycle=%s tick_ms=%llu epoch=%llu old_avatar=%u observed_avatar=%u; old work retired, completed effects not rolled back",
            current->mission?"ACTOR_CHANGED":"MISSION_ENDED",(unsigned long long)now,(unsigned long long)owner->epoch,
            owner->context.local.avatar,current->mission?current->context.local.avatar:0u);
        return USS_CONTROL_WORK; /* Current observation still carries OLD policy/generations. */
    }
    return USS_CONTROL_IDLE;
}
static int application_recovery_input(void* user,const UssRecoverySnapshot* snapshot)
{
    UssGameplayApplication* app=user;UssCoreObservation current={0};UssPlayerInputSample raw={0};UssPlayerGesture gesture;
    if(snapshot->death)return 1; /* No usable weapon input context after native death. */
    current.valid=current.mission=1;current.flags=app->loop.policy;current.inventory=snapshot->inventory;current.primary=snapshot->primary;
    if(!uss_player_sdk_sample(&app->input,&current,&raw))return 0;
    gesture=uss_player_input_poll(&app->player.input,&app->settings,&current,&app->formation.life.owner,&raw,application_clock(app),1);
    return gesture.known && !gesture.interrupted;
}
static unsigned application_recovery(void* user,const UssCoreObservation* current,uint64_t now)
{
    UssGameplayApplication* app=user;
    if(!app->initialized)return USS_CONTROL_STOP;
    app->formation.policy=app->loop.policy;
    {UssRecoveryPhase before=app->recovery.work.phase;unsigned result=uss_recovery_control(&app->recovery,current,now);
        /* Match Lua's switch_entity acknowledgment: the survivor is freshly
         * verified and the pair cleared, before later publication ticks allow
         * the native HUD to treat restoration as a new acquisition. */
        if(before==USS_RECOVERY_ATTACH && app->recovery.work.phase==USS_RECOVERY_PRIMARY)
        {
            uss_hud_runtime_restored(&app->hud,&app->recovery.work,now);
            if(app->native.api->log)app->native.api->log(app->native.mod,
                "USS survivor HUD acknowledgment result=%u reason=%s disabled=%u writes=%llu",
                app->hud.last_result,app->hud.reason?app->hud.reason:"NONE",app->hud.ack.disabled,
                (unsigned long long)app->hud.ack.writes);
        }
        return result;}
}
static unsigned application_control(void* user,UssCoreLifecycle* life,const UssCoreObservation* current,uint64_t now)
{
    UssGameplayApplication* app=user;
    unsigned result;if(!app->initialized)return USS_CONTROL_STOP;
    result=uss_player_control(&app->player,life,current,now);
    if(result==USS_CONTROL_STOP)uss_hud_runtime_cancel(&app->hud);
    else {
        uss_hud_runtime_completed_pair(&app->hud);
        /* Completion can arm an event using a later clock sample. Poll only
         * active cosmetic work with fresh time, not the callback's old start. */
        if(app->hud.popup.active || app->hud.handoff.active || app->hud.drop.active)
            uss_hud_runtime_poll(&app->hud,application_clock(app));
    }
    return result; /* HUD failures never change the gameplay result. */
}
static void application_refilled(void* user,const UssRefillNotice* notice)
{
    UssGameplayApplication* app=user;
    if(app->environment.refill_notice)app->environment.refill_notice(app->environment.user,notice);
}
static inline int uss_gameplay_application_register(UssGameplayApplication* app,const HfApi* api,HfMod* mod,
    const UssGameplayEnvironment* environment,const UssSettings* saved)
{
    UssGameplayLoopIo io={0};
    if(!app || app->attempted || !environment || !environment->clock_ms || !environment->foreground ||
        !environment->dedicated || !environment->platform.thread_id || !environment->platform.query ||
        !environment->platform.path || !environment->platform.export_address ||
        !environment->formation_effect || !environment->replacement_effect || !environment->drop_stored || !uss_sdk_compatible(api) || !mod)return 0;
    memset(app,0,sizeof(*app));app->attempted=1;app->environment=*environment;uss_settings_init(&app->settings,saved);
    app->native.api=api;app->native.mod=mod;app->native.platform=environment->platform;app->native.runtime=&app->formation;
    app->native.invoke=application_formation_effect;app->native.invoke_user=app;
    uss_hud_runtime_init(&app->hud,&app->native);
    app->hud.presentation.invoke=environment->hud_presentation;app->hud.presentation.user=environment->effect_user;
    app->native.acknowledge=uss_hud_runtime_ack;app->native.acknowledge_user=&app->hud;
    app->input.api=api;app->input.mod=mod;app->input.settings=&app->settings;app->input.runtime=&app->formation;app->input.native=&app->native;
    app->input.platform.user=environment->platform.user;app->input.platform.thread_id=environment->platform.thread_id;
    app->input.input_user=environment->user;app->input.foreground=environment->foreground;app->input.dedicated=environment->dedicated;
    app->player.automatic=&app->automatic;app->player.settings=&app->settings;app->player.user=&app->input;app->player.sample=uss_player_sdk_sample;app->player.select_single=uss_player_sdk_select_single;
    app->recovery.automatic=&app->automatic;app->recovery.native.formation=&app->native;
    app->recovery.native.reader.api=api;app->recovery.native.reader.mod=mod;
    app->recovery.native.reader.platform=app->input.platform;app->recovery.native.reader.policy=&app->formation.policy;
    app->recovery.native.input_user=app;app->recovery.native.input_clear=application_recovery_input;
    app->recovery.native.drop_user=environment->effect_user;app->recovery.native.drop=environment->drop_stored;
    io.frame=application_menu_frame;
    io.recover=application_recovery;io.lifecycle=application_lifecycle;io.refilled=application_refilled;
    io.user=app;io.clock_ms=application_clock;io.thread_id=application_thread;io.policy=application_policy;io.control=application_control;
    app->reason="REGISTERING_NORMAL_GAMEPLAY";
    if(!uss_gameplay_loop_register(&app->loop,api,mod,&app->formation.life,&io)){
        app->reason="APPLICATION_CALLBACK_FAILED";return 0;
    }
    return 1;
}
#endif
