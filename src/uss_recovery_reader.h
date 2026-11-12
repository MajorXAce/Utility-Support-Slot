/* Recovery-only bounded reader. Unlike ordinary Core admission, this can
 * describe an empty native support slot or vanilla's empty death inventory. */
#ifndef USS_RECOVERY_READER_H
#define USS_RECOVERY_READER_H
#include "uss_recovery.h"
typedef struct UssRecoveryReader {
    const HfApi* api;HfMod* mod;UssCorePlatform platform;const UssCoreFlags* policy;
} UssRecoveryReader;
static int recovery_member(Reader* memory,uintptr_t base,const Inventory* inventory,const UssPairMember* member,
    UssMemberEvidence* output,unsigned* absent)
{
    UssCaptureReader lookup_reader={0};uintptr_t manager,again_manager;uint32_t index=0;int found;
    lookup_reader.memory=*memory;*absent=0;
    if(!get_ptr(&lookup_reader.memory,base+0x3326dc0,&manager) || manager!=inventory->roots[4])goto unknown;
    found=capture_lookup(&lookup_reader,manager+0x20,member->entity,&index);
    if(found<0 || lookup_reader.memory.failure)goto unknown;
    if(!get_ptr(&lookup_reader.memory,base+0x3326dc0,&again_manager) || manager!=again_manager)goto unknown;
    *memory=lookup_reader.memory;
    if(!found){memset(output,0,sizeof(*output));*absent=1;return 1;}
    return member_fields(memory,base,inventory,member->entity,member->identity,output);
unknown:
    *memory=lookup_reader.memory;return 0;
}
static inline int recovery_snapshot_context(const UssRecoveryWork* work,const UssCoreScope* scope,const Inventory* inventory)
{
    UssPairOwnerContext context=core_context(inventory,scope);UssPairOwnerContext expected=work->owner.context;
    if(!publication_id(inventory->slots[0]) || !publication_id(inventory->slots[1])){
        /* Only standard/backpack fields may change during this avatar's native
         * death release. Ownership, row, roster and session still match exactly. */
        expected.local.primary=context.local.primary;expected.local.secondary=context.local.secondary;
        expected.local.backpack=context.local.backpack;
    }
    return pair_owner_context_equal(&context,&expected);
}
static inline int uss_recovery_observe(const UssRecoveryReader* source,const UssRecoveryWork* work,UssRecoverySnapshot* output)
{
    Reader memory={0};UssRecoverySnapshot result={0},again={0};UssCoreScope before={0},after={0};
    Inventory final_inventory;unsigned survivor_absent=0,again_absent=0;const char* build;unsigned char pin[32];
    static const unsigned char member_pin[32]={0x40,0x53,0x41,0x54,0x41,0x57,0x48,0x83,0xec,0x20,0x3b,0x15,0x50,0xb1,0xb0,0x02,0x45,0x0f,0xb6,0xf9,0x48,0x8b,0x1d,0x8d,0xe2,0x9a,0x02,0x45,0x8b,0xe0,0x0f,0x84};
    if(output)memset(output,0,sizeof(*output));
    if(!source || !work || !output || !uss_sdk_compatible(source->api) || !source->mod ||
        !source->api->read || !source->api->build_id || !source->api->module_base || !source->api->session || !source->api->game_state ||
        !source->platform.thread_id || source->platform.thread_id(source->platform.user)!=work->owner.thread ||
        !source->policy || !source->policy->known || !source->policy->eligible || source->policy->disabled ||
        source->policy->suspended || source->policy->paused || source->policy->input)return 0;
    build=source->api->build_id();if(!build || strcmp(build,"25480438"))return 0;
    memory.api=source->api;memory.slot=-1;
    if(!core_scope(&memory,source->api,source->mod,&before) || !before.mission ||
        !inventory_sample(&memory,&result.inventory) || !recovery_snapshot_context(work,&before,&result.inventory) ||
        !get(&memory,before.game+0x978ac0,pin,32) || memcmp(pin,member_pin,32))return 0;
    result.death=!publication_id(result.inventory.slots[0]) || !publication_id(result.inventory.slots[1]);
    result.all_empty=result.inventory.slots[0]==0 && result.inventory.slots[1]==0 && result.inventory.slots[2]==0 && result.inventory.slots[3]==0;
    if(!result.death && result.inventory.slots[2]!=0 && result.inventory.slots[2]!=work->survivor.entity)return 0;
    if(!recovery_member(&memory,before.game,&result.inventory,&work->survivor,&result.survivor,&survivor_absent) || survivor_absent ||
        !recovery_member(&memory,before.game,&result.inventory,&work->outgoing,&result.outgoing,&result.outgoing_absent))return 0;
    if(!result.death){
        if(!primary_fields(&memory,before.game,&result.inventory,&result.primary.fields))return 0;
        primary_predicate(&result.primary,&result.inventory);result.primary.valid=1;
    }
    if(!inventory_sample(&memory,&final_inventory) || memcmp(&result.inventory,&final_inventory,sizeof(final_inventory)) ||
        !recovery_member(&memory,before.game,&final_inventory,&work->survivor,&again.survivor,&again_absent) || again_absent ||
        memcmp(&result.survivor,&again.survivor,sizeof(result.survivor)) ||
        !recovery_member(&memory,before.game,&final_inventory,&work->outgoing,&again.outgoing,&again.outgoing_absent) ||
        result.outgoing_absent!=again.outgoing_absent || memcmp(&result.outgoing,&again.outgoing,sizeof(result.outgoing)) ||
        !core_scope(&memory,source->api,source->mod,&after) || memcmp(&before,&after,sizeof(before)) ||
        source->platform.thread_id(source->platform.user)!=work->owner.thread)return 0;
    result.valid=1;*output=result;return 1;
}
#endif
