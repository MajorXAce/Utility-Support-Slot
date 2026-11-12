/* Read-only production ground-source policy, ported from resupply_policy.lua.
 * Bounded hash absence is distinct from unreadable/changed/exhausted evidence. */
#ifndef USS_REFILL_SOURCE_POLICY_H
#define USS_REFILL_SOURCE_POLICY_H
#include "uss_refill_transaction.h"
#include <stddef.h>
typedef struct UssRefillMemoryIo {
    void* user;uintptr_t base;
    int (*read)(void*,uintptr_t,void*,size_t);
    uint64_t (*context)(void*);
} UssRefillMemoryIo;
typedef struct UssRefillMemory {const UssRefillMemoryIo* io;unsigned calls,bytes,failed;} UssRefillMemory;
static inline uint32_t refill_u32(const unsigned char* p){uint32_t n;memcpy(&n,p,4);return n;}
static inline uintptr_t refill_pointer(const unsigned char* p){uintptr_t n;memcpy(&n,p,8);return n;}
static inline int refill_read(UssRefillMemory* r,uintptr_t at,void* out,size_t length)
{
    if(r->failed || at<0x10000 || at>UINTPTR_MAX-length || !length || length>512 || r->calls>=512 || length>8192-r->bytes){r->failed=1;return 0;}
    ++r->calls;r->bytes+=(unsigned)length;if(!r->io->read(r->io->user,at,out,length)){r->failed=1;return 0;}return 1;
}
static inline int refill_read_pointer_alignment(UssRefillMemory* r,uintptr_t at,uintptr_t* out,unsigned alignment)
{return refill_read(r,at,out,8) && *out>=0x10000 && *out<UINT64_C(0x800000000000) && !(*out&(alignment-1));}
static inline int refill_read_pointer(UssRefillMemory* r,uintptr_t at,uintptr_t* out)
{return refill_read_pointer_alignment(r,at,out,8);}
/* 1 found, 0 proven absent, -1 unknown. Optional diagnostics describe
 * the same reads; they neither relax admission nor perform another lookup. */
