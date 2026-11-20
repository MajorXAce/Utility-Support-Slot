/* Transient native client of the public Lua menu API. All Lua work happens
 * within lua_cpcall on an interpreter-supplied state; no borrowed tick state.
 * Registry anchors keep only provider tables, never native callbacks. */
#ifndef USS_NATIVE_LUA_CLIENT_H
#define USS_NATIVE_LUA_CLIENT_H
#include "uss_native_lua_api.h"
#include "uss_lua_menu_mailbox.h"
#define USS_NATIVE_OPTION "majorxace.uss.binding_mode"
#define USS_NATIVE_BINDING "majorxace.uss.utility_weapon"
typedef struct UssNativeLuaClient {
 UssLuaApi api;void* original;uintptr_t exe;size_t exe_size;
 void* user;unsigned (*mode)(void*);void (*emit)(void*,const UssLuaMenuPost*);
 volatile LONG busy,stopped,prefer_c;volatile LONG64 calls,reentry,foreign,wrong_thread,samples,errors,posts,aligned,cap_skips;
 DWORD thread;const void* state_identity;const void* registry_identity;const void* update_identity;const void* options;const void* bindings;
 uint64_t next_sample,next_register,sequence;uint32_t generation,option_attempts,binding_attempts;
 unsigned registered_options,registered_bindings,last_flags,error_streak;
 uint64_t sample_now;DWORD sample_thread;UssLuaMenuPost result,last_post;const char* reason;
} UssNativeLuaClient;
static __declspec(thread) UssNativeLuaClient* uss_native_lua_active;
static __declspec(thread) unsigned uss_native_lua_depth;
static inline void uss_native_lua_pop(UssLuaApi* a,lua_State* L){a->settop(L,-2);}
static inline void uss_native_lua_number(UssLuaApi* a,lua_State* L,const char* name,double n){a->pushnumber(L,n);a->setfield(L,-2,name);}
static inline void uss_native_lua_string(UssLuaApi* a,lua_State* L,const char* name,const char* s){a->pushstring(L,s);a->setfield(L,-2,name);}
static inline int uss_native_lua_method(UssLuaApi* a,lua_State* L,int table,const char* name)
{int ok;a->getfield(L,table,name);ok=a->type(L,-1)==USS_LUA_FUNCTION;uss_native_lua_pop(a,L);return ok;}
static inline int uss_native_lua_shape(UssLuaApi* a,lua_State* L,int table,int binding)
{
 int ok;if(a->type(L,table)!=USS_LUA_TABLE)return 0;
 a->getfield(L,table,"api");ok=a->type(L,-1)==USS_LUA_NUMBER && a->tonumber(L,-1)==1;uss_native_lua_pop(a,L);
 a->getfield(L,table,"version");ok=ok && a->type(L,-1)==USS_LUA_NUMBER && a->tonumber(L,-1)>=(binding?2:3);uss_native_lua_pop(a,L);
 return ok && uss_native_lua_method(a,L,table,binding?"register_binding":"register_option") &&
  uss_native_lua_method(a,L,table,binding?"is_down":"get") && (binding || uss_native_lua_method(a,L,table,"on_change"));
}
/* A failed provider call raises only inside our protected C entry. */
static inline void uss_native_lua_call(UssNativeLuaClient* c,lua_State* L,int args,int results)
{if(c->api.pcall(L,args,results,0)){c->reason="PROVIDER_ERROR";c->api.error(L);}}
static inline int uss_native_lua_ready(UssNativeLuaClient* c,lua_State* L,int table)
{
 UssLuaApi* a=&c->api;int ok;a->getfield(L,table,"ready");
 if(a->type(L,-1)==USS_LUA_NIL){uss_native_lua_pop(a,L);return 1;}
 if(a->type(L,-1)!=USS_LUA_FUNCTION){uss_native_lua_pop(a,L);return 0;}
 uss_native_lua_call(c,L,0,1);ok=a->type(L,-1)==USS_LUA_BOOLEAN && a->toboolean(L,-1);uss_native_lua_pop(a,L);return ok;
}
static inline void uss_native_lua_generation(UssNativeLuaClient* c)
{
 if(c->generation==UINT32_MAX){InterlockedExchange(&c->stopped,1);c->reason="GENERATION_LIMIT";return;}
 ++c->generation;c->registered_options=c->registered_bindings=0;c->option_attempts=c->binding_attempts=0;c->next_register=0;
}
static int uss_native_lua_protected(lua_State* L)
{
 UssNativeLuaClient* c=uss_native_lua_active;UssLuaApi* a=&c->api;
 const void *registry,*options,*bindings;int present,bind_present,option_ready,binding_ready,mode=0,input=0,down=0,blocked=0,anchor;
 unsigned default_mode=c->mode?c->mode(c->user):0;
 if(!a->checkstack(L,48)){c->reason="STACK_CAPACITY";return 0;}
 if(a->pushthread(L)!=1){uss_native_lua_pop(a,L);c->reason="COROUTINE";return 0;}uss_native_lua_pop(a,L);
 registry=a->topointer(L,USS_LUA_REGISTRY);
 a->getfield(L,USS_LUA_GLOBALS,"ModOptionsMenu");a->getfield(L,USS_LUA_GLOBALS,"ModBindingsMenu");
 present=uss_native_lua_shape(a,L,2,0);c->result.flags=present?USS_LUA_MENU_OPTIONS_PRESENT:0;
 bind_present=uss_native_lua_shape(a,L,3,1);
 /* Once admitted, OS-thread changes revoke samples without touching its VM. */
 if(c->thread && c->thread!=c->sample_thread){c->reason="WRONG_THREAD";InterlockedIncrement64(&c->wrong_thread);return 0;}
 if(!present && !bind_present){
  if(c->state_identity && (c->state_identity!=L || c->registry_identity!=registry)){c->reason="UNIDENTIFIED_STATE";return 0;}
  if(c->options || c->bindings){uss_native_lua_generation(c);c->options=c->bindings=NULL;}
  c->reason="NO_PROVIDER";return 0;
 }
 if(c->state_identity!=L || c->registry_identity!=registry){
  uss_native_lua_generation(c);c->state_identity=L;c->registry_identity=registry;c->options=c->bindings=NULL;c->thread=c->sample_thread;
 }
 a->getfield(L,USS_LUA_GLOBALS,"update");c->update_identity=a->type(L,-1)==USS_LUA_FUNCTION?a->topointer(L,-1):NULL;uss_native_lua_pop(a,L);
 options=present?a->topointer(L,2):NULL;bindings=bind_present?a->topointer(L,3):NULL;
 if(c->options!=options || c->bindings!=bindings){uss_native_lua_generation(c);c->options=options;c->bindings=bindings;}
 /* Anchor the two current tables in this VM's registry. We never dereference
  * old state pointers or unref a VM that may already have closed. */
 a->pushlightuserdata(L,c);a->rawget(L,USS_LUA_REGISTRY);
 if(a->type(L,-1)!=USS_LUA_TABLE){uss_native_lua_generation(c);uss_native_lua_pop(a,L);a->createtable(L,2,0);a->pushlightuserdata(L,c);a->pushvalue(L,-2);a->rawset(L,USS_LUA_REGISTRY);}
 anchor=a->gettop(L);a->pushvalue(L,2);a->rawseti(L,anchor,1);a->pushvalue(L,3);a->rawseti(L,anchor,2);uss_native_lua_pop(a,L);
 if(c->stopped)return 0;
 option_ready=present && uss_native_lua_ready(c,L,2);binding_ready=bind_present && uss_native_lua_ready(c,L,3);
 if(!InterlockedCompareExchange(&c->prefer_c,0,0) && c->sample_now>=c->next_register){
  c->next_register=c->sample_now+1000;
  if(option_ready && !c->registered_options && c->option_attempts<60 && (default_mode==1 || default_mode==2)){
   ++c->option_attempts;a->getfield(L,2,"register_option");a->pushstring(L,USS_NATIVE_OPTION);a->createtable(L,0,7);
   uss_native_lua_string(a,L,"type","choice");uss_native_lua_string(a,L,"label","Binding Mode");uss_native_lua_string(a,L,"mod","Utility Support Slot");uss_native_lua_string(a,L,"mod_id","majorxace.uss");
   uss_native_lua_string(a,L,"description","Default uses the normal support weapon control. Dedicated uses Utility Weapon.");uss_native_lua_number(a,L,"default",default_mode);
   a->createtable(L,2,0);a->pushstring(L,"Default");a->rawseti(L,-2,1);a->pushstring(L,"Dedicated");a->rawseti(L,-2,2);a->setfield(L,-2,"choices");
   uss_native_lua_call(c,L,2,1);c->registered_options=a->type(L,-1)==USS_LUA_BOOLEAN && a->toboolean(L,-1);uss_native_lua_pop(a,L);
  }
  if(binding_ready && !c->registered_bindings && c->binding_attempts<60){
   ++c->binding_attempts;a->getfield(L,3,"register_binding");a->pushstring(L,USS_NATIVE_BINDING);a->pushstring(L,"Utility Weapon");a->pushnil(L);a->createtable(L,0,1);uss_native_lua_string(a,L,"category","Utility Support Slot");
   uss_native_lua_call(c,L,4,1);c->registered_bindings=a->type(L,-1)==USS_LUA_BOOLEAN && a->toboolean(L,-1);uss_native_lua_pop(a,L);
  }
 }
 if(option_ready && c->registered_options){a->getfield(L,2,"get");a->pushstring(L,USS_NATIVE_OPTION);uss_native_lua_call(c,L,1,1);
  if(a->type(L,-1)==USS_LUA_NUMBER && (a->tonumber(L,-1)==1 || a->tonumber(L,-1)==2))mode=(int)a->tonumber(L,-1);uss_native_lua_pop(a,L);}
 if(binding_ready && c->registered_bindings){a->getfield(L,3,"is_down");a->pushstring(L,USS_NATIVE_BINDING);uss_native_lua_call(c,L,1,1);
  if(a->type(L,-1)==USS_LUA_BOOLEAN){input=1;down=a->toboolean(L,-1);}else if(a->type(L,-1)==USS_LUA_NIL){input=1;blocked=1;}uss_native_lua_pop(a,L);}
 c->result.options_registered=present && c->registered_options;c->result.bindings_registered=c->registered_bindings;
 c->result.mode=mode?(unsigned)mode:(default_mode==2?2:1);c->result.input_valid=(unsigned)input;c->result.utility_down=(unsigned)down;c->result.action_blocked=(unsigned)blocked;
 c->result.ready=option_ready && binding_ready && c->registered_options && c->registered_bindings && mode && input;
 c->reason=c->result.ready?"READY":"WAIT_READY_REGISTRATION_OR_VALUE";return 0;
}
static inline void uss_native_lua_post(UssNativeLuaClient* c,int valid)
{
 UssLuaMenuPost p=c->result;
 if(!valid){memset(&p,0,sizeof p);p.flags=c->last_flags|c->result.flags;p.mode=1;}
 if(c->sequence==UINT64_MAX){InterlockedExchange(&c->stopped,1);return;}
 p.magic=USS_LUA_MENU_MAGIC;p.protocol=USS_LUA_MENU_PROTOCOL;p.size=sizeof p;p.instance=(uint64_t)(uintptr_t)c;p.sequence=++c->sequence;p.generation=c->generation?c->generation:1;
 c->last_flags=p.flags;c->last_post=p;if(c->emit)c->emit(c->user,&p);InterlockedIncrement64(&c->posts);
}
/* Call only from an actual interpreter-call boundary. No state is used later
 * from an SDK tick. A busy/wrong-thread path still forwards the original. */
