/* HF049: explicit finite formation/draw commands, idle until a fresh command_request. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <wchar.h>
#include "../src/formation_command.h"
#ifndef USS_FORMATION_VERSION_TEXT
#define USS_FORMATION_VERSION_TEXT "development"
#endif
static const HfApi* prototype_api;
static HfMod* prototype_mod;
static UssFormationNative prototype_native;
static UssFormationRuntime prototype_runtime;
static UssFormationTicket prototype_ticket;
static wchar_t prototype_folder[1024];
static unsigned prototype_sequence=1,prototype_terminal,prototype_initialized;
static uint64_t prototype_next;
static atomic_flag prototype_busy=ATOMIC_FLAG_INIT;
static uint64_t prototype_clock(void* user){(void)user;return GetTickCount64();}
static int prototype_path(wchar_t* out,const wchar_t* name)
{
    size_t n=wcslen(prototype_folder),m=wcslen(name);if(n+m+1>=1024)return 0;
    memcpy(out,prototype_folder,n*sizeof(wchar_t));memcpy(out+n,name,(m+1)*sizeof(wchar_t));return 1;
}
static void prototype_stop(const char* reason)
{
    prototype_terminal=1;
    prototype_api->log(prototype_mod,"HF049 TERMINAL reason=%s sequence=%u effects=%u no_retry=1",reason,prototype_sequence,prototype_native.effects);
}
static int prototype_request(UssFormationRequest* command_request)
{
    wchar_t name[64],path[1024];HANDLE file;LARGE_INTEGER size;DWORD received=0;int ok;
    if(swprintf(name,64,L"hf049-request-%02u.bin",prototype_sequence)<0 || !prototype_path(path,name))return -1;
    file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE)return GetLastError()==ERROR_FILE_NOT_FOUND?0:-1;
    ok=GetFileSizeEx(file,&size) && size.QuadPart==sizeof(*command_request) &&
        ReadFile(file,command_request,sizeof(*command_request),&received,NULL) && received==sizeof(*command_request);
    if(!CloseHandle(file))ok=0;return ok?1:-1;
}
static void prototype_tick(HfMod* mod,float dt,void* user)
{
    UssFormationRequest command_request;UssCoreFlags policy={0};UssFormationIo io;
    uint64_t now;int found;UssPairPhase phase;(void)mod;(void)dt;(void)user;
    if(prototype_terminal || atomic_flag_test_and_set_explicit(&prototype_busy,memory_order_acquire))return;
    now=GetTickCount64();if(now<prototype_next)goto done;
    prototype_next=now+(prototype_runtime.work.phase==USS_PAIR_ACTIVE?25:100);
    if(prototype_initialized && GetCurrentThreadId()!=prototype_runtime.thread){prototype_stop("CALLBACK_THREAD_CHANGED");goto done;}
    if(prototype_runtime.work.phase==USS_PAIR_ACTIVE){
        phase=uss_formation_tick(&prototype_runtime);
        if(phase==USS_PAIR_CANCELLED){prototype_stop(prototype_runtime.reason);goto done;}
        if(phase==USS_PAIR_COMPLETE)prototype_api->log(prototype_mod,"HF049 COMPLETE sequence=%u support=%u active=%u effects=%u sends=%llu",
            prototype_sequence-1,prototype_runtime.work.real,prototype_runtime.work.selected,prototype_native.effects,
            (unsigned long long)prototype_runtime.publication.returned);
        goto done;
    }
    if(prototype_sequence>16){prototype_stop("COMMAND_LIMIT");goto done;}
    found=prototype_request(&command_request);if(!found)goto done;
    if(found<0 || !uss_formation_request_valid(&prototype_ticket,&command_request,prototype_sequence,now)){
        prototype_stop("REQUEST_IDENTITY_SEQUENCE_OR_TTL");goto done;}
    ++prototype_sequence; /* Consumed before reads/effects; never replay a file. */
    if(!prototype_initialized){
        policy.known=policy.eligible=1;prototype_native.api=prototype_api;prototype_native.mod=prototype_mod;
        prototype_native.platform=uss_capture_windows_platform();prototype_native.runtime=&prototype_runtime;
        prototype_native.invoke=formation_native_invoke;io=uss_formation_native_io(&prototype_native,prototype_clock);
        if(!uss_formation_runtime_init(&prototype_runtime,&io,GetCurrentThreadId(),&policy)){prototype_stop("RUNTIME_INIT");goto done;}
        prototype_initialized=1;
    }
    if(!uss_formation_command(&prototype_native,command_request.action,command_request.switch_on_pickup,command_request.role)){
        prototype_stop("COMMAND_CONTEXT_OR_ORDER_REFUSED");goto done;}
    prototype_api->log(prototype_mod,"HF049 ACCEPTED sequence=%u action=%u phase=%u unit=%u avatar=%u support=%u effects=%u policy=OPERATOR_STANDING_PRIMARY",
        command_request.sequence,command_request.action,prototype_runtime.work.phase,prototype_runtime.latest.inventory.local_unit,
        prototype_runtime.latest.inventory.avatar,prototype_runtime.latest.inventory.slots[2],prototype_native.effects);
    prototype_next=now+25;
