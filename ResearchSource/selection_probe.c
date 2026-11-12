#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <wchar.h>
#include "../src/selection_control.h"
#include "../src/selection_entry_pins.h"
#ifndef HF_USS_SELECTION_ENABLED
#define HF_USS_SELECTION_ENABLED 0
#endif
static const HfApi* selection_api;
static UssSelectionControl selection_control;
static UssControlServices selection_services;
static wchar_t selection_folder[1024];
static atomic_uint selection_enabled;
static atomic_flag selection_busy=ATOMIC_FLAG_INIT;

static int control_path(wchar_t* output,const wchar_t* name)
{
    size_t folder=wcslen(selection_folder),length=wcslen(name);
    if(folder+length+1>=1024) return 0;
    memcpy(output,selection_folder,folder*sizeof(wchar_t));
    memcpy(output+folder,name,(length+1)*sizeof(wchar_t));return 1;
}
static int write_token_file(const wchar_t* name,const UssControlToken* control_token)
{
    wchar_t final[1024],temporary[1024];HANDLE file;DWORD written=0;int ok;
    if(!control_path(final,name) || wcslen(final)+5>=1024) return 0;
    memcpy(temporary,final,(wcslen(final)+1)*sizeof(wchar_t));memcpy(temporary+wcslen(final),L".tmp",5*sizeof(wchar_t));
    file=CreateFileW(temporary,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE) return 0;
    ok=WriteFile(file,control_token,sizeof(*control_token),&written,NULL) && written==sizeof(*control_token);
    if(!CloseHandle(file)) ok=0;
    if(ok) ok=MoveFileExW(temporary,final,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
    if(!ok) DeleteFileW(temporary);
    return ok;
}
static uint64_t control_clock(void* user) {(void)user;return GetTickCount64();}
static uint32_t control_thread(void* user) {(void)user;return GetCurrentThreadId();}
static int read_control_token(void* user,unsigned phase,UssControlToken* output)
{
    wchar_t path[1024];HANDLE file;LARGE_INTEGER size;DWORD received=0;int ok;
    (void)user;
    if(phase>1 || !control_path(path,phase?L"hf027-arm.bin":L"hf027-request.bin")) return -1;
    file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                     NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE) return GetLastError()==ERROR_FILE_NOT_FOUND?0:-1;
    ok=GetFileSizeEx(file,&size) && size.QuadPart==sizeof(*output) &&
       ReadFile(file,output,sizeof(*output),&received,NULL) && received==sizeof(*output);
    if(!CloseHandle(file)) ok=0;
    return ok?1:-1;
}
static int write_control_ready(void* user,const UssControlToken* control_token)
{(void)user;return write_token_file(L"hf027-ready.bin",control_token);}
static int bind_control_selection(void* user,const HfApi* api,
                                   const UssActionState* snapshot,UssNativeCalls* output)
{
    Reader reader={0};UssSelectionAddresses addresses;uintptr_t base;size_t size=0;
    (void)user;reader.api=api;memset(output,0,sizeof(*output));
    base=api->module_base("game.dll",&size);
    if(base!=snapshot->game_base || size!=snapshot->game_size ||
       !uss_selection_entry_pins(&reader,base,size,&addresses)) return 0;
    base=api->module_base("game.dll",&size);
    if(base!=snapshot->game_base || size!=snapshot->game_size) return 0;
    /* Reviewed Windows x64 boundary only; byte pins do not prove the full ABI.
     * Binding occurs only after a fresh matching arm on the SDK game thread. */
    output->select_state=(UssSelectStateFn)addresses.state;
    output->select_weapon=(UssSelectWeaponFn)addresses.weapon;
    return 1;
}
static void selection_tick(HfMod* mod,float dt,void* user)
{
    (void)dt;(void)user;
    if(!atomic_load_explicit(&selection_enabled,memory_order_acquire)) return;
    if(atomic_flag_test_and_set_explicit(&selection_busy,memory_order_acquire)) return;
    uss_selection_control_tick(&selection_control,selection_api,mod,&selection_services);
    atomic_flag_clear_explicit(&selection_busy,memory_order_release);
}
static int prepare_control_platform(const HfApi* api,HfMod* mod)
{
    FILETIME created,exited,kernel,user;
    ULARGE_INTEGER started;
    const char* folder=api->logs_folder(mod);
    size_t length;
    if(!folder || !*folder || !MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,folder,-1,
                                                  selection_folder,1024)) return 0;
    length=wcslen(selection_folder);
    if(!length || length+2>=1024) return 0;
    if(selection_folder[length-1]!=L'\\' && selection_folder[length-1]!=L'/') {
        selection_folder[length++]=L'\\';selection_folder[length]=0;
    }
    if(!GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel,&user)) return 0;
    started.LowPart=created.dwLowDateTime;started.HighPart=created.dwHighDateTime;
    memset(&selection_control,0,sizeof(selection_control));
    selection_control.session.magic=USS_CONTROL_MAGIC;
    selection_control.session.version=USS_CONTROL_VERSION;
    selection_control.session.pid=GetCurrentProcessId();
    selection_control.session.process_started=started.QuadPart;
    if(BCryptGenRandom(NULL,(PUCHAR)&selection_control.session.nonce,sizeof(uint64_t),
                       BCRYPT_USE_SYSTEM_PREFERRED_RNG)!=0 || !selection_control.session.nonce) return 0;
    selection_control.started_ms=GetTickCount64();
    selection_services.user=NULL;selection_services.clock_ms=control_clock;
    selection_services.thread_id=control_thread;selection_services.read_token=read_control_token;
    selection_services.write_ready=write_control_ready;selection_services.bind_selection=bind_control_selection;
    return write_token_file(L"hf027-session.bin",&selection_control.session);
}
HF_MOD_EXPORT int hellforge_mod_init(const HfApi* api,HfMod* mod)
{
    const char* build;
    if(!api || !mod || api->version!=9 || api->size!=sizeof(HfApi) || !api->log) return 1;
    if(!HF_USS_SELECTION_ENABLED) {api->log(mod,"HF027 inactive");return 0;}
    if(!api->on_game_thread || !api->read || !api->module_base || !api->session ||
       !api->build_id || !api->game_state || !api->logs_folder) return 1;
    build=api->build_id();if(!build || strcmp(build,"25480438")) return 1;
    if(!prepare_control_platform(api,mod)) {api->log(mod,"HF027 REFUSED platform initialization");return 0;}
    selection_api=api;
    atomic_store_explicit(&selection_enabled,1,memory_order_release);
    if(!api->on_game_thread(mod,selection_tick,NULL)) {
        atomic_store_explicit(&selection_enabled,0,memory_order_release);
        api->log(mod,"HF027 TERMINAL registration failed; staying loaded/no retry");return 0;
    }
    api->log(mod,"HF027 WAIT_REQUEST pid=%u started=%llu automatic_action=0 target_slot=2",
        selection_control.session.pid,(unsigned long long)selection_control.session.process_started);
    return 0;
}
