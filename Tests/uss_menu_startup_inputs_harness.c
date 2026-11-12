#include <assert.h>
#include <stdio.h>
#include "../src/uss_menu_startup_inputs.h"
static unsigned calls,change,invalid;static uint64_t generation=1;
static uint64_t own_context(void* user){(void)user;return generation;}
static int own_read(void* user,uintptr_t at,void* out,size_t size){
    unsigned char bytes[216]={0};(void)user;assert(at==0x10000000+0x32ecc60 && size==216);
    ++calls;uss_menu_put_word(bytes,0,invalid?140:139);if(change && calls==2)bytes[80]=1;
    memcpy(out,bytes,size);return 1;
}
int main(void){
    wchar_t directory[1024];
    wchar_t path[1024];unsigned char bytes[216];UssMenuObservationIo io={0};
    assert(uss_menu_settings_path("C:/Mods/USS",path) && !wcscmp(path,L"C:/Mods/USS\\uss-settings.json"));
    assert(uss_menu_settings_path("C:/Mods/USS/",path) && !wcscmp(path,L"C:/Mods/USS/uss-settings.json"));
    assert(!uss_menu_settings_path("C:relative",path) && !uss_menu_settings_path("relative",path));
    assert(!uss_menu_settings_path("C:/bad\xff",path));
    assert(uss_menu_user_settings_path(L"C:/Session/environment/Local",directory,path));
    assert(!wcscmp(directory,L"C:/Session/environment/Local\\UtilitySupportSlotHellForge"));
    assert(!wcscmp(path,L"C:/Session/environment/Local\\UtilitySupportSlotHellForge\\uss-settings.json"));
    assert(!uss_menu_user_settings_path(L"C:relative",directory,path));
    assert(!uss_menu_user_settings_path(L"relative",directory,path));
    /* Real Windows directory setup uses a test-local environment root, then
     * restores the process environment. No user AppData or game files touched. */
    {wchar_t prior[32768],root[1024],actual[1024];DWORD count=GetEnvironmentVariableW(L"LOCALAPPDATA",prior,32768);
     assert(count<32768);
     assert(GetFullPathNameW(L"build",1024,root,NULL));
     assert(SetEnvironmentVariableW(L"LOCALAPPDATA",root));
     assert(uss_menu_user_settings_startup(actual) && uss_menu_user_settings_startup(actual));
     assert(uss_menu_user_settings_path(root,directory,path) && !wcscmp(actual,path));
     assert(RemoveDirectoryW(directory));
     assert(SetEnvironmentVariableW(L"LOCALAPPDATA",count?prior:NULL));}
    io.base=0x10000000;io.module_size=0x4000000;io.read=own_read;io.context=own_context;
    assert(uss_menu_read_inert(&io,bytes) && calls==2 && uss_menu_word(bytes,0)==139);
    calls=0;change=1;memset(bytes,0xaa,sizeof(bytes));assert(!uss_menu_read_inert(&io,bytes) && bytes[0]==0xaa);
    calls=0;change=0;invalid=1;assert(!uss_menu_read_inert(&io,bytes));invalid=0;generation=0;
    assert(!uss_menu_read_inert(&io,bytes));
    puts("USS startup inputs: absolute UTF-8 settings path, invalid path refusal, double-read inert descriptor and no partial output PASS");return 0;
}
