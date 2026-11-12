/* Single-peer replacement services. Reuse the typed transaction and existing
 * native entry pins, but do not construct a network mapping/dispatch capture.
 * All calls still require coherent named members, current lifetime and readback. */
#ifndef USS_LOCAL_REPLACEMENT_ADAPTER_H
#define USS_LOCAL_REPLACEMENT_ADAPTER_H
#include "replacement_adapter.h"
#include "formation_native.h"
typedef struct UssLocalReplacementAdapter {
    UssFormationNative* native;UssReplacementWork* work;UssReplacementOrigin origin;
    uint64_t consumed;unsigned failed,in_effect;
    void* invoke_user;
    int (*invoke)(void*,UssReplacementEffect,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uint32_t,uint32_t,const UssInventoryMessage*);
} UssLocalReplacementAdapter;
static inline int local_replacement_stop(UssLocalReplacementAdapter* a)
{a->failed=1;return 0;}
static int local_replacement_capture(UssLocalReplacementAdapter* a,const UssReplacementWork* work,
    UssReplacementStage stage,UssReplacementSnapshot* output)
{
    UssReplacementSnapshot result={0};UssReplacementWork effective,actual;
    UssFormationRuntime* r=a->native->runtime;UssPairMember members[3];UssCoreCandidateNames names;
    UssCorePlatform platform={0};UssCoreFlags flags;Reader memory={0};uint32_t empty=0,again=0;
    uintptr_t base=formation_native_module(a->native);
    if(a->failed || a->in_effect || !base || !work || !output || !r->life.owner.context.local_only)return 0;
    actual=*a->work;effective=*work;effective.stage=stage;
    if(!effective.local_only || !effective.origin.known || memcmp(&effective.origin,&a->origin,sizeof(a->origin)))return 0;
    members[0]=work->intent.incoming;members[1]=work->intent.kept;members[2]=work->intent.released;
    if(!uss_core_candidate_names(&r->life.owner,members,3,&names))return 0;
    flags=uss_lifecycle_flags(&r->life,&r->policy);
    platform.user=a->native->platform.user;platform.thread_id=a->native->platform.thread_id;memory.api=a->native->api;
    if(!get(&memory,base+0x348456c,&empty,4) || publication_id(empty))return 0;
    result.bundle=uss_core_observe_candidates(a->native->api,a->native->mod,&r->life.owner,&flags,&platform,&names,NULL);
    if(!get(&memory,base+0x348456c,&again,4) || again!=empty || memcmp(&actual,a->work,sizeof(actual)))return 0;
    result.empty_valid=1;result.native_empty=empty;
    if(!replacement_guard(&effective,&r->life,r->thread,&r->history,&result))return 0;
    *output=result;return 1;
}
static UssReplacementSnapshot local_replacement_observe(void* user,const UssReplacementWork* work)
{
    UssLocalReplacementAdapter* a=user;UssReplacementSnapshot result={0};
    local_replacement_capture(a,work,work->stage,&result);return result;
}
static int local_replacement_entry(UssLocalReplacementAdapter* a,UssReplacementStage before,
    const UssReplacementSnapshot* expected,UssReplacementSnapshot* output)
{
    UssReplacementSnapshot first={0};
    return local_replacement_capture(a,a->work,before,&first) && !memcmp(expected,&first,sizeof(first)) &&
        local_replacement_capture(a,a->work,before,output) && !memcmp(&first,output,sizeof(first));
}
static int local_replacement_invoke(UssLocalReplacementAdapter* a,UssReplacementEffect effect,
    uintptr_t first,uintptr_t second,uintptr_t record,const UssReplacementSnapshot* snapshot,uint32_t value)
{
    UssReplacementWork actual=*a->work;unsigned index=(unsigned)actual.stage*4+(unsigned)effect;int returned;
    if(a->failed || a->in_effect || !a->invoke || effect==USS_REPLACEMENT_SEND || index>=64 ||
        (a->consumed&(UINT64_C(1)<<index)))return local_replacement_stop(a);
    a->consumed|=UINT64_C(1)<<index;a->in_effect=1;
    returned=a->invoke(a->invoke_user,effect,first,second,record,snapshot->bundle.current.inventory.roots[2],
        snapshot->bundle.current.inventory.avatar,value,NULL);a->in_effect=0;
    return returned && !memcmp(&actual,a->work,sizeof(actual))?1:local_replacement_stop(a);
}
static UssPairServiceResult local_replacement_select(void* user,const UssReplacementSnapshot* expected,uint32_t slot)
{
    UssLocalReplacementAdapter* a=user;UssReplacementSnapshot entry,after;UssSelectionAddresses addresses;Reader memory={0};
    uintptr_t base=formation_native_module(a->native);size_t size=0;UssCapturePage page;
    UssReplacementStage prior=a->work->stage==USS_REPLACE_RECOVER?USS_REPLACE_PREPARE:USS_REPLACE_SELECT_FINAL;
    memory.api=a->native->api;
    if(!base || slot<1 || slot>3 || !publication_id(expected->bundle.current.inventory.slots[slot-1]) ||
        expected->bundle.current.primary.fields.global_guard || expected->bundle.current.primary.fields.state_byte ||
        a->native->api->module_base("game.dll",&size)!=base ||
        !uss_selection_entry_pins(&memory,base,size,&addresses) || !a->native->platform.query ||
        !a->native->platform.query(a->native->platform.user,addresses.state,&page) ||
        !capture_page_executable(&page,base,addresses.state,64) ||
        !a->native->platform.query(a->native->platform.user,addresses.weapon,&page) ||
        !capture_page_executable(&page,base,addresses.weapon,64) ||
        !local_replacement_entry(a,prior,expected,&entry) ||
        !local_replacement_invoke(a,USS_REPLACEMENT_SELECT,addresses.state,addresses.weapon,
            entry.bundle.current.primary.fields.action.record,&entry,slot) ||
        !local_replacement_capture(a,a->work,a->work->stage,&after) ||
        after.bundle.current.inventory.active_selector!=slot ||
        after.bundle.current.inventory.slots[2]!=entry.bundle.current.inventory.slots[2])goto fail;
    return USS_PAIR_SERVICE_DONE;
fail:local_replacement_stop(a);return USS_PAIR_SERVICE_STOP;
}
static UssPairServiceResult local_replacement_ready(void* user,const UssReplacementWork* work,
    const UssReplacementSnapshot* expected,uint64_t now)
{
    UssLocalReplacementAdapter* a=user;UssReplacementSnapshot current;
    if(!local_replacement_capture(a,work,work->stage,&current) || memcmp(expected,&current,sizeof(current)))return USS_PAIR_SERVICE_STOP;
    return uss_replacement_primary_ready(work,&current,now)?USS_PAIR_SERVICE_DONE:USS_PAIR_SERVICE_WAIT;
}
static UssPairServiceResult local_replacement_switch(void* user,const UssReplacementSnapshot* expected,
    const UssPairMember* member,UssPublicationStage stage)
{
    UssLocalReplacementAdapter* a=user;UssReplacementSnapshot entry,after;UssReplacementStage prior;unsigned i,found=0;
    UssFormationRuntime* r=a->native->runtime;uintptr_t base=formation_native_module(a->native);(void)stage;
    if(a->work->stage==USS_REPLACE_REPRESENT && pair_member_equal(member,&a->work->intent.kept))prior=USS_REPLACE_RECOVER;
    else if(a->work->stage==USS_REPLACE_DROP && pair_member_equal(member,&a->work->intent.released))prior=USS_REPLACE_REPRESENT;
    else if(a->work->stage==USS_REPLACE_PROMOTE && pair_member_equal(member,&a->work->intent.incoming))prior=USS_REPLACE_RESTORE_NEW;
    else goto fail;
    if(!base || !uss_replacement_primary_ready(a->work,expected,r->io.clock_ms(r->io.user)) ||
        !formation_native_entry(a->native,base,0x9aa290,uss_attachment_entry_pin,64) ||
        !local_replacement_entry(a,prior,expected,&entry))goto fail;
    for(i=0;i<3;++i)if(pair_member_equal(member,&entry.bundle.names.members[i])){
        const UssMemberEvidence* evidence=&entry.bundle.members[i];
        found=evidence->interaction_admissible && (!evidence->parent || evidence->parent==entry.bundle.current.inventory.avatar);}
    if(!found || !local_replacement_invoke(a,USS_REPLACEMENT_ATTACH,base+0x9aa290,0,0,&entry,member->entity) ||
        !local_replacement_capture(a,a->work,a->work->stage,&after) ||
        after.bundle.current.inventory.slots[2]!=member->entity || after.bundle.current.inventory.active_selector!=1 ||
        !uss_local_pair_accept(&r->local,&r->life.owner,&after.bundle.current))goto fail;
    return USS_PAIR_SERVICE_DONE;
fail:local_replacement_stop(a);return USS_PAIR_SERVICE_STOP;
}
static UssPairServiceResult local_replacement_drop(void* user,const UssReplacementSnapshot* expected)
{
    UssLocalReplacementAdapter* a=user;UssReplacementSnapshot entry,after;unsigned i;
    UssFormationRuntime* r=a->native->runtime;uintptr_t base=formation_native_module(a->native);
    if(!base || a->work->stage!=USS_REPLACE_HANDOVER || !uss_local_pair_owned(&r->local,&r->life.owner) ||
        r->local.member!=a->work->intent.released.entity ||
        !uss_replacement_primary_ready(a->work,expected,r->io.clock_ms(r->io.user)))goto fail;
    for(i=0;i<sizeof(uss_drop_control_pins)/sizeof(uss_drop_control_pins[0]);++i){
        const UssCapturePin* pin=&uss_drop_control_pins[i];
        if(pin->exe || !formation_native_entry(a->native,base,pin->rva,pin->bytes,pin->size))goto fail;
    }
    if(!local_replacement_entry(a,USS_REPLACE_DROP,expected,&entry) ||
        !local_replacement_invoke(a,USS_REPLACEMENT_DROP,base+0x9af090,0,0,&entry,3) ||
        !local_replacement_capture(a,a->work,USS_REPLACE_HANDOVER,&after) ||
        after.bundle.current.inventory.slots[2]!=after.native_empty || after.bundle.members[2].parent!=0)goto fail;
    return USS_PAIR_SERVICE_DONE;
fail:local_replacement_stop(a);return USS_PAIR_SERVICE_STOP;
}
static inline int uss_local_replacement_init(UssLocalReplacementAdapter* a,UssFormationNative* native,UssReplacementWork* work,
    const UssReplacementOrigin* origin,
    int (*invoke)(void*,UssReplacementEffect,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uint32_t,uint32_t,const UssInventoryMessage*),void* user)
{
    if(!a || !native || !native->runtime || !work || !origin || !origin->known || !invoke ||
        !uss_local_pair_owned(&native->runtime->local,&native->runtime->life.owner))return 0;
    memset(a,0,sizeof(*a));a->native=native;a->work=work;a->origin=*origin;a->invoke=invoke;a->invoke_user=user;return 1;
}
static inline UssReplacementServices uss_local_replacement_services(UssLocalReplacementAdapter* a)
{
    UssReplacementServices services={0};services.user=a;services.observe=local_replacement_observe;
    services.select_slot=local_replacement_select;services.primary_ready=local_replacement_ready;
    services.switch_member=local_replacement_switch;services.drop_slot=local_replacement_drop;
    services.local=&a->native->runtime->local;services.retained=formation_services(a->native->runtime);return services;
}
#endif
