#define USS_DIAGNOSTIC_TEST_MAIN diagnostic_regression_checks
#include "capture_diagnostic_harness.c"
#define hellforge_mod_init hf034_fixture_init
#include "../ResearchSource/capture_probe.c"
#undef hellforge_mod_init
static HfTickCallback recorded_capture_tick;
static unsigned capture_registration_result,capture_registration_count;
static int capture_register_fixture(HfMod* mod,HfTickCallback callback,void* user)
{assert(mod==MOD && callback && !user);recorded_capture_tick=callback;++capture_registration_count;return (int)capture_registration_result;}
static const char* capture_fixture_logs(HfMod* mod)
{
    static char path[1024];DWORD count;assert(mod==MOD);
    count=GetFullPathNameA("build/hf-uss-039/platform-fixture",1024,path,NULL);assert(count && count<1024);return path;
}
static void clean_capture_fixture(void)
{
    const wchar_t* names[]={L"hf034-session.bin",L"hf034-session.tmp",L"hf034-request.bin",L"hf039-identity-request.bin",L"hf039-identity-result.bin",L"hf039-identity-result.tmp"};unsigned i;wchar_t path[1024];
    assert(CreateDirectoryW(L"build/hf-uss-039/platform-fixture",NULL) || GetLastError()==ERROR_ALREADY_EXISTS);
    for(i=0;i<6;++i){assert(swprintf(path,1024,L"build/hf-uss-039/platform-fixture/%ls",names[i])>0);DeleteFileW(path);}
}
int main(void)
{
    assert(diagnostic_regression_checks()==0);clean_capture_fixture();
    setup();recorded_capture_tick=NULL;capture_registration_count=0;capture_registration_result=0;
    fake.on_game_thread=capture_register_fixture;fake.logs_folder=capture_fixture_logs;
    {uint32_t version=fake.version,size=fake.size;
        fake.version=9;assert(hf034_fixture_init(&fake,MOD)==1 && !capture_registration_count);
        fake.version=version;fake.size=(uint32_t)USS_REQUIRED_API_BYTES-1;
        assert(hf034_fixture_init(&fake,MOD)==1 && !capture_registration_count);
        fake.size=size;
    }
    assert(hf034_fixture_init(&fake,MOD)==0);
    if(HF_USS_CAPTURE_ENABLED) {
        assert(recorded_capture_tick && capture_registration_count==1 && !atomic_load(&capture_enabled));
        recorded_capture_tick(MOD,0,NULL);assert(!reads && logs==1);
        clean_capture_fixture();setup();capture_registration_result=1;
        fake.on_game_thread=capture_register_fixture;fake.logs_folder=capture_fixture_logs;
        assert(hf034_fixture_init(&fake,MOD)==0 && atomic_load(&capture_enabled));
        assert(capture_control.session.pid==GetCurrentProcessId() && capture_control.session.nonce);
        recorded_capture_tick(MOD,0,NULL);assert(!reads && !capture_control.terminal);
        {UssCaptureRequest result={0},received={0};wchar_t path[1024];HANDLE file;DWORD bytes=0;
            result.session=capture_control.session;result.mode=1;result.local_unit=11;
            assert(capture_write_identity(NULL,&result));
            assert(!capture_write_identity(NULL,&result)); /* Never overwrite result. */
            assert(capture_owned_path(path,L"hf039-identity-result.bin"));
            file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
            assert(file!=INVALID_HANDLE_VALUE && ReadFile(file,&received,sizeof(received),&bytes,NULL));
            assert(bytes==sizeof(result) && !memcmp(&result,&received,sizeof(result)) && CloseHandle(file));
        }
        assert(!atomic_flag_test_and_set(&capture_busy));recorded_capture_tick(MOD,0,NULL);assert(!reads);
        atomic_flag_clear(&capture_busy);
        {wchar_t path[1024];HANDLE file;DWORD written;UssCaptureRequest request={0};
            request.session=capture_control.session;
            assert(capture_owned_path(path,L"hf039-identity-request.bin"));
            file=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);assert(file!=INVALID_HANDLE_VALUE);
            assert(WriteFile(file,&request,3,&written,NULL) && written==3 && CloseHandle(file));
            capture_control.next_poll=0;recorded_capture_tick(MOD,0,NULL);
            assert(capture_control.terminal && !reads && strstr(last_log,"REQUEST_IDENTITY_OR_TTL"));
        }
    } else assert(!recorded_capture_tick && !capture_registration_count && !reads);
    clean_capture_fixture();puts("capture wrapper: inactive/retained failed registration, own-process files/RNG, idle/reentry/malformed file PASS");return 0;
}
