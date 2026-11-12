/* One session-owned UI frame assembly. No registration or game access at
 * construction. Settings/file callbacks run only through this explicit driver. */
#ifndef USS_NATIVE_MENU_FRAME_H
#define USS_NATIVE_MENU_FRAME_H
#include "uss_native_menu_lifecycle.h"
#include "uss_native_menu_controls.h"
#include "uss_menu_capture_guard.h"
#include "uss_settings_store.h"
#include "uss_dedicated_input_windows.h"
typedef struct UssMenuFrame {
    UssMenuLifecycle* lifecycle;
    UssMenuSettingsController controls;
    UssMenuCaptureGuard guard;
    void* input_user;
    int (*sample)(void*,uint64_t,uint64_t,uint64_t,unsigned,UssDedicatedCaptureSample*);
    wchar_t settings_path[1024];
    UssSettingsSaveReport save_report;
    const char* checkpoint; /* Last software step; no extra native reads for reporting. */
} UssMenuFrame;
static int uss_menu_frame_consume(void* user)
{return uss_menu_capture_guard_consume(&((UssMenuFrame*)user)->guard);}
static int uss_menu_frame_save(void* user,const UssSettings* settings)
{UssMenuFrame* frame=user;return uss_settings_save_file_report(frame->settings_path,settings,&frame->save_report);}
static inline int uss_menu_frame_bind(UssMenuFrame* frame,UssMenuLifecycle* lifecycle,const wchar_t* path)
{
    size_t length;
    if(!frame || !lifecycle || !lifecycle->owner || !path || !(length=wcslen(path)) || length>980 ||
        frame->controls.capture.phase!=USS_DEDICATED_IDLE)return 0;
    frame->lifecycle=lifecycle;frame->guard.owner=lifecycle->owner;frame->guard.invoke=uss_menu_consume_invoke;
    frame->controls.user=frame;frame->controls.consume=uss_menu_frame_consume;frame->controls.save=uss_menu_frame_save;
    memcpy(frame->settings_path,path,(length+1)*sizeof(wchar_t));return 1;
}
static int uss_menu_frame_windows_sample(void* user,uint64_t host,uint64_t epoch,uint64_t now,
    unsigned page,UssDedicatedCaptureSample* sample)
{return uss_dedicated_windows_sample((UssDedicatedWindows*)user,host,epoch,now,page,sample);}
/* Wire an already prepared Windows adapter. OS module initialization remains an
 * explicit startup action; offline fixtures may supply their own OS functions. */
static inline int uss_menu_frame_bind_input(UssMenuFrame* frame,UssDedicatedWindows* input)
{
    if(!frame || !input || !input->foreground || !input->key_state || !input->get_state ||
        frame->controls.capture.phase!=USS_DEDICATED_IDLE)return 0;
    frame->input_user=input;frame->sample=uss_menu_frame_windows_sample;return 1;
}
/* Call before publishing settings to gameplay. Invalid/future/locked files are
 * not defaults: refuse without overwriting existing state or touching that file. */
