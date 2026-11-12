/* Read-only HUD name/icon lookup, translated from our hud_weapon_metadata.lua.
 * Fixed resource buckets only, never a world scan. Name bytes are opaque IDs,
 * including high-bit values; they must never be interpreted as pointers. */
#ifndef USS_HUD_METADATA_H
#define USS_HUD_METADATA_H
#include "uss_hud_pins.h"
#include "publication_capture.h"
typedef struct UssHudMetadataRead {const HfApi* api;unsigned calls,bytes;} UssHudMetadataRead;
typedef struct UssHudMetadataEntry {
    unsigned valid,override;uintptr_t root,owner,weapons,icons,name_address,icon_address,projectile_table;
    UssPairMember member;unsigned char display[12];
} UssHudMetadataEntry;
typedef struct UssHudMetadata {UssHudMetadataEntry entries[2];} UssHudMetadata;
static int hud_metadata_read(UssHudMetadataRead* io,uintptr_t at,void* out,size_t length)
{
    unsigned char bytes[4096];
    if(!io || !io->api || !io->api->read || !out || !length || length>sizeof(bytes) ||
        !address_ok(at,length) || io->calls>=256 || io->bytes>65536 || length>65536-io->bytes)return 0;
    ++io->calls;io->bytes+=(unsigned)length;
    if(io->api->read(at,bytes,length)!=0)return 0;
    memcpy(out,bytes,length);return 1;
}
static int hud_metadata_pointer(UssHudMetadataRead* io,uintptr_t at,uintptr_t* pointer)
{return hud_metadata_read(io,at,pointer,8) && address_ok(*pointer,72);}
static int hud_metadata_asset(UssHudMetadataRead* io,uintptr_t at,const unsigned char id[8],
    unsigned slots,unsigned bias,unsigned stride,uintptr_t* output)
{
    uintptr_t table=0,again=0;unsigned block,index;unsigned char rows[4096],row[16];
    if(!hud_metadata_pointer(io,at,&table))return -1;
    for(block=0;block<slots;block+=256){
        unsigned count=slots-block;if(count>256)count=256;
        if(!hud_metadata_read(io,table+(uintptr_t)block*16,rows,(size_t)count*16))return -1;
        for(index=0;index<count;++index)if(!memcmp(rows+index*16,id,8)){
            uint32_t entry=u32(rows+index*16+8);uintptr_t address;
            if(entry>1048576)return -1;
            address=table+((uintptr_t)entry+bias)*stride;
            if(!address_ok(address,56) || !hud_metadata_pointer(io,at,&again) || again!=table ||
                !hud_metadata_read(io,table+(uintptr_t)(block+index)*16,row,16) || memcmp(row,rows+index*16,16))return -1;
            *output=address;return 1;
        }
    }
    return 0;
}
static int hud_metadata_unit(const HfApi* api,uintptr_t base,const UssPairMember* member)
{
    Reader reader={0};uintptr_t manager=0,again=0,unit=0,end_unit=0;uint32_t index=0;
    unsigned char header[72],after[72],descriptor[24],end_descriptor[24];
    reader.api=api;
    return publication_id(member->entity) && get_ptr(&reader,base+0x3326dc0,&manager) &&
        get(&reader,manager,header,72) && lookup(&reader,manager+0x20,member->entity,&index) &&
        u32(header+12)<=16384 && index<u32(header+12) && index<u32(header) &&
        address_ok(ptr_value(header+56),(size_t)u32(header+12)*8) &&
        get_ptr(&reader,ptr_value(header+56)+(uintptr_t)index*8,&unit) && get(&reader,unit,descriptor,24) &&
        u32(descriptor+8)==member->entity && !memcmp(descriptor,member->identity,8) &&
        get_ptr(&reader,base+0x3326dc0,&again) && again==manager && get(&reader,manager,after,72) &&
        !memcmp(header,after,72) && get_ptr(&reader,ptr_value(header+56)+(uintptr_t)index*8,&end_unit) && end_unit==unit &&
        get(&reader,unit,end_descriptor,24) && !memcmp(descriptor,end_descriptor,24);
}
static int hud_metadata_override(UssHudMetadataRead* io,uintptr_t base,uintptr_t owner,uintptr_t system,
    const UssPairMember* member,uintptr_t table,uintptr_t* icon_address,unsigned char icon[4])
{
    unsigned char list[4096],again[4096];uint32_t count=0,end_count=0;uintptr_t end_system=0;unsigned i;
    if(!hud_metadata_read(io,system+0x1c14,&count,4) || count>256 ||
        (count && !hud_metadata_read(io,system+0x1c18,list,(size_t)count*16)))return 0;
    for(i=0;i<count;++i)if(u32(list+i*16+8)==100){
        Reader reader={0};UssCaptureReader lookup_reader={0};uintptr_t component=ptr_value(list+i*16),units=0,unit=0;
        unsigned char descriptor[24],after[24];uint32_t index=0;int found;uintptr_t resource=0,display=0,end_unit=0;
        if(!address_ok(component,72))return 0;
        reader.api=io->api;lookup_reader.memory=reader;
        found=capture_lookup(&lookup_reader,component+0x20,member->entity,&index);reader=lookup_reader.memory;
        if(found<0 || reader.failure)return 0; /* Unknown is not an absent override. */
        if(!found)continue;
        if(index>=16384 || !get_ptr(&reader,component+0x38,&units) ||
            !get_ptr(&reader,units+(uintptr_t)index*8,&unit) || !get(&reader,unit,descriptor,24) ||
            u32(descriptor+8)!=member->entity)return 0;
        int asset=hud_metadata_asset(io,owner+0xf129f8,descriptor,16,8,32,&resource);
        if(asset<0)return 0;
        if(asset==1){
            uint32_t handle=0,candidate=0,type=0;
            if(!hud_metadata_read(io,resource,&handle,4))return 0;
            if(handle){
                uintptr_t end_display=0;uint32_t end_type=0,end_candidate=0;
                unsigned limit=table==base+0x37c7670?351:415; /* Reviewed HF 351+64 allocation. */
                if(handle>=limit || !hud_metadata_pointer(io,table+(uintptr_t)handle*8,&display) ||
                    !hud_metadata_read(io,display,&type,4) || type!=handle ||
                    !hud_metadata_read(io,display+4,&candidate,4) ||
                    !hud_metadata_pointer(io,table+(uintptr_t)handle*8,&end_display) || end_display!=display ||
                    !hud_metadata_read(io,display,&end_type,4) || end_type!=type ||
                    !hud_metadata_read(io,display+4,&end_candidate,4) || end_candidate!=candidate)return 0;
                if(candidate){memcpy(icon,&candidate,4);*icon_address=display+4;}
            }
        }
        if(!get_ptr(&reader,units+(uintptr_t)index*8,&end_unit) || end_unit!=unit ||
            !get(&reader,unit,after,24) || memcmp(descriptor,after,24))return 0;
    }
    return hud_metadata_pointer(io,base+0x3326e68,&end_system) && end_system==system &&
        hud_metadata_read(io,system+0x1c14,&end_count,4) && end_count==count &&
        (!count || (hud_metadata_read(io,system+0x1c18,again,(size_t)count*16) && !memcmp(list,again,(size_t)count*16)));
}
static inline int uss_hud_metadata(UssHudMetadata* cache,const HfApi* api,uintptr_t base,uintptr_t root,
    const UssPairMember allowed[2],const UssPairMember* member,unsigned char output[12])
{
    UssHudMetadataRead io={0};UssHudMetadataEntry result={0};uintptr_t owner=0,weapons=0,icons=0,system=0,weapon=0,again=0;
    unsigned char check[12],zero[8]={0};unsigned i,slot=0;Reader table_reader={0};uintptr_t table=0,end_table=0;
    if(!cache || !member || !allowed || !root || !output ||
        (!pair_member_equal(member,&allowed[0]) && !pair_member_equal(member,&allowed[1])) || !hud_metadata_unit(api,base,member))return 0;
    io.api=api;table_reader.api=api;
    if(!uss_hud_projectile_table(&table_reader,base,&table))return 0;
    if(!hud_metadata_pointer(&io,base+0x346bf98,&owner) || !hud_metadata_pointer(&io,owner+0xf126d8,&weapons) ||
        !hud_metadata_pointer(&io,owner+0xf129f8,&icons))return 0;
    for(i=0;i<2;++i){UssHudMetadataEntry* old=&cache->entries[i];
        if(old->valid && (!pair_member_equal(&old->member,&allowed[0]) && !pair_member_equal(&old->member,&allowed[1])))old->valid=0;
        if(old->valid && pair_member_equal(&old->member,member)){
            slot=i;
            if(!old->override && old->projectile_table==table && old->root==root && old->owner==owner && old->weapons==weapons && old->icons==icons &&
                hud_metadata_read(&io,old->name_address,check+4,8) && hud_metadata_read(&io,old->icon_address,check,4) &&
                !memcmp(check,old->display,12) && uss_hud_projectile_table(&table_reader,base,&end_table) && end_table==table){memcpy(output,check,12);return 1;}
            old->valid=0;
        }else if(!old->valid)slot=i;
    }
    if(hud_metadata_asset(&io,owner+0xf126d8,member->identity,0x532,0x17c,0x38,&weapon)!=1 ||
        !hud_metadata_read(&io,weapon+0x30,result.display+4,8) || !memcmp(result.display+4,zero,8) ||
        !hud_metadata_read(&io,weapon+8,result.display,4) || !hud_metadata_pointer(&io,base+0x3326e68,&system))return 0;
    result.icon_address=weapon+8;
    if(!hud_metadata_override(&io,base,owner,system,member,table,&result.icon_address,result.display) || !u32(result.display) ||
        !hud_metadata_pointer(&io,base+0x346bf98,&again) || again!=owner ||
        !hud_metadata_pointer(&io,owner+0xf126d8,&again) || again!=weapons ||
        !hud_metadata_pointer(&io,owner+0xf129f8,&again) || again!=icons || !hud_metadata_unit(api,base,member) ||
        !hud_metadata_read(&io,weapon+0x30,check+4,8) || !hud_metadata_read(&io,result.icon_address,check,4) ||
        memcmp(check,result.display,12) || !uss_hud_projectile_table(&table_reader,base,&end_table) || end_table!=table)return 0;
    result.projectile_table=table;result.override=result.icon_address!=weapon+8;
    result.valid=1;result.member=*member;result.root=root;result.owner=owner;result.weapons=weapons;result.icons=icons;
    result.name_address=weapon+0x30;cache->entries[slot]=result;memcpy(output,result.display,12);return 1;
}
#endif
