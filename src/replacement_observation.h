/* Fresh replacement evidence only. No native effects or SDK registration. */
#ifndef USS_REPLACEMENT_OBSERVATION_H
#define USS_REPLACEMENT_OBSERVATION_H
#include "replacement_transaction.h"
#include "replacement_dispatch.h"

typedef struct UssReplacementOriginEvidence {
    unsigned valid,thread;
    UssReplacementOrigin origin;
    UssPairOwnerContext context;
    UssPairMember utility,ordinary;
    uint32_t wires[4]; /* avatar, primary, old utility, old ordinary */
    unsigned char descriptors[4][24];
} UssReplacementOriginEvidence;

typedef struct UssReplacementProducer {
    const HfApi* api;HfMod* mod;UssCapturePlatform platform;
    const UssCoreLifecycle* life;const UssPickupHistory* history;
    const UssCoreFlags* flags;const UssReplacementWork* work;
    const UssReplacementOriginEvidence* origin;
    void* user;uint64_t (*clock_ms)(void*);
} UssReplacementProducer;

typedef struct UssReplacementControl {
    unsigned valid,entry_ready;
    UssReplacementSnapshot snapshot;
    UssReplacementOriginEvidence origin;
    UssReplacementWork work;
    uintptr_t game;size_t game_size;
    uint32_t wires[5]; /* avatar, primary, incoming, retained, outgoing */
    unsigned char descriptors[5][24];
    size_t core_calls,core_bytes,control_calls,control_bytes;
    size_t dispatch_fingerprint_size;
    unsigned char dispatch_fingerprint[USS_PUBLICATION_FINGERPRINT_BYTES];
    size_t fingerprint_size;
    unsigned char fingerprint[USS_PUBLICATION_FINGERPRINT_BYTES];
} UssReplacementControl;

/* Called while the OLD pair still represents the pre-pickup state. Reads fresh
 * inventory; a later incoming slot value is never used to reconstruct origin. */