static inline int uss_menu_frame_load(UssMenuFrame* frame,UssSettingsState* settings)
{
    UssSettings saved;int result;
    if(!frame || !frame->lifecycle || !settings || frame->lifecycle->host.phase!=USS_MENU_CLOSED ||
        settings->external || settings->provider || !frame->settings_path[0])return USS_SETTINGS_IO_ERROR;
    result=uss_settings_load_file(frame->settings_path,&saved);
    if(result==USS_SETTINGS_LOADED || result==USS_SETTINGS_MISSING)
        uss_settings_init(settings,result==USS_SETTINGS_LOADED?&saved:NULL);
    return result;
}
static inline unsigned uss_menu_frame_step(UssMenuFrame* frame,float dt,uint64_t now_ms,unsigned transaction_active)
{
    UssMenuLifecycle* lifecycle;UssSettingsState* settings;UssDedicatedCaptureSample sample={0};
    unsigned result,values[3]={0},page;UssMenuExpectedRows expected={0};UssMenuHostSnapshot current={0};
    if(!frame)return USS_MENU_STEP_REFUSED;
    frame->checkpoint="FRAME_CONTEXT";
    if(!(lifecycle=frame->lifecycle) || !(settings=lifecycle->settings) ||
        frame->controls.stopped || !frame->sample)return USS_MENU_STEP_REFUSED;
    frame->checkpoint="FRAME_TIMING";
    if(!now_ms || !isfinite(dt) || dt<0 || dt>1)return USS_MENU_STEP_REFUSED;
    if(settings->external || settings->provider){
        uss_dedicated_capture_cancel(&frame->controls.capture,settings);return USS_MENU_STEP_SUSPENDED;
    }
    if(frame->controls.capture.phase!=USS_DEDICATED_IDLE){
        /* Validate the current owned host BEFORE polling/consuming input. Ordinary
         * selector polling is skipped for the entire capture frame. */
        frame->checkpoint="CAPTURE_HOST";
        expected.tab_text=(uintptr_t)lifecycle->owner->chrome->tab_title;
        expected.require_tab=lifecycle->owner->chrome->tab_ready;
        expected.category_text=(uintptr_t)lifecycle->owner->chrome->category_title;
        uss_menu_rows_expected(lifecycle->owner->rows,&expected);
        if(uss_menu_observe_host(&lifecycle->owner->observer,&expected,&current,NULL) &&
            uss_menu_same_host(&lifecycle->owner->identity,&current)){
            if(!current.topmost || current.language_changing){
                uss_dedicated_capture_cancel(&frame->controls.capture,settings);return USS_MENU_STEP_SUSPENDED;
            }
            if(current.selected_tab!=current.uss_tab){
                uss_dedicated_capture_cancel(&frame->controls.capture,settings);
                frame->checkpoint="CAPTURE_TAB_EXIT";
                return uss_menu_lifecycle_step(lifecycle,dt,values,3);
            }
        }
        if(lifecycle->host.phase!=USS_MENU_OWNED ||
            !uss_menu_ownership_guard(lifecycle->owner,&lifecycle->owner->identity)){
            uss_dedicated_capture_cancel(&frame->controls.capture,settings);return USS_MENU_STEP_SUSPENDED;
        }
        page=frame->controls.capture.page;
        (void)frame->sample(frame->input_user,lifecycle->owner->identity.screen,
            lifecycle->owner->identity.epoch,now_ms,page,&sample);
        frame->checkpoint="CAPTURE_CONSUME";
        (void)uss_menu_settings_capture(&frame->controls,settings,&sample);
        if(frame->controls.stopped)return USS_MENU_STEP_REFUSED;
        if(!uss_menu_controls_present(lifecycle->owner->rows,&lifecycle->io,&frame->controls,settings))return USS_MENU_STEP_REFUSED;
        frame->checkpoint="CAPTURE_PRESENT";
        if(!uss_menu_chrome_present(lifecycle->owner->chrome,&lifecycle->io,1,dt))return USS_MENU_STEP_REFUSED;
        frame->checkpoint="CAPTURE_APPLY";
        if(uss_menu_settings_apply(&frame->controls,settings,transaction_active)<0)return USS_MENU_STEP_REFUSED;
        return USS_MENU_STEP_UPDATED;
    }
    frame->checkpoint="LIFECYCLE_STEP";
    result=uss_menu_lifecycle_step(lifecycle,dt,values,3);
    if(result!=USS_MENU_STEP_UPDATED)return result;
    page=values[1]?USS_DEDICATED_ANY:0;
    if(page && !frame->sample(frame->input_user,lifecycle->owner->identity.screen,
        lifecycle->owner->identity.epoch,now_ms,page,&sample))return USS_MENU_STEP_UPDATED;
    frame->checkpoint="CONTROLS_UPDATE";
    if(!uss_menu_controls_update(lifecycle->owner->rows,&lifecycle->io,&frame->controls,settings,values,page?&sample:NULL))
        return USS_MENU_STEP_REFUSED;
    if(!uss_menu_controls_present(lifecycle->owner->rows,&lifecycle->io,&frame->controls,settings))return USS_MENU_STEP_REFUSED;
    frame->checkpoint="SETTINGS_APPLY";
    return uss_menu_settings_apply(&frame->controls,settings,transaction_active)<0?USS_MENU_STEP_REFUSED:result;
}
#endif
