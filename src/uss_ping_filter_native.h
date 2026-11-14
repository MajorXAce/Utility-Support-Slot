/* Native side of the stored-weapon ping filter (see uss_ping_filter.h): verified reads and the single engine call.
 * A port of Source/utility_support_slot.lua filter_init, filter_body and filter_send for game build 25480438.
 *
 * Everything is read-only except uss_ping_native_send, which makes exactly one call into the game's own filter function
 * after re-proving that the function pointer in the game's table is still the pinned one. Any structure that does not
 * match what the Lua version documented makes the read REFUSE (return 0); nothing is guessed or repaired. These
 * layouts were established by the Lua project on this build; offline fixtures model them, so a live check of the
 * first resolved helper (catalog index 26 for a carried weapon) is the real acceptance of this port.
 *
 *   game.dll+0x3326338  -> engine function table: +8 destroy, +0x100 property, +0x110 filter command
 *   helldivers2.exe     +0x77DA10 filter command, +0x77F4F0 destroy, +0x77D840 property, +0x27C5E48 filter catalog
 *   game.dll+0x3326450  -> physics actor manager (entity -> actor id), actor id -> descriptor table -> body record */
#ifndef USS_PING_FILTER_NATIVE_H
#define USS_PING_FILTER_NATIVE_H
#include "uss_ping_filter.h"
#include "sdk_compat.h"
#include "native_calls.h"
#include "../ResearchSource/inventory_reader.h"

#define USS_PING_GAME_ENGINE_TABLE 0x3326338u
#define USS_PING_GAME_ACTORS 0x3326450u
#define USS_PING_EXE_COMMAND 0x77DA10u
#define USS_PING_EXE_DESTROY 0x77F4F0u
#define USS_PING_EXE_PROPERTY 0x77D840u
#define USS_PING_EXE_CATALOG 0x27C5E48u
#define USS_PING_CATALOG_COUNT 104u

/* function = the pinned game function (re-verified in the table just before the call). */
typedef void (*UssPingInvoke)(void* user,uintptr_t function,uint32_t* actors,uint32_t count,uint32_t hash);
typedef struct UssPingNative {
    const HfApi* api;HfMod* mod;UssPingInvoke invoke;void* invoke_user;
    uintptr_t game,exe,function,table,worlds;unsigned verified;
} UssPingNative;

/* Reads of any length through the bounded reader (which allows 72 bytes per read). */
static int ping_read(Reader* r,uintptr_t address,void* out,size_t length)
{
    unsigned char* bytes=(unsigned char*)out;size_t done=0;
    while(done<length){
        size_t chunk=length-done>72?72:length-done;
        if(!get(r,address+done,bytes+done,chunk))return 0;
        done+=chunk;
    }
    return 1;
}
/* The one-time proof: pinned build, pinned engine addresses, the function's and its helpers' code bytes, and the filter
 * catalog (104 entries: the expected hashes at indices 26, 37, 57 and the mask pattern the Lua version verified). */
