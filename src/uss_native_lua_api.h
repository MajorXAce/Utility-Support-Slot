/* Public Lua 5.1 C ABI subset, resolved from the already loaded interpreter.
 * Declarations follow lua.org/source/5.1/lua.h; no private VM layouts. */
#ifndef USS_NATIVE_LUA_API_H
#define USS_NATIVE_LUA_API_H
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef struct lua_State lua_State;
typedef int (*UssLuaCFunction)(lua_State*);
typedef int (*UssLuaPcall)(lua_State*,int,int,int);
enum { USS_LUA_REGISTRY=-10000,USS_LUA_GLOBALS=-10002,USS_LUA_NIL=0,USS_LUA_BOOLEAN=1,USS_LUA_NUMBER=3,USS_LUA_TABLE=5,USS_LUA_FUNCTION=6 };
#define USS_LUA_API_FIELDS(X) \
 X(int,gettop,(lua_State*)) X(void,settop,(lua_State*,int)) \
 X(int,checkstack,(lua_State*,int)) X(int,type,(lua_State*,int)) \
 X(int,toboolean,(lua_State*,int)) X(double,tonumber,(lua_State*,int)) \
 X(void*,touserdata,(lua_State*,int)) X(const void*,topointer,(lua_State*,int)) \
 X(int,pushthread,(lua_State*)) X(void,pushvalue,(lua_State*,int)) \
 X(void,pushnil,(lua_State*)) X(void,pushnumber,(lua_State*,double)) \
 X(void,pushstring,(lua_State*,const char*)) X(void,pushlightuserdata,(lua_State*,void*)) \
 X(void,getfield,(lua_State*,int,const char*)) X(void,setfield,(lua_State*,int,const char*)) \
 X(void,rawget,(lua_State*,int)) X(void,rawset,(lua_State*,int)) \
 X(void,rawseti,(lua_State*,int,int)) X(void,createtable,(lua_State*,int,int)) \
 X(int,pcall,(lua_State*,int,int,int)) X(int,cpcall,(lua_State*,UssLuaCFunction,void*)) \
 X(int,error,(lua_State*))
typedef struct UssLuaApi {
#define USS_LUA_DECLARE(result,name,args) result (*name) args;
 USS_LUA_API_FIELDS(USS_LUA_DECLARE)
#undef USS_LUA_DECLARE
} UssLuaApi;
static inline int uss_native_lua_api(UssLuaApi* out,HMODULE module)
{
 UssLuaApi a={0};FARPROC symbol;
 if(!out || !module)return 0;
#define USS_LUA_RESOLVE(result,name,args) symbol=GetProcAddress(module,"lua_" #name);if(!symbol)return 0;memcpy(&a.name,&symbol,sizeof a.name);
 USS_LUA_API_FIELDS(USS_LUA_RESOLVE)
#undef USS_LUA_RESOLVE
 *out=a;return 1;
}
#endif
