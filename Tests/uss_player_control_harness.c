#include "../src/uss_player_input_windows.h"
#define USS_AUTOMATIC_TEST_MAIN automatic_input_regressions
#include "uss_automatic_controller_harness.c"
static UssSettingsState player_settings;static UssPlayerControl player;static UssPlayerInputSdk input_source;
static unsigned foreground_ok,dedicated_down;
#define INPUT_UI ((uintptr_t)0xb1000000)
#define INPUT_ACTIONS (ACTION_MANAGER+0x150+0x1b68)
static int own_foreground(void* user){(void)user;return (int)foreground_ok;}
static int own_dedicated(void* user,const UssSettings* settings,unsigned* down)
{(void)user;assert(uss_settings_valid(settings));*down=dedicated_down;return 1;}
static unsigned player_update(void)
{assert(formation_refresh(&fixture_runtime));return uss_player_control(&player,&fixture_runtime.life,&fixture_runtime.latest,now_ms);}
static void player_finish(void)
{
    unsigned i;for(i=0;i<12 && fixture_runtime.work.phase==USS_PAIR_ACTIVE;++i){now_ms+=25;assert(player_update()==USS_CONTROL_WORK);}
    assert(!controller.stopped && fixture_runtime.work.phase==USS_PAIR_COMPLETE);
}
static void player_wiring(void)
{
    ptr_segment(BASE+0x347ce28,INPUT_UI);segment(INPUT_UI+0x10,8);segment(INPUT_UI+0x429c,24);
    segment(INPUT_ACTIONS+24*32,1);segment(INPUT_ACTIONS+19*32,8);
    foreground_ok=1;dedicated_down=0;uss_settings_init(&player_settings,NULL);
    memset(&input_source,0,sizeof(input_source));input_source.api=&fake;input_source.mod=MOD;
    input_source.platform.thread_id=core_thread;input_source.settings=&player_settings;input_source.runtime=&fixture_runtime;input_source.native=&fixture_native;
    input_source.foreground=own_foreground;input_source.dedicated=own_dedicated;
    memset(&player,0,sizeof(player));player.automatic=&controller;player.settings=&player_settings;
    player.user=&input_source;player.sample=uss_player_sdk_sample;player.select_single=uss_player_sdk_select_single;
}
static void player_fixture(void)
{
    unsigned i;automatic_fixture();(void)update_auto();(void)update_auto();
    put32(fixture(0x70000300,48)+8,103);put32(fixture(0x70000700,144)+48+4,0);put32(fixture(0x70000700,144)+96+4,22);
    (void)update_auto();for(i=0;i<10 && fixture_runtime.work.phase==USS_PAIR_ACTIVE;++i){now_ms+=25;(void)update_auto();}
    assert(fixture_runtime.life.phase==USS_LIFE_FORMED);
    assert(formation_refresh(&fixture_runtime));assert(runtime_select(NULL,&fixture_runtime.latest,3)==USS_PAIR_SERVICE_DONE);
    player_wiring();
}

