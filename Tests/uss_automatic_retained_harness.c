#include "../src/uss_automatic_native.h"
#define USS_REPLACEMENT_ADAPTER_TEST_MAIN adapter_regressions
#include "replacement_adapter_harness.c"
static UssFormationRuntime automatic_runtime;static UssFormationNative automatic_formation;
static UssAutomaticController automatic_controller;static UssAutomaticNative automatic_native;
static unsigned automatic_pickup;
static int automatic_pickup_option(void* user,UssPickupPolicy* out)
{(void)user;out->known=1;out->switch_on_pickup=automatic_pickup;return 1;}
static int automatic_own_effect(void* user,UssReplacementEffect effect,uintptr_t first,uintptr_t second,
    uintptr_t record,uintptr_t inventory,uint32_t avatar,uint32_t value,const UssInventoryMessage* message)
{
    /* Existing own-engine functions consult these fixture mirrors, never game
     * memory. The actual adapter still owns automatic_runtime/controller. */
    candidate_life=automatic_runtime.life;replacement_work=automatic_controller.replacement;
    hf048_history=automatic_runtime.history;
    return own_invoke(user,effect,first,second,record,inventory,avatar,value,message);
}
static int automatic_formation_effect(void* user,UssFormationEffect effect,uintptr_t first,uintptr_t second,
    uintptr_t record,uintptr_t inventory,uint32_t avatar,uint32_t value,const UssInventoryMessage* message)
{
    UssReplacementEffect mapped=effect==USS_EFFECT_SELECT?USS_REPLACEMENT_SELECT:
        effect==USS_EFFECT_ATTACH?USS_REPLACEMENT_ATTACH:USS_REPLACEMENT_SEND;
    return automatic_own_effect(user,mapped,first,second,record,inventory,avatar,value,message);
}
static unsigned automatic_retained_update(void)
{
    UssCorePlatform platform={0};UssCoreObservation core_snapshot;UssPairObservation pair;UssPublicationView view={0};UssPublicationOwner row={0};
    UssCoreFlags flags=uss_lifecycle_flags(&automatic_runtime.life,&core_flags);platform.thread_id=core_thread;
    core_snapshot=uss_core_observe(&fake,MOD,&automatic_runtime.life.owner,&flags,&platform,NULL);
    assert(core_snapshot.valid && uss_lifecycle_observe(&automatic_runtime.life,7,&core_snapshot,&pair,&view,&row));
    return uss_automatic_control(&automatic_controller,&automatic_runtime.life,&core_snapshot,replacement_now);
}
static void automatic_retained_scene(unsigned utility,unsigned pickup,unsigned selected,unsigned role)
{
    UssCoreObservation core_snapshot;UssPairObservation pair;UssPublicationView view={0};UssPublicationOwner row={0};
    UssCorePlatform platform={0};UssCoreFlags flags;
    /* Reuse exact dispatch/pin/mapping graphs, but restore PRE-pickup state.
     * adapter_fixture's admission is software-only; assert no own native effect. */
    adapter_fixture(utility,pickup,selected,role);assert(!effect_calls);
    memset(&automatic_runtime,0,sizeof(automatic_runtime));automatic_runtime.life=candidate_life;
    automatic_runtime.life.phase=USS_LIFE_FORMED;memset(&automatic_runtime.life.plan,0,sizeof(automatic_runtime.life.plan));
    automatic_runtime.thread=7;automatic_runtime.policy=core_flags;automatic_runtime.io.clock_ms=replacement_clock_fixture;
    put32(fixture(0x70000300,48)+8,replacement_origin.real);set_selected(selected);
    put32(fixture(0x70000700,240)+48+4,22);put32(fixture(0x70000700,240)+96+4,22);put32(fixture(0x70000700,240)+144+4,0);
    flags=uss_lifecycle_flags(&automatic_runtime.life,&core_flags);platform.thread_id=core_thread;
    core_snapshot=uss_core_observe(&fake,MOD,&automatic_runtime.life.owner,&flags,&platform,NULL);
    assert(core_snapshot.valid && uss_lifecycle_observe(&automatic_runtime.life,7,&core_snapshot,&pair,&view,&row));
    automatic_runtime.publication.record=view.key;automatic_runtime.publication.formed=automatic_runtime.publication.record_present=1;
    automatic_runtime.publication.member=replacement_origin.real;automatic_runtime.publication.game_thread=7;
    memset(&automatic_formation,0,sizeof(automatic_formation));automatic_formation.api=&fake;automatic_formation.mod=MOD;
    automatic_formation.runtime=&automatic_runtime;automatic_formation.platform=producer().platform;
    automatic_formation.invoke=automatic_formation_effect;
    automatic_runtime.io=uss_formation_native_io(&automatic_formation,replacement_clock_fixture);
    automatic_pickup=pickup;
    assert(uss_automatic_native_init(&automatic_native,&automatic_controller,&automatic_formation,automatic_pickup_option,NULL,automatic_own_effect,NULL));
    replacement_now+=25;assert(automatic_retained_update()==USS_CONTROL_IDLE && automatic_controller.origin_valid && !effect_calls);
    put32(fixture(0x70000300,48)+8,104);set_selected(pickup?3:selected);
    put32(fixture(0x70000700,240)+(utility?48:96)+4,0);put32(fixture(0x70000700,240)+144+4,22);
}
#ifndef USS_RETAINED_TEST_MAIN
#define USS_RETAINED_TEST_MAIN main
#endif
int USS_RETAINED_TEST_MAIN(void)
{
    unsigned role,utility,pickup,selected,i,count=0,before,j,drops,mode,draw;uint64_t generation;(void)adapter_regressions;(void)formation_native_invoke;
    for(role=HF_SESSION_HOST;role<=HF_SESSION_JOINER;++role)for(utility=0;utility<=1;++utility)
    for(pickup=0;pickup<=1;++pickup)for(selected=1;selected<=3;++selected){
        ++count;automatic_retained_scene(utility,pickup,selected,role);generation=automatic_runtime.life.owner.pair_generation;
        replacement_now+=25;assert(automatic_retained_update()==USS_CONTROL_WORK);
        if(automatic_controller.stopped)fprintf(stderr,"start: %s adapter=%s\n",automatic_controller.reason,automatic_native.adapter.reason);
        assert(!automatic_controller.stopped && automatic_controller.replacement.phase==USS_REPLACE_ACTIVE);
        for(i=0;i<50 && automatic_controller.replacement.phase==USS_REPLACE_ACTIVE;++i){replacement_now+=25;(void)automatic_retained_update();}
        if(automatic_controller.stopped)fprintf(stderr,"completion: %s adapter=%s stage=%u\n",automatic_controller.reason,automatic_native.adapter.reason,automatic_controller.replacement.stage);
        assert(!automatic_controller.stopped && automatic_controller.replacement.phase==USS_REPLACE_COMPLETE && automatic_runtime.life.phase==USS_LIFE_FORMED);
        assert(automatic_runtime.life.owner.pair_generation==generation+1 && automatic_runtime.publication.formed);
        assert(automatic_controller.replacement.real==(pickup?104u:replacement_origin.real));
        assert(automatic_controller.replacement.selected==(pickup?3u:selected));
        assert(automatic_runtime.publication.record.utility==(utility?104u:103u) && automatic_runtime.publication.record.ordinary==(utility?102u:104u));
        drops=0;for(j=0;j<effect_calls;++j)if(effect_kind[j]==USS_REPLACEMENT_DROP)++drops;
        assert(drops==1 && u32(fixture(0x70000700,240)+(utility?96:48)+4)==0);
        before=effect_calls;replacement_now+=25;assert(automatic_retained_update()==USS_CONTROL_IDLE && effect_calls==before);
        for(draw=0;draw<2;++draw){
            UssPairMember target=draw?automatic_runtime.life.owner.ordinary:automatic_runtime.life.owner.utility;
            set_selected(1);assert(uss_formation_start(&automatic_runtime,USS_PAIR_DRAW,&target));
            for(i=0;i<30 && automatic_runtime.work.phase==USS_PAIR_ACTIVE;++i){replacement_now+=25;(void)automatic_retained_update();}
            assert(!automatic_controller.stopped && automatic_runtime.work.phase==USS_PAIR_COMPLETE && automatic_runtime.work.real==target.entity);
            assert(automatic_runtime.latest.inventory.active_selector==3 && automatic_runtime.publication.formed &&
                automatic_runtime.publication.record.pair_generation==generation+1);
        }
    }
    assert(count==24);
    for(mode=0;mode<=4;mode+=2){
        automatic_retained_scene(1,0,1,HF_SESSION_HOST);replacement_now+=25;
        assert(automatic_retained_update()==USS_CONTROL_WORK && automatic_controller.replacement.phase==USS_REPLACE_ACTIVE);
        fault_effect=1;fault_mode=mode;
        for(i=0;i<50 && !automatic_controller.stopped;++i){replacement_now+=25;(void)automatic_retained_update();}
        assert(automatic_controller.stopped && automatic_controller.replacement.phase==USS_REPLACE_CANCELLED && effect_calls==1);
        before=effect_calls;replacement_now+=25;assert(automatic_retained_update()==USS_CONTROL_STOP && effect_calls==before);
    }
    puts("USS automatic displaced-retained: 24roles/classes/options/primary-secondary-support origins -> concrete guarded OWN cleanup/publication, generation/selection and stable idle PASS");return 0;
}
