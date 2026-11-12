/* Platform fixture uses only this test executable's PID/time/RNG and ignored files.
 * It never inspects a game process or calls the production binder. */
#define USS_CONTROL_TEST_MAIN control_regression_checks
#include "selection_control_harness.c"
#undef USS_CONTROL_TEST_MAIN
#define hellforge_mod_init hf027_fixture_init
#include "../ResearchSource/selection_probe.c"
#undef hellforge_mod_init
static HfTickCallback recorded_tick;
static unsigned registration_result,registration_count;
static int register_tick_fixture(HfMod* mod,HfTickCallback callback,void* user)
{assert(mod==MOD && callback && !user);recorded_tick=callback;++registration_count;return (int)registration_result;}
static const char* fixture_logs(HfMod* mod)
{
    static char path[1024];DWORD n;
    assert(mod==MOD);
    n=GetFullPathNameA("build/hf-uss-027/platform-fixture",1024,path,NULL);
    assert(n && n<1024);return path;
}
static void clean_fixture_files(void)
{
    static const wchar_t* names[]={L"hf027-session.bin",L"hf027-ready.bin",L"hf027-request.bin",L"hf027-arm.bin"};
    unsigned i;wchar_t path[1024];
    assert(CreateDirectoryW(L"build/hf-uss-027/platform-fixture",NULL) || GetLastError()==ERROR_ALREADY_EXISTS);
    for(i=0;i<4;++i) {
        int n=swprintf(path,1024,L"build/hf-uss-027/platform-fixture/%ls",names[i]);
        assert(n>0 && n<1024);DeleteFileW(path);
    }
}
int main(void)
{
    assert(control_regression_checks()==0);
    clean_fixture_files();
    setup();recorded_tick=NULL;registration_count=0;registration_result=0;
    fake.on_game_thread=register_tick_fixture;fake.logs_folder=fixture_logs;
    assert(hf027_fixture_init(&fake,MOD)==0);
    if(HF_USS_SELECTION_ENABLED) {
        assert(recorded_tick && registration_count==1 && !atomic_load(&selection_enabled));
        recorded_tick(MOD,0,NULL);assert(!reads && logs==1);
        setup();registration_count=0;registration_result=1;
        fake.on_game_thread=register_tick_fixture;fake.logs_folder=fixture_logs;
        assert(hf027_fixture_init(&fake,MOD)==0 && registration_count==1);
        assert(selection_control.session.pid==GetCurrentProcessId() && selection_control.session.nonce &&
               selection_control.session.process_started && selection_control.phase==0);
        recorded_tick(MOD,0,NULL);assert(!reads && logs==1 && selection_control.phase==0);
        assert(!atomic_flag_test_and_set(&selection_busy));
        recorded_tick(MOD,0,NULL);assert(!reads && logs==1);
        atomic_flag_clear(&selection_busy);
        selection_control.thread=GetCurrentThreadId()+1;
        recorded_tick(MOD,0,NULL);assert(!reads && selection_control.phase==2);
        assert(strstr(last_log,"CALLBACK_THREAD_CHANGED"));
    } else assert(!recorded_tick && !registration_count && !reads);
    clean_fixture_files();
    puts("selection wrapper: inactive/retained registration failure, own-process tokens, idle/reentrant/thread refusal PASS");
    return 0;
}
