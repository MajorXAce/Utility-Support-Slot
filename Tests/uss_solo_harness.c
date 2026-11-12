/* Coherent own-engine single-peer scenes; no game process or actual SDK registration. */
#include "../src/uss_refill_core.h"
#define USS_RECOVERY_TEST_MAIN solo_recovery_regressions
#include "uss_recovery_harness.c"
static unsigned solo_steps,scene_role,scene_order,scene_pickup,scene_selected;
static int solo_formation_effect(void* user,UssFormationEffect effect,uintptr_t first,uintptr_t second,
    uintptr_t record,uintptr_t inventory,uint32_t avatar,uint32_t value,const UssInventoryMessage* message)
{
    UssReplacementEffect mapped=effect==USS_EFFECT_SELECT?USS_REPLACEMENT_SELECT:
        effect==USS_EFFECT_ATTACH?USS_REPLACEMENT_ATTACH:USS_REPLACEMENT_SEND;
    candidate_life=automatic_runtime.life;replacement_work=automatic_controller.replacement;hf048_history=automatic_runtime.history;
    /* The active route uses formation, not a retained-replacement work object.
     * Own engine must use the real pair member rather than an absent old intent. */
    if(effect==USS_EFFECT_ATTACH && value==104)replacement_work.intent.incoming=
        automatic_runtime.life.owner.utility.entity==value?automatic_runtime.life.owner.utility:automatic_runtime.life.owner.ordinary;
    return own_invoke(user,mapped,first,second,record,inventory,avatar,value,message);
}

/* Native empty is allowed to be zero, not only the UINT_MAX sentinel used
 * by the older replacement-only fixture. */
