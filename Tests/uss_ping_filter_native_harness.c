/* Native ping-filter adapter (uss_ping_filter_native.h) over a small sparse memory modelling the structures the Lua version
 * documented. This proves the PORT against that model and every refusal path; it cannot prove the model matches the live
 * game (the first live helper must read catalog index 26). The game's filter function is replaced by a recording fake. */
#ifdef NDEBUG
#error Ping filter native checks require active assertions
#endif
#include "../src/uss_ping_filter_native.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#define GAME ((uintptr_t)0x10000000)
#define EXE ((uintptr_t)0x20000000)
typedef struct Region { uintptr_t at;size_t n;unsigned char* data; } Region;
static Region regions[64];static unsigned used;static unsigned reads,fail_at;
static unsigned char* region(uintptr_t at,size_t n)
{
    Region* r;assert(used<64);r=&regions[used++];r->at=at;r->n=n;r->data=(unsigned char*)calloc(1,n);assert(r->data);return r->data;
}
static void release_regions(void){unsigned i;for(i=0;i<used;++i)free(regions[i].data);used=0;}
static int fake_read(uintptr_t a,void* out,size_t n)
{
    unsigned i;++reads;
    if(fail_at && reads==fail_at)return 1;
    for(i=0;i<used;++i)if(a>=regions[i].at && a+n<=regions[i].at+regions[i].n){memcpy(out,regions[i].data+(a-regions[i].at),n);return 0;}
    return 1;
}
static void put32(void* p,uint32_t v){memcpy(p,&v,4);}
static void put64(void* p,uint64_t v){memcpy(p,&v,8);}
static unsigned char* at(uintptr_t a){unsigned i;for(i=0;i<used;++i)if(a>=regions[i].at && a<regions[i].at+regions[i].n)return regions[i].data+(a-regions[i].at);assert(0);return NULL;}

static HfApi fake;static const char* build_text="25480438";static int hide_exe,hide_game;
static uintptr_t fake_module(const char* name,size_t* size)
{
    if(!strcmp(name,"game.dll")){*size=0x4000000;return hide_game?0:GAME;}
    if(!strcmp(name,"helldivers2.exe")){*size=0x4000000;return hide_exe?0:EXE;}
    return 0;
}
static const char* fake_build(void){return build_text;}
static char last_log[160];static unsigned logs;
static void fake_log(HfMod* mod,const char* fmt,...){va_list a;(void)mod;va_start(a,fmt);vsnprintf(last_log,sizeof(last_log),fmt,a);va_end(a);++logs;}
#define MOD ((HfMod*)&fake)

static const unsigned char command_code[37]={0x41,0x56,0x41,0x57,0x48,0x83,0xec,0x48,0x44,0x8b,0xf2,0x4c,0x8b,0xf9,0x48,0x8b,
    0x15,0x23,0x84,0x04,0x02,0x48,0x85,0xd2,0x0f,0x84,0x53,0x01,0x00,0x00,0x44,0x8b,0x92,0xf8,0x00,0x00,0x00};
static const unsigned char getter_code[20]={0x8B,0xC2,0x25,0xFF,0xFF,0xFF,0x00,0x48,0x8D,0x04,0x80,0x48,0xC1,0xE0,0x05,0x48,0x03,0x41,0x18,0xC3};
static const unsigned char ID_A[8]={0xAA,0x79,0xCA,0x92,0x05,0x46,0x13,0xFC},ID_B[8]={9,8,7,6,5,4,3,2};
#define ENGINE_TABLE ((uintptr_t)0x30000000)
#define CATALOG ((uintptr_t)0x31000000)
#define HASHES ((uintptr_t)0x32000000)
#define TABLE ((uintptr_t)0x33000000)
#define WORLDS ((uintptr_t)0x34000000)
#define MANAGER ((uintptr_t)0x35000000)
#define MAPDATA ((uintptr_t)0x36000000)
#define UNITS ((uintptr_t)0x37000000)
#define UNIT_OBJECTS ((uintptr_t)0x38000000)
#define ROWS ((uintptr_t)0x39000000)
#define DATA ((uintptr_t)0x3A000000)
#define GETTER ((uintptr_t)0x3B000000)
#define VTABLE ((uintptr_t)0x3C000000)
#define BODIES ((uintptr_t)0x3D000000)
#define GETTER_FN (EXE+0x1000)

