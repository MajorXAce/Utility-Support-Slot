/* Fixed-game production adapters. Only the typed invoker enters game code;
 * fixtures replace that invoker with own functions after the same read checks. */
#ifndef USS_FORMATION_NATIVE_H
#define USS_FORMATION_NATIVE_H
#include "formation_runtime.h"
#include "selection_entry_pins.h"
#include "attachment_entry_pin.h"
#include "native_calls.h"
typedef enum UssFormationEffect { USS_EFFECT_SELECT=1,USS_EFFECT_ATTACH,USS_EFFECT_SEND } UssFormationEffect;
typedef struct UssFormationNative {
    const HfApi* api;
    HfMod* mod;
    UssCapturePlatform platform;
    UssFormationRuntime* runtime;
    unsigned effects;
    const char* reason;
    /* Required explicit binding: production or own fixture. Never an unchecked
     * alternative path; the adapter validates entry bytes/page/context first. */
    int (*invoke)(void*,UssFormationEffect,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uint32_t,uint32_t,const UssInventoryMessage*);
    void* invoke_user;
    void* acknowledge_user;
    void (*acknowledge)(void*,uint32_t,uint32_t,UssPairLabel); /* Optional, cosmetic only. */
} UssFormationNative;
static UssCoreObservation formation_native_observe(void* user,const UssPairOwner* owner,const UssCoreFlags* flags)
{
    UssFormationNative* native=user;UssCorePlatform platform={0};
    platform.user=native->platform.user;platform.thread_id=native->platform.thread_id;
    return uss_core_observe(native->api,native->mod,owner,flags,&platform,NULL);
}
static int formation_native_input(void* user,UssCaptureInput* out)
{
    const UssFormationRuntime* runtime=user;memset(out,0,sizeof(*out));
    if(!runtime || !pair_owner_ready(&runtime->life.owner,runtime->thread))return 0;
    out->pair_generation=runtime->life.owner.pair_generation;
    out->context_generation=runtime->life.owner.context_generation;
    out->owner=runtime->latest.inventory;out->utility=runtime->life.owner.utility.entity;
    out->ordinary=runtime->life.owner.ordinary.entity;out->callback_thread=runtime->thread;
    memcpy(out->utility_identity,runtime->life.owner.utility.identity,8);
    memcpy(out->ordinary_identity,runtime->life.owner.ordinary.identity,8);return 1;
}
static int formation_native_capture(void* user,const UssFormationRuntime* runtime,UssPublicationStage stage,UssPublicationCapture* out)
{
    UssFormationNative* native=user;UssCaptureProducer producer={0};(void)stage;
    producer.api=native->api;producer.mod=native->mod;producer.platform=native->platform;
    producer.user=(void*)runtime;producer.input=formation_native_input;
    if(formation_controller_draw_ready(runtime,&runtime->latest)){
        uint64_t now=runtime->io.clock_ms(runtime->io.user),end;UssPairTransaction before=runtime->work;
        if(!uss_capture_managed_draw_publication(&producer,&runtime->work,now,out,NULL))return 0;
        end=runtime->io.clock_ms(runtime->io.user);
        return !memcmp(&before,&runtime->work,sizeof(before)) && end>=now && end<before.deadline_ms &&
            uss_pair_owner_publication(&runtime->life.owner,runtime->thread,runtime->life.owner.context.role,&out->owner,&out->key);
    }
    return uss_capture_publication(&producer,out,NULL) &&
        uss_pair_owner_publication(&runtime->life.owner,runtime->thread,runtime->life.owner.context.role,&out->owner,&out->key);
}
static int formation_native_current_scope(UssFormationNative* native,const UssCoreObservation* expected,UssCoreObservation* current,unsigned require_ready)
{
    UssFormationRuntime* runtime=native->runtime;UssCoreFlags flags;
    if(!runtime || !native->invoke || !expected || !expected->valid || !expected->pair_matched ||
        !native->platform.thread_id || native->platform.thread_id(native->platform.user)!=runtime->thread)return 0;
    flags=uss_lifecycle_flags(&runtime->life,&runtime->policy);
    *current=formation_native_observe(native,&runtime->life.owner,&flags);
    return lifecycle_current(&runtime->life,runtime->thread,current) && current->pair_matched &&
        pair_owner_context_equal(&expected->context,&current->context) &&
        expected->expected_epoch==current->expected_epoch && expected->expected_pair==current->expected_pair &&
        expected->expected_guard==current->expected_guard && expected->expected_context==current->expected_context &&
        !memcmp(&expected->inventory,&current->inventory,sizeof(current->inventory)) &&
        current->primary.valid && (!require_ready || (current->primary.fields.global_guard==0 && current->primary.fields.state_byte==0 &&
        u32(current->primary.fields.row+28)==0));
}
static inline int formation_native_current(UssFormationNative* native,const UssCoreObservation* expected,UssCoreObservation* current)
{return formation_native_current_scope(native,expected,current,1);}
static int formation_native_selection_ready(const UssCoreObservation* current)
{return current->primary.fields.global_guard==0 && current->primary.fields.state_byte==0 && u32(current->primary.fields.row+28)==0;}

