/* Concrete recovery services reuse accepted typed selection/attachment/send.
 * Every effect is consumed by the finite owner before entry; no direct writes. */
#ifndef USS_RECOVERY_NATIVE_H
#define USS_RECOVERY_NATIVE_H
#include "uss_recovery_reader.h"
#include "formation_native.h"
#include "uss_death_drop_pin.h"
typedef struct UssRecoveryNative {
    UssFormationNative* formation;UssRecoveryReader reader;const UssRecoveryWork* work;
    Inventory capture_owner;
    void* input_user;int (*input_clear)(void*,const UssRecoverySnapshot*);
    void* drop_user;int (*drop)(void*,uintptr_t,uintptr_t,uint32_t,uint32_t);
} UssRecoveryNative;
static inline int recovery_native_observe(void* user,const UssRecoveryWork* work,UssRecoverySnapshot* output)
{return uss_recovery_observe(&((UssRecoveryNative*)user)->reader,work,output);}
static inline int recovery_native_current(UssRecoveryNative* native,const UssRecoveryWork* work,
    const UssRecoverySnapshot* expected,UssRecoverySnapshot* current)
{
    return native->formation && recovery_lifetime(work,&native->formation->runtime->life.owner) &&
        uss_recovery_observe(&native->reader,work,current) && !memcmp(expected,current,sizeof(*current));
}
static int recovery_native_input(void* user,UssCaptureInput* input)
{
    UssRecoveryNative* native=user;const UssRecoveryWork* work=native->work;
    if(!work || native->formation->runtime->life.owner.pair_present ||
        !recovery_lifetime(work,&native->formation->runtime->life.owner))return 0;
    memset(input,0,sizeof(*input));input->owner=native->capture_owner;
    input->pair_generation=work->owner.pair_generation;input->context_generation=work->owner.context_generation;
    input->callback_thread=work->owner.thread;input->utility=work->owner.utility.entity;input->ordinary=work->owner.ordinary.entity;
    memcpy(input->utility_identity,work->owner.utility.identity,8);memcpy(input->ordinary_identity,work->owner.ordinary.identity,8);return 1;
}
static int recovery_native_publish(UssRecoveryNative* native,const UssRecoveryWork* work,const UssRecoverySnapshot* snapshot)
{
    UssCaptureProducer producer={0};UssLoneCaptureContext lone;UssPublicationCapture before,entry;
    UssPublicationState* state=&native->formation->runtime->publication;UssInventoryMessage message={0};
    uintptr_t base=formation_native_module(native->formation);uint64_t now=native->formation->runtime->io.clock_ms(native->formation->runtime->io.user);
    if(work->local_only || work->owner.context.local_only || !base || state->disabled || state->suspended || state->busy || !state->formed ||
        (state->game_thread && state->game_thread!=work->owner.thread) ||
        state->record.pair_generation!=work->owner.pair_generation || now>UINT64_MAX-10000)return 0;
    if(now>=state->rate_until){state->rate_until=now+10000;state->used=0;}
    if(state->used>=64)return 0;
    native->work=work;native->capture_owner=snapshot->inventory;lone.target=work->survivor.entity;lone.outgoing=work->outgoing.entity;
    producer.api=native->formation->api;producer.mod=native->formation->mod;producer.platform=native->formation->platform;
    producer.user=native;producer.input=recovery_native_input;
    if(!uss_capture_lone_publication(&producer,&lone,&before,NULL) ||
        !publication_key_equal(&before.key,&state->record) ||
        !uss_capture_lone_publication(&producer,&lone,&entry,NULL) ||
        !publication_key_equal(&before.key,&entry.key) || !publication_owner_equal(&before.owner,&entry.owner) ||
        before.thread!=entry.thread || before.fingerprint_size!=entry.fingerprint_size ||
        memcmp(before.fingerprint,entry.fingerprint,before.fingerprint_size) || memcmp(before.wires,entry.wires,sizeof(before.wires)) ||
        !recovery_lifetime(work,&native->formation->runtime->life.owner))return 0;
    message.avatar=entry.owner.avatar;message.primary=u32(entry.owner.row);message.secondary=u32(entry.owner.row+4);
    message.support=u32(entry.owner.row+8);message.backpack=u32(entry.owner.row+12);message.row_field24=u32(entry.owner.row+24);
    memcpy(&message.metadata,entry.owner.row+40,8);
    state->busy=1;state->game_thread=entry.thread;++state->used;++state->attempted;++native->formation->effects;
    if(!native->formation->invoke(native->formation->invoke_user,USS_EFFECT_SEND,base+0xbe3f40,0,0,0,0,0,&message)){
        state->disabled=1;state->busy=0;return 0;
    }
    ++state->returned;state->busy=0;return 1;
}
static inline int recovery_native_select(UssRecoveryNative* native,const UssRecoveryWork* work,
    const UssRecoverySnapshot* expected,uint32_t slot)
{
    UssFormationNative* formation=native->formation;UssRecoverySnapshot current,after;Reader memory={0};
    UssSelectionAddresses addresses;UssCapturePage page;uintptr_t base=formation_native_module(formation);size_t size=0;
    if(!base || slot<1 || slot>2 || !recovery_native_current(native,work,expected,&current) || current.death ||
        !current.primary.valid || !publication_id(current.inventory.slots[slot-1]) ||
        current.primary.fields.global_guard || current.primary.fields.state_byte || u32(current.primary.fields.row+28))return 0;
    memory.api=formation->api;
    if(formation->api->module_base("game.dll",&size)!=base || !uss_selection_entry_pins(&memory,base,size,&addresses) ||
        !formation->platform.query(formation->platform.user,addresses.state,&page) || !capture_page_executable(&page,base,addresses.state,64) ||
        !formation->platform.query(formation->platform.user,addresses.weapon,&page) || !capture_page_executable(&page,base,addresses.weapon,64) ||
        !recovery_native_current(native,work,expected,&current))return 0;
    ++formation->effects;
    if(!formation->invoke(formation->invoke_user,USS_EFFECT_SELECT,addresses.state,addresses.weapon,
        current.primary.fields.action.record,current.inventory.roots[2],current.inventory.avatar,slot,NULL) ||
        !uss_recovery_observe(&native->reader,work,&after) || after.death ||
        after.inventory.active_selector!=slot ||
        memcmp(after.inventory.slots,current.inventory.slots,sizeof(current.inventory.slots)))return 0;
    return 1;
}
static int recovery_native_effect(void* user,const UssRecoveryWork* work,const UssRecoverySnapshot* expected,
    UssRecoveryEffect effect,uint32_t slot)
{
    UssRecoveryNative* native=user;UssFormationNative* formation=native->formation;
    UssRecoverySnapshot current,after;uintptr_t base;
    if(!formation)return 0;
    formation->reason="RECOVERY_EFFECT_SERVICE";
    if(!formation->invoke || !formation->platform.query || !(base=formation_native_module(formation)))return 0;
    formation->reason="RECOVERY_EFFECT_CONTEXT";
    if(!recovery_native_current(native,work,expected,&current))return 0;
    formation->reason="RECOVERY_EFFECT_INPUT";
    if(native->input_clear && !native->input_clear(native->input_user,&current))return 0;
    if(effect==USS_RECOVERY_SELECT_PRIMARY || effect==USS_RECOVERY_SELECT_FINAL)return recovery_native_select(native,work,&current,slot);
    if(effect==USS_RECOVERY_PUBLISH)return current.primary.valid && current.primary.ready && recovery_native_publish(native,work,&current);
    if(effect==USS_RECOVERY_DROP_STORED){
        if(!native->drop || !current.all_empty || current.outgoing_absent || current.outgoing.parent ||
            current.survivor.parent!=current.inventory.avatar ||
            !formation_native_entry(formation,base,0x9a8840,uss_death_drop_pin,sizeof(uss_death_drop_pin)) ||
            !recovery_native_current(native,work,expected,&current))return 0;
        ++formation->effects;
        return native->drop(native->drop_user,base+0x9a8840,current.inventory.roots[2],current.inventory.avatar,work->survivor.entity) &&
            uss_recovery_observe(&native->reader,work,&after) && after.all_empty && !after.survivor.parent &&
            !memcmp(current.inventory.row,after.inventory.row,48);
    }
    formation->reason="RECOVERY_RESTORE_SHAPE";
    if(effect!=USS_RECOVERY_RESTORE || current.death || current.inventory.slots[2]!=0 ||
        current.survivor.parent!=current.inventory.avatar)return 0;
    formation->reason="RECOVERY_RESTORE_INTERACTION";
    if(!current.survivor.interaction_admissible)return 0;
    formation->reason="RECOVERY_RESTORE_ENTRY_PIN";
    if(!formation_native_entry(formation,base,0x9aa290,uss_attachment_entry_pin,64))return 0;
    formation->reason="RECOVERY_RESTORE_PRIMARY_SELECT";
    if(current.inventory.active_selector!=1){
        if(!recovery_native_select(native,work,&current,1) || !uss_recovery_observe(&native->reader,work,&current))return 0;
    }
    formation->reason="RECOVERY_RESTORE_PRIMARY_READY";
    if(current.inventory.active_selector!=1 ||
        !current.primary.valid || current.primary.fields.global_guard || current.primary.fields.state_byte || u32(current.primary.fields.row+28))return 0;
    /* Selection may have changed the snapshot; take one matching fresh read
     * before entering attachment. No attempt is repeated after a returned call. */
    formation->reason="RECOVERY_RESTORE_FINAL_CONTEXT";
    if(!recovery_native_current(native,work,&current,&after))return 0;
    formation->reason="RECOVERY_RESTORE_ENTERED";
    ++formation->effects;
    if(!formation->invoke(formation->invoke_user,USS_EFFECT_ATTACH,base+0x9aa290,0,0,current.inventory.roots[2],
        current.inventory.avatar,work->survivor.entity,NULL) || !uss_recovery_observe(&native->reader,work,&after))return 0;
    formation->reason="RECOVERY_RESTORE_RETURNED_VERIFY";
    return !after.death && after.inventory.slots[2]==work->survivor.entity && after.inventory.active_selector==1 &&
        after.survivor.parent==current.inventory.avatar;
}
/* Typed production death-drop forwarding; constructor inclusion is inert. */
static inline int uss_recovery_drop_invoke(void* user,uintptr_t entry,uintptr_t inventory,uint32_t avatar,uint32_t entity)
{UssNativeCalls calls={0};(void)user;calls.drop_entity=(UssDropEntityFn)entry;return uss_drop_stored_entity(&calls,(void*)inventory,avatar,entity)==USS_CALL_RETURNED;}
static inline UssRecoveryIo uss_recovery_native_io(UssRecoveryNative* native)
{UssRecoveryIo io={native,recovery_native_observe,recovery_native_effect};return io;}
#endif
