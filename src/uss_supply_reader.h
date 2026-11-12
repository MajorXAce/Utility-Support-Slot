/* Actual Pack charge and local ammo/grenade/stim signal snapshots.
 * Missing optional signals stay unknown via signal_mask, never fabricated zero. */
#ifndef USS_SUPPLY_READER_H
#define USS_SUPPLY_READER_H
#include "uss_supply_observer.h"
static inline int uss_supply_counts(const UssRefillMemoryIo* io,const UssRefillTarget* target,
    uintptr_t row_address,uint32_t output[2])
{
    UssRefillMemory r={0};uintptr_t manager,again,rows,units,extended,unit,check_unit;uint32_t index;
    unsigned char header[96],check_header[96],descriptor[24],check_descriptor[24],row[8],check_row[8];
    if(!io || !io->read || !io->context || !output || !uss_refill_target_valid(target) ||
        io->base<0x10000 || io->base>UINTPTR_MAX-0x3326740 || io->context(io->user)!=target->epoch)return 0;
    r.io=io;
    if(!refill_read_pointer(&r,io->base+0x3326738,&manager) || manager!=target->inventory ||
        !refill_read(&r,manager,header,96))return 0;
    units=refill_pointer(header+0x40);rows=refill_pointer(header+0x50);extended=refill_pointer(header+0x58);
    if(row_address<rows || (row_address-rows)%48 || (row_address-rows)/48>=4096)return 0;
    index=(uint32_t)((row_address-rows)/48);
    if(units>UINTPTR_MAX-(uintptr_t)index*8 || extended>UINTPTR_MAX-(uintptr_t)index*8 ||
        !refill_read_pointer(&r,units+(uintptr_t)index*8,&unit) || !refill_read(&r,unit,descriptor,24) ||
        refill_u32(descriptor+8)!=target->avatar || !refill_read(&r,extended+(uintptr_t)index*8,row,8) ||
        refill_u32(row)>100 || refill_u32(row+4)>100)return 0;
    if(!refill_read_pointer(&r,io->base+0x3326738,&again) || again!=manager ||
        !refill_read(&r,manager,check_header,96) || memcmp(header,check_header,96) ||
        !refill_read_pointer(&r,units+(uintptr_t)index*8,&check_unit) || unit!=check_unit ||
        !refill_read(&r,unit,check_descriptor,24) || memcmp(descriptor,check_descriptor,24) ||
        !refill_read(&r,extended+(uintptr_t)index*8,check_row,8) || memcmp(row,check_row,8) ||
        io->context(io->user)!=target->epoch)return 0;
    output[0]=refill_u32(row);output[1]=refill_u32(row+4);return 1;
}
static inline int uss_supply_read(const UssRefillMemoryIo* io,const UssRefillTarget* target,
    uintptr_t inventory_row,const unsigned char identities[3][8],UssSupplySnapshot* output)
{
    static const unsigned char pack_id[8]={0x58,0x9a,0x23,0x09,0x71,0xa4,0xf9,0x4e};
    UssSupplySnapshot snapshot={0};UssRefillComponent check,ammo;uint32_t entities[3],counts[2];unsigned i;
    if(!output || !identities || !uss_refill_target_valid(target))return 0;
    snapshot.target=*target;snapshot.primary=target->primary;snapshot.secondary=target->secondary;snapshot.backpack=target->backpack;
    if(uss_refill_component_read(io,target->backpack,pack_id,3,&snapshot.pack)!=1 ||
        !uss_supply_valid(&snapshot))return 0;
    entities[0]=target->primary;entities[1]=target->secondary;entities[2]=target->real_support;
    for(i=0;i<3;++i)if(uss_refill_ammo_read(io,entities[i],identities[i],&ammo)==1 && ammo.epoch==target->epoch){
        snapshot.signals[i]=ammo.reserve;snapshot.signal_mask|=1u<<i;}
    if(uss_supply_counts(io,target,inventory_row,counts)){
        snapshot.signals[3]=counts[0];snapshot.signals[4]=counts[1];snapshot.signal_mask|=24;}
    if(uss_refill_component_read(io,target->backpack,pack_id,3,&check)!=1 ||
        check.epoch!=snapshot.pack.epoch || check.manager!=snapshot.pack.manager || check.unit!=snapshot.pack.unit ||
        check.index!=snapshot.pack.index || check.reserve!=snapshot.pack.reserve || check.raw!=snapshot.pack.raw ||
        uss_refill_component_read(io,target->target,target->identity,target->kind,&ammo)!=1 ||
        ammo.epoch!=target->epoch || ammo.manager!=target->ammo_manager || ammo.unit!=target->ammo_unit ||
        ammo.index!=target->index || ammo.reserve!=target->reserve || io->context(io->user)!=target->epoch)return 0;
    *output=snapshot;return 1;
}
#endif