static int formation_native_entry(UssFormationNative* native,uintptr_t base,uint32_t rva,const unsigned char* pin,size_t length)
{
    UssCaptureReader reader={0};UssCapturePage before={0},after={0};
    reader.memory.api=native->api;reader.platform=&native->platform;
    return length && length<=4096 && native->platform.query &&
        native->platform.query(native->platform.user,base+rva,&before) &&
        capture_page_executable(&before,base,base+rva,length) &&
        capture_pin(&reader,base+rva,pin,length) &&
        native->platform.query(native->platform.user,base+rva,&after) && !memcmp(&before,&after,sizeof(before));
}
static uintptr_t formation_native_module(UssFormationNative* native)
{
    size_t size=0;uintptr_t base;const char* build;
    if(!native->api || !native->api->module_base || !native->api->build_id)return 0;
    build=native->api->build_id();if(!build || strcmp(build,"25480438"))return 0;
    base=native->api->module_base("game.dll",&size);
    return base && size>=0x3484570 && size<=0x20000000 && address_ok(base,size)?base:0;
}
static UssPairServiceResult formation_native_select(void* user,const UssCoreObservation* expected,uint32_t slot)
{
    UssFormationNative* native=user;UssCoreObservation current;UssSelectionAddresses addresses;
    Reader reader={0};UssCapturePage page;uintptr_t base=formation_native_module(native);size_t size=0;
    reader.api=native->api;
    native->reason="SELECT_CONTEXT";
    if(!base || slot<1 || slot>3 || !formation_native_current_scope(native,expected,&current,0) ||
        !publication_id(current.inventory.slots[slot-1]))return USS_PAIR_SERVICE_STOP;
    if(!formation_native_selection_ready(&current) && !formation_solo_draw_ready(native->runtime,&current) && !formation_controller_draw_ready(native->runtime,&current)){
        native->reason="SELECT_NOT_READY_BEFORE_ENTRY";
        return slot==1?USS_PAIR_SERVICE_WAIT:USS_PAIR_SERVICE_STOP;
    }
    native->reason="SELECT_ENTRY_PINS";
    if(native->api->module_base("game.dll",&size)!=base || !uss_selection_entry_pins(&reader,base,size,&addresses) ||
        !native->platform.query(native->platform.user,addresses.state,&page) || !capture_page_executable(&page,base,addresses.state,64) ||
        !native->platform.query(native->platform.user,addresses.weapon,&page) || !capture_page_executable(&page,base,addresses.weapon,64))return USS_PAIR_SERVICE_STOP;
    native->reason="SELECT_FINAL_CONTEXT";
    if(!formation_native_current_scope(native,expected,&current,0))return USS_PAIR_SERVICE_STOP;
    if(!formation_native_selection_ready(&current) && !formation_solo_draw_ready(native->runtime,&current) && !formation_controller_draw_ready(native->runtime,&current)){
        native->reason="SELECT_NOT_READY_BEFORE_ENTRY";
        return slot==1?USS_PAIR_SERVICE_WAIT:USS_PAIR_SERVICE_STOP;
    }
    ++native->effects;native->reason="SELECT_ENTERED";
    if(!native->invoke(native->invoke_user,USS_EFFECT_SELECT,addresses.state,addresses.weapon,
        current.primary.fields.action.record,current.inventory.roots[2],current.inventory.avatar,slot,NULL)){
        native->reason="SELECT_ENTRY_UNCONFIRMED";return USS_PAIR_SERVICE_STOP;}
    native->reason="SELECT_CALLS_RETURNED";return USS_PAIR_SERVICE_DONE;
}
/* Lone utility selection has no virtual pair to satisfy the paired adapter.
 * Keep that adapter unchanged. This narrower route only selects the already
 * owned utility in native slot3, with fresh identity/parent and post-call checks. */
