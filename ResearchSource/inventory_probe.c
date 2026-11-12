#include "inventory_reader.h"
#ifndef HF_USS_INVENTORY_ENABLED
#define HF_USS_INVENTORY_ENABLED 0
#endif
static const HfApi* sdk;
static atomic_uint attempt;
static void spawned(HfMod* mod,const HfEventInfo* event,void* user)
{
    HfPlayerEvent player;
    HfSession before={0},after={0};
    Inventory result;
    Reader reader;
    unsigned expected=1;
    int ok;
    uint32_t state;
    unsigned present=0,k;
    (void)user;
    if(!event || event->kind!=HF_ON_PLAYER_SPAWN || event->size<sizeof(player)) return;
    memcpy(&player,event,sizeof(player));
    if(player.local!=1) return;
    if(!atomic_compare_exchange_strong_explicit(&attempt,&expected,0,memory_order_acquire,memory_order_relaxed)) return;
    memset(&reader,0,sizeof(reader)); reader.api=sdk; reader.slot=-1;
    state=sdk->game_state(mod);
    sdk->session(mod,&before);
    if(before.role!=HF_SESSION_SOLO) { sdk->log(mod,"HF019 REFUSED session role"); return; }
    ok=inventory_sample(&reader,&result);
    sdk->session(mod,&after);
    if(after.role!=before.role || after.version!=before.version) { ok=0; reader.failure="SESSION_CHANGED"; }
    if(ok) for(k=0;k<5;++k) if(valid_id(k<4?result.slots[k]:result.held)) present|=1u<<k;
    if(ok) sdk->log(mod,"HF019 OBSERVED category=UNCLASSIFIED gameplay_approved=0 scope=LOCAL_INVENTORY event_binding=UNVERIFIED avatar=%u slots=%u,%u,%u,%u active_selector=%u held=%u kinds=%u,%u,%u,%u,%u present_mask=%u absent_kinds=NA event_entity=%u local=%u state=%u update=%llu reads=%u bytes=%u",
        result.avatar,result.slots[0],result.slots[1],result.slots[2],result.slots[3],result.active_selector,result.held,
        result.observed_kinds[0],result.observed_kinds[1],result.observed_kinds[2],result.observed_kinds[3],result.observed_kinds[4],
        present,player.entity,player.local,state,(unsigned long long)player.info.update,reader.calls,reader.bytes);
    else sdk->log(mod,"HF019 REFUSED %s stage=%s pass=%u slot=%d predicate_valid=%u expected=%u observed=%u kind=%u category=UNCLASSIFIED gameplay_approved=0 scope=LOCAL_INVENTORY event_binding=UNVERIFIED event_entity=%u local=%u state=%u update=%llu reads=%u bytes=%u partial=1 final_valid=%u final_reasons=%u diff_mask=%u units=%u,%u avatars=%u,%u session_before=%u,%u session_after=%u,%u descriptor_u12=%u,%u descriptor_u16=%u,%u descriptor_u20=%u,%u",
        reader.failure?reader.failure:"UNKNOWN",reader.stage?reader.stage:"SESSION",reader.pass,reader.slot,
        reader.predicate_valid,reader.expected_entity,reader.observed_entity,reader.observed_kind,
        player.entity,player.local,state,(unsigned long long)player.info.update,reader.calls,reader.bytes,
        reader.final_valid,reader.final_reasons,reader.difference_mask,
        reader.first_unit,reader.second_unit,reader.first_avatar,reader.second_avatar,
        before.role,before.version,after.role,after.version,
        reader.descriptor_u12[0],reader.descriptor_u12[1],
        reader.descriptor_u16[0],reader.descriptor_u16[1],
        reader.descriptor_u20[0],reader.descriptor_u20[1]);
}
HF_MOD_EXPORT int hellforge_mod_init(const HfApi* api,HfMod* mod)
{
    const char* build;
    if(!api || !mod || !uss_sdk_compatible(api) || !api->log) return 1;
    if(!HF_USS_INVENTORY_ENABLED) { api->log(mod,"HF019 inactive"); return 0; }
    if(!api->read || !api->module_base || !api->session || !api->subscribe || !api->build_id || !api->game_state) return 1;
    build=api->build_id();
    if(!build || strcmp(build,"25480438")) return 1;
    sdk=api;
    atomic_store_explicit(&attempt,1,memory_order_release);
    if(!api->subscribe(mod,HF_ON_PLAYER_SPAWN,spawned,NULL,0)) {
        atomic_store_explicit(&attempt,0,memory_order_release);
        api->log(mod,"HF019 stopped registration failure; staying loaded/no retry");
    }
    return 0;
}
