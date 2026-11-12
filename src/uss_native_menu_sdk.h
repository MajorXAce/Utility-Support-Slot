/* Concrete SDK/Windows readers for native menu observation and code pins.
 * Construct on the validated game callback thread, not from DLL initialization.
 * Reader construction performs no effects. The explicit writer below touches
 * only already writable private menu fields, never protection changes/hooks. */
#ifndef USS_NATIVE_MENU_SDK_H
#define USS_NATIVE_MENU_SDK_H
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "sdk_compat.h"
#include "uss_native_menu_observation.h"
#include "uss_native_menu_entries.h"
#include "uss_native_menu_rows.h"
typedef struct UssMenuSdkContext {
    const HfApi* api;uintptr_t base;size_t module_size;DWORD thread;uint64_t epoch;
} UssMenuSdkContext;
static uint64_t uss_menu_sdk_scope(void* user)
{
    UssMenuSdkContext* c=(UssMenuSdkContext*)user;size_t size=0;const char* build;
    if(!c || !uss_sdk_compatible(c->api) || !c->api->read || !c->api->module_base || !c->api->build_id ||
        !c->epoch || c->thread!=GetCurrentThreadId() || !uss_menu_pointer(c->base) || c->module_size<0x347cf20 ||
        c->module_size>0x20000000 || c->base>UINTPTR_MAX-c->module_size)return 0;
    build=c->api->build_id();
    return build && !strcmp(build,"25480438") && c->api->module_base("game.dll",&size)==c->base &&
        size==c->module_size?c->epoch:0;
}
static int uss_menu_sdk_read(void* user,uintptr_t at,void* out,size_t length)
{
    UssMenuSdkContext* c=(UssMenuSdkContext*)user;
    return uss_menu_sdk_scope(c) && out && length && length<=512 && at>=0x10000 && at<=UINTPTR_MAX-length &&
        c->api->read(at,out,length)==0; /* SDK success is ZERO; observer success is ONE. */
}
static int uss_menu_sdk_language(void* user)
{
    UssMenuSdkContext* c=(UssMenuSdkContext*)user;uintptr_t manager=0,again=0;unsigned char switching=1;
    /* Retained HUD+ field-layout reference, exact supported game build. Unknown
     * pointer/read/transition blocks entry rather than claiming language stable. */
    if(!uss_menu_sdk_read(c,c->base+55037640,&manager,8) || !uss_menu_pointer(manager) ||
        manager>UINTPTR_MAX-106676 || !uss_menu_sdk_read(c,manager+106676,&switching,1) ||
        !uss_menu_sdk_read(c,c->base+55037640,&again,8) || again!=manager)return -1;
    return switching?1:0;
}
static int uss_menu_sdk_read_code(void* user,uintptr_t at,unsigned char* out,size_t length)
{
    UssMenuSdkContext* c=(UssMenuSdkContext*)user;MEMORY_BASIC_INFORMATION before={0},after={0};DWORD protection;
    unsigned char bytes[64];int read_result;
    if(!uss_menu_sdk_scope(c) || !out || !length || length>64 || at<c->base || at-c->base>c->module_size ||
        length>c->module_size-(at-c->base) || VirtualQuery((const void*)at,&before,sizeof(before))!=sizeof(before))return 0;
    protection=before.Protect;
    if(before.State!=MEM_COMMIT || before.Type!=MEM_IMAGE || (uintptr_t)before.AllocationBase!=c->base ||
        !(protection==PAGE_EXECUTE || protection==PAGE_EXECUTE_READ || protection==PAGE_EXECUTE_READWRITE || protection==PAGE_EXECUTE_WRITECOPY) ||
        at<(uintptr_t)before.BaseAddress || (uintptr_t)before.BaseAddress>UINTPTR_MAX-before.RegionSize ||
        length>(uintptr_t)before.BaseAddress+before.RegionSize-at)return 0;
    /* SDK13 exposes bytes before HellForge-managed hooks, not arbitrary patches.
     * Native entry resolver still compares every required pin. Failure of an
     * available read_code is final; never retry via ordinary read. */
    read_result=uss_sdk_has_read_code(c->api)?c->api->read_code(at,bytes,length):c->api->read(at,bytes,length);
    if(read_result!=0 || VirtualQuery((const void*)at,&after,sizeof(after))!=sizeof(after) ||
        memcmp(&before,&after,sizeof(before)) || !uss_menu_sdk_scope(c))return 0;
    memcpy(out,bytes,length);return 1;
}
static inline UssMenuObservationIo uss_menu_sdk_observer(UssMenuSdkContext* context)
{
    UssMenuObservationIo io={context,context->base,context->module_size,uss_menu_sdk_scope,uss_menu_sdk_language,uss_menu_sdk_read};return io;
}
static inline UssMenuEntryReader uss_menu_sdk_entries(UssMenuSdkContext* context)
{UssMenuEntryReader io={context,uss_menu_sdk_scope,uss_menu_sdk_read_code};return io;}

/* A current menu owner supplies the stage-aware proof. It must verify roots,
 * retained text/row identity and executable pins, including nominated partial
 * construction states; this writer never invents that proof from address alone.
 * One context is owned by the game callback thread for its entire lifetime. */
