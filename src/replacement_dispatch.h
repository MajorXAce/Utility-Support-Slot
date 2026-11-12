/* Read-only drop-control dependencies. This is evidence, never an effect permit.
 * Ordinary capture does not call this additional replacement-only validator. */
#ifndef USS_REPLACEMENT_DISPATCH_H
#define USS_REPLACEMENT_DISPATCH_H
#include "publication_capture.h"
#include "replacement_control_pins.h"

static int replacement_control_code(UssCaptureReader* reader)
{
    unsigned i;size_t done;
    for(i=0;i<sizeof(uss_drop_control_pins)/sizeof(uss_drop_control_pins[0]);++i) {
        const UssCapturePin* pin=&uss_drop_control_pins[i];
        if(pin->rva>reader->game_size || pin->size>reader->game_size-pin->rva)return 0;
        /* Check each page-sized fragment, including a body crossing page edges. */
        for(done=0;done<pin->size;) {
            uintptr_t at=reader->game+pin->rva+done;UssCapturePage before,after;
            size_t n=4096-(at%4096);if(n>pin->size-done)n=pin->size-done;
            if(!capture_query(reader,at,&before) || !capture_page_executable(&before,reader->game,at,n) ||
                !capture_pin(reader,at,pin->bytes+done,n) || !capture_query(reader,at,&after) ||
                memcmp(&before,&after,sizeof(before)) || !capture_page_executable(&after,reader->game,at,n))return 0;
            done+=n;
        }
    }
    return 1;
}
static int replacement_event(UssCaptureReader* reader,uintptr_t records,uint32_t index,uint32_t key)
{
    unsigned char row[12];
    return capture_bytes(reader,records+(uintptr_t)index*104,row,12,1) && u32(row)==key &&
        row[4]==1 && row[5]==1 && u32(row+8)==0;
}
static int replacement_drop_message(UssCaptureReader* reader,uintptr_t registry,uintptr_t records,uint32_t count)
{
    unsigned char row[16],indexes[8],policy[12];uint32_t low=0,high=count,index=UINT32_MAX,i;
    uintptr_t metadata,args,receiver;
    for(i=0;i<11 && low<high;++i) {
        uint32_t candidate=(low+high)/2,key;
        if(!capture_bytes(reader,records+(uintptr_t)candidate*104,&key,4,1))return 0;
        if(key==UINT32_C(0xFE5BF7A3)){index=candidate;break;}
        if(key<UINT32_C(0xFE5BF7A3))low=candidate+1;else high=candidate;
    }
    if(index!=590 || !capture_pointer(reader,registry+0x18,&metadata,1) ||
        !capture_bytes(reader,records+(uintptr_t)index*104+0x50,row,16,1) || u32(row)!=2)return 0;
    args=ptr_value(row+8);
    if(!address_ok(args,8) || !capture_bytes(reader,args,indexes,8,1))return 0;
    for(i=0;i<2;++i) {
        uint32_t expected=i?164:256;
        if(u32(indexes+i*4)!=expected || !capture_bytes(reader,metadata+(uintptr_t)expected*24+12,policy,12,1) ||
            policy[0]!=1 || policy[1]!=(i?32:15))return 0;
        {unsigned j;for(j=2;j<12;++j)if(policy[j])return 0;}
    }
    return capture_pointer(reader,reader->game+0x214C2E0+(uintptr_t)index*8,&receiver,1) &&
        receiver==reader->game+0xBC2C50;
}
static int replacement_open_peers(UssCaptureReader* reader,uintptr_t provider,uintptr_t session,
                                   const UssCapturePeers* peers)
{
    uintptr_t array,entry;unsigned char id[8],closed;uint32_t count,i,j;unsigned found=0,required=0;
    UssCapturePeers after;
    for(i=0;i<peers->count;++i)if(memcmp(peers->sorted+i*8,peers->self,8))required|=1u<<i;
    if(!required || !capture_bytes(reader,provider+0xA0,&count,4,1) || !count || count>32 ||
        !capture_pointer(reader,provider+0xA8,&array,1) || !address_ok(array,(size_t)count*8))return 0;
    for(i=0;i<count;++i) {
        if(!capture_pointer(reader,array+(uintptr_t)i*8,&entry,1) || !capture_bytes(reader,entry+0x10,id,8,1))return 0;
        for(j=0;j<peers->count;++j)if((required&(1u<<j)) && !memcmp(id,peers->sorted+j*8,8)) {
            if((found&(1u<<j)) || !capture_bytes(reader,entry+0x1878C,&closed,1,1) || closed)return 0;
            found|=1u<<j;
        }
    }
    return found==required && capture_peers(reader,session,&after) && peers->size==after.size &&
        !memcmp(peers->context,after.context,peers->size);
}
static int uss_capture_replacement_dispatch(UssCaptureReader* reader,uintptr_t session)
{
    UssCaptureImage image,after;UssCapturePeers peers;
    uintptr_t root,registry,records,transport,table_at,provider,provider_table,method;
    unsigned char row[16],flags[16];uint32_t count;unsigned i;
    if(!capture_dispatch(reader,session))return 0; /* Full existing publication policy/imports/lock/transport. */
    reader->memory.stage="REPLACEMENT_DISPATCH";
    if(!capture_image(reader,&image) || !capture_peers(reader,session,&peers) || peers.count<2 ||
        !capture_pointer(reader,image.base+0x1A10278,&root,1) || !capture_pointer(reader,root,&registry,1) ||
        !capture_bytes(reader,registry+0x88,row,16,1))return 0;
    count=u32(row);records=ptr_value(row+8);
    if(count<=590 || count>1024 || !address_ok(records,(size_t)count*104) ||
        !replacement_drop_message(reader,registry,records,count) ||
        !replacement_event(reader,records,152,0x42729286) ||
        !replacement_event(reader,records,590,UINT32_C(0xFE5BF7A3)) ||
        !capture_bytes(reader,registry,flags,16,1) || flags[5] || flags[13]<10 || flags[13]>16 ||
        !capture_pointer(reader,root+0xA8,&transport,1) || !capture_pointer(reader,transport,&table_at,1) ||
        !capture_bytes(reader,table_at+0x180,&method,8,1) || method!=image.base+0x293F50 ||
        !capture_pointer(reader,transport+0x10,&provider,1) || !capture_pointer(reader,provider,&provider_table,1) ||
        !capture_bytes(reader,provider_table+0x18,&method,8,1) || method!=image.base+0x2AA350)return 0;
    for(i=0;i<sizeof(uss_queue_control_pins)/sizeof(uss_queue_control_pins[0]);++i) {
        const UssCapturePin* pin=&uss_queue_control_pins[i];
        if(!capture_code(reader,&image,image.base+pin->rva,pin->bytes,pin->size))return 0;
    }
    return replacement_open_peers(reader,provider,session,&peers) && replacement_control_code(reader) &&
        capture_image(reader,&after) && !memcmp(&image,&after,sizeof(image));
}
#endif