typedef struct UssRefillHashDiagnostic {
    const char* stage;uint32_t capacity,empty,multiplier,alignment,key,probe,found,index;
} UssRefillHashDiagnostic;
static inline int refill_hash_diagnose(UssRefillMemory* r,uintptr_t at,uint32_t key,uint32_t* index,
    UssRefillHashDiagnostic* diagnostic)
{
    unsigned char header[20],again[20],row[8],row_again[8];uintptr_t rows;uint32_t cap,empty,mult,first,probe;
#define USS_REFILL_HASH_STAGE(name) do {if(diagnostic)diagnostic->stage=(name);}while(0)
    if(diagnostic){memset(diagnostic,0,sizeof(*diagnostic));diagnostic->key=key;}
    USS_REFILL_HASH_STAGE("HASH_HEADER_READ");
    if(!refill_read(r,at,header,20))return -1;
    cap=refill_u32(header+8);empty=refill_u32(header+12);mult=refill_u32(header+16);rows=refill_pointer(header);
    if(diagnostic){diagnostic->capacity=cap;diagnostic->empty=empty;diagnostic->multiplier=mult;diagnostic->alignment=(uint32_t)(rows&7);}
    USS_REFILL_HASH_STAGE("HASH_CAPACITY");
    if(!cap || cap>1048576 || (cap&(cap-1)))return -1;
    USS_REFILL_HASH_STAGE("HASH_ARRAY_RANGE");
    if(rows<0x10000 || rows>=UINT64_C(0x800000000000) || rows>UINTPTR_MAX-(uintptr_t)cap*8)return -1;
    USS_REFILL_HASH_STAGE("HASH_ARRAY_ALIGNMENT");
    /* Buckets contain two uint32_t fields. Their eight-byte stride does not
     * require an eight-byte base: the live action table is four-byte aligned. */
    if(rows&3)return -1;
    first=(uint32_t)((uint64_t)(key&(cap-1))*(mult&(cap-1)))&(cap-1);
    for(probe=0;probe<cap && probe<128;++probe){
        uintptr_t bucket=rows+(uintptr_t)((first+probe)&(cap-1))*8;uint32_t found;
        if(diagnostic)diagnostic->probe=probe;
        USS_REFILL_HASH_STAGE("HASH_BUCKET_READ");
        if(!refill_read(r,bucket,row,8))return -1;found=refill_u32(row);
        if(diagnostic){diagnostic->found=found;diagnostic->index=refill_u32(row+4);}
        if(found!=key && found!=empty)continue;
        USS_REFILL_HASH_STAGE("HASH_HEADER_RECHECK_READ");
        if(!refill_read(r,at,again,20))return -1;
        USS_REFILL_HASH_STAGE("HASH_HEADER_CHANGED");
        if(memcmp(header,again,20))return -1;
        USS_REFILL_HASH_STAGE("HASH_BUCKET_RECHECK_READ");
        if(!refill_read(r,bucket,row_again,8))return -1;
        USS_REFILL_HASH_STAGE("HASH_BUCKET_CHANGED");
        if(memcmp(row,row_again,8))return -1;
        USS_REFILL_HASH_STAGE("HASH_ABSENT");
        if(found!=key || refill_u32(row+4)==UINT32_MAX)return 0;
        USS_REFILL_HASH_STAGE("HASH_FOUND");*index=refill_u32(row+4);return 1;
    }
    USS_REFILL_HASH_STAGE("HASH_PROBE_BOUND");return -1;
#undef USS_REFILL_HASH_STAGE
}
static inline int refill_hash(UssRefillMemory* r,uintptr_t at,uint32_t key,uint32_t* index)
{return refill_hash_diagnose(r,at,key,index,NULL);}
static inline int uss_refill_ground_scale(const UssRefillMemoryIo* io,uint32_t source,uint32_t kind,float* output)
{
    UssRefillMemory r={0};uintptr_t manager,again_manager,units,rows,unit,again_unit;uint64_t epoch;
    unsigned char header[80],again_header[80],descriptor[24],again_descriptor[24],row[56],again_row[56];
    uint32_t index=0,again_index=0,n,j;int found,again_found;float scale;
    if(!io || !io->read || !io->context || !output || !source || source==UINT32_MAX || io->base<0x10000 || io->base>UINTPTR_MAX-0x3326ac8)return 0;
    if(kind!=4 && kind!=7 && kind!=8 && kind!=9)return 0;
    r.io=io;epoch=io->context(io->user);if(!epoch)return 0;
    if(kind==4){if(io->context(io->user)!=epoch)return 0;*output=0.5f;return 1;}
    if(!refill_read_pointer(&r,io->base+0x3326ac0,&manager) || !refill_read(&r,manager,header,80))return 0;
    found=refill_hash(&r,manager+0x18,source,&index);if(found<0)return 0;
    if(!refill_read(&r,io->base+0x21417b4,&scale,4))return 0;
    if(found){
        if(index>=16384)return 0;units=refill_pointer(header+0x30);rows=refill_pointer(header+0x40);
        if(!units || !rows || units>UINTPTR_MAX-(uintptr_t)index*8 || rows>UINTPTR_MAX-(uintptr_t)index*56 ||
            !refill_read_pointer(&r,units+(uintptr_t)index*8,&unit) || !refill_read(&r,unit,descriptor,24) || refill_u32(descriptor+8)!=source ||
            !refill_read(&r,rows+(uintptr_t)index*56,row,56))return 0;
        n=refill_u32(row);if(n>6)return 0;
        for(j=0;j<n;++j){uint32_t property=refill_u32(row+4+j*4);if(!property)break;
            if(property==9){uint32_t values=refill_u32(row+0x1c);if(values>6 || j>=values)return 0;memcpy(&scale,row+0x20+j*4,4);break;}}
        if(!refill_read_pointer(&r,units+(uintptr_t)index*8,&again_unit) || again_unit!=unit ||
            !refill_read(&r,unit,again_descriptor,24) || memcmp(descriptor,again_descriptor,24) ||
            !refill_read(&r,rows+(uintptr_t)index*56,again_row,56) || memcmp(row,again_row,56))return 0;
    }
    again_found=refill_hash(&r,manager+0x18,source,&again_index);
    if(again_found!=found || (found && again_index!=index) ||
        !refill_read_pointer(&r,io->base+0x3326ac0,&again_manager) || again_manager!=manager ||
        !refill_read(&r,manager,again_header,80) || memcmp(header,again_header,80) ||
        io->context(io->user)!=epoch || !uss_refill_scale_valid(scale))return 0;
    *output=scale;return 1;
}
#endif
