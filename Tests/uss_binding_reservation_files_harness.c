#include <assert.h>
#include <stdio.h>
#include "../src/uss_binding_reservation_files.h"
static UssBindingReservationFiles files;
static void write_text(const wchar_t* path,const char* text)
{
    HANDLE file=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);DWORD written=0;
    assert(file!=INVALID_HANDLE_VALUE && WriteFile(file,text,(DWORD)strlen(text),&written,NULL) && written==strlen(text));assert(CloseHandle(file));
}
#ifndef USS_BIND_FILES_TEST_MAIN
#define USS_BIND_FILES_TEST_MAIN main
#endif
int USS_BIND_FILES_TEST_MAIN(void)
{
    wchar_t temporary[MAX_PATH],path[MAX_PATH],backup[MAX_PATH+5];DWORD length;HANDLE held;uint32_t code=10*65536u;
    const char* empty="format 2\nsession 1\nend\n";
    length=GetTempPathW(MAX_PATH,temporary);assert(length && length<MAX_PATH);
    assert(GetTempFileNameW(temporary,L"usb",0,path));
    {size_t n=wcslen(path);memcpy(backup,path,n*sizeof(wchar_t));memcpy(backup+n,L".bak",5*sizeof(wchar_t));}
    assert(DeleteFileW(path));
    assert(uss_binding_reservation_files_read(path,&files) && files.primary.status==USS_BIND_FILE_MISSING && files.backup.status==USS_BIND_FILE_MISSING);
    assert(uss_binding_reservation_files_query(&files,code,"USS.Utility")==USS_BIND_RESERVATION_CANDIDATE && uss_binding_reservation_files_current(&files));
    assert(uss_binding_reservation_files_query(&files,code,NULL)==USS_BIND_RESERVATION_UNKNOWN);
    write_text(path,empty);assert(!uss_binding_reservation_files_current(&files));
    assert(uss_binding_reservation_files_read(path,&files) && files.primary.status==USS_BIND_FILE_VALID && uss_binding_reservation_files_current(&files));
    write_text(backup,"format 2\nsession 1\nAbsent.Mod\t10\t0\t0\nend\n");
    assert(!uss_binding_reservation_files_current(&files));assert(uss_binding_reservation_files_read(path,&files));
    assert(uss_binding_reservation_files_query(&files,code,"USS.Utility")==USS_BIND_RESERVATION_FOREIGN);
    write_text(path,"format 3\nsession 1\nend\n");
    assert(!uss_binding_reservation_files_read(path,&files) && files.primary.status==USS_BIND_FILE_INVALID && files.backup.status==USS_BIND_FILE_VALID);
    assert(uss_binding_reservation_files_query(&files,code,"USS.Utility")==USS_BIND_RESERVATION_UNKNOWN); /* No silent backup recovery. */
    write_text(path,empty);held=CreateFileW(path,GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);assert(held!=INVALID_HANDLE_VALUE);
    assert(!uss_binding_reservation_files_read(path,&files) && files.primary.status==USS_BIND_FILE_ERROR);assert(CloseHandle(held));
    assert(!uss_binding_reservation_files_read(L"relative.assignments",&files));
    assert(DeleteFileW(path) && DeleteFileW(backup));
    puts("USS reservation files: actual file identity/content receipts, primary+backup union, replacement/future/locked refusal and no file mutations PASS");return 0;
}
