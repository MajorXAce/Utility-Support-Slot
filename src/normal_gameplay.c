/* Normal USS entry, compiled offline. Enable explicitly only in a reviewed
 * candidate. Stable process/session storage: SDK tick has no unregister API. */
#include "uss_gameplay_application.h"
#include "uss_gameplay_windows.h"
#include "uss_player_input_windows.h"
#include "uss_native_menu_session.h"
#include "uss_menu_startup_inputs.h"
#include "uss_replacement_invoke.h"
#include "uss_vpm_windows.h"
#ifndef HF_USS_NORMAL_ENABLED
#define HF_USS_NORMAL_ENABLED 0
#endif
typedef struct UssNormalSession {
    UssGameplayApplication app;
    UssPlayerWindows player_windows;
    UssDedicatedWindows capture_windows;
    UssMenuSession menus;
    wchar_t settings_path[1024];
    UssVpmWindows external_menus;UssMenuHandover handover;
    UssSettings standalone_settings;unsigned standalone_saved,external_down;
    const char* external_report;unsigned external_owner;
    uint64_t next_menu_check;
    unsigned attempted,menu_stopped,menu_frames,bootstrap_reports,bootstrap_status;
    const char* bootstrap_checkpoint;
    const UssMenuFrame* save_frame;
    unsigned save_reports,tab_hook_attempted;
    void* tab_original; /* SDK-owned chain slot; session-retained. */
    unsigned gameplay_reported,gameplay_reports,last_pause,last_queued;
    UssPairPhase last_pair_phase;UssReplacementPhase last_replace_phase;
    const char* last_player_reason;const char* last_loop_reason;
    UssRecoveryPhase last_recovery_phase;const char* last_recovery_reason;const char* last_refill_reason;const char* last_pickup_reason;uint64_t last_refill_calls;const char* last_action_stage;const char* last_hash_stage;const char* last_source_reason;const char* last_selected_source_reason;
} UssNormalSession;
static UssNormalSession normal;
static int normal_foreground(void* user)
{(void)user;return uss_player_windows_foreground(NULL);}
static int normal_dedicated(void* user,const UssSettings* settings,unsigned* down)
{
    UssNormalSession* state=user;
    if(state->app.settings.external){if(!down)return 0;*down=state->external_down;return 1;}
    return uss_player_windows_dedicated(&state->player_windows,settings,down);
}
static void normal_menu_stop(UssNormalSession* state,const char* reason)
{
    if(state->menu_stopped)return;state->menu_stopped=1;
    if(state->app.native.api->log)state->app.native.api->log(state->app.native.mod,"USS menu stopped: %s; weapon handling remains active",reason);
}
static uint32_t normal_tab_input(uintptr_t bar,uintptr_t input,uintptr_t cursor)
{
    UssMenuSessionNode* node=normal.menus.current;UssMenuOwnership* owner=NULL;unsigned capturing=0;
    if(!normal.menu_stopped && node && node->menu.frame.controls.capture.phase!=USS_DEDICATED_IDLE &&
        node->sdk.thread==GetCurrentThreadId() && uss_menu_sdk_scope(&node->sdk)){
        owner=&node->menu.owner;capturing=1;
    }
    return uss_menu_tab_input_dispatch(&normal.tab_original,owner,capturing,bar,input,cursor);
}
/* Install once through the SDK, only after a current owned UI context exists.
 * A reported failure is not retried: SDK may have retained the detour slot. */
static int normal_tab_hook(UssNormalSession* state)
{
    UssMenuSessionNode* node=state->menus.current;unsigned char pin[24];int result;
    if(state->tab_hook_attempted)return state->tab_original!=NULL;
    state->tab_hook_attempted=1;
    if(!node || !state->app.native.api->hook ||
        !uss_menu_sdk_read_code(&node->sdk,node->sdk.base+0x17aa950,pin,sizeof(pin)) ||
        memcmp(pin,uss_menu_tab_input_pin,sizeof(pin)))return 0;
    result=state->app.native.api->hook(state->app.native.mod,node->sdk.base+0x17aa950,
        (void*)normal_tab_input,&state->tab_original);
    if(result || !state->tab_original)return 0;
    state->app.native.api->log(state->app.native.mod,"USS capture-only tab-input guard installed");return 1;
}
/* Observe software state only. Gameplay can pause while its SDK loop remains
 * enabled; that is distinct from the loop-stop report. Always report a new
 * pause, even after the bounded progress-log allowance is exhausted. */