static void input_setting_pins(void)
{
    static const struct {uintptr_t rva;const char* hex;} pins[]={
        {0x102fb73,"488d1566f92201440fb683f7000000488bce4c8b"},
        {0x1030260,"48895c2408574883ec20488d99bc0000004c8bd2488bcb"},
        {0x1030631,"488d15a8ee220188433a488b05c65c2f02488b8828010000488b8190000000488bcfffd0440fb6433f488d1597ee220188433b"},
        {0x97ca43,"488b4580488b802840010080b8f700000000750c4585e47407443b6424507507c644"},
        {0x13779a7,"488b0d92e9fa01458bc14881c1dcc30a00498bd2e8a088cbff4c8b0579"}
    };unsigned i,j;
    for(i=0;i<sizeof(pins)/sizeof(pins[0]);++i){size_t n=strlen(pins[i].hex)/2;unsigned char* bytes=segment(BASE+pins[i].rva,n);
        for(j=0;j<n;++j)bytes[j]=(unsigned char)((hex_digit(pins[i].hex[2*j])<<4)|hex_digit(pins[i].hex[2*j+1]));}
    ptr_segment(0x90000000+0x14028,0xa0000000+0xac3dc);segment(0xa0000000+0xac3dc+0xf7,1);
}
#ifndef USS_PLAYER_TEST_MAIN
#define USS_PLAYER_TEST_MAIN main
#endif
int USS_PLAYER_TEST_MAIN(void)
{
    unsigned before,i;UssCoreFlags policy;UssPickupPolicy pickup;UssPlayerInputSample raw;
    (void)automatic_input_regressions;
    player_fixture();assert(uss_player_sdk_policy(&input_source,&policy) && policy.known && policy.eligible);
    /* A one-frame hint wakes work without consuming input or entering effects.
     * Release must wake too, so the full sampler clears its previous-down bit. */
    {unsigned effect_count=fixture_native.effects;
     assert(!uss_player_sdk_support_pending(&input_source,0));
     *fixture(INPUT_ACTIONS+24*32,1)=1;
     assert(uss_player_sdk_support_pending(&input_source,0));
     *fixture(INPUT_ACTIONS+24*32,1)=0;
     assert(uss_player_sdk_support_pending(&input_source,1));
     assert(!uss_player_sdk_support_pending(&input_source,0));
     foreground_ok=0;assert(!uss_player_sdk_support_pending(&input_source,1));foreground_ok=1;
     put32(fixture(INPUT_ACTIONS+19*32,8)+4,0x3f800000);
     assert(uss_player_sdk_managed_pending(&input_source,0,0));
     put32(fixture(INPUT_ACTIONS+19*32,8)+4,0);
     assert(uss_player_sdk_managed_pending(&input_source,0,1));
     assert(!uss_player_sdk_managed_pending(&input_source,0,0));
     put32(fixture(INPUT_ACTIONS+19*32,8)+4,1);
     assert(!uss_player_sdk_managed_pending(&input_source,0,0));
     put32(fixture(INPUT_ACTIONS+19*32,8)+4,0);
     assert(fixture_native.effects==effect_count);}
    /* Connected native semantic edge -> finite OWN attachment/publication/draw. */
    assert(player_update()==USS_CONTROL_IDLE);before=fixture_native.effects;
    *fixture(INPUT_ACTIONS+24*32,1)=1;now_ms+=25;assert(player_update()==USS_CONTROL_WORK);
    assert(!strcmp(player.reason,"PLAYER_DRAW_STARTED") && fixture_runtime.work.phase==USS_PAIR_ACTIVE);
    player_finish();assert(fixture_runtime.work.real==103 && fixture_runtime.latest.inventory.held==103);
    for(i=0;i<4;++i){now_ms+=25;(void)player_update();}
    assert(fixture_native.effects>before);before=fixture_native.effects;
    now_ms+=25;(void)player_update();assert(fixture_native.effects==before); /* held != repeated press */
    *fixture(INPUT_ACTIONS+24*32,1)=0;now_ms+=25;(void)player_update();
    *fixture(INPUT_ACTIONS+24*32,1)=1;now_ms+=25;(void)player_update();player_finish();assert(fixture_runtime.work.real==102);
    assert(!fixture_runtime.controller_draw); /* Keyboard edge never opts into earlier network readiness. */
    /* A dedicated lone utility draw selects slot3 without inventing a pair or
     * doing attachment/publication work. Use a separate actual inventory scene. */
    command_fixture();put32(fixture(0x70000300,48)+8,103);
    put32(fixture(0x70000700,144)+48+4,0);put32(fixture(0x70000700,144)+96+4,22);
    {UssCorePlatform platform={0};UssAutomaticIo io={0};platform.thread_id=core_thread;io.pickup_policy=pickup_option;
        assert(uss_automatic_init(&controller,&fixture_runtime,&fake,MOD,&platform,&io));}
    player_wiring();player_settings.current.mode=USS_BIND_DEDICATED;++player_settings.revision;
    (void)player_update();(void)player_update();dedicated_down=1;now_ms+=25;before=fixture_native.effects;
    assert(player_update()==USS_CONTROL_WORK && !strcmp(player.reason,"SINGLE_UTILITY_SELECTED"));
    assert(fixture_native.effects==before+1 && !fixture_runtime.life.owner.pair_present);
    assert(formation_refresh(&fixture_runtime) && fixture_runtime.latest.inventory.held==103);
    /* Controller hold requires a real transition, even after250ms. */
    player_fixture();(void)player_update();put32(fixture(INPUT_ACTIONS+19*32,8)+4,0x3f800000);
    now_ms+=25;(void)player_update();before=fixture_native.effects;now_ms+=300;(void)player_update();assert(fixture_native.effects==before);
    assert(runtime_select(NULL,&fixture_runtime.latest,1)==USS_PAIR_SERVICE_DONE);
    {unsigned char* row=fixture(ACTION_MANAGER+0x53dcd0,32);
     put32(row+8,3);put32(row+16,2);put32(row+20,1);put32(row+28,0x3e99999a);
     put32(fixture(ACTION_MANAGER+0x53e880,24)+12,0x8000);}
    now_ms+=25;(void)player_update();assert(!strcmp(player.reason,"PLAYER_DRAW_STARTED") && fixture_runtime.controller_draw);
    player_finish();assert(fixture_runtime.work.real==103 && u32(fixture(ACTION_MANAGER+0x53dcd0,32)+28)==0x3e99999a);
    before=fixture_native.effects;now_ms+=25;(void)player_update();assert(fixture_native.effects==before);
    /* Dedicated is a utility draw command. Repeated draws do not toggle back;
     * ordinary support semantic gestures are ignored until utility is real. */
    player_fixture();player_settings.current.mode=USS_BIND_DEDICATED;++player_settings.revision;(void)player_update();
    *fixture(INPUT_ACTIONS+24*32,1)=1;now_ms+=25;(void)player_update();assert(fixture_runtime.work.real==102);
    *fixture(INPUT_ACTIONS+24*32,1)=0;dedicated_down=1;now_ms+=25;(void)player_update();player_finish();assert(fixture_runtime.work.real==103);
    dedicated_down=0;now_ms+=25;(void)player_update();dedicated_down=1;before=fixture_native.effects;now_ms+=25;(void)player_update();assert(fixture_native.effects==before);
    dedicated_down=0;now_ms+=25;(void)player_update();
    assert(runtime_select(NULL,&fixture_runtime.latest,1)==USS_PAIR_SERVICE_DONE);
    now_ms+=25;(void)player_update();assert(!strcmp(player.reason,"UTILITY_EXIT_STARTED"));player_finish();
    assert(fixture_runtime.latest.inventory.slots[2]==102 && fixture_runtime.latest.inventory.active_selector==1 && fixture_runtime.latest.inventory.held==101);
    /* Focus/menu/capture loss does not preserve a held edge. */
    player_fixture();(void)player_update();foreground_ok=0;*fixture(INPUT_ACTIONS+24*32,1)=1;
    before=fixture_native.effects;now_ms+=25;assert(player_update()==USS_CONTROL_WORK && fixture_native.effects==before);
    foreground_ok=1;now_ms+=25;(void)player_update();assert(fixture_native.effects==before);
    *fixture(INPUT_ACTIONS+24*32,1)=0;now_ms+=25;(void)player_update();
    put32(fixture(INPUT_UI+0x429c,24),1);put32(fixture(INPUT_UI+0x429c,24)+20,1);
    assert(!uss_player_sdk_policy(&input_source,&policy));assert(!uss_player_sdk_sample(&input_source,&fixture_runtime.latest,&raw));
    put32(fixture(INPUT_UI+0x429c,24)+20,0);*fixture(INPUT_UI+0x10,8)=1;assert(!uss_player_sdk_policy(&input_source,&policy));
    *fixture(INPUT_UI+0x10,8)=0;input_source.binding_capture=1;assert(!uss_player_sdk_policy(&input_source,&policy));
    input_source.binding_capture=0;assert(uss_player_sdk_policy(&input_source,&policy));
    /* A second gesture queues one fresh draw after safe completion, without
     * interrupting the first or accumulating spam. */
    player_fixture();(void)player_update();put32(fixture(ACTION_MANAGER+0x53dcd0,32)+16,3);
    *fixture(INPUT_ACTIONS+24*32,1)=1;now_ms+=25;(void)player_update();
    assert(fixture_runtime.work.phase==USS_PAIR_ACTIVE);
    *fixture(INPUT_ACTIONS+24*32,1)=0;now_ms+=25;(void)player_update();
    assert(fixture_runtime.work.phase==USS_PAIR_ACTIVE);
    *fixture(INPUT_ACTIONS+24*32,1)=1;now_ms+=25;before=fixture_native.effects;
    assert(player_update()==USS_CONTROL_WORK && !controller.stopped && player.pending.present && fixture_native.effects==before);
    *fixture(INPUT_ACTIONS+24*32,1)=0;now_ms+=25;(void)player_update();
    *fixture(INPUT_ACTIONS+24*32,1)=1;now_ms+=25;(void)player_update();assert(player.pending.present && !controller.stopped);
    *fixture(INPUT_ACTIONS+24*32,1)=0;
    put32(fixture(ACTION_MANAGER+0x53dcd0,32)+16,0);
    player_finish();assert(fixture_runtime.work.real==103 && player.pending.present);
    now_ms+=25;assert(player_update()==USS_CONTROL_WORK && !player.pending.present && fixture_runtime.work.phase==USS_PAIR_ACTIVE);
    player_finish();assert(fixture_runtime.work.real==102 && !controller.stopped);
    now_ms+=25;(void)player_update();assert(!player.pending.present && fixture_runtime.work.phase==USS_PAIR_COMPLETE);
    /* A queue is discarded when its software identity/lifetime changes. */
    player.pending.present=1;player.pending.epoch=0;
    now_ms+=25;(void)player_update();assert(!player.pending.present);
    /* Queue identity anchors reject changed pair, epoch, guard or mode, and
     * cancellation. None of these checks enters native effects. */
    player_fixture();assert(formation_refresh(&fixture_runtime));
    fixture_runtime.work.phase=USS_PAIR_ACTIVE;fixture_runtime.work.order=USS_PAIR_DRAW;
    fixture_runtime.work.started_ms=now_ms;fixture_runtime.work.deadline_ms=now_ms+2000;
    uss_player_queue_swap(&player,&fixture_runtime.life.owner,0);
    assert(uss_player_pending_current(&player,&fixture_runtime.life));before=fixture_native.effects;
    ++player_settings.revision;assert(!uss_player_pending_current(&player,&fixture_runtime.life));--player_settings.revision;
    ++fixture_runtime.life.owner.guard_generation;assert(!uss_player_pending_current(&player,&fixture_runtime.life));--fixture_runtime.life.owner.guard_generation;
    ++fixture_runtime.life.owner.pair_generation;assert(!uss_player_pending_current(&player,&fixture_runtime.life));--fixture_runtime.life.owner.pair_generation;
    fixture_runtime.work.phase=USS_PAIR_CANCELLED;assert(!uss_player_pending_current(&player,&fixture_runtime.life));
    assert(fixture_native.effects==before);
    /* Loss of UI/focus clears a queued command and never launches its draw. */
    fixture_runtime.work.phase=USS_PAIR_COMPLETE;foreground_ok=0;now_ms+=25;before=fixture_native.effects;
    assert(player_update()==USS_CONTROL_WORK && !player.pending.present && fixture_native.effects==before);
    /* Pickup option false is valid; value/context/signature failure is unknown. */
    player_fixture();input_setting_pins();assert(uss_player_sdk_pickup(&input_source,&pickup) && pickup.known && !pickup.switch_on_pickup);
    *fixture(0xa0000000+0xac3dc+0xf7,1)=1;assert(uss_player_sdk_pickup(&input_source,&pickup) && pickup.switch_on_pickup);
    *fixture(0xa0000000+0xac3dc+0xf7,1)=2;assert(!uss_player_sdk_pickup(&input_source,&pickup) && !pickup.known);
    *fixture(0xa0000000+0xac3dc+0xf7,1)=1;putptr(fixture(0x90000000+0x14028,8),0xa1000000);assert(!uss_player_sdk_pickup(&input_source,&pickup));
    player_fixture();input_setting_pins();fixture(BASE+0x97ca43,32)[0]^=1;assert(!uss_player_sdk_pickup(&input_source,&pickup));
    player_fixture();assert(formation_refresh(&fixture_runtime));reads=0;assert(uss_player_sdk_sample(&input_source,&fixture_runtime.latest,&raw));before=reads;
    for(i=1;i<=before;++i){reads=0;fail_at=i;assert(!uss_player_sdk_sample(&input_source,&fixture_runtime.latest,&raw) && !raw.known);}fail_at=0;
    puts("USS player control: SDK semantic/default/controller/dedicated input -> OWN draw/exit, UI/focus/release/interruption and live pickup policy PASS");return 0;
}
