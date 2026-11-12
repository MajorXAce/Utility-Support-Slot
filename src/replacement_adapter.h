/* Finite replacement services. Explicit invoker only; no production targets,
 * callback registration or runtime installer is provided by this header. */
#ifndef USS_REPLACEMENT_ADAPTER_H
#define USS_REPLACEMENT_ADAPTER_H
#include "replacement_observation.h"
#include "selection_entry_pins.h"
#include "attachment_entry_pin.h"
#include "native_calls.h"

typedef enum UssReplacementEffect {USS_REPLACEMENT_SELECT=1,USS_REPLACEMENT_ATTACH,USS_REPLACEMENT_DROP,USS_REPLACEMENT_SEND} UssReplacementEffect;
typedef struct UssReplacementAdapter {
    UssReplacementProducer producer;
    UssPublicationState* publication;UssCoreLifecycle* life;
    uintptr_t anchor_game;size_t anchor_game_size;uint64_t effect_attempts;
    void* invoke_user;
    int (*invoke)(void*,UssReplacementEffect,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uint32_t,uint32_t,const UssInventoryMessage*);
    unsigned anchor_valid,failed,in_effect,effects,pub_busy,pub_captures;
    uint64_t anchor_serial;
    uint32_t wires[5];unsigned char descriptors[5][24];
    UssReplacementControl latest,pub_first;
    UssReplacementEntryEvidence pub_entry;
    UssPairObservation pair;UssPublicationView view;UssPublicationOwner row;
    const char* reason;
} UssReplacementAdapter;
static int replacement_adapter_stop(UssReplacementAdapter* a,const char* why)
{if(a){a->failed=1;a->reason=why;}return 0;}
static int replacement_adapter_maps(UssReplacementAdapter* a,const UssReplacementControl* c)
{
    if(!a->anchor_valid){a->anchor_valid=1;a->anchor_serial=c->work.intent.serial;
        a->anchor_game=c->game;a->anchor_game_size=c->game_size;
        memcpy(a->wires,c->wires,sizeof(a->wires));memcpy(a->descriptors,c->descriptors,sizeof(a->descriptors));}
    return a->anchor_game==c->game && a->anchor_game_size==c->game_size && a->anchor_serial==c->work.intent.serial && !memcmp(a->wires,c->wires,sizeof(a->wires)) &&
        !memcmp(a->descriptors,c->descriptors,sizeof(a->descriptors));
}
/* Effective stage describes the physical BEFORE/AFTER shape. The REAL work has
 * already advanced/consumed its effect bit; compare it independently throughout. */
