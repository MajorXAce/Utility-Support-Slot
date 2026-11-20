/* Optional, restart-only Windows hook adapter. The SDK tick discovers modules
 * and reports counters; it never uses a cached Lua state. */
#ifndef USS_NATIVE_LUA_WINDOWS_H
#define USS_NATIVE_LUA_WINDOWS_H
#include <intrin.h>
#include "hellforge_mod.h"
#include "uss_native_lua_client.h"
typedef struct UssNativeLuaWindows {
 UssNativeLuaClient client;HMODULE module;UssLuaMenuMailbox* mailbox;volatile LONG* published_mode;
 uint64_t discover_after,summary_after;const char* module_reason;const char* reported_reason;
 const void* reported_state;DWORD reported_thread;uint32_t reported_generation;unsigned attempted,reports,summaries;UssLuaMenuPost reported_post;
} UssNativeLuaWindows;
static UssNativeLuaWindows uss_native_lua_windows;
static unsigned uss_native_lua_default(void* user)
{UssNativeLuaWindows* w=user;return (unsigned)InterlockedCompareExchange(w->published_mode,0,0);}
static void uss_native_lua_emit(void* user,const UssLuaMenuPost* post)
{UssNativeLuaWindows* w=user;(void)uss_lua_mailbox_post(w->mailbox,post,sizeof *post);}
static int uss_native_lua_detour(lua_State* L,int nargs,int nresults,int errfunc)
{
 uintptr_t caller;
#ifdef _MSC_VER
 caller=(uintptr_t)_ReturnAddress();
#else
 caller=(uintptr_t)__builtin_return_address(0);
#endif
 return uss_native_lua_boundary(&uss_native_lua_windows.client,L,nargs,nresults,errfunc,caller,GetCurrentThreadId(),GetTickCount64());
}
static inline void uss_native_lua_windows_tick(const HfApi* api,HfMod* mod,UssLuaMenuMailbox* mailbox,volatile LONG* published_mode,uint64_t now)
{
 UssNativeLuaWindows* w=&uss_native_lua_windows;UssNativeLuaClient* c=&w->client;
 union {void* raw;UssLuaPcall fn;} pcall_address;
 if(!w->attempted && now>=w->discover_after){
  w->discover_after=now+1000;w->module_reason="WAIT_INTERPRETER";
  if(!GetModuleHandleExW(0,L"lua51.dll",&w->module))goto report;
  w->attempted=1;w->mailbox=mailbox;w->published_mode=published_mode;c->user=w;c->mode=uss_native_lua_default;c->emit=uss_native_lua_emit;
  if(!uss_native_lua_api(&c->api,w->module)){w->module_reason="MISSING_PUBLIC_LUA_EXPORT";goto report;}
  if(api->version<13 || api->size<offsetof(HfApi,code_matches)+sizeof api->code_matches || !api->code_matches || !api->hook || !api->module_base){w->module_reason="SDK_HOOK_CODE_CHECK_UNAVAILABLE";goto report;}
  c->exe=api->module_base("helldivers2.exe",&c->exe_size);pcall_address.fn=c->api.pcall;
  if(!c->exe || !c->exe_size || !api->code_matches((uintptr_t)pcall_address.raw,"48 89 5C 24 08 57 48 83 EC 20 8B 59 08 4C 8B D1 0F B6 7B 7D 40 80 E7 F0")){w->module_reason="UNSUPPORTED_INTERPRETER_ENTRY";goto report;}
  if(api->hook(mod,(uintptr_t)pcall_address.raw,(void*)uss_native_lua_detour,&c->original) || !c->original){InterlockedExchange(&c->stopped,1);w->module_reason="HOOK_REFUSED";goto report;}
  w->module_reason="HOOK_READY";c->reason="WAIT_EXECUTABLE_LUA_CALL";
  api->log(mod,"USS native Lua bridge module=%p pcall=%p executable=%p size=%llu phase=update-aligned-or-throttled sample_cap_ms=16 protected=lua_cpcall",
   (void*)w->module,pcall_address.raw,(void*)c->exe,(unsigned long long)c->exe_size);
 }
 report:
 if(w->module_reason!=w->reported_reason && !c->original){w->reported_reason=w->module_reason;api->log(mod,"USS native Lua bridge discovery=%s",w->module_reason?w->module_reason:"WAIT");}
 if(!c->original || InterlockedCompareExchange(&c->busy,1,0))return;
 if(w->reports<32 && (c->reason!=w->reported_reason || c->state_identity!=w->reported_state || c->thread!=w->reported_thread || c->generation!=w->reported_generation || c->last_post.options_registered!=w->reported_post.options_registered || c->last_post.bindings_registered!=w->reported_post.bindings_registered || c->last_post.ready!=w->reported_post.ready || c->last_post.mode!=w->reported_post.mode || c->last_post.utility_down!=w->reported_post.utility_down || c->last_post.action_blocked!=w->reported_post.action_blocked)){
  ++w->reports;w->reported_reason=c->reason;w->reported_state=c->state_identity;w->reported_thread=c->thread;w->reported_generation=c->generation;w->reported_post=c->last_post;
  api->log(mod,"USS native Lua bridge status=%s state=%p registry=%p thread=%lu generation=%u options=%u bindings=%u ready=%u mode=%u down=%u blocked=%u attempts=%u/%u stopped=%ld",
   c->reason?c->reason:"WAIT",c->state_identity,c->registry_identity,(unsigned long)c->thread,c->generation,c->last_post.options_registered,c->last_post.bindings_registered,c->last_post.ready,c->last_post.mode,c->last_post.utility_down,c->last_post.action_blocked,c->option_attempts,c->binding_attempts,c->stopped);
 }
 if(w->summaries<12 && now>=w->summary_after){unsigned queued,taken,canceled,overflow;++w->summaries;w->summary_after=now+5000;
  AcquireSRWLockShared(&mailbox->lock);queued=mailbox->edge_queued;taken=mailbox->edge_taken;canceled=mailbox->edge_canceled;overflow=mailbox->edge_overflow;ReleaseSRWLockShared(&mailbox->lock);
  api->log(mod,"USS native Lua bridge counts calls=%lld samples=%lld posts=%lld errors=%lld reentry=%lld foreign=%lld wrong_thread=%lld aligned=%lld capped=%lld edge_queued=%u edge_taken=%u edge_canceled=%u edge_overflow=%u",
   c->calls,c->samples,c->posts,c->errors,c->reentry,c->foreign,c->wrong_thread,c->aligned,c->cap_skips,queued,taken,canceled,overflow);
 }
 InterlockedExchange(&c->busy,0);
}
#endif
