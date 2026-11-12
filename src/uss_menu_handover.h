/* Provider-neutral menu policy. Called only on the owning game thread after a
 * transport has copied a sample. No Lua pointers, provider calls or native effects.
 * The provider owns its binding; its evaluated action is not a Windows key code. */
#ifndef USS_MENU_HANDOVER_H
#define USS_MENU_HANDOVER_H
#include "uss_settings.h"

enum { USS_MENU_HANDOVER_PROTOCOL=1, USS_MENU_HANDOVER_TIMEOUT_MS=1000 };
typedef struct UssMenuProviderSample {
    uint32_t protocol,size;
    uint64_t provider,sequence;
    unsigned options_registered,bindings_registered,ready,mode,input_valid,utility_down,action_blocked;
} UssMenuProviderSample;
typedef struct UssMenuHandover {
    uint64_t provider,sequence,received_ms,last_ms;
    unsigned have_clock,usable,external,mode,applied_mode,utility_down,release_required,action_blocked;
} UssMenuHandover;
typedef struct UssMenuHandoverView {
    unsigned external,mode,utility_down,release_required;
} UssMenuHandoverView;

/* No field in a malformed sample can refresh a provider's lease. Sequence is
 * strictly increasing within an instance; restart must use a new identity. */
static inline int uss_menu_provider_valid(const UssMenuProviderSample* sample)
{
    return sample && sample->protocol==USS_MENU_HANDOVER_PROTOCOL &&
        sample->size==sizeof(*sample) && sample->provider && sample->sequence &&
        sample->options_registered==1 && sample->bindings_registered==1 &&
        sample->ready==1 && sample->input_valid==1 && sample->utility_down<=1 && sample->action_blocked<=1 &&
        !(sample->action_blocked && sample->utility_down) &&
        (sample->mode==USS_BIND_DEFAULT || sample->mode==USS_BIND_DEDICATED);
}
static inline void uss_menu_handover_init(UssMenuHandover* handover)
{
    memset(handover,0,sizeof(*handover));handover->release_required=1;
}
/* Supply NULL if there is no NEW copied sample. A repeated sequence does not
 * count as a heartbeat. Time comes from the C receiver, not the provider. */
static inline UssMenuHandoverView uss_menu_handover_step(UssMenuHandover* handover,
    const UssMenuProviderSample* sample,uint64_t now_ms,unsigned transaction_active,
    unsigned input_blocked,const UssSettings* standalone,unsigned local_input_down)
{
    UssMenuHandoverView view={0,USS_BIND_DEFAULT,0,1};unsigned changed=0;
    if(!handover || !uss_settings_valid(standalone))return view;
    if(handover->have_clock && now_ms<handover->last_ms){
        handover->usable=0;handover->release_required=1;
    }else if(sample){
        if(!uss_menu_provider_valid(sample)){
            handover->usable=0;handover->release_required=1;
        }else if(sample->provider!=handover->provider || sample->sequence>handover->sequence){
            changed=sample->provider!=handover->provider || sample->mode!=handover->mode || !handover->usable;
            handover->provider=sample->provider;handover->sequence=sample->sequence;
            handover->received_ms=now_ms;handover->usable=1;
            handover->mode=sample->mode;handover->utility_down=sample->utility_down;handover->action_blocked=sample->action_blocked;
            if(changed)handover->release_required=1;
        }
    }
    handover->last_ms=now_ms;handover->have_clock=1;
    if(handover->usable && now_ms-handover->received_ms>USS_MENU_HANDOVER_TIMEOUT_MS){
        handover->usable=0;handover->release_required=1;
    }
    /* A menu/input source change must not interrupt an entered weapon operation.
     * Loss still suppresses external input immediately while fallback waits. */
    if(!transaction_active && handover->external!=handover->usable){
        handover->external=handover->usable;handover->release_required=1;
    }
    input_blocked=input_blocked || (handover->external && handover->action_blocked);
    if(input_blocked)handover->release_required=1;
    if(!transaction_active){
        unsigned next_mode=handover->external && handover->usable?handover->mode:standalone->mode;
        if(handover->applied_mode!=next_mode)handover->release_required=1;
        handover->applied_mode=next_mode;
    }
    view.external=handover->external;
    view.mode=handover->applied_mode?handover->applied_mode:standalone->mode;
    if(!input_blocked && handover->external==handover->usable &&
        (!handover->external || handover->mode==handover->applied_mode)){
        unsigned down=handover->external?handover->utility_down:local_input_down;
        if(handover->release_required){if(!down)handover->release_required=0;}
        else view.utility_down=down!=0;
    }
    view.release_required=handover->release_required;
    return view;
}
#endif