static int replacement_adapter_capture(UssReplacementAdapter* a,const UssReplacementWork* requested,
    UssReplacementStage stage,unsigned entry,UssReplacementControl* out)
{
    UssReplacementProducer p;UssReplacementWork actual,effective;int ok;
    if(!a || a->failed || a->in_effect || !a->producer.work || !requested || !out)return 0;
    actual=*a->producer.work;effective=*requested;effective.stage=stage;p=a->producer;p.work=&effective;
    ok=uss_replacement_capture(&p,entry,out,NULL);
    return ok && !memcmp(&actual,a->producer.work,sizeof(actual)) && replacement_adapter_maps(a,out);
}
static int replacement_adapter_entry(UssReplacementAdapter* a,UssReplacementStage stage,
    const UssReplacementSnapshot* expected,UssReplacementEntryEvidence* out)
{
    UssReplacementControl second;
    memset(out,0,sizeof(*out));
    if(!replacement_adapter_capture(a,a->producer.work,stage,1,&out->captured) ||
        (expected && memcmp(expected,&out->captured.snapshot,sizeof(*expected))) ||
        !replacement_adapter_capture(a,a->producer.work,stage,1,&second) ||
        !replacement_controls_equal(&out->captured,&second))return 0;
    out->valid=1;return 1;
}
static int replacement_adapter_consume(UssReplacementAdapter* a,UssReplacementEntryEvidence* e)
{
    UssReplacementWork effective=*a->producer.work;UssReplacementProducer p=a->producer;
    /* Keep real consumed bits/deadlines, changing only the expected physical stage. */
    effective.stage=e->captured.work.stage;p.work=&effective;
    return uss_replacement_consume_entry(&p,e);
}
static int replacement_adapter_pin(UssReplacementAdapter* a,uintptr_t game,uint32_t rva,const unsigned char* bytes,size_t n)
{
    UssCaptureReader reader={0};UssCapturePage before,after;
    reader.memory.api=a->producer.api;reader.platform=&a->producer.platform;
    return n && n<=4096 && capture_query(&reader,game+rva,&before) &&
        capture_page_executable(&before,game,game+rva,n) && capture_pin(&reader,game+rva,bytes,n) &&
        capture_query(&reader,game+rva,&after) && !memcmp(&before,&after,sizeof(before));
}
static int replacement_adapter_invoke(UssReplacementAdapter* a,UssReplacementEffect effect,uintptr_t first,uintptr_t second,
    uintptr_t record,uintptr_t inventory,uint32_t avatar,uint32_t value,const UssInventoryMessage* message)
{
    int returned;unsigned effect_index;UssReplacementWork actual=*a->producer.work;
    if(a->failed || a->in_effect || !a->invoke)return replacement_adapter_stop(a,"INVOKER_OR_REENTRY");
    effect_index=(unsigned)actual.stage*6+(unsigned)effect;
    if(actual.stage==USS_REPLACE_RETAINED_TICK && effect==USS_REPLACEMENT_SELECT &&
        actual.retained.index>=actual.retained.step_count)effect_index=(unsigned)actual.stage*6+5;
    if(effect_index>=64 || (a->effect_attempts&(UINT64_C(1)<<effect_index)))return replacement_adapter_stop(a,"EFFECT_ALREADY_CONSUMED");
    a->effect_attempts|=UINT64_C(1)<<effect_index;
    a->in_effect=1;++a->effects; /* Consumed before callback, including uncertainty. */
    returned=a->invoke(a->invoke_user,effect,first,second,record,inventory,avatar,value,message);
    a->in_effect=0;
    if(!returned || memcmp(&actual,a->producer.work,sizeof(actual)))return replacement_adapter_stop(a,"EFFECT_UNCERTAIN");
    return 1;
}
static UssReplacementSnapshot replacement_adapter_observe(void* user,const UssReplacementWork* work)
{
    UssReplacementAdapter* a=user;UssReplacementSnapshot empty={0};
    if(!replacement_adapter_capture(a,work,work->stage,0,&a->latest))return empty;
    return a->latest.snapshot;
}
static int replacement_adapter_refresh(UssReplacementAdapter* a)
{
    return replacement_adapter_capture(a,a->producer.work,a->producer.work->stage,0,&a->latest) &&
        uss_lifecycle_observe(a->life,a->latest.origin.thread,&a->latest.snapshot.bundle.current,
            &a->pair,&a->view,&a->row);
}
static int replacement_adapter_pair_same(const UssPairObservation* x,const UssPairObservation* y)
{
    return x && y && x->valid && y->valid && x->epoch==y->epoch && x->pair_generation==y->pair_generation &&
        x->guard_generation==y->guard_generation && pair_context_equal(&x->context,&y->context) &&
        pair_member_equal(&x->utility,&y->utility) && pair_member_equal(&x->ordinary,&y->ordinary) &&
        x->support==y->support && x->active==y->active;
}
static UssPairServiceResult replacement_adapter_select(void* user,const UssReplacementSnapshot* expected,uint32_t slot)
{
    UssReplacementAdapter* a=user;UssReplacementStage stage=a->producer.work->stage;
    UssReplacementControl before,entry;UssSelectionAddresses addresses;Reader reader={0};
    if(stage==USS_REPLACE_RECOVER)stage=USS_REPLACE_PREPARE;
    else if(stage==USS_REPLACE_VERIFY)stage=USS_REPLACE_SELECT_FINAL;
    if(slot<1 || slot>3 || !replacement_adapter_capture(a,a->producer.work,stage,0,&before) ||
        memcmp(expected,&before.snapshot,sizeof(*expected)) ||
        !publication_id(before.snapshot.bundle.current.inventory.slots[slot-1]) ||
        before.snapshot.bundle.current.primary.fields.global_guard || before.snapshot.bundle.current.primary.fields.state_byte)
        goto fail;
    reader.api=a->producer.api;
    if(!uss_selection_entry_pins(&reader,before.game,before.game_size,&addresses))goto fail;
    {UssCapturePage page;
        if(!a->producer.platform.query(a->producer.platform.user,addresses.state,&page) ||
            !capture_page_executable(&page,before.game,addresses.state,64) ||
            !a->producer.platform.query(a->producer.platform.user,addresses.weapon,&page) ||
            !capture_page_executable(&page,before.game,addresses.weapon,64))goto fail;
    }
    if(!replacement_adapter_capture(a,a->producer.work,stage,0,&entry) ||
        memcmp(&before.snapshot,&entry.snapshot,sizeof(before.snapshot)))goto fail;
    if(!replacement_adapter_invoke(a,USS_REPLACEMENT_SELECT,addresses.state,addresses.weapon,
        entry.snapshot.bundle.current.primary.fields.action.record,entry.snapshot.bundle.current.inventory.roots[2],
        entry.snapshot.bundle.current.inventory.avatar,slot,NULL))goto fail;
    if(!replacement_adapter_capture(a,a->producer.work,a->producer.work->stage,0,&a->latest) ||
        a->latest.snapshot.bundle.current.inventory.active_selector!=slot ||
        a->latest.snapshot.bundle.current.inventory.slots[2]!=entry.snapshot.bundle.current.inventory.slots[2])goto fail;
    return USS_PAIR_SERVICE_DONE;
fail:replacement_adapter_stop(a,"SELECTION_UNVERIFIED");return USS_PAIR_SERVICE_STOP;
}
static UssPairServiceResult replacement_adapter_ready(void* user,const UssReplacementWork* work,
    const UssReplacementSnapshot* expected,uint64_t now)
{
    UssReplacementAdapter* a=user;(void)now;
    if(!replacement_adapter_capture(a,work,work->stage,0,&a->latest) || memcmp(expected,&a->latest.snapshot,sizeof(*expected)))return USS_PAIR_SERVICE_STOP;
    return replacement_entry_ready(work,&a->latest.snapshot,a->producer.clock_ms(a->producer.user))?USS_PAIR_SERVICE_DONE:USS_PAIR_SERVICE_WAIT;
}
static uint64_t replacement_adapter_clock(void* user)
{UssReplacementAdapter* a=user;return a->producer.clock_ms(a->producer.user);}
static UssPublicationView replacement_adapter_view(void* user)
{
    UssReplacementAdapter* a=user;UssPublicationView empty={0};
    return replacement_adapter_refresh(a)?a->view:empty;
}
static int replacement_adapter_project(UssReplacementAdapter* a,const UssReplacementControl* c,UssPublicationCapture* out)
{
    UssPublicationCapture value={0};UssPublicationKey* key=&value.key;const UssCoreObservation* current=&c->snapshot.bundle.current;
    const UssPairOwner* owner=&a->producer.life->owner;unsigned i;int utility=-1,ordinary=-1;
    if(!c->valid || !c->entry_ready || current->inventory.active_selector!=1 ||
        (current->inventory.slots[2]!=owner->utility.entity && current->inventory.slots[2]!=owner->ordinary.entity))return 0;
    for(i=0;i<3;++i){uint32_t entity=c->snapshot.bundle.names.members[i].entity;
        if(entity==owner->utility.entity)utility=(int)i+2;if(entity==owner->ordinary.entity)ordinary=(int)i+2;}
    if(utility<0 || ordinary<0 || current->utility.parent!=current->inventory.avatar || current->ordinary.parent!=current->inventory.avatar)return 0;
    key->pair_generation=owner->pair_generation;key->context_generation=owner->context_generation;
    key->session=current->context.session;key->mapping=current->context.mapping;key->owner=current->context.local.owner;
    key->inventory=current->context.local.inventory;key->avatar=current->inventory.avatar;key->utility=owner->utility.entity;key->ordinary=owner->ordinary.entity;
    memcpy(key->utility_identity,owner->utility.identity,8);memcpy(key->ordinary_identity,owner->ordinary.identity,8);
    key->peer_size=current->context.peer_size;memcpy(key->peers,current->context.peers,key->peer_size);
    value.owner.owner=key->owner;value.owner.inventory=key->inventory;value.owner.avatar=key->avatar;
    value.owner.address=current->inventory.row_address;memcpy(value.owner.row,current->inventory.row,48);
    value.thread=current->thread;value.wires[0]=c->wires[0];value.wires[1]=c->wires[1];value.wires[2]=c->wires[utility];value.wires[3]=c->wires[ordinary];
    /* Full traces remain owned in pub_first/pub_entry and are compared THERE.
     * This small public projection is not a truncated trace or hash substitute. */
    value.fingerprint_size=48;memcpy(value.fingerprint,value.owner.row,48);value.valid=1;*out=value;return 1;
}
static int replacement_adapter_pub_capture(void* user,UssPublicationStage stage,UssPublicationCapture* out)
{
    UssReplacementAdapter* a=user;(void)stage;
    if(!a->pub_busy || a->pub_captures>=2 || !replacement_adapter_capture(a,a->producer.work,a->producer.work->stage,1,&a->latest))return 0;
    if(!a->pub_captures)a->pub_first=a->latest;
    else {
        if(!replacement_controls_equal(&a->pub_first,&a->latest))return 0;
        a->pub_entry.captured=a->latest;a->pub_entry.valid=1;
    }
    ++a->pub_captures;return replacement_adapter_project(a,&a->latest,out);
}
static UssPublicationSend replacement_adapter_send(void* user,UssPublicationKind kind,const UssPublicationCapture* capture,uint64_t metadata)
{
    UssReplacementAdapter* a=user;UssInventoryMessage message;UssPublicationCapture projected;uint64_t actual;
    const UssCapturePin* pin=&capture_pins[0];(void)kind;
    if(!a->pub_busy || a->pub_captures!=2 || !a->pub_entry.valid ||
        !replacement_adapter_project(a,&a->pub_entry.captured,&projected) ||
        !publication_key_equal(&projected.key,&capture->key) || !publication_owner_equal(&projected.owner,&capture->owner) ||
        pin->exe || pin->rva!=0xBE3F40 || !replacement_adapter_pin(a,a->pub_entry.captured.game,pin->rva,pin->bytes,pin->size) ||
        !replacement_adapter_consume(a,&a->pub_entry))return USS_PUBLICATION_SEND_NOT_ENTERED;
    memcpy(&actual,capture->owner.row+40,8);if(actual!=metadata)return USS_PUBLICATION_SEND_NOT_ENTERED;
    message.avatar=capture->owner.avatar;message.primary=u32(capture->owner.row);message.secondary=u32(capture->owner.row+4);
    message.support=u32(capture->owner.row+8);message.backpack=u32(capture->owner.row+12);message.row_field24=u32(capture->owner.row+24);message.metadata=metadata;
    if(!replacement_adapter_invoke(a,USS_REPLACEMENT_SEND,a->pub_entry.captured.game+pin->rva,0,0,0,0,0,&message) ||
        !replacement_adapter_refresh(a) || !publication_owner_equal(&capture->owner,&a->row) ||
        !publication_key_equal(&capture->key,&a->view.key))return USS_PUBLICATION_SEND_UNCERTAIN;
    return USS_PUBLICATION_SEND_RETURNED;
}
static UssPairServiceResult replacement_adapter_publish(UssReplacementAdapter* a,UssPublicationStage stage,unsigned allow_wait)
{
    UssPublicationServices services={0};UssPublicationResult result;UssPublicationOwner caller;
    if(a->pub_busy || !replacement_adapter_refresh(a))return USS_PAIR_SERVICE_STOP;
    caller=a->row;a->pub_busy=1;a->pub_captures=0;memset(&a->pub_entry,0,sizeof(a->pub_entry));
    services.user=a;services.clock_ms=replacement_adapter_clock;services.view=replacement_adapter_view;
    services.capture=replacement_adapter_pub_capture;services.send=replacement_adapter_send;
    result=uss_publication_boundary(a->publication,&services,&caller,0,stage);a->pub_busy=0;
    if(result.outcome==USS_PUBLICATION_RETURNED || result.outcome==USS_PUBLICATION_UNCHANGED)return USS_PAIR_SERVICE_DONE;
    if(allow_wait && result.outcome==USS_PUBLICATION_REFUSED && !a->failed && !a->publication->disabled &&
        (result.reason==USS_PUB_CAPTURE || result.reason==USS_PUB_RATE_LIMIT || result.reason==USS_PUB_ENTRY_CHANGED))return USS_PAIR_SERVICE_WAIT;
    replacement_adapter_stop(a,"PUBLICATION_UNCERTAIN_OR_REFUSED");return USS_PAIR_SERVICE_STOP;
}
static UssPairServiceResult replacement_adapter_attach(UssReplacementAdapter* a,const UssReplacementSnapshot* expected,
    const UssPairMember* member,UssReplacementStage before_stage)
{
    UssReplacementEntryEvidence entry;unsigned i;int found=0;
    if(!replacement_adapter_pin(a,a->latest.game,0x9AA290,uss_attachment_entry_pin,64) ||
        !replacement_adapter_entry(a,before_stage,expected,&entry))goto fail;
    for(i=0;i<3;++i)if(pair_member_equal(member,&entry.captured.snapshot.bundle.names.members[i])) {
        const UssMemberEvidence* evidence=&entry.captured.snapshot.bundle.members[i];
        found=evidence->interaction_admissible && (!evidence->parent || evidence->parent==entry.captured.snapshot.bundle.current.inventory.avatar);
    }
    if(!found || !replacement_adapter_consume(a,&entry) || !replacement_adapter_invoke(a,USS_REPLACEMENT_ATTACH,
        entry.captured.game+0x9AA290,0,0,entry.captured.snapshot.bundle.current.inventory.roots[2],
        entry.captured.snapshot.bundle.current.inventory.avatar,member->entity,NULL) || !replacement_adapter_refresh(a) ||
        a->latest.snapshot.bundle.current.inventory.slots[2]!=member->entity || a->latest.snapshot.bundle.current.inventory.active_selector!=1)goto fail;
    return USS_PAIR_SERVICE_DONE;
fail:replacement_adapter_stop(a,"ATTACHMENT_UNCERTAIN");return USS_PAIR_SERVICE_STOP;
}
static UssPairServiceResult replacement_adapter_switch(void* user,const UssReplacementSnapshot* expected,const UssPairMember* member,UssPublicationStage stage)
{
    UssReplacementAdapter* a=user;UssReplacementStage prior;
    if(a->producer.work->stage==USS_REPLACE_REPRESENT && pair_member_equal(member,&a->producer.work->intent.kept))prior=USS_REPLACE_RECOVER;
    else if(a->producer.work->stage==USS_REPLACE_DROP && pair_member_equal(member,&a->producer.work->intent.released))prior=USS_REPLACE_REPRESENT;
    else if(a->producer.work->stage==USS_REPLACE_PROMOTE && pair_member_equal(member,&a->producer.work->intent.incoming))prior=USS_REPLACE_RESTORE_NEW;
    else return USS_PAIR_SERVICE_STOP;
    return replacement_adapter_attach(a,expected,member,prior)==USS_PAIR_SERVICE_DONE?replacement_adapter_publish(a,stage,0):USS_PAIR_SERVICE_STOP;
}
static UssPairServiceResult replacement_adapter_drop(void* user,const UssReplacementSnapshot* expected)
{
    UssReplacementAdapter* a=user;UssReplacementEntryEvidence entry;UssReplacementWork effective;UssReplacementProducer p;
    if(a->producer.work->stage!=USS_REPLACE_HANDOVER || !replacement_record_matches(a->producer.life,a->publication) ||
        a->publication->member!=a->producer.work->intent.released.entity ||
        !replacement_adapter_entry(a,USS_REPLACE_DROP,expected,&entry) || !replacement_adapter_consume(a,&entry))goto fail;
    if(!replacement_adapter_invoke(a,USS_REPLACEMENT_DROP,entry.captured.game+0x9AF090,0,0,
        entry.captured.snapshot.bundle.current.inventory.roots[2],entry.captured.snapshot.bundle.current.inventory.avatar,3,NULL))goto fail;
    effective=*a->producer.work;effective.stage=USS_REPLACE_DROP;p=a->producer;p.work=&effective;
    if(!uss_replacement_verify_drop(&p,&entry,&a->latest) || !replacement_adapter_maps(a,&a->latest))goto fail;
    return USS_PAIR_SERVICE_DONE;
fail:replacement_adapter_stop(a,"DROP_UNCERTAIN");return USS_PAIR_SERVICE_STOP;
}
static UssPairObservation replacement_adapter_retained_observe(void* user)
{UssReplacementAdapter* a=user;UssPairObservation empty={0};return replacement_adapter_refresh(a)?a->pair:empty;}
static UssPairServiceResult replacement_adapter_retained_select(void* user,const UssPairObservation* expected,uint32_t slot)
{UssReplacementAdapter* a=user;if(!replacement_adapter_refresh(a) || !replacement_adapter_pair_same(expected,&a->pair))return USS_PAIR_SERVICE_STOP;
 return replacement_adapter_select(a,&a->latest.snapshot,slot);}
