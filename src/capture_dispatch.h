/* Exact ordinary inventory dispatch dependencies, observed only. */
#ifndef USS_CAPTURE_DISPATCH_H
#define USS_CAPTURE_DISPATCH_H
#include "capture_read.h"
static int capture_export(UssCaptureReader* reader,const UssCaptureImage* image,
    const char* name,uint32_t slot)
{
    uintptr_t imported,exported;UssCapturePage page;
    if(!capture_bytes(reader,image->base+slot,&imported,8,1)) return 0;
    exported=reader->platform->export_address(reader->platform->user,name);
    return address_ok(exported,1) && imported==exported &&
        reader_evidence(&reader->memory,0,&exported,8) && capture_query(reader,exported,&page) &&
        capture_page_executable(&page,page.allocation,exported,1);
}
static int capture_dispatch(UssCaptureReader* reader,uintptr_t session)
{
    UssCaptureImage image,after;
    uintptr_t helpers,root,dispatch_interface,auxiliary,targets[3],creator,registry_root,registry,records,metadata,indexes;
    uintptr_t transport_a,transport_b,selected,table_at,method;
    unsigned char bytes[40],header[16],flags[16],indexes_bytes[28],policy[12];
    uint32_t count,index=UINT32_MAX,low,high,i;unsigned char lifecycle;
    static const unsigned char enter_pin[]={0x48,0xff,0x25,0x01,0x26,0x35,0x01};
    static const unsigned char leave_pin[]={0x48,0xff,0x25,0xf9,0x25,0x35,0x01};
    static const unsigned char argument_pin[]={0x45,0x8b,0xf9,0x4d,0x8b,0xf0,0x48,0x8b,0xfa,0x8b,0xe9};
    static const unsigned char creator_pin[]={0xe9,0xda,0x41,0x2f,0x00};
    reader->memory.stage="CAPTURE_DISPATCH";
    if (!capture_image(reader,&image) ||
        !capture_pointer(reader,session+0x1F860,&auxiliary,0) ||
        !capture_pointer(reader,reader->game+0x33263E8,&helpers,0) ||
        !capture_pointer(reader,reader->game+0x3326308,&root,0) ||
        !capture_pointer(reader,root+0x38,&dispatch_interface,0) ||
        !capture_pointer(reader,helpers+0x70,&targets[0],1) ||
        !capture_pointer(reader,helpers+0x78,&targets[1],1) ||
        !capture_pointer(reader,dispatch_interface+0x40,&targets[2],1) ||
        targets[0]!=image.base+0xA40B0 || targets[1]!=image.base+0xA40C0 || targets[2]!=image.base+0x34FF40 ||
        !capture_code(reader,&image,targets[0],enter_pin,7) ||
        !capture_code(reader,&image,targets[1],leave_pin,7) ||
        !capture_export(reader,&image,"EnterCriticalSection",0x13F66B8) ||
        !capture_export(reader,&image,"LeaveCriticalSection",0x13F66C0) ||
        !capture_bytes(reader,auxiliary,bytes,40,1) ||
        !capture_bytes(reader,session+0x1F86B,&lifecycle,1,1) || lifecycle!=1 ||
        !capture_pointer(reader,helpers+0x60,&creator,1) || creator!=image.base+0xA4030 ||
        !capture_pointer(reader,helpers+0x68,&creator,1) || creator!=image.base+0xA4070) return 0;
    for(i=0;i<sizeof(capture_pins)/sizeof(capture_pins[0]);++i) {
        const UssCapturePin* pin=&capture_pins[i];
        if(pin->exe && !capture_code(reader,&image,image.base+pin->rva,pin->bytes,pin->size)) return 0;
    }
    if (!capture_pin(reader,reader->game+0xBDE452,argument_pin,sizeof(argument_pin)) ||
        !capture_bytes(reader,image.base+0x13F6690,&creator,8,1) || creator!=image.base+0x2C6E642 ||
        !capture_code(reader,&image,creator,creator_pin,5) ||
        !capture_pointer(reader,auxiliary+0x28,&creator,1) || creator!=image.base+0x16677B8 ||
        !capture_export(reader,&image,"DeleteCriticalSection",0x13F6628) ||
        !capture_pointer(reader,image.base+0x1A10278,&registry_root,1) ||
        !capture_pointer(reader,registry_root,&registry,1) ||
        !capture_bytes(reader,registry+0x88,header,16,1)) return 0;
    count=u32(header);records=ptr_value(header+8);
    if(!count || count>1024 || !address_ok(records,(size_t)count*104) ||
        !capture_pointer(reader,registry+0x18,&metadata,1)) return 0;
    low=0;high=count;
    for(i=0;i<11 && low<high;++i) {
        uint32_t candidate=(low+high)/2,key;
        if(!capture_bytes(reader,records+(uintptr_t)candidate*104,&key,4,1)) return 0;
        if(key==0x42729286){index=candidate;break;}
        if(key<0x42729286)low=candidate+1;else high=candidate;
    }
    if(index!=152 || !capture_bytes(reader,records+(uintptr_t)index*104,bytes,12,1) ||
        !capture_bytes(reader,records+(uintptr_t)index*104+0x50,header,16,1) || u32(header)!=7) return 0;
    indexes=ptr_value(header+8);
    if(!address_ok(indexes,28) || !capture_bytes(reader,indexes,indexes_bytes,28,1)) return 0;
    for(i=0;i<7;++i) {
        uint32_t expected=i==6?408:256;
        if(u32(indexes_bytes+i*4)!=expected ||
            !capture_bytes(reader,metadata+(uintptr_t)expected*24+12,policy,12,1) ||
            policy[0]!=(i==6?7:1) || policy[1]!=(i==6?0:15)) return 0;
        {unsigned j;for(j=2;j<12;++j)if(policy[j])return 0;}
    }
    if(!capture_pointer(reader,reader->game+0x214C2E0+(uintptr_t)index*8,&creator,1) ||
        creator!=reader->game+0xBAABD0 || !capture_bytes(reader,registry,flags,16,1) ||
        flags[13]<8 || flags[13]>16 || index>=(1u<<flags[13]) ||
        !capture_bytes(reader,registry_root+0xA8,&transport_a,8,1) ||
        !capture_bytes(reader,registry_root+0x98,&transport_b,8,1)) return 0;
    selected=transport_a?transport_a:transport_b;
    if(!address_ok(selected,1) || selected%8 || !capture_pointer(reader,selected,&table_at,1) ||
        !capture_bytes(reader,table_at+(transport_a?0x180:0x18),&method,8,1)) return 0;
    {UssCapturePage page;if(!capture_query(reader,method,&page) ||
        !capture_page_executable(&page,page.allocation,method,1))return 0;}
    if(flags[5]) {
        UssCapturePage page;
        if(!transport_a || !capture_bytes(reader,table_at+0x90,&method,8,1) ||
            !capture_query(reader,method,&page) || !capture_page_executable(&page,page.allocation,method,1))return 0;
    }
    return capture_image(reader,&after) && !memcmp(&image,&after,sizeof(image));
}
#endif