done:
    atomic_flag_clear_explicit(&prototype_busy,memory_order_release);
}
static int prototype_prepare(const HfApi* api,HfMod* mod)
{
    const char* folder=api->logs_folder(mod);size_t length;FILETIME created,exit,kernel,user;ULARGE_INTEGER start;
    wchar_t path[1024],temporary[1024];HANDLE file;DWORD written=0;int ok;
    if(!folder || !MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,folder,-1,prototype_folder,1024))return 0;
    length=wcslen(prototype_folder);if(!length || length+2>=1024)return 0;
    if(prototype_folder[length-1]!=0x5c && prototype_folder[length-1]!=L'/'){
        prototype_folder[length++]=0x5c;prototype_folder[length]=0;}
    if(!GetProcessTimes(GetCurrentProcess(),&created,&exit,&kernel,&user))return 0;
    start.LowPart=created.dwLowDateTime;start.HighPart=created.dwHighDateTime;
    prototype_ticket.magic=USS_FORMATION_MAGIC;prototype_ticket.version=1;prototype_ticket.pid=GetCurrentProcessId();
    prototype_ticket.sdk=api->version;prototype_ticket.process_started=start.QuadPart;prototype_ticket.build=25480438;
    if(BCryptGenRandom(NULL,(PUCHAR)&prototype_ticket.nonce,8,BCRYPT_USE_SYSTEM_PREFERRED_RNG)!=0 || !prototype_ticket.nonce)return 0;
    if(!prototype_path(path,L"hf049-session.bin") || !prototype_path(temporary,L"hf049-session.tmp"))return 0;
    file=CreateFileW(temporary,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE)return 0;
    ok=WriteFile(file,&prototype_ticket,sizeof(prototype_ticket),&written,NULL) && written==sizeof(prototype_ticket);
    if(!CloseHandle(file))ok=0;
    if(ok)ok=MoveFileExW(temporary,path,MOVEFILE_WRITE_THROUGH)!=0;
    if(!ok)DeleteFileW(temporary);return ok;
}
HF_MOD_EXPORT int hellforge_mod_init(const HfApi* api,HfMod* mod)
{
    const char* build;
    if(!api || !mod || !uss_sdk_compatible(api) || !api->log || !api->on_game_thread ||
        !api->read || !api->module_base || !api->session || !api->build_id || !api->game_state || !api->logs_folder)return 1;
    build=api->build_id();if(!build || strcmp(build,"25480438"))return 1;
    prototype_api=api;prototype_mod=mod;
    if(!prototype_prepare(api,mod)){api->log(mod,"HF049 TERMINAL platform initialization refused native_calls=0");return 0;}
    if(!api->on_game_thread(mod,prototype_tick,NULL)){prototype_stop("CALLBACK_REGISTRATION");return 0;}
    api->log(mod,"HF049 WAIT_REQUEST version=" USS_FORMATION_VERSION_TEXT " pid=%u started=%llu sdk=%u build=25480438 native_calls=0 automatic_pickups=0 input_bindings=0",
        prototype_ticket.pid,(unsigned long long)prototype_ticket.process_started,api->version);
    return 0;
}
