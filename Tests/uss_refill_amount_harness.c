#include <assert.h>
#include <stdio.h>
#include "../src/uss_refill_amount.h"
static unsigned char manager[176],buckets[32],units[8],unit[24],row[8];
static unsigned reads,fail_at,changed,scopes;static unsigned kind;
static unsigned char settings_hash[32],settings[160],resource[10000],resource_owner[8];static uintptr_t resource_offset;
static uint64_t context(void* p){(void)p;return ++scopes>1 && changed?2:1;}
static int read_memory(void* p,uintptr_t at,void* out,size_t size)
{
    const struct {uintptr_t at;void* data;size_t size;} ranges[]={
        {0x20000000,manager,176},{0x21000000,buckets,32},{0x22000000,units,8},{0x23000000,unit,24},{0x24000000,row,8}};
    const struct {uintptr_t at;void* data;size_t size;} extra[]={
        {0x25000000,settings_hash,32},{0x26000000,settings,160},{0x27000000,resource,10000}};
    uintptr_t global=kind==1?0x3326648:kind==2?0x3326cf0:0x33265e8;unsigned i;(void)p;
    if(++reads==fail_at)return 0;
    if(at==0x10000000+0x346bf98 && size==8){uintptr_t value=0x28000000;memcpy(out,&value,8);return 1;}
    if(at==0x28000000+resource_offset && size==8){memcpy(out,resource_owner,8);return 1;}
    for(i=0;i<3;++i)if(at>=extra[i].at && at-extra[i].at<=extra[i].size && size<=extra[i].size-(at-extra[i].at)){
        memcpy(out,(unsigned char*)extra[i].data+at-extra[i].at,size);return 1;}

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
    kind=k;memset(manager,0,176);memset(buckets,0,32);memset(unit,0,24);memset(row,0,8);
    pointer(manager+h,0x21000000);word(manager+h+8,4);word(manager+h+12,UINT32_MAX);word(manager+h+16,1);
    pointer(manager+u,0x22000000);pointer(manager+r,0x24000000);pointer(units,0x23000000);
    word(buckets+8,5);word(buckets+12,0);unit[0]=7;word(unit+8,5);word(row,12);word(row+4,99);
    reads=fail_at=changed=scopes=0;
}
static void amount_fixture(unsigned k,unsigned fallback)
{
    unsigned h=k==1?0x60:0x68,rows=k==1?0xa0:0xa8,a=k==1?0x90:0x54;
    unsigned cap=k==1?540:50,bucket=7%cap;uintptr_t address=k==1?54*160:800;
    fixture(k);memset(settings_hash,0,32);memset(settings,0,160);memset(resource,0,10000);
    pointer(manager+h,0x25000000);word(manager+h+8,4);word(manager+h+12,UINT32_MAX);word(manager+h+16,1);
    word(settings_hash+8,fallback?UINT32_MAX:5);word(settings_hash+12,0);pointer(manager+rows,0x26000000);word(settings+a,3);
    resource_offset=k==1?0xf124a0:0xf12820;pointer(resource_owner,0x27000000);
    resource[bucket*16]=7;word(resource+bucket*16+8,0);word(resource+address+a,4);
}
int main(void)
{
    UssRefillMemoryIo io={NULL,0x10000000,read_memory,context};UssRefillComponent component;
    unsigned char id[8]={7};unsigned k,f,i,baseline;uint32_t amount;
    for(k=1;k<=2;++k)for(f=0;f<=1;++f){
        amount_fixture(k,f);assert(uss_refill_component_read(&io,5,id,k,&component)==1);
        reads=0;assert(uss_refill_direct_amount(&io,&component,&amount) && amount==(f?4u:3u));baseline=reads;
        for(i=1;i<=baseline;++i){amount_fixture(k,f);fail_at=i;amount=42;
            assert(!uss_refill_direct_amount(&io,&component,&amount) && amount==42);}
        amount_fixture(k,f);changed=1;assert(!uss_refill_direct_amount(&io,&component,&amount));
    }
    amount_fixture(1,0);word(settings+0x90,0);assert(uss_refill_direct_amount(&io,&component,&amount)==0); /* stale rounds snapshot */
    assert(uss_refill_component_read(&io,5,id,1,&component)==1);
    assert(uss_refill_direct_amount(&io,&component,&amount) && amount==0);
    word(settings+0x90,1001);assert(!uss_refill_direct_amount(&io,&component,&amount));
    amount_fixture(1,1);resource[7*16]=0;amount=42;
    assert(!uss_refill_direct_amount(&io,&component,&amount) && amount==42);
    amount_fixture(1,1);word(settings_hash,6);word(settings_hash+8,6);word(settings_hash+16,6);word(settings_hash+24,6);
    assert(!uss_refill_direct_amount(&io,&component,&amount)); /* exhausted override must not use valid resource */
    puts("USS refill amounts: MAG/ROUNDS override/resource, every read fault, zero, invalid amount, stale context PASS");return 0;
}
