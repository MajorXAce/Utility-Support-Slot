/* Read-only ammo/deposit component snapshots. Caller still proves pair ownership.
 * Missing component (0) and unknown evidence (-1) must remain distinct. */
#ifndef USS_REFILL_COMPONENTS_H
#define USS_REFILL_COMPONENTS_H
#include "uss_refill_source_policy.h"
typedef struct UssRefillComponent {
    uintptr_t manager,unit;uint64_t epoch;
    uint32_t entity,index,reserve,raw,kind;unsigned char identity[8];
} UssRefillComponent;
static inline int uss_refill_component_read(const UssRefillMemoryIo* io,
    uint32_t entity,const unsigned char identity[8],unsigned kind,UssRefillComponent* output)
{
    UssRefillMemory r={0};UssRefillComponent result={0};
    uintptr_t global,hash_offset,units_offset,rows_offset,stride,manager,again_manager,units,rows,unit,again_unit;
    unsigned char header[96],again_header[96],descriptor[24],again_descriptor[24],row[8],again_row[8];
    uint32_t index=0,again_index=0,limit;size_t row_size,header_size;int found,again_found;
    if(!io || !io->read || !io->context || !identity || !output || !entity || entity==UINT32_MAX ||
        io->base<0x10000 || io->base>UINTPTR_MAX-0x3326cf8)return -1;
    if(kind==USS_REFILL_MAG){global=0x3326648;hash_offset=0x20;units_offset=0x38;rows_offset=0x50;stride=12;limit=1048576;row_size=4;}
    else if(kind==USS_REFILL_ROUNDS){global=0x3326cf0;hash_offset=0x28;units_offset=0x40;rows_offset=0x58;stride=20;limit=1048576;row_size=4;}
    else if(kind==3){global=0x33265e8;hash_offset=0x20;units_offset=0x38;rows_offset=0x50;stride=8;limit=4096;row_size=8;}
    else return -1;
    header_size=(size_t)rows_offset+8;
    r.io=io;result.epoch=io->context(io->user);if(!result.epoch)return -1;
    if(!refill_read_pointer(&r,io->base+global,&manager) || !refill_read(&r,manager,header,header_size))return -1;
    found=refill_hash(&r,manager+hash_offset,entity,&index);if(found<0)return -1;
    if(found){
        if(index>=limit)return -1;
        units=refill_pointer(header+units_offset);rows=refill_pointer(header+rows_offset);
        if(units>UINTPTR_MAX-(uintptr_t)index*8 || rows>UINTPTR_MAX-(uintptr_t)index*stride ||
            !refill_read_pointer(&r,units+(uintptr_t)index*8,&unit) ||
            !refill_read(&r,unit,descriptor,24) || refill_u32(descriptor+8)!=entity || memcmp(descriptor,identity,8) ||
            !refill_read(&r,rows+(uintptr_t)index*stride,row,row_size) || refill_u32(row)>1000)return -1;
        if(!refill_read_pointer(&r,units+(uintptr_t)index*8,&again_unit) || unit!=again_unit ||
            !refill_read(&r,unit,again_descriptor,24) || memcmp(descriptor,again_descriptor,24) ||
            !refill_read(&r,rows+(uintptr_t)index*stride,again_row,row_size) || memcmp(row,again_row,row_size))return -1;
        result.manager=manager;result.unit=unit;result.entity=entity;result.index=index;
        result.reserve=refill_u32(row);result.raw=row_size==8?refill_u32(row+4):0;result.kind=kind;memcpy(result.identity,identity,8);
    }
    again_found=refill_hash(&r,manager+hash_offset,entity,&again_index);
    if(again_found!=found || (found && again_index!=index) ||
        !refill_read_pointer(&r,io->base+global,&again_manager) || manager!=again_manager ||
        !refill_read(&r,manager,again_header,header_size) || memcmp(header,again_header,header_size) ||
        io->context(io->user)!=result.epoch)return -1;
    if(!found)return 0;
    *output=result;return 1;
}
/* Never fall back to rounds after an unreadable or malformed magazine component. */
static inline int uss_refill_ammo_read(const UssRefillMemoryIo* io,uint32_t entity,
    const unsigned char identity[8],UssRefillComponent* output)
{
    int result=uss_refill_component_read(io,entity,identity,USS_REFILL_MAG,output);
    return result==0?uss_refill_component_read(io,entity,identity,USS_REFILL_ROUNDS,output):result;
}
#endif
