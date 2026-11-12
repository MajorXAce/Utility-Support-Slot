#include <assert.h>
#include <stdio.h>
#include "../src/uss_refill_components.h"
static unsigned char manager[96],buckets[32],units[8],unit[24],row[8];
static unsigned reads,fail_at,changed,scopes;static unsigned kind;
static uint64_t context(void* p){(void)p;return ++scopes>1 && changed?2:1;}
static int read_memory(void* p,uintptr_t at,void* out,size_t size)
{
    const struct {uintptr_t at;void* data;size_t size;} ranges[]={
        {0x20000000,manager,96},{0x21000000,buckets,32},{0x22000000,units,8},{0x23000000,unit,24},{0x24000000,row,8}};
    uintptr_t global=kind==1?0x3326648:kind==2?0x3326cf0:0x33265e8;unsigned i;(void)p;
    if(++reads==fail_at)return 0;
    if(at==0x10000000+global && size==8){uintptr_t value=0x20000000;memcpy(out,&value,8);return 1;}
    for(i=0;i<5;++i)if(at>=ranges[i].at && at-ranges[i].at<=ranges[i].size && size<=ranges[i].size-(at-ranges[i].at)){
        memcpy(out,(unsigned char*)ranges[i].data+at-ranges[i].at,size);return 1;}
    return 0;
}
static void word(unsigned char* p,uint32_t n){memcpy(p,&n,4);}
static void pointer(unsigned char* p,uintptr_t n){memcpy(p,&n,8);}
static void fixture(unsigned k)
{
    unsigned h=k==2?0x28:0x20,u=k==2?0x40:0x38,r=k==2?0x58:0x50;
    kind=k;memset(manager,0,96);memset(buckets,0,32);memset(unit,0,24);memset(row,0,8);
    pointer(manager+h,0x21000000);word(manager+h+8,4);word(manager+h+12,UINT32_MAX);word(manager+h+16,1);
    pointer(manager+u,0x22000000);pointer(manager+r,0x24000000);pointer(units,0x23000000);
    word(buckets+8,5);word(buckets+12,0);unit[0]=7;word(unit+8,5);word(row,12);word(row+4,99);
    reads=fail_at=changed=scopes=0;
}
int main(void)
{
    UssRefillMemoryIo io={NULL,0x10000000,read_memory,context};unsigned char id[8]={7};
    UssRefillComponent out,before;unsigned k,i,baseline;
    for(k=1;k<=3;++k){
        fixture(k);assert(uss_refill_component_read(&io,5,id,k,&out)==1 && out.reserve==12 && out.kind==k);
        assert(out.raw==(k==3?99u:0u));baseline=reads;
        for(i=1;i<=baseline;++i){fixture(k);fail_at=i;memset(&out,0xa5,sizeof(out));before=out;
            assert(uss_refill_component_read(&io,5,id,k,&out)==-1 && !memcmp(&out,&before,sizeof(out)));}
        fixture(k);changed=1;assert(uss_refill_component_read(&io,5,id,k,&out)==-1);
        fixture(k);word(row,1001);assert(uss_refill_component_read(&io,5,id,k,&out)==-1);
        fixture(k);unit[0]=8;assert(uss_refill_component_read(&io,5,id,k,&out)==-1);
        fixture(k);word(buckets+8,UINT32_MAX);assert(uss_refill_component_read(&io,5,id,k,&out)==0);
    }
    fixture(1);word(row,1001);assert(uss_refill_ammo_read(&io,5,id,&out)==-1);
    puts("USS ammo/deposit readers: all read faults, scope, identity, ranges, absence and no malformed fallback PASS");return 0;
}
