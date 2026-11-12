/* Persistent session-owned game-thread update, without a diagnostic command file.
 * Zero-initialize once and keep at a stable address for the entire session: SDK12 has
 * no tick-unregister API. stop() disables work; it does not free this context or
 * pretend that completed native effects have been rolled back. */
#ifndef USS_GAMEPLAY_LOOP_H
#define USS_GAMEPLAY_LOOP_H
#include <stdatomic.h>
#include "uss_refill_runtime.h"
enum {USS_CONTROL_IDLE=0,USS_CONTROL_WORK=1,USS_CONTROL_STOP=2};
typedef struct UssGameplayLoopIo {
    void* user;uint64_t (*clock_ms)(void*);unsigned (*thread_id)(void*);
    int (*policy)(void*,UssCoreFlags*);
    /* Owned automatic pickup/input/recovery controller. WORK means the current
     * observation may have been changed by an effect; never reuse it for refill. */
    unsigned (*control)(void*,UssCoreLifecycle*,const UssCoreObservation*,uint64_t);
    /* Before ordinary lifecycle apply: preserve the old owned pair for empty/
     * death observations. IDLE permits normal apply; WORK/STOP do not. */
    unsigned (*recover)(void*,const UssCoreObservation*,uint64_t);
    /* Read-only boundary handling, including while gameplay is paused. It may
     * retire transient software state; it must never call native game effects.
     * WORK requires a new observation before any gameplay resumes. */
    unsigned (*lifecycle)(void*,const UssCoreObservation*,uint64_t);
    UssRefillObserver refilled; /* Optional confirmed-gain observer, never another grant. */
    void (*frame)(void*,float); /* Optional UI step: each valid callback, before gameplay throttle/policy. */
} UssGameplayLoopIo;
typedef struct UssGameplayLoop {
    const HfApi* api;HfMod* mod;UssCoreLifecycle* life;UssGameplayLoopIo io;
    UssCoreFlags policy;UssRefillCore refill_core;UssRefillRuntime refill_runtime;UssRefillDriver refill;
    atomic_uint enabled,busy;unsigned attempted,registered,thread,clock_known,handling_paused,stop_reported;
    uint64_t last_ms,next_tick,updates,refill_notified;unsigned last_refill;const char* reason;
} UssGameplayLoop;
static inline void uss_gameplay_loop_stop(UssGameplayLoop* loop)
{if(loop)atomic_store_explicit(&loop->enabled,0,memory_order_release);}
/* Only schedules a fresh update; no input queue and no effect permission. */
static inline void uss_gameplay_loop_wake(UssGameplayLoop* loop)
{if(loop)loop->next_tick=0;}
static inline void uss_gameplay_loop_pause(UssGameplayLoop* loop)
{
    uss_refill_driver_clear(&loop->refill);
    if(loop->life->owner.thread==loop->thread)pair_owner_interrupt(&loop->life->owner);
}
static inline void uss_gameplay_loop_update(UssGameplayLoop* loop,float dt)
{
    UssCoreFlags flags;UssCoreObservation current;UssPairObservation pair;UssPublicationView view;UssPublicationOwner row;
    UssRefillTarget target;UssSupplySnapshot supply;UssRefillMemoryIo memory;UssRefillServices services;
    uint64_t now;unsigned thread,control,policy_ok,blocked,lifecycle=USS_CONTROL_IDLE,have_supply=0;size_t size=0;const char* build;
    thread=loop->io.thread_id(loop->io.user);now=loop->io.clock_ms(loop->io.user);
    if(!thread || (loop->thread && thread!=loop->thread) ||
        (loop->life->owner.thread && thread!=loop->life->owner.thread)){
        loop->reason="CALLBACK_THREAD_CHANGED";uss_gameplay_loop_stop(loop);return;}
    loop->thread=thread;
    if((loop->clock_known && now<loop->last_ms) || now>UINT64_MAX-100){
        loop->reason="CLOCK_INVALID";uss_gameplay_loop_pause(loop);uss_gameplay_loop_stop(loop);return;}
    loop->last_ms=now;loop->clock_known=1;
    if(loop->io.frame)loop->io.frame(loop->io.user,dt);
    if(!atomic_load_explicit(&loop->enabled,memory_order_acquire))return;
    if(now<loop->next_tick)return;
    loop->next_tick=now+(loop->life->owner.pair_present?25:100);
    memset(&loop->policy,0,sizeof(loop->policy));
    policy_ok=loop->io.policy(loop->io.user,&loop->policy) && loop->policy.known;
    /* Unknown player/UI policy cannot authorize mission reads or effects. A
     * non-mission SDK hint only permits the normal reader to VERIFY an exit;
     * that hint alone never resets the mission or a failed transaction. */
    if(!policy_ok && loop->api->game_state(loop->mod)==HF_STATE_MISSION){
        loop->reason="POLICY_PAUSED";uss_gameplay_loop_pause(loop);return;}
    blocked=!policy_ok || !loop->policy.eligible || loop->policy.disabled || loop->policy.suspended ||
        loop->policy.paused || loop->policy.down || loop->policy.input || loop->handling_paused;
    if(blocked)uss_gameplay_loop_pause(loop);
    build=loop->api->build_id();
    if(!build || strcmp(build,"25480438")){loop->reason="BUILD_UNSUPPORTED";uss_gameplay_loop_stop(loop);return;}
    loop->refill_core.base=loop->api->module_base("game.dll",&size);loop->refill_core.module_size=size;
    flags=uss_lifecycle_flags(loop->life,&loop->policy);
    current=uss_core_observe(loop->api,loop->mod,&loop->life->owner,&flags,&loop->refill_core.platform,NULL);
    ++loop->updates;
    if(loop->io.lifecycle)lifecycle=loop->io.lifecycle(loop->io.user,&current,now);
    if(lifecycle==USS_CONTROL_STOP){loop->handling_paused=1;uss_gameplay_loop_pause(loop);loop->reason="LIFECYCLE_PAUSED";return;}
    if(blocked || lifecycle==USS_CONTROL_WORK){
        /* Apply only a verified exit or actor change while paused. Same-actor
         * observations never turn an interrupted transaction back into work. */
        if(current.valid && (!current.mission || !loop->life->owner.context_valid ||
            !pair_owner_same_actor(&current.context,&loop->life->owner.context)))
            (void)uss_lifecycle_observe(loop->life,thread,&current,&pair,&view,&row);
        uss_refill_driver_clear(&loop->refill);loop->reason="LIFECYCLE_OBSERVATION_ONLY";return;
    }
    if(loop->io.recover){
        control=loop->io.recover(loop->io.user,&current,now);
        if(control!=USS_CONTROL_IDLE){
            uss_refill_driver_clear(&loop->refill);loop->next_tick=now+25;
            loop->reason=control==USS_CONTROL_WORK?"RECOVERY_ACTIVE":"RECOVERY_STOPPED";
            if(control!=USS_CONTROL_WORK){loop->handling_paused=1;uss_gameplay_loop_pause(loop);}return;
        }
    }
    if(!uss_lifecycle_observe(loop->life,thread,&current,&pair,&view,&row)){
        loop->reason="CORE_UNKNOWN";uss_refill_driver_clear(&loop->refill);return;}
    if(!current.mission){loop->reason="OUTSIDE_MISSION";uss_refill_driver_clear(&loop->refill);return;}
    control=loop->io.control(loop->io.user,loop->life,&current,now);
    if(control!=USS_CONTROL_IDLE){
        uss_refill_driver_clear(&loop->refill);loop->next_tick=now+25;
        loop->reason=control==USS_CONTROL_WORK?"CONTROL_ACTIVE":"CONTROL_STOPPED";
        if(control!=USS_CONTROL_WORK){loop->handling_paused=1;uss_gameplay_loop_pause(loop);}return;
    }
    if(!uss_refill_core_sample(&loop->refill_core,&current,&target,&supply,&have_supply)){
        loop->reason="NO_STORED_REFILL_TARGET";uss_refill_driver_clear(&loop->refill);return;}
    memory=uss_refill_core_io(&loop->refill_core);loop->refill_runtime.now=now;
    services=uss_refill_runtime_services(&loop->refill_runtime);
    loop->last_refill=uss_refill_driver_tick(&loop->refill,&memory,&services,&target,have_supply?&supply:NULL,now);
    (void)uss_refill_notify(&loop->refill.transaction,loop->last_refill,&loop->refill_notified,loop->io.user,loop->io.refilled);
    loop->reason=loop->last_refill==USS_REFILL_STOPPED?"REFILL_STOPPED":"UPDATED";
}
static void uss_gameplay_loop_callback(HfMod* mod,float dt,void* user)
{
    UssGameplayLoop* loop=(UssGameplayLoop*)user;unsigned expected=0;
    if(!loop || !atomic_load_explicit(&loop->enabled,memory_order_acquire) ||
        !atomic_compare_exchange_strong_explicit(&loop->busy,&expected,1,memory_order_acq_rel,memory_order_relaxed))return;
    if(atomic_load_explicit(&loop->enabled,memory_order_acquire)){
        if(mod!=loop->mod){loop->reason="CALLBACK_MOD_CHANGED";uss_gameplay_loop_stop(loop);}
        else uss_gameplay_loop_update(loop,dt);
        if(!atomic_load_explicit(&loop->enabled,memory_order_acquire) && !loop->stop_reported){
            loop->stop_reported=1;
            if(loop->api->log)loop->api->log(loop->mod,"USS update loop stopped reason=%s updates=%llu thread=%u dt=%.9g",
                loop->reason?loop->reason:"UNSET",(unsigned long long)loop->updates,loop->thread,(double)dt);
        }
    }
    atomic_store_explicit(&loop->busy,0,memory_order_release);
}
static inline int uss_gameplay_loop_register(UssGameplayLoop* loop,const HfApi* api,HfMod* mod,
    UssCoreLifecycle* life,const UssGameplayLoopIo* io)
{
    if(!loop || loop->attempted || !uss_sdk_compatible(api) || !mod || !life || !io ||
        !io->clock_ms || !io->thread_id || !io->policy || !io->control || !api->on_game_thread ||
        !api->build_id || !api->module_base || !api->read || !api->session || !api->game_state)return 0;
    memset(loop,0,sizeof(*loop));atomic_init(&loop->enabled,0);atomic_init(&loop->busy,0);
    loop->attempted=1;loop->api=api;loop->mod=mod;loop->life=life;loop->io=*io;loop->reason="REGISTERING";
    loop->refill_core.api=api;loop->refill_core.mod=mod;loop->refill_core.life=life;loop->refill_core.policy=&loop->policy;
    loop->refill_core.platform.user=io->user;loop->refill_core.platform.thread_id=io->thread_id;
    loop->refill_runtime.core=&loop->refill_core;
    if(!api->on_game_thread(mod,uss_gameplay_loop_callback,loop)){loop->reason="CALLBACK_REGISTRATION_FAILED";return 0;}
    loop->registered=1;loop->reason="WAITING_FOR_UPDATE";
    atomic_store_explicit(&loop->enabled,1,memory_order_release);return 1;
}
#endif
