/* Event-order scenes for the legacy recovery path (consumed or dropped real member, stored survivor still attached), over owned
 * memory only: no game, no live reproduction. Each scene ends with one SCENARIO line (phase, reason, native effects, survivor
 * slot, pair kept) so the same program can be compiled against the previous source and the differences read directly.
 * The scenes model an interrupted recovery; they do not establish what the reporter's knockdown actually did. */
#include <limits.h>
#define USS_RECOVERY_TEST_MAIN recovery_regression_checks
#include "uss_recovery_harness.c"
static const char* phase_name(UssRecoveryPhase phase)
{
    switch(phase){case USS_RECOVERY_IDLE:return "IDLE";case USS_RECOVERY_ATTACH:return "ATTACH";case USS_RECOVERY_PRIMARY:return "PRIMARY";
        case USS_RECOVERY_SEND:return "SEND";case USS_RECOVERY_SELECTION:return "SELECTION";case USS_RECOVERY_DEATH_WAIT:return "DEATH_WAIT";
        case USS_RECOVERY_COMPLETE:return "COMPLETE";case USS_RECOVERY_CANCELLED:return "CANCELLED";}
    return "?";
}
static void scene_report(const char* name,uint32_t survivor)
{
    printf("SCENARIO %s phase=%s reason=%s effects=%u slot2=%u survivor_parent=%u pair=%u\n",name,phase_name(recovery.work.phase),
        recovery.work.reason?recovery.work.reason:"NONE",recovery_effects,u32(fixture(0x70000300,48)+8),
        u32(fixture(0x70000700,240)+(survivor-101)*48+4),(unsigned)automatic_runtime.life.owner.pair_present);
}
/* Print the scene, then (unless compiled against the previous source for comparison) require the expected outcome. */
static void scene_check(const char* name,uint32_t survivor,const char* phase,const char* reason,unsigned effects,unsigned slot2,unsigned pair)
{
    scene_report(name,survivor);
#ifndef USS_RECOVERY_SCENES_BASELINE
    assert(!strcmp(phase_name(recovery.work.phase),phase));
    assert(recovery.work.reason && !strcmp(recovery.work.reason,reason));
    assert(recovery_effects==effects && u32(fixture(0x70000300,48)+8)==slot2 && (unsigned)automatic_runtime.life.owner.pair_present==pair);
    /* A stopped recovery is never silent: the log names the reason, the phase and the consumed-effect mask. Scenes driven through
     * the persistent loop log other lines afterwards, so only the direct scenes are matched against the LAST line. */
    if(!strcmp(phase,"CANCELLED") && strncmp(name,"policy_blip",11)){
        char expected[160];
        snprintf(expected,sizeof(expected),"USS recovery stopped: reason=%s phase=CANCELLED consumed=0x%x elapsed_ms=",reason,recovery.work.consumed);
        assert(strstr(last_log,expected));
    }
#else
    (void)phase;(void)reason;(void)effects;(void)slot2;(void)pair;
#endif
}
/* Advance by step_ms per update until the recovery completes/cancels or ticks run out. */
static void scene_run(unsigned ticks,unsigned step_ms)
{
    unsigned i;
    for(i=0;i<ticks && recovery.work.phase!=USS_RECOVERY_COMPLETE && recovery.work.phase!=USS_RECOVERY_CANCELLED;++i){
        replacement_now+=step_ms;(void)recovery_update();}
}
static unsigned input_refusals_left;
/* Stands in for the player-input gate the application installs: refuses while the player has a fresh support/dedicated press
 * edge. Refusal happens BEFORE the restore enters any native call. UINT_MAX refuses forever. */
