/* Production completion reader and one-shot source classifier from the Lua
 * pickup filter. Selected/cache entries are evidence, never refill triggers. */
#ifndef USS_PICKUP_COMPLETION_H
#define USS_PICKUP_COMPLETION_H
#include "uss_refill_source_policy.h"
enum {USS_PICKUP_CACHE_CAP=16,USS_PICKUP_ZONE_CAP=8};
typedef struct UssPickupAction {
    uint64_t epoch;uintptr_t manager,unit,row;uint32_t avatar,entity,zone;
    unsigned char bytes[16];unsigned valid;
} UssPickupAction;
typedef struct UssPickupSource {
    uint32_t entity;unsigned char identity[8];uint64_t time_ms;
    uint32_t kind[8];float scale[8];unsigned verified_zones,consumed;
} UssPickupSource;
typedef struct UssPickupActionDiagnostic {const char* stage;uint32_t actual,expected;UssRefillHashDiagnostic hash;} UssPickupActionDiagnostic;

typedef struct UssPickupCompletions {
    UssRefillTarget target;unsigned bound,have_previous,disabled;
    uint32_t previous_entity,previous_zone;
    uint64_t serial,activity_ms,next_poll,next_source;unsigned activity_valid;
    uint32_t selected_entity,selected_zone;uintptr_t selected_winner;
    const char* reason;const char* source_reason;const char* selected_source_reason;UssPickupActionDiagnostic action_diagnostic;
    UssPickupSource cache[USS_PICKUP_CACHE_CAP];
} UssPickupCompletions;
/* Checkpoints annotate the existing reads; they never retry or add a read. */
static inline int uss_pickup_read_action_diagnose(const UssRefillMemoryIo* io,const UssRefillTarget* target,
    UssPickupAction* output,UssPickupActionDiagnostic* diagnostic)
{
    UssRefillMemory r={0};UssPickupAction found={0};uintptr_t manager,again,unit;uint32_t index,again_index,count,again_count,avatar;
    unsigned char descriptor[24],again_descriptor[24],again_bytes[16];
#define USS_PICKUP_ACTION_STAGE(name) do {if(diagnostic){diagnostic->stage=(name);diagnostic->actual=diagnostic->expected=0;}}while(0)
#define USS_PICKUP_ACTION_VALUE(value,wanted) do {if(diagnostic){diagnostic->actual=(value);diagnostic->expected=(wanted);}}while(0)
    USS_PICKUP_ACTION_STAGE("ACTION_ARGUMENTS_OR_SCOPE");
    if(!io || !io->read || !io->context || !output || !uss_refill_target_valid(target) ||
        io->base<0x10000 || io->base>UINTPTR_MAX-0x3326d28 || io->context(io->user)!=target->epoch)return 0;
    r.io=io;
    USS_PICKUP_ACTION_STAGE("ACTION_MANAGER");
    if(!refill_read_pointer(&r,io->base+0x3326d20,&manager))return 0;
    USS_PICKUP_ACTION_STAGE("ACTION_COUNT_READ");
    if(!refill_read(&r,manager+0x70,&count,4))return 0;
    USS_PICKUP_ACTION_STAGE("ACTION_COUNT_RANGE");USS_PICKUP_ACTION_VALUE(count,4096);
    if(!count || count>4096)return 0;
    USS_PICKUP_ACTION_STAGE("ACTION_AVATAR_HASH");
    if(refill_hash_diagnose(&r,manager+0xf8,target->avatar,&index,diagnostic?&diagnostic->hash:NULL)!=1)return 0;
    USS_PICKUP_ACTION_STAGE("ACTION_INDEX_RANGE");USS_PICKUP_ACTION_VALUE(index,count);
    if(index>=count)return 0;
    USS_PICKUP_ACTION_STAGE("ACTION_UNIT_POINTER");
    if(!refill_read_pointer(&r,manager+0x110+(uintptr_t)index*8,&unit))return 0;
    USS_PICKUP_ACTION_STAGE("ACTION_UNIT_READ");
    if(!refill_read(&r,unit,descriptor,24))return 0;
    USS_PICKUP_ACTION_STAGE("ACTION_UNIT_OWNER");USS_PICKUP_ACTION_VALUE(refill_u32(descriptor+8),target->avatar);
    if(refill_u32(descriptor+8)!=target->avatar)return 0;
    found.row=manager+0x53dcd0+(uintptr_t)index*0x1238;
    USS_PICKUP_ACTION_STAGE("ACTION_ROW_READ");
    if(!refill_read(&r,found.row,&avatar,4))return 0;
    USS_PICKUP_ACTION_STAGE("ACTION_ROW_OWNER");USS_PICKUP_ACTION_VALUE(avatar,target->avatar);
    if(avatar!=target->avatar)return 0;
    USS_PICKUP_ACTION_STAGE("ACTION_COMPLETION_READ");
    if(!refill_read(&r,found.row+0x9ec,found.bytes,16))return 0;
    USS_PICKUP_ACTION_STAGE("ACTION_COMPLETION_OWNER");USS_PICKUP_ACTION_VALUE(refill_u32(found.bytes),target->avatar);
    if(refill_u32(found.bytes)!=target->avatar)return 0;
    USS_PICKUP_ACTION_STAGE("ACTION_MANAGER_RECHECK");
    if(!refill_read_pointer(&r,io->base+0x3326d20,&again) || again!=manager)return 0;
    USS_PICKUP_ACTION_STAGE("ACTION_COUNT_RECHECK");
    if(!refill_read(&r,manager+0x70,&again_count,4) || count!=again_count)return 0;
    USS_PICKUP_ACTION_STAGE("ACTION_HASH_RECHECK");
    if(refill_hash(&r,manager+0xf8,target->avatar,&again_index)!=1 || index!=again_index)return 0;
    USS_PICKUP_ACTION_STAGE("ACTION_UNIT_RECHECK");
    if(!refill_read(&r,unit,again_descriptor,24) || memcmp(descriptor,again_descriptor,24))return 0;
    USS_PICKUP_ACTION_STAGE("ACTION_COMPLETION_RECHECK");
    if(!refill_read(&r,found.row+0x9ec,again_bytes,16) || memcmp(found.bytes,again_bytes,16))return 0;
    USS_PICKUP_ACTION_STAGE("ACTION_INVENTORY_OWNER_RECHECK");
    if(!refill_read_pointer(&r,io->base+0x3326b90,&again) || again!=target->owner)return 0;
    USS_PICKUP_ACTION_STAGE("ACTION_EPOCH_RECHECK");
    if(io->context(io->user)!=target->epoch)return 0;
    found.manager=manager;found.unit=unit;found.avatar=target->avatar;found.epoch=target->epoch;
    found.entity=refill_u32(found.bytes+8);found.zone=refill_u32(found.bytes+12);found.valid=1;
    USS_PICKUP_ACTION_STAGE("ACTION_READY");*output=found;return 1;
#undef USS_PICKUP_ACTION_STAGE
#undef USS_PICKUP_ACTION_VALUE
}
static inline int uss_pickup_read_action(const UssRefillMemoryIo* io,const UssRefillTarget* target,UssPickupAction* output)
{return uss_pickup_read_action_diagnose(io,target,output,NULL);}
/* Selected cache captures all zones. Completion fallback proves only the
 * completed zone, so unrelated fields cannot invalidate its type evidence. */
