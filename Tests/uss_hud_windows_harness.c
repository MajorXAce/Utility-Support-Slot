#include <assert.h>
#include <stdio.h>
#include "../src/uss_hud_windows.h"
static unsigned char* allocation;static unsigned root_reads,deny_context;
static const char* build(void){return "25480438";}
static uintptr_t module(const char* name,size_t* size){assert(!strcmp(name,"game.dll"));*size=0x4000000;return 0x10000000;}
static int read_memory(uintptr_t at,void* out,size_t length)
{
    if(at==0x10000000+0x346d538 && length==8){uintptr_t root=(uintptr_t)allocation;memcpy(out,&root,8);++root_reads;return 0;}
    if(at<(uintptr_t)allocation || length>0x395000 || at-(uintptr_t)allocation>0x395000-length)return 1;
    memcpy(out,(void*)at,length);return 0;
}
static int context(void* user,const UssHudEvent* event,UssHudSnapshot* out)
{
    (void)user;(void)event;if(deny_context)return 0;memset(out,0,sizeof(*out));
    out->epoch=out->owner=out->inventory=out->row=1;out->root=(uintptr_t)allocation;out->avatar=2;
    out->context_verified=out->metadata_verified=1;out->display[0]=9;return 1;
}
static void word(unsigned at,uint32_t value){memcpy(allocation+0x394cd8+at,&value,4);}
int main(void)
{
    HfApi api={0};UssHudWindows adapter;UssHudAckServices io;UssHudAckState state={0};UssHudEvent event={1,1,USS_HUD_DRAW,10,11,10};UssHudSnapshot snapshot;unsigned char value[4]={0};
    allocation=VirtualAlloc(NULL,0x395000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);assert(allocation);
    api.version=12;api.size=sizeof(api);api.read=read_memory;api.module_base=module;api.build_id=build;
    memset(&adapter,0,sizeof(adapter));adapter.api=&api;adapter.base=0x10000000;adapter.module_size=0x4000000;adapter.thread=GetCurrentThreadId();adapter.context=context;
    word(0,10);word(16,1);word(24,1);word(28,10);word(32,10);io=uss_hud_windows_services(&adapter);
    assert(uss_hud_acknowledge(&state,&io,&event)==USS_HUD_ACKED && state.writes==2 && root_reads);
    snapshot=uss_hud_windows_observe(&adapter,&event);assert(snapshot.valid && uss_hud_word(snapshot.record,32)==11);
    assert(!uss_hud_windows_write(&adapter,&snapshot,20,value,4)); /* Pending/current fields are never writable here. */
    deny_context=1;assert(!uss_hud_windows_observe(&adapter,&event).valid);deny_context=0;
    ++adapter.thread;assert(!uss_hud_windows_observe(&adapter,&event).valid);
    assert(VirtualFree(allocation,0,MEM_RELEASE));puts("USS HUD Windows: SDK0-success, actual OWN private-memory writes/readback, allowed fields, context/thread gates PASS");return 0;
}
