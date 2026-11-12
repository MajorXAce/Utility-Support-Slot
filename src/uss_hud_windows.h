/* Concrete HUD record IO. Uses SDK exact reads and existing writable private
 * memory only; never calls SDK write (which may change page protection).
 * The Core producer must verify the event's pair/attachments and metadata. */
#ifndef USS_HUD_WINDOWS_H
#define USS_HUD_WINDOWS_H
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "sdk_compat.h"
#include "uss_hud_ack.h"
typedef struct UssHudWindows {
    const HfApi* api;uintptr_t base;size_t module_size;DWORD thread;
    void* user;
    int (*context)(void*,const UssHudEvent*,UssHudSnapshot*);
} UssHudWindows;
static inline int uss_hud_windows_scope(UssHudWindows* io)
{
    size_t size=0;const char* build;
    return io && uss_sdk_compatible(io->api) && io->api->read && io->api->module_base && io->api->build_id && io->context &&
        io->thread==GetCurrentThreadId() && io->base>=0x10000 && io->base<=UINTPTR_MAX-io->module_size && io->module_size>=0x346d540 && io->module_size<=0x20000000 &&
        io->api->module_base("game.dll",&size)==io->base && size==io->module_size &&
        (build=io->api->build_id()) && !strcmp(build,"25480438");
}
static inline int uss_hud_windows_private(uintptr_t root)
{
    MEMORY_BASIC_INFORMATION page;uintptr_t at,end;
    if(root<0x10000 || root>UINTPTR_MAX-0x394ff0)return 0;
    at=root+0x394cdc;end=at+32;
    return VirtualQuery((const void*)at,&page,sizeof(page))==sizeof(page) && page.State==MEM_COMMIT && page.Type==MEM_PRIVATE &&
        (page.Protect==PAGE_READWRITE || page.Protect==PAGE_WRITECOPY) && at>=(uintptr_t)page.BaseAddress &&
        (uintptr_t)page.BaseAddress<=UINTPTR_MAX-page.RegionSize && end<=(uintptr_t)page.BaseAddress+page.RegionSize;
}
static UssHudSnapshot uss_hud_windows_observe(void* user,const UssHudEvent* event)
{
    UssHudWindows* io=(UssHudWindows*)user;UssHudSnapshot s={0};uintptr_t root=0,again=0;unsigned char expected[40];
    if(!uss_hud_windows_scope(io) || !io->context(io->user,event,&s) || !s.context_verified || !s.metadata_verified){
        memset(&s,0,sizeof(s));return s;}
    memcpy(expected,s.record,40);
    if(
        io->api->read(io->base+0x346d538,&root,8)!=0 || root!=s.root || !uss_hud_windows_private(root) ||
        io->api->read(root+0x394cd8,s.record,40)!=0 ||
        (s.record_verified && memcmp(expected,s.record,40)) || io->api->read(root+0x394fdc,s.selection,24)!=0 ||
        io->api->read(io->base+0x346d538,&again,8)!=0 || root!=again || !uss_hud_windows_scope(io)){
        memset(&s,0,sizeof(s));return s;}
    s.active_index=uss_hud_word(s.selection,0);s.private_writable=1;s.valid=1;return s;
}
static int uss_hud_windows_write(void* user,const UssHudSnapshot* expected,unsigned offset,const unsigned char* bytes,unsigned length)
{
    UssHudWindows* io=(UssHudWindows*)user;uintptr_t root=0;unsigned char record[40];UssHudSnapshot context={0};SIZE_T written=0;
    if(!expected || !bytes || !uss_hud_windows_scope(io) ||
        !((offset==4 && length==12) || ((offset==16 || offset==32) && length==4)) ||
        !io->context(io->user,NULL,&context) || !context.context_verified || !context.metadata_verified ||
        context.epoch!=expected->epoch || context.owner!=expected->owner || context.inventory!=expected->inventory ||
        context.row!=expected->row || context.avatar!=expected->avatar || context.root!=expected->root ||
        (context.record_verified && memcmp(context.record,expected->record,40)) ||
        !uss_hud_windows_private(expected->root) || io->api->read(io->base+0x346d538,&root,8)!=0 || root!=expected->root ||
        io->api->read(root+0x394cd8,record,40)!=0 || memcmp(record,expected->record,40))return 0;
    return WriteProcessMemory(GetCurrentProcess(),(void*)(root+0x394cd8+offset),bytes,length,&written)!=0 && written==length;
}
static inline UssHudAckServices uss_hud_windows_services(UssHudWindows* io)
{UssHudAckServices s={io,uss_hud_windows_observe,uss_hud_windows_write};return s;}
#endif