static inline int uss_pickup_read_source_scope(const UssRefillMemoryIo* io,const UssRefillTarget* target,uint32_t entity,
    uint64_t now,uint32_t completed_zone,UssPickupSource* output,const char** reason)
{
    UssRefillMemory r={0};UssPickupSource source={0};uintptr_t manager,again,units,rows,unit,record_at;uint32_t index,again_index;
    unsigned char arrays[16],again_arrays[16],descriptor[24],again_descriptor[24],record[880]={0},again_record[880]={0};
    unsigned z,first=0,last=8,needs_scale=0,native_scale_verified=0;float native_scale=0;
#define USS_SOURCE_STAGE(name) do {if(reason)*reason=(name);}while(0)
    USS_SOURCE_STAGE("SOURCE_ARGUMENTS_OR_SCOPE");
    if(!io || !io->read || !io->context || !output || !uss_refill_target_valid(target) || !entity || entity==UINT32_MAX ||
        (completed_zone!=UINT32_MAX && completed_zone>=8) ||
        io->base<0x10000 || io->base>UINTPTR_MAX-0x3326d70 || io->context(io->user)!=target->epoch)return 0;
    r.io=io;
    USS_SOURCE_STAGE("SOURCE_MANAGER");
    if(!refill_read_pointer(&r,io->base+0x3326d68,&manager))return 0;
    USS_SOURCE_STAGE("SOURCE_ENTITY_HASH");
    if(refill_hash(&r,manager+0x14050,entity,&index)!=1 || index>=16384)return 0;
    USS_SOURCE_STAGE("SOURCE_ARRAYS");
    if(!refill_read(&r,manager+0x14068,arrays,16))return 0;
    units=refill_pointer(arrays);rows=refill_pointer(arrays+8);
    if(!units || !rows || units>UINTPTR_MAX-(uintptr_t)index*8 || rows>UINTPTR_MAX-(uintptr_t)index*880)return 0;
    USS_SOURCE_STAGE("SOURCE_UNIT");
    if(!refill_read_pointer(&r,units+(uintptr_t)index*8,&unit) || !refill_read(&r,unit,descriptor,24) || refill_u32(descriptor+8)!=entity)return 0;
    record_at=rows+(uintptr_t)index*880;
    USS_SOURCE_STAGE("SOURCE_TYPE_READ");
    if(completed_zone==UINT32_MAX){
        if(!refill_read(&r,record_at,record,440) || !refill_read(&r,record_at+440,record+440,440))return 0;
    }else{
        first=completed_zone;last=first+1;
        if(!refill_read(&r,record_at+(uintptr_t)first*0x68+0x44,record+first*0x68+0x44,4))return 0;
    }
    for(z=first;z<last;++z){source.kind[z]=refill_u32(record+z*0x68+0x44);if(source.kind[z]>=7 && source.kind[z]<=9)needs_scale=1;}
    if(needs_scale)native_scale_verified=(unsigned)uss_refill_ground_scale(io,entity,7,&native_scale);
    for(z=first;z<last;++z){
        if(source.kind[z]==4){source.scale[z]=0.5f;source.verified_zones|=1u<<z;}
        else if(source.kind[z]>=7 && source.kind[z]<=9 && native_scale_verified){source.scale[z]=native_scale;source.verified_zones|=1u<<z;}
    }
    USS_SOURCE_STAGE("SOURCE_OWNER_RECHECK");
    if(!refill_read_pointer(&r,io->base+0x3326d68,&again) || again!=manager ||
        !refill_read(&r,manager+0x14068,again_arrays,16) || memcmp(arrays,again_arrays,16) ||
        refill_hash(&r,manager+0x14050,entity,&again_index)!=1 || index!=again_index ||
        !refill_read(&r,unit,again_descriptor,24) || memcmp(descriptor,again_descriptor,24))return 0;
    USS_SOURCE_STAGE("SOURCE_TYPE_RECHECK");
    if(completed_zone==UINT32_MAX){
        if(!refill_read(&r,record_at,again_record,440) || !refill_read(&r,record_at+440,again_record+440,440) || memcmp(record,again_record,880))return 0;
    }else{
        unsigned at=first*0x68+0x44;
        if(!refill_read(&r,record_at+at,again_record+at,4) || memcmp(record+at,again_record+at,4))return 0;
    }
    USS_SOURCE_STAGE("SOURCE_CONTEXT_RECHECK");
    if(!refill_read_pointer(&r,io->base+0x3326b90,&again) || again!=target->owner || io->context(io->user)!=target->epoch)return 0;
    source.entity=entity;memcpy(source.identity,descriptor,8);source.time_ms=now;
    USS_SOURCE_STAGE("SOURCE_CLASSIFIED");*output=source;return 1;
#undef USS_SOURCE_STAGE
}
static inline int uss_pickup_read_source(const UssRefillMemoryIo* io,const UssRefillTarget* target,uint32_t entity,
    uint64_t now,UssPickupSource* output)
{return uss_pickup_read_source_scope(io,target,entity,now,UINT32_MAX,output,NULL);}
static inline void uss_pickup_completion_clear(UssPickupCompletions* state)
{uint64_t serial=state->serial;unsigned disabled=state->disabled;memset(state,0,sizeof(*state));state->serial=serial;state->disabled=disabled;}
static inline int uss_pickup_completion_bind(UssPickupCompletions* state,const UssRefillTarget* target)
{
    if(!state || state->disabled)return 0;
    if(!uss_refill_target_valid(target)){uss_pickup_completion_clear(state);return 0;}
    if(!state->bound || !uss_refill_target_same(&state->target,target,0)){
        uss_pickup_completion_clear(state);state->bound=1;}
    state->target=*target;return 1;
}
static inline int uss_pickup_cache_source(UssPickupCompletions* state,const UssPickupSource* source,uint64_t now)
{
    unsigned i,slot=USS_PICKUP_CACHE_CAP;static const unsigned char zero[8]={0};
    if(!state || !state->bound || state->disabled || !source || !source->entity || source->entity==UINT32_MAX ||
        !memcmp(source->identity,zero,8) || source->time_ms>now || now-source->time_ms>1500 || source->consumed || source->verified_zones>255)return 0;
    for(i=0;i<USS_PICKUP_CACHE_CAP;++i){
        UssPickupSource* entry=&state->cache[i];
        if(entry->entity==source->entity){slot=i;break;}
        if(!entry->entity || entry->time_ms>now || now-entry->time_ms>1500){if(slot==USS_PICKUP_CACHE_CAP)slot=i;}
    }
    if(slot==USS_PICKUP_CACHE_CAP){memset(state->cache,0,sizeof(state->cache));slot=0;}
    state->cache[slot]=*source;return 1; /* Selection cannot publish a completion. */
}
static inline int uss_pickup_publish_completion(UssPickupCompletions* state,const UssPickupAction* action,uint64_t now,
    const UssRefillMemoryIo* memory,UssRefillEvidence* output)
{
    UssPickupSource live,*source=NULL;UssRefillEvidence evidence={0};unsigned i;
    if(!state || !state->bound || state->disabled || !action || !output || !action->valid ||
        action->epoch!=state->target.epoch || action->avatar!=state->target.avatar){if(state)uss_pickup_completion_clear(state);return 0;}
    if(state->have_previous && state->previous_entity==action->entity && state->previous_zone==action->zone){
        state->reason="PICKUP_COMPLETION_UNCHANGED";return 0;
    }
    {unsigned previous=state->have_previous;state->have_previous=1;state->previous_entity=action->entity;state->previous_zone=action->zone;
        if(!previous || !action->entity || action->entity==UINT32_MAX){state->reason="PICKUP_COMPLETION_BASELINE";return 0;}}
    state->activity_ms=now;state->activity_valid=1; /* Unknown completion still excludes pack inference. */
    state->reason="PICKUP_COMPLETION_ZONE";
    if(action->zone>=8)return 0;
    for(i=0;i<USS_PICKUP_CACHE_CAP;++i)if(state->cache[i].entity==action->entity){source=&state->cache[i];break;}
    if(!source || source->consumed || source->time_ms>now || now-source->time_ms>1500 || !(source->verified_zones&(1u<<action->zone))){
        UssPickupAction fresh;
        state->reason="PICKUP_COMPLETION_SOURCE_UNAVAILABLE";
        if(!memory || !uss_pickup_read_source_scope(memory,&state->target,action->entity,now,action->zone,&live,&state->source_reason))return 0;
        state->source_reason="SOURCE_COMPLETION_RECHECK";
        if(!uss_pickup_read_action(memory,&state->target,&fresh) || fresh.manager!=action->manager || fresh.unit!=action->unit ||
            fresh.row!=action->row || memcmp(fresh.bytes,action->bytes,16))return 0;
        state->source_reason="SOURCE_COMPLETION_VERIFIED";
        source=&live;}
    state->reason="PICKUP_COMPLETION_SOURCE_POLICY";
    if(!(source->verified_zones&(1u<<action->zone)) || !uss_refill_scale_valid(source->scale[action->zone]))return 0;
    if(state->serial==UINT64_MAX){state->disabled=1;return 0;}
    source->consumed=1; /* Spend entire source bundle, not one sibling zone. */
    evidence.target=state->target;evidence.serial=++state->serial;evidence.time_ms=now;evidence.policy_time_ms=source->time_ms;
    evidence.route=USS_REFILL_GROUND;evidence.source=source->entity;evidence.zone=action->zone;evidence.source_kind=source->kind[action->zone];
    memcpy(evidence.source_identity,source->identity,8);evidence.source_scale=source->scale[action->zone];
    evidence.completed=evidence.source_verified=evidence.policy_verified=1;state->reason="PICKUP_COMPLETION_PUBLISHED";*output=evidence;return 1;
}

