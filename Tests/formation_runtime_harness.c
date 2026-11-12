/* Exercise actual bounded readers with simulated engine effects, never game calls. */
#define USS_HISTORY_TEST_MAIN history_regression_checks
#include "pickup_history_harness.c"
#include "../src/formation_native.h"
static UssFormationRuntime fixture_runtime;
static unsigned selections,attachments,sends,capture_block,attach_stop,send_uncertain;
static uint64_t now_ms;
static uint64_t runtime_clock(void* user){(void)user;return now_ms;}
static UssCoreObservation runtime_observe(void* user,const UssPairOwner* owner,const UssCoreFlags* flags)
{
    UssCorePlatform platform={0};(void)user;platform.thread_id=core_thread;
    return uss_core_observe(&fake,MOD,owner,flags,&platform,NULL);
}
static int runtime_input(void* user,UssCaptureInput* out)
{
    const UssFormationRuntime* r=user;memset(out,0,sizeof(*out));
    out->pair_generation=r->life.owner.pair_generation;out->context_generation=r->life.owner.context_generation;
    out->owner=r->latest.inventory;out->utility=r->life.owner.utility.entity;out->ordinary=r->life.owner.ordinary.entity;
    out->callback_thread=r->thread;memcpy(out->utility_identity,r->life.owner.utility.identity,8);
    memcpy(out->ordinary_identity,r->life.owner.ordinary.identity,8);return 1;
}
static int runtime_capture(void* user,const UssFormationRuntime* r,UssPublicationStage stage,UssPublicationCapture* out)
{
    UssCaptureProducer p=producer();Reader budget={0};int ok;(void)user;(void)stage;
    if(capture_block && attachments)return 0;
    p.input=runtime_input;p.user=(void*)r;
    ok=uss_capture_publication(&p,out,&budget);
    if(ok && !uss_pair_owner_publication(&r->life.owner,7,r->life.owner.context.role,&out->owner,&out->key)){fprintf(stderr,"owner stamp failed peers=%zu/%zu role=%u ready=%u\n",out->key.peer_size,r->life.owner.context.peer_size,r->life.owner.context.role,r->life.owner.ready);return 0;}
    return ok;
}
static UssPairServiceResult runtime_select(void* user,const UssCoreObservation* expected,uint32_t slot)
{
    (void)user;assert(expected->valid);++selections;
    put32(fixture(0x70000300,48)+28,slot);
    put32(fixture(0x70000500,4),slot==3?expected->inventory.slots[2]:101);
    put32(fixture(ACTION_MANAGER+0x53DCD0,32)+4,slot);return USS_PAIR_SERVICE_DONE;
}
static UssPairServiceResult runtime_attach(void* user,const UssCoreObservation* expected,const UssPairMember* member)
{
    (void)user;assert(expected->primary.ready || formation_controller_draw_ready(&fixture_runtime,expected));++attachments;
    if(attach_stop)return USS_PAIR_SERVICE_STOP;
    put32(fixture(0x70000300,48)+8,member->entity);
    put32(fixture(0x70000700,144)+(member->entity==103?96:48)+4,22);
    return USS_PAIR_SERVICE_DONE;
}
static UssPublicationSend runtime_send(void* user,const UssPublicationCapture* captured,uint64_t metadata)
{
    uint64_t expected;(void)user;memcpy(&expected,captured->owner.row+40,8);
    assert(captured->valid && metadata==expected);++sends;
    return send_uncertain?USS_PUBLICATION_SEND_UNCERTAIN:USS_PUBLICATION_SEND_RETURNED;
}
static void runtime_fixture(unsigned order,unsigned pickup,unsigned role)
{
    UssCoreCandidates bundle;UssPickupDecision decision;UssFormationIo io={0};
    history_fixture(0);initial_role=role;if(role==HF_SESSION_JOINER)fixture(SESSION+0xB3A8,8)[0]=2;
    /* Rebuild the lifetime after changing the simulated session role. */
    memset(&candidate_life,0,sizeof(candidate_life));recover_scope();
    preference.switch_on_pickup=pickup;
    bundle=read_support(order?103:102,1);decision=step(&bundle);assert(decision.kind==USS_PICKUP_REMEMBERED);
    put32(fixture(0x70000700,144)+(order?96:48)+4,0);
    bundle=read_support(order?102:103,pickup?3:1);decision=complete_read(step(&bundle));
    assert(decision.kind==USS_PICKUP_PAIR_PENDING);
    io.clock_ms=runtime_clock;io.observe=runtime_observe;io.capture=runtime_capture;
    io.select=runtime_select;io.attach=runtime_attach;io.send=runtime_send;
    assert(uss_formation_runtime_init(&fixture_runtime,&io,7,&core_flags));fixture_runtime.life=candidate_life;
    selections=attachments=sends=capture_block=attach_stop=send_uncertain=0;now_ms=100;
}
static void finish_work(void)
{
    unsigned i;for(i=0;i<8 && fixture_runtime.work.phase==USS_PAIR_ACTIVE;++i){now_ms+=25;uss_formation_tick(&fixture_runtime);}
    if(fixture_runtime.work.phase!=USS_PAIR_COMPLETE)fprintf(stderr,"Formation failed: %s, attaches=%u sends=%u\n",fixture_runtime.reason,attachments,sends);
    assert(fixture_runtime.work.phase==USS_PAIR_COMPLETE);
}
#ifndef USS_FORMATION_TEST_MAIN
#define USS_FORMATION_TEST_MAIN main
#endif
int USS_FORMATION_TEST_MAIN(void)
{
    unsigned order,pickup,role,i,before;UssPairMember target;
    (void)formation_native_invoke;(void)uss_formation_native_io;
    assert(history_regression_checks()==0);
    for(role=HF_SESSION_HOST;role<=HF_SESSION_JOINER;++role)
    for(order=0;order<2;++order)for(pickup=0;pickup<2;++pickup){
        runtime_fixture(order,pickup,role);
        assert(uss_formation_start(&fixture_runtime,order?USS_PAIR_UTILITY_FIRST:USS_PAIR_ORDINARY_FIRST,NULL));finish_work();
        assert(fixture_runtime.life.phase==USS_LIFE_FORMED && fixture_runtime.publication.formed);
        assert(attachments==(order || pickup?2u:1u));
        for(i=0;i<4;++i){
            target=fixture_runtime.work.real==103?fixture_runtime.life.owner.ordinary:fixture_runtime.life.owner.utility;
            before=attachments;assert(uss_formation_start(&fixture_runtime,USS_PAIR_DRAW,&target));finish_work();
            assert(attachments==before+1 && fixture_runtime.work.real==target.entity && fixture_runtime.work.selected==3);
        }
    }
    /* A refused capture can wait after attachment; the consumed attachment is not repeated. */
    runtime_fixture(0,0,HF_SESSION_HOST);capture_block=1;
    assert(uss_formation_start(&fixture_runtime,USS_PAIR_ORDINARY_FIRST,NULL));
    assert(uss_formation_tick(&fixture_runtime)==USS_PAIR_ACTIVE && attachments==1);
    assert(uss_formation_tick(&fixture_runtime)==USS_PAIR_ACTIVE && attachments==1);
    capture_block=0;finish_work();assert(attachments==1);
    /* Uncertain effects stop once and cannot be restarted on this lifetime. */
    for(i=0;i<2;++i){
        runtime_fixture(0,0,HF_SESSION_HOST);attach_stop=!i;send_uncertain=i;
        assert(uss_formation_start(&fixture_runtime,USS_PAIR_ORDINARY_FIRST,NULL));
        assert(uss_formation_tick(&fixture_runtime)==USS_PAIR_CANCELLED);
        before=attachments;assert(uss_formation_tick(&fixture_runtime)==USS_PAIR_CANCELLED && attachments==before);
        assert(!uss_formation_start(&fixture_runtime,USS_PAIR_ORDINARY_FIRST,NULL));
    }
    runtime_fixture(0,0,HF_SESSION_HOST);
    assert(uss_formation_start(&fixture_runtime,USS_PAIR_ORDINARY_FIRST,NULL));now_ms+=2000;
    assert(uss_formation_tick(&fixture_runtime)==USS_PAIR_CANCELLED && !attachments && !sends);
    puts("Formation fixture_runtime: both pickup orders/preferences/roles, repeated draws, publication wait, uncertain effects, timeout PASS");
    return 0;
}
