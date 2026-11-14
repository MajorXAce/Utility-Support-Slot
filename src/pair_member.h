/* Read-only member evidence. No native bindings or gameplay admission by itself. */
#ifndef USS_PAIR_MEMBER_H
#define USS_PAIR_MEMBER_H
#include "../ResearchSource/inventory_reader.h"
#include "weapon_policy.h"

typedef enum UssMemberStatus {
    USS_MEMBER_UNKNOWN=0, USS_MEMBER_EMPTY, USS_MEMBER_OBSERVED
} UssMemberStatus;
typedef struct UssMemberEvidence {
    UssMemberStatus status;
    uint32_t entity, parent, kind, zone, tail;
    uint32_t native_kind; /* Preserve the runtime value; kind is the effective support category. */
    unsigned char gate;
    unsigned parent_matches, interaction_admissible;
    UssSupportClass classification;
    unsigned char descriptor[24], interaction_descriptor[24];
    uintptr_t weapon_manager, weapon_unit, weapon_row;
    uintptr_t interaction_manager, interaction_unit, interaction_row;
    uint32_t weapon_index, interaction_index;
    unsigned char weapon_header[72], interaction_header[40];
} UssMemberEvidence;

static int member_fields(Reader* reader,uintptr_t base,const Inventory* context,
                         uint32_t entity,const unsigned char expected[8],
                         UssMemberEvidence* result)
{
    uintptr_t unit_rows,unit_pointers,interaction_rows,interaction_pointers;
    uint32_t count;
    unsigned char row[20];
    memset(result,0,sizeof(*result));
    reader->stage="MEMBER_IDENTITY";
    if (!get_ptr(reader,base+0x3326DC0,&result->weapon_manager) ||
        result->weapon_manager!=context->roots[4] ||
        !get(reader,result->weapon_manager,result->weapon_header,72) ||
        !lookup(reader,result->weapon_manager+0x20,entity,&result->weapon_index)) return 0;
    count=u32(result->weapon_header+12);
    unit_pointers=ptr_value(result->weapon_header+56);
    unit_rows=ptr_value(result->weapon_header+64);
    if (count>16384 || count>u32(result->weapon_header) || result->weapon_index>=count ||
        !address_ok(unit_pointers,(size_t)count*8) ||
        !address_ok(unit_rows,(size_t)count*48) ||
        !get_ptr(reader,unit_pointers+(uintptr_t)result->weapon_index*8,&result->weapon_unit) ||
        !get(reader,result->weapon_unit,result->descriptor,24) ||
        u32(result->descriptor+8)!=entity || memcmp(result->descriptor,expected,8)) return 0;
    result->weapon_row=unit_rows+(uintptr_t)result->weapon_index*48;
    if (!get(reader,result->weapon_row,row,20)) return 0;
    result->entity=entity; result->parent=u32(row+4); result->native_kind=u32(row+12);
    result->kind=weapon_support_kind(reader,result->descriptor,result->native_kind);
    if(reader->failure)return 0;
    result->classification=uss_classify_support(result->descriptor,result->kind);
    result->parent_matches=result->parent==context->avatar;

    reader->stage="MEMBER_INTERACTION";
    if (!get_ptr(reader,base+0x3326D68,&result->interaction_manager) ||
        !get(reader,result->interaction_manager+0x14050,result->interaction_header,40) ||
        !lookup(reader,result->interaction_manager+0x14050,entity,&result->interaction_index) ||
        result->interaction_index>=16384) return 0;
    /* Diagnostic cap, not an inferred allocation count. Missing rows stay unknown. */
    interaction_pointers=ptr_value(result->interaction_header+24);
    interaction_rows=ptr_value(result->interaction_header+32);
    if (!address_ok(interaction_pointers,((size_t)result->interaction_index+1)*8) ||
        !address_ok(interaction_rows,((size_t)result->interaction_index+1)*0x370) ||
        !get_ptr(reader,interaction_pointers+(uintptr_t)result->interaction_index*8,
                 &result->interaction_unit) ||
        !get(reader,result->interaction_unit,result->interaction_descriptor,24) ||
        u32(result->interaction_descriptor+8)!=entity ||
        memcmp(result->interaction_descriptor,expected,8)) return 0;
    result->interaction_row=interaction_rows+(uintptr_t)result->interaction_index*0x370;
    if (!get(reader,result->interaction_row+0xA8,&result->zone,4) ||
        !get(reader,result->interaction_row+0xC9,&result->gate,1) ||
        !get(reader,result->interaction_row+0x360,&result->tail,4)) return 0;
    /* Exact Lua switch_entity gate. These fields are NOT the separate pickup mask. */
    result->interaction_admissible=(result->gate==0 || result->gate==1) &&
                                  (result->zone==UINT32_MAX || result->gate==0);
    result->status=USS_MEMBER_OBSERVED;
    return 1;
}