static int uss_replacement_read_origin(const UssReplacementProducer* p,UssReplacementOriginEvidence* output)
{
    UssReplacementOriginEvidence result={0};UssCoreObservation current;Reader budget={0};
    UssCorePlatform platform;UssCoreFlags flags,original_flags;UssCoreLifecycle before;
    if(!output)return 0;memset(output,0,sizeof(*output));
    if(!p || !p->life || !p->flags || p->life->phase!=USS_LIFE_FORMED ||
        (p->work && p->work->phase==USS_REPLACE_ACTIVE))return 0;
    before=*p->life;original_flags=*p->flags;flags=uss_lifecycle_flags(p->life,&original_flags);
    platform.user=p->platform.user;platform.thread_id=p->platform.thread_id;
    current=uss_core_observe(p->api,p->mod,&before.owner,&flags,&platform,&budget);
    if(!lifecycle_current(&before,current.thread,&current) || !current.pair_matched ||
        !current.flags.eligible || current.flags.disabled || current.flags.suspended || current.flags.paused ||
        current.flags.down || current.flags.input || current.utility.parent!=current.inventory.avatar ||
        current.ordinary.parent!=current.inventory.avatar || current.inventory.active_selector<1 ||
        current.inventory.active_selector>3 ||
        (current.inventory.slots[2]!=before.owner.utility.entity && current.inventory.slots[2]!=before.owner.ordinary.entity) ||
        memcmp(&before,p->life,sizeof(before)) || memcmp(&original_flags,p->flags,sizeof(original_flags)))return 0;
    {UssCaptureReader mapping_reader={0};Inventory final_inventory;unsigned i;
        uint32_t roles[4]={current.inventory.avatar,current.inventory.slots[0],before.owner.utility.entity,before.owner.ordinary.entity};
        mapping_reader.memory=budget;
        for(i=0;i<4;++i)if(!capture_mapping(&mapping_reader,current.context.mapping,roles[i],result.descriptors[i],&result.wires[i]))return 0;
        if(memcmp(result.descriptors[2],current.utility.descriptor,24) || memcmp(result.descriptors[3],current.ordinary.descriptor,24) ||
            !inventory_sample(&mapping_reader.memory,&final_inventory) || memcmp(&final_inventory,&current.inventory,sizeof(final_inventory)) ||
            memcmp(&before,p->life,sizeof(before)) || memcmp(&original_flags,p->flags,sizeof(original_flags)))return 0;
    }
    result.origin.known=1;result.origin.real=current.inventory.slots[2];
    result.origin.selected=current.inventory.active_selector;
    result.origin.utility_drawn=result.origin.selected==3 && result.origin.real==before.owner.utility.entity;
    result.origin.epoch=before.owner.epoch;result.origin.context_generation=before.owner.context_generation;
    result.origin.pair_generation=before.owner.pair_generation;result.origin.guard_generation=before.owner.guard_generation;
    result.context=current.context;result.utility=before.owner.utility;result.ordinary=before.owner.ordinary;
    result.thread=current.thread;result.valid=1;*output=result;return 1;
}
static int replacement_origin_matches(const UssReplacementProducer* p)
{
    const UssReplacementOriginEvidence* o=p->origin;const UssReplacementWork* w=p->work;
    return o && o->valid && o->thread==p->life->owner.thread &&
        !memcmp(&o->origin,&w->origin,sizeof(o->origin)) && o->origin.known &&
        o->origin.real==w->intent.kept.entity && o->origin.selected>=1 && o->origin.selected<=3 &&
        o->origin.epoch==w->intent.epoch && o->origin.context_generation==w->intent.context_generation &&
        o->origin.pair_generation==w->intent.pair_generation && o->origin.guard_generation==w->intent.guard_generation &&
        pair_owner_context_equal(&o->context,&w->intent.context) &&
        pair_member_equal(&o->utility,&w->old_utility) && pair_member_equal(&o->ordinary,&w->old_ordinary);
}
static int replacement_capture_lifetime(const UssReplacementProducer* p,uint64_t now)
{
    const UssReplacementWork* w=p->work;
    uint64_t deadline=w->deadline_ms; /* Coordinator owns any child-deadline handover. */
    return w->phase==USS_REPLACE_ACTIVE &&
        (w->admitted || (w->stage==USS_REPLACE_PREPARE && p->life->phase==USS_LIFE_FORMED &&
            uss_pickup_intent_current(p->history,p->life,&w->intent))) && now>=w->started_ms && now<deadline &&
        replacement_origin_matches(p);
}
/* The initial Core sample already checks two independent graphs. At the end,
 * compare one freshly resolved COMPLETE graph to that owned sample instead of
 * taking an unrelated two-graph sample and then comparing it. Shared graph and
 * final_context retain all ownership/header/row checks and module revalidation. */
