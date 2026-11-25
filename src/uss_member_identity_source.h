/* Read-only positive identity corroboration. Cached identity is not attachment,
 * selection or destruction authority. Missing sources remain UNKNOWN. */
#ifndef USS_MEMBER_IDENTITY_SOURCE_H
#define USS_MEMBER_IDENTITY_SOURCE_H
#include "core_observation.h"

enum {USS_IDENTITY_UNKNOWN=0,USS_IDENTITY_PRESENT=1,USS_IDENTITY_ABSENT=2};
/* ABSENT is reserved for a complete qualified feed. This bounded positive
 * provider never returns it: an uncaptured owner-array suffix is unknown. */
enum {USS_IDENTITY_OWNER_ROWS=32}; /* acquisition budget, not an engine limit */
typedef struct UssIdentityBudget {unsigned calls,bytes;} UssIdentityBudget;
typedef struct UssIdentityEntry {
    UssPairMember member;uintptr_t direct,row;unsigned direct_valid,row_valid;
    unsigned char header[24];
} UssIdentityEntry;
typedef struct UssIdentityCache {
    unsigned valid,owner_prefix_complete;UssPairOwner owner;
    const HfApi* api;HfMod* mod;uintptr_t game,units,owner_manager;size_t game_size;
    UssCorePlatform platform;UssIdentityEntry entries[2];
    UssIdentityBudget seed_budget,query_budget;unsigned seeds;
} UssIdentityCache;
typedef struct UssIdentityResult {
    unsigned status,direct_present,row_present,direct_unknown,row_unknown;
    UssIdentityBudget budget;
} UssIdentityResult;
static inline void uss_identity_forget(UssIdentityCache* cache)
{if(cache)memset(cache,0,sizeof(*cache));}
static inline int identity_owner_matches(const UssPairOwner* a,const UssPairOwner* b)
{
    return pair_owner_ready(a,a?a->thread:0) && pair_owner_ready(b,b?b->thread:0) &&
        a->thread==b->thread && a->epoch==b->epoch && a->context_generation==b->context_generation &&
        a->pair_generation==b->pair_generation && a->guard_generation==b->guard_generation &&
        pair_owner_context_equal(&a->context,&b->context) &&
        pair_member_equal(&a->utility,&b->utility) && pair_member_equal(&a->ordinary,&b->ordinary);
}
static inline int identity_core_current(const UssPairOwner* owner,const UssCoreObservation* current)
{
    return owner && current && pair_owner_ready(owner,owner->thread) && current->valid && current->mission &&
        current->pair_matched && current->expected_pair_present && current->thread==owner->thread &&
        current->expected_epoch==owner->epoch && current->expected_context==owner->context_generation &&
        current->expected_pair==owner->pair_generation && current->expected_guard==owner->guard_generation &&
        pair_owner_context_equal(&owner->context,&current->context) &&
        current->flags.known && current->flags.eligible && !current->flags.disabled && !current->flags.suspended &&
        !current->flags.paused && !current->flags.input;
}
static inline int identity_member_source(const UssPairMember* member,const UssMemberEvidence* seen,const Inventory* inventory)
{
    return member && seen && inventory && seen->status==USS_MEMBER_OBSERVED && seen->entity==member->entity &&
        seen->parent==inventory->avatar && seen->weapon_manager==inventory->roots[4] &&
        address_ok(seen->weapon_unit,24) && u32(seen->descriptor+8)==member->entity &&
        !memcmp(seen->descriptor,member->identity,8);
}
/* Called only with a fully accepted current pair observation. Direct sources
 * are copied, not rediscovered. One bounded owner-array prefix may also supply
 * exact rows; an unreadable/missed row is never evidence of absence. */
