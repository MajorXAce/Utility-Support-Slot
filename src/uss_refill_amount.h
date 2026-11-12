/* Native settings policy: per-entity override wins; proven absence alone permits
 * resource fallback. Zero is a valid amount, never an unknown-value sentinel. */
#ifndef USS_REFILL_AMOUNT_H
#define USS_REFILL_AMOUNT_H
#include "uss_refill_components.h"
static inline int refill_resource_address(UssRefillMemory* r,const unsigned char id[8],unsigned kind,uintptr_t* output)
{
    uintptr_t owner,table,again,bucket,address;unsigned cap=kind==USS_REFILL_MAG?540:50;
    uintptr_t offset=kind==USS_REFILL_MAG?0xf124a0:0xf12820,stride=kind==USS_REFILL_MAG?160:136;
    unsigned first=0,j,probe;unsigned char row[16],check[16],zero[8]={0};
    if(!memcmp(id,zero,8) || !refill_read_pointer(r,r->io->base+0x346bf98,&owner) ||
        !refill_read_pointer(r,owner+offset,&table))return 0;
    for(j=8;j>0;--j)first=(first*256+id[j-1])%cap;
    for(probe=0;probe<cap && probe<128;++probe){
        bucket=table+(uintptr_t)((first+probe)%cap)*16;
        if(!refill_read(r,bucket,row,16))return 0;
        if(!memcmp(row,id,8)){
            uint32_t index=refill_u32(row+8);if(index>=1048576)return 0;
            address=table+(kind==USS_REFILL_MAG?((uintptr_t)index+54)*stride:800+(uintptr_t)index*stride);
            if(!refill_read_pointer(r,r->io->base+0x346bf98,&again) || again!=owner ||
                !refill_read_pointer(r,owner+offset,&again) || again!=table ||
                !refill_read(r,bucket,check,16) || memcmp(row,check,16))return 0;
            *output=address;return 1;
        }
        if(!memcmp(row,zero,8))return 0;
    }
    return 0;
}
static inline int uss_refill_direct_amount(const UssRefillMemoryIo* io,const UssRefillComponent* component,uint32_t* output)
{
    UssRefillMemory r={0};UssRefillComponent current;uintptr_t address,again,rows=0,hash_offset,rows_offset,stride,amount_offset;
    uint32_t index=0,again_index=0,amount,check;int found,again_found;
    if(!io || !io->context || !io->read || !component || !output ||
        (component->kind!=USS_REFILL_MAG && component->kind!=USS_REFILL_ROUNDS) ||
        io->base<0x10000 || io->base>UINTPTR_MAX-0x346bfa0 ||
        io->context(io->user)!=component->epoch)return 0;
    if(uss_refill_component_read(io,component->entity,component->identity,component->kind,&current)!=1 ||
        current.epoch!=component->epoch || current.manager!=component->manager || current.unit!=component->unit ||
        current.index!=component->index || current.reserve!=component->reserve)return 0;
    hash_offset=component->kind==USS_REFILL_MAG?0x60:0x68;
    rows_offset=component->kind==USS_REFILL_MAG?0xa0:0xa8;
    stride=component->kind==USS_REFILL_MAG?160:136;
    amount_offset=component->kind==USS_REFILL_MAG?0x90:0x54;
    r.io=io;found=refill_hash(&r,component->manager+hash_offset,component->entity,&index);
    if(found<0)return 0;
    if(found){
        if(index>=16384 || !refill_read_pointer(&r,component->manager+rows_offset,&rows))return 0;
        address=rows+(uintptr_t)index*stride;
    }else if(!refill_resource_address(&r,component->identity,component->kind,&address))return 0;
    if(!refill_read(&r,address+amount_offset,&amount,4) || amount>1000)return 0;
    again_found=refill_hash(&r,component->manager+hash_offset,component->entity,&again_index);
    if(again_found!=found || (found && again_index!=index))return 0;
    if(found){if(!refill_read_pointer(&r,component->manager+rows_offset,&again) || again!=rows)return 0;}
    else if(!refill_resource_address(&r,component->identity,component->kind,&again) || again!=address)return 0;
    if(!refill_read(&r,address+amount_offset,&check,4) || amount!=check ||
        uss_refill_component_read(io,component->entity,component->identity,component->kind,&current)!=1 ||
        current.epoch!=component->epoch || current.manager!=component->manager || current.unit!=component->unit ||
        current.index!=component->index || current.reserve!=component->reserve || io->context(io->user)!=component->epoch)return 0;
    *output=amount;return 1;
}
#endif