static int uss_ping_native_ready(void* user)
{
    static const unsigned char command_code[37]={0x41,0x56,0x41,0x57,0x48,0x83,0xec,0x48,0x44,0x8b,0xf2,0x4c,0x8b,0xf9,0x48,0x8b,
        0x15,0x23,0x84,0x04,0x02,0x48,0x85,0xd2,0x0f,0x84,0x53,0x01,0x00,0x00,0x44,0x8b,0x92,0xf8,0x00,0x00,0x00};
    UssPingNative* n=(UssPingNative*)user;Reader reader={0};const char* build;size_t game_size=0,exe_size=0;
    uintptr_t table,command,destroy,property,catalog,hashes_at,masks_at;
    unsigned char destroy_code[0x80],property_code[0x66],code[37],masks[USS_PING_CATALOG_COUNT*16];
    uint32_t hashes[USS_PING_CATALOG_COUNT],count;unsigned i,j;
    if(!n || !uss_sdk_compatible(n->api) || !n->mod || !n->api->read || !n->api->module_base || !n->api->build_id)return 0;
    n->verified=0;
    build=n->api->build_id();if(!build || strcmp(build,"25480438"))return 0;
    n->game=n->api->module_base("game.dll",&game_size);n->exe=n->api->module_base("helldivers2.exe",&exe_size);
    if(!n->game || !n->exe || game_size<0x3326DC8 || game_size>0x20000000 || exe_size<=USS_PING_EXE_CATALOG+0x110 ||
        exe_size>0x20000000 || !address_ok(n->game,game_size) || !address_ok(n->exe,exe_size))return 0;
    reader.api=n->api;reader.slot=-1;reader.stage="PING_FILTER_INIT";
    if(!get_ptr(&reader,n->game+USS_PING_GAME_ENGINE_TABLE,&table) || !get_ptr(&reader,table+0x110,&command) ||
        !get_ptr(&reader,table+8,&destroy) || !get_ptr(&reader,table+0x100,&property))return 0;
    if(command!=n->exe+USS_PING_EXE_COMMAND || destroy!=n->exe+USS_PING_EXE_DESTROY || property!=n->exe+USS_PING_EXE_PROPERTY)return 0;
    if(!ping_read(&reader,destroy,destroy_code,sizeof(destroy_code)) || !ping_read(&reader,property,property_code,sizeof(property_code)) ||
        !ping_read(&reader,command,code,sizeof(code)) || memcmp(code,command_code,sizeof(code)))return 0;
    /* lea r14,[rip+d] ... lea r8,[r14+d] in destroy, lea rax,[rip+d] in property: the displacements locate the tables. */
    if(destroy_code[0x56]!=0x4C || destroy_code[0x57]!=0x8D || destroy_code[0x58]!=0x35 ||
        destroy_code[0x73]!=0x4D || destroy_code[0x74]!=0x8D || destroy_code[0x75]!=0x86 ||
        property_code[0x3D]!=0x48 || property_code[0x3E]!=0x8D || property_code[0x3F]!=0x05)return 0;
    if(!get_ptr(&reader,n->exe+USS_PING_EXE_CATALOG,&catalog) || !get(&reader,catalog+0xF8,&count,4) || count!=USS_PING_CATALOG_COUNT ||
        !get_ptr(&reader,catalog+0x100,&hashes_at) || !get_ptr(&reader,catalog+0x108,&masks_at) || masks_at!=hashes_at+sizeof(hashes))return 0;
    if(!ping_read(&reader,hashes_at,hashes,sizeof(hashes)) || !ping_read(&reader,masks_at,masks,sizeof(masks)))return 0;
    if(hashes[USS_PING_NORMAL_INDEX]!=USS_PING_NORMAL_HASH || hashes[USS_PING_EXCLUDED_INDEX]!=USS_PING_EXCLUDED_HASH || hashes[57]!=0x74B9D874u)return 0;
    for(j=0;j<16;++j)if(masks[USS_PING_NORMAL_INDEX*16+j] || masks[USS_PING_EXCLUDED_INDEX*16+j])return 0;
    for(i=0;i<USS_PING_CATALOG_COUNT;++i){
        const unsigned char* row=masks+i*16;int old=(row[3]&4)!=0,fresh=(row[4]&32)!=0,special=i==57 || i==94;
        if((old!=fresh)!=special || (i==57 && (!old || fresh)))return 0;
    }
    n->table=(uintptr_t)((int64_t)destroy+0x5D+(int64_t)(int32_t)u32(destroy_code+0x59)+(int64_t)u32(destroy_code+0x76));
    n->worlds=(uintptr_t)((int64_t)property+0x44+(int64_t)(int32_t)u32(property_code+0x40));
    if(!address_ok(n->table,64*40) || !address_ok(n->worlds,0xB0*4))return 0;
    n->function=command;n->verified=1;return 1;
}
/* Fresh read-only resolution of a carried weapon's ping helper. The physics actor is NOT the weapon entity: the weapon's
 * actor id is checked against its descriptor table and body record before its filter data is reported. */
