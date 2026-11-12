#include "../src/uss_recovery_runtime.h"
#define USS_RETAINED_TEST_MAIN retained_recovery_regressions
#include "uss_automatic_retained_harness.c"
static UssRecoveryRuntime recovery;static unsigned recovery_effects,recovery_sends,recovery_drops,recovery_fail,recovery_postread_fault;
static int recovery_own_effect(void* user,UssFormationEffect effect,uintptr_t first,uintptr_t second,
    uintptr_t record,uintptr_t inventory,uint32_t avatar,uint32_t value,const UssInventoryMessage* message)
{
    (void)user;(void)second;(void)record;assert(avatar==22 || effect==USS_EFFECT_SEND);++recovery_effects;
    if(recovery_fail==recovery_effects)return 0;
    if(effect==USS_EFFECT_SELECT){assert(first==BASE+0xa93e90 && inventory==0x40000000);set_selected(value);return 1;}
    if(effect==USS_EFFECT_ATTACH){assert(first==BASE+0x9aa290 && inventory==0x40000000 && (value==102 || value==103));
        put32(fixture(0x70000300,48)+8,value);put32(fixture(0x70000700,240)+(value-101)*48+4,22);
        if(recovery_postread_fault)fail_at=reads+1;return 1;}
    assert(effect==USS_EFFECT_SEND && first==BASE+0xbe3f40 && message && message->avatar==22 && message->primary==101 &&
        message->secondary==105 && (message->support==102 || message->support==103));
    ++recovery_sends;return 1;
}
static int recovery_own_drop(void* user,uintptr_t entry,uintptr_t inventory,uint32_t avatar,uint32_t entity)
{
    (void)user;assert(entry==BASE+0x9a8840 && inventory==0x40000000 && avatar==22 && (entity==102 || entity==103));
    ++recovery_effects;++recovery_drops;if(recovery_fail==recovery_effects)return 0;
    put32(fixture(0x70000700,240)+(entity-101)*48+4,0);return 1;
}
static unsigned recovery_update(void)
{
    UssCorePlatform platform={0};UssCoreObservation snapshot;UssCoreFlags flags=uss_lifecycle_flags(&automatic_runtime.life,&core_flags);
    platform.thread_id=core_thread;snapshot=uss_core_observe(&fake,MOD,&automatic_runtime.life.owner,&flags,&platform,NULL);
    return uss_recovery_control(&recovery,&snapshot,replacement_now);
}
static void recovery_scene(unsigned outgoing_utility,unsigned selected,unsigned role)
{
    uint32_t real=outgoing_utility?103:102;
    automatic_retained_scene(!outgoing_utility,0,selected,role);
    /* Restore PRE-pickup inventory; the fixture already owns the exact formed
     * pair/publication and all raw mapping/dispatch evidence. */
    put32(fixture(0x70000300,48)+8,real);set_selected(selected);
    put32(fixture(0x70000700,240)+48+4,22);put32(fixture(0x70000700,240)+96+4,22);put32(fixture(0x70000700,240)+144+4,0);
    automatic_formation.invoke=recovery_own_effect;recovery_effects=recovery_sends=recovery_drops=recovery_fail=recovery_postread_fault=0;
    memset(&recovery,0,sizeof(recovery));recovery.automatic=&automatic_controller;recovery.native.formation=&automatic_formation;
    recovery.native.reader.api=&fake;recovery.native.reader.mod=MOD;recovery.native.reader.platform.thread_id=core_thread;
    recovery.native.reader.policy=&automatic_runtime.policy;recovery.native.drop=recovery_own_drop;
    memcpy(segment(BASE+0x9a8840,sizeof(uss_death_drop_pin)),uss_death_drop_pin,sizeof(uss_death_drop_pin));
    assert(recovery_update()==USS_CONTROL_IDLE && recovery.real==real);
}
static void empty_support(unsigned consumed,unsigned selected)
{
    uint32_t real=recovery.real;unsigned i;put32(fixture(0x70000300,48)+8,0);set_selected(selected);
    put32(fixture(0x70000700,240)+(real-101)*48+4,0);
    if(consumed){
        unsigned char* rows=fixture(0x70000800,64);
        for(i=0;i<8;++i)if(u32(rows+i*8)==real){put32(rows+i*8,UINT32_MAX);put32(rows+i*8+4,UINT32_MAX);break;}
        assert(i<8);
    }
}
static void finish_recovery(void)
{
    unsigned i;for(i=0;i<12 && recovery.work.phase!=USS_RECOVERY_COMPLETE;++i){replacement_now+=25;
        if(recovery_update()!=USS_CONTROL_WORK)fprintf(stderr,"phase=%u detail=%s effects=%u sends=%u\n",recovery.work.phase,recovery.work.reason,recovery_effects,recovery_sends);
        assert(recovery.work.phase!=USS_RECOVERY_CANCELLED);}
    assert(recovery.work.phase==USS_RECOVERY_COMPLETE);
}
static HfTickCallback recovery_callback;static void* recovery_callback_user;
static UssGameplayLoop recovery_loop;
static int register_recovery_loop(HfMod* mod,HfTickCallback callback,void* user)
{assert(mod==MOD);recovery_callback=callback;recovery_callback_user=user;return 1;}
static int recovery_loop_policy(void* user,UssCoreFlags* output)
{(void)user;*output=core_flags;return 1;}
static unsigned recovery_loop_idle(void* user,UssCoreLifecycle* life,const UssCoreObservation* snapshot,uint64_t now)
{(void)user;(void)life;(void)snapshot;(void)now;return USS_CONTROL_IDLE;}
static unsigned recovery_loop_pre(void* user,const UssCoreObservation* snapshot,uint64_t now)
{(void)user;return uss_recovery_control(&recovery,snapshot,now);}
#ifndef USS_RECOVERY_TEST_MAIN
#define USS_RECOVERY_TEST_MAIN main
#endif
int USS_RECOVERY_TEST_MAIN(void)
{
    unsigned role,utility,selected,consumed,before,cases=0;UssRecoverySnapshot snapshot;
    (void)retained_recovery_regressions;(void)uss_recovery_drop_invoke;
    for(role=HF_SESSION_HOST;role<=HF_SESSION_JOINER;++role)for(utility=0;utility<=1;++utility)
    for(selected=1;selected<=2;++selected)for(consumed=0;consumed<=1;++consumed){
        /* Consumption is meaningful for utility; ordinary absence exercises the
         * same lifetime/read path, not a claim that ordinary guns expend. */
        recovery_scene(utility,selected,role);empty_support(consumed,selected);
        assert(recovery_update()==USS_CONTROL_WORK && recovery.work.phase==USS_RECOVERY_ATTACH);finish_recovery();++cases;
        assert(!automatic_runtime.life.owner.pair_present && !automatic_runtime.publication.record_present && recovery_sends==1 && !recovery_drops);
        assert(u32(fixture(0x70000300,48)+8)==(utility?102u:103u) && u32(fixture(0x70000300,48)+28)==selected);
        before=recovery_effects;replacement_now+=25;assert(recovery_update()==USS_CONTROL_IDLE && recovery_effects==before);
    }
    assert(cases==16);
    /* Ordinary survivor restoration waits before consuming any effect, then
     * completes once the same native readiness blocker clears. */
    recovery_scene(1,1,HF_SESSION_HOST);empty_support(0,1);
    assert(recovery_update()==USS_CONTROL_WORK);
    assert(uss_recovery_observe(&recovery.native.reader,&recovery.work,&snapshot));
    put32(fixture(snapshot.primary.fields.action.record,32)+28,1);
    replacement_now+=25;assert(recovery_update()==USS_CONTROL_WORK);
    assert(!strcmp(recovery.work.reason,"WAIT_RECOVERY_RESTORE_READY") && !recovery_effects && !recovery.work.consumed);
    replacement_now+=25;assert(recovery_update()==USS_CONTROL_WORK && !recovery_effects);
    put32(fixture(snapshot.primary.fields.action.record,32)+28,0);finish_recovery();
    /* Unknown reads never become disappearance. A consumed member is supported
     * only through an actually empty bucket in the weapon lookup. */
    recovery_scene(1,1,HF_SESSION_HOST);empty_support(0,1);assert(recovery_update()==USS_CONTROL_WORK);
    reads=0;fail_at=1;assert(!uss_recovery_observe(&recovery.native.reader,&recovery.work,&snapshot) && !snapshot.valid);fail_at=0;
    recovery_fail=1;assert(recovery_update()==USS_CONTROL_STOP && recovery.work.phase==USS_RECOVERY_CANCELLED);
    before=recovery_effects;assert(recovery_update()==USS_CONTROL_STOP && recovery_effects==before);
    /* A returned attachment followed by unreadable verification is uncertain.
     * Keep its actual effect, cancel the operation, and never attach/send again. */
    recovery_scene(1,1,HF_SESSION_HOST);empty_support(0,1);(void)recovery_update();recovery_postread_fault=1;
    assert(recovery_update()==USS_CONTROL_STOP && recovery.work.phase==USS_RECOVERY_CANCELLED);
    assert(u32(fixture(0x70000300,48)+8)==102 && automatic_runtime.life.owner.pair_present);
    before=recovery_effects;fail_at=0;assert(recovery_update()==USS_CONTROL_STOP && recovery_effects==before && !recovery_sends);
    /* A foreign publication lifetime cannot authorize even local restoration. */
    recovery_scene(1,1,HF_SESSION_HOST);empty_support(0,1);automatic_runtime.publication.record.mapping^=8;
    assert(recovery_update()==USS_CONTROL_STOP && !recovery_effects);
    /* A native primary wait is bounded and does not repeat attachment. */
    recovery_scene(1,1,HF_SESSION_HOST);empty_support(0,1);(void)recovery_update();
    put32(fixture(ACTION_MANAGER+0x53dcd0,32)+16,3);
    replacement_now+=25;(void)recovery_update();replacement_now+=25;(void)recovery_update();before=recovery_effects;
    replacement_now+=25;assert(recovery_update()==USS_CONTROL_WORK && !strcmp(recovery.work.reason,"WAIT_RECOVERY_PRIMARY") && recovery_effects==before);
    put32(fixture(ACTION_MANAGER+0x53dcd0,32)+16,0);finish_recovery();
    /* Vanilla death releases the real member first; only stored is dropped. */
    for(utility=0;utility<=1;++utility){
        recovery_scene(utility,1,HF_SESSION_HOST);empty_support(0,1);
        memset(fixture(0x70000300,48),0,16);put32(fixture(0x70000500,4),0);
        assert(recovery_update()==USS_CONTROL_WORK && recovery.work.phase==USS_RECOVERY_DEATH_WAIT);finish_recovery();
        assert(recovery_drops==1 && !recovery_sends && !automatic_runtime.life.owner.pair_present && !u32(fixture(0x70000300,48)+8));
    }
    /* The actual persistent loop must route a consumed member's failed Core
     * read BEFORE ordinary apply interrupts the owned pair's guard generation. */
    recovery_scene(1,1,HF_SESSION_HOST);empty_support(1,1);
    {UssGameplayLoopIo io={0};unsigned i;uint64_t guard=automatic_runtime.life.owner.guard_generation;
        memset(&recovery_loop,0,sizeof(recovery_loop));fake.on_game_thread=register_recovery_loop;
        io.clock_ms=replacement_clock_fixture;io.thread_id=core_thread;io.policy=recovery_loop_policy;
        io.control=recovery_loop_idle;io.recover=recovery_loop_pre;
        assert(uss_gameplay_loop_register(&recovery_loop,&fake,MOD,&automatic_runtime.life,&io));
        for(i=0;i<12 && recovery.work.phase!=USS_RECOVERY_COMPLETE;++i){replacement_now+=25;recovery_callback(MOD,0,recovery_callback_user);}
        assert(recovery.work.phase==USS_RECOVERY_COMPLETE && recovery_sends==1 && !automatic_runtime.life.owner.pair_present);
        assert(automatic_runtime.life.owner.guard_generation==guard && atomic_load(&recovery_loop.enabled));
    }
    /* A dead avatar whose vanilla inventory has not emptied is observed for at
     * most500ms, then the old software pair is forgotten without a native drop. */
    recovery_scene(0,1,HF_SESSION_JOINER);put32(fixture(0x70000300,48),0);put32(fixture(0x70000300,48)+4,0);
    assert(recovery_update()==USS_CONTROL_WORK && recovery.work.phase==USS_RECOVERY_DEATH_WAIT);
    replacement_now+=250;assert(recovery_update()==USS_CONTROL_WORK && !recovery_drops && automatic_runtime.life.owner.pair_present);
    replacement_now+=251;assert(recovery_update()==USS_CONTROL_WORK && recovery.work.phase==USS_RECOVERY_COMPLETE);
    assert(!recovery_drops && !automatic_runtime.life.owner.pair_present);
    /* Native outgoing parenting can settle after the empty slot. Wait without
     * consuming recovery/attachment; a foreign parent never becomes release. */
    recovery_scene(1,1,HF_SESSION_HOST);empty_support(0,1);put32(fixture(0x70000700,240)+96+4,22);
    assert(recovery_update()==USS_CONTROL_WORK && recovery.work.phase==USS_RECOVERY_IDLE && !recovery_effects);
    put32(fixture(0x70000700,240)+96+4,0);replacement_now+=25;
    assert(recovery_update()==USS_CONTROL_WORK && recovery.work.phase==USS_RECOVERY_ATTACH);finish_recovery();
    puts("USS recovery:16host/joiner/class/selection/drop-consumption scenes -> concrete OWN restore/lone send/selection/clear; death stored-only, waits, unknown/failure no retry PASS");return 0;
}



