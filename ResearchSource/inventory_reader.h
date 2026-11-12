/* Own fixed-build diagnostic. Only the SDK read callback touches game addresses. */
#ifndef USS_INVENTORY_READER_H
#define USS_INVENTORY_READER_H
#include "../src/sdk_compat.h"
#include <string.h>
#include <stdio.h>
#include <stdatomic.h>

typedef struct Reader {
    const HfApi* api;
    unsigned char* evidence;
    size_t evidence_size,evidence_capacity;
    unsigned calls, bytes;
    const char* failure;
    const char* stage;
    int slot; /* 0..3 inventory, 4 held, -1 not in a slot check */
    unsigned pass, predicate_valid;
    uint32_t expected_entity, observed_entity, observed_kind;
    unsigned final_valid, final_reasons, difference_mask;
    uint32_t first_unit, second_unit, first_avatar, second_avatar;
    uint32_t descriptor_u12[2], descriptor_u16[2], descriptor_u20[2];
} Reader;
typedef struct Inventory {
    uintptr_t roots[5];
    uintptr_t row_address;
    unsigned char avatar_header[24], row[48];
    uint32_t local_unit, avatar, slots[4], active_selector, held;
    unsigned char identities[5][24];
    uint32_t observed_kinds[5]; /* Unclassified observations, never gameplay approval. */
} Inventory;

static uint32_t u32(const void* p) { uint32_t n; memcpy(&n,p,4); return n; }
static uintptr_t ptr_value(const void* p) { uintptr_t n; memcpy(&n,p,8); return n; }
static int address_ok(uintptr_t a, size_t n)
{
    return a >= 0x10000 && a <= UINT64_C(0x00007fffffffffff) &&
           n <= UINT64_C(0x00007fffffffffff)-a;
}
static int reader_evidence(Reader* r,uintptr_t address,const void* bytes,size_t length)
{
    uint32_t size=(uint32_t)length;
    if (!r->evidence) return 1;
    if (length>UINT32_MAX || r->evidence_size>r->evidence_capacity ||
        r->evidence_capacity-r->evidence_size<12 ||
        length>r->evidence_capacity-r->evidence_size-12) {
        r->failure="EVIDENCE_BOUNDS";return 0;
    }
    memcpy(r->evidence+r->evidence_size,&address,8);
    memcpy(r->evidence+r->evidence_size+8,&size,4);
    memcpy(r->evidence+r->evidence_size+12,bytes,length);
    r->evidence_size+=12+length;return 1;
}
static int get(Reader* r, uintptr_t a, void* out, size_t n)
{
    unsigned char temporary[72];
    if (!n || n>sizeof(temporary) || !address_ok(a,n) ||
        r->calls>=2165 || r->bytes>20000 || n>20000-r->bytes) { r->failure="READ_BOUNDS"; return 0; }
    ++r->calls; r->bytes+=(unsigned)n;
    if (r->api->read(a,temporary,n)!=0) { r->failure="READ_STATUS"; return 0; }
    if (!reader_evidence(r,a,temporary,n)) return 0;
    memcpy(out,temporary,n); /* Failed partial writes never reach decoded output. */
    return 1;
}
static int get_ptr(Reader* r, uintptr_t a, uintptr_t* out)
{
    return get(r,a,out,8) && address_ok(*out,72);
}
static int lookup(Reader* r, uintptr_t table, uint32_t key, uint32_t* index)
{
    unsigned char h[20],row[8];
    uint32_t cap,empty,mult,p;
    uintptr_t data;
    if(!get(r,table,h,20)) return 0;
    data=ptr_value(h); cap=u32(h+8); empty=u32(h+12); mult=u32(h+16);
    if(!cap || cap>1048576 || (cap&(cap-1)) || !address_ok(data,(size_t)cap*8)) return 0;
    for(p=0;p<cap && p<128;++p) {
        uint32_t bucket=(key*mult+p)&(cap-1);
        if(!get(r,data+(uintptr_t)bucket*8,row,8)) return 0;
        if(u32(row)==key) { *index=u32(row+4); return *index!=UINT32_MAX; }
        if(u32(row)==empty) return 0;
    }
    return 0;
}
static unsigned hex_digit(char c) { return (unsigned)(c<='9'?c-'0':c-'a'+10); }
static int valid_id(uint32_t id) { return id && id!=UINT32_MAX; }

