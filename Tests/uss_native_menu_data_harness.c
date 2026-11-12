#include <assert.h>
#include <stdio.h>
#include "../src/uss_native_menu_data.h"
int main(void)
{
    unsigned char inert[216]={0};UssOptionDescriptor descriptor,previous;uint32_t choices[17];uintptr_t at;unsigned i;
    for(i=0;i<17;++i)choices[i]=100+i;
    uss_menu_put_word(inert,0,139);assert(uss_menu_inert_template(inert,sizeof(inert)));
    memset(&descriptor,0x5a,sizeof(descriptor));previous=descriptor;
    assert(!uss_menu_selector(&descriptor,inert,215,1,choices,2) && !memcmp(&descriptor,&previous,sizeof(descriptor)));
    assert(!uss_menu_selector(&descriptor,inert,216,1,choices,17));
    uss_menu_put_word(inert,12,1);assert(!uss_menu_selector(&descriptor,inert,216,1,choices,2));
    uss_menu_put_word(inert,12,0);uss_menu_put_word(inert,32,1);assert(!uss_menu_selector(&descriptor,inert,216,1,choices,2));
    uss_menu_put_word(inert,32,0);assert(uss_menu_selector(&descriptor,inert,216,1,choices,2));
    assert(uss_menu_word(descriptor.bytes,8)==2 && uss_menu_word(descriptor.bytes,12)==0);
    assert(uss_menu_word(descriptor.bytes,64)==2 && uss_menu_word(descriptor.bytes,72)==101);
    /* A dirty selector payload must never leak choices/pointers into an action row.
     * Refusing an invalid current template must preserve the caller's old value. */
    previous=descriptor;
    uss_menu_put_word(inert,12,1);
    assert(!uss_menu_assignment_descriptor(&descriptor,inert,sizeof(inert),1));
    assert(!memcmp(&descriptor,&previous,sizeof(descriptor)));
    uss_menu_put_word(inert,12,0);
    assert(!uss_menu_assignment_descriptor(&descriptor,inert,sizeof(inert),0));
    assert(!memcmp(&descriptor,&previous,sizeof(descriptor)));
    assert(uss_menu_assignment_descriptor(&descriptor,inert,sizeof(inert),123));
    assert(uss_menu_word(descriptor.bytes,0)==139 && uss_menu_word(descriptor.bytes,8)==5);
    assert(uss_menu_word(descriptor.bytes,12)==0 && uss_menu_word(descriptor.bytes,16)==123);
    for(i=20;i<sizeof(descriptor.bytes);++i)assert(descriptor.bytes[i]==0);
    assert(uss_menu_option_address(4096,31,&at) && at==4096+(uintptr_t)31*31464);
    assert(!uss_menu_option_address(4096,32,&at));assert(!uss_menu_option_address(UINTPTR_MAX,1,&at));
    assert(uss_menu_binding_address(4096,96,&at) && at==4096+(uintptr_t)96*24784);
    assert(!uss_menu_binding_address(4096,97,&at));assert(!uss_menu_binding_address(UINTPTR_MAX,1,&at));
    puts("USS native menu data: aligned selector descriptors, inert-template checks and distinct host row bounds PASS");return 0;
}
