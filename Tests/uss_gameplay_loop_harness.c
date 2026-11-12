#include "../src/uss_gameplay_windows.h"
#define USS_REFILL_CORE_TEST_MAIN refill_core_regressions
#include "uss_refill_core_harness.c"
static HfTickCallback registered_callback;static void* registered_user;
static unsigned registrations,registration_result,policy_known,control_result,controls,thread_value,reenter;
static uint64_t now_ms;static UssGameplayLoop loop;
static unsigned frames,frame_reenter,frame_wake;static float frame_dt;
static void loop_frame(void* user,float dt)
{(void)user;++frames;frame_dt=dt;if(frame_wake)uss_gameplay_loop_wake(&loop);if(frame_reenter)registered_callback(MOD,dt,registered_user);}
static uint64_t loop_clock(void* user){(void)user;return now_ms;}
static unsigned loop_thread(void* user){(void)user;return thread_value;}
static int loop_policy(void* user,UssCoreFlags* out)
{(void)user;*out=core_flags;return (int)policy_known;}
static unsigned loop_control(void* user,UssCoreLifecycle* owner,const UssCoreObservation* observation,uint64_t now)
{
    (void)user;(void)now;assert(owner==&life && observation->valid);++controls;
    if(reenter)registered_callback(MOD,0,registered_user);
    return control_result;
}
static int register_loop(HfMod* mod,HfTickCallback callback,void* user)
{
    assert(mod==MOD);++registrations;registered_callback=callback;registered_user=user;
    callback(mod,0,user); /* SDK may invoke synchronously: still disabled. */
    assert(!controls);return (int)registration_result;
}
static UssGameplayLoopIo loop_fixture(void)
{
    UssGameplayLoopIo io={NULL,loop_clock,loop_thread,loop_policy,loop_control,NULL,NULL,NULL,loop_frame};
    refill_core_fixture();
    {unsigned char* bytes=segment(0x80002000,24);put32(bytes+8,22);
        bytes=segment(ACTION_MANAGER+0x53dcd0+0x9ec,16);put32(bytes,22);}
    memset(&loop,0,sizeof(loop));registered_callback=NULL;registered_user=NULL;
    registrations=controls=reenter=frames=frame_reenter=0;registration_result=policy_known=1;control_result=USS_CONTROL_IDLE;
    thread_value=7;now_ms=1000;fake.on_game_thread=register_loop;return io;
}
int main(void)
{
    UssGameplayLoopIo io;unsigned before;UssSupplySnapshot supply;UssRefillTarget target;unsigned have_supply;UssCoreObservation observation;
    (void)refill_core_regressions;
    {UssGameplayLoopIo windows=uss_gameplay_windows_io(NULL,loop_policy,loop_control);uint64_t first=windows.clock_ms(NULL);
        assert(windows.thread_id(NULL)==GetCurrentThreadId() && windows.clock_ms(NULL)>=first);}

    io=loop_fixture();assert(uss_gameplay_loop_register(&loop,&fake,MOD,&life,&io));
    assert(registrations==1 && !controls && !loop.updates);registered_callback(MOD,0,registered_user);
    assert(controls==1 && loop.updates==1 && loop.refill.pickups.have_previous);
    before=reads;now_ms=1001;registered_callback(MOD,0,registered_user);assert(reads==before && controls==1);
    reenter=1;now_ms=1025;registered_callback(MOD,0,registered_user);assert(controls==2 && loop.updates==2);
    assert(!uss_gameplay_loop_register(&loop,&fake,MOD,&life,&io) && registrations==1);
    uss_gameplay_loop_stop(&loop);before=reads;now_ms=1050;registered_callback(MOD,0,registered_user);assert(reads==before);
    io=loop_fixture();assert(uss_gameplay_loop_register(&loop,&fake,MOD,&life,&io));
    registered_callback(MOD,0,registered_user);assert(controls==1);
    /* A16ms press followed by a32ms release falls between the normal25ms
     * samples. Frame hint wakes both, without a queued event or a fixed delay. */
    frame_wake=1;now_ms=1016;registered_callback(MOD,0,registered_user);assert(controls==2);
    now_ms=1032;registered_callback(MOD,0,registered_user);assert(controls==3);
    frame_wake=0;now_ms=1040;registered_callback(MOD,0,registered_user);assert(controls==3);
    /* Full callback sees a real-layout Pack event, then refuses the synthetic
     * native image before entry. Its consumed event cannot be retried. */
    io=loop_fixture();assert(uss_gameplay_loop_register(&loop,&fake,MOD,&life,&io));registered_callback(MOD,0,registered_user);
    put32(fixture(0xd0003000,8),2);put32(fixture(0xd2000000,8),3);
    now_ms=1025;registered_callback(MOD,0,registered_user);now_ms=1225;registered_callback(MOD,0,registered_user);
    assert(loop.refill.pickups.serial==1 && loop.refill.transaction.last_serial==1 && !loop.refill.transaction.calls);
    now_ms=1250;registered_callback(MOD,0,registered_user);assert(loop.refill.pickups.serial==1 && !loop.refill.transaction.calls);
    io=loop_fixture();registration_result=0;assert(!uss_gameplay_loop_register(&loop,&fake,MOD,&life,&io));
    registered_callback(MOD,0,registered_user);assert(!controls && !reads);
    io=loop_fixture();assert(uss_gameplay_loop_register(&loop,&fake,MOD,&life,&io));policy_known=0;
    registered_callback(MOD,0,registered_user);assert(!controls && !reads && !life.owner.ready);
    policy_known=1;now_ms=1025;registered_callback(MOD,0,registered_user);assert(controls==1 && life.owner.ready);
    io=loop_fixture();assert(uss_gameplay_loop_register(&loop,&fake,MOD,&life,&io));control_result=USS_CONTROL_WORK;
    registered_callback(MOD,0,registered_user);assert(controls==1 && !loop.refill.pickups.bound);
    /* Gameplay STOP latches handling but preserves read-only boundary polling.
     * Same scene never executes its controller again; explicit stop stays hard. */
    io=loop_fixture();assert(uss_gameplay_loop_register(&loop,&fake,MOD,&life,&io) && !frames);
    frame_reenter=1;registered_callback(MOD,0.016f,registered_user);assert(frames==1 && frame_dt==0.016f);
    before=reads;now_ms=1001;registered_callback(MOD,0.02f,registered_user);
    assert(frames==2 && frame_dt==0.02f && reads==before); /* UI bypasses only the gameplay throttle. */
    thread_value=8;registered_callback(MOD,0,registered_user);assert(frames==2 && !atomic_load(&loop.enabled));
    io=loop_fixture();control_result=USS_CONTROL_STOP;
    assert(uss_gameplay_loop_register(&loop,&fake,MOD,&life,&io));registered_callback(MOD,0,registered_user);
    assert(loop.handling_paused && atomic_load(&loop.enabled) && controls==1);
    now_ms+=25;registered_callback(MOD,0,registered_user);assert(controls==1 && loop.handling_paused && atomic_load(&loop.enabled));
    io=loop_fixture();assert(uss_gameplay_loop_register(&loop,&fake,MOD,&life,&io));registered_callback(MOD,0,registered_user);
    thread_value=8;now_ms=1025;before=reads;registered_callback(MOD,0,registered_user);
    assert(!atomic_load(&loop.enabled) && reads==before);
    io=loop_fixture();assert(uss_gameplay_loop_register(&loop,&fake,MOD,&life,&io));registered_callback(MOD,0,registered_user);
    now_ms=999;before=reads;registered_callback(MOD,0,registered_user);assert(!atomic_load(&loop.enabled) && reads==before);
    io=loop_fixture();assert(uss_gameplay_loop_register(&loop,&fake,MOD,&life,&io));registered_callback((HfMod*)1,0,registered_user);
    assert(!atomic_load(&loop.enabled) && !controls);
    /* Reusing the normal update's observation performs fewer reads than the
     * independent fresh mutation boundary. This is count evidence, not latency. */
    refill_core_fixture();assert(uss_refill_core_supply(&core,&supply));before=reads;
    reads=0;observation=life_read();assert(observation.valid);
    assert(uss_refill_core_sample(&core,&observation,&target,&supply,&have_supply) && have_supply);
    assert(reads<before);printf("Refill fixture fresh boundary=%u reads; update observation+sample=%u reads\n",before,reads);
    ++life.owner.guard_generation;assert(!uss_refill_core_sample(&core,&observation,&target,&supply,&have_supply));
    puts("USS game loop: SDK registration/reentrancy, thread/clock/stop/context ownership, one update Core read, sample reuse and fresh-boundary separation PASS");return 0;
}