static inline int uss_native_lua_boundary(UssNativeLuaClient* c,lua_State* L,int nargs,int nresults,int errfunc,uintptr_t caller,DWORD thread,uint64_t now)
{
 union {void* raw;UssLuaPcall fn;} next;int status,top,rc;const void* target=NULL;
 if(!uss_native_lua_depth && !c->stopped && L && nargs>=0 && caller>=c->exe && caller-c->exe<c->exe_size && (!c->thread || c->thread==thread))
  target=c->api.topointer(L,-nargs-1); /* Public, nonallocating function identity; original stack untouched. */
 ++uss_native_lua_depth;InterlockedIncrement64(&c->calls);next.raw=c->original;
 status=next.fn(L,nargs,nresults,errfunc);
 if(uss_native_lua_depth!=1){InterlockedIncrement64(&c->reentry);--uss_native_lua_depth;return status;}
 if(c->stopped){--uss_native_lua_depth;return status;}
 if(!L || caller<c->exe || caller-c->exe>=c->exe_size){InterlockedIncrement64(&c->foreign);--uss_native_lua_depth;return status;}
 if(InterlockedCompareExchange(&c->busy,1,0)){--uss_native_lua_depth;return status;}
 if(c->thread && c->thread!=thread){InterlockedIncrement64(&c->wrong_thread);if(!c->reason || strcmp(c->reason,"WRONG_THREAD")){c->reason="WRONG_THREAD";uss_native_lua_post(c,0);}goto done;}
 /* A recognized global update completes one provider frame. Preserve that
  * phase even below 16 ms; unrelated calls retain the bootstrap throttle. */
 if(now<c->next_sample && (!target || target!=c->update_identity)){InterlockedIncrement64(&c->cap_skips);goto done;}
 if(target && target==c->update_identity)InterlockedIncrement64(&c->aligned);
 c->next_sample=now+16;c->sample_now=now;c->sample_thread=thread;memset(&c->result,0,sizeof c->result);c->result.mode=1;
 top=c->api.gettop(L);uss_native_lua_active=c;rc=c->api.cpcall(L,uss_native_lua_protected,c);uss_native_lua_active=NULL;InterlockedIncrement64(&c->samples);
 if(rc){c->api.settop(L,top);++c->error_streak;InterlockedIncrement64(&c->errors);if(c->error_streak>=8)InterlockedExchange(&c->stopped,1);c->reason="PROTECTED_ERROR";uss_native_lua_post(c,0);}
 else if(c->api.gettop(L)!=top){c->api.settop(L,top);c->reason="STACK_MISMATCH";InterlockedExchange(&c->stopped,1);uss_native_lua_post(c,0);}
 else {c->error_streak=0;uss_native_lua_post(c,c->reason && (!strcmp(c->reason,"READY") || !strcmp(c->reason,"WAIT_READY_REGISTRATION_OR_VALUE") || !strcmp(c->reason,"NO_PROVIDER")));}
 done:InterlockedExchange(&c->busy,0);--uss_native_lua_depth;return status;
}
static inline void uss_native_lua_stop(UssNativeLuaClient* c)
{InterlockedExchange(&c->stopped,1);c->reason="STOPPED";uss_native_lua_post(c,0);}
#endif