static inline int uss_identity_seed(UssIdentityCache* cache,const HfApi* api,HfMod* mod,
    const UssCorePlatform* platform,const UssPairOwner* owner,const UssCoreObservation* current)
{
    UssIdentityCache next={0};Reader memory={0};uintptr_t before=0,after=0;size_t size=0;unsigned i,j,found=0;
    unsigned char row[12],again[12];const UssMemberEvidence* seen[2];const char* build;
    if(!cache)return 0;cache->seed_budget=(UssIdentityBudget){0};
    if(!owner || !owner->pair_present){uss_identity_forget(cache);return 0;}
    if(cache->valid && !identity_owner_matches(&cache->owner,owner))uss_identity_forget(cache);
    if(!identity_core_current(owner,current) || !platform || !platform->thread_id ||
        platform->thread_id(platform->user)!=owner->thread || !uss_sdk_compatible(api) || !api->read ||
        !api->module_base || !api->build_id || !mod || owner->utility.entity==owner->ordinary.entity ||
        !identity_member_source(&owner->utility,&current->utility,&current->inventory) ||
        !identity_member_source(&owner->ordinary,&current->ordinary,&current->inventory))return 0;
    if(cache->valid && cache->api==api && cache->mod==mod && identity_owner_matches(&cache->owner,owner) &&
        cache->units==current->inventory.roots[4] && cache->owner_manager==current->inventory.roots[0] &&
        cache->platform.thread_id==platform->thread_id && cache->platform.user==platform->user){
        /* This invocation already qualified both current member observations.
         * Refresh direct evidence even if the association is unchanged: the
         * manager may now supply another unit or updated descriptor bytes.
         * Preserve owner-row evidence independently; no prefix scan or read. */
        seen[0]=&current->utility;seen[1]=&current->ordinary;
        for(i=0;i<2;++i){
            if(!cache->entries[i].direct_valid || cache->entries[i].direct!=seen[i]->weapon_unit ||
                memcmp(cache->entries[i].header,seen[i]->descriptor,24)){
                cache->entries[i].direct=seen[i]->weapon_unit;
                memcpy(cache->entries[i].header,seen[i]->descriptor,24);
                cache->entries[i].direct_valid=1;
            }
        }
        return 1;
    }
    /* A source provider change is a new qualified acquisition, not pointer reuse. */
    build=api->build_id();if(!build || strcmp(build,"25480438"))return 0;
    next.game=api->module_base("game.dll",&size);next.game_size=size;
    if(!next.game || size<0x3484570 || size>0x20000000 || !address_ok(next.game,size))return 0;
    next.valid=1;next.owner=*owner;next.api=api;next.mod=mod;next.platform=*platform;
    next.units=current->inventory.roots[4];next.owner_manager=current->inventory.roots[0];next.seeds=cache->seeds+1;
    seen[0]=&current->utility;seen[1]=&current->ordinary;
    next.entries[0].member=owner->utility;next.entries[1].member=owner->ordinary;
    for(i=0;i<2;++i){next.entries[i].direct=seen[i]->weapon_unit;next.entries[i].direct_valid=1;memcpy(next.entries[i].header,seen[i]->descriptor,24);}
    memory.api=api;memory.slot=-1;
    if(address_ok(current->inventory.owner_records,USS_IDENTITY_OWNER_ROWS*24) &&
        get_ptr(&memory,next.game+0x3326b90,&before) && before==next.owner_manager){
        for(i=0;i<USS_IDENTITY_OWNER_ROWS;++i){
            uintptr_t address=current->inventory.owner_records+(uintptr_t)i*24;
            if(!get(&memory,address,row,12))break;
            for(j=0;j<2;++j)if(!next.entries[j].row_valid && u32(row+8)==next.entries[j].member.entity &&
                !memcmp(row,next.entries[j].member.identity,8) && get(&memory,address,again,12) && !memcmp(row,again,12)){
                next.entries[j].row=address;next.entries[j].row_valid=1;++found;
            }
            if(found==2){next.owner_prefix_complete=1;break;}
        }
        if(i==USS_IDENTITY_OWNER_ROWS)next.owner_prefix_complete=1;
    }
    /* Owner rows are committed only with a stable root. Direct evidence came
     * from the accepted observation and remains a separate source. */
    if(!get_ptr(&memory,next.game+0x3326b90,&after) || before!=after || after!=next.owner_manager)
        for(j=0;j<2;++j){next.entries[j].row_valid=0;next.entries[j].row=0;}
    next.seed_budget=(UssIdentityBudget){memory.calls,memory.bytes};
    if(!identity_core_current(owner,current) || !identity_owner_matches(&next.owner,owner) ||
        platform->thread_id(platform->user)!=owner->thread){uss_identity_forget(cache);return 0;}
    *cache=next;return 1;
}
/* The caller supplies an already coherent current inventory/owner. Fresh reads
 * bracket retained source bytes. Positive means exact identity corroboration,
 * not carried/held eligibility; source drift and incomplete feeds stay UNKNOWN. */
