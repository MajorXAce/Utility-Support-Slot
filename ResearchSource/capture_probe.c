#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <wchar.h>
#include "../src/capture_diagnostic.h"
_Static_assert(HF_MOD_SDK_VERSION>=USS_MIN_RUNTIME_SDK,"Reviewed SDK11 prefix required");
#ifndef HF_USS_CAPTURE_ENABLED
#define HF_USS_CAPTURE_ENABLED 0
#endif
static const HfApi* capture_api;
static UssCaptureDiagnostic capture_control;
static UssCaptureDiagnosticServices capture_services;
static wchar_t capture_folder[1024];
static atomic_uint capture_enabled;
static atomic_flag capture_busy=ATOMIC_FLAG_INIT;
static int capture_owned_path(wchar_t* out,const wchar_t* name)
{
    size_t folder=wcslen(capture_folder),length=wcslen(name);
    if(folder+length+1>=1024)return 0;
    memcpy(out,capture_folder,folder*sizeof(wchar_t));memcpy(out+folder,name,(length+1)*sizeof(wchar_t));return 1;
}
static int capture_write_session(void)
{
    wchar_t path[1024],temporary[1024];HANDLE file;DWORD written=0;int ok;
    if(!capture_owned_path(path,L"hf034-session.bin") || !capture_owned_path(temporary,L"hf034-session.tmp"))return 0;
    file=CreateFileW(temporary,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE)return 0;
    ok=WriteFile(file,&capture_control.session,sizeof(capture_control.session),&written,NULL) && written==sizeof(capture_control.session);
    if(!CloseHandle(file))ok=0;
    if(ok)ok=MoveFileExW(temporary,path,MOVEFILE_WRITE_THROUGH)!=0; /* Never replace a session ticket. */
    if(!ok)DeleteFileW(temporary);return ok;
}
static int capture_write_identity(void* user,const UssCaptureRequest* result)
{
    wchar_t path[1024],temporary[1024];HANDLE file;DWORD written=0;int ok;(void)user;
    if(!capture_owned_path(path,L"hf039-identity-result.bin") || !capture_owned_path(temporary,L"hf039-identity-result.tmp"))return 0;
    file=CreateFileW(temporary,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE)return 0;
    ok=WriteFile(file,result,sizeof(*result),&written,NULL) && written==sizeof(*result);
    if(!CloseHandle(file))ok=0;
    if(ok)ok=MoveFileExW(temporary,path,MOVEFILE_WRITE_THROUGH)!=0;
    if(!ok)DeleteFileW(temporary);return ok;
}
static uint64_t capture_clock(void* user){(void)user;return GetTickCount64();}
static uint32_t capture_thread(void* user){(void)user;return GetCurrentThreadId();}
static int capture_read_request(void* user,UssCaptureRequest* out)
{
    wchar_t path[1024];HANDLE file;LARGE_INTEGER size;DWORD received=0;int ok;
    (void)user;if(!capture_owned_path(path,capture_control.identity_attempted?L"hf034-request.bin":L"hf039-identity-request.bin"))return -1;
    file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE)return GetLastError()==ERROR_FILE_NOT_FOUND?0:-1;
    ok=GetFileSizeEx(file,&size) && size.QuadPart==sizeof(*out) &&
        ReadFile(file,out,sizeof(*out),&received,NULL) && received==sizeof(*out);
    if(!CloseHandle(file))ok=0;return ok?1:-1;
}
static void capture_tick(HfMod* mod,float dt,void* user)
{
    (void)dt;(void)user;
    if(!atomic_load_explicit(&capture_enabled,memory_order_acquire))return;
    if(atomic_flag_test_and_set_explicit(&capture_busy,memory_order_acquire))return;
    uss_capture_diagnostic_tick(&capture_control,capture_api,mod,&capture_services);
    atomic_flag_clear_explicit(&capture_busy,memory_order_release);
}
static int capture_prepare_platform(const HfApi* api,HfMod* mod)
{
    FILETIME created,exited,kernel,user;ULARGE_INTEGER started;size_t length;
    const char* folder=api->logs_folder(mod);
    if(!folder || !*folder || !MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,folder,-1,capture_folder,1024))return 0;
    length=wcslen(capture_folder);if(!length || length+2>=1024)return 0;
    if(capture_folder[length-1]!=L'\\' && capture_folder[length-1]!=L'/') {capture_folder[length++]=L'\\';capture_folder[length]=0;}
    if(!GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel,&user))return 0;
    started.LowPart=created.dwLowDateTime;started.HighPart=created.dwHighDateTime;
    memset(&capture_control,0,sizeof(capture_control));capture_control.session.magic=USS_CAPTURE_MAGIC;
    capture_control.session.version=USS_CAPTURE_VERSION;capture_control.session.pid=GetCurrentProcessId();
    capture_control.session.sdk=api->version;capture_control.session.build=USS_CAPTURE_BUILD;
    capture_control.session.process_started=started.QuadPart;
    if(BCryptGenRandom(NULL,(PUCHAR)&capture_control.session.nonce,8,BCRYPT_USE_SYSTEM_PREFERRED_RNG)!=0 ||
        !capture_control.session.nonce)return 0;
    capture_control.started_ms=GetTickCount64();capture_services.clock_ms=capture_clock;
    capture_services.thread_id=capture_thread;capture_services.write_identity=capture_write_identity;capture_services.read_request=capture_read_request;
    capture_services.platform=uss_capture_windows_platform();return capture_write_session();
}
#ifndef USS_CAPTURE_VERSION_TEXT
#define USS_CAPTURE_VERSION_TEXT "development"
#endif
HF_MOD_EXPORT int hellforge_mod_init(const HfApi* api,HfMod* mod)
{
    const char* build;
    if(!api || !mod || !uss_sdk_compatible(api) || !api->log)return 1;
    if(!HF_USS_CAPTURE_ENABLED){api->log(mod,"HF034 inactive");return 0;}
    if(!api->on_game_thread || !api->read || !api->module_base || !api->session ||
        !api->build_id || !api->game_state || !api->logs_folder)return 1;
    build=api->build_id();if(!build || strcmp(build,"25480438"))return 1;
    if(!capture_prepare_platform(api,mod)){api->log(mod,"HF034 REFUSED platform initialization");return 0;}
    capture_api=api;atomic_store_explicit(&capture_enabled,1,memory_order_release);
    if(!api->on_game_thread(mod,capture_tick,NULL)) {
        atomic_store_explicit(&capture_enabled,0,memory_order_release);
        api->log(mod,"HF034 TERMINAL registration failed; staying loaded/no retry");return 0;
    }
    api->log(mod,"HF034 WAIT_REQUEST version=" USS_CAPTURE_VERSION_TEXT " pid=%u started=%llu sdk=%u build=25480438 automatic_capture=0 native_calls=0 scope=CANDIDATE_PAIR",
        capture_control.session.pid,(unsigned long long)capture_control.session.process_started,api->version);
    return 0;
}