static int formation_native_single_current(UssFormationNative* native,const UssCoreObservation* expected,UssCoreObservation* current)
{
    UssFormationRuntime* runtime=native->runtime;UssCoreFlags flags;UssPairMember utility;UssCoreCandidateNames names;
    UssCoreCandidates bundle;UssCorePlatform platform;
    if(!runtime || !native->invoke || !expected || !expected->valid || expected->pair_matched ||
        runtime->life.owner.pair_present || !native->platform.thread_id ||
        native->platform.thread_id(native->platform.user)!=runtime->thread)return 0;
    utility.entity=expected->inventory.slots[2];memcpy(utility.identity,expected->inventory.identities[2],8);
    if(!uss_core_candidate_names(&runtime->life.owner,&utility,1,&names))return 0;
    flags=uss_lifecycle_flags(&runtime->life,&runtime->policy);
    platform.user=native->platform.user;platform.thread_id=native->platform.thread_id;
    bundle=uss_core_observe_candidates(native->api,native->mod,&runtime->life.owner,&flags,&platform,&names,NULL);
    *current=bundle.current;
    return bundle.valid && lifecycle_current(&runtime->life,runtime->thread,current) && !current->pair_matched &&
        current->support_class==USS_UTILITY_SUPPORT && bundle.members[0].parent==current->inventory.avatar &&
        pair_owner_context_equal(&expected->context,&current->context) &&
        expected->expected_epoch==current->expected_epoch && expected->expected_pair==current->expected_pair &&
        expected->expected_guard==current->expected_guard && expected->expected_context==current->expected_context &&
        !memcmp(&expected->inventory,&current->inventory,sizeof(current->inventory)) && current->primary.valid &&
        current->inventory.active_selector>=1 && current->inventory.active_selector<=2 &&
        current->inventory.held==current->inventory.slots[current->inventory.active_selector-1] &&
        !current->primary.fields.global_guard && !current->primary.fields.state_byte && !u32(current->primary.fields.row+28);
}
static inline UssPairServiceResult formation_native_select_single(UssFormationNative* native,const UssCoreObservation* expected)
{
    UssCoreObservation current,after;UssSelectionAddresses addresses;UssCoreFlags flags;
    Reader reader={0};UssCapturePage page;uintptr_t base;size_t size=0;
    if(!native || !native->platform.query)return USS_PAIR_SERVICE_STOP;base=formation_native_module(native);reader.api=native->api;
    if(!base || !formation_native_single_current(native,expected,&current) ||
        native->api->module_base("game.dll",&size)!=base || !uss_selection_entry_pins(&reader,base,size,&addresses) ||
        !native->platform.query(native->platform.user,addresses.state,&page) || !capture_page_executable(&page,base,addresses.state,64) ||
        !native->platform.query(native->platform.user,addresses.weapon,&page) || !capture_page_executable(&page,base,addresses.weapon,64) ||
        !formation_native_single_current(native,expected,&current))return USS_PAIR_SERVICE_STOP;
    ++native->effects;
    if(!native->invoke(native->invoke_user,USS_EFFECT_SELECT,addresses.state,addresses.weapon,
        current.primary.fields.action.record,current.inventory.roots[2],current.inventory.avatar,3,NULL))return USS_PAIR_SERVICE_STOP;
    flags=uss_lifecycle_flags(&native->runtime->life,&native->runtime->policy);
    after=formation_native_observe(native,&native->runtime->life.owner,&flags);
    if(!lifecycle_current(&native->runtime->life,native->runtime->thread,&after) || after.expected_pair_present ||
        !pair_owner_context_equal(&current.context,&after.context) || after.expected_epoch!=current.expected_epoch ||
        after.expected_pair!=current.expected_pair || after.expected_guard!=current.expected_guard ||
        after.expected_context!=current.expected_context || after.inventory.row_address!=current.inventory.row_address ||
        memcmp(after.inventory.roots,current.inventory.roots,sizeof(current.inventory.roots)) ||
        memcmp(after.inventory.slots,current.inventory.slots,sizeof(current.inventory.slots)) ||
        memcmp(after.inventory.identities,current.inventory.identities,4*24) ||
        after.inventory.active_selector!=3 || after.inventory.held!=current.inventory.slots[2])return USS_PAIR_SERVICE_STOP;
    return USS_PAIR_SERVICE_DONE;
}
static UssPairServiceResult formation_native_attach(void* user,const UssCoreObservation* expected,const UssPairMember* member)
{
    UssFormationNative* native=user;UssCoreObservation current;const UssMemberEvidence* evidence;
    uintptr_t base=formation_native_module(native);
    if(!base || !member || !formation_native_current_scope(native,expected,&current,0) || !formation_draw_ready(native->runtime,&current))return USS_PAIR_SERVICE_STOP;
    evidence=member->entity==native->runtime->life.owner.utility.entity?&current.utility:&current.ordinary;
    if(evidence->status!=USS_MEMBER_OBSERVED || evidence->entity!=member->entity ||
        memcmp(evidence->descriptor,member->identity,8) || !evidence->interaction_admissible ||
        (evidence->parent && evidence->parent!=current.inventory.avatar) ||
        !formation_native_entry(native,base,0x9AA290,uss_attachment_entry_pin,64) ||
        !formation_native_current_scope(native,expected,&current,0) || !formation_draw_ready(native->runtime,&current))return USS_PAIR_SERVICE_STOP;
    ++native->effects;
    return native->invoke(native->invoke_user,USS_EFFECT_ATTACH,base+0x9AA290,0,0,
        current.inventory.roots[2],current.inventory.avatar,member->entity,NULL)?USS_PAIR_SERVICE_DONE:USS_PAIR_SERVICE_STOP;
}
static UssPublicationSend formation_native_send(void* user,const UssPublicationCapture* captured,uint64_t metadata)
{
    UssFormationNative* native=user;UssFormationRuntime* runtime=native->runtime;
    UssInventoryMessage message={0};uintptr_t base=formation_native_module(native);uint64_t actual;
    const UssCapturePin* pin=&capture_pins[0];UssCoreObservation current;
    if(!base || !captured || !captured->valid || !runtime || runtime->life.owner.context.local_only ||
        !formation_native_current_scope(native,&runtime->latest,&current,0) || !formation_draw_ready(runtime,&current) ||
        !publication_owner_equal(&captured->owner,&runtime->row) ||
        !publication_key_equal(&captured->key,&runtime->view.key) || pin->exe || pin->rva!=0xBE3F40 ||
        !formation_native_entry(native,base,pin->rva,pin->bytes,pin->size))return USS_PUBLICATION_SEND_NOT_ENTERED;
    if(!formation_native_current_scope(native,&runtime->latest,&current,0) || !formation_draw_ready(runtime,&current))return USS_PUBLICATION_SEND_NOT_ENTERED;
    memcpy(&actual,captured->owner.row+40,8);if(metadata!=actual)return USS_PUBLICATION_SEND_NOT_ENTERED;
    message.avatar=captured->owner.avatar;message.primary=u32(captured->owner.row);
    message.secondary=u32(captured->owner.row+4);message.support=u32(captured->owner.row+8);
    message.backpack=u32(captured->owner.row+12);message.row_field24=u32(captured->owner.row+24);message.metadata=metadata;
    ++native->effects;
    return native->invoke(native->invoke_user,USS_EFFECT_SEND,base+pin->rva,0,0,0,0,0,&message)?
        USS_PUBLICATION_SEND_RETURNED:USS_PUBLICATION_SEND_UNCERTAIN;
}
/* This is the only real native-entry implementation. No call occurs by including
 * the header, constructing services, initializing the mod, or polling idle files. */
