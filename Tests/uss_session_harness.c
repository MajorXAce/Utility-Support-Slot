#define USS_APPLICATION_TEST_MAIN application_session_regressions
#include "uss_gameplay_application_harness.c"
static void session_tick(void)
{now_ms+=100;app_callback(MOD,0,app_user);}
static void session_scene(void)
{
    UssGameplayEnvironment environment=application_fixture();
    assert(uss_gameplay_application_register(&application,&fake,MOD,&environment,NULL));
    app_callback(MOD,0,app_user);session_tick();
    assert(application.initialized && application.formation.life.owner.mission && application.formation.history.remembered);
}
static void inject_stopped_actor(void)
{
    /* Fault injection represents a consumed old actor operation. No native
     * effect is claimed here; real failure/no-retry is covered by recovery. */
    application.formation.work.phase=USS_PAIR_CANCELLED;application.formation.work.attachment_consumed=1;
    application.formation.life.phase=USS_LIFE_PAUSED;application.automatic.stopped=1;
    application.recovery.work.phase=USS_RECOVERY_CANCELLED;application.loop.handling_paused=1;
    application.formation.publication.disabled=1;application.formation.publication.attempted=9;
}
static void new_avatar_graph(void)
{
    unsigned i;put32(fixture(0x20000000+0xf31ad8,24)+8,23);
    put32(fixture(0x70000100,8),23);put32(fixture(0x70000400,8),23);put32(fixture(0x80001000,8),23);
    put32(fixture(0x80002008,4),23);put32(fixture(ACTION_MANAGER+0x53dcd0,32),23);
    for(i=0;i<3;++i)if(u32(fixture(0x70000700,144)+i*48+4)==22)put32(fixture(0x70000700,144)+i*48+4,23);
}
int main(void)
{
    uint64_t epoch,generation,guard;unsigned before;UssCoreObservation stale,current;UssPairMember target;
    (void)application_session_regressions;
    session_scene();epoch=application.formation.life.owner.epoch;inject_stopped_actor();
    session_tick();assert(application.loop.handling_paused && application.automatic.stopped && application.formation.work.attachment_consumed);
    assert(!app_effects && atomic_load(&application.loop.enabled));
    /* SDK state alone cannot retire a mission. Native GameState must agree. */
    core_state=HF_STATE_SHIP;session_tick();assert(application.loop.handling_paused && application.automatic.stopped);
    put32(fixture(0xa0000000+0xac21c,4),3);session_tick();
    assert(!application.loop.handling_paused && !application.automatic.stopped && !application.formation.life.owner.mission);
    assert(application.formation.work.phase==USS_PAIR_IDLE && !application.formation.work.attachment_consumed);
    assert(application.formation.publication.disabled && application.formation.publication.attempted==9 && !app_effects);
    assert(application.settings.revision==1 && application.settings.release_required);
    core_state=HF_STATE_MISSION;put32(fixture(0xa0000000+0xac21c,4),4);session_tick();session_tick();
    assert(application.formation.life.owner.epoch==epoch+1 && application.formation.history.remembered && !app_effects);
    /* A failed read while paused does not establish a respawn. A fully matched
     * new avatar/ownership/action graph does, without an intervening ship cue. */
    session_scene();inject_stopped_actor();before=reads;fail_at=before+1;session_tick();fail_at=0;
    assert(application.automatic.stopped && application.loop.handling_paused);
    new_avatar_graph();session_tick();
    assert(!application.automatic.stopped && !application.loop.handling_paused);
    assert(application.formation.life.owner.context.local.avatar==23 && !application.formation.life.owner.pair_present);
    assert(application.formation.publication.disabled && application.formation.publication.attempted==9 && !app_effects);
    session_tick();assert(application.formation.history.remembered && !app_effects);
    /* Roster/authority changes preserve membership but invalidate stale work,
     * generation-stamped snapshots and publication context. */
    player_fixture();assert(formation_refresh(&fixture_runtime));stale=fixture_runtime.latest;
    epoch=fixture_runtime.life.owner.epoch;generation=fixture_runtime.life.owner.pair_generation;
    guard=fixture_runtime.life.owner.guard_generation;
    put32(fixture(SESSION+0x162d8,4),3);fixture(SESSION+0x162e0,32)[16]=3;
    assert(formation_refresh(&fixture_runtime));
    assert(fixture_runtime.life.owner.pair_present && fixture_runtime.life.owner.pair_generation==generation);
    assert(fixture_runtime.life.owner.context_generation>stale.expected_context && fixture_runtime.life.owner.guard_generation>guard);
    {UssPairObservation pair;UssPublicationView view;UssPublicationOwner row;
        assert(!uss_core_apply(&fixture_runtime.life.owner,7,&stale,&pair,&view,&row));}
    assert(formation_refresh(&fixture_runtime) && fixture_runtime.latest.pair_matched);
    initial_role=HF_SESSION_JOINER;fixture(SESSION+0xb3a8,8)[0]=2;
    assert(formation_refresh(&fixture_runtime) && fixture_runtime.life.owner.pair_present);
    assert(formation_refresh(&fixture_runtime) && fixture_runtime.latest.pair_matched && fixture_runtime.life.owner.epoch==epoch);
    /* A draw admitted before a roster change cannot enter its old native effect.
     * Keeping pair membership does not keep old transaction authority. */
    player_fixture();target=fixture_runtime.life.owner.utility;
    assert(uss_formation_start(&fixture_runtime,USS_PAIR_DRAW,&target));before=fixture_native.effects;
    put32(fixture(SESSION+0x162d8,4),3);fixture(SESSION+0x162e0,32)[16]=3;
    assert(uss_formation_tick(&fixture_runtime)==USS_PAIR_CANCELLED && fixture_native.effects==before && fixture_runtime.life.owner.pair_present);
    /* Fresh standard weapon/backpack values invalidate work, not membership.
     * Different actor/world ownership still forgets the old pair. */
    core_fixture();core_pair();current=core_read();stale=current;
    {UssPairOwnerContext changed=core_owner.context;changed.local.backpack=777;
        assert(uss_pair_owner_update(&core_owner,7,1,1,&changed) && core_owner.pair_present);
        changed.local.primary=778;assert(uss_pair_owner_update(&core_owner,7,1,1,&changed) && core_owner.pair_present);
        changed.local.avatar=23;assert(uss_pair_owner_update(&core_owner,7,1,1,&changed) && !core_owner.pair_present);}
    puts("USS session: verified exit/new mission/respawn retires old work, unknown does not; publisher disable preserved; roster/role retains pair but rejects stale draw/context PASS");return 0;
}