typedef struct UssMenuSdkWriter {
    UssMenuSdkContext* sdk;
    UssMenuHostSnapshot host;
    void* owner;
    int (*owned)(void*,const UssMenuHostSnapshot*);
    unsigned stopped,attempts,entered;
} UssMenuSdkWriter;
static inline int uss_menu_sdk_field(const UssMenuHostSnapshot* host,uintptr_t at,size_t length)
{
    uintptr_t panel,bar;unsigned i;
    if(!host || !uss_menu_same_host(host,host) || !uss_menu_pointer((uintptr_t)host->screen) ||
        host->screen>UINTPTR_MAX-5000000 || host->content!=host->screen+3010456 ||
        host->container!=host->content+1150848+544)return 0;
    bar=(uintptr_t)host->screen+1248;panel=(uintptr_t)host->content+1150848;
    if(length==1){
        if(at==(uintptr_t)host->content+1319449)return 1; /* Native UNAPPLIED flag. */
        for(i=0;i<8;++i)if(at==bar+3400*i+11021)return 1;
        return 0;
    }
    if(length!=4)return 0;
    if(at==(uintptr_t)host->screen+8 || at==(uintptr_t)host->content+1318492 ||
        at==(uintptr_t)host->content+1318500 || at==(uintptr_t)host->content+1318476 ||
        at==panel+2832 || at==panel+2836 || at==panel+2840 || at==panel+2844 || at==panel+2872)return 1;
    for(i=0;i<8;++i)if(at==bar+3400*i+11004)return 1;
    return 0;
}
static inline int uss_menu_sdk_writable(uintptr_t at,size_t length,MEMORY_BASIC_INFORMATION* page)
{
    uintptr_t end;
    if(!length || at<0x10000 || at>UINTPTR_MAX-length || !page ||
        VirtualQuery((const void*)at,page,sizeof(*page))!=sizeof(*page))return 0;
    if(page->State!=MEM_COMMIT || page->Type!=MEM_PRIVATE || page->Protect!=PAGE_READWRITE ||
        (uintptr_t)page->BaseAddress>UINTPTR_MAX-page->RegionSize)return 0;
    end=(uintptr_t)page->BaseAddress+page->RegionSize;
    return at>=(uintptr_t)page->BaseAddress && at<=end && length<=end-at;
}
static inline int uss_menu_sdk_writer_guard(UssMenuSdkWriter* writer)
{
    return writer && !writer->stopped && writer->sdk && writer->owned &&
        uss_menu_sdk_scope(writer->sdk)==writer->host.epoch &&
        writer->owned(writer->owner,&writer->host) &&
        uss_menu_sdk_scope(writer->sdk)==writer->host.epoch;
}
/* A failed preflight has not entered WriteProcessMemory. After entry, failure,
 * short write, changed page/context or failed readback permanently stops this
 * writer: no rollback/retry of an uncertain effect. No generic SDK write API. */
static inline int uss_menu_sdk_write_field(void* user,uintptr_t at,const void* data,size_t length)
{
    UssMenuSdkWriter* writer=(UssMenuSdkWriter*)user;
    MEMORY_BASIC_INFORMATION before={0},after={0};unsigned char copy[4],actual[4];SIZE_T written=0;
    if(!writer || !data || !uss_menu_sdk_field(&writer->host,at,length) ||
        !uss_menu_sdk_writer_guard(writer) || !uss_menu_sdk_writable(at,length,&before))return 0;
    memcpy(copy,data,length);
    /* Ownership validation can call readers; repeat page/scope checks after it. */
    if(!uss_menu_sdk_writer_guard(writer) || !uss_menu_sdk_writable(at,length,&after) ||
        memcmp(&before,&after,sizeof(before)))return 0;
    ++writer->attempts;writer->entered=1;
    if(!WriteProcessMemory(GetCurrentProcess(),(void*)at,copy,length,&written) || written!=length ||
        !uss_menu_sdk_writable(at,length,&after) || memcmp(&before,&after,sizeof(before)) ||
        !uss_menu_sdk_writer_guard(writer) || !uss_menu_sdk_read(writer->sdk,at,actual,length) || memcmp(copy,actual,length) || !uss_menu_sdk_writer_guard(writer)){
        writer->stopped=1;return 0;
    }
    return 1;
}

static int uss_menu_sdk_writer_read(void* user,uintptr_t at,void* out,size_t length)
{
    UssMenuSdkWriter* writer=(UssMenuSdkWriter*)user;
    return writer && !writer->stopped && uss_menu_sdk_read(writer->sdk,at,out,length);
}
static int uss_menu_sdk_rows_guard(void* user,const UssMenuHostSnapshot* expected,unsigned built)
{
    UssMenuSdkWriter* writer=(UssMenuSdkWriter*)user;(void)built;
    /* owned() uses the owning lifecycle's stage, not a caller's built claim. */
    return writer && expected && uss_menu_same_host(&writer->host,expected) && uss_menu_sdk_writer_guard(writer);
}
static inline UssMenuRowsIo uss_menu_sdk_rows_io(UssMenuSdkWriter* writer,const UssNativeMenuEntries* entries)
{
    UssMenuRowsIo io={0};
    if(writer && entries){io.user=writer;io.guard=uss_menu_sdk_rows_guard;
        io.read=uss_menu_sdk_writer_read;io.write=uss_menu_sdk_write_field;io.native=*entries;}
    return io; /* Explicit binding only; no guard, registration, write or native call. */
}
#endif
