/* Read-only ordinary formation/draw readiness; not native send permission. */
#ifndef USS_PRIMARY_READINESS_H
#define USS_PRIMARY_READINESS_H
#include "action_state.h"

typedef struct UssPrimaryFields {
    UssActionState action;
    uintptr_t game_state;
    unsigned char row[32],flags[24],state_byte;
    uint32_t global_guard;
} UssPrimaryFields;
typedef struct UssPrimaryReadiness {
    unsigned valid, ready, settled, holstering, bit47;
    UssPrimaryFields fields;
} UssPrimaryReadiness;

/* Same unsigned timer-bit comparison as Lua, not a float conversion. */
static void primary_predicate(UssPrimaryReadiness* result,const Inventory* owner)
{
    const unsigned char* row=result->fields.row;
    uint32_t phase=u32(row+16),timer=u32(row+24),prior=u32(row+8);
    result->bit47=(u32(result->fields.flags+12)>>15)&1;
    result->settled=phase==0 && timer==0 && result->bit47==0;
    result->holstering=phase==1 && (prior==2 || prior==3) && u32(row+20)==1 &&
                       result->bit47==1 && timer>0 && timer<0x7F800000;
    result->ready=valid_id(owner->slots[0]) && u32(row)==owner->avatar && u32(row+4)==1 &&
                  owner->active_selector==1 && owner->held==owner->slots[0] &&
                  u32(row+28)==0 && result->fields.global_guard==0 &&
                  result->fields.state_byte==0 && (result->settled || result->holstering);
}
static int primary_fields(Reader* reader,uintptr_t base,const Inventory* owner,
                          UssPrimaryFields* result)
{
    memset(result,0,sizeof(*result));
    if (!action_record(reader,base+0x3326D20,owner->avatar,&result->action)) return 0;
    reader->stage="PRIMARY_FIELDS";
    if (!get(reader,result->action.record,result->row,32) ||
        memcmp(result->row,result->action.row,24) ||
        !get(reader,result->action.manager+0x53E880+(uintptr_t)result->action.index*0x1238,
             result->flags,24) ||
        !get_ptr(reader,base+0x3326340,&result->game_state) ||
        !get(reader,result->game_state+0xAC218,&result->global_guard,4) ||
        !get(reader,result->action.manager+0x53EAE0+(uintptr_t)result->action.index*0x1238,
             &result->state_byte,1)) return 0;
    return 1;
}
static inline UssPrimaryReadiness uss_observe_primary_readiness(const HfApi* api,HfMod* mod,
    const Inventory* expected_owner,Reader* diagnostics)
{
    UssPrimaryReadiness result={0};
    UssPrimaryFields first={0},second={0};
    Inventory before,after;
    Reader reader={0};
    HfSession start={0},end={0};
    uintptr_t base=0,end_base=0;
    size_t size=0,end_size=0;
    const char* build;
    unsigned char action_pin[7],state_pin[30];
    static const unsigned char expected_action[7]={0x4c,0x8b,0x35,0x4c,0x2e,0x89,0x02};
    /* Lua settings_owner pin144024, retained mapped image, resolves3326340. */
    static const unsigned char expected_state[30]={
        0x48,0x8b,0x0d,0x15,0x23,0x1e,0x03,0x48,0xbf,0xf3,0x9d,0xe1,0x38,0x8c,0x41,
        0xe2,0x12,0x48,0x81,0xc1,0x58,0xfa,0x01,0x00,0x48,0x8b,0xd7,0x41,0xb1,0x01};
    if (diagnostics) {reader.calls=diagnostics->calls;reader.bytes=diagnostics->bytes;}
    reader.api=api;reader.slot=-1;
    if (reader.calls>2165 || reader.bytes>20000) {reader.failure="READ_BOUNDS";goto done;}
    if (!api || !mod || !expected_owner || !uss_sdk_compatible(api) ||
        !api->read || !api->module_base || !api->session || !api->build_id) {
        reader.failure="PRIMARY_CONTRACT";goto done;
    }
    build=api->build_id();
    if (!build || strcmp(build,"25480438")) {reader.failure="BUILD_CONTRACT";goto done;}
    api->session(mod,&start);
    if (start.role!=HF_SESSION_SOLO) {reader.failure="SESSION_ROLE";goto done;}
    base=api->module_base("game.dll",&size);
    reader.stage="PRIMARY_PINS";
    if (!base || size<0x3326DC8 || size>0x20000000 || !address_ok(base,size) ||
        !get(&reader,base+0xA93ECD,action_pin,7) || memcmp(action_pin,expected_action,7) ||
        (int64_t)0xA93ECD+7+(int32_t)u32(action_pin+3)!=0x3326D20 ||
        !get(&reader,base+0x144024,state_pin,30) || memcmp(state_pin,expected_state,30) ||
        (int64_t)0x144024+7+(int32_t)u32(state_pin+3)!=0x3326340) {
        if (!reader.failure) reader.failure="PRIMARY_PINS";
        goto done;
    }
    if (!inventory_sample(&reader,&before) || memcmp(expected_owner,&before,sizeof(before)) ||
        !primary_fields(&reader,base,&before,&first) ||
        !inventory_sample(&reader,&after) ||
        !primary_fields(&reader,base,&after,&second)) {
        if (!reader.failure) reader.failure="PRIMARY_IDENTITY_CONTEXT";
        goto done;
    }
    end_base=api->module_base("game.dll",&end_size);
    api->session(mod,&end);
    if (base!=end_base || size!=end_size || memcmp(&before,&after,sizeof(before)) ||
        memcmp(&first,&second,sizeof(first)) || start.role!=end.role || start.version!=end.version) {
        reader.failure="PRIMARY_CONTEXT_CHANGED";goto done;
    }
    result.fields=second;result.valid=1;
    result.fields.action.valid=1;result.fields.action.inventory=after;
    result.fields.action.game_base=base;result.fields.action.game_size=size;
    result.fields.action.session_role=end.role;result.fields.action.session_version=end.version;
    result.fields.action.switch_ready=valid_id(after.slots[0]) && valid_id(after.slots[1]) &&
                                      after.active_selector>=1 && after.active_selector<=3;
    primary_predicate(&result,&after);
done:
    if (diagnostics) *diagnostics=reader;
    return result;
}
#endif