static void normal_gameplay_report(UssNormalSession* state)
{
    UssGameplayApplication* app=&state->app;const HfApi* api=app->native.api;
    unsigned paused=app->loop.handling_paused,new_pause;
    if(!app->initialized || !api || !api->log)return;
    new_pause=paused && (!state->gameplay_reported || !state->last_pause);
    if(state->gameplay_reported && paused==state->last_pause && app->player.pending.present==state->last_queued &&
        app->formation.work.phase==state->last_pair_phase && app->automatic.replacement.phase==state->last_replace_phase &&
        app->player.reason==state->last_player_reason && app->loop.reason==state->last_loop_reason &&
        app->recovery.work.phase==state->last_recovery_phase && app->recovery.reason==state->last_recovery_reason && app->loop.refill_core.reason==state->last_refill_reason &&
        app->loop.refill.pickups.reason==state->last_pickup_reason && app->loop.refill.transaction.calls==state->last_refill_calls &&
        app->loop.refill.pickups.action_diagnostic.stage==state->last_action_stage &&
        app->loop.refill.pickups.action_diagnostic.hash.stage==state->last_hash_stage &&
        app->loop.refill.pickups.source_reason==state->last_source_reason &&
        app->loop.refill.pickups.selected_source_reason==state->last_selected_source_reason)return;
    state->gameplay_reported=1;state->last_pause=paused;state->last_queued=app->player.pending.present;state->last_pair_phase=app->formation.work.phase;
    state->last_replace_phase=app->automatic.replacement.phase;state->last_player_reason=app->player.reason;
    state->last_loop_reason=app->loop.reason;state->last_recovery_phase=app->recovery.work.phase;
    state->last_recovery_reason=app->recovery.reason;state->last_refill_reason=app->loop.refill_core.reason;
    state->last_pickup_reason=app->loop.refill.pickups.reason;state->last_refill_calls=app->loop.refill.transaction.calls;state->last_action_stage=app->loop.refill.pickups.action_diagnostic.stage;
    state->last_hash_stage=app->loop.refill.pickups.action_diagnostic.hash.stage;state->last_source_reason=app->loop.refill.pickups.source_reason;
    state->last_selected_source_reason=app->loop.refill.pickups.selected_source_reason;
    if(!new_pause && state->gameplay_reports>=128)return;
    ++state->gameplay_reports;
    api->log(app->native.mod,"USS gameplay state paused=%u loop=%s player=%s automatic=%s pair_phase=%u pair_reason=%s step=%u primary_requested=%u attach_entered=%u replacement_phase=%u replacement_stage=%u replacement_consumed=%u replacement_reason=%s support_down=%u release_required=%u queued_swap=%u controller_draw=%u native_reason=%s native_effects=%u recovery_phase=%u recovery_reason=%s recovery_work_reason=%s recovery_consumed=%u refill_reason=%s pickup_reason=%s source_reason=%s selected_source_reason=%s pickup_completed=%u pickup_zone=%u pickup_selected=%u refill_driver=%s refill_result=%u refill_transaction=%s refill_calls=%llu refill_reserve=%u refill_amount=%u action_stage=%s action_actual=%u action_expected=%u primary_phase=%u primary_prior=%u primary_timer=%u primary_block=%u publication_disabled=%u publication_suspended=%u",
        paused,app->loop.reason?app->loop.reason:"NONE",app->player.reason?app->player.reason:"NONE",
        app->automatic.reason?app->automatic.reason:"NONE",app->formation.work.phase,
        app->formation.work.reason?app->formation.work.reason:"NONE",app->formation.work.index,
        app->formation.work.primary_requested,app->formation.work.attachment_consumed,app->automatic.replacement.phase,
        app->automatic.replacement.stage,app->automatic.replacement.consumed_mask,
        app->automatic.replacement.reason?app->automatic.replacement.reason:"NONE",app->player.input.support_down,
        app->settings.release_required,app->player.pending.present,app->formation.controller_draw,app->native.reason?app->native.reason:"NONE",app->native.effects,app->recovery.work.phase,
        app->recovery.reason?app->recovery.reason:"NONE",app->recovery.work.reason?app->recovery.work.reason:"NONE",
        app->recovery.work.consumed,app->loop.refill_core.reason?app->loop.refill_core.reason:"NONE",
        app->loop.refill.pickups.reason?app->loop.refill.pickups.reason:"NONE",
        app->loop.refill.pickups.source_reason?app->loop.refill.pickups.source_reason:"NONE",
        app->loop.refill.pickups.selected_source_reason?app->loop.refill.pickups.selected_source_reason:"NONE",app->loop.refill.pickups.previous_entity,
        app->loop.refill.pickups.previous_zone,app->loop.refill.pickups.selected_entity,
        app->loop.refill.reason?app->loop.refill.reason:"NONE",app->loop.last_refill,
        app->loop.refill.transaction.reason?app->loop.refill.transaction.reason:"NONE",
        (unsigned long long)app->loop.refill.transaction.calls,app->loop.refill.pickups.target.reserve,
        app->loop.refill.pickups.target.direct_amount,
        app->loop.refill.pickups.action_diagnostic.stage?app->loop.refill.pickups.action_diagnostic.stage:"NONE",
        app->loop.refill.pickups.action_diagnostic.actual,app->loop.refill.pickups.action_diagnostic.expected,
        u32(app->formation.latest.primary.fields.row+16),u32(app->formation.latest.primary.fields.row+8),
        u32(app->formation.latest.primary.fields.row+24),u32(app->formation.latest.primary.fields.row+28),app->formation.publication.disabled,app->formation.publication.suspended);
    if(app->loop.refill.pickups.action_diagnostic.hash.stage){
        const UssRefillHashDiagnostic* hash=&app->loop.refill.pickups.action_diagnostic.hash;
        api->log(app->native.mod,"USS pickup hash stage=%s capacity=%u empty=%u multiplier=%u array_alignment=%u key=%u probe=%u found=%u index=%u",
            hash->stage,hash->capacity,hash->empty,hash->multiplier,hash->alignment,hash->key,hash->probe,hash->found,hash->index);
    }
}
/* Run on the existing owning game callback. A live standalone screen finishes
 * its lifetime before external ownership starts. Local settings never get saved
 * from an external edit; the provider persists its own mode/action mappings. */
