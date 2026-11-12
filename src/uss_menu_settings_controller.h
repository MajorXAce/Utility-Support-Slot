/* Standalone menu edit coordinator. UI/native and storage effects are explicit
 * callbacks; queued edits become gameplay settings only at an idle boundary. */
#ifndef USS_MENU_SETTINGS_CONTROLLER_H
#define USS_MENU_SETTINGS_CONTROLLER_H
#include "uss_dedicated_capture.h"
typedef struct UssMenuSettingsController {
    UssDedicatedCapture capture;
    void* user;
    int (*consume)(void*);
    int (*save)(void*,const UssSettings*);
    unsigned stopped,save_failed,save_attempts;
} UssMenuSettingsController;
static inline int uss_menu_settings_begin(UssMenuSettingsController* menu,UssSettingsState* settings,
    const UssDedicatedCaptureSample* sample)
{
    if(!menu || menu->stopped || !menu->consume || !menu->save ||
        !uss_dedicated_capture_begin(&menu->capture,settings,sample))return 0;
    if(menu->consume(menu->user)!=1){
        uss_dedicated_capture_cancel(&menu->capture,settings);menu->stopped=1;return 0;
    }
    return 1;
}
static inline int uss_menu_settings_mode(UssMenuSettingsController* menu,UssSettingsState* settings,unsigned mode)
{
    UssSettings draft;
    if(!menu || menu->stopped || !settings || settings->external || settings->provider ||
        menu->capture.phase!=USS_DEDICATED_IDLE)return 0;
    draft=settings->pending_present?settings->pending:settings->current;draft.mode=mode;
    return uss_settings_queue(settings,&draft);
}
/* Run before ordinary native selector handling. While capturing, consume Menu
 * actions first, so a binding key cannot also activate another option. */
static inline unsigned uss_menu_settings_capture(UssMenuSettingsController* menu,UssSettingsState* settings,
    const UssDedicatedCaptureSample* sample)
{
    if(!menu || menu->stopped)return USS_DEDICATED_CANCELLED;
    if(menu->capture.phase==USS_DEDICATED_IDLE)return USS_DEDICATED_WAIT;
    if(!settings || settings->external || settings->provider || !sample || !sample->valid || !sample->foreground ||
        sample->host!=menu->capture.host || sample->epoch!=menu->capture.epoch ||
        sample->page!=menu->capture.page || sample->now_ms<menu->capture.last_ms ||
        sample->now_ms>menu->capture.deadline_ms)
        return uss_dedicated_capture_cancel(&menu->capture,settings);
    if(!menu->consume || menu->consume(menu->user)!=1){
        menu->stopped=1;return uss_dedicated_capture_cancel(&menu->capture,settings);
    }
    return uss_dedicated_capture_step(&menu->capture,settings,sample);
}
/* A failed file save leaves the applied choice usable for this session and
 * reports failure once. Never repeatedly write every frame or roll gameplay back. */
static inline int uss_menu_settings_apply(UssMenuSettingsController* menu,UssSettingsState* settings,unsigned transaction_active)
{
    if(!menu || menu->stopped || !menu->save || !settings || settings->external || settings->provider ||
        menu->capture.phase!=USS_DEDICATED_IDLE || transaction_active)return 0;
    if(!uss_settings_commit(settings,0))return 0;
    ++menu->save_attempts;
    /* File persistence is separate from native UI safety. Keep the applied
     * choice usable and permit another deliberate edit; never retry each tick. */
    menu->save_failed=menu->save(menu->user,&settings->current)!=1;
    return menu->save_failed?2:1; /* 2 = session-only, not a native refusal. */
}
#endif
