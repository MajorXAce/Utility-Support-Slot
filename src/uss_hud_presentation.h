/* Exact native popup formatting adapter, translated from our Lua baseline.
 * Explicit invoker only; constructing it registers/executes nothing. */
#ifndef USS_HUD_PRESENTATION_H
#define USS_HUD_PRESENTATION_H
#include "formation_native.h"
#include "uss_hud_metadata.h"
#include "uss_hud_presentation_pins.h"
enum {USS_HUD_PRESENT_SKIP=0,USS_HUD_PRESENT_DONE,USS_HUD_PRESENT_DISABLED};
typedef enum UssHudPresentationEffect {
    USS_HUD_MEASURE=1,USS_HUD_SET_ICON,USS_HUD_SET_SIZE,USS_HUD_SET_NAME,USS_HUD_LAYOUT
} UssHudPresentationEffect;
typedef int (*UssHudPresentationInvoke)(void*,UssHudPresentationEffect,uintptr_t,uintptr_t,uint64_t,uint32_t*,uint32_t*);
typedef struct UssHudPresentation {
    UssFormationNative* native;void* user;UssHudPresentationInvoke invoke;
    uint64_t last_event,effects;unsigned disabled,busy;const char* reason;
} UssHudPresentation;
typedef struct UssHudExecutable {
    UssCapturePage page;uintptr_t allocation;uint32_t image_size;
    unsigned char dos[64],nt[24],optional[64],sections[3840],entry;
} UssHudExecutable;
typedef struct UssHudPresentationSnapshot {
    uintptr_t base,root,widget,text_node,icon_node,context,engine,dispatch,measure;
    UssCapturePage writable;UssHudExecutable executable;
    unsigned char text[0x158],icon[0x278],mode[4];
} UssHudPresentationSnapshot;
static inline uint16_t hud_presentation_u16(const unsigned char* bytes,unsigned offset)
{uint16_t value;memcpy(&value,bytes+offset,2);return value;}
static int hud_presentation_executable(UssHudPresentation* p,UssHudMetadataRead* reader,uintptr_t at,UssHudExecutable* output)
{
    UssHudExecutable value={0};uint32_t pe,rva;unsigned count,size,i,matched=0;
    if(!p->native->platform.query || !p->native->platform.query(p->native->platform.user,at,&value.page) ||
        value.page.state!=0x1000 || value.page.kind!=0x1000000 ||
        (value.page.protection!=32 && value.page.protection!=64) || !capture_page_readable(&value.page,at,1))return 0;
    value.allocation=value.page.allocation;
    if(!address_ok(value.allocation,64) || at<value.allocation ||
        !hud_metadata_read(reader,value.allocation,value.dos,64) || memcmp(value.dos,"MZ",2))return 0;
    pe=u32(value.dos+0x3c);if(pe<64 || pe>1048576 ||
        !hud_metadata_read(reader,value.allocation+pe,value.nt,24) || memcmp(value.nt,"PE\0\0",4) ||
        hud_presentation_u16(value.nt,4)!=0x8664)return 0;
    count=hud_presentation_u16(value.nt,6);size=hud_presentation_u16(value.nt,20);
    if(!count || count>96 || size<112 || size>4096 ||
        !hud_metadata_read(reader,value.allocation+pe+24,value.optional,64) || hud_presentation_u16(value.optional,0)!=0x20b)return 0;
    value.image_size=u32(value.optional+56);
    if(value.image_size<4096 || value.image_size>0x20000000 ||
        (uint64_t)pe+24+size+(uint64_t)count*40>value.image_size || at-value.allocation>=value.image_size ||
        !hud_metadata_read(reader,value.allocation+pe+24+size,value.sections,(size_t)count*40))return 0;
    rva=(uint32_t)(at-value.allocation);
    for(i=0;i<count;++i){const unsigned char* section=value.sections+i*40;uint32_t start=u32(section+12),length=u32(section+8);
        if(u32(section+16)>length)length=u32(section+16);
        if(start<=value.image_size && length<=value.image_size-start && rva>=start && rva-start<length &&
            (u32(section+36)&0x20000000u))matched=1;
    }
    if(!matched || !hud_metadata_read(reader,at,&value.entry,1))return 0;
    *output=value;return 1;
}
static int hud_presentation_capture(UssHudPresentation* p,uintptr_t root,const unsigned char expected[12],UssHudPresentationSnapshot* output)
{
    UssHudPresentationSnapshot value={0};UssHudMetadataRead reader={0};uintptr_t pointer=0;unsigned i;
    if(!p || !p->native || !p->invoke || p->disabled || !root || !(value.base=formation_native_module(p->native)) ||
        !p->native->platform.thread_id || p->native->platform.thread_id(p->native->platform.user)!=p->native->runtime->thread)return 0;
    reader.api=p->native->api;value.root=root;
    for(i=0;i<5;++i){const UssHudPin* pin=&uss_hud_presentation_pins[i];
        if(!formation_native_entry(p->native,value.base,pin->rva,pin->bytes,pin->size))return 0;}
    if(!hud_metadata_pointer(&reader,value.base+0x346d538,&pointer) || pointer!=root || !address_ok(root,0x395128))return 0;
    value.widget=root+0x392828;value.text_node=value.widget+0x13a8;value.icon_node=value.widget+0x1cc0;
    if(!p->native->platform.query(p->native->platform.user,value.widget,&value.writable) ||
        !capture_page_readable(&value.writable,value.widget,0x2900) || value.writable.kind!=0x20000 ||
        (value.writable.protection!=4 && value.writable.protection!=8) ||
        !hud_metadata_read(&reader,value.text_node,value.text,sizeof(value.text)) ||
        !hud_metadata_read(&reader,value.icon_node,value.icon,sizeof(value.icon)) ||
        !hud_metadata_read(&reader,value.widget+0x27cc,value.mode,4) || u32(value.mode)!=1 ||
        ((u32(value.text)>>18)&15)!=3 || ((u32(value.icon)>>18)&15)!=7 ||
        memcmp(value.text+0x150,expected+4,8) || memcmp(value.icon+0x110,expected,4) ||
        !hud_metadata_pointer(&reader,value.text_node+0xf8,&value.context) ||
        !hud_metadata_pointer(&reader,value.context+8,&pointer) ||
        !hud_metadata_pointer(&reader,value.base+USS_HUD_ENGINE_RVA,&value.engine) ||
        !hud_metadata_pointer(&reader,value.engine+0x28,&pointer) ||
        !hud_metadata_pointer(&reader,value.engine+0xd0,&value.dispatch) ||
        !hud_metadata_pointer(&reader,value.dispatch+0x338,&value.measure) ||
        !hud_presentation_executable(p,&reader,value.measure,&value.executable))return 0;
    *output=value;return 1;
}
static unsigned hud_presentation_disable(UssHudPresentation* p,const char* reason)
{p->disabled=1;p->busy=0;p->reason=reason;return USS_HUD_PRESENT_DISABLED;}
static inline unsigned uss_hud_present(UssHudPresentation* p,uint64_t event,uintptr_t root,
    const unsigned char expected[12],const unsigned char desired[12],int (*unchanged)(void*),void* user)
{
    UssHudPresentationSnapshot before,entry;UssHudMetadataRead reader={0};uint32_t width=0,height=0,scaled;
    uint64_t name=0,packed;float dimensions[2];unsigned char zero[8]={0},after_name[8],after_icon[4];
    if(!p || p->disabled || p->busy || !p->invoke || !event || event<=p->last_event || !expected || !desired ||
        !u32(desired) || !memcmp(desired+4,zero,8) || !unchanged || !unchanged(user))return USS_HUD_PRESENT_SKIP;
    p->busy=1;
    if(!hud_presentation_capture(p,root,expected,&before) || !unchanged(user)){p->busy=0;p->reason="PRESENTATION_CONTEXT";return USS_HUD_PRESENT_SKIP;}
    memcpy(&name,desired+4,8);p->last_event=event;++p->effects; /* Consume before measurement, including uncertain return. */
    if(!p->invoke(p->user,USS_HUD_MEASURE,before.measure,before.dispatch,name,&width,&height))
        return hud_presentation_disable(p,"PRESENTATION_MEASURE_UNCERTAIN");
    if(!width || !height || width>16384 || height>16384){p->busy=0;p->reason="PRESENTATION_DIMENSIONS";return USS_HUD_PRESENT_SKIP;}
    scaled=(32u*width+height-1)/height;
    if(!scaled || scaled>2048 || !hud_presentation_capture(p,root,expected,&entry) || memcmp(&before,&entry,sizeof(entry)) || !unchanged(user)){
        p->busy=0;p->reason="PRESENTATION_RECHECK_CHANGED";return USS_HUD_PRESENT_SKIP;}
    dimensions[0]=(float)scaled;dimensions[1]=32.0f;memcpy(&packed,dimensions,8);
    ++p->effects;if(!p->invoke(p->user,USS_HUD_SET_ICON,entry.base+uss_hud_presentation_pins[3].rva,entry.icon_node,u32(desired),NULL,NULL))goto uncertain;
    ++p->effects;if(!p->invoke(p->user,USS_HUD_SET_SIZE,entry.base+uss_hud_presentation_pins[0].rva,entry.text_node,packed,NULL,NULL))goto uncertain;
    ++p->effects;if(!p->invoke(p->user,USS_HUD_SET_NAME,entry.base+uss_hud_presentation_pins[2].rva,entry.text_node,name,NULL,NULL))goto uncertain;
    ++p->effects;if(!p->invoke(p->user,USS_HUD_LAYOUT,entry.base+uss_hud_presentation_pins[4].rva,entry.widget,0,NULL,NULL))goto uncertain;
    reader.api=p->native->api;
    if(!hud_metadata_read(&reader,entry.text_node+0x150,after_name,8) || !hud_metadata_read(&reader,entry.icon_node+0x110,after_icon,4) ||
        memcmp(after_name,desired+4,8) || memcmp(after_icon,desired,4) || !unchanged(user))goto uncertain;
    p->busy=0;p->reason="PRESENTATION_CORRECTED";return USS_HUD_PRESENT_DONE;
uncertain:
    return hud_presentation_disable(p,"PRESENTATION_SETTER_OR_READBACK_UNCERTAIN");
}
/* Win64 ABI: two float dimensions occupy one integer register, not a C struct.
 * This is an explicit binding for the eventual normal entry; tests bind own functions. */
static inline int uss_hud_presentation_invoke(void* user,UssHudPresentationEffect effect,uintptr_t entry,uintptr_t node,
    uint64_t value,uint32_t* width,uint32_t* height)
{
    typedef void (*Measure)(uint64_t,uint32_t*,uint32_t*,void*);
    typedef void (*Icon)(void*,uint32_t);typedef void (*Size)(void*,uint64_t);
    typedef void (*Name)(void*,uint64_t,uint64_t,uint8_t);typedef void (*Layout)(void*);(void)user;
    if(effect==USS_HUD_MEASURE)((Measure)entry)(value,width,height,(void*)node);
    else if(effect==USS_HUD_SET_ICON)((Icon)entry)((void*)node,(uint32_t)value);
    else if(effect==USS_HUD_SET_SIZE)((Size)entry)((void*)node,value);
    else if(effect==USS_HUD_SET_NAME)((Name)entry)((void*)node,UINT64_C(0xd08fc1346df3d6fc),value,0);
    else if(effect==USS_HUD_LAYOUT)((Layout)entry)((void*)node);
    else return 0;
    return 1; /* A native access violation cannot be caught by this return contract. */
}
#endif
