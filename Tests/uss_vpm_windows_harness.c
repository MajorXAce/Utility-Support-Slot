#include <assert.h>
#include <stdio.h>
#include <windows.h>
static BOOL fixture_module(DWORD flags,LPCWSTR name,HMODULE* module);
static FARPROC fixture_export(HMODULE module,LPCSTR name);
#define GetModuleHandleExW fixture_module
#define GetProcAddress fixture_export
#include "../src/uss_vpm_windows.h"
static unsigned present=1,exports_present=1,api_ready=1,module_calls,export_calls,getter_calls,option_calls,binding_calls;
static int ready(void){return 1;}
static int option(const char* id,const UssVpmOption* spec,const char** error)
{(void)error;assert(!strcmp(id,USS_VPM_MODE_ID) && spec->choice_count==2);++option_calls;return 1;}
static int binding(const char* id,const UssVpmBinding* spec,const char** error)
{(void)error;assert(!strcmp(id,USS_VPM_BINDING_ID) && !strcmp(spec->label,"Utility Weapon"));++binding_calls;return 1;}
static int get(const char* id,double* value){assert(!strcmp(id,USS_VPM_MODE_ID));*value=2;return 1;}
static int down(const char* id){assert(!strcmp(id,USS_VPM_BINDING_ID));return 0;}
/* Authored future provider: extended tables, same version1 prefix. No file hash. */
static struct {UssVpmOptionsApi prefix;uint64_t extra[2];} options={{1,sizeof(options),112,64,option,get,NULL,NULL,ready},{1,2}};
static struct {UssVpmBindingsApi prefix;uint64_t extra[2];} bindings={{1,sizeof(bindings),388,0,binding,down,NULL,NULL,ready},{3,4}};
static const UssVpmOptionsApi* options_api(uint32_t version){assert(version==1);++getter_calls;return api_ready?&options.prefix:NULL;}
static const UssVpmBindingsApi* bindings_api(uint32_t version){assert(version==1);++getter_calls;return api_ready?&bindings.prefix:NULL;}
static BOOL fixture_module(DWORD flags,LPCWSTR name,HMODULE* module)
{assert(!flags && !wcscmp(name,L"vanilla_plus_megapack.dll"));++module_calls;if(!present)return FALSE;*module=(HMODULE)(uintptr_t)0x12340000;return TRUE;}
static FARPROC fixture_export(HMODULE module,LPCSTR name)
{
 union {FARPROC raw;const UssVpmOptionsApi* (*options)(uint32_t);const UssVpmBindingsApi* (*bindings)(uint32_t);} fn;
 assert(module==(HMODULE)(uintptr_t)0x12340000);++export_calls;if(!exports_present)return NULL;
 if(!strcmp(name,"vpm_mom_api"))fn.options=options_api;else {assert(!strcmp(name,"vpm_mbm_api"));fn.bindings=bindings_api;}return fn.raw;
}
int main(void)
{
 UssVpmWindows adapter={0};UssMenuProviderSample sample;unsigned before;
 assert(uss_vpm_windows_discover(&adapter,0,2) && !strcmp(adapter.reason,"BOUND"));
 assert(uss_vpm_menus_sample(&adapter.menus,&sample) && sample.mode==2 && uss_menu_provider_valid(&sample));
 assert(option_calls==1 && binding_calls==1);before=export_calls;
 assert(uss_vpm_windows_discover(&adapter,1,2) && export_calls==before && module_calls==1);
 options.prefix.size=sizeof(UssVpmOptionsApi);bindings.prefix.size=sizeof(UssVpmBindingsApi);memset(&adapter,0,sizeof(adapter));
 assert(uss_vpm_windows_discover(&adapter,0,2)); /* older exact-size API */
 options.prefix.version=2;memset(&adapter,0,sizeof(adapter));assert(!uss_vpm_windows_discover(&adapter,0,2) && !strcmp(adapter.reason,"API_UNAVAILABLE"));options.prefix.version=1;
 options.prefix.size=55;memset(&adapter,0,sizeof(adapter));assert(!uss_vpm_windows_discover(&adapter,0,2));options.prefix.size=56;
 bindings.prefix.down=NULL;memset(&adapter,0,sizeof(adapter));assert(!uss_vpm_windows_discover(&adapter,0,2));bindings.prefix.down=down;
 options.prefix.metadata0=0;options.prefix.metadata1=UINT32_MAX;bindings.prefix.metadata0=999;bindings.prefix.metadata1=17;
 memset(&adapter,0,sizeof(adapter));assert(uss_vpm_windows_discover(&adapter,0,2) && uss_vpm_menus_sample(&adapter.menus,&sample)); /* unused metadata may evolve */
 present=0;memset(&adapter,0,sizeof(adapter));before=module_calls;assert(!uss_vpm_windows_discover(&adapter,0,2));assert(!uss_vpm_windows_discover(&adapter,999,2) && module_calls==before+1);present=1;
 exports_present=0;memset(&adapter,0,sizeof(adapter));assert(!uss_vpm_windows_discover(&adapter,0,2) && !strcmp(adapter.reason,"EXPORTS_UNAVAILABLE"));before=export_calls;assert(!uss_vpm_windows_discover(&adapter,2000,2) && export_calls==before);exports_present=1;
 api_ready=0;memset(&adapter,0,sizeof(adapter));assert(!uss_vpm_windows_discover(&adapter,0,2) && !strcmp(adapter.reason,"WAIT_API"));api_ready=1;assert(uss_vpm_windows_discover(&adapter,1000,2));
 puts("VPM discovery: negotiated APIv1 exact/extended prefixes; direct registration, missing/truncated/incompatible contract refusal, retry/cache and absent-provider behavior PASS");return 0;
}
