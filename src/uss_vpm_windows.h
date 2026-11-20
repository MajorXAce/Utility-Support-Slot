/* Optional discovery: retain an already-loaded module; never LoadLibrary.
 * Compatibility follows the exported API version and required table prefix,
 * not pack release numbers or whole-file hashes. Discovery is bounded. */
#ifndef USS_VPM_WINDOWS_H
#define USS_VPM_WINDOWS_H
#include <windows.h>
#include "uss_vpm_menus.h"
typedef struct UssVpmWindows {
    HMODULE module;UssVpmMenus menus;
    unsigned options_present,options_version,options_size;
    uint64_t next_discovery;unsigned attempted;const char* reason;
} UssVpmWindows;
/* API getters negotiate version1. Retaining the loaded module keeps its code
 * mapped; component readiness is still checked separately on every sample. */
static inline int uss_vpm_windows_discover(UssVpmWindows* adapter,uint64_t now,unsigned mode)
{
    union {FARPROC raw;const UssVpmOptionsApi* (*get)(uint32_t);} options;
    union {FARPROC raw;const UssVpmBindingsApi* (*get)(uint32_t);} bindings;
    const UssVpmOptionsApi* mom;const UssVpmBindingsApi* mbm;
    if(!adapter)return 0;
    if(adapter->menus.provider){adapter->options_present=uss_vpm_options_valid(adapter->menus.options);return 1;}
    if(adapter->attempted==2)return 0;
    if(now<adapter->next_discovery)return 0;
    adapter->next_discovery=now+1000;
    if(!adapter->attempted){
        if(!GetModuleHandleExW(0,L"vanilla_plus_megapack.dll",&adapter->module))return 0;
        adapter->attempted=1;adapter->reason="WAIT_API";
    }
    options.raw=GetProcAddress(adapter->module,"vpm_mom_api");
    bindings.raw=GetProcAddress(adapter->module,"vpm_mbm_api");
    if(!options.raw && !bindings.raw){adapter->reason="EXPORTS_UNAVAILABLE";adapter->attempted=2;return 0;}
    mom=options.raw?options.get(1):NULL;mbm=bindings.raw?bindings.get(1):NULL;
    adapter->options_present=uss_vpm_options_valid(mom);
    adapter->options_version=adapter->options_present?mom->version:0;adapter->options_size=adapter->options_present?mom->size:0;
    if(!mom || !mbm){adapter->reason="WAIT_API";return 0;}
    if(!uss_vpm_menus_bind(&adapter->menus,mom,mbm,(uint64_t)(uintptr_t)adapter->module,mode)){
        adapter->reason="API_UNAVAILABLE";adapter->attempted=2;return 0;
    }
    adapter->reason="BOUND";return 1;
}
#endif
