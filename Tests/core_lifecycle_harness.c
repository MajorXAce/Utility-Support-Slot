#ifdef NDEBUG
#error Lifecycle assertions required
#endif
#define USS_CORE_TEST_MAIN observation_regression_checks
#include "core_observation_harness.c"
#include "../src/core_lifecycle.h"
static UssCoreLifecycle life;
static UssCoreObservation lifecycle_snapshot;
static UssMemberEvidence utility_evidence,ordinary_evidence;
static UssCoreObservation life_read(void)
{
    UssCorePlatform platform={0};UssCoreFlags flags=uss_lifecycle_flags(&life,&core_flags);
    platform.thread_id=core_thread;
    return uss_core_observe(&fake,MOD,&life.owner,&flags,&platform,&last_reader);
}
static int life_apply(UssCoreObservation* value)
{
    UssPairObservation observation;UssPublicationView view;UssPublicationOwner row;
    return uss_lifecycle_observe(&life,7,value,&observation,&view,&row);
}
static UssMemberEvidence named(uint32_t entity)
{
    UssMemberEvidence evidence={0};Reader memory={0};const unsigned char* identity;
    identity=fixture(entity==103?0x70002900:0x70001900,24);memory.api=&fake;memory.slot=-1;
    assert(member_fields(&memory,BASE,&lifecycle_snapshot.inventory,entity,identity,&evidence));return evidence;
}
static void life_fixture(UssPairOrder order)
{
    core_fixture();memset(&life,0,sizeof(life));
    put32(fixture(0x70000300,48)+8,order==USS_PAIR_ORDINARY_FIRST?103:102);
    lifecycle_snapshot=life_read();assert(lifecycle_snapshot.valid && life_apply(&lifecycle_snapshot));lifecycle_snapshot=life_read();
    utility_evidence=named(103);ordinary_evidence=named(102);
    if(order==USS_PAIR_ORDINARY_FIRST)ordinary_evidence.parent=0;else utility_evidence.parent=0;
}
static void create(UssPairOrder order,unsigned pickup)
{
    assert(uss_lifecycle_create(&life,7,&lifecycle_snapshot,&utility_evidence,&ordinary_evidence,order,pickup?3:1,pickup));
    assert(life.phase==USS_LIFE_PENDING && life.owner.pair_present);
    assert(!uss_lifecycle_flags(&life,&core_flags).formed && uss_lifecycle_flags(&life,&core_flags).formation_pending);
}
static UssPairTransaction completed(void)
{
    UssPairTransaction work={0};UssPublicationView view;UssPublicationOwner row;
    lifecycle_snapshot=life_read();assert(lifecycle_snapshot.valid);
    assert(uss_lifecycle_observe(&life,7,&lifecycle_snapshot,&work.origin,&view,&row));
    work.phase=USS_PAIR_COMPLETE;work.order=life.plan.order;work.real=lifecycle_snapshot.inventory.slots[2];
    work.selected=life.plan.selected;return work;
}
static void finish(void)
{
    UssPairTransaction work=completed();assert(uss_lifecycle_complete(&life,7,&lifecycle_snapshot,&work));
    assert(life.phase==USS_LIFE_FORMED && uss_lifecycle_flags(&life,&core_flags).formed);
}
#ifndef USS_LIFECYCLE_TEST_MAIN
#define USS_LIFECYCLE_TEST_MAIN main
#endif
int USS_LIFECYCLE_TEST_MAIN(void)
{
    unsigned order,pickup,i;uint64_t generation,guard;UssPairTransaction work;
    UssCoreFlags flags;UssCoreObservation stale;UssMemberEvidence incoming,old,u,o;
    assert(observation_regression_checks()==0);
    for(order=USS_PAIR_ORDINARY_FIRST;order<=USS_PAIR_UTILITY_FIRST;++order)for(pickup=0;pickup<=1;++pickup){
        life_fixture((UssPairOrder)order);create((UssPairOrder)order,pickup);generation=life.owner.pair_generation;
        assert(!uss_lifecycle_create(&life,7,&lifecycle_snapshot,&utility_evidence,&ordinary_evidence,(UssPairOrder)order,1,0));
        put32(fixture(0x70000300,48)+28,life.plan.selected);
        for(i=0;i<4;++i){lifecycle_snapshot=life_read();assert(lifecycle_snapshot.valid && life_apply(&lifecycle_snapshot));assert(life.owner.pair_generation==generation);}
        work=completed();work.phase=USS_PAIR_ACTIVE;assert(!uss_lifecycle_complete(&life,7,&lifecycle_snapshot,&work));
        work.phase=USS_PAIR_COMPLETE;++work.origin.pair_generation;assert(!uss_lifecycle_complete(&life,7,&lifecycle_snapshot,&work));
        finish();flags=uss_lifecycle_flags(&life,&core_flags);assert(flags.formed && !flags.formation_pending);
        generation=life.owner.pair_generation;guard=life.owner.guard_generation;
        fail_at=reads+1;lifecycle_snapshot=life_read();assert(!lifecycle_snapshot.valid && !life_apply(&lifecycle_snapshot));
        assert(life.phase==USS_LIFE_FORMED && life.owner.pair_generation==generation && !life.owner.ready);
        fail_at=0;lifecycle_snapshot=life_read();assert(lifecycle_snapshot.valid && life_apply(&lifecycle_snapshot));assert(life.owner.guard_generation==guard+1);
        assert(!uss_lifecycle_remove(&life,7,generation+1));assert(uss_lifecycle_remove(&life,7,generation));
        assert(!life.owner.pair_present && life.phase==USS_LIFE_EMPTY);
        assert(!uss_lifecycle_complete(&life,7,&lifecycle_snapshot,&work));
        lifecycle_snapshot=life_read();assert(lifecycle_snapshot.valid);
        create((UssPairOrder)order,pickup);assert(life.owner.pair_generation==generation+1);
        assert(!uss_lifecycle_complete(&life,7,&lifecycle_snapshot,&work));
    }
    life_fixture(USS_PAIR_ORDINARY_FIRST);lifecycle_snapshot.inventory.slots[1]=0;
    assert(!uss_lifecycle_create(&life,7,&lifecycle_snapshot,&utility_evidence,&ordinary_evidence,USS_PAIR_ORDINARY_FIRST,1,0));
    assert(!life.owner.pair_present);
    life_fixture(USS_PAIR_ORDINARY_FIRST);lifecycle_snapshot.flags.input=1;
    assert(!uss_lifecycle_create(&life,7,&lifecycle_snapshot,&utility_evidence,&ordinary_evidence,USS_PAIR_ORDINARY_FIRST,1,0));
    life_fixture(USS_PAIR_ORDINARY_FIRST);utility_evidence.parent=999;
    assert(!uss_lifecycle_create(&life,7,&lifecycle_snapshot,&utility_evidence,&ordinary_evidence,USS_PAIR_ORDINARY_FIRST,1,0));
    assert(!life.owner.pair_present);
    life_fixture(USS_PAIR_ORDINARY_FIRST);core_flags.formed=core_flags.formation_pending=1;
    flags=uss_lifecycle_flags(&life,&core_flags);assert(!flags.formed && !flags.formation_pending);
    create(USS_PAIR_ORDINARY_FIRST,0);guard=life.owner.guard_generation;
    assert(uss_lifecycle_cancel(&life,7,life.owner.pair_generation));assert(life.phase==USS_LIFE_PAUSED && life.owner.guard_generation==guard+1);
    assert(!uss_lifecycle_cancel(&life,7,life.owner.pair_generation));
    life_fixture(USS_PAIR_ORDINARY_FIRST);create(USS_PAIR_ORDINARY_FIRST,0);
    fail_at=reads+1;lifecycle_snapshot=life_read();assert(!life_apply(&lifecycle_snapshot));assert(life.phase==USS_LIFE_PAUSED);
    life_fixture(USS_PAIR_ORDINARY_FIRST);create(USS_PAIR_ORDINARY_FIRST,0);finish();
    /* Synthetic replacement evidence, not a native effect or a producer acceptance.
     * Reuse descriptor validation from real fixture reads, assigning a new id. */
    for(order=0;order<2;++order){
        life_fixture(USS_PAIR_ORDINARY_FIRST);create(USS_PAIR_ORDINARY_FIRST,0);finish();
        lifecycle_snapshot=life_read();incoming=named(order?102:103);old=incoming;
        incoming.entity=104;put32(incoming.descriptor+8,104);put32(incoming.interaction_descriptor+8,104);
        lifecycle_snapshot.inventory.slots[2]=104;memcpy(lifecycle_snapshot.inventory.identities[2],incoming.descriptor,24);
        generation=life.owner.pair_generation;
        assert(uss_lifecycle_replace_begin(&life,7,&lifecycle_snapshot,&incoming,1,0));assert(life.owner.pair_generation==generation);
        u=order?named(103):incoming;o=order?incoming:named(102);
        assert(!uss_lifecycle_replace_commit(&life,7,&lifecycle_snapshot,&u,&o,&old));assert(life.owner.pair_generation==generation);
        old.parent=0;stale=lifecycle_snapshot;
        lifecycle_snapshot.inventory.slots[2]=105;
        assert(!uss_lifecycle_replace_commit(&life,7,&lifecycle_snapshot,&u,&o,&old));
        assert(life.owner.pair_generation==generation);lifecycle_snapshot=stale;
        assert(uss_lifecycle_replace_commit(&life,7,&lifecycle_snapshot,&u,&o,&old));
        assert(life.owner.pair_generation==generation+1 && life.phase==USS_LIFE_PENDING);
        assert(!uss_lifecycle_replace_commit(&life,7,&stale,&u,&o,&old));
        assert(!uss_lifecycle_flags(&life,&core_flags).formed);
    }
    life_fixture(USS_PAIR_ORDINARY_FIRST);create(USS_PAIR_ORDINARY_FIRST,0);finish();
    putptr(fixture(BASE+0x346BF98,8),MAPPING+8);lifecycle_snapshot=life_read();assert(lifecycle_snapshot.valid && life_apply(&lifecycle_snapshot));
    assert(!life.owner.pair_present && life.phase==USS_LIFE_EMPTY);
    life_fixture(USS_PAIR_ORDINARY_FIRST);create(USS_PAIR_ORDINARY_FIRST,0);finish();
    core_state=HF_STATE_SHIP;put32(fixture(0xA0000000+0xAC21C,4),3);lifecycle_snapshot=life_read();assert(lifecycle_snapshot.valid && life_apply(&lifecycle_snapshot));
    assert(life.phase==USS_LIFE_EMPTY && !life.owner.mission);
    puts("Core lifecycle: explicit create/replace/remove, pending/completion distinction, stable polls, unknown/reset/cancel guards PASS");return 0;
}
