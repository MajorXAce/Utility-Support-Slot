/* Standalone USS capture uses the EXISTING Windows/XInput settings namespace.
 * This is deliberately distinct from engine logical IDs in uss_binding_capture.
 * Pure state machine: no keyboard polling, native call or settings-file write. */
#ifndef USS_DEDICATED_CAPTURE_H
#define USS_DEDICATED_CAPTURE_H
#include "uss_settings.h"
enum {USS_DEDICATED_KEYBOARD=1,USS_DEDICATED_CONTROLLER=2,USS_DEDICATED_ANY=3};
enum {USS_DEDICATED_IDLE=0,USS_DEDICATED_ARMING,USS_DEDICATED_LISTENING,USS_DEDICATED_RELEASING};
enum {USS_DEDICATED_WAIT=0,USS_DEDICATED_QUEUED,USS_DEDICATED_CANCELLED};
typedef struct UssDedicatedCaptureSample {
    uint64_t host,epoch,now_ms;unsigned valid,foreground,page;
    unsigned char keys[255]; /* Windows virtual-key DOWN flags, never engine IDs. */
    uint32_t controller; /* XInput buttons plus the two USS trigger bits. */
} UssDedicatedCaptureSample;
typedef struct UssDedicatedCapture {
    uint64_t host,epoch,start_ms,last_ms,deadline_ms;unsigned phase,page;
    uint32_t choice;unsigned choice_page;unsigned char previous[255];uint32_t previous_controller;
} UssDedicatedCapture;
static inline int uss_dedicated_capture_any(const UssDedicatedCaptureSample* sample)
{
    unsigned i;if(sample->controller)return 1;
    for(i=1;i<255;++i)if(sample->keys[i])return 1;return 0;
}
static inline unsigned uss_dedicated_capture_cancel(UssDedicatedCapture* capture,UssSettingsState* settings)
{
    if(capture){capture->phase=USS_DEDICATED_IDLE;capture->choice=0;}
    if(settings)settings->release_required=1;return USS_DEDICATED_CANCELLED;
}
static inline int uss_dedicated_capture_begin(UssDedicatedCapture* capture,UssSettingsState* settings,
    const UssDedicatedCaptureSample* sample)
{
    if(!capture || !settings || !sample || capture->phase!=USS_DEDICATED_IDLE || settings->external || settings->provider ||
        !uss_settings_valid(&settings->current) || !sample->valid || !sample->foreground || !sample->host || !sample->epoch ||
        !sample->now_ms || sample->now_ms>UINT64_MAX-15000 ||
        (sample->page!=USS_DEDICATED_KEYBOARD && sample->page!=USS_DEDICATED_CONTROLLER && sample->page!=USS_DEDICATED_ANY))return 0;
    memset(capture,0,sizeof(*capture));capture->host=sample->host;capture->epoch=sample->epoch;
    capture->start_ms=capture->last_ms=sample->now_ms;capture->deadline_ms=sample->now_ms+15000;
    capture->page=sample->page;capture->phase=USS_DEDICATED_ARMING;settings->release_required=1;return 1;
}
static inline unsigned uss_dedicated_capture_step(UssDedicatedCapture* capture,UssSettingsState* settings,
    const UssDedicatedCaptureSample* sample)
{
    unsigned i,count=0,choice_page=0;uint32_t choice=0;UssSettings draft;
    if(!capture || capture->phase==USS_DEDICATED_IDLE)return USS_DEDICATED_WAIT;
    if(!settings || !sample || settings->external || settings->provider || !sample->valid || !sample->foreground ||
        sample->host!=capture->host || sample->epoch!=capture->epoch || sample->page!=capture->page ||
        sample->now_ms<capture->last_ms || sample->now_ms>capture->deadline_ms ||
        (sample->controller&~UINT32_C(0x3f3ff)))return uss_dedicated_capture_cancel(capture,settings);
    capture->last_ms=sample->now_ms;
    if(capture->phase==USS_DEDICATED_ARMING){
        /* Require all opening/held inputs released, including the other device.
         * The activating click/key can never choose its own new binding. */
        if(!uss_dedicated_capture_any(sample))capture->phase=USS_DEDICATED_LISTENING;
        return USS_DEDICATED_WAIT;
    }
    if(sample->keys[27] && !capture->previous[27])return uss_dedicated_capture_cancel(capture,settings);
    if(capture->phase==USS_DEDICATED_RELEASING){
        if(uss_dedicated_capture_any(sample))return USS_DEDICATED_WAIT;
        draft=settings->pending_present?settings->pending:settings->current;
        if(capture->choice_page==USS_DEDICATED_KEYBOARD){draft.utility_key=capture->choice;
            if(capture->page==USS_DEDICATED_ANY)draft.utility_controller=0;}
        else {draft.utility_controller=capture->choice;
            if(capture->page==USS_DEDICATED_ANY)draft.utility_key=0;}
        if(!uss_settings_queue(settings,&draft))return uss_dedicated_capture_cancel(capture,settings);
        settings->release_required=1;capture->phase=USS_DEDICATED_IDLE;return USS_DEDICATED_QUEUED;
    }
    if(capture->page==USS_DEDICATED_KEYBOARD || capture->page==USS_DEDICATED_ANY){
        for(i=1;i<255;++i){
            /* Generic Shift/Ctrl/Alt duplicate their left/right Windows keys. */
            if(i==16 || i==17 || i==18)continue;
            if(sample->keys[i] && !capture->previous[i]){choice=i;choice_page=USS_DEDICATED_KEYBOARD;++count;}
        }
    }
    if(capture->page==USS_DEDICATED_CONTROLLER || capture->page==USS_DEDICATED_ANY){
        uint32_t pressed=sample->controller&~capture->previous_controller;
        for(i=0;i<18;++i)if(pressed&(1u<<i)){choice=1u<<i;choice_page=USS_DEDICATED_CONTROLLER;++count;}
    }
    memcpy(capture->previous,sample->keys,sizeof(capture->previous));capture->previous_controller=sample->controller;
    if(count>1)return uss_dedicated_capture_cancel(capture,settings); /* Single-key/button contract; never silently truncate chords. */
    if(count==1){
        draft=settings->pending_present?settings->pending:settings->current;
        if(choice_page==USS_DEDICATED_KEYBOARD){draft.utility_key=choice;if(capture->page==USS_DEDICATED_ANY)draft.utility_controller=0;}
        else {draft.utility_controller=choice;if(capture->page==USS_DEDICATED_ANY)draft.utility_key=0;}
        if(!uss_settings_valid(&draft))return uss_dedicated_capture_cancel(capture,settings);
        capture->choice=choice;capture->choice_page=choice_page;capture->phase=USS_DEDICATED_RELEASING;
    }
    return USS_DEDICATED_WAIT;
}
#endif
