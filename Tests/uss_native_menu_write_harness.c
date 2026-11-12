#include <assert.h>
#include <stdio.h>
#include "../src/uss_native_menu_sdk.h"
static unsigned char* screen;
static unsigned bad_build,proof_calls,fail_proof,fail_read,change_epoch;
static const char* build(void){return bad_build?"other":"25480438";}
static uintptr_t module(const char* name,size_t* size)
{assert(!strcmp(name,"game.dll"));*size=0x4000000;return 0x10000000;}
static int read_sdk(uintptr_t at,void* out,size_t length)
{
    if(fail_read || at<(uintptr_t)screen || at>(uintptr_t)screen+5000000 ||
        length>(uintptr_t)screen+5000000-at)return 1;
    memcpy(out,(const void*)at,length);return 0;
}
static int owned(void* user,const UssMenuHostSnapshot* expected)
{
    UssMenuSdkWriter* writer=(UssMenuSdkWriter*)user;assert(uss_menu_same_host(expected,&writer->host));
    ++proof_calls;if(change_epoch && proof_calls==change_epoch)++writer->sdk->epoch;
    return !fail_proof || proof_calls!=fail_proof;
}
static UssMenuSdkWriter fixture(UssMenuSdkContext* sdk,HfApi* api)
{
    UssMenuSdkWriter writer={0};memset(screen,0,5000000);memset(api,0,sizeof(*api));
    bad_build=proof_calls=fail_proof=fail_read=change_epoch=0;
    api->version=12;api->size=sizeof(*api);api->build_id=build;api->module_base=module;api->read=read_sdk;
    sdk->api=api;sdk->base=0x10000000;sdk->module_size=0x4000000;sdk->thread=GetCurrentThreadId();sdk->epoch=1;
    writer.sdk=sdk;writer.host.screen=(uintptr_t)screen;writer.host.content=(uintptr_t)screen+3010456;
    writer.host.container=writer.host.content+1150848+544;writer.host.input=0x20000;writer.host.epoch=1;writer.host.valid=1;
    writer.owned=owned;return writer;
}
int main(void)
{
    HfApi api;UssMenuSdkContext sdk;UssMenuSdkWriter writer;
    UssNativeMenuEntries entries={0};UssMenuRowsIo io;uint32_t readback=0;
    uintptr_t fields[13],panel,bar;unsigned count=0,i,before;uint32_t value=3;unsigned char active=1;DWORD old;
    screen=VirtualAlloc(NULL,5000000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);assert(screen);
    writer=fixture(&sdk,&api);writer.owner=&writer;panel=(uintptr_t)screen+3010456+1150848;bar=(uintptr_t)screen+1248;
    io=uss_menu_sdk_rows_io(&writer,&entries);assert(io.user==&writer && !proof_calls && !writer.attempts);
    assert(io.guard(io.user,&writer.host,0));
    assert(io.write(io.user,(uintptr_t)screen+8,&value,4));
    assert(io.read(io.user,(uintptr_t)screen+8,&readback,4) && readback==value);
    fields[count++]=(uintptr_t)screen+8;fields[count++]=(uintptr_t)screen+3010456+1318492;
    fields[count++]=(uintptr_t)screen+3010456+1318500;fields[count++]=(uintptr_t)screen+3010456+1318476;
    fields[count++]=panel+2832;fields[count++]=panel+2836;fields[count++]=panel+2840;fields[count++]=panel+2844;fields[count++]=panel+2872;
    for(i=0;i<3;++i)fields[count++]=bar+3400*i+11004;
    for(i=0;i<count;++i){assert(uss_menu_sdk_write_field(&writer,fields[i],&value,4));assert(!memcmp((const void*)fields[i],&value,4));}
    for(i=0;i<3;++i)assert(uss_menu_sdk_write_field(&writer,bar+3400*i+11021,&active,1));
    assert(uss_menu_sdk_write_field(&writer,(uintptr_t)screen+3010456+1319449,&active,1));
    before=writer.attempts;
    assert(!uss_menu_sdk_write_field(&writer,fields[0]+1,&value,4));
    assert(!uss_menu_sdk_write_field(&writer,fields[0],&value,1));
    assert(!uss_menu_sdk_write_field(&writer,panel+2856,&value,4)); /* Never settings pointer. */
    assert(!uss_menu_sdk_write_field(&writer,bar+11021,&value,4));
    assert(writer.attempts==before);
    for(i=1;i<=2;++i){writer=fixture(&sdk,&api);writer.owner=&writer;fail_proof=i;
        assert(!uss_menu_sdk_write_field(&writer,fields[0],&value,4) && !writer.attempts);}
    writer=fixture(&sdk,&api);writer.owner=&writer;fail_proof=3;
    assert(!uss_menu_sdk_write_field(&writer,fields[0],&value,4) && writer.stopped && writer.attempts==1);
    assert(!memcmp((const void*)fields[0],&value,4)); /* Entered effect is not rolled back. */
    fail_proof=0;assert(!uss_menu_sdk_write_field(&writer,fields[0],&value,4) && writer.attempts==1);
    writer=fixture(&sdk,&api);writer.owner=&writer;fail_proof=4;
    assert(!uss_menu_sdk_write_field(&writer,fields[0],&value,4) && writer.stopped && writer.attempts==1);
    writer=fixture(&sdk,&api);writer.owner=&writer;fail_read=1;
    assert(!uss_menu_sdk_write_field(&writer,fields[0],&value,4) && writer.stopped);
    writer=fixture(&sdk,&api);writer.owner=&writer;change_epoch=2;
    assert(!uss_menu_sdk_write_field(&writer,fields[0],&value,4) && !writer.attempts);
    writer=fixture(&sdk,&api);writer.owner=&writer;bad_build=1;
    assert(!uss_menu_sdk_write_field(&writer,fields[0],&value,4) && !writer.attempts);
    writer=fixture(&sdk,&api);writer.owner=&writer;
    assert(VirtualProtect(screen,4096,PAGE_READONLY,&old));
    assert(!uss_menu_sdk_write_field(&writer,fields[0],&value,4) && !writer.attempts);
    assert(VirtualProtect(screen,4096,old,&old)); /* Fixture cleanup only; production never changes protection. */
    assert(VirtualFree(screen,0,MEM_RELEASE));
    puts("USS native menu writer: real own-memory writes, exact field allowlist, scope/page proof and post-entry no retry PASS");return 0;
}