static void build_fixture(void)
{
    unsigned char *b,*destroy_code,*property_code;uint32_t i,j,d2=0x100;int64_t disp;uint32_t actor[3]={0x40000011u,0x40000012u,0x40000013u};
    release_regions();reads=fail_at=0;hide_exe=hide_game=0;build_text="25480438";logs=0;
    memset(&fake,0,sizeof(fake));fake.version=HF_MOD_SDK_VERSION;fake.size=(uint32_t)sizeof(fake);fake.read=fake_read;
    fake.module_base=fake_module;fake.build_id=fake_build;fake.log=fake_log;
    /* engine table and code */
    put64(region(GAME+USS_PING_GAME_ENGINE_TABLE,8),ENGINE_TABLE);
    b=region(ENGINE_TABLE,0x200);put64(b+8,EXE+USS_PING_EXE_DESTROY);put64(b+0x100,EXE+USS_PING_EXE_PROPERTY);put64(b+0x110,EXE+USS_PING_EXE_COMMAND);
    memcpy(region(EXE+USS_PING_EXE_COMMAND,37),command_code,37);
    destroy_code=region(EXE+USS_PING_EXE_DESTROY,0x80);property_code=region(EXE+USS_PING_EXE_PROPERTY,0x66);
    destroy_code[0x56]=0x4C;destroy_code[0x57]=0x8D;destroy_code[0x58]=0x35;destroy_code[0x73]=0x4D;destroy_code[0x74]=0x8D;destroy_code[0x75]=0x86;
    disp=(int64_t)TABLE-(int64_t)(EXE+USS_PING_EXE_DESTROY)-0x5D-(int64_t)d2;put32(destroy_code+0x59,(uint32_t)(int32_t)disp);put32(destroy_code+0x76,d2);
    property_code[0x3D]=0x48;property_code[0x3E]=0x8D;property_code[0x3F]=0x05;
    disp=(int64_t)WORLDS-(int64_t)(EXE+USS_PING_EXE_PROPERTY)-0x44;put32(property_code+0x40,(uint32_t)(int32_t)disp);
    /* catalog */
    put64(region(EXE+USS_PING_EXE_CATALOG,8),CATALOG);
    b=region(CATALOG,0x110);put32(b+0xF8,104);put64(b+0x100,HASHES);put64(b+0x108,HASHES+416);
    b=region(HASHES,416+1664);
    for(i=0;i<104;++i)put32(b+i*4,0x1000u+i);
    put32(b+26*4,USS_PING_NORMAL_HASH);put32(b+37*4,USS_PING_EXCLUDED_HASH);put32(b+57*4,0x74B9D874u);
    for(i=0;i<104;++i){unsigned char* row=b+416+i*16;if(i==57)row[3]=4;if(i==94)row[4]=32;} /* old/new pattern; rows 26 and 37 stay zero */
    /* physics actor manager: entities 101..103, three units, three rows */
    put64(region(GAME+USS_PING_GAME_ACTORS,8),MANAGER);
    b=region(MANAGER,0x80);put32(b+4,16);put32(b+0x0C,3);put64(b+0x20,MAPDATA);put32(b+0x28,8);put32(b+0x2C,UINT32_MAX);put32(b+0x30,1);
    put64(b+0x38,UNITS);put64(b+0x40,ROWS);
    b=region(MAPDATA,64);for(i=0;i<16;++i)put32(b+i*4,UINT32_MAX);
    for(i=0;i<3;++i){uint32_t key=101+i,bucket=key&7;put32(b+bucket*8,key);put32(b+bucket*8+4,i);}
    b=region(UNITS,24);for(i=0;i<3;++i)put64(b+i*8,UNIT_OBJECTS+i*32);
    b=region(UNIT_OBJECTS,96);for(i=0;i<3;++i){memcpy(b+i*32,i==0?ID_A:ID_B,8);put32(b+i*32+8,101+i);}
    b=region(ROWS,72);for(i=0;i<3;++i)put32(b+i*24,actor[i]);
    /* descriptor for world 1, group 0 */
    b=region(TABLE,0x800);put64(b+640,DATA);put32(b+640+0x28,0xFF);put32(b+640+0x1C,0x08000030u);put32(b+640+0x24,0x200);put32(b+640+0x34,0x40000000u);
    b=region(DATA,0x20*48);for(i=0;i<3;++i){put32(b+(actor[i]&0xFF)*48,actor[i]);put32(b+(actor[i]&0xFF)*48+8+0x14,0x50+i);}
    /* worlds -> getter -> vtable -> function; bodies */
    put64(region(WORLDS,0x2C0)+0xB0+0x10,GETTER);
    b=region(GETTER,0x40);put64(b,VTABLE);put64(b+0x18,BODIES);
    put64(region(VTABLE,0x80)+0x70,GETTER_FN);memcpy(region(GETTER_FN,20),getter_code,20);
    b=region(BODIES,0xD000);for(j=0;j<3;++j)put32(b+(0x50+j)*160+0x6C,0x00AB0000u|26u);
}
static unsigned invoked,invoked_count;static uintptr_t invoked_function;static uint32_t invoked_actor,invoked_hash;
static void fake_invoke(void* user,uintptr_t function,uint32_t* actors,uint32_t count,uint32_t hash)
{(void)user;++invoked;invoked_function=function;invoked_count=count;invoked_actor=actors[0];invoked_hash=hash;}
static UssPingNative native;
static void native_scene(void)
{
    build_fixture();memset(&native,0,sizeof(native));native.api=&fake;native.mod=MOD;native.invoke=fake_invoke;invoked=0;
}
/* Corrupt one byte, run `ready`, expect refusal, rebuild. */
static void expect_ready_refused(uintptr_t address,unsigned char value)
{
    native_scene();*at(address)=value;assert(!uss_ping_native_ready(&native) && !native.verified);
}
int main(void)
{
    UssPingHelper helper;unsigned i,total,steps;uint32_t wrong;
    (void)inventory_sample; /* part of the shared reader header, not used here */
    /* Verified init: the engine function, the code, the catalog; derived tables are exactly the modelled ones. */
    native_scene();assert(uss_ping_native_ready(&native) && native.verified);
    assert(native.function==EXE+USS_PING_EXE_COMMAND && native.table==TABLE && native.worlds==WORLDS && reads<2165);
    /* Every precondition is individually enforced. */
    native_scene();build_text="25480439";assert(!uss_ping_native_ready(&native));
    native_scene();hide_exe=1;assert(!uss_ping_native_ready(&native));
    native_scene();hide_game=1;assert(!uss_ping_native_ready(&native));
    native_scene();fake.read=NULL;assert(!uss_ping_native_ready(&native));
    native_scene();fake.version=10;assert(!uss_ping_native_ready(&native));
    native_scene();assert(!uss_ping_native_ready(NULL));
    expect_ready_refused(ENGINE_TABLE+0x110,0x11);                                  /* command pointer */
    expect_ready_refused(ENGINE_TABLE+8,0x11);                                      /* destroy pointer */
    expect_ready_refused(ENGINE_TABLE+0x100,0x11);                                  /* property pointer */
    for(i=0;i<37;++i)expect_ready_refused(EXE+USS_PING_EXE_COMMAND+i,(unsigned char)(command_code[i]^1));
    expect_ready_refused(EXE+USS_PING_EXE_DESTROY+0x56,0x4D);expect_ready_refused(EXE+USS_PING_EXE_DESTROY+0x58,0x36);
    expect_ready_refused(EXE+USS_PING_EXE_DESTROY+0x73,0x4C);expect_ready_refused(EXE+USS_PING_EXE_DESTROY+0x75,0x87);
    expect_ready_refused(EXE+USS_PING_EXE_PROPERTY+0x3D,0x49);expect_ready_refused(EXE+USS_PING_EXE_PROPERTY+0x3F,0x06);
    expect_ready_refused(CATALOG+0xF8,103);expect_ready_refused(CATALOG+0xF8,105);
    expect_ready_refused(CATALOG+0x108,0x10);                                       /* masks must follow the hashes */
    expect_ready_refused(HASHES+26*4,0x00);expect_ready_refused(HASHES+37*4,0x00);expect_ready_refused(HASHES+57*4,0x00);
    expect_ready_refused(HASHES+416+26*16+7,1);expect_ready_refused(HASHES+416+37*16+15,1);   /* masks of 26/37 must be zero */
    expect_ready_refused(HASHES+416+57*16+3,0);                                      /* index 57: old must be set */
    expect_ready_refused(HASHES+416+57*16+4,32);                                     /* index 57: new must be clear */
    expect_ready_refused(HASHES+416+94*16+4,0);                                      /* index 94 must differ */
    expect_ready_refused(HASHES+416+10*16+3,4);                                      /* any other index must not differ */
    expect_ready_refused(HASHES+416+103*16+4,32);
    /* Each pinned function pointer must be EXACTLY the pinned address: a pointer to a readable copy of the same code at another
     * address is refused by the pin itself (no other check can catch it). */
    native_scene();memcpy(region(EXE+USS_PING_EXE_COMMAND+0x20,37),command_code,37);put64(at(ENGINE_TABLE)+0x110,EXE+USS_PING_EXE_COMMAND+0x20);
    assert(!uss_ping_native_ready(&native));
    native_scene();memcpy(region(EXE+USS_PING_EXE_DESTROY+0x200,0x80),at(EXE+USS_PING_EXE_DESTROY),0x80);put64(at(ENGINE_TABLE)+8,EXE+USS_PING_EXE_DESTROY+0x200);
    assert(!uss_ping_native_ready(&native));
    native_scene();memcpy(region(EXE+USS_PING_EXE_PROPERTY+0x200,0x66),at(EXE+USS_PING_EXE_PROPERTY),0x66);put64(at(ENGINE_TABLE)+0x100,EXE+USS_PING_EXE_PROPERTY+0x200);
    assert(!uss_ping_native_ready(&native));
    /* A read failure anywhere in init refuses (never crashes, never half-verifies). */
    native_scene();assert(uss_ping_native_ready(&native));total=reads;
    for(i=1;i<=total;++i){native_scene();fail_at=i;assert(!uss_ping_native_ready(&native) && !native.verified);}
    /* Derived addresses that fall outside user space are refused. */
    native_scene();put32(at(EXE+USS_PING_EXE_DESTROY)+0x59,0xD0000000u);assert(!uss_ping_native_ready(&native)); /* table would be negative */

    /* Resolution of the carried weapons' helpers: actor id, body id and the packed filter (low 7 bits = catalog index 26). */
    native_scene();assert(uss_ping_native_ready(&native));
    assert(uss_ping_native_resolve(&native,101,ID_A,&helper) && helper.entity==101 && helper.actor==0x40000011u &&
           helper.body_id==0x50 && (helper.packed&127u)==26 && !memcmp(helper.identity,ID_A,8));
    assert(uss_ping_native_resolve(&native,102,ID_B,&helper) && helper.actor==0x40000012u && helper.body_id==0x51);
    assert(uss_ping_native_resolve(&native,103,ID_B,&helper) && helper.actor==0x40000013u && helper.body_id==0x52);
    assert(reads<2165);
    /* Mid-run filter change is visible (this is how confirmation is read back). */
    put32(at(BODIES)+0x50*160+0x6C,0x00AB0000u|37u);assert(uss_ping_native_resolve(&native,101,ID_A,&helper) && (helper.packed&127u)==37);
    put32(at(BODIES)+0x50*160+0x6C,0x00AB0000u|26u);
    /* Refusals: not verified, bad arguments, identity, unknown entity, every structure check. */
    native_scene();assert(!uss_ping_native_resolve(&native,101,ID_A,&helper));
    native_scene();(void)uss_ping_native_ready(&native);
    assert(!uss_ping_native_resolve(&native,101,ID_B,&helper) && !uss_ping_native_resolve(&native,500,ID_A,&helper) &&
           !uss_ping_native_resolve(&native,0,ID_A,&helper) && !uss_ping_native_resolve(&native,UINT32_MAX,ID_A,&helper) &&
           !uss_ping_native_resolve(&native,101,NULL,&helper) && !uss_ping_native_resolve(&native,101,ID_A,NULL));
    wrong=0;
    #define REFUSE_AFTER(corrupt) do{native_scene();(void)uss_ping_native_ready(&native);corrupt;assert(!uss_ping_native_resolve(&native,101,ID_A,&helper));++wrong;}while(0)
    REFUSE_AFTER(put32(at(MANAGER)+0x0C,17));                                       /* count above capacity */
    REFUSE_AFTER(put32(at(MANAGER)+4,16385));                                       /* capacity too large */
    REFUSE_AFTER(put32(at(MANAGER)+0x0C,0));                                        /* index beyond count */
    REFUSE_AFTER(put32(at(UNIT_OBJECTS)+8,999));                                    /* unit header names another entity */
    REFUSE_AFTER(at(UNIT_OBJECTS)[0]^=1);                                           /* unit header identity */
    REFUSE_AFTER(put32(at(DATA)+0x11*48,0x40000099u));                              /* descriptor key does not equal the actor id */
    REFUSE_AFTER(put32(at(TABLE)+640+0x34,0));                                      /* actor id has no bit of the descriptor mask */
    REFUSE_AFTER(put32(at(TABLE)+640+0x24,0x10));                                   /* index beyond the descriptor's count */
    REFUSE_AFTER(put32(at(TABLE)+640+0x1C,0x08000010u));                            /* stride < 24 */
    REFUSE_AFTER(put32(at(TABLE)+640+0x1C,0x08002000u));                            /* stride > 4096 */
    REFUSE_AFTER(put32(at(TABLE)+640+0x1C,0x08FE0030u));                            /* key offset + 4 > stride */
    REFUSE_AFTER(put32(at(TABLE)+640+0x1C,0x20000030u));                            /* data offset + 24 > stride */
    REFUSE_AFTER(put64(at(TABLE)+640,0));                                           /* no data pointer */
    REFUSE_AFTER(at(GETTER_FN)[3]^=1);                                              /* getter code signature */
    REFUSE_AFTER(put32(at(DATA)+0x11*48+8+0x14,0x00100000u));                       /* body index out of range */
    REFUSE_AFTER(put64(at(GETTER),0));                                              /* no vtable */
    REFUSE_AFTER(put64(at(WORLDS)+0xB0+0x10,0));                                    /* no getter for the world */
    assert(wrong==17);
    /* A body index at the limit is refused by the bound even when that memory is mapped and holds a plausible body. */
    native_scene();(void)uss_ping_native_ready(&native);
    put32(at(DATA)+0x11*48+8+0x14,0x00100000u);put32(region(BODIES+(uintptr_t)0x100000*160,112)+0x6C,0x00AB0000u|26u);
    assert(!uss_ping_native_resolve(&native,101,ID_A,&helper));
    put32(at(DATA)+0x11*48+8+0x14,0x000FFFFFu);put32(region(BODIES+(uintptr_t)0xFFFFF*160,112)+0x6C,0x00AB0000u|26u);
    assert(uss_ping_native_resolve(&native,101,ID_A,&helper) && helper.body_id==0x000FFFFFu);        /* the last valid index works */
    /* Any single failed read refuses; none crashes. */
    native_scene();(void)uss_ping_native_ready(&native);reads=0;assert(uss_ping_native_resolve(&native,101,ID_A,&helper));total=reads;
    for(i=1;i<=total;++i){native_scene();(void)uss_ping_native_ready(&native);reads=0;fail_at=i;assert(!uss_ping_native_resolve(&native,101,ID_A,&helper));}

    /* The one effect: exactly the pinned function, one actor, an accepted hash; the table is re-proven first. */
    native_scene();(void)uss_ping_native_ready(&native);
    assert(uss_ping_native_send(&native,0x40000011u,USS_PING_EXCLUDED_HASH) && invoked==1 && invoked_function==EXE+USS_PING_EXE_COMMAND &&
           invoked_count==1 && invoked_actor==0x40000011u && invoked_hash==USS_PING_EXCLUDED_HASH);
    assert(uss_ping_native_send(&native,0x40000012u,USS_PING_NORMAL_HASH) && invoked==2 && invoked_hash==USS_PING_NORMAL_HASH);
    assert(!uss_ping_native_send(&native,0x40000011u,0x12345678u) && !uss_ping_native_send(&native,0,USS_PING_NORMAL_HASH) &&
           !uss_ping_native_send(&native,UINT32_MAX,USS_PING_NORMAL_HASH) && invoked==2);
    put64(at(ENGINE_TABLE)+0x110,EXE+0x77DA20);                                      /* the game's table now points elsewhere */
    assert(!uss_ping_native_send(&native,0x40000011u,USS_PING_EXCLUDED_HASH) && invoked==2);
    native_scene();assert(!uss_ping_native_send(&native,0x40000011u,USS_PING_EXCLUDED_HASH) && !invoked);        /* not verified */
    native_scene();(void)uss_ping_native_ready(&native);native.invoke=NULL;assert(!uss_ping_native_send(&native,0x40000011u,USS_PING_EXCLUDED_HASH));
    native_scene();(void)uss_ping_native_ready(&native);reads=0;fail_at=1;assert(!uss_ping_native_send(&native,0x40000011u,USS_PING_EXCLUDED_HASH) && !invoked);fail_at=0;
    /* Events are logged with the fixed text; the io table wires the four functions. */
    {UssPingFilterIo io=uss_ping_native_io(&native);
     assert(io.user==&native && io.ready==uss_ping_native_ready && io.resolve==uss_ping_native_resolve && io.send==uss_ping_native_send && io.note==uss_ping_native_note);
     logs=0;io.note(io.user,"APPLY_QUEUED",101,0x40000011u);assert(logs==1 && strstr(last_log,"USS ping filter APPLY_QUEUED entity=101"));}
    steps=0;(void)steps;release_regions();
    puts("USS ping filter native: verified init (every pin/code/catalog byte), helper resolution, 17 structure refusals, every read failure, single pinned engine call PASS");
    return 0;
}
