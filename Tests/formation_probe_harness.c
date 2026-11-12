#define USS_COMMAND_TEST_MAIN command_regression_checks
#include "formation_command_harness.c"
#define hellforge_mod_init hf049_fixture_init
#include "../ResearchSource/formation_probe.c"
#undef hellforge_mod_init
static HfTickCallback prototype_callback;
static unsigned prototype_registration;
static int register_prototype(HfMod* mod,HfTickCallback callback,void* user)
{assert(mod==MOD && callback && !user);prototype_callback=callback;return (int)prototype_registration;}
static const char* prototype_logs(HfMod* mod)
{
    static char path[1024];DWORD n;assert(mod==MOD);
    n=GetFullPathNameA("build/hf-uss-049/platform-fixture",1024,path,NULL);assert(n && n<1024);return path;
}
static void clean_prototype_files(void)
{
    wchar_t path[1024];unsigned i;
    assert(CreateDirectoryW(L"build/hf-uss-049",NULL) || GetLastError()==ERROR_ALREADY_EXISTS);
    assert(CreateDirectoryW(L"build/hf-uss-049/platform-fixture",NULL) || GetLastError()==ERROR_ALREADY_EXISTS);
    DeleteFileW(L"build/hf-uss-049/platform-fixture/hf049-session.bin");
    DeleteFileW(L"build/hf-uss-049/platform-fixture/hf049-session.tmp");
    for(i=1;i<=16;++i){assert(swprintf(path,1024,L"build/hf-uss-049/platform-fixture/hf049-request-%02u.bin",i)>0);DeleteFileW(path);}
}
static void own_request(const void* data,DWORD size,unsigned sequence)
{
    wchar_t name[64],path[1024];DWORD written;HANDLE file;
    assert(swprintf(name,64,L"hf049-request-%02u.bin",sequence)>0 && prototype_path(path,name));
    file=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);assert(file!=INVALID_HANDLE_VALUE);
    assert(WriteFile(file,data,size,&written,NULL) && written==size && CloseHandle(file));
}
int main(void)
{
    UssFormationRequest command_request={0};unsigned before;
    assert(command_regression_checks()==0);clean_prototype_files();command_fixture();
    fake.logs_folder=prototype_logs;fake.on_game_thread=register_prototype;prototype_registration=1;
    assert(hf049_fixture_init(&fake,MOD)==0 && prototype_callback && !prototype_terminal);
    before=reads;prototype_callback(MOD,0,NULL);assert(reads==before && !prototype_initialized);
    /* Own-process real file transport, synthetic SDK graph. Remember performs
     * observations only; fixture_native effects remain zero without a form command. */
    command_request.ticket=prototype_ticket;command_request.sequence=1;command_request.action=USS_COMMAND_REMEMBER;
    command_request.role=HF_SESSION_HOST;command_request.issued_ms=GetTickCount64();own_request(&command_request,sizeof(command_request),1);
    prototype_next=0;prototype_callback(MOD,0,NULL);
    assert(!prototype_terminal && prototype_initialized && prototype_runtime.history.remembered && !prototype_native.effects);
    before=reads;prototype_next=0;prototype_callback(MOD,0,NULL);assert(reads==before && prototype_sequence==2);
    own_request(&command_request,3,2);prototype_next=0;prototype_callback(MOD,0,NULL);
    assert(prototype_terminal && reads==before && !prototype_native.effects);
    prototype_callback(MOD,0,NULL);assert(reads==before);
    clean_prototype_files();puts("Formation wrapper: startup idle, explicit read-only remember, no replay, malformed terminal PASS");return 0;
}
