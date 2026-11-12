#include <assert.h>
#include "../src/uss_vpm_menus.h"
static unsigned option_calls,binding_calls,ready_value=1,option_ok=1,binding_ok=1;
static int input_value;static double applied=1.0;
static int ready(void){return (int)ready_value;}
static int option(const char* id,const UssVpmOption* d,const char** error)
{
    (void)error;++option_calls;assert(!strcmp(id,USS_VPM_MODE_ID));
    assert(!strcmp(d->type,"choice") && !strcmp(d->label,"Binding Mode"));
    assert(d->choice_count==2 && !strcmp(d->choices[1],"Dedicated") && d->default_value==2.0);
    assert(!d->range_flags[0] && !d->range[0] && !d->final_flag);return (int)option_ok;
}
static int binding(const char* id,const UssVpmBinding* d,const char** error)
{(void)error;++binding_calls;assert(!strcmp(id,USS_VPM_BINDING_ID));assert(!strcmp(d->label,"Utility Weapon") && !d->slot);return (int)binding_ok;}
static int get(const char* id,double* value){assert(!strcmp(id,USS_VPM_MODE_ID));*value=applied;return 1;}
static int down(const char* id){assert(!strcmp(id,USS_VPM_BINDING_ID));return input_value;}
int main(void)
{
    UssVpmOptionsApi options={1,56,112,64,option,get,NULL,NULL,ready};
    UssVpmBindingsApi bindings={1,56,388,0,binding,down,NULL,NULL,ready};
    UssVpmMenus m={0};UssMenuProviderSample sample;UssMenuHandover h;UssMenuHandoverView v;
    UssSettings local=uss_settings_defaults();
    assert(!uss_vpm_menus_bind(&m,&options,NULL,1,2));assert(!m.provider);
    options.size--;assert(!uss_vpm_menus_bind(&m,&options,&bindings,1,2));options.size++;
    assert(uss_vpm_menus_bind(&m,&options,&bindings,1,2));
    ready_value=0;assert(!uss_vpm_menus_sample(&m,&sample) && !option_calls && !binding_calls);
    ready_value=1;assert(uss_vpm_menus_sample(&m,&sample));assert(option_calls==1 && binding_calls==1);
    assert(uss_menu_provider_valid(&sample) && sample.mode==1 && !sample.utility_down);
    uss_menu_handover_init(&h);v=uss_menu_handover_step(&h,&sample,1,0,0,&local,0);assert(v.external);
    applied=2;input_value=1;assert(uss_vpm_menus_sample(&m,&sample));
    v=uss_menu_handover_step(&h,&sample,2,0,0,&local,0);assert(v.mode==2 && !v.utility_down);
    input_value=0;assert(uss_vpm_menus_sample(&m,&sample));uss_menu_handover_step(&h,&sample,3,0,0,&local,0);
    input_value=1;assert(uss_vpm_menus_sample(&m,&sample));v=uss_menu_handover_step(&h,&sample,4,0,0,&local,0);assert(v.utility_down);
    assert(option_calls==1 && binding_calls==1);input_value=-1;assert(uss_vpm_menus_sample(&m,&sample) && sample.action_blocked && !sample.utility_down);
    v=uss_menu_handover_step(&h,&sample,5,0,0,&local,0);assert(v.external && !v.utility_down && v.release_required);
    input_value=2;assert(!uss_vpm_menus_sample(&m,&sample));
    applied=3;input_value=0;assert(!uss_vpm_menus_sample(&m,&sample));
    v=uss_menu_handover_step(&h,NULL,1006,0,0,&local,0);assert(!v.external && local.mode==1);
    memset(&m,0,sizeof(m));option_ok=0;assert(uss_vpm_menus_bind(&m,&options,&bindings,2,2));
    assert(!uss_vpm_menus_sample(&m,&sample));assert(!uss_vpm_menus_sample(&m,&sample));
    assert(option_calls==2 && binding_calls==2);assert(!strcmp(m.reason,"REGISTRATION_REFUSED"));
    puts("VPM adapter: recovered layout, readiness, once-only registration, applied values, semantic input and fallback PASS");return 0;
}
