#include <assert.h>
#include <stdio.h>
#include "../src/uss_native_menu_sdk.h"
static unsigned fail_read,bad_build,language_switch;static uintptr_t language_manager=0x20000000;
static uintptr_t fixture_base=0x10000000,code_at;
static unsigned raw_calls,code_calls,code_fail,change_during_code;
static unsigned char original_byte=0x48;
static const char* build(void){return bad_build?"other":"25480438";}
static uintptr_t module(const char* name,size_t* size){assert(!strcmp(name,"game.dll"));*size=0x4000000;return fixture_base;}
static int read_sdk(uintptr_t at,void* out,size_t length)
{
    if(at==code_at && code_at){++raw_calls;memset(out,0xe9,length);return 0;}
    if(fail_read)return 1;
    if(at==0x10000000+55037640 && length==8){memcpy(out,&language_manager,8);return 0;}
    if(at==language_manager+106676 && length==1){*(unsigned char*)out=(unsigned char)language_switch;return 0;}
    return 1;
}
static int read_original_code(uintptr_t at,void* out,size_t length)
{
    assert(at==code_at);++code_calls;memset(out,original_byte,length);
    if(change_during_code)bad_build=1;
    return code_fail?1:0;
}
int main(void)
{
    HfApi api={0};UssMenuSdkContext context;UssMenuObservationIo observer;UssMenuEntryReader entries;uintptr_t value=0;unsigned char pin[64];
    api.version=12;api.size=sizeof(api);api.build_id=build;api.module_base=module;api.read=read_sdk;
    context.api=&api;context.base=0x10000000;context.module_size=0x4000000;context.thread=GetCurrentThreadId();context.epoch=1;
    observer=uss_menu_sdk_observer(&context);entries=uss_menu_sdk_entries(&context);
    assert(observer.context(observer.user)==1 && observer.read(observer.user,0x10000000+55037640,&value,8) && value==language_manager);
    assert(observer.language_state(observer.user)==0);language_switch=1;assert(observer.language_state(observer.user)==1);language_switch=0;
    fail_read=1;assert(!observer.read(observer.user,0x10000000+55037640,&value,8) && observer.language_state(observer.user)==-1);fail_read=0;
    bad_build=1;assert(!observer.context(observer.user));bad_build=0;
    ++context.thread;assert(!observer.context(observer.user));--context.thread;
    context.epoch=0;assert(!observer.context(observer.user));context.epoch=1;
    assert(!entries.read_executable(entries.user,0x10000000,pin,0));
    assert(!entries.read_executable(entries.user,0x10000000+0x4000000,pin,1));
    /* Own EXE's code page supplies a real MEM_IMAGE/executable-page identity.
     * Mock SDK bytes model an installed hook; no process or game is accessed. */
    fixture_base=(uintptr_t)GetModuleHandleW(NULL);code_at=(uintptr_t)read_sdk;
    context.base=fixture_base;
    api.version=16;api.size=USS_READ_CODE_API_BYTES;api.read_code=read_original_code;
    memset(pin,0xaa,sizeof(pin));
    assert(entries.read_executable(entries.user,code_at,pin,16) && pin[0]==0x48 && code_calls==1 && !raw_calls);
    api.version=12;
    assert(entries.read_executable(entries.user,code_at,pin,16) && pin[0]==0xe9 && raw_calls==1 && code_calls==1);
    api.version=16;api.size=USS_READ_CODE_API_BYTES-1;
    assert(entries.read_executable(entries.user,code_at,pin,16) && raw_calls==2 && code_calls==1);
    api.size=USS_READ_CODE_API_BYTES;api.read_code=NULL;
    assert(entries.read_executable(entries.user,code_at,pin,16) && raw_calls==3 && code_calls==1);
    api.read_code=read_original_code;code_fail=1;memset(pin,0xaa,sizeof(pin));
    assert(!entries.read_executable(entries.user,code_at,pin,16) && raw_calls==3 && pin[0]==0xaa);
    code_fail=0;original_byte=0xcc;
    assert(entries.read_executable(entries.user,code_at,pin,16) && pin[0]==0xcc); /* Real mismatches are NOT erased. */
    original_byte=0x48;change_during_code=1;memset(pin,0xaa,sizeof(pin));
    assert(!entries.read_executable(entries.user,code_at,pin,16) && pin[0]==0xaa && raw_calls==3);
    bad_build=change_during_code=0;
    {unsigned before_code=code_calls,before_raw=raw_calls;
     assert(!entries.read_executable(entries.user,fixture_base+0x4000000,pin,1));
     assert(code_calls==before_code && raw_calls==before_raw);}
    puts("USS menu SDK: actual0-success translation, thread/build/epoch, language unknown/switch and executable bounds and optional hook-aware reads without failure fallback PASS");return 0;
}
