/* Execute the real normal menu callback using authored SDK memory and focus/clock.

 * No initialization export, third-party module, game, installation or file save. */

#define WIN32_LEAN_AND_MEAN

#include <windows.h>

#include <stdint.h>

static ULONGLONG fixture_now;
static unsigned fallback_held;
static unsigned fixture_focused=1,policy_calls;
static SHORT WINAPI fixture_key(int key){return key==0x36 && fallback_held?(-32767-1):0;}

static ULONGLONG WINAPI fixture_tick(void){return fixture_now;}

static HWND WINAPI fixture_foreground(void){return fixture_focused?(HWND)(uintptr_t)1:NULL;}

static DWORD WINAPI fixture_window_process(HWND window,LPDWORD process)

{(void)window;*process=GetCurrentProcessId();return GetCurrentThreadId();}

#define GetAsyncKeyState fixture_key
#define GetTickCount64 fixture_tick

#define GetForegroundWindow fixture_foreground

#define GetWindowThreadProcessId fixture_window_process

#include "../src/normal_gameplay.c"

#include <assert.h>

static unsigned char image[0x3480000],stack_memory[0x4400],menu_memory[256],screen_memory[5000000],language_memory[106680];

static HfApi fake_api;static unsigned logs;

static const char* fake_build(void){return "25480438";}

static uintptr_t fake_module(const char* name,size_t* size){assert(!strcmp(name,"game.dll"));*size=sizeof image;return 0x10000000;}

static int fake_read(uintptr_t at,void* out,size_t length)

{

    const struct {uintptr_t at;unsigned char* bytes;size_t size;} ranges[]={

        {0x10000000,image,sizeof image},{0x20000000,stack_memory,sizeof stack_memory},{0x21000000,menu_memory,sizeof menu_memory},

        {0x22000000,screen_memory,sizeof screen_memory},{0x24000000,language_memory,sizeof language_memory}};

    unsigned i;for(i=0;i<5;++i)if(at>=ranges[i].at && at-ranges[i].at<=ranges[i].size && length<=ranges[i].size-(at-ranges[i].at)){

        memcpy(out,ranges[i].bytes+(at-ranges[i].at),length);return 0;}

    return 1;

}

static void fake_log(HfMod* mod,const char* text,...){(void)mod;assert(text);++logs;}

static void pointer(unsigned char* where,uintptr_t value){memcpy(where,&value,8);}

static void prepare(void)

{

    memset(&normal,0,sizeof normal);memset(&fake_api,0,sizeof fake_api);

    memset(image,0,sizeof image);memset(stack_memory,0,sizeof stack_memory);memset(menu_memory,0,sizeof menu_memory);memset(screen_memory,0,sizeof screen_memory);

    fake_api.version=16;fake_api.size=sizeof fake_api;fake_api.log=fake_log;fake_api.build_id=fake_build;fake_api.module_base=fake_module;fake_api.read=fake_read;

    normal.app.native.api=&fake_api;normal.external_menus.attempted=2; /* no C discovery is performed in this own-process fixture */

    uss_settings_init(&normal.app.settings,NULL);uss_menu_handover_init(&normal.handover);uss_lua_mailbox_init(&normal.lua_mailbox);

    pointer(image+0x347ce28,0x20000000);pointer(image+0x347ce38,0x21000000);pointer(image+0x347cf18,0x23000000);

    pointer(image+55037640,0x24000000);pointer(menu_memory+200,0x22000000);

    uss_menu_put_word(stack_memory,0x429c,1);uss_menu_put_word(stack_memory,0x429c+20,1);

    uss_menu_put_word(screen_memory,1248+57448,4);fixture_now=0;logs=0;

}

static void post(unsigned present,unsigned ready,unsigned down,uint64_t sequence)

{

    UssLuaMenuPost p={0};p.magic=USS_LUA_MENU_MAGIC;p.protocol=USS_LUA_MENU_PROTOCOL;p.size=sizeof p;

    p.flags=present;p.generation=1;p.instance=55;p.sequence=sequence;p.options_registered=ready;p.bindings_registered=ready;p.ready=ready;

    p.input_valid=ready;p.utility_down=down;p.mode=USS_BIND_DEDICATED;assert(uss_lua_mailbox_post(&normal.lua_mailbox,&p,sizeof p)==0);

}

