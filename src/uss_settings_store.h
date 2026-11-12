/* Own settings-file adapter. Windows system APIs only; no game memory or SDK. */
#ifndef USS_SETTINGS_STORE_H
#define USS_SETTINGS_STORE_H
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wchar.h>
#include "uss_settings.h"
typedef struct UssSettingsSaveReport {
    const char* stage;
    DWORD windows_error;
} UssSettingsSaveReport;
static inline int uss_settings_save_failed(UssSettingsSaveReport* report,const char* stage,DWORD error)
{if(report){report->stage=stage;report->windows_error=error;}return 0;}

enum {USS_SETTINGS_IO_ERROR=-1,USS_SETTINGS_INVALID=-2,USS_SETTINGS_MISSING=0,USS_SETTINGS_LOADED=1};
static inline int uss_settings_load_file(const wchar_t* path,UssSettings* output)
{
    HANDLE file;LARGE_INTEGER size;char bytes[512];DWORD received=0;int valid;
    if(!path || !output)return USS_SETTINGS_IO_ERROR;
    file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE)return GetLastError()==ERROR_FILE_NOT_FOUND?USS_SETTINGS_MISSING:USS_SETTINGS_IO_ERROR;
    if(!GetFileSizeEx(file,&size) || size.QuadPart<1 || size.QuadPart>512){CloseHandle(file);return USS_SETTINGS_INVALID;}
    valid=ReadFile(file,bytes,(DWORD)size.QuadPart,&received,NULL) && received==(DWORD)size.QuadPart;
    if(!CloseHandle(file))valid=0;
    if(!valid)return USS_SETTINGS_IO_ERROR;
    return uss_settings_decode(bytes,received,output)?USS_SETTINGS_LOADED:USS_SETTINGS_INVALID;
}
/* A replace is atomic to readers. A per-file handle excludes other USS writers;
 * an invalid/future-schema existing file is preserved, never silently replaced. */
static inline int uss_settings_save_file_report(const wchar_t* path,const UssSettings* settings,UssSettingsSaveReport* report)
{
    wchar_t temporary[1024],lock_path[1024];char bytes[512];int count,existing,result=0;
    HANDLE lock,file;DWORD written=0;UssSettings previous;size_t length;
    if(report){report->stage="VALIDATE";report->windows_error=0;}
    if(!path || !uss_settings_valid(settings))return uss_settings_save_failed(report,"VALIDATE",ERROR_INVALID_DATA);
    length=wcslen(path);if(length>980)return uss_settings_save_failed(report,"PATH_LENGTH",ERROR_FILENAME_EXCED_RANGE);
    memcpy(lock_path,path,length*sizeof(wchar_t));memcpy(lock_path+length,L".lock",6*sizeof(wchar_t));
    memcpy(temporary,path,length*sizeof(wchar_t));memcpy(temporary+length,L".tmp",5*sizeof(wchar_t));
    lock=CreateFileW(lock_path,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_TEMPORARY|FILE_FLAG_DELETE_ON_CLOSE,NULL);
    if(lock==INVALID_HANDLE_VALUE)return uss_settings_save_failed(report,"LOCK_OPEN",GetLastError());
    existing=uss_settings_load_file(path,&previous);
    if(existing!=USS_SETTINGS_LOADED && existing!=USS_SETTINGS_MISSING){
        uss_settings_save_failed(report,"EXISTING_READ",existing==USS_SETTINGS_INVALID?ERROR_INVALID_DATA:GetLastError());goto done;}
    count=uss_settings_encode(settings,bytes,sizeof(bytes));if(!count){uss_settings_save_failed(report,"ENCODE",ERROR_INVALID_DATA);goto done;}
    /* A leftover temp file is evidence of interrupted saving. Don't delete it. */
    file=CreateFileW(temporary,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE){uss_settings_save_failed(report,"TEMP_CREATE",GetLastError());goto done;}
    result=WriteFile(file,bytes,(DWORD)count,&written,NULL)!=0;
    if(!result)uss_settings_save_failed(report,"WRITE",GetLastError());
    else if(written!=(DWORD)count){result=0;uss_settings_save_failed(report,"SHORT_WRITE",ERROR_WRITE_FAULT);}
    if(result && !FlushFileBuffers(file)){result=0;uss_settings_save_failed(report,"FLUSH",GetLastError());}
    if(!CloseHandle(file) && result){result=0;uss_settings_save_failed(report,"TEMP_CLOSE",GetLastError());}
    if(result && !MoveFileExW(temporary,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){
        result=0;uss_settings_save_failed(report,"REPLACE",GetLastError());}
    if(result && report){report->stage="COMPLETE";report->windows_error=0;}
    if(!result)DeleteFileW(temporary); /* Only the temp file this call created. */
done:
    CloseHandle(lock);return result;
}
/* Legacy callers need only success; UI retains the detailed failure for logging. */
static inline int uss_settings_save_file(const wchar_t* path,const UssSettings* settings)
{return uss_settings_save_file_report(path,settings,NULL);}
#endif
