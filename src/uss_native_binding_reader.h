/* Native binding records/evaluated actions, per original 0BSD ModBindingsMenu.
 * Read-only. This does not reserve an action, change mappings or create rows.
 * Mapping payloads stay opaque: engine IDs are not Windows virtual keys. */
#ifndef USS_NATIVE_BINDING_READER_H
#define USS_NATIVE_BINDING_READER_H
#include "uss_native_menu_observation.h"
enum {USS_BINDING_RECORD_BYTES=328,USS_BINDING_MAP_RECORDS=256,USS_BINDING_MAPPING_BYTES=20,
    USS_BINDING_MAX_MAPPINGS=16,USS_BINDING_READ_CALLS=272,USS_BINDING_READ_BYTES=4096};
typedef struct UssNativeBindingCache {uint64_t epoch;uintptr_t owner,buckets,record;uint32_t code,map_offset;} UssNativeBindingCache;
typedef struct UssNativeBindingSnapshot {
    uint64_t epoch;uintptr_t owner,buckets,record;uint32_t code,count;
    unsigned active,action_known,defaults,valid;unsigned char mappings[320];
} UssNativeBindingSnapshot;
typedef struct UssNativeBindingBudget {
    const UssMenuObservationIo* io;unsigned calls,bytes,failed;
} UssNativeBindingBudget;
static inline int uss_native_binding_read(UssNativeBindingBudget* b,uintptr_t at,void* out,size_t size)
{
    if(b->failed || !size || size>328 || b->calls>=USS_BINDING_READ_CALLS ||
        size>USS_BINDING_READ_BYTES-b->bytes || at<0x10000 || at>UINTPTR_MAX-size){b->failed=1;return 0;}
    ++b->calls;b->bytes+=(unsigned)size;
    if(!b->io->read(b->io->user,at,out,size)){b->failed=1;return 0;}return 1;
}
static inline int uss_native_binding_observe_map(const UssMenuObservationIo* io,UssNativeBindingCache* cache,
    unsigned group,unsigned action,unsigned defaults,UssNativeBindingSnapshot* output,UssNativeBindingBudget* statistics)
{
    UssNativeBindingBudget budget={0};UssNativeBindingSnapshot found={0};UssNativeBindingCache next={0};
    unsigned char map[12],again_map[12],record[328],again_record[328],header[8],state;
    uintptr_t owner,buckets,again,record_at=0;uint64_t epoch;unsigned i;uint32_t code,map_offset;
    if(!io || !io->context || !io->read || !cache || !output || group>=13 || action>=97 || defaults>1 ||
        !uss_menu_pointer(io->base) || io->module_size<0x347cf20 || io->base>UINTPTR_MAX-io->module_size)return 0;
    budget.io=io;epoch=io->context(io->user);if(!epoch)goto refused;code=group*65536u+action;map_offset=defaults?686968:686800;state=0;
    if(!uss_native_binding_read(&budget,io->base+0x347cf18,&owner,8) || !uss_menu_pointer(owner) ||
        owner>UINTPTR_MAX-687000 || !uss_native_binding_read(&budget,owner+map_offset,map,12))goto refused;
    memcpy(&buckets,map,8);
    if(!uss_menu_pointer(buckets) || uss_menu_word(map,8)!=USS_BINDING_MAP_RECORDS ||
        buckets>UINTPTR_MAX-USS_BINDING_RECORD_BYTES*USS_BINDING_MAP_RECORDS)goto refused;
    if(cache->map_offset==map_offset && cache->epoch==epoch && cache->owner==owner && cache->buckets==buckets && cache->code==code &&
        cache->record>=buckets && cache->record-buckets<USS_BINDING_RECORD_BYTES*USS_BINDING_MAP_RECORDS &&
        (cache->record-buckets)%USS_BINDING_RECORD_BYTES==0){
        if(!uss_native_binding_read(&budget,cache->record,header,8))goto refused;
        if(uss_menu_word(header,0)==code)record_at=cache->record;
    }
    if(!record_at){
        /* Fixed scan, header only; never follow linked hash entries. Duplicated
         * target code refuses rather than arbitrarily selecting an owner. */
        for(i=0;i<USS_BINDING_MAP_RECORDS;++i){
            uintptr_t at=buckets+(uintptr_t)i*USS_BINDING_RECORD_BYTES;
            if(!uss_native_binding_read(&budget,at,header,8))goto refused;
            if(uss_menu_word(header,0)==code){if(record_at)goto refused;record_at=at;}
        }
        if(!record_at)goto refused;
    }
    if(!uss_native_binding_read(&budget,record_at,record,sizeof(record)) || uss_menu_word(record,0)!=code ||
        uss_menu_word(record,4)>USS_BINDING_MAX_MAPPINGS ||
        (!defaults && !uss_native_binding_read(&budget,owner+808+32*(97*group+action),&state,1)) ||
        !uss_native_binding_read(&budget,record_at,again_record,sizeof(again_record)) || memcmp(record,again_record,sizeof(record)) ||
        !uss_native_binding_read(&budget,owner+map_offset,again_map,12) || memcmp(map,again_map,sizeof(map)) ||
        !uss_native_binding_read(&budget,io->base+0x347cf18,&again,8) || again!=owner || io->context(io->user)!=epoch)goto refused;
    found.epoch=epoch;found.owner=owner;found.buckets=buckets;found.record=record_at;found.code=code;
    found.count=uss_menu_word(record,4);found.active=state!=0;found.action_known=!defaults;found.defaults=defaults;found.valid=1;
    memcpy(found.mappings,record+8,found.count*USS_BINDING_MAPPING_BYTES);
    next.epoch=epoch;next.owner=owner;next.buckets=buckets;next.record=record_at;next.code=code;next.map_offset=map_offset;
    *cache=next;*output=found;if(statistics)*statistics=budget;return 1;
refused:
    memset(cache,0,sizeof(*cache));if(statistics)*statistics=budget;return 0; /* Caller output unchanged; unknown is not released. */
}
static inline int uss_native_binding_observe(const UssMenuObservationIo* io,UssNativeBindingCache* cache,
    unsigned group,unsigned action,UssNativeBindingSnapshot* output,UssNativeBindingBudget* statistics)
{return uss_native_binding_observe_map(io,cache,group,action,0,output,statistics);}
#endif
