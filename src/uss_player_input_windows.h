/* Windows focus and fallback dedicated binding sampler. Default support actions
 * always come from the game's semantic action records, never hard-coded keys. */
#ifndef USS_PLAYER_INPUT_WINDOWS_H
#define USS_PLAYER_INPUT_WINDOWS_H
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <Xinput.h>
#include "uss_player_input_sdk.h"
typedef DWORD (WINAPI *UssXInputGetState)(DWORD,XINPUT_STATE*);
typedef struct UssPlayerWindows {
    HMODULE xinput;UssXInputGetState get_state;unsigned attempted,connected,connection_known;
} UssPlayerWindows;
static inline int uss_player_windows_foreground(void* user)
{
    DWORD process=0;HWND window=GetForegroundWindow();(void)user;
    return window && GetWindowThreadProcessId(window,&process) && process==GetCurrentProcessId();
}
static inline int uss_player_windows_dedicated(void* user,const UssSettings* settings,unsigned* down)
{
    UssPlayerWindows* windows=(UssPlayerWindows*)user;unsigned controller_down=0,connected=0;DWORD index;
    if(!windows || !settings || !down || !uss_settings_valid(settings))return 0;
    *down=0;
    if(settings->utility_controller){
        if(!windows->attempted){
            windows->attempted=1;
            windows->xinput=LoadLibraryExW(L"xinput1_4.dll",NULL,LOAD_LIBRARY_SEARCH_SYSTEM32);
            if(windows->xinput){FARPROC symbol=GetProcAddress(windows->xinput,"XInputGetState");
                /* Windows function pointers share representation; memcpy avoids
                 * incompatible-pointer cast diagnostics in both compilers. */
                memcpy(&windows->get_state,&symbol,sizeof(windows->get_state));}
        }
        if(!windows->get_state)return 0;
        for(index=0;index<XUSER_MAX_COUNT;++index){
            XINPUT_STATE state={0};DWORD status=windows->get_state(index,&state);
            if(status==ERROR_DEVICE_NOT_CONNECTED)continue;
            if(status!=ERROR_SUCCESS)return 0;
            connected|=1u<<index;
            if(settings->utility_controller==USS_BIND_LEFT_TRIGGER)controller_down|=state.Gamepad.bLeftTrigger>XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
            else if(settings->utility_controller==USS_BIND_RIGHT_TRIGGER)controller_down|=state.Gamepad.bRightTrigger>XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
            else controller_down|=(state.Gamepad.wButtons&settings->utility_controller)!=0;
        }
        if(!windows->connection_known || connected!=windows->connected){
            windows->connected=connected;windows->connection_known=1;return 0; /* Hotplug requires a release baseline. */
        }
    }
    *down=((settings->utility_key && GetAsyncKeyState((int)settings->utility_key)<0)) || controller_down;return 1;
}
#endif
