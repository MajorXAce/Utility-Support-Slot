/* Finite two-phase diagnostic control. Services are own files/clock/binder only. */
#ifndef USS_SELECTION_CONTROL_H
#define USS_SELECTION_CONTROL_H
#include "guarded_selection.h"
#define USS_CONTROL_MAGIC UINT32_C(0x37324648)
#define USS_CONTROL_VERSION 1

typedef struct UssControlToken {
    uint32_t magic,version,pid,avatar;
    uint64_t process_started,nonce;
    uint32_t local_unit,reserved;
} UssControlToken;
_Static_assert(sizeof(UssControlToken)==40,"control control_token wire size");
typedef struct UssSelectionControl {
    unsigned phase; /* 0 request,1 arm,2 terminal */
    uint32_t thread;
    uint64_t started_ms,next_poll,prepared_ms;
    UssControlToken session,ready;
    UssActionState prepared;
} UssSelectionControl;
typedef struct UssControlServices {
    void* user;
    uint64_t (*clock_ms)(void*);
    uint32_t (*thread_id)(void*);
    /* Request=0/arm=1: absent0, exact token1, error/malformed-1. */
    int (*read_token)(void*,unsigned,UssControlToken*);
    int (*write_ready)(void*,const UssControlToken*);
    int (*bind_selection)(void*,const HfApi*,const UssActionState*,UssNativeCalls*);
} UssControlServices;
static int control_primary(const UssActionState* observation)
{
    const Inventory* inventory=&observation->inventory;
    return observation->valid && observation->switch_ready &&
        inventory->active_selector==1 && inventory->held==inventory->slots[0] &&
        valid_id(inventory->slots[0]) && valid_id(inventory->slots[1]) &&
        !valid_id(inventory->slots[2]);
}
static void control_refuse(UssSelectionControl* control,const HfApi* api,HfMod* mod,
                            const char* reason)
{
    control->phase=2;
    api->log(mod,"HF027 TERMINAL REFUSED reason=%s entered=0 returned=0",reason);
}
static void uss_selection_control_tick(UssSelectionControl* control,const HfApi* api,
                                       HfMod* mod,const UssControlServices* services)
{
    UssControlToken control_token,expected;
    UssActionState current;
    UssNativeCalls targets={0};
    UssSelectionAttempt selection_attempt={0};
    UssSelectionResult result;
    Reader diagnostics={0};
    uint64_t now;
    uint32_t thread;
    int present;
    if (control->phase==2) return;
    thread=services->thread_id(services->user);
    if (!thread || (control->thread && control->thread!=thread)) {
        control_refuse(control,api,mod,"CALLBACK_THREAD_CHANGED");return;
    }
    control->thread=thread;
    now=services->clock_ms(services->user);
    if (now<control->started_ms || now-control->started_ms>1800000 ||
        (control->phase==1 && (now<control->prepared_ms || now-control->prepared_ms>30000))) {
        control_refuse(control,api,mod,"EXPIRED");return;
    }
    if (now<control->next_poll) return;
    control->next_poll=now+250;
    present=services->read_token(services->user,control->phase,&control_token);
    if (!present) return;
    expected=control->phase ? control->ready : control->session;
    if (present!=1 || memcmp(&control_token,&expected,sizeof(control_token))) {
        control_refuse(control,api,mod,"TOKEN_MISMATCH_OR_IO");return;
    }
    /* No automatic retries even when preflight refuses the requested command. */
    if (control->phase==1) control->phase=2;
    if (api->game_state(mod)!=HF_STATE_MISSION) {
        control_refuse(control,api,mod,"MISSION_REQUIRED");return;
    }
    current=uss_observe_action_state(api,mod,&diagnostics);
    if (!control_primary(&current)) {
        control_refuse(control,api,mod,diagnostics.failure?diagnostics.failure:"PRIMARY_OR_LOADOUT");return;
    }
    if (!control->phase) {
        control->prepared=current;control->prepared_ms=now;
        control->ready=control->session;
        control->ready.avatar=current.avatar;control->ready.local_unit=current.inventory.local_unit;
        control->phase=1;
        if (!services->write_ready(services->user,&control->ready)) {
            control_refuse(control,api,mod,"READY_WRITE");return;
        }
        api->log(mod,"HF027 PREPARED avatar=%u unit=%u primary=%u secondary=%u ttl_ms=30000 callback_thread=%u",
            current.avatar,current.inventory.local_unit,current.inventory.slots[0],current.inventory.slots[1],thread);
        return;
    }
    if (!same_selection_context(&control->prepared,&current) ||
        !services->bind_selection(services->user,api,&current,&targets)) {
        control_refuse(control,api,mod,"CONTEXT_OR_ENTRY_PINS");return;
    }
    result=uss_select_primary_control(api,mod,&targets,&current,&selection_attempt);
    api->log(mod,"HF027 TERMINAL outcome=%u reason=%s entered=%u returned=%u index_verified=%u held_matches=%u preflight_reads=%u preflight_bytes=%u selection_reads=%u selection_bytes=%u delivery_proven=0 callback_thread=%u",
        (unsigned)result.outcome,result.reason,result.entered,result.calls_returned,
        result.outcome==USS_SELECTION_INDEX_VERIFIED,result.held_matches,
        diagnostics.calls,diagnostics.bytes,result.reads,result.bytes,thread);
}
#endif
