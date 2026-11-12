#include <assert.h>
#include <stdio.h>
#include "../src/uss_dedicated_input_windows.h"
static unsigned focused=1,key_calls,pad_mask=1,fail_pad,focus_calls,lose_focus;
static int foreground(void* user){(void)user;++focus_calls;return (int)(focused && !(lose_focus && focus_calls%2==0));}
static SHORT WINAPI key_state(int key){++key_calls;return key==75?(SHORT)-32768:0;}
static DWORD WINAPI controller(DWORD index,XINPUT_STATE* state){
    if(fail_pad)return ERROR_GEN_FAILURE;
    if(!(pad_mask&(1u<<index)))return ERROR_DEVICE_NOT_CONNECTED;
    state->Gamepad.wButtons=XINPUT_GAMEPAD_A;
    state->Gamepad.bLeftTrigger=XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
    state->Gamepad.bRightTrigger=XINPUT_GAMEPAD_TRIGGER_THRESHOLD+1;
    return ERROR_SUCCESS;
}
int main(void){
    UssDedicatedWindows input={0};UssDedicatedCaptureSample sample;
    input.foreground=foreground;input.key_state=key_state;input.get_state=controller;
    assert(!uss_dedicated_windows_sample(&input,1,2,100,1,&sample) && !sample.valid);
    assert(uss_dedicated_windows_sample(&input,1,2,101,1,&sample));
    assert(sample.keys[75] && sample.controller==(XINPUT_GAMEPAD_A|USS_BIND_RIGHT_TRIGGER));
    assert(key_calls==508 && sample.host==1 && sample.epoch==2 && sample.now_ms==101);
    pad_mask=0;assert(!uss_dedicated_windows_sample(&input,1,2,102,1,&sample) && !sample.valid);
    assert(uss_dedicated_windows_sample(&input,1,2,103,1,&sample) && !sample.controller);
    focused=0;key_calls=0;assert(!uss_dedicated_windows_sample(&input,1,2,104,1,&sample) && !key_calls);
    focused=1;fail_pad=1;assert(!uss_dedicated_windows_sample(&input,1,2,105,1,&sample) && !sample.valid);
    fail_pad=0;lose_focus=1;focus_calls=0;
    assert(!uss_dedicated_windows_sample(&input,1,2,106,1,&sample) && !sample.valid);
    assert(!uss_dedicated_windows_sample(&input,1,2,107,4,&sample));
    input.get_state=NULL;assert(!uss_dedicated_windows_sample(&input,1,2,108,1,&sample));
    puts("USS capture producer: injected input, trigger threshold, focus loss, hotplug and OS failure PASS");return 0;
}