static int replacement_inventory_final(UssCaptureReader* reader,const Inventory* expected)
{
    uintptr_t globals[5],end_base;size_t end_size;Inventory current;
    if(!resolve(&reader->memory,reader->game,reader->game_size,globals) ||
        !graph(&reader->memory,globals,&current) || memcmp(expected,&current,sizeof(current)))return 0;
    end_base=reader->memory.api->module_base("game.dll",&end_size);
    return final_context(&reader->memory,expected,&current,reader->game,end_base,reader->game_size,end_size);
}
static int replacement_attached_dependencies(UssCaptureReader* reader,const UssReplacementSnapshot* snapshot)
{
    uintptr_t registry,component=0,relationships,rows;uint32_t count,i;
    if(!capture_pointer(reader,reader->game+0x3326E68,&registry,0) ||
        !capture_bytes(reader,registry+0x6C0,&count,4,0) || count>32)return 0;
    for(i=0;i<count;++i){uintptr_t at=registry+0x6C8+(uintptr_t)i*16;uint32_t kind;
        if(!capture_bytes(reader,at+8,&kind,4,0))return 0;
        if(kind==141 && (component || !capture_pointer(reader,at,&component,0)))return 0;
    }
    if(!component || !capture_pointer(reader,reader->game+0x3326698,&relationships,0) ||
        !capture_pointer(reader,relationships+0x38,&rows,0))return 0;
    /* The detached outgoing member after drop may acquire pickup interaction;
     * only attached members must have this held-weapon relationship shape. */
    for(i=0;i<3;++i)if(snapshot->bundle.members[i].parent==snapshot->bundle.current.inventory.avatar) {
        uint32_t absent,index,active;
        if(!(snapshot->bundle.members[i].descriptor[20]&1) ||
            capture_lookup(reader,component+0x20,snapshot->bundle.members[i].entity,&absent)!=0 || reader->memory.failure ||
            capture_lookup(reader,relationships+0x18,snapshot->bundle.members[i].entity,&index)!=1 || index>1048576 ||
            !capture_bytes(reader,rows+(uintptr_t)index*172,&active,4,0) || !active)return 0;
    }
    {const Inventory* inventory=&snapshot->bundle.current.inventory;
        uintptr_t units,unit_rows;uint32_t index,parent,absent,relation,active;
        if(!capture_pointer(reader,reader->game+0x3326DC0,&units,0) || units!=inventory->roots[4] ||
            capture_lookup(reader,units+0x20,inventory->slots[0],&index)!=1 || index>16383 ||
            !capture_pointer(reader,units+64,&unit_rows,0) ||
            !capture_bytes(reader,unit_rows+(uintptr_t)index*48+4,&parent,4,0) || parent!=inventory->avatar ||
            capture_lookup(reader,component+0x20,inventory->slots[0],&absent)!=0 || reader->memory.failure ||
            capture_lookup(reader,relationships+0x18,inventory->slots[0],&relation)!=1 || relation>1048576 ||
            !capture_bytes(reader,rows+(uintptr_t)relation*172,&active,4,0) || !active)return 0;
    }
    return 1;
}
static int replacement_payload_mappings(UssCaptureReader* reader,uintptr_t mapping,const UssReplacementSnapshot* snapshot)
{
    static const unsigned offsets[]={0,4,8,12,24};uint32_t seen[5];unsigned i,j,count=0;
    const Inventory* inventory=&snapshot->bundle.current.inventory;
    for(i=0;i<5;++i){unsigned char descriptor[24];uint32_t entity=u32(inventory->row+offsets[i]),wire;
        if(entity==snapshot->native_empty){if(i==0)return 0;continue;}
        if(!publication_id(entity) || entity==inventory->avatar)return 0;
        for(j=0;j<count;++j)if(seen[j]==entity)return 0;
        seen[count++]=entity;
        if(!capture_mapping(reader,mapping,entity,descriptor,&wire))return 0;
    }
    return 1;
}
/* Normal readiness uses the coordinator's current deadline (including its
 * separately bounded retained child). Only early replacement can use phase2;
 * retaining/restoring selection must use ordinary settled/holstering readiness. */
static int replacement_entry_ready(const UssReplacementWork* work,const UssReplacementSnapshot* snapshot,uint64_t now)
{
    if(!work || !snapshot || !snapshot->bundle.current.primary.valid || now>=work->deadline_ms)return 0;
    return snapshot->bundle.current.primary.ready ||
        (work->stage<=USS_REPLACE_PROMOTE && uss_replacement_primary_ready(work,snapshot,now));
}
/* Each output owns its evidence. No result is retained implicitly by the producer.
 * entry=0 observes readiness; entry=1 additionally checks strict drop dependencies.
 * Neither value binds an effect or grants native authority on its own. */