/* No scanner fallback. Mismatch on this exact build refuses the whole attempt. */
static int resolve(Reader* r, uintptr_t base, size_t size, uintptr_t globals[5])
{
    static const uint32_t rvas[]={0x518C2A,0x3A4BDE,0x3A9E00,0x4AABCA,0xC30B8};
    static const uint32_t targets[]={0x3326B90,0x3326468,0x3326738,0x3326420,0x3326DC0};
    static const char* patterns[]={
        "488b1d????????488bf9448b4308443b4328720c488bcbe8ca490d00448b4308",
        "488b05????????488d542448488b88e8000000448b4108e8361a2600488b0d2f",
        "4c8b05????????33db8bcb458b4830458b5038440fafd2418d71ff4585c9744c",
        "4c8b15????????7517b8ffffffff8bc84869c1d0010000490342604883c408c3",
        "488b3d????????4c89742438448b4f28448b5f30440fafd8458d71ff4585c90f"};
    unsigned i,j;
    if(size<0x3326DC8 || size>0x20000000 || !address_ok(base,size)) return 0;
    for(i=0;i<5;++i) {
        unsigned char bytes[32];
        size_t length=strlen(patterns[i])/2;
        int32_t displacement;
        if(length>sizeof(bytes) || !get(r,base+rvas[i],bytes,length)) return 0;
        for(j=0;j<length;++j) if(patterns[i][j*2]!='?') {
            unsigned expected=16*hex_digit(patterns[i][j*2])+hex_digit(patterns[i][j*2+1]);
            if(bytes[j]!=expected) return 0;
        }
        memcpy(&displacement,bytes+3,4);
        if((int64_t)rvas[i]+7+displacement!=targets[i]) return 0;
        globals[i]=base+targets[i];
    }
    return 1;
}
static int weapon(Reader* r, uintptr_t global, uintptr_t manager, uint32_t id, unsigned char out[24], uint32_t* kind)
{
    unsigned char h[72],again[72],row[20],descriptor[24];
    uintptr_t unit,now;
    uint32_t i,count;
    if(!valid_id(id)) return 1; /* Empty slots remain raw sentinels, never dereferenced. */
    r->stage="WEAPON_HEADER_LOOKUP";
    if(!get(r,manager,h,72) || !lookup(r,manager+0x20,id,&i)) return 0;
    count=u32(h+12);
    r->stage="WEAPON_COUNT_ARRAYS";
    if(count>16384 || count>u32(h) || i>=count || i>=16384) return 0;
    if(!address_ok(ptr_value(h+56),(size_t)count*8) ||
       !address_ok(ptr_value(h+64),(size_t)count*48)) return 0;
    r->stage="WEAPON_DESCRIPTOR_ROW";
    if(!get_ptr(r,ptr_value(h+56)+(uintptr_t)i*8,&unit) || !get(r,unit,out,24) ||
       !get(r,ptr_value(h+64)+(uintptr_t)i*48,row,20)) return 0;
    r->stage="WEAPON_PREDICATE";
    r->predicate_valid=1; r->expected_entity=id;
    r->observed_entity=u32(out+8); r->observed_kind=u32(row+12);
    *kind=u32(row+12);
    /* Category admission belongs to future typed gameplay services. This reader
     * only observes the already-read kind, retaining structural ID checks. */
    if(u32(out+8)!=id) return 0;
    r->stage="WEAPON_RECHECK";
    return get_ptr(r,global,&now) && now==manager && get(r,manager,again,72) &&
           !memcmp(h,again,72) && get(r,unit,descriptor,24) && !memcmp(out,descriptor,24);
}
static int graph(Reader* r, const uintptr_t globals[5], Inventory* out)
{
    unsigned k;
    uint32_t oi,ii,hi;
    uintptr_t owned,rows,held_rows;
    unsigned char arrays[24],descriptor[24];
    memset(out,0,sizeof(*out));
    r->predicate_valid=0;
    r->expected_entity=0; r->observed_entity=0; r->observed_kind=0;
    r->slot=-1; r->stage="MANAGER_ROOTS";
    for(k=0;k<5;++k) if(!get_ptr(r,globals[k],&out->roots[k])) return 0;
    r->stage="LOCAL_OWNER_AVATAR";
    if(!get(r,out->roots[1]+0x3A8,&out->local_unit,4) || !valid_id(out->local_unit) ||
       !lookup(r,out->roots[0]+0xF21A88,out->local_unit,&oi) || oi>1048576 ||
       !get(r,out->roots[0]+0xF31AD8+(uintptr_t)oi*24,out->avatar_header,24)) return 0;
    out->avatar=u32(out->avatar_header+8);
    r->stage="INVENTORY_LOOKUP_ARRAYS";
    if(!valid_id(out->avatar) || !lookup(r,out->roots[2]+40,out->avatar,&ii) || ii>4096 ||
       !get(r,out->roots[2]+64,arrays,24)) return 0;
    rows=ptr_value(arrays+16);
    r->stage="INVENTORY_OWNER_ROW";
    if(!address_ok(ptr_value(arrays),(size_t)(ii+1)*8) || !address_ok(rows,(size_t)(ii+1)*48) ||
       !get_ptr(r,ptr_value(arrays)+(uintptr_t)ii*8,&owned) ||
       !get(r,owned,descriptor,24) || memcmp(descriptor,out->avatar_header,24) ||
       !get(r,rows+(uintptr_t)ii*48,out->row,48)) return 0;
    out->row_address=rows+(uintptr_t)ii*48;
    for(k=0;k<4;++k) out->slots[k]=u32(out->row+k*4);
    out->active_selector=u32(out->row+28);
    r->stage="HELD_LOOKUP_ROW";
    if(!lookup(r,out->roots[3]+0x30,out->avatar,&hi) || hi>=16384 ||
       !get_ptr(r,out->roots[3]+0x60,&held_rows) ||
       !get(r,held_rows+(uintptr_t)hi*0x1D0,&out->held,4)) return 0;
    for(k=0;k<5;++k) {
        r->slot=(int)k; r->predicate_valid=0;
        if(!weapon(r,globals[4],out->roots[4],k<4?out->slots[k]:out->held,out->identities[k],&out->observed_kinds[k])) return 0;
    }
    return 1;
}
/* Final reasons: bit1 reserved (former unproven event/avatar check), copied samples=2, module base=4, size=8.
 * Difference bits: roots=1, avatar descriptor=2, local unit=4, row=8,
 * slots=16, held=32, weapon descriptors=64, observed kinds=128, avatar ID=256, row address=512.
 * These explain a refusal; they never turn partial evidence into approval. */
