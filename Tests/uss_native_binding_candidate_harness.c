#include "../src/uss_native_binding_context.h"
#define USS_BIND_CONTEXT_TEST_MAIN context_tests
#include "uss_native_binding_context_harness.c"
#define USS_BIND_FILES_TEST_MAIN file_tests
#include "uss_binding_reservation_files_harness.c"
int main(void)
{
    UssMenuObservationIo io;UssNativeBindingCache live={0},defaults={0};UssNativeBindingContext result={0},before;
    wchar_t temporary[MAX_PATH],path[MAX_PATH],backup[MAX_PATH+5];DWORD length;
    assert(context_tests()==0 && file_tests()==0);
    length=GetTempPathW(MAX_PATH,temporary);assert(length && length<MAX_PATH);assert(GetTempFileNameW(temporary,L"usc",0,path));
    {size_t n=wcslen(path);memcpy(backup,path,n*sizeof(wchar_t));memcpy(backup+n,L".bak",5*sizeof(wchar_t));}
    write_text(path,"format 2\nsession 1\nend\n");assert(uss_binding_reservation_files_read(path,&files));
    io=context_fixture();assert(uss_native_binding_candidate(&files,&io,10,0,"USS.Utility",&live,&defaults,&result));before=result;
    write_text(path,"format 2\nsession 1\nOther.Mod\t10\t0\t1\nend\n");
    io=context_fixture();assert(!uss_native_binding_candidate(&files,&io,10,0,"USS.Utility",&live,&defaults,&result) && !memcmp(&result,&before,sizeof(result)));
    assert(uss_binding_reservation_files_read(path,&files));io=context_fixture();
    assert(!uss_native_binding_candidate(&files,&io,10,0,"USS.Utility",&live,&defaults,&result) && !reads);
    write_text(path,"format 2\nsession 1\nUSS.Utility\t10\t0\t1\nend\n");assert(uss_binding_reservation_files_read(path,&files));
    io=context_fixture();assert(uss_native_binding_candidate(&files,&io,10,0,"USS.Utility",&live,&defaults,&result));
    io=context_fixture();native_label=123;assert(!uss_native_binding_candidate(&files,&io,10,0,"USS.Utility",&live,&defaults,&result));
    write_text(backup,"format 2\nsession 0\nOther.Mod\t10\t0\t0\nend\n");assert(uss_binding_reservation_files_read(path,&files));
    io=context_fixture();assert(!uss_native_binding_candidate(&files,&io,10,0,"USS.Utility",&live,&defaults,&result) && !reads);
    assert(DeleteFileW(path) && DeleteFileW(backup));
    puts("USS binding candidate: actual primary/backup receipt plus native context, changed file, foreign reservation/label and self snapshot PASS; no claim granted");return 0;
}