static int uss_replacement_capture(const UssReplacementProducer* p,unsigned entry,UssReplacementControl* output,Reader* diagnostics)
{
    UssReplacementControl result={0};UssCaptureReader reader={0};UssCoreScope scope={0},after_scope={0};
    UssCorePlatform core_platform;UssCoreCandidateNames names;UssCoreFlags flags,original_flags;
    UssCoreLifecycle life;UssPickupHistory history;UssReplacementWork work;UssReplacementOriginEvidence origin;
    UssPairMember members[3];UssPrimaryFields primary;uint64_t start,end;
    uintptr_t mapping;uint32_t sentinel,empty_wire;unsigned i;int ok=0;
    if(!output)return 0;memset(output,0,sizeof(*output));
    if(!p || !p->api || !p->mod || !p->life || !p->history || !p->flags || !p->work || !p->origin ||
        !p->clock_ms || !p->platform.thread_id || !p->platform.query || !p->platform.path || !p->platform.export_address || entry>1)goto done;
    start=p->clock_ms(p->user);
    if(!replacement_capture_lifetime(p,start))goto done;
    life=*p->life;history=*p->history;work=*p->work;origin=*p->origin;original_flags=*p->flags;
    flags=uss_lifecycle_flags(&life,&original_flags);core_platform.user=p->platform.user;core_platform.thread_id=p->platform.thread_id;
    members[0]=work.intent.incoming;members[1]=work.intent.kept;members[2]=work.intent.released;
    if(!uss_core_candidate_names(&life.owner,members,3,&names))goto done;
    result.snapshot.bundle=uss_core_observe_candidates(p->api,p->mod,&life.owner,&flags,&core_platform,&names,&reader.memory);
    if(!result.snapshot.bundle.valid)goto done;
    /* Core owns its complete decoded graph. Supplemental evidence owns separate
     * traces, but ALL supplemental reads share one unchanged 20k/2165 budget.
     * Explicit aggregate ceiling: two bounded passes, 40k bytes / 4330 reads.
     * No public reader limit changes; no truncation or cross-effect reuse. */
    result.core_calls=reader.memory.calls;result.core_bytes=reader.memory.bytes;
    reader.memory.calls=reader.memory.bytes=0;
    reader.platform=&p->platform;reader.game=p->api->module_base("game.dll",&reader.game_size);
    reader.memory.evidence=result.fingerprint;reader.memory.evidence_capacity=sizeof(result.fingerprint);reader.memory.evidence_size=0;
    reader.memory.stage="REPLACEMENT_CAPTURE";
    if(!core_scope(&reader.memory,p->api,p->mod,&scope) || !scope.mission || scope.game!=reader.game || scope.game_size!=reader.game_size ||
        !pair_owner_context_equal(&life.owner.context,&result.snapshot.bundle.current.context) ||
        !capture_bytes(&reader,reader.game+0x348456C,&sentinel,4,0) || publication_id(sentinel) ||
        !capture_bytes(&reader,reader.game+0x2154544,&empty_wire,4,0) || empty_wire!=32767)goto done;
    result.snapshot.empty_valid=1;result.snapshot.native_empty=sentinel;
    if(!replacement_guard(&work,&life,origin.thread,&history,&result.snapshot))goto done;
    if(entry && (!work.admitted || !replacement_entry_ready(&work,&result.snapshot,start) ||
        work.stage<USS_REPLACE_RECOVER || work.stage>USS_REPLACE_RETAINED_TICK || work.stage==USS_REPLACE_HANDOVER))goto done;
    mapping=scope.mapping;
    if(!capture_mapping(&reader,mapping,life.owner.context.local.avatar,result.descriptors[0],&result.wires[0]) ||
        !capture_mapping(&reader,mapping,result.snapshot.bundle.current.inventory.slots[0],result.descriptors[1],&result.wires[1]))goto done;
    if(memcmp(result.descriptors[1],result.snapshot.bundle.current.inventory.identities[0],8) || !(result.descriptors[1][20]&1))goto done;
    /* All three named roles, even when one is detached or slot3 is empty. */
    for(i=0;i<3;++i)if(!capture_mapping(&reader,mapping,members[i].entity,result.descriptors[i+2],&result.wires[i+2]) ||
        memcmp(result.descriptors[i+2],result.snapshot.bundle.members[i].descriptor,24))goto done;
    /* Old-role wires/descriptors belong to the pre-pickup lifetime, not merely
     * whatever happens to reverse-map successfully at this later stage. */
    for(i=0;i<2;++i)if(result.wires[i]!=origin.wires[i] || memcmp(result.descriptors[i],origin.descriptors[i],24))goto done;
    for(i=1;i<3;++i){unsigned old_index=members[i].entity==origin.utility.entity?2:3;
        if(result.wires[i+2]!=origin.wires[old_index] || memcmp(result.descriptors[i+2],origin.descriptors[old_index],24))goto done;
    }
    if(!replacement_payload_mappings(&reader,mapping,&result.snapshot))goto done;
    if(entry) {
        size_t observed_size;
        if(!replacement_attached_dependencies(&reader,&result.snapshot))goto done;
        observed_size=reader.memory.evidence_size;
        reader.memory.evidence=result.dispatch_fingerprint;reader.memory.evidence_size=0;
        if(!uss_capture_replacement_dispatch(&reader,scope.session))goto done;
        result.dispatch_fingerprint_size=reader.memory.evidence_size;
        reader.memory.evidence=result.fingerprint;reader.memory.evidence_size=observed_size;
        for(i=0;i<sizeof(capture_pins)/sizeof(capture_pins[0]);++i) {
            const UssCapturePin* pin=&capture_pins[i];
            if(!pin->exe && !capture_pin(&reader,reader.game+pin->rva,pin->bytes,pin->size))goto done;
        }
    }
    if(!replacement_inventory_final(&reader,&result.snapshot.bundle.current.inventory) ||
        !primary_fields(&reader.memory,reader.game,&result.snapshot.bundle.current.inventory,&primary) ||
        memcmp(&primary,&result.snapshot.bundle.current.primary.fields,sizeof(primary)))goto done;
    for(i=0;i<3;++i){UssMemberEvidence member;
        if(!member_fields(&reader.memory,reader.game,&result.snapshot.bundle.current.inventory,members[i].entity,members[i].identity,&member) ||
            memcmp(&member,&result.snapshot.bundle.members[i],sizeof(member)))goto done;
    }
    for(i=0;i<5;++i){unsigned char descriptor[24];uint32_t wire;
        uint32_t entity=i==0?life.owner.context.local.avatar:i==1?result.snapshot.bundle.current.inventory.slots[0]:members[i-2].entity;
        if(!capture_mapping(&reader,mapping,entity,descriptor,&wire) || wire!=result.wires[i] ||
            memcmp(descriptor,result.descriptors[i],24))goto done;
    }
    if(!capture_bytes(&reader,reader.game+0x348456C,&empty_wire,4,0) || empty_wire!=sentinel ||
        !core_scope(&reader.memory,p->api,p->mod,&after_scope) || memcmp(&scope,&after_scope,sizeof(scope)))goto done;
    end=p->clock_ms(p->user);
    if(end<start || !replacement_capture_lifetime(p,end) || (entry && !replacement_entry_ready(&work,&result.snapshot,end)) ||
        p->platform.thread_id(p->platform.user)!=origin.thread || memcmp(&life,p->life,sizeof(life)) ||
        memcmp(&history,p->history,sizeof(history)) || memcmp(&work,p->work,sizeof(work)) ||
        memcmp(&origin,p->origin,sizeof(origin)) || memcmp(&original_flags,p->flags,sizeof(original_flags)))goto done;
    result.game=reader.game;result.game_size=reader.game_size;result.origin=origin;result.work=work;
    result.control_calls=reader.memory.calls;result.control_bytes=reader.memory.bytes;
    result.entry_ready=entry;result.fingerprint_size=reader.memory.evidence_size;result.valid=1;*output=result;ok=1;
done:
    if(!ok && !reader.memory.failure)reader.memory.failure="REPLACEMENT_CAPTURE";
    reader.memory.evidence=NULL;reader.memory.evidence_capacity=0;if(diagnostics)*diagnostics=reader.memory;return ok;
}