static int scene_input_clear(void* user,const UssRecoverySnapshot* snapshot)
{
    (void)user;(void)snapshot;
    if(input_refusals_left){if(input_refusals_left!=UINT_MAX)--input_refusals_left;return 0;}
    return 1;
}
static unsigned char* action_row(void){return fixture(ACTION_MANAGER+0x53dcd0,32);}
int main(void)
{
    const uint32_t survivor=102; /* recovery_scene(1,...): the Silo (103) was the real member, the ordinary weapon (102) survives */
    unsigned i;
    (void)recovery_regression_checks;
    /* Control: ordinary order. Silo consumed while the primary is selected; vanilla has already returned to the primary. */
    recovery_scene(1,1,HF_SESSION_HOST);empty_support(1,1);scene_run(1,25);scene_run(40,25);scene_check("control_consumed_primary",survivor,"COMPLETE","LONE_RECOVERY_COMPLETE",2,102,0);
    /* Selection still on the emptied support slot (3) when recovery starts; vanilla moves it to the primary 300 ms later. */
    recovery_scene(1,1,HF_SESSION_HOST);empty_support(1,3);
    scene_run(1,25);scene_run(12,25);set_selected(1);scene_run(40,25);scene_check("selection_still_on_empty_slot",survivor,"COMPLETE","LONE_RECOVERY_COMPLETE",2,102,0);
    /* One unreadable observation after recovery began, before any effect. */
    recovery_scene(1,1,HF_SESSION_HOST);empty_support(1,1);scene_run(1,25);
    {unsigned core_reads;UssCorePlatform platform={0};UssCoreFlags flags=uss_lifecycle_flags(&automatic_runtime.life,&core_flags);
     platform.thread_id=core_thread;reads=0;(void)uss_core_observe(&fake,MOD,&automatic_runtime.life.owner,&flags,&platform,NULL);core_reads=reads;
     /* The first read of the RECOVERY observation (the ordinary Core observation runs first in this fixture). */
     reads=0;fail_at=core_reads+1;replacement_now+=25;(void)recovery_update();fail_at=0;}
    scene_run(40,25);scene_check("one_unreadable_observation",survivor,"COMPLETE","LONE_RECOVERY_COMPLETE",2,102,0);
    /* Native primary-selection block (knockdown-like) lasting 3 s, cleared afterwards. */
    recovery_scene(1,1,HF_SESSION_HOST);empty_support(1,1);scene_run(1,25);
    put32(action_row()+28,1);scene_run(30,100);put32(action_row()+28,0);scene_run(40,25);scene_check("primary_blocked_3s",survivor,"COMPLETE","LONE_RECOVERY_COMPLETE",2,102,0);
    /* The same block lasting 12 s. */
    recovery_scene(1,1,HF_SESSION_HOST);empty_support(1,1);scene_run(1,25);
    put32(action_row()+28,1);scene_run(120,100);put32(action_row()+28,0);scene_run(40,25);scene_check("primary_blocked_12s",survivor,"CANCELLED","RECOVERY_RESTORE_WAIT_EXPIRED",0,0,1);
    /* The survivor is deleted by the game while recovery waits. */
    recovery_scene(1,1,HF_SESSION_HOST);empty_support(1,1);scene_run(1,25);
    lose_entity(survivor);scene_run(40,100);scene_check("survivor_deleted",survivor,"CANCELLED","RECOVERY_SURVIVOR_GONE",0,0,1);
    /* A different actor/session lifetime appears mid-recovery. */
    recovery_scene(1,1,HF_SESSION_HOST);empty_support(1,1);scene_run(1,25);
    ++automatic_runtime.life.owner.context_generation;scene_run(40,25);scene_check("owner_lifetime_changed",survivor,"CANCELLED","RECOVERY_LIFETIME_CHANGED",0,0,1);
    /* Vanilla empties the standard weapons (death shape), then they return (stood up). */
    recovery_scene(1,1,HF_SESSION_HOST);empty_support(1,1);
    {uint32_t primary=u32(fixture(0x70000300,48)),secondary=u32(fixture(0x70000300,48)+4);
     memset(fixture(0x70000300,48),0,8);scene_run(1,25);scene_run(3,25);
     put32(fixture(0x70000300,48),primary);put32(fixture(0x70000300,48)+4,secondary);scene_run(40,25);}
    scene_check("death_shape_then_stood_up",survivor,"CANCELLED","DEATH_CONTEXT_CHANGED",0,0,1);
    /* A different weapon fills the support slot while recovery waits: definitive, never waited out, no effect. */
    recovery_scene(1,1,HF_SESSION_HOST);empty_support(1,1);scene_run(1,25);
    put32(fixture(0x70000300,48)+8,104);scene_run(40,25);scene_check("support_slot_filled_by_player",survivor,"CANCELLED","RECOVERY_CONTEXT_CHANGED",0,104,1);
    /* A reading that stays unavailable (state pin changed) for longer than the pre-effect bound: nothing is called, then it expires. */
    recovery_scene(1,1,HF_SESSION_HOST);empty_support(1,1);scene_run(1,25);
    fixture(BASE+0x144024,7)[0]^=1;scene_run(60,100);
    scene_check("persistently_unreadable_6s_still_waiting",survivor,"ATTACH","WAIT_RECOVERY_READ_UNKNOWN",0,0,1);
    scene_run(60,100);scene_check("persistently_unreadable_12s_expired",survivor,"CANCELLED","RECOVERY_RESTORE_WAIT_EXPIRED",0,0,1);
    fixture(BASE+0x144024,7)[0]^=1;
    /* After the restore effect is consumed the wait no longer applies: an unreadable verification stays terminal (uss_recovery covers it). */
    /* The player presses the support/dedicated key while recovery is about to restore: the input gate refuses three times BEFORE any
     * native call (the restore is not entered), then the key is released. */
    recovery_scene(1,1,HF_SESSION_HOST);empty_support(1,1);recovery.native.input_clear=scene_input_clear;input_refusals_left=3;
    scene_run(1,25);scene_run(80,25);input_refusals_left=0;
    scene_check("key_edge_refuses_restore_three_times",survivor,"COMPLETE","LONE_RECOVERY_COMPLETE",2,102,0);
    /* The same refusal that never clears: it waits out the pre-effect bound and stops with no native call. */
    recovery_scene(1,1,HF_SESSION_HOST);empty_support(1,1);recovery.native.input_clear=scene_input_clear;input_refusals_left=UINT_MAX;
    scene_run(1,25);scene_run(130,100);input_refusals_left=0;
    scene_check("key_edge_never_clears_12s",survivor,"CANCELLED","RECOVERY_RESTORE_WAIT_EXPIRED",0,0,1);
    /* A restore whose native call WAS entered and then refused is consumed and terminal: it is never retried. */
    recovery_scene(1,1,HF_SESSION_HOST);empty_support(1,1);scene_run(1,25);recovery_fail=1;scene_run(80,25);recovery_fail=0;
    scene_check("restore_entered_then_refused",survivor,"CANCELLED","SURVIVOR_RESTORE_FAILED",1,0,1);
    /* A policy block (focus/UI) for one tick through the persistent loop interrupts the owner's guard generation. */
    recovery_scene(1,1,HF_SESSION_HOST);empty_support(1,1);
    {UssGameplayLoopIo io={0};uint64_t guard=automatic_runtime.life.owner.guard_generation;
     memset(&recovery_loop,0,sizeof(recovery_loop));fake.on_game_thread=register_recovery_loop;
     io.clock_ms=replacement_clock_fixture;io.thread_id=core_thread;io.policy=recovery_loop_policy;io.control=recovery_loop_idle;io.recover=recovery_loop_pre;
     assert(uss_gameplay_loop_register(&recovery_loop,&fake,MOD,&automatic_runtime.life,&io));
     replacement_now+=25;recovery_callback(MOD,0,recovery_callback_user);
     core_flags.input=1;replacement_now+=25;recovery_callback(MOD,0,recovery_callback_user);core_flags.input=0;
     for(i=0;i<40 && recovery.work.phase!=USS_RECOVERY_COMPLETE && recovery.work.phase!=USS_RECOVERY_CANCELLED;++i){
        replacement_now+=25;recovery_callback(MOD,0,recovery_callback_user);}
     printf("SCENARIO policy_blip_mid_recovery guard_bumped=%u ",(unsigned)(automatic_runtime.life.owner.guard_generation!=guard));
     scene_check("policy_blip_mid_recovery_state",survivor,"CANCELLED","RECOVERY_LIFETIME_CHANGED",0,0,1);}
    puts("Recovery interruption scenes printed PASS");
    return 0;
}