static void normal_external_menus(UssNormalSession* state,UssSettingsState* settings,unsigned busy,uint64_t now)
{
    UssMenuProviderSample sample={0};UssMenuHandoverView view;UssSettings draft;
    unsigned available=0,was_down=state->external_down,transition;
    const char* reason;
    if(!settings->external){state->standalone_settings=settings->current;state->standalone_saved=1;}
    if(!state->standalone_saved)return;
    if(uss_vpm_windows_discover(&state->external_menus,now,state->standalone_settings.mode))
        available=uss_vpm_menus_sample(&state->external_menus.menus,&sample);
    /* A failed provider read revokes input now, rather than holding the last
     * pressed sample until lease expiry. Unknown/absent modules remain optional. */
    if(state->menus.current && !settings->external)(void)uss_menu_session_retire(&state->menus);
    view=uss_menu_handover_step(&state->handover,&sample,now,
        busy || (!settings->external && state->menus.current!=NULL),!normal_foreground(state),
        &state->standalone_settings,0);
    transition=view.external!=settings->external;
    if(!busy && (!transition || settings->external || !state->menus.current)){
        draft=view.external?settings->current:state->standalone_settings;draft.mode=view.mode;
        if(transition || !uss_settings_equal(&draft,&settings->current)){
            if(settings->revision==UINT64_MAX)return;
            settings->pending_present=0;
            if(!uss_settings_equal(&draft,&settings->current)){
                (void)uss_settings_queue(settings,&draft);(void)uss_settings_commit(settings,0);
            }else ++settings->revision;
            settings->release_required=1;
        }
        settings->external=view.external;settings->provider=view.external?state->handover.provider:0;
    }
    state->external_down=settings->external && available && view.external?view.utility_down:0;
    if(settings->external && settings->current.mode==USS_BIND_DEDICATED && (state->external_down || was_down))
        uss_gameplay_loop_wake(&state->app.loop);
    reason=state->external_menus.menus.provider?state->external_menus.menus.reason:state->external_menus.reason;
    if(reason && (reason!=state->external_report || settings->external!=state->external_owner)){
        state->external_report=reason;state->external_owner=settings->external;
        state->app.native.api->log(state->app.native.mod,"USS external menus status=%s owner=%u options=%u bindings=%u",
            reason,settings->external,state->external_menus.menus.option_registered,state->external_menus.menus.binding_registered);
    }
}
static void normal_menu_frame(void* user,float dt,UssSettingsState* settings,unsigned busy)
{
    UssNormalSession* state=user;const HfApi* api=state->app.native.api;
    UssMenuSdkContext context={0};UssMenuObservationIo observer;UssMenuHostSnapshot current={0};
    UssMenuReadBudget budget={0};unsigned observed,status;
    unsigned char inert[216];unsigned result;uint64_t now=GetTickCount64();size_t size=0;
    normal_gameplay_report(state);
    normal_external_menus(state,settings,busy,now);
    if(!state->menu_frames++)api->log(state->app.native.mod,"USS menu callback active; bootstrap observation pending");
    if(state->menu_stopped || settings->external || settings->provider)return;
    if(state->menus.current){
        /* Closed-screen check occurs before the frame's destructive ownership
         * proof, allowing a new lifetime without reusing old native storage. */
        if(uss_menu_session_retire(&state->menus))return;
        result=uss_menu_frame_step(&state->menus.current->menu.frame,dt,now,busy);
        {UssMenuFrame* frame=&state->menus.current->menu.frame;
         if(state->save_frame!=frame){state->save_frame=frame;state->save_reports=0;}
         if(state->save_reports!=frame->controls.save_attempts){
            state->save_reports=frame->controls.save_attempts;
            api->log(state->app.native.mod,"USS settings save status=%s stage=%s windows_error=%lu path=%ls; menu remains active",
                frame->controls.save_failed?"SESSION_ONLY":"SAVED",frame->save_report.stage?frame->save_report.stage:"UNSET",
                (unsigned long)frame->save_report.windows_error,frame->settings_path);
         }}
        if(result==USS_MENU_STEP_REFUSED){
            UssMenuSessionNode* node=state->menus.current;UssMenuLifetime* menu=&node->menu;
            /* Only retained software state: reporting never repeats game effects
             * or performs new reads after a refusal. Log once, then stop. */
            api->log(state->app.native.mod,
                "USS menu refusal checkpoint=%s dt=%.9g phase=%u stage=%u owner_stopped=%u writer_stopped=%u writes=%u tab_ready=%u rows_built=%u chrome_effects=%u row_effects=%u host_reason=%s",
                menu->frame.checkpoint?menu->frame.checkpoint:"UNSET",(double)dt,
                menu->lifecycle.host.phase,menu->owner.stage,menu->owner.stopped,node->writer.stopped,
                node->writer.attempts,menu->chrome.tab_ready,menu->rows.built,menu->chrome.effects,menu->rows.effects,
                menu->lifecycle.host.reason?menu->lifecycle.host.reason:"NONE");
            api->log(state->app.native.mod,
                "USS menu boundary lifecycle=%s owner_failure=%s owner_read=%s lifecycle_read=%s observed_valid=%u screen_same=%u content_same=%u epoch_same=%u input_same=%u container_same=%u",
                menu->lifecycle.checkpoint?menu->lifecycle.checkpoint:"UNSET",menu->owner.failure?menu->owner.failure:"NONE",
                menu->owner.last_read.checkpoint?menu->owner.last_read.checkpoint:"UNSET",
                menu->lifecycle.last_read.checkpoint?menu->lifecycle.last_read.checkpoint:"UNSET",menu->lifecycle.last_observed.valid,
                menu->owner.identity.screen==menu->lifecycle.last_observed.screen,menu->owner.identity.content==menu->lifecycle.last_observed.content,
                menu->owner.identity.epoch==menu->lifecycle.last_observed.epoch,menu->owner.identity.input==menu->lifecycle.last_observed.input,
                menu->owner.identity.container==menu->lifecycle.last_observed.container);
            api->log(state->app.native.mod,"USS menu validation entry=%u rva=%08x owner_read_index=%u owner_actual=%08x owner_expected=%08x lifecycle_read_index=%u lifecycle_actual=%08x lifecycle_expected=%08x",
                menu->owner.failed_entry,menu->owner.failed_entry<USS_MENU_ENTRY_COUNT?uss_menu_entry_pins[menu->owner.failed_entry].rva:0,
                menu->owner.last_read.index,menu->owner.last_read.actual,menu->owner.last_read.expected,
                menu->lifecycle.last_read.index,menu->lifecycle.last_read.actual,menu->lifecycle.last_read.expected);
            api->log(state->app.native.mod,"USS menu row state selected=%u uss=%u shown=%u category=%u ours_intact=%u foreign=%u topmost=%u language=%u",
                menu->owner.last_observed.selected_tab,menu->owner.last_observed.uss_tab,
                menu->owner.last_observed.tab,menu->owner.last_observed.category,
                menu->owner.last_observed.ours_intact,menu->owner.last_observed.foreign_owner,
                menu->owner.last_observed.topmost,menu->owner.last_observed.language_changing);
            normal_menu_stop(state,"FRAME_REFUSED");
        }
        return;
    }
    if(now<state->next_menu_check)return;state->next_menu_check=now+100;
    context.api=api;context.base=api->module_base("game.dll",&size);context.module_size=size;
    context.thread=GetCurrentThreadId();context.epoch=1;
    observer=uss_menu_sdk_observer(&context);
    observed=uss_menu_observe_host(&observer,NULL,&current,&budget);
    status=!observed?1:!current.escape_present?2:!current.topmost?3:current.language_changing?4:current.foreign_owner?5:!current.vanilla_intact?6:7;
    if(state->bootstrap_reports<16 && (!state->bootstrap_reports || status!=state->bootstrap_status || budget.checkpoint!=state->bootstrap_checkpoint)){
        ++state->bootstrap_reports;state->bootstrap_status=status;state->bootstrap_checkpoint=budget.checkpoint;
        api->log(state->app.native.mod,"USS menu bootstrap status=%u checkpoint=%s reads=%u bytes=%u read_failed=%u index=%u actual=%08x expected=%08x escape=%u topmost=%u language=%u foreign=%u vanilla=%u",
            status,budget.checkpoint?budget.checkpoint:"UNSET",budget.calls,budget.bytes,budget.failed,budget.index,budget.actual,budget.expected,
            current.escape_present,current.topmost,current.language_changing,current.foreign_owner,current.vanilla_intact);
    }
    if(status!=7)return;
    if(!uss_menu_read_inert(&observer,inert)){normal_menu_stop(state,"INERT_DESCRIPTOR_REFUSED");return;}
    if(!uss_menu_session_bootstrap(&state->menus,&context,settings,inert,sizeof(inert),state->settings_path,&state->capture_windows)){
        if(state->menus.stopped)normal_menu_stop(state,"BOOTSTRAP_REFUSED");
    }else if(!normal_tab_hook(state))normal_menu_stop(state,"TAB_INPUT_HOOK_REFUSED");
}
HF_MOD_EXPORT int hellforge_mod_init(const HfApi* api,HfMod* mod)
{
    UssGameplayEnvironment environment={0};UssSettings saved;int loaded;const char* build;
    if(normal.attempted)return 1;normal.attempted=1;
    if(!uss_sdk_compatible(api) || !mod || !api->log || !api->build_id)return 1;
    if(!HF_USS_NORMAL_ENABLED){api->log(mod,"USS normal gameplay entry: inactive offline build");return 0;}
    build=api->build_id();if(!build || strcmp(build,"25480438") || !api->mod_folder)return 1;
    if(!uss_menu_user_settings_startup(normal.settings_path)){
        api->log(mod,"USS settings directory unavailable; Windows error=%lu",(unsigned long)GetLastError());return 1;}
    api->log(mod,"USS settings path=%ls",normal.settings_path);
    loaded=uss_settings_load_file(normal.settings_path,&saved);
    if(loaded==USS_SETTINGS_MISSING){
        wchar_t legacy[1024];
        if(!uss_menu_settings_path(api->mod_folder(mod),legacy))return 1;
        loaded=uss_settings_load_file(legacy,&saved);
        if(loaded==USS_SETTINGS_LOADED)api->log(mod,"USS legacy settings loaded read-only; next changed choice saves to AppData");
    }
    if(loaded!=USS_SETTINGS_LOADED && loaded!=USS_SETTINGS_MISSING){api->log(mod,"USS refused invalid/unreadable settings; file preserved");return 1;}
    if(!uss_dedicated_windows_bind(&normal.capture_windows))return 1;
    normal.player_windows.xinput=normal.capture_windows.module;
    normal.player_windows.get_state=normal.capture_windows.get_state;normal.player_windows.attempted=1;
    normal.menus.allocate=uss_menu_session_heap;
    environment.user=&normal;environment.platform=uss_capture_windows_platform();
    environment.clock_ms=uss_gameplay_windows_clock;environment.foreground=normal_foreground;environment.dedicated=normal_dedicated;
    environment.formation_effect=formation_native_invoke;environment.replacement_effect=replacement_runtime_invoke;
    environment.drop_stored=uss_recovery_drop_invoke;environment.hud_presentation=uss_hud_presentation_invoke;
    environment.zone_close=uss_zone_close_invoke;environment.ping_invoke=uss_ping_native_invoke;
    environment.menu_frame=normal_menu_frame;
    if(!uss_gameplay_application_register(&normal.app,api,mod,&environment,loaded==USS_SETTINGS_LOADED?&saved:NULL)){
        /* Registration may have retained our callback even on reported failure.
         * Keep DLL storage loaded; loop remains disabled, never retry. */
        api->log(mod,"USS normal registration refused; retained disabled callback storage");return 0;
    }
    api->log(mod,"USS normal gameplay registered; native menu uses standalone settings");return 0;
}