typedef struct UssReplacementEntryEvidence {
    unsigned valid,consumed;
    UssReplacementControl captured;
} UssReplacementEntryEvidence;
static int replacement_controls_equal(const UssReplacementControl* a,const UssReplacementControl* b)
{
    return a->valid && b->valid && a->entry_ready && b->entry_ready &&
        a->fingerprint_size<=sizeof(a->fingerprint) && b->fingerprint_size<=sizeof(b->fingerprint) &&
        a->dispatch_fingerprint_size<=sizeof(a->dispatch_fingerprint) && b->dispatch_fingerprint_size<=sizeof(b->dispatch_fingerprint) &&
        a->game==b->game && a->game_size==b->game_size &&
        !memcmp(&a->snapshot,&b->snapshot,sizeof(a->snapshot)) && !memcmp(&a->origin,&b->origin,sizeof(a->origin)) &&
        !memcmp(&a->work,&b->work,sizeof(a->work)) && !memcmp(a->wires,b->wires,sizeof(a->wires)) &&
        !memcmp(a->descriptors,b->descriptors,sizeof(a->descriptors)) &&
        a->fingerprint_size==b->fingerprint_size && a->dispatch_fingerprint_size==b->dispatch_fingerprint_size &&
        !memcmp(a->fingerprint,b->fingerprint,a->fingerprint_size) &&
        !memcmp(a->dispatch_fingerprint,b->dispatch_fingerprint,a->dispatch_fingerprint_size);
}
/* Two independent whole captures, not two booleans from one cached observation.
 * Result is preparatory evidence only: the later real binder must consume it
 * immediately before its one reviewed call, with no intervening callback/work. */
