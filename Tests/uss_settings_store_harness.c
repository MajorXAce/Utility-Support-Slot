#include <assert.h>
#include "../src/uss_settings_store.h"
int main(void)
{
    UssSettingsSaveReport report={0};
    const wchar_t* path=L"build/uss-settings-store-fixture.json";
    UssSettings saved=uss_settings_defaults(),loaded=saved;HANDLE file;DWORD written;char bytes[3]={0};
    DeleteFileW(path);DeleteFileW(L"build/uss-settings-store-fixture.json.tmp");
    assert(uss_settings_load_file(path,&loaded)==USS_SETTINGS_MISSING);
    assert(uss_settings_save_file(path,&saved));assert(uss_settings_load_file(path,&loaded)==USS_SETTINGS_LOADED);
    assert(uss_settings_equal(&saved,&loaded));saved.mode=USS_BIND_DEDICATED;saved.utility_controller=USS_BIND_RIGHT_TRIGGER;
    assert(uss_settings_save_file(path,&saved));assert(uss_settings_load_file(path,&loaded)==USS_SETTINGS_LOADED && uss_settings_equal(&saved,&loaded));
    /* A competing writer's temp file must survive; old settings remain readable. */
    file=CreateFileW(L"build/uss-settings-store-fixture.json.tmp",GENERIC_WRITE,0,NULL,CREATE_NEW,0,NULL);assert(file!=INVALID_HANDLE_VALUE);
    assert(!uss_settings_save_file_report(path,&saved,&report) && !strcmp(report.stage,"TEMP_CREATE") && report.windows_error);assert(CloseHandle(file));assert(DeleteFileW(L"build/uss-settings-store-fixture.json.tmp"));
    /* A malformed/future format is never overwritten with defaults. */
    file=CreateFileW(path,GENERIC_WRITE,0,NULL,TRUNCATE_EXISTING,0,NULL);assert(file!=INVALID_HANDLE_VALUE);
    assert(WriteFile(file,"{}",2,&written,NULL) && written==2 && CloseHandle(file));
    assert(uss_settings_load_file(path,&loaded)==USS_SETTINGS_INVALID && uss_settings_equal(&saved,&loaded));
    assert(!uss_settings_save_file_report(path,&saved,&report) && !strcmp(report.stage,"EXISTING_READ") && report.windows_error==ERROR_INVALID_DATA);
    file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);assert(file!=INVALID_HANDLE_VALUE);
    assert(ReadFile(file,bytes,3,&written,NULL) && written==2 && !memcmp(bytes,"{}",2) && CloseHandle(file));
    assert(DeleteFileW(path));puts("USS settings store: roundtrip, atomic replace, competing temp and invalid-file preservation PASS");return 0;
}