static UssPairServiceResult replacement_adapter_retained_ready(void* user,const UssPairObservation* expected)
{UssReplacementAdapter* a=user;if(!replacement_adapter_refresh(a) || !replacement_adapter_pair_same(expected,&a->pair))return USS_PAIR_SERVICE_STOP;
 return a->latest.snapshot.bundle.current.primary.ready?USS_PAIR_SERVICE_DONE:USS_PAIR_SERVICE_WAIT;}
static UssPairServiceResult replacement_adapter_retained_switch(void* user,const UssPairObservation* expected,const UssPairStep* step,unsigned defer_ack)
{UssReplacementAdapter* a=user;(void)defer_ack;
 if(!replacement_adapter_refresh(a) || !replacement_adapter_pair_same(expected,&a->pair))return USS_PAIR_SERVICE_STOP;
 return replacement_adapter_attach(a,&a->latest.snapshot,&step->member,USS_REPLACE_RETAINED_TICK);}
static UssPairServiceResult replacement_adapter_retained_publish(void* user,const UssPairObservation* expected,const UssPairStep* step)
{UssReplacementAdapter* a=user;(void)step;
 if(!replacement_adapter_refresh(a) || !replacement_adapter_pair_same(expected,&a->pair))return USS_PAIR_SERVICE_STOP;
 return replacement_adapter_publish(a,USS_PUBLICATION_POST_ATTACH,1);}
