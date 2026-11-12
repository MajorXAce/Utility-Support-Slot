/* USS binding editor lifecycle, adapted in design from CowboyBingus's 0BSD
 * ModBindingsMenu capture.lua. No input polling, native UI calls or game effects.
 * Engine adapters report logical device IDs rather than Windows virtual keys. */
#ifndef USS_BINDING_CAPTURE_H
#define USS_BINDING_CAPTURE_H
#include <stdint.h>
#include <string.h>

enum {USS_DEVICE_KEYBOARD=1,USS_DEVICE_MOUSE=2,USS_DEVICE_CONTROLLER=3};
enum {USS_CAPTURE_KEYBOARD_PAGE=1,USS_CAPTURE_CONTROLLER_PAGE=2};
enum {USS_CAPTURE_IDLE=0,USS_CAPTURE_ARMING,USS_CAPTURE_LISTENING,USS_CAPTURE_RELEASING};
enum {USS_CAPTURE_NONE=0,USS_CAPTURE_CHOSEN,USS_CAPTURE_CLEARED,USS_CAPTURE_CANCELLED};
enum {USS_CAPTURE_INPUT_CAP=5}; /* Main input plus at most four modifiers. */
typedef struct UssBindingInput {uint32_t device,button;} UssBindingInput;
typedef struct UssBindingChoice {
    unsigned count;UssBindingInput inputs[USS_CAPTURE_INPUT_CAP];
} UssBindingChoice;
typedef struct UssCaptureSample {
    uint64_t host,epoch,now_ms;
    unsigned valid,foreground,page,activator_down,back_pressed,clear_pressed;
    unsigned pressed_present,pressed_modifier,pressed_ignored;
    UssBindingInput pressed;
    unsigned modifier_count;UssBindingInput modifiers[4];
    /* Adapter checks these against capture.pending/choice in the same sample. */
    unsigned pending_down,chosen_inputs_down;
} UssCaptureSample;
typedef struct UssBindingCapture {
    unsigned phase,page,guard_owned,outcome,pending_present;
    uint64_t host,epoch,started_ms,deadline_ms;
    UssBindingInput pending;
    UssBindingChoice choice;
    unsigned release_required;
} UssBindingCapture;
static inline int uss_binding_input_valid(UssBindingInput input)
{return input.device>=USS_DEVICE_KEYBOARD && input.device<=USS_DEVICE_CONTROLLER && input.button<65536;}
static inline int uss_binding_input_equal(UssBindingInput a,UssBindingInput b)
{return a.device==b.device && a.button==b.button;}
static inline int uss_capture_device_on_page(unsigned page,unsigned device)
{return page==USS_CAPTURE_KEYBOARD_PAGE?(device==USS_DEVICE_KEYBOARD || device==USS_DEVICE_MOUSE):device==USS_DEVICE_CONTROLLER;}
/* Caller restores its Back guard whenever guard_owned transitions to zero.
 * Never restore into a replaced native screen merely because an old address exists. */
static inline void uss_binding_capture_cancel(UssBindingCapture* capture)
{
    if(!capture || capture->phase==USS_CAPTURE_IDLE)return;
    capture->phase=USS_CAPTURE_IDLE;capture->guard_owned=0;capture->pending_present=0;
    memset(&capture->choice,0,sizeof(capture->choice));capture->outcome=USS_CAPTURE_CANCELLED;capture->release_required=1;
}
static inline int uss_binding_capture_begin(UssBindingCapture* capture,const UssCaptureSample* sample,unsigned guard_acquired)
{
    if(!capture || !sample || capture->phase!=USS_CAPTURE_IDLE || !guard_acquired || !sample->valid ||
        !sample->foreground || !sample->host || !sample->epoch || !sample->now_ms ||
        sample->now_ms>UINT64_MAX-15000 || (sample->page!=USS_CAPTURE_KEYBOARD_PAGE && sample->page!=USS_CAPTURE_CONTROLLER_PAGE))return 0;
    memset(capture,0,sizeof(*capture));capture->phase=USS_CAPTURE_ARMING;capture->guard_owned=1;
    capture->page=sample->page;capture->host=sample->host;capture->epoch=sample->epoch;
    capture->started_ms=sample->now_ms;capture->deadline_ms=sample->now_ms+15000;
    capture->release_required=1;return 1;
}
static inline int uss_binding_capture_append(UssBindingChoice* choice,UssBindingInput input)
{
    unsigned i;if(!uss_binding_input_valid(input) || choice->count>=USS_CAPTURE_INPUT_CAP)return 0;
    for(i=0;i<choice->count;++i)if(uss_binding_input_equal(choice->inputs[i],input))return 1;
    choice->inputs[choice->count++]=input;return 1;
}
/* Return an outcome only after captured inputs release. Saving/application is the
 * settings owner's job; the capture itself never activates a gameplay binding. */