static int final_context(Reader* r, const Inventory* first, const Inventory* second,
                         uintptr_t base, uintptr_t after_base,
                         size_t size, size_t after_size)
{
    unsigned differences=0, reasons=0;
    if(memcmp(first->roots,second->roots,sizeof(first->roots))) differences|=1;
    if(memcmp(first->avatar_header,second->avatar_header,24)) differences|=2;
    if(first->local_unit!=second->local_unit) differences|=4;
    if(memcmp(first->row,second->row,48)) differences|=8;
    if(memcmp(first->slots,second->slots,sizeof(first->slots))) differences|=16;
    if(first->held!=second->held) differences|=32;
    if(memcmp(first->identities,second->identities,sizeof(first->identities))) differences|=64;
    if(memcmp(first->observed_kinds,second->observed_kinds,sizeof(first->observed_kinds))) differences|=128;
    if(first->avatar!=second->avatar) differences|=256;
    if(first->row_address!=second->row_address) differences|=512;
    /* Keep the original whole-sample equality guard, including every copied field. */
    if(memcmp(first,second,sizeof(*first))) reasons|=2;
    if(base!=after_base) reasons|=4;
    if(size!=after_size) reasons|=8;
    r->stage="FINAL_CONTEXT"; r->slot=-1; r->predicate_valid=0;
    r->expected_entity=0; r->observed_entity=0; r->observed_kind=0;
    r->final_valid=1; r->final_reasons=reasons; r->difference_mask=differences;
    r->first_unit=first->local_unit; r->second_unit=second->local_unit;
    r->first_avatar=first->avatar; r->second_avatar=second->avatar;
    /* Copied words only: documented unit/network/flags interpretation is not
     * an assumed conversion from the SDK event's entity field. */
    r->descriptor_u12[0]=u32(first->avatar_header+12);
    r->descriptor_u12[1]=u32(second->avatar_header+12);
    r->descriptor_u16[0]=u32(first->avatar_header+16);
    r->descriptor_u16[1]=u32(second->avatar_header+16);
    r->descriptor_u20[0]=u32(first->avatar_header+20);
    r->descriptor_u20[1]=u32(second->avatar_header+20);
    if(reasons) { r->failure="CONTEXT_CHANGED"; return 0; }
    return 1;
}
/* Local target follows the ownership chain, as in Lua sample().
 * Spawn event IDs are metadata; their relationship to avatar IDs is unverified. */
static int inventory_sample(Reader* r, Inventory* output)
{
    uintptr_t base,globals[5];
    size_t size=0,after_size=0;
    Inventory first,second;
    base=r->api->module_base("game.dll",&size);
    r->slot=-1; r->stage="MODULE_PINS";
    if(!base || !resolve(r,base,size,globals)) { r->failure="MODULE_PINS"; return 0; }
    r->pass=1;
    if(!graph(r,globals,&first)) {
        if(!r->failure) r->failure="GRAPH_IDENTITY";
        return 0;
    }
    r->pass=2;
    if(!graph(r,globals,&second)) {
        if(!r->failure) r->failure="GRAPH_IDENTITY";
        return 0;
    }
    /* Always collect final module metadata, even when another check will refuse. */
    {
        uintptr_t after_base=r->api->module_base("game.dll",&after_size);
        if(!final_context(r,&first,&second,base,after_base,size,after_size)) return 0;
    }
    *output=first;
    return 1;
}
#endif