static uint64_t wake_clock(void* user){(void)user;return fixture_now;}
static unsigned wake_thread(void* user){(void)user;return GetCurrentThreadId();}
static int wake_policy(void* user,UssCoreFlags* flags){(void)user;memset(flags,0,sizeof *flags);++policy_calls;return 0;}
static uint32_t wake_game_state(HfMod* mod){(void)mod;return HF_STATE_MISSION;}
static void wake_frame(void* user,float dt){normal_menu_frame(user,dt,&normal.app.settings,0);}
static void wake_setup(void)
{
    prepare();fixture_focused=1;fallback_held=0;policy_calls=0;
    normal.app.settings.current.mode=USS_BIND_DEDICATED;
    post(1,1,0,1);normal_external_menus(&normal,&normal.app.settings,0,0);
    normal.app.settings.release_required=0;
    normal.app.loop.life=&normal.app.formation.life;normal.app.formation.life.owner.pair_present=1;
    normal.app.loop.api=&fake_api;fake_api.game_state=wake_game_state;
    normal.app.loop.io.user=&normal;normal.app.loop.io.clock_ms=wake_clock;normal.app.loop.io.thread_id=wake_thread;
    normal.app.loop.io.frame=wake_frame;normal.app.loop.io.policy=wake_policy;atomic_store(&normal.app.loop.enabled,1);
    normal.app.loop.next_tick=1000;
}
static void wake_regressions(void)
{
    UssMenuFirstUse untouched;unsigned i;
    wake_setup();untouched=normal.menu_first_use;
    fixture_now=1;uss_gameplay_loop_update(&normal.app.loop,0.016f);assert(!policy_calls && normal.app.loop.next_tick==1000);
    fallback_held=1;fixture_now=2;uss_gameplay_loop_update(&normal.app.loop,0.016f);
    assert(policy_calls==1 && !memcmp(&untouched,&normal.menu_first_use,sizeof untouched));
    assert(!strcmp(normal.app.loop.reason,"POLICY_PAUSED")); /* the wake did not bypass admission */
    for(i=3;i<10;++i){fixture_now=i;normal.app.loop.next_tick=1000;uss_gameplay_loop_update(&normal.app.loop,0.016f);assert(policy_calls==1);}
    fallback_held=0;fixture_now=10;uss_gameplay_loop_update(&normal.app.loop,0.016f);assert(policy_calls==2);
    wake_setup();normal.menu_first_use.taken=1;fallback_held=1;fixture_now=2;
    uss_gameplay_loop_update(&normal.app.loop,0.016f);assert(!policy_calls && normal.app.loop.next_tick==1000);
    /* Provider loss can use fallback only in the already-existing Lua-owned path. */
    post(1,0,0,2);normal_external_menus(&normal,&normal.app.settings,1,2);
    assert(!normal.handover.usable && normal.app.settings.external && normal.app.loop.next_tick==0);
    wake_setup();normal.arbiter.owner=USS_SOURCE_C;fallback_held=1;normal_fallback_wake(&normal,&normal.app.settings);assert(normal.app.loop.next_tick==1000);
    normal.arbiter.owner=USS_SOURCE_LUA;normal.app.settings.current.mode=USS_BIND_DEFAULT;
    normal_fallback_wake(&normal,&normal.app.settings);assert(normal.app.loop.next_tick==1000);
    normal.app.settings.current.mode=USS_BIND_DEDICATED;normal.app.settings.external=0;
    normal_fallback_wake(&normal,&normal.app.settings);assert(normal.app.loop.next_tick==1000);
    normal.app.settings.external=1;fixture_focused=0;
    normal_fallback_wake(&normal,&normal.app.settings);assert(normal.app.loop.next_tick==1000);
    fixture_focused=1;normal.app.settings.release_required=1;
    normal_fallback_wake(&normal,&normal.app.settings);assert(normal.app.loop.next_tick==0 && normal.app.settings.release_required && !normal.menu_first_use.taken);
    puts("FALLBACK WAKE PARITY: PASS actual frame/loop press/release, throttle, held/no-rescan, takeover, source/focus guards and ordinary admission");
}
int main(void)