static int uss_ping_native_resolve(void* user,uint32_t entity,const unsigned char identity[8],UssPingHelper* out)
{
    static const unsigned char getter_code[20]={0x8B,0xC2,0x25,0xFF,0xFF,0xFF,0x00,0x48,0x8D,0x04,0x80,0x48,0xC1,0xE0,0x05,0x48,0x03,0x41,0x18,0xC3};
    UssPingNative* n=(UssPingNative*)user;Reader reader={0};
    unsigned char h[0x48],header[16],row[24],d[0x38],key[4],record[24],code[20],body[112];
    uintptr_t m,units,rows,unit,descriptor,data,getter,vt,fn,bodies;
    uint32_t i,id,world,group,index,layout,stride,key_offset,data_offset,body_id,body_index;
    if(!n || !n->verified || !out || !identity || !valid_id(entity))return 0;
    memset(out,0,sizeof(*out));
    reader.api=n->api;reader.slot=-1;reader.stage="PING_FILTER_HELPER";
    if(!get_ptr(&reader,n->game+USS_PING_GAME_ACTORS,&m) || !get(&reader,m,h,sizeof(h)) ||
        u32(h+0x0C)>u32(h+4) || u32(h+4)>16384 || !lookup(&reader,m+0x20,entity,&i) || i>=u32(h+0x0C))return 0;
    if(!get_ptr(&reader,m+0x38,&units) || !get_ptr(&reader,m+0x40,&rows) || !get_ptr(&reader,units+(uintptr_t)i*8,&unit) ||
        !get(&reader,unit,header,sizeof(header)) || u32(header+8)!=entity || memcmp(header,identity,8) ||
        !get(&reader,rows+(uintptr_t)i*24,row,sizeof(row)))return 0;
    id=u32(row);world=id>>30;group=(id>>28)&3u;
    descriptor=n->table+(uintptr_t)(group+world*10u)*64u;
    if(!get(&reader,descriptor,d,sizeof(d)))return 0;
    index=id&u32(d+0x28);layout=u32(d+0x1C);stride=layout&0xFFFFu;key_offset=(layout>>16)&0xFFu;data_offset=layout>>24;
    if(!get_ptr(&reader,descriptor,&data) || index>=u32(d+0x24) || (id&u32(d+0x34))==0 || stride<24 || stride>4096 ||
        key_offset+4>stride || data_offset+24>stride)return 0;
    if(!get(&reader,data+(uintptr_t)index*stride+key_offset,key,4) || u32(key)!=id ||
        !get(&reader,data+(uintptr_t)index*stride+data_offset,record,sizeof(record)))return 0;
    if(!get_ptr(&reader,n->worlds+(uintptr_t)world*0xB0+0x10,&getter) || !get_ptr(&reader,getter,&vt) ||
        !get_ptr(&reader,vt+0x70,&fn) || !ping_read(&reader,fn,code,sizeof(code)) || memcmp(code,getter_code,sizeof(code)))return 0;
    body_id=u32(record+0x14);
    if(!get_ptr(&reader,getter+0x18,&bodies))return 0;
    body_index=body_id&0xFFFFFFu;
    if(body_index>=0x100000u || !ping_read(&reader,bodies+(uintptr_t)body_index*160,body,sizeof(body)))return 0;
    out->entity=entity;out->actor=id;out->body_id=body_id;out->packed=u32(body+0x6C);memcpy(out->identity,identity,8);
    return 1;
}
/* The ONLY effect: ask the game's own function to queue a filter change for one actor. The table is re-read first and
 * its function pointer must still be the pinned one; only the two catalog hashes verified at init are accepted. */
static int uss_ping_native_send(void* user,uint32_t actor,uint32_t hash)
{
    UssPingNative* n=(UssPingNative*)user;Reader reader={0};uintptr_t table,command;uint32_t actors[1];
    if(!n || !n->verified || !n->invoke || !valid_id(actor) || (hash!=USS_PING_NORMAL_HASH && hash!=USS_PING_EXCLUDED_HASH))return 0;
    reader.api=n->api;reader.slot=-1;reader.stage="PING_FILTER_SEND";
    if(!get_ptr(&reader,n->game+USS_PING_GAME_ENGINE_TABLE,&table) || !get_ptr(&reader,table+0x110,&command) || command!=n->function)return 0;
    actors[0]=actor;
    n->invoke(n->invoke_user,command,actors,1,hash);
    return 1;
}
static void uss_ping_native_note(void* user,const char* event,uint32_t entity,uint32_t value)
{
    UssPingNative* n=(UssPingNative*)user;
    if(n && n->api && n->api->log && n->mod)n->api->log(n->mod,"USS ping filter %s entity=%u value=%u",event,(unsigned)entity,(unsigned)value);
}
/* Production forwarder: the game's own filter command, typed. */
static inline void uss_ping_native_invoke(void* user,uintptr_t function,uint32_t* actors,uint32_t count,uint32_t hash)
{
    (void)user;((UssFilterCommandFn)function)(actors,count,hash);
}
static inline UssPingFilterIo uss_ping_native_io(UssPingNative* native)
{
    UssPingFilterIo io={0};
    io.user=native;io.ready=uss_ping_native_ready;io.resolve=uss_ping_native_resolve;io.send=uss_ping_native_send;io.note=uss_ping_native_note;
    return io;
}
#endif
