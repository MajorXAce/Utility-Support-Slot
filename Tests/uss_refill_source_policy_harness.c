#include <assert.h>
#include <stdio.h>
#include "../src/uss_refill_source_policy.h"
static unsigned char manager[80],buckets[32],units[8],unit[24],row[56];static unsigned reads,fail_at,scopes,changed;static float fallback=1;
static uint64_t context(void* p){(void)p;return ++scopes>1 && changed?2:1;}
static int read_memory(void* p,uintptr_t at,void* out,size_t size)
{
    const struct {uintptr_t at;void* data;size_t size;} ranges[]={{0x20000000,manager,80},{0x21000000,buckets,32},{0x22000000,units,8},{0x23000000,unit,24},{0x24000000,row,56}};unsigned i;
    (void)p;if(++reads==fail_at)return 0;
    if(at==0x10000000+0x3326ac0 && size==8){uintptr_t value=0x20000000;memcpy(out,&value,8);return 1;}
    if(at==0x10000000+0x21417b4 && size==4){memcpy(out,&fallback,4);return 1;}
    for(i=0;i<5;++i)if(at>=ranges[i].at && at-ranges[i].at<=ranges[i].size && size<=ranges[i].size-(at-ranges[i].at)){
        memcpy(out,(unsigned char*)ranges[i].data+at-ranges[i].at,size);return 1;}
    return 0;
}
static void word(unsigned char* p,uint32_t n){memcpy(p,&n,4);}
static void pointer(unsigned char* p,uintptr_t n){memcpy(p,&n,8);}
static void fixture(void)
{
    float scale=9999;memset(manager,0,80);memset(buckets,0,32);memset(row,0,56);memset(unit,0,24);
    pointer(manager+0x18,0x21000000);word(manager+0x20,4);word(manager+0x24,UINT32_MAX);word(manager+0x28,1);
    pointer(manager+0x30,0x22000000);pointer(manager+0x40,0x24000000);pointer(units,0x23000000);
    word(buckets+8,5);word(buckets+12,0);word(unit+8,5);word(row,1);word(row+4,9);word(row+0x1c,1);memcpy(row+0x20,&scale,4);
    reads=fail_at=scopes=changed=0;
}
int main(void)
{
    UssRefillMemoryIo io={NULL,0x10000000,read_memory,context};float scale;unsigned baseline,i;
    fixture();assert(uss_refill_ground_scale(&io,5,7,&scale) && scale==9999);baseline=reads;
    for(i=1;i<=baseline;++i){fixture();fail_at=i;scale=42;assert(!uss_refill_ground_scale(&io,5,7,&scale) && scale==42);}
    fixture();word(buckets+8,UINT32_MAX);assert(uss_refill_ground_scale(&io,5,7,&scale) && scale==1);
    fixture();word(buckets+8,6);word(buckets+16,6);word(buckets+24,6);word(buckets,6);scale=42;assert(!uss_refill_ground_scale(&io,5,7,&scale) && scale==42);
    fixture();word(manager+0x20,3);assert(!uss_refill_ground_scale(&io,5,7,&scale));
    fixture();changed=1;assert(!uss_refill_ground_scale(&io,5,7,&scale));
    fixture();word(row+0x1c,0);assert(!uss_refill_ground_scale(&io,5,7,&scale));
    fixture();assert(uss_refill_ground_scale(&io,5,4,&scale) && scale==0.5f && !reads);
    assert(!uss_refill_ground_scale(&io,5,1,&scale));
    puts("USS refill source: native half/default/property9 scale, exact +0x20 value, all read faults, absence vs unknown and scope checks PASS");return 0;
}
