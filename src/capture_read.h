/* Read-only observation helpers for the exact Lua publication capture contract. */
#ifndef USS_CAPTURE_READ_H
#define USS_CAPTURE_READ_H
#include "primary_readiness.h"
#include "publication_state.h"
#include "capture_platform.h"
#include "capture_pins.h"
typedef struct UssCaptureReader {
    Reader memory;
    const UssCapturePlatform* platform;
    uintptr_t game,exe;
    size_t game_size,exe_size;
} UssCaptureReader;
typedef struct UssCaptureImage {
    unsigned char dos[64],header[24],optional[64],sections[96*40];
    uint16_t path[2048];size_t path_size;
    unsigned count;uintptr_t base;size_t size;
} UssCaptureImage;
static int capture_page_readable(const UssCapturePage* page,uintptr_t address,size_t size)
{
    uint32_t p=page->protection;
    return page->state==0x1000 && p<256 && address>=page->base &&
        address-page->base<=page->size && size<=page->size-(address-page->base) &&
        (p==2 || p==4 || p==8 || p==32 || p==64 || p==128);
}
static int capture_page_executable(const UssCapturePage* page,uintptr_t base,uintptr_t address,size_t size)
{
    return capture_page_readable(page,address,size) && page->kind==0x1000000 &&
        page->allocation==base && (page->protection==32 || page->protection==64 || page->protection==128);
}
static int capture_query(UssCaptureReader* reader,uintptr_t address,UssCapturePage* page)
{
    memset(page,0,sizeof(*page));
    return reader->platform->query(reader->platform->user,address,page) &&
        reader_evidence(&reader->memory,0,page,sizeof(*page));
}
static int capture_bytes(UssCaptureReader* reader,uintptr_t address,void* out,size_t size,unsigned checked)
{
    UssCapturePage before={0},after={0};size_t done=0;
    if (!size || size>4096 || !address_ok(address,size)) return 0;
    if (checked && (!capture_query(reader,address,&before) || !capture_page_readable(&before,address,size))) return 0;
    while(done<size) {
        size_t chunk=size-done>72 ? 72 : size-done;
        if (!get(&reader->memory,address+done,(unsigned char*)out+done,chunk)) return 0;
        done+=chunk;
    }
    return !checked || (capture_query(reader,address,&after) && !memcmp(&before,&after,sizeof(before)));
}
static int capture_pointer(UssCaptureReader* reader,uintptr_t at,uintptr_t* out,unsigned checked)
{
    return capture_bytes(reader,at,out,8,checked) && address_ok(*out,1) && *out%8==0;
}
static int capture_pin(UssCaptureReader* reader,uintptr_t at,const unsigned char* expected,size_t size)
{
    unsigned char bytes[72];size_t done=0;
    while(done<size) {
        size_t chunk=size-done>sizeof(bytes)?sizeof(bytes):size-done;
        if (!capture_bytes(reader,at+done,bytes,chunk,0) || memcmp(bytes,expected+done,chunk)) return 0;
        done+=chunk;
    }
    return 1;
}
static int capture_image(UssCaptureReader* reader,UssCaptureImage* image)
{
    unsigned char optional_size[2];uint32_t pe;unsigned i;size_t basename=0;UssCapturePage page;
    static const char expected_name[]="helldivers2.exe";
    memset(image,0,sizeof(*image));image->base=reader->memory.api->module_base("helldivers2.exe",&image->size);
    if (!image->base || image->size!=60719104 || !address_ok(image->base,image->size) ||
        !capture_query(reader,image->base,&page) || page.kind!=0x1000000 || page.allocation!=image->base ||
        !capture_bytes(reader,image->base,image->dos,64,1) || memcmp(image->dos,"MZ",2)) return 0;
    pe=u32(image->dos+0x3C);
    if (pe<64 || pe>4096 || !capture_bytes(reader,image->base+pe,image->header,24,1) ||
        memcmp(image->header,"PE\0\0\x64\x86",6) || u32(image->header+8)!=0x6AB382E4) return 0;
    image->count=(unsigned)image->header[6]+((unsigned)image->header[7]<<8);
    memcpy(optional_size,image->header+20,2);i=(unsigned)optional_size[0]+((unsigned)optional_size[1]<<8);
    if (!image->count || image->count>96 || i<64 || i>512 ||
        !capture_bytes(reader,image->base+pe+24,image->optional,64,1) ||
        image->optional[0]!=0x0B || image->optional[1]!=2 || u32(image->optional+56)!=image->size ||
        (size_t)pe+24+i+(size_t)image->count*40>image->size ||
        !capture_bytes(reader,image->base+pe+24+i,image->sections,(size_t)image->count*40,1) ||
        !reader->platform->path(reader->platform->user,image->base,image->path,2048,&image->path_size) ||
        !image->path_size || image->path_size>2046 ||
        !reader_evidence(&reader->memory,0,image->path,image->path_size*2)) return 0;
    for(i=0;i<image->path_size;++i) if(image->path[i]=='\\' || image->path[i]=='/') basename=i+1;
    if (image->path_size-basename!=sizeof(expected_name)-1) return 0;
    for(i=0;i<sizeof(expected_name)-1;++i) {
        unsigned c=image->path[basename+i];if(c>='A' && c<='Z') c+='a'-'A';
        if(c!=(unsigned char)expected_name[i]) return 0;
    }
    reader->exe=image->base;reader->exe_size=image->size;return 1;
}
static int capture_code(UssCaptureReader* reader,const UssCaptureImage* image,
    uintptr_t address,const unsigned char* expected,size_t size)
{
    UssCapturePage page,after;unsigned i,matches=0;
    if (address<image->base || address-image->base>=image->size) return 0;
    for(i=0;i<image->count;++i) {
        const unsigned char* section=image->sections+i*40;
        uint32_t start=u32(section+12),span=u32(section+8);
        if (span && start<=image->size && span<=image->size-start &&
            address-image->base>=start && address-image->base-start<span &&
            (u32(section+36)&0x20000000)) {
            ++matches;if(size>span-(address-image->base-start))return 0;
        }
    }
    if(matches!=1 || !capture_query(reader,address,&page) ||
        !capture_page_executable(&page,image->base,address,size)) return 0;
    return capture_pin(reader,address,expected,size) && capture_query(reader,address,&after) &&
        !memcmp(&page,&after,sizeof(page)) && capture_page_executable(&after,image->base,address,size);
}
#endif
