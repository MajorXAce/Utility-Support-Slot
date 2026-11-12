/* Bounded read-only port of Lua choose()/can_switch() target discovery.
 * An observed record is not authority to call a function with its address. */
#ifndef USS_ACTION_STATE_H
#define USS_ACTION_STATE_H
#include "../ResearchSource/inventory_reader.h"

typedef struct UssActionState {
    unsigned valid, switch_ready;
    uintptr_t manager, record, context;
    uint32_t avatar, index, count;
    unsigned char row[24];
    Inventory inventory;
    uintptr_t game_base;
    size_t game_size;
    uint32_t session_role, session_version;
} UssActionState;

static int action_record(Reader* reader,uintptr_t global,uint32_t avatar,
                         UssActionState* result)
{
    unsigned char context_avatar[4];
    reader->stage = "ACTION_RECORD";
    memset(result,0,sizeof(*result));
    if (!get_ptr(reader,global,&result->manager) ||
        !get(reader,result->manager+0x70,&result->count,4) ||
        !result->count || result->count>4096 ||
        !lookup(reader,result->manager+0xF8,avatar,&result->index) ||
        result->index>=result->count ||
        !get_ptr(reader,result->manager+0x110+(uintptr_t)result->index*8,&result->context) ||
        !get(reader,result->context+8,context_avatar,4) || u32(context_avatar)!=avatar)
        return 0;
    result->record=result->manager+0x53DCD0+(uintptr_t)result->index*0x1238;
    if (!get(reader,result->record,result->row,sizeof(result->row)) ||
        u32(result->row)!=avatar) return 0;
    result->avatar=avatar;
    return 1;
}

static UssActionState action_state_sample(const HfApi* api,HfMod* mod,
                                              Reader* diagnostics)
{
    UssActionState result={0}, first={0}, second={0};
    Inventory before,after;
    HfSession session_before={0},session_after={0};
    Reader reader={0};
    uintptr_t base=0,after_base=0,global=0;
    size_t size=0,after_size=0;
    const char* build;
    unsigned char pin[7];
    /* Retained select_state instruction A93ECD -> global3326D20, exact build. */
    static const unsigned char expected[7]={0x4c,0x8b,0x35,0x4c,0x2e,0x89,0x02};
    /* Preserve one shared budget across a selection preflight and readback. */
    if (diagnostics) { reader.calls=diagnostics->calls; reader.bytes=diagnostics->bytes; }
    reader.api=api; reader.slot=-1;
    if (!api || !mod || !uss_sdk_compatible(api) ||
        !api->read || !api->module_base || !api->session || !api->build_id) {
        reader.failure="API_CONTRACT"; goto done;
    }
    build=api->build_id();
    if (!build || strcmp(build,"25480438")) {reader.failure="BUILD_CONTRACT";goto done;}
    api->session(mod,&session_before);
    if (session_before.role!=HF_SESSION_SOLO) {reader.failure="SESSION_ROLE";goto done;}
    base=api->module_base("game.dll",&size);
    reader.stage="ACTION_PIN";
    if (!base || size<0x3326D28 || size>0x20000000 ||
        !get(&reader,base+0xA93ECD,pin,sizeof(pin)) || memcmp(pin,expected,sizeof(pin))) {
        reader.failure="ACTION_PIN";goto done;
    }
    global=base+0xA93ECD+7+(uintptr_t)(int64_t)(int32_t)u32(pin+3);
    if (global!=base+0x3326D20) {reader.failure="ACTION_PIN";goto done;}
    if (!inventory_sample(&reader,&before) ||
        !action_record(&reader,global,before.avatar,&first) ||
        !inventory_sample(&reader,&after) ||
        !action_record(&reader,global,after.avatar,&second)) {
        if (!reader.failure) reader.failure="ACTION_IDENTITY";
        goto done;
    }
    after_base=api->module_base("game.dll",&after_size);
    api->session(mod,&session_after);
    if (base!=after_base || size!=after_size ||
        memcmp(&before,&after,sizeof(before)) || memcmp(&first,&second,sizeof(first)) ||
        session_before.role!=session_after.role || session_before.version!=session_after.version) {
        reader.failure="ACTION_CONTEXT_CHANGED";goto done;
    }
    result=second; result.valid=1;
    result.inventory=after; result.game_base=base; result.game_size=size;
    result.session_role=session_after.role; result.session_version=session_after.version;
    /* Lua can_switch() readiness; no phase inference or mutation permission. */
    result.switch_ready=before.slots[0]!=0 && before.slots[1]!=0 &&
                        before.active_selector>=1 && before.active_selector<=3;
done:
    if (diagnostics) *diagnostics=reader;
    return result;
}
/* Standalone observations always start with a fresh budget. */
static inline UssActionState uss_observe_action_state(const HfApi* api,HfMod* mod,
                                              Reader* diagnostics)
{
    Reader budget={0};
    UssActionState result=action_state_sample(api,mod,&budget);
    if (diagnostics) *diagnostics=budget;
    return result;
}
#endif