static inline UssIdentityResult uss_identity_query(UssIdentityCache* cache,const HfApi* api,HfMod* mod,
    const UssCorePlatform* platform,const UssPairOwner* owner,const Inventory* current,const UssPairMember* member)
{
    UssIdentityResult result={0};Reader memory={0};UssIdentityEntry* entry=NULL;
    uintptr_t owner1=0,owner2=0,units1=0,units2=0,game;size_t size=0;unsigned i;
    unsigned char header1[24],header2[24],row1[12],row2[12];int direct=0,row=0,owner_ok,units_ok;const char* build;
    if(!cache)return result;cache->query_budget=(UssIdentityBudget){0};
    if(!owner || !owner->pair_present){uss_identity_forget(cache);return result;}
    if(cache->valid && (!identity_owner_matches(&cache->owner,owner) || cache->api!=api || cache->mod!=mod)){uss_identity_forget(cache);return result;}
    if(!uss_sdk_compatible(api) || !api->read || !api->build_id || !api->module_base)return result;
    if(!cache->valid || !identity_owner_matches(&cache->owner,owner) || !current || !member ||
        cache->api!=api || cache->mod!=mod || !platform || platform->thread_id!=cache->platform.thread_id ||
        platform->user!=cache->platform.user || !platform->thread_id || platform->thread_id(platform->user)!=owner->thread ||
        current->avatar!=owner->context.local.avatar || current->roots[0]!=cache->owner_manager){uss_identity_forget(cache);return result;}
    for(i=0;i<2;++i)if(pair_member_equal(&cache->entries[i].member,member))entry=&cache->entries[i];
    if(!entry)return result;
    build=api->build_id();game=api->module_base("game.dll",&size);
    if(!build || strcmp(build,"25480438") || game!=cache->game || size!=cache->game_size){uss_identity_forget(cache);return result;}
    memory.api=api;memory.slot=-1;
    owner_ok=get_ptr(&memory,game+0x3326b90,&owner1)&&owner1==cache->owner_manager;
    units_ok=get_ptr(&memory,game+0x3326dc0,&units1)&&units1==cache->units&&units1==current->roots[4];
    if(owner_ok && units_ok && entry->direct_valid && get(&memory,entry->direct,header1,24) &&
        get(&memory,entry->direct,header2,24) && !memcmp(header1,header2,24)){
        direct=!memcmp(header1,entry->header,24) && u32(header1+8)==member->entity && !memcmp(header1,member->identity,8);
        if(!direct)entry->direct_valid=0;
    }
    if(owner_ok && address_ok(current->owner_records,USS_IDENTITY_OWNER_ROWS*24) &&
        entry->row>=current->owner_records && entry->row-current->owner_records<USS_IDENTITY_OWNER_ROWS*24 &&
        (entry->row-current->owner_records)%24==0 && entry->row_valid && get(&memory,entry->row,row1,12) && get(&memory,entry->row,row2,12) && !memcmp(row1,row2,12)){
        row=u32(row1+8)==member->entity && !memcmp(row1,member->identity,8);
        if(!row)entry->row_valid=0;
    }
    owner_ok=get_ptr(&memory,game+0x3326b90,&owner2)&&owner1==owner2&&owner2==cache->owner_manager&&owner_ok;
    units_ok=get_ptr(&memory,game+0x3326dc0,&units2)&&units1==units2&&units2==cache->units&&units_ok;
    result.budget=(UssIdentityBudget){memory.calls,memory.bytes};cache->query_budget=result.budget;
    if(!identity_owner_matches(&cache->owner,owner) || platform->thread_id(platform->user)!=owner->thread)return result;
    if(!owner_ok){uss_identity_forget(cache);return result;}
    if(!units_ok)entry->direct_valid=0;
    result.direct_present=owner_ok&&units_ok&&direct;result.row_present=owner_ok&&row;
    result.direct_unknown=!result.direct_present;result.row_unknown=!result.row_present;
    result.status=(result.direct_present||result.row_present)?USS_IDENTITY_PRESENT:USS_IDENTITY_UNKNOWN;
    return result;
}
#endif