static int formation_native_invoke(void* user,UssFormationEffect effect,uintptr_t first,uintptr_t second,
    uintptr_t record,uintptr_t inventory,uint32_t avatar,uint32_t value,const UssInventoryMessage* message)
{
    UssNativeCalls calls={0};(void)user;
    if(effect==USS_EFFECT_SELECT){calls.select_state=(UssSelectStateFn)first;calls.select_weapon=(UssSelectWeaponFn)second;
        return uss_select_slot(&calls,(void*)record,(void*)inventory,avatar,value)==USS_CALL_RETURNED;}
    if(effect==USS_EFFECT_ATTACH){calls.attach=(UssAttachFn)first;
        return uss_attach_support(&calls,(void*)inventory,avatar,value)==USS_CALL_RETURNED;}
    if(effect==USS_EFFECT_SEND){calls.send_inventory=(UssSendInventoryFn)first;
        return uss_send_inventory(&calls,message)==USS_CALL_RETURNED;}
    return 0;
}
static void formation_native_acknowledge(void* user,uint32_t from,uint32_t target,UssPairLabel label)
{
    UssFormationNative* native=user;
    if(native->acknowledge)native->acknowledge(native->acknowledge_user,from,target,label);
}
static inline UssFormationIo uss_formation_native_io(UssFormationNative* native,uint64_t (*clock)(void*))
{
    UssFormationIo io={0};io.user=native;io.clock_ms=clock;io.observe=formation_native_observe;
    io.capture=formation_native_capture;io.select=formation_native_select;io.attach=formation_native_attach;io.send=formation_native_send;
    if(native->acknowledge)io.acknowledge=formation_native_acknowledge;return io;
}
#endif
