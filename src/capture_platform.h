/* Platform observations only; no memory write, module load, hook or game call. */
#ifndef USS_CAPTURE_PLATFORM_H
#define USS_CAPTURE_PLATFORM_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <windows.h>
typedef struct UssCapturePage {
    uintptr_t base,size,allocation;
    uint32_t kind,state,protection;
} UssCapturePage;
typedef struct UssCapturePlatform {
    void* user;
    int (*query)(void*,uintptr_t,UssCapturePage*);
    int (*path)(void*,uintptr_t,uint16_t*,size_t,size_t*);
    uintptr_t (*export_address)(void*,const char*);
    uint32_t (*thread_id)(void*);
} UssCapturePlatform;
static int capture_windows_query(void* user,uintptr_t address,UssCapturePage* out)
{
    MEMORY_BASIC_INFORMATION info; (void)user;
    if (VirtualQuery((const void*)address,&info,sizeof(info))!=sizeof(info)) return 0;
    memset(out,0,sizeof(*out));out->base=(uintptr_t)info.BaseAddress;out->size=info.RegionSize;
    out->allocation=(uintptr_t)info.AllocationBase;out->kind=info.Type;
    out->state=info.State;out->protection=info.Protect;return 1;
}
static int capture_windows_path(void* user,uintptr_t base,uint16_t* out,size_t capacity,size_t* length)
{
    DWORD count; (void)user;
    if (capacity<2 || capacity>2048) return 0;
    count=GetModuleFileNameW((HMODULE)base,(wchar_t*)out,(DWORD)capacity);
    if (!count || count>=capacity || count*2>4092) return 0;
    *length=count;return 1;
}
static uintptr_t capture_windows_export(void* user,const char* name)
{
    HMODULE kernel;(void)user;
    if (strcmp(name,"EnterCriticalSection") && strcmp(name,"LeaveCriticalSection") &&
        strcmp(name,"DeleteCriticalSection")) return 0;
    kernel=GetModuleHandleW(L"kernel32.dll");
    return kernel ? (uintptr_t)GetProcAddress(kernel,name) : 0;
}
static uint32_t capture_windows_thread(void* user){(void)user;return GetCurrentThreadId();}
static inline UssCapturePlatform uss_capture_windows_platform(void)
{
    UssCapturePlatform out={0};out.query=capture_windows_query;out.path=capture_windows_path;
    out.export_address=capture_windows_export;out.thread_id=capture_windows_thread;return out;
}
#endif