static inline UssMemberEvidence uss_observe_pair_member(const HfApi* api,HfMod* mod,
    const Inventory* expected_context,uint32_t entity,const unsigned char expected[8],
    Reader* diagnostics)
{
    UssMemberEvidence result={0},first={0},second={0};
    Inventory before,after;
    Reader reader={0};
    HfSession start={0},end={0};
    uintptr_t base=0,end_base=0;
    size_t size=0,end_size=0;
    const char* build;
    unsigned char pin[32];
    /* Retained 978AC0 setter prefix; RIP load at978AD4 resolves3326D68. Pin only. */
    static const unsigned char expected_pin[32]={
        0x40,0x53,0x41,0x54,0x41,0x57,0x48,0x83,0xec,0x20,0x3b,0x15,0x50,0xb1,0xb0,0x02,
        0x45,0x0f,0xb6,0xf9,0x48,0x8b,0x1d,0x8d,0xe2,0x9a,0x02,0x45,0x8b,0xe0,0x0f,0x84};
    /* Preserve a supplied transaction budget, never grant a second read allowance. */
    if (diagnostics) {reader.calls=diagnostics->calls;reader.bytes=diagnostics->bytes;}
    reader.api=api;reader.slot=-1;
    if (reader.calls>2165 || reader.bytes>20000) {reader.failure="READ_BOUNDS";goto done;}
    if (!valid_id(entity)) {result.status=USS_MEMBER_EMPTY;goto done;}
    if (!expected_context || !expected || !api || !mod || !uss_sdk_compatible(api) || !api->read || !api->module_base || !api->session ||
        !api->build_id) {reader.failure="MEMBER_CONTRACT";goto done;}
    build=api->build_id();
    if (!build || strcmp(build,"25480438")) {reader.failure="BUILD_CONTRACT";goto done;}
    api->session(mod,&start);
    if (start.role!=HF_SESSION_SOLO) {reader.failure="SESSION_ROLE";goto done;}
    base=api->module_base("game.dll",&size);
    reader.stage="INTERACTION_PIN";
    if (!base || size<0x3326DC8 || size>0x20000000 || !address_ok(base,size) ||
        !get(&reader,base+0x978AC0,pin,32) || memcmp(pin,expected_pin,32) ||
        (int64_t)0x978AD4+7+(int32_t)u32(pin+23)!=0x3326D68) {
        reader.failure="INTERACTION_PIN";goto done;
    }
    if (!inventory_sample(&reader,&before) ||
        memcmp(expected_context,&before,sizeof(before)) ||
        !member_fields(&reader,base,&before,entity,expected,&first) ||
        !inventory_sample(&reader,&after) ||
        !member_fields(&reader,base,&after,entity,expected,&second)) {
        if (!reader.failure) reader.failure="MEMBER_IDENTITY_CONTEXT";
        goto done;
    }
    end_base=api->module_base("game.dll",&end_size);
    api->session(mod,&end);
    if (base!=end_base || size!=end_size || memcmp(&before,&after,sizeof(before)) ||
        memcmp(&first,&second,sizeof(first)) || start.role!=end.role || start.version!=end.version) {
        reader.failure="MEMBER_CONTEXT_CHANGED";goto done;
    }
    result=second;
done:
    if (diagnostics) *diagnostics=reader;
    return result;
}
#endif
