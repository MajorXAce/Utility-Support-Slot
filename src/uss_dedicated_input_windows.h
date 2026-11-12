/* Bounded physical-input producer for standalone menu capture. The caller only
 * samples while capture is active. Injectable OS calls keep fixtures off hardware. */
#ifndef USS_DEDICATED_INPUT_WINDOWS_H
#define USS_DEDICATED_INPUT_WINDOWS_H
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <Xinput.h>
#include "uss_dedicated_capture.h"
typedef SHORT (WINAPI *UssCaptureKeyState)(int);
typedef int (*UssCaptureForeground)(void*);
typedef struct UssDedicatedWindows {
    void* user;
    UssCaptureForeground foreground;
    UssCaptureKeyState key_state;
    DWORD (WINAPI *get_state)(DWORD,XINPUT_STATE*);
    unsigned connection_known,connected;
    HMODULE module; /* Session retained, never unload while callbacks may sample. */
} UssDedicatedWindows;
static inline int uss_dedicated_windows_foreground(void* user)
{
    DWORD process=0;HWND window=GetForegroundWindow();(void)user;
    return window && GetWindowThreadProcessId(window,&process) && process==GetCurrentProcessId();
}
/* Explicit startup binding, never invoked by fixtures or header inclusion. */
static inline int uss_dedicated_windows_bind(UssDedicatedWindows* input)
{
    FARPROC symbol;
    if(!input || input->module || input->key_state || input->get_state)return 0;
    input->module=LoadLibraryExW(L"xinput1_4.dll",NULL,LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!input->module)return 0;
    symbol=GetProcAddress(input->module,"XInputGetState");
    memcpy(&input->get_state,&symbol,sizeof(input->get_state));
    if(!input->get_state)return 0;
    input->key_state=GetAsyncKeyState;input->foreground=uss_dedicated_windows_foreground;return 1;
}
static inline int uss_dedicated_windows_sample(UssDedicatedWindows* input,
    uint64_t host,uint64_t epoch,uint64_t now_ms,unsigned page,UssDedicatedCaptureSample* sample)
{
    unsigned key,connected=0;DWORD index;
    if(!sample)return 0;
    memset(sample,0,sizeof(*sample));
    sample->host=host;sample->epoch=epoch;sample->now_ms=now_ms;sample->page=page;
    if(!input || !input->foreground || !input->key_state || !input->get_state ||
       !host || !epoch || !now_ms || (page!=USS_DEDICATED_KEYBOARD && page!=USS_DEDICATED_CONTROLLER && page!=USS_DEDICATED_ANY))return 0;
    if(!input->foreground(input->user))return 0;
    sample->foreground=1;
    for(key=1;key<255;++key)sample->keys[key]=(unsigned char)(input->key_state((int)key)<0);
    for(index=0;index<XUSER_MAX_COUNT;++index){
        XINPUT_STATE state={0};DWORD status=input->get_state(index,&state);
        if(status==ERROR_DEVICE_NOT_CONNECTED)continue;
        if(status!=ERROR_SUCCESS)return 0;
        connected|=1u<<index;
        sample->controller|=state.Gamepad.wButtons&0xf3ffu;
        if(state.Gamepad.bLeftTrigger>XINPUT_GAMEPAD_TRIGGER_THRESHOLD)sample->controller|=USS_BIND_LEFT_TRIGGER;
        if(state.Gamepad.bRightTrigger>XINPUT_GAMEPAD_TRIGGER_THRESHOLD)sample->controller|=USS_BIND_RIGHT_TRIGGER;
    }
    /* A changed controller roster is an invalid sample, never a release edge. */
    if(!input->connection_known || connected!=input->connected){
        input->connection_known=1;input->connected=connected;return 0;
    }
    if(!input->foreground(input->user))return 0;
    sample->valid=1;return 1;
}
#endif
