#include <assert.h>
#include <stdio.h>
#include "../src/uss_refill_entries.h"
static unsigned reads,fail_at,changed,scopes,bad_pack,bad_default;
static uint64_t scope(void* user){(void)user;return ++scopes>1 && changed?2:1;}
static int code(void* user,uintptr_t at,void* out,size_t length)
{
    unsigned i;(void)user;if(++reads==fail_at)return 0;
    for(i=0;i<28;++i)if(at==0x10000000+uss_refill_pins[i].rva && length==uss_refill_pins[i].length){
        memcpy(out,uss_refill_pins[i].bytes,length);if(bad_pack && i>=16)((unsigned char*)out)[0]^=1;return 1;}
    return 0;
}
static int data(void* user,uintptr_t at,void* out,size_t length)
{
    uint32_t one=bad_default?0:0x3f800000;(void)user;
    if(++reads==fail_at)return 0;
    if(length!=4 || (at!=0x10000000+0x21417b4 && at!=0x10000000+0x23c6d70))return 0;
    memcpy(out,&one,4);return 1;
}
static void reset(void){reads=fail_at=changed=scopes=bad_pack=bad_default=0;}
int main(void)
{
    UssRefillEntryIo io={NULL,0x10000000,0x4000000,scope,code,data};UssRefillEntries result,before;unsigned i,total;
    reset();assert(uss_refill_entries_read(&io,&result) && result.pack_valid && result.epoch==1 &&
        (uintptr_t)result.calls.mag==io.base+0x76e6d0 && (uintptr_t)result.calls.rounds==io.base+0x77a430);total=reads;
    for(i=1;i<=total;++i){unsigned position=(i-1)%30;reset();fail_at=i;memset(&result,0xa5,sizeof(result));before=result;
        if(position>=16 && position<28)assert(uss_refill_entries_read(&io,&result) && !result.pack_valid);
        else assert(!uss_refill_entries_read(&io,&result) && !memcmp(&result,&before,sizeof(result)));}
    reset();bad_pack=1;assert(uss_refill_entries_read(&io,&result) && !result.pack_valid);
    reset();bad_default=1;assert(!uss_refill_entries_read(&io,&result));
    reset();changed=1;assert(!uss_refill_entries_read(&io,&result));
    puts("USS refill entries: exact two-pass pins, every read fault, optional Pack isolation, default scale and scope refusal; NO native call PASS");return 0;
}
