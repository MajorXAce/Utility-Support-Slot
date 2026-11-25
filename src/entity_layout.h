/* Source-qualified field schema: HellForge a4898611 moved_fields.c, MF_ENTITY_RECORDS.
 * Caller supplies bounded, fault-aware reads and retains/rechecks all observations.
 * No native calls, allocation, retries, or retained game-pointer dereference. */
#ifndef EIF_ENTITY_LAYOUT_H
#define EIF_ENTITY_LAYOUT_H
#include <stdint.h>
#include <string.h>
typedef int (*EelRead)(void*,uint64_t,void*,unsigned);
typedef struct EelSite {uint32_t rva;unsigned size,scale;int32_t add;unsigned char code[14];} EelSite;
static const EelSite eel_sites[]={
 {0xfd9dd4,10,8,0,{0x8b,0x40,4,0x48,0x8d,4,0x40,0x48,0x8d,0x80}},
 {0xfd9c49,10,8,8,{0x8b,0x40,4,0x48,0x8d,4,0x40,0x48,0x8d,0x80}},
 {0xfd9b88,5,24,16,{0x8b,0x40,4,0x48,5}},
 {0xfd949f,8,1,12,{0x48,0x8d,0x0c,0x40,0x44,0x8b,0x8c,0xcb}},
 {0xfda207,3,1,16,{0x48,0x8d,0x8b}},
 {0xfdc1a4,4,8,0,{0x4e,0x8d,0x34,0x7d}},
 {0xfdc1fc,3,1,8,{0x39,0x8c,0xd6}},
 {0xab402b,11,8,0xe49f0,{0x41,0x8b,0x41,4,0x48,0x8d,4,0x40,0x48,0x8d,0x80}}
};
static int eel_records(EelRead read,void* user,uint64_t base,uint64_t image_size,int32_t* output)
{
 unsigned i;int32_t anchor=0;
 if(!read||!output||base<0x10000||base>=UINT64_C(0x800000000000)||image_size>UINT64_C(0x800000000000)-base)return 0;
 for(i=0;i<sizeof(eel_sites)/sizeof(eel_sites[0]);++i){
  const EelSite* s=&eel_sites[i];unsigned char bytes[20]={0};int32_t raw;int64_t value;
  if(s->rva>image_size||s->size+4>image_size-s->rva||!read(user,base+s->rva,bytes,s->size+4)||memcmp(bytes,s->code,s->size))return 0;
  memcpy(&raw,bytes+s->size,4);value=(int64_t)raw*s->scale-s->add;
  if(value<INT32_MIN||value>INT32_MAX||((uint32_t)value&7)||(i&&value!=anchor))return 0;
  anchor=(int32_t)value;
 }
 *output=anchor;return 1;
}
static int eel_record_address(uint64_t root,int32_t displacement,uint32_t index,uint64_t* output)
{
 int64_t delta=(int64_t)displacement+(int64_t)index*24;uint64_t result;
 if(!output||root<0x10000||root>=UINT64_C(0x800000000000))return 0;
 if(delta<0){if(root<(uint64_t)(-delta))return 0;result=root-(uint64_t)(-delta);}
 else {if((uint64_t)delta>UINT64_C(0x800000000000)-root)return 0;result=root+(uint64_t)delta;}
 if(result<0x10000||result>UINT64_C(0x800000000000)-24)return 0;
 *output=result;return 1;
}
#endif
