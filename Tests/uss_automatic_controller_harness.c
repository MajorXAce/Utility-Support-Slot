#include "../src/uss_automatic_native.h"
#define USS_COMMAND_TEST_MAIN command_regression_checks
#include "formation_command_harness.c"
static UssAutomaticController controller;static unsigned pickup_known=1,pickup_yes,origin_calls;
static int pickup_option(void* user,UssPickupPolicy* out)
{(void)user;out->known=pickup_known;out->switch_on_pickup=pickup_yes;return (int)pickup_known;}
static int origin_unavailable(void* user,UssReplacementOrigin* out)
{(void)user;(void)out;++origin_calls;return 0;}
static unsigned update_auto(void)
{
    assert(formation_refresh(&fixture_runtime));
    return uss_automatic_control(&controller,&fixture_runtime.life,&fixture_runtime.latest,now_ms);
}
static void automatic_fixture(void)
{
    UssCorePlatform platform={0};UssAutomaticIo io={0};
    command_fixture();platform.thread_id=core_thread;io.pickup_policy=pickup_option;io.origin=origin_unavailable;
    pickup_known=1;pickup_yes=origin_calls=0;
    assert(uss_automatic_init(&controller,&fixture_runtime,&fake,MOD,&platform,&io));
}
static int refuse_replacement(void* user,UssReplacementEffect effect,uintptr_t a,uintptr_t b,uintptr_t d,uintptr_t e,
    uint32_t avatar,uint32_t entity,const UssInventoryMessage* message)
{(void)user;(void)effect;(void)a;(void)b;(void)d;(void)e;(void)avatar;(void)entity;(void)message;assert(0);return 0;}
#ifndef USS_AUTOMATIC_TEST_MAIN
#define USS_AUTOMATIC_TEST_MAIN main
#endif
int USS_AUTOMATIC_TEST_MAIN(void)
{
    unsigned i,before;(void)command_regression_checks;
    automatic_fixture();(void)update_auto();(void)update_auto();
    assert(fixture_runtime.history.remembered && !fixture_runtime.life.owner.pair_present && !fixture_native.effects);
    put32(fixture(0x70000300,48)+8,103);put32(fixture(0x70000700,144)+48+4,0);put32(fixture(0x70000700,144)+96+4,22);
    assert(update_auto()==USS_CONTROL_WORK && fixture_runtime.work.phase==USS_PAIR_ACTIVE);
    for(i=0;i<10 && fixture_runtime.work.phase==USS_PAIR_ACTIVE;++i){now_ms+=25;assert(update_auto()==USS_CONTROL_WORK);}
    assert(fixture_runtime.life.phase==USS_LIFE_FORMED && fixture_runtime.work.phase==USS_PAIR_COMPLETE);
    before=fixture_native.effects;now_ms+=25;assert(update_auto()==USS_CONTROL_IDLE && origin_calls==1);
    now_ms+=25;assert(update_auto()==USS_CONTROL_IDLE && fixture_native.effects==before && !controller.origin_valid);
    /* Opposite pickup order is discovered without selecting a diagnostic command. */
    automatic_fixture();put32(fixture(0x70000300,48)+8,103);put32(fixture(0x70000700,144)+48+4,0);put32(fixture(0x70000700,144)+96+4,22);
    (void)update_auto();(void)update_auto();assert(fixture_runtime.history.remembered);
    put32(fixture(0x70000300,48)+8,102);put32(fixture(0x70000700,144)+48+4,22);put32(fixture(0x70000700,144)+96+4,0);
    assert(update_auto()==USS_CONTROL_WORK && fixture_runtime.work.phase==USS_PAIR_ACTIVE);
    for(i=0;i<10 && fixture_runtime.work.phase==USS_PAIR_ACTIVE;++i){now_ms+=25;(void)update_auto();}
    assert(fixture_runtime.life.phase==USS_LIFE_FORMED && fixture_runtime.work.phase==USS_PAIR_COMPLETE);
    automatic_fixture();(void)update_auto();(void)update_auto();pickup_known=0;
    put32(fixture(0x70000300,48)+8,103);put32(fixture(0x70000700,144)+48+4,0);put32(fixture(0x70000700,144)+96+4,22);
    before=fixture_native.effects;assert(update_auto()==USS_CONTROL_WORK && !fixture_runtime.life.owner.pair_present && fixture_native.effects==before);
    pickup_known=1;assert(update_auto()==USS_CONTROL_WORK && fixture_runtime.work.phase==USS_PAIR_ACTIVE);
    attach_stop=1;for(i=0;i<10 && !controller.stopped;++i){now_ms+=25;(void)update_auto();}
    assert(controller.stopped);before=fixture_native.effects;assert(update_auto()==USS_CONTROL_STOP && fixture_native.effects==before);
    /* Concrete pre-pickup reader and fresh replacement adapter, no effect entered. */
    automatic_fixture();(void)update_auto();(void)update_auto();
    put32(fixture(0x70000300,48)+8,103);put32(fixture(0x70000700,144)+48+4,0);put32(fixture(0x70000700,144)+96+4,22);
    (void)update_auto();for(i=0;i<10 && fixture_runtime.work.phase==USS_PAIR_ACTIVE;++i){now_ms+=25;(void)update_auto();}
    {UssAutomaticNative native;UssReplacementServices services;UssReplacementOrigin old;
        assert(uss_automatic_native_init(&native,&controller,&fixture_native,pickup_option,NULL,refuse_replacement,NULL));
        now_ms+=25;assert(update_auto()==USS_CONTROL_IDLE && controller.origin_valid && native.origin.valid);
        {uint64_t sampled=controller.origin_ms;now_ms+=25;assert(update_auto()==USS_CONTROL_IDLE && controller.origin_ms==sampled);}
        old=controller.origin;assert(automatic_native_prepare(&native,&controller.replacement,&old,&services));
        ++old.guard_generation;assert(!automatic_native_prepare(&native,&controller.replacement,&old,&services));
        extra_candidate();memset(fixture(0x70003900,24),0,8);fixture(0x70003900,24)[0]=8;
        put32(fixture(0x70000300,48)+8,104);before=fixture_native.effects;
        now_ms+=25;assert(update_auto()==USS_CONTROL_WORK && !strcmp(controller.reason,"WAIT_NATIVE_RELEASE") && controller.origin_valid);
        assert(!fixture_runtime.history.last_serial && fixture_native.effects==before);
        fixture_runtime.policy.down=1;assert(!automatic_native_origin(&native,&old) && !native.origin.valid);
    }
    puts("USS automatic controller: both pickup orders -> guarded OWN formation, policy pause/resume, idle deduplication and failed-work no retry PASS");return 0;
}