static int replacement_adapter_verify(void* user,const UssPairObservation* expected)
{UssReplacementAdapter* a=user;return replacement_adapter_refresh(a) && replacement_adapter_pair_same(expected,&a->pair) &&
 a->latest.snapshot.bundle.current.utility.parent_matches && a->latest.snapshot.bundle.current.ordinary.parent_matches;}
static UssReplacementServices uss_replacement_adapter_services(UssReplacementAdapter* a)
{
    UssReplacementServices s={0};s.user=a;s.observe=replacement_adapter_observe;s.select_slot=replacement_adapter_select;
    s.primary_ready=replacement_adapter_ready;s.switch_member=replacement_adapter_switch;s.drop_slot=replacement_adapter_drop;
    s.retained.user=a;s.retained.observe=replacement_adapter_retained_observe;s.retained.select_slot=replacement_adapter_retained_select;
    s.retained.primary_ready=replacement_adapter_retained_ready;s.retained.switch_member=replacement_adapter_retained_switch;
    s.retained.publish_boundary=replacement_adapter_retained_publish;s.retained.verify_pair=replacement_adapter_verify;return s;
}
/* The reader borrows a const view; the adapter's Core-observe application has an
 * explicit mutable owner. Construction registers nothing and binds no game code. */
static int uss_replacement_adapter_init(UssReplacementAdapter* a,const UssReplacementProducer* producer,
    UssCoreLifecycle* life,UssPublicationState* publication,
    int (*invoke)(void*,UssReplacementEffect,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uint32_t,uint32_t,const UssInventoryMessage*),void* user)
{
    if(!a || !producer || !life || producer->life!=life || !publication || !invoke || !producer->origin ||
        !producer->origin->valid || !producer->work || !producer->history || !producer->flags ||
        !producer->clock_ms || !producer->api || !producer->mod || !uss_sdk_compatible(producer->api) ||
        !producer->api->read || !producer->api->module_base || !producer->api->build_id || !producer->api->session || !producer->api->game_state ||
        !producer->platform.query || !producer->platform.thread_id || !producer->platform.path || !producer->platform.export_address ||
        publication->disabled || publication->suspended ||
        (producer->work->phase!=USS_REPLACE_IDLE &&
            !(producer->work->phase==USS_REPLACE_ACTIVE && producer->work->stage==USS_REPLACE_PREPARE)))return 0;
    memset(a,0,sizeof(*a));a->producer=*producer;a->publication=publication;a->life=life;
    a->invoke=invoke;a->invoke_user=user;a->reason="READY_OFFLINE_SERVICES";return 1;
}
#endif
