#include "../src/uss_menu_capture_guard.h"
#define USS_MENU_LIFECYCLE_TEST_MAIN lifecycle_tests
#include "uss_native_menu_lifecycle_harness.c"
static unsigned tab_calls;
static uint32_t tab_next(uintptr_t bar,uintptr_t input,uintptr_t cursor)
{assert(bar && input && cursor==0x33000);++tab_calls;return 17;}
static unsigned char action_states[97*32];static unsigned consume_calls,fail_consume,bad_consume_pin;
static uint64_t codes[3];static unsigned abi_calls;
static void own_consume(uintptr_t input,uint64_t code,float seconds)
{assert(input==0x10000 && code==(UINT64_C(96)<<32) && seconds==-1.0f);++abi_calls;}
static int capture_read(void* user,uintptr_t at,void* out,size_t length)
{
    if(at>=0x23000000+808 && at-(0x23000000+808)<=sizeof(action_states) && length<=sizeof(action_states)-(at-(0x23000000+808))){
        memcpy(out,action_states+at-(0x23000000+808),length);return 1;}
    return read_memory(user,at,out,length);
}
static int capture_code(void* user,uintptr_t at,unsigned char* out,size_t length)
{
    if(at==0x10000000+0x12fde90 && length==20){memcpy(out,uss_menu_consume_pin,20);if(bad_consume_pin)out[0]^=1;return 1;}
    return code_read(user,at,out,length);
}
static int invoke_consume(void* user,uintptr_t entry,uintptr_t input,uint64_t code,float seconds)
{
    (void)user;assert(entry==0x10000000+0x12fde90 && input==0x23000000 && seconds==-1);
    assert(consume_calls<3);codes[consume_calls++]=code;return !fail_consume || consume_calls!=fail_consume;
}
static UssMenuCaptureGuard capture_fixture(void)
{
    UssMenuCaptureGuard guard={0};setup_lifecycle();assert(uss_menu_lifecycle_open(&lifecycle));
    memset(action_states,0,sizeof(action_states));consume_calls=fail_consume=bad_consume_pin=0;
    owner.observer.read=capture_read;owner.entry_reader.read_executable=capture_code;
    guard.owner=&owner;guard.invoke=invoke_consume;return guard;
}
int main(void)
{
    UssMenuCaptureGuard guard;unsigned before;
    (void)lifecycle_tests;
    assert(uss_menu_consume_invoke(NULL,(uintptr_t)own_consume,0x10000,UINT64_C(96)<<32,-1.0f) && abi_calls==1);
    assert(!uss_menu_consume_invoke(NULL,(uintptr_t)own_consume,0x10000,1,-1.0f) && abi_calls==1);
    guard=capture_fixture();assert(uss_menu_capture_guard_consume(&guard) && !consume_calls);
    action_states[32*1]=1;action_states[32*12]=1;action_states[32*96]=1;
    assert(uss_menu_capture_guard_consume(&guard) && consume_calls==3 && guard.consumed==3);
    assert(codes[0]==(UINT64_C(1)<<32) && codes[1]==(UINT64_C(12)<<32) && codes[2]==(UINT64_C(96)<<32));
    guard=capture_fixture();action_states[32*12]=1;fail_consume=1;
    assert(!uss_menu_capture_guard_consume(&guard) && guard.stopped && owner.stopped && consume_calls==1);
    before=consume_calls;assert(!uss_menu_capture_guard_consume(&guard) && consume_calls==before);
    guard=capture_fixture();bad_consume_pin=1;assert(!uss_menu_capture_guard_consume(&guard) && !consume_calls);
    guard=capture_fixture();text_identity(3010456+144000+3992,0xdead0000);
    assert(!uss_menu_capture_guard_consume(&guard) && !consume_calls);
    /* The shared handler keeps working outside capture and for every foreign
     * bar/input. Only our positively owned selected tab is suppressed. */
    {UssMenuTabInputCall fn=tab_next;void* next=NULL;uintptr_t bar,input;
     guard=capture_fixture();bar=owner.identity.screen+1248;input=owner.identity.input;
     uss_menu_put_word(screen,1248+57452,3);memcpy(&next,&fn,sizeof(next));
     assert(uss_menu_tab_input_dispatch(&next,&owner,0,bar,input,0x33000)==17 && tab_calls==1);
     assert(!uss_menu_tab_input_dispatch(&next,&owner,1,bar,input,0x33000) && tab_calls==1);
     assert(uss_menu_tab_input_dispatch(&next,&owner,1,bar+8,input,0x33000)==17 && tab_calls==2);
     assert(uss_menu_tab_input_dispatch(&next,&owner,1,bar,input+8,0x33000)==17 && tab_calls==3);
     uss_menu_put_word(screen,1248+57452,2);
     assert(uss_menu_tab_input_dispatch(&next,&owner,1,bar,input,0x33000)==17 && tab_calls==4);
     owner.stopped=1;
     assert(uss_menu_tab_input_dispatch(&next,&owner,1,bar,input,0x33000)==17 && tab_calls==5);
     fn=tab_next;memcpy(&next,&fn,sizeof(next));
     assert(uss_menu_tab_input_dispatch(&next,NULL,1,bar,input,0x33000)==17 && tab_calls==6);}
    puts("USS native capture guard: actual owned menu graph, Menu-only action encoding, until-release invocation, code/foreign/uncertain refusal and no retry PASS");return 0;
}