static unsigned solo_empty_variant;
static int solo_composed_effect(void* user,UssReplacementEffect effect,uintptr_t first,uintptr_t second,
    uintptr_t record,uintptr_t inventory,uint32_t avatar,uint32_t value,const UssInventoryMessage* message)
{
    int result=automatic_own_effect(user,effect,first,second,record,inventory,avatar,value,message);
    if(result && effect==USS_REPLACEMENT_DROP)put32(fixture(0x70000300,48)+8,solo_empty_variant?UINT32_MAX:0);
    return result;
}
static void solo_no_publication(void)
{
    assert(!automatic_runtime.publication.formed && !automatic_runtime.publication.record_present &&
        !automatic_runtime.publication.attempted && !automatic_runtime.publication.returned && !recovery_sends);
    for(unsigned i=0;i<effect_calls;++i)assert(effect_kind[i]!=USS_REPLACEMENT_SEND);
}
static void solo_finish(void)
{
    for(solo_steps=0;solo_steps<40 && (automatic_runtime.work.phase==USS_PAIR_ACTIVE ||
        automatic_controller.replacement.phase==USS_REPLACE_ACTIVE);++solo_steps){replacement_now+=25;(void)automatic_retained_update();}
    if(automatic_controller.stopped)fprintf(stderr,"solo stopped %s stage=%u local_failed=%u work=%s\n",
        automatic_controller.reason,automatic_controller.replacement.stage,automatic_native.local.failed,
        automatic_runtime.work.reason?automatic_runtime.work.reason:"none");
    if(automatic_controller.stopped)fprintf(stderr,"scene role=%u order=%u pickup=%u selected=%u real=%u pair=%llu effects=%u\n",scene_role,scene_order,scene_pickup,scene_selected,automatic_runtime.latest.inventory.slots[2],(unsigned long long)automatic_runtime.life.owner.pair_generation,effect_calls);
    assert(!automatic_controller.stopped && automatic_runtime.life.phase==USS_LIFE_FORMED &&
        automatic_runtime.work.phase!=USS_PAIR_ACTIVE && automatic_controller.replacement.phase!=USS_REPLACE_ACTIVE);
    assert(uss_local_pair_owned(&automatic_runtime.local,&automatic_runtime.life.owner));solo_no_publication();
}
static void solo_scene(unsigned role,unsigned order,unsigned pickup,unsigned selected)
{
    recovery_scene(1,1,HF_SESSION_HOST);(void)solo_recovery_regressions;
    scene_role=role;scene_order=order;scene_pickup=pickup;scene_selected=selected;
    initial_role=role;put32(fixture(SESSION+0x162d8,4),1);fixture(SESSION+0xb3a8,8)[0]=1;
    memset(&automatic_runtime.life,0,sizeof(automatic_runtime.life));memset(&automatic_runtime.history,0,sizeof(automatic_runtime.history));
    memset(&automatic_runtime.work,0,sizeof(automatic_runtime.work));memset(&automatic_runtime.publication,0,sizeof(automatic_runtime.publication));
    uss_local_pair_invalidate(&automatic_runtime.local);automatic_formation.invoke=solo_formation_effect;
    automatic_pickup=pickup;effect_calls=typed_calls=0;
    assert(uss_automatic_native_init(&automatic_native,&automatic_controller,&automatic_formation,automatic_pickup_option,NULL,automatic_own_effect,NULL));
    put32(fixture(0x70000300,48)+8,order?103:102);set_selected(selected);
    put32(fixture(0x70000700,240)+48+4,order?0:22);put32(fixture(0x70000700,240)+96+4,order?22:0);
    (void)automatic_retained_update();(void)automatic_retained_update();assert(automatic_runtime.history.remembered);
    put32(fixture(0x70000300,48)+8,order?102:103);set_selected(pickup?3:selected);
    put32(fixture(0x70000700,240)+48+4,order?22:0);put32(fixture(0x70000700,240)+96+4,order?0:22);
    assert(automatic_retained_update()==USS_CONTROL_WORK && automatic_runtime.work.phase==USS_PAIR_ACTIVE);solo_finish();
}
static void solo_draw(const UssPairMember* member)
{
    UssPairMember target=*member;
    assert(uss_formation_start(&automatic_runtime,USS_PAIR_DRAW,&target));solo_finish();
    assert(automatic_runtime.latest.inventory.slots[2]==target.entity && automatic_runtime.latest.inventory.active_selector==3);
}
static void solo_incoming(unsigned utility,unsigned selected,unsigned pickup,unsigned active)
{
    UssPairMember real=active?(utility?automatic_runtime.life.owner.utility:automatic_runtime.life.owner.ordinary):
        (utility?automatic_runtime.life.owner.ordinary:automatic_runtime.life.owner.utility);
    solo_draw(&real);set_selected(selected);replacement_now+=101;
    assert(automatic_retained_update()==USS_CONTROL_IDLE && automatic_controller.origin_valid);
    memcpy(fixture(0x70003900,24),utility?automatic_runtime.life.owner.utility.identity:automatic_runtime.life.owner.ordinary.identity,8);
    put32(fixture(0x70000300,48)+8,104);set_selected(pickup?3:selected);
    put32(fixture(0x70000700,240)+(real.entity-101)*48+4,0);put32(fixture(0x70000700,240)+144+4,22);
    replacement_now+=25;(void)automatic_retained_update();
    assert(!automatic_controller.stopped);solo_finish();
}
#ifndef USS_SOLO_TEST_MAIN
#define USS_SOLO_TEST_MAIN main
#endif
int USS_SOLO_TEST_MAIN(void)
{
    unsigned role,order,pickup,selected,utility,active,before,cases=0,mode;uint64_t generation;UssPublicationCapture capture;
    UssRefillCore refill={0};UssRefillPairView view;UssCoreObservation current;
    /* Run recovery BEFORE replacement on every tick, as the normal application
     * does. Direct transaction tests previously omitted this composition. */
    for(solo_empty_variant=0;solo_empty_variant<2;++solo_empty_variant)
    for(utility=0;utility<2;++utility)for(pickup=0;pickup<2;++pickup){
        UssPairMember held;unsigned result,done=0,saw_empty=0;
        solo_scene(HF_SESSION_SOLO,0,0,1);
        held=utility?automatic_runtime.life.owner.ordinary:automatic_runtime.life.owner.utility;
        solo_draw(&held);
        automatic_native.invoke=solo_composed_effect;
        put32(fixture(BASE+0x348456c,4),solo_empty_variant?UINT32_MAX:0);replacement_now+=101;
        assert(automatic_retained_update()==USS_CONTROL_IDLE && automatic_controller.origin_valid);
        assert(recovery_update()==USS_CONTROL_IDLE);
        memcpy(fixture(0x70003900,24),utility?automatic_runtime.life.owner.utility.identity:automatic_runtime.life.owner.ordinary.identity,8);
        put32(fixture(0x70000300,48)+8,104);set_selected(3);
        put32(fixture(0x70000700,240)+(held.entity-101)*48+4,0);
        put32(fixture(0x70000700,240)+144+4,22);automatic_pickup=pickup;
        for(unsigned tick=0;tick<40;++tick){
            replacement_now+=25;result=recovery_update();
            if(result!=USS_CONTROL_IDLE)fprintf(stderr,"composed replacement utility=%u pickup=%u stage=%u consumed=%u recovery=%s slot=%u\n",
                utility,pickup,automatic_controller.replacement.stage,automatic_controller.replacement.consumed_mask,
                recovery.reason,u32(fixture(0x70000300,48)+8));
            assert(result==USS_CONTROL_IDLE);
            if(!solo_empty_variant && automatic_controller.replacement.phase==USS_REPLACE_ACTIVE &&
                automatic_controller.replacement.stage==USS_REPLACE_HANDOVER){
                UssCoreObservation denied=automatic_runtime.latest;UssReplacementWork saved=automatic_controller.replacement;
                /* The last runtime snapshot predates the drop: obtain the same
                 * fresh empty observation the normal callback uses. */
                denied=formation_native_observe(&automatic_formation,&automatic_runtime.life.owner,&automatic_runtime.policy);
                automatic_controller.replacement.admitted=0;
                assert(uss_recovery_control(&recovery,&denied,replacement_now)==USS_CONTROL_STOP);
                automatic_controller.replacement=saved;automatic_controller.replacement.consumed_mask&=~8u;
                assert(uss_recovery_control(&recovery,&denied,replacement_now)==USS_CONTROL_STOP);
                automatic_controller.replacement=saved;automatic_controller.replacement.stage=USS_REPLACE_DROP;
                assert(uss_recovery_control(&recovery,&denied,replacement_now)==USS_CONTROL_STOP);
                automatic_controller.replacement=saved;denied.valid=0;
                assert(uss_recovery_control(&recovery,&denied,replacement_now)==USS_CONTROL_STOP);
                assert(recovery_update()==USS_CONTROL_IDLE);
            }
            if(automatic_controller.replacement.phase==USS_REPLACE_ACTIVE &&
                automatic_controller.replacement.stage==USS_REPLACE_HANDOVER)saw_empty=1;
            result=automatic_retained_update();
            if(result==USS_CONTROL_STOP)fprintf(stderr,"replacement control stop stage=%u reason=%s work=%s\n",automatic_controller.replacement.stage,automatic_controller.reason,automatic_controller.replacement.reason);
            assert(result!=USS_CONTROL_STOP);
            if(automatic_controller.replacement.phase==USS_REPLACE_COMPLETE){done=1;break;}
        }
        assert(done && saw_empty && !automatic_controller.stopped);
        solo_no_publication();solo_draw(&automatic_runtime.life.owner.utility);solo_draw(&automatic_runtime.life.owner.ordinary);
    }
    /* Controller has already reached primary, but its drawing block is still
     * positive. Solo draws finish without waiting; network/formation/storage
     * cannot borrow this local-only contract. No timer is written by the mod. */
    solo_scene(HF_SESSION_SOLO,0,0,1);
    {unsigned char* row=fixture(ACTION_MANAGER+0x53DCD0,32);UssPairMember target=automatic_runtime.life.owner.utility;
     put32(row+8,3);put32(row+16,2);put32(row+20,1);put32(row+24,0);put32(row+28,0x3e99999a);
     put32(fixture(ACTION_MANAGER+0x53E880,24)+12,0x8000);
     assert(uss_formation_start(&automatic_runtime,USS_PAIR_DRAW,&target));
     assert(formation_solo_draw_ready(&automatic_runtime,&automatic_runtime.latest));
     {UssCoreObservation denied=automatic_runtime.latest;
      denied.context.local_only=0;assert(!formation_solo_draw_ready(&automatic_runtime,&denied));
      denied=automatic_runtime.latest;denied.primary.fields.global_guard=1;
      assert(!formation_solo_draw_ready(&automatic_runtime,&denied));
      denied=automatic_runtime.latest;denied.ordinary.parent=0;
      assert(!formation_solo_draw_ready(&automatic_runtime,&denied));
      denied=automatic_runtime.latest;put32(denied.primary.fields.row+16,3);
      assert(!formation_solo_draw_ready(&automatic_runtime,&denied));}
     automatic_runtime.work.order=USS_PAIR_ORDINARY_FIRST;
     assert(!formation_solo_draw_ready(&automatic_runtime,&automatic_runtime.latest));
     automatic_runtime.work.order=USS_PAIR_DRAW;
     automatic_runtime.work.origin.local_only=0;
     assert(!formation_solo_draw_ready(&automatic_runtime,&automatic_runtime.latest));
     automatic_runtime.work.origin.local_only=1;
     automatic_runtime.work.order=USS_PAIR_RETAINED;
     assert(!formation_solo_draw_ready(&automatic_runtime,&automatic_runtime.latest));
     automatic_runtime.work.order=USS_PAIR_DRAW;
     solo_finish();assert(automatic_runtime.latest.inventory.slots[2]==target.entity);
     assert(automatic_runtime.latest.inventory.active_selector==3 && u32(row+28)==0x3e99999a);
     solo_no_publication();}
    for(role=HF_SESSION_SOLO;role<=HF_SESSION_HOST;++role)for(order=0;order<2;++order)
    for(pickup=0;pickup<2;++pickup)for(selected=1;selected<=3;++selected){
        solo_scene(role,order,pickup,selected);++cases;
        assert(automatic_runtime.life.owner.context.local_only && !automatic_runtime.view.supported_pair);
        assert(!formation_native_capture(&automatic_formation,&automatic_runtime,USS_PUBLICATION_POST_ATTACH,&capture));
        solo_draw(&automatic_runtime.life.owner.utility);
        /* Real refill Core projection admits local formed pairs without any network receipt. */
        refill.api=&fake;refill.mod=MOD;refill.life=&automatic_runtime.life;refill.policy=&automatic_runtime.policy;
        refill.base=BASE;refill.module_size=0x4000000;refill.platform.thread_id=core_thread;
        assert(uss_refill_core_pair(&refill,&view) && view.formed && view.ordinary==102);
        solo_draw(&automatic_runtime.life.owner.ordinary);
    }
    assert(cases==24);cases=0;
    for(role=HF_SESSION_SOLO;role<=HF_SESSION_HOST;++role)for(utility=0;utility<2;++utility)
    for(pickup=0;pickup<2;++pickup)for(selected=1;selected<=3;++selected)for(active=0;active<2;++active){
        solo_scene(role,0,pickup,1);generation=automatic_runtime.life.owner.pair_generation;
        solo_incoming(utility,selected,pickup,active);++cases;
        assert(automatic_runtime.life.owner.pair_generation==generation+1);
        assert((utility?automatic_runtime.life.owner.utility.entity:automatic_runtime.life.owner.ordinary.entity)==104);
        assert(u32(fixture(0x70000700,240)+(utility?96:48)+4)==0);
        solo_draw(&automatic_runtime.life.owner.utility);solo_draw(&automatic_runtime.life.owner.ordinary);
    }
    assert(cases==48);
    /* Actual local receipt authorizes recovery; no dummy network record supplied. */
    for(role=HF_SESSION_SOLO;role<=HF_SESSION_HOST;++role)for(utility=0;utility<2;++utility)
    for(selected=1;selected<=2;++selected)for(active=0;active<2;++active){
        solo_scene(role,0,0,1);solo_draw(utility?&automatic_runtime.life.owner.utility:&automatic_runtime.life.owner.ordinary);
        automatic_formation.invoke=recovery_own_effect;set_selected(selected);
        assert(recovery_update()==USS_CONTROL_IDLE);empty_support(active,selected);
        assert(recovery_update()==USS_CONTROL_WORK && recovery.work.local_only);finish_recovery();
        solo_no_publication();assert(!automatic_runtime.life.owner.pair_present);
        before=recovery_effects;assert(recovery_update()==USS_CONTROL_IDLE && recovery_effects==before);
    }
    for(utility=0;utility<2;++utility){
        solo_scene(HF_SESSION_SOLO,0,0,1);solo_draw(utility?&automatic_runtime.life.owner.utility:&automatic_runtime.life.owner.ordinary);
        assert(recovery_update()==USS_CONTROL_IDLE);empty_support(0,1);
        memset(fixture(0x70000300,48),0,16);put32(fixture(0x70000500,4),0);
        assert(recovery_update()==USS_CONTROL_WORK && recovery.work.local_only);finish_recovery();
        assert(recovery_drops==1 && !automatic_runtime.life.owner.pair_present);solo_no_publication();
    }
    /* Unknown parents/stale lifetimes/changed identity cannot mint local proof. */
    solo_scene(HF_SESSION_SOLO,0,0,1);
    {UssLocalPairState rejected={0};UssPairOwner old=automatic_runtime.life.owner;
        UssCoreObservation observed=automatic_runtime.latest;
        observed.utility.parent=0;assert(!uss_local_pair_accept(&rejected,&old,&observed));
        observed=automatic_runtime.latest;observed.ordinary.descriptor[0]^=1;
        assert(!uss_local_pair_accept(&rejected,&old,&observed));
        observed=automatic_runtime.latest;++observed.expected_guard;
        assert(!uss_local_pair_accept(&rejected,&old,&observed));
        ++old.pair_generation;assert(!uss_local_pair_owned(&automatic_runtime.local,&old));
        old=automatic_runtime.life.owner;++old.guard_generation;assert(!uss_local_pair_owned(&automatic_runtime.local,&old));
    }
    /* Returned/no-effect, uncertain-return and unreadable post-effect all stop.
     * Each failing native operation stays consumed, including the actual drop. */
    for(mode=0;mode<=4;++mode){
        if(mode==1)continue;
        /* New isolated scene: arrange the incoming pickup but stop before native work. */
        solo_scene(HF_SESSION_SOLO,0,0,1);solo_draw(&automatic_runtime.life.owner.ordinary);
        set_selected(1);replacement_now+=101;assert(automatic_retained_update()==USS_CONTROL_IDLE && automatic_controller.origin_valid);
        memcpy(fixture(0x70003900,24),automatic_runtime.life.owner.utility.identity,8);
        put32(fixture(0x70000300,48)+8,104);put32(fixture(0x70000700,240)+48+4,0);put32(fixture(0x70000700,240)+144+4,22);
        replacement_now+=25;assert(automatic_retained_update()==USS_CONTROL_WORK && automatic_controller.replacement.phase==USS_REPLACE_ACTIVE);
        fault_effect=effect_calls+1;fault_mode=mode;
        for(unsigned i=0;i<40 && !automatic_controller.stopped;++i){replacement_now+=25;(void)automatic_retained_update();}
        assert(automatic_controller.stopped && automatic_native.local.failed);
        before=effect_calls;fail_at=0;assert(automatic_retained_update()==USS_CONTROL_STOP && effect_calls==before);
        solo_no_publication();
    }
    /* Corrupt the actual native drop entry: prior attachments may have returned,
     * but the slot drop itself never enters and cannot be retried. */
    solo_scene(HF_SESSION_SOLO,0,0,1);solo_draw(&automatic_runtime.life.owner.ordinary);
    set_selected(1);replacement_now+=101;assert(automatic_retained_update()==USS_CONTROL_IDLE && automatic_controller.origin_valid);
    memcpy(fixture(0x70003900,24),automatic_runtime.life.owner.utility.identity,8);
    put32(fixture(0x70000300,48)+8,104);put32(fixture(0x70000700,240)+48+4,0);put32(fixture(0x70000700,240)+144+4,22);
    replacement_now+=25;(void)automatic_retained_update();fixture(BASE+0x9af090,64)[0]^=1;
    for(unsigned i=0;i<40 && !automatic_controller.stopped;++i){replacement_now+=25;(void)automatic_retained_update();}
    assert(automatic_controller.stopped && automatic_native.local.failed);
    for(unsigned i=0;i<effect_calls;++i)assert(effect_kind[i]!=USS_REPLACEMENT_DROP);
    solo_no_publication();
    /* Solo receipt cannot masquerade as published when a real second peer joins.
     * Same actor retains membership, but context changes and the local proof expires. */
    solo_scene(HF_SESSION_SOLO,0,0,1);generation=automatic_runtime.life.owner.pair_generation;
    initial_role=HF_SESSION_HOST;put32(fixture(SESSION+0x162d8,4),2);
    assert(formation_refresh(&automatic_runtime));assert(!automatic_runtime.local.valid);
    assert(formation_refresh(&automatic_runtime) && automatic_runtime.life.owner.pair_generation==generation && automatic_runtime.life.owner.pair_present);
    assert(!automatic_runtime.publication.formed && !replacement_record_matches(&automatic_runtime.life,&automatic_runtime.publication));
    before=effect_calls;assert(uss_formation_start(&automatic_runtime,USS_PAIR_DRAW,&automatic_runtime.life.owner.utility));
    for(unsigned i=0;i<30 && automatic_runtime.work.phase==USS_PAIR_ACTIVE;++i){replacement_now+=25;uss_formation_tick(&automatic_runtime);}
    assert(automatic_runtime.work.phase==USS_PAIR_COMPLETE && automatic_runtime.publication.formed && automatic_runtime.publication.returned);
    assert(effect_calls>before); /* Now the real network branch supplies its own proof. */
    /* Contradictory SDK/native rosters do not qualify as solo or network. */
    solo_scene(HF_SESSION_SOLO,0,0,1);put32(fixture(SESSION+0x162d8,4),2);
    current=formation_native_observe(&automatic_formation,&automatic_runtime.life.owner,&automatic_runtime.policy);assert(!current.valid);
    solo_scene(HF_SESSION_SOLO,0,0,1);initial_role=HF_SESSION_JOINER;
    current=formation_native_observe(&automatic_formation,&automatic_runtime.life.owner,&automatic_runtime.policy);assert(!current.valid);
    puts("USS solo:24formation/order/preference/selection scenes;48typed replacement routes and later draws;16recovery +2death;refill;real scope transition/no fake publication PASS");return 0;
}
