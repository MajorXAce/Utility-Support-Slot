#include <assert.h>
#include <stdio.h>
#include "../src/uss_native_binding_reader.h"
static unsigned char input[687000],buckets[328*256],default_buckets[328*256];
static unsigned reads,fail_at,scopes,bad_scope,change_record;static uintptr_t root;
static uint64_t scope(void* user){(void)user;return ++scopes>1 && bad_scope?2:1;}
static int read_memory(void* user,uintptr_t at,void* out,size_t length)
{
    (void)user;++reads;if(reads==fail_at)return 0;
    if(at==0x10000000+0x347cf18 && length==8){memcpy(out,&root,8);return 1;}
    if(at>=0x20000000 && at-0x20000000<=sizeof(input) && length<=sizeof(input)-(at-0x20000000)){
        memcpy(out,input+(at-0x20000000),length);return 1;}
    if(at>=0x30000000 && at-0x30000000<=sizeof(buckets) && length<=sizeof(buckets)-(at-0x30000000)){
        if(length==328 && change_record){++buckets[328*3+8];}
        memcpy(out,buckets+(at-0x30000000),length);return 1;}
    if(at>=0x31000000 && at-0x31000000<=sizeof(default_buckets) && length<=sizeof(default_buckets)-(at-0x31000000)){
        memcpy(out,default_buckets+(at-0x31000000),length);return 1;}
    return 0;
}
static UssMenuObservationIo fixture(void)
{
    UssMenuObservationIo io={NULL,0x10000000,0x4000000,scope,NULL,read_memory};uintptr_t pointer=0x30000000;unsigned i;
    memset(input,0,sizeof(input));memset(buckets,0,sizeof(buckets));reads=fail_at=scopes=bad_scope=change_record=0;root=0x20000000;
    memcpy(input+686800,&pointer,8);uss_menu_put_word(input,686808,256);
    for(i=0;i<256;++i)uss_menu_put_word(buckets,328*i,UINT32_MAX);
    uss_menu_put_word(buckets,328*3,12*65536+1);uss_menu_put_word(buckets,328*3+4,2);
    for(i=0;i<40;++i)buckets[328*3+8+i]=(unsigned char)(i+1);
    memcpy(default_buckets,buckets,sizeof(buckets));default_buckets[328*3+8]=200;
    pointer=0x31000000;memcpy(input+686968,&pointer,8);uss_menu_put_word(input,686976,256);
    input[808+32*(97*12+1)]=1;return io;
}
#ifndef USS_NATIVE_BINDING_READER_TEST_MAIN
#define USS_NATIVE_BINDING_READER_TEST_MAIN main
#endif
int USS_NATIVE_BINDING_READER_TEST_MAIN(void)
{
    UssMenuObservationIo io;UssNativeBindingCache cache={0};UssNativeBindingSnapshot result={0},before;
    UssNativeBindingBudget budget;unsigned total,i,cold;
    io=fixture();assert(uss_native_binding_observe(&io,&cache,12,1,&result,&budget));
    assert(result.valid && result.active && result.count==2 && result.record==0x30000000+328*3 && result.mappings[39]==40 && result.mappings[40]==0);
    total=reads;cold=reads;before=result;assert(budget.calls<=272 && budget.bytes<=4096);
    reads=scopes=0;assert(uss_native_binding_observe(&io,&cache,12,1,&result,&budget) && reads<cold && reads==8);
    for(i=1;i<=total;++i){io=fixture();memset(&cache,0,sizeof(cache));fail_at=i;result=before;
        assert(!uss_native_binding_observe(&io,&cache,12,1,&result,&budget) && !memcmp(&result,&before,sizeof(result)) && !cache.record);}
    io=fixture();memset(&cache,0,sizeof(cache));bad_scope=1;assert(!uss_native_binding_observe(&io,&cache,12,1,&result,&budget));
    io=fixture();change_record=1;assert(!uss_native_binding_observe(&io,&cache,12,1,&result,&budget));
    io=fixture();uss_menu_put_word(buckets,328*4,12*65536+1);assert(!uss_native_binding_observe(&io,&cache,12,1,&result,&budget));
    io=fixture();uss_menu_put_word(buckets,328*3+4,17);assert(!uss_native_binding_observe(&io,&cache,12,1,&result,&budget));
    io=fixture();uss_menu_put_word(input,686808,255);assert(!uss_native_binding_observe(&io,&cache,12,1,&result,&budget));
    io=fixture();assert(!uss_native_binding_observe(&io,&cache,13,0,&result,&budget) && !reads);
    io=fixture();input[808+32*(97*12+1)]=0;assert(uss_native_binding_observe(&io,&cache,12,1,&result,&budget) && !result.active);
    io=fixture();memset(&cache,0,sizeof(cache));
    assert(uss_native_binding_observe(&io,&cache,12,1,&result,&budget) && result.action_known && !result.defaults && result.mappings[0]==1);
    reads=scopes=0;
    assert(uss_native_binding_observe_map(&io,&cache,12,1,1,&result,&budget) && result.defaults && !result.action_known && !result.active && result.mappings[0]==200);
    assert(cache.map_offset==686968);reads=scopes=0;
    assert(uss_native_binding_observe_map(&io,&cache,12,1,1,&result,&budget) && reads==7);
    assert(uss_native_binding_observe(&io,&cache,12,1,&result,&budget) && result.mappings[0]==1 && result.action_known);
    puts("USS native binding reader: bounded map discovery, opaque mappings, engine action state, warm cache and all cold read refusals PASS");return 0;
}