{

    UssSettings saved;static UssMenuSessionNode old;unsigned before;

    prepare();saved=normal.app.settings.current;post(1,0,0,1);

    normal_menu_frame(&normal,0.016f,&normal.app.settings,0);

    assert(normal.menu_ui.presence && normal.menu_ui.deferred && !normal.menus.current && !normal.menu_stopped);

    assert(!normal.app.settings.external && !normal.external_down && uss_settings_equal(&saved,&normal.app.settings.current));

    assert(uss_menu_word(screen_memory,1248+57448)==4);

    puts("actual normal callback: identified partial Lua options defer bootstrap and retain saved settings PASS");

    prepare();post(1,1,0,1);normal_external_menus(&normal,&normal.app.settings,0,0);

    assert(normal.app.settings.external && !normal.external_down);

    fixture_now=16;post(1,1,1,2);normal_external_menus(&normal,&normal.app.settings,0,16);

    assert(!normal.external_menus.menus.provider && normal.external_down);

    fixture_now=32;post(1,0,0,3);normal_external_menus(&normal,&normal.app.settings,1,32);assert(!normal.external_down);

    puts("actual normal input path: Lua press works without C discovery; readiness loss suppresses input during transaction PASS");

    prepare();memset(&old,0,sizeof old);old.menu.prepared=1;old.menu.lifecycle.settings=&normal.app.settings;

    old.sdk.api=&fake_api;old.sdk.base=0x10000000;old.sdk.module_size=sizeof image;old.sdk.thread=GetCurrentThreadId();old.sdk.epoch=1;

    old.menu.owner.observer=uss_menu_sdk_observer(&old.sdk);normal.menus.current=&old;normal.menus.retained=&old;

    normal.menu_stopped=1;normal.menu_recover_after_close=1;post(1,0,0,1);

    normal_menu_frame(&normal,0.016f,&normal.app.settings,0);

    assert(normal.menu_stopped && normal.menu_ui.close_required && normal.menus.current==&old && !old.writer.attempts);

    uss_menu_put_word(stack_memory,0x429c+20,0);fixture_now=100;

    normal_menu_frame(&normal,0.016f,&normal.app.settings,0);

    assert(!normal.menu_stopped && !normal.menus.current && normal.menus.retained==&old && old.menu.retired);

    before=old.writer.attempts;uss_menu_put_word(stack_memory,0x429c+20,1);fixture_now=200;

    normal_menu_frame(&normal,0.016f,&normal.app.settings,0);

    assert(normal.menu_ui.deferred && !normal.menus.current && old.writer.attempts==before && !normal.menu_stopped);

    puts("actual normal callback: prior refused lifetime retires only on positive close; reopening defers without recycling old node PASS");

    prepare();memset(&old,0,sizeof old);old.menu.prepared=1;old.menu.lifecycle.settings=&normal.app.settings;

    old.sdk.api=&fake_api;old.sdk.base=0x10000000;old.sdk.module_size=sizeof image;old.sdk.thread=GetCurrentThreadId();old.sdk.epoch=1;

    old.menu.owner.observer=uss_menu_sdk_observer(&old.sdk);normal.menus.current=&old;normal.menus.retained=&old;

    normal.menu_stopped=1;normal.menu_recover_after_close=1;uss_menu_put_word(stack_memory,0x429c+20,0);

    normal_menu_frame(&normal,0.016f,&normal.app.settings,0);

    assert(!normal.menu_stopped && !normal.menus.current && old.menu.retired);

    fixture_now=100;post(1,0,0,1);uss_menu_put_word(stack_memory,0x429c+20,1);

    normal_menu_frame(&normal,0.016f,&normal.app.settings,0);

    assert(normal.menu_ui.deferred && !normal.menu_stopped && !normal.menus.current);

    puts("actual normal callback: close before companion announcement still ends the refused lifetime safely PASS");

    prepare();normal.app.settings.current.mode=USS_BIND_DEDICATED;
    normal.app.settings.external=1;normal.app.settings.release_required=0;normal.arbiter.owner=USS_SOURCE_LUA;normal.handover.usable=1;
    {unsigned down=99;fixture_now=0;fallback_held=0;assert(normal_dedicated(&normal,&normal.app.settings.current,&down)&&!down);
     fixture_now=10;fallback_held=1;assert(normal_dedicated(&normal,&normal.app.settings.current,&down)&&down&&!normal.menu_first_use.taken);
     fixture_now=20;fallback_held=0;assert(normal_dedicated(&normal,&normal.app.settings.current,&down)&&!down);
     fixture_now=200;normal.external_down=1;assert(normal_dedicated(&normal,&normal.app.settings.current,&down)&&down&&normal.menu_first_use.taken);
     fixture_now=210;normal.external_down=0;fallback_held=1;assert(normal_dedicated(&normal,&normal.app.settings.current,&down)&&!down);
     normal.arbiter.owner=USS_SOURCE_C;normal.external_down=0;assert(normal_dedicated(&normal,&normal.app.settings.current,&down)&&!down);
     normal.external_down=1;fallback_held=0;assert(normal_dedicated(&normal,&normal.app.settings.current,&down)&&down);
     normal.app.settings.external=0;normal.app.settings.current.utility_key=0x37;fallback_held=1;
     assert(normal_dedicated(&normal,&normal.app.settings.current,&down)&&!down);
     normal.app.settings.current.utility_key=0x36;assert(normal_dedicated(&normal,&normal.app.settings.current,&down)&&down);}
    puts("actual dedicated input: Lua fallback6 then first use; C and standalone sources unchanged PASS");
    wake_regressions();
    puts("NORMAL MENU PRESENCE CALLBACK: PASS");return 0;

}