static inline int uss_replacement_capture_entry(const UssReplacementProducer* p,UssReplacementEntryEvidence* output)
{
    UssReplacementEntryEvidence result={0};UssReplacementControl second;
    if(!output)return 0;memset(output,0,sizeof(*output));
    if(!uss_replacement_capture(p,1,&result.captured,NULL) || !uss_replacement_capture(p,1,&second,NULL) ||
        !replacement_controls_equal(&result.captured,&second))return 0;
    result.valid=1;*output=result;return 1;
}
/* One-shot accounting, NO native call. A failed consume also retires the ticket.
 * It cannot be re-used after work advances, even to an identically shaped pair. */
static int uss_replacement_consume_entry(const UssReplacementProducer* p,UssReplacementEntryEvidence* entry)
{
    const UssReplacementControl* captured;uint64_t now;
    if(!entry || entry->consumed)return 0;entry->consumed=1;
    if(!p || !p->work || !p->life || !p->history || !p->origin || !p->flags || !p->clock_ms ||
        !p->platform.thread_id || !entry->valid)return 0;
    captured=&entry->captured;now=p->clock_ms(p->user);
    return captured->valid && captured->entry_ready && replacement_capture_lifetime(p,now) &&
        replacement_entry_ready(p->work,&captured->snapshot,now) && !memcmp(p->work,&captured->work,sizeof(*p->work)) &&
        !memcmp(p->origin,&captured->origin,sizeof(*p->origin)) &&
        p->flags->known && p->flags->eligible && !p->flags->input && !p->flags->disabled && !p->flags->suspended && !p->flags->paused && !p->flags->down &&
        replacement_guard(p->work,p->life,captured->origin.thread,p->history,&captured->snapshot) &&
        p->platform.thread_id(p->platform.user)==captured->origin.thread;
}
/* Read-only postcondition for the consumed DROP boundary. Keep the REAL Core
 * work at DROP while its effect service verifies the result; use HANDOVER only
 * as the expected post-drop read shape. No logical membership changes here. */
static int uss_replacement_verify_drop(const UssReplacementProducer* p,const UssReplacementEntryEvidence* entry,
    UssReplacementControl* output)
{
    UssReplacementProducer after;UssReplacementWork actual,expected;UssReplacementControl fresh;
    unsigned i;
    if(output)memset(output,0,sizeof(*output));
    if(!p || !p->work || !output || !entry || !entry->valid || !entry->consumed ||
        !entry->captured.valid || entry->captured.work.stage!=USS_REPLACE_DROP ||
        memcmp(p->work,&entry->captured.work,sizeof(*p->work)))return 0;
    actual=*p->work;expected=actual;expected.stage=USS_REPLACE_HANDOVER;after=*p;after.work=&expected;
    if(!uss_replacement_capture(&after,0,&fresh,NULL) || memcmp(p->work,&actual,sizeof(actual)) ||
        fresh.game!=entry->captured.game || fresh.game_size!=entry->captured.game_size ||
        fresh.snapshot.native_empty!=entry->captured.snapshot.native_empty ||
        memcmp(&fresh.origin,&entry->captured.origin,sizeof(fresh.origin)))return 0;
    for(i=0;i<5;++i)if(fresh.wires[i]!=entry->captured.wires[i] ||
        memcmp(fresh.descriptors[i],entry->captured.descriptors[i],24))return 0;
    *output=fresh;return 1;
}
#endif