typedef struct UssSelectedPickup {uint32_t entity,zone,index;uintptr_t winner;} UssSelectedPickup;
static inline int uss_pickup_read_selected(const UssRefillMemoryIo* io,const UssRefillTarget* target,UssSelectedPickup* output)
{
    UssRefillMemory r={0};UssSelectedPickup found={0};uintptr_t manager,again,units,rows,unit,winner,interaction;
    uint32_t index,again_index,count,again_count;unsigned char arrays[16],again_arrays[16],descriptor[24],again_descriptor[24],spatial[20],again_spatial[20],source[8],again_source[8];
    if(!io || !io->read || !io->context || !output || !uss_refill_target_valid(target) ||
        io->base<0x10000 || io->base>UINTPTR_MAX-0x3326d70 || io->context(io->user)!=target->epoch)return 0;
    r.io=io;
    if(!refill_read_pointer(&r,io->base+0x33269a0,&manager) || !refill_read(&r,manager+0x1c,&count,4) || !count || count>4096 ||
        refill_hash(&r,manager+0x28,target->avatar,&index)!=1 || index>=count || !refill_read(&r,manager+0x40,arrays,16))return 0;
    units=refill_pointer(arrays);rows=refill_pointer(arrays+8);
    if(!units || !rows || units>UINTPTR_MAX-(uintptr_t)index*8 || rows>UINTPTR_MAX-(uintptr_t)index*0x880 ||
        !refill_read_pointer(&r,units+(uintptr_t)index*8,&unit) || !refill_read(&r,unit,descriptor,24) || refill_u32(descriptor+8)!=target->avatar ||
        !refill_read_pointer_alignment(&r,rows+(uintptr_t)index*0x880,&winner,4) || !refill_read(&r,winner,spatial,20))return 0;
    found.index=refill_u32(spatial+16);if(found.index>=2048)return 0;
    if(!refill_read_pointer(&r,io->base+0x3326d68,&interaction) || !refill_read(&r,interaction+0xe000+(uintptr_t)found.index*12,source,8))return 0;
    found.entity=refill_u32(source);found.zone=refill_u32(source+4);found.winner=winner;
    if(!found.entity || found.entity==UINT32_MAX || found.zone>=8 ||
        !refill_read_pointer(&r,io->base+0x33269a0,&again) || again!=manager || !refill_read(&r,manager+0x1c,&again_count,4) || count!=again_count ||
        !refill_read(&r,manager+0x40,again_arrays,16) || memcmp(arrays,again_arrays,16) ||
        !refill_read_pointer(&r,units+(uintptr_t)index*8,&again) || again!=unit || !refill_read(&r,unit,again_descriptor,24) || memcmp(descriptor,again_descriptor,24) ||
        refill_hash(&r,manager+0x28,target->avatar,&again_index)!=1 || index!=again_index ||
        !refill_read_pointer_alignment(&r,rows+(uintptr_t)index*0x880,&again,4) || again!=winner || !refill_read(&r,winner,again_spatial,20) || memcmp(spatial,again_spatial,20) ||
        !refill_read_pointer(&r,io->base+0x3326d68,&again) || again!=interaction ||
        !refill_read(&r,interaction+0xe000+(uintptr_t)found.index*12,again_source,8) || memcmp(source,again_source,8) ||
        !refill_read_pointer(&r,io->base+0x3326b90,&again) || again!=target->owner || io->context(io->user)!=target->epoch)return 0;
    *output=found;return 1;
}
/* Automatic 25ms classifier poll while an eligible target exists; source refresh
 * is 250ms unless the exact selected token changes. No nearby-source scans. */
