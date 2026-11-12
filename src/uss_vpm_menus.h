/* Reconstructed Megapack0.2.1 menu ABI, Windows x64 only.
 * Evidence: Research/MENU_API_BINARY_REVIEW.md. No game addresses/callbacks.
 * Reserved fields stay zero; unused operations deliberately have no guessed ABI. */
#ifndef USS_VPM_MENUS_H
#define USS_VPM_MENUS_H
#include "uss_menu_handover.h"
typedef struct UssVpmOption {
    const char *type,*label,*mod_name,*mod_id,*description;
    const char* const* choices;
    int32_t choice_count,gap;
    double default_value;
    uint32_t range_flags[3],reserved;
    double range[3];
    uint32_t final_flag,padding;
} UssVpmOption;
typedef struct UssVpmBinding {
    const char* label;uint32_t text_id,padding;
    const char* mod_name;uint32_t slot,reserved;
} UssVpmBinding;
typedef struct UssVpmOptionsApi {
    uint32_t version,size,metadata0,metadata1;
    int (*register_option)(const char*,const UssVpmOption*,const char**);
    int (*get)(const char*,double*);
    void (*unused_set)(void);
    void (*unused_subscribe)(void);
    int (*ready)(void);
} UssVpmOptionsApi;
typedef struct UssVpmBindingsApi {
    uint32_t version,size,metadata0,metadata1;
    int (*register_binding)(const char*,const UssVpmBinding*,const char**);
    int (*down)(const char*);
    void (*unused_info)(void);
    void (*unused_revision)(void);
    int (*ready)(void);
} UssVpmBindingsApi;
_Static_assert(sizeof(void*)==8,"Recovered interface is x64 only");
_Static_assert(sizeof(UssVpmOption)==112,"Option layout");
_Static_assert(offsetof(UssVpmOption,choices)==40,"Choices offset");
_Static_assert(offsetof(UssVpmOption,default_value)==56,"Default offset");
_Static_assert(offsetof(UssVpmOption,final_flag)==104,"Final flag offset");
_Static_assert(sizeof(UssVpmBinding)==32,"Binding layout");
_Static_assert(offsetof(UssVpmBinding,slot)==24,"Slot offset");
_Static_assert(sizeof(UssVpmOptionsApi)==56 && sizeof(UssVpmBindingsApi)==56,"Table layout");
_Static_assert(offsetof(UssVpmOptionsApi,ready)==48,"Readiness slot");
typedef struct UssVpmMenus {
    const UssVpmOptionsApi* options;const UssVpmBindingsApi* bindings;
    UssVpmOption option;UssVpmBinding binding;
    uint64_t provider,sequence;
    unsigned option_attempted,binding_attempted,option_registered,binding_registered;
    const char* reason;
} UssVpmMenus;
#define USS_VPM_MODE_ID "majorxace.uss.binding_mode"
#define USS_VPM_BINDING_ID "majorxace.uss.utility_weapon"
static inline int uss_vpm_tables_valid(const UssVpmOptionsApi* options,const UssVpmBindingsApi* bindings)
{
    return options && bindings && options->version==1 && options->size==56 &&
        options->metadata0==112 && options->metadata1==64 &&
        bindings->version==1 && bindings->size==56 && bindings->metadata0==388 && !bindings->metadata1 &&
        options->register_option && options->get && options->ready &&
        bindings->register_binding && bindings->down && bindings->ready;
}
/* Stable session-owned descriptor storage, not a temporary stack registration.
 * No provider callback is registered. A refusal is attempted once per instance. */
static inline int uss_vpm_menus_bind(UssVpmMenus* menus,const UssVpmOptionsApi* options,
    const UssVpmBindingsApi* bindings,uint64_t provider,unsigned default_mode)
{
    static const char* const choices[]={"Default","Dedicated"};
    if(!menus || menus->provider || !provider || !uss_vpm_tables_valid(options,bindings) ||
        (default_mode!=USS_BIND_DEFAULT && default_mode!=USS_BIND_DEDICATED))return 0;
    memset(menus,0,sizeof(*menus));menus->options=options;menus->bindings=bindings;menus->provider=provider;
    menus->option.type="choice";menus->option.label="Binding Mode";
    menus->option.mod_name="Utility Support Slot";menus->option.mod_id="majorxace.uss";
    menus->option.description="Default uses the normal support weapon control. Dedicated uses Utility Weapon.";
    menus->option.choices=choices;menus->option.choice_count=2;menus->option.default_value=(double)default_mode;
    menus->binding.label="Utility Weapon";menus->binding.mod_name="Utility Support Slot";
    menus->reason="WAIT_READY";return 1;
}
/* Owning game thread only. Caller establishes the provider module's lifetime.
 * Read applied mode and evaluated input; never write provider values on a poll. */
static inline int uss_vpm_menus_sample(UssVpmMenus* menus,UssMenuProviderSample* output)
{
    double mode=0;int down;const char* error=NULL;
    if(!menus || !output || !menus->provider ||
        !uss_vpm_tables_valid(menus->options,menus->bindings))return 0;
    memset(output,0,sizeof(*output));
    if(menus->options->ready()!=1 || menus->bindings->ready()!=1){menus->reason="WAIT_READY";return 0;}
    if(!menus->option_attempted){
        menus->option_attempted=1;
        menus->option_registered=menus->options->register_option(USS_VPM_MODE_ID,&menus->option,&error)==1;
    }
    if(!menus->binding_attempted){
        menus->binding_attempted=1;
        menus->binding_registered=menus->bindings->register_binding(USS_VPM_BINDING_ID,&menus->binding,&error)==1;
    }
    if(!menus->option_registered || !menus->binding_registered){menus->reason="REGISTRATION_REFUSED";return 0;}
    if(menus->options->get(USS_VPM_MODE_ID,&mode)!=1 || (mode!=1.0 && mode!=2.0)){
        menus->reason="APPLIED_MODE_UNAVAILABLE";return 0;
    }
    down=menus->bindings->down(USS_VPM_BINDING_ID);
    if(down < -1 || down > 1){menus->reason="INPUT_UNAVAILABLE";return 0;}
    if(menus->sequence==UINT64_MAX){menus->reason="SEQUENCE_EXHAUSTED";return 0;}
    output->protocol=USS_MENU_HANDOVER_PROTOCOL;output->size=sizeof(*output);
    output->provider=menus->provider;output->sequence=++menus->sequence;
    output->options_registered=output->bindings_registered=output->ready=output->input_valid=1;
    output->mode=(unsigned)mode;output->utility_down=down==1;output->action_blocked=down==-1;
    menus->reason=down==-1?"INPUT_CONTEXT_UNAVAILABLE":"READY";return 1;
}
#endif