static inline unsigned uss_binding_capture_step(UssBindingCapture* capture,const UssCaptureSample* sample)
{
    unsigned i;if(!capture || capture->phase==USS_CAPTURE_IDLE)return USS_CAPTURE_NONE;
    if(!sample || !sample->valid || !sample->foreground || sample->host!=capture->host || sample->epoch!=capture->epoch ||
        sample->page!=capture->page || sample->now_ms<capture->started_ms || sample->now_ms>capture->deadline_ms ||
        sample->modifier_count>4){uss_binding_capture_cancel(capture);return USS_CAPTURE_CANCELLED;}
    if(sample->back_pressed){uss_binding_capture_cancel(capture);return USS_CAPTURE_CANCELLED;}
    if(capture->phase==USS_CAPTURE_ARMING){
        if(!sample->activator_down)capture->phase=USS_CAPTURE_LISTENING;
        return USS_CAPTURE_NONE; /* Never bind the input which opened the row. */
    }
    if(capture->phase==USS_CAPTURE_RELEASING){
        if(sample->chosen_inputs_down)return USS_CAPTURE_NONE;
        capture->phase=USS_CAPTURE_IDLE;capture->guard_owned=0;capture->pending_present=0;
        return capture->outcome;
    }
    if(sample->clear_pressed){
        memset(&capture->choice,0,sizeof(capture->choice));capture->outcome=USS_CAPTURE_CLEARED;
        capture->phase=USS_CAPTURE_RELEASING;return USS_CAPTURE_NONE;
    }
    if(sample->pressed_present && !sample->pressed_ignored){
        if(!uss_binding_input_valid(sample->pressed)){uss_binding_capture_cancel(capture);return USS_CAPTURE_CANCELLED;}
        if(uss_capture_device_on_page(capture->page,sample->pressed.device)){
            if(sample->pressed_modifier){
                if(!capture->pending_present){capture->pending=sample->pressed;capture->pending_present=1;}
            }else{
                memset(&capture->choice,0,sizeof(capture->choice));
                if(!uss_binding_capture_append(&capture->choice,sample->pressed)){uss_binding_capture_cancel(capture);return USS_CAPTURE_CANCELLED;}
                for(i=0;i<sample->modifier_count;++i){
                    UssBindingInput modifier=sample->modifiers[i];
                    unsigned expected=sample->pressed.device==USS_DEVICE_CONTROLLER?USS_DEVICE_CONTROLLER:USS_DEVICE_KEYBOARD;
                    if(modifier.device!=expected || !uss_binding_capture_append(&capture->choice,modifier)){
                        uss_binding_capture_cancel(capture);return USS_CAPTURE_CANCELLED;}
                }
                capture->outcome=USS_CAPTURE_CHOSEN;capture->phase=USS_CAPTURE_RELEASING;return USS_CAPTURE_NONE;
            }
        }
    }
    if(capture->pending_present && !sample->pending_down){
        memset(&capture->choice,0,sizeof(capture->choice));
        if(!uss_binding_capture_append(&capture->choice,capture->pending)){uss_binding_capture_cancel(capture);return USS_CAPTURE_CANCELLED;}
        capture->outcome=USS_CAPTURE_CHOSEN;capture->phase=USS_CAPTURE_RELEASING;
    }
    return USS_CAPTURE_NONE;
}
#endif