static inline int uss_pickup_completion_poll(UssPickupCompletions* state,const UssRefillMemoryIo* io,
    const UssRefillTarget* target,uint64_t now,UssRefillEvidence* output)
{
    UssPickupAction action;UssPickupActionDiagnostic diagnostic={0};UssSelectedPickup selected;UssPickupSource source;unsigned published,i,spent=0;
    if(!state || !output || !uss_pickup_completion_bind(state,target))return 0;
    if(now>UINT64_MAX-250){state->disabled=1;return 0;}
    if(now<state->next_poll)return 0;state->next_poll=now+25;
    if(!uss_pickup_read_action_diagnose(io,target,&action,&diagnostic)){
        uss_pickup_completion_clear(state);state->action_diagnostic=diagnostic;state->reason="PICKUP_ACTION_UNAVAILABLE";return 0;
    }
    state->action_diagnostic=diagnostic;
    published=(unsigned)uss_pickup_publish_completion(state,&action,now,io,output);
    if(!uss_pickup_read_selected(io,target,&selected)){state->selected_entity=0;state->selected_winner=0;state->selected_source_reason="SELECTED_PROMPT_UNAVAILABLE";return (int)published;}
    if(selected.entity==state->selected_entity && selected.zone==state->selected_zone && selected.winner==state->selected_winner && now<state->next_source)return (int)published;
    state->selected_entity=selected.entity;state->selected_zone=selected.zone;state->selected_winner=selected.winner;state->next_source=now+250;
    for(i=0;i<USS_PICKUP_CACHE_CAP;++i)if(state->cache[i].entity==selected.entity && state->cache[i].consumed)spent=1;
    if(spent && state->previous_entity==selected.entity && state->previous_zone==selected.zone)return (int)published;
    if(uss_pickup_read_source_scope(io,target,selected.entity,now,UINT32_MAX,&source,&state->selected_source_reason)){
        UssSelectedPickup after;
        if(uss_pickup_read_selected(io,target,&after) && after.entity==selected.entity && after.zone==selected.zone &&
            after.winner==selected.winner && after.index==selected.index){
            uss_pickup_cache_source(state,&source,now);state->selected_source_reason="SELECTED_SOURCE_CACHED";
        }else state->selected_source_reason="SELECTED_PROMPT_CHANGED";
    }
    return (int)published;
}
static inline int uss_pickup_activity_near(const UssPickupCompletions* state,uint64_t now)
{return state && state->bound && state->activity_valid && now>=state->activity_ms && now-state->activity_ms<=500;}
#endif
