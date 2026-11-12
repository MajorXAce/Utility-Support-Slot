/* Startup inputs from the SDK's own mod directory and reviewed native setting
 * descriptor. No dependency discovery, directory creation or game mutation. */
#ifndef USS_MENU_STARTUP_INPUTS_H
#define USS_MENU_STARTUP_INPUTS_H
#include "uss_native_menu_sdk.h"
#include "uss_settings_store.h"
static inline int uss_menu_settings_path(const char* folder,wchar_t out[1024])
{
    wchar_t directory[1024];int count;size_t length;
    static const wchar_t name[]=L"uss-settings.json";
    if(!folder || !out)return 0;
    /* Bound the UTF-8 input scan before conversion. */
    for(length=0;length<4096 && folder[length];++length){}
    if(!length || length==4096)return 0;
    count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,folder,(int)length,directory,1023);
    if(count<3 || count>980)return 0;
    directory[count]=0;
    /* SDK mod folders are absolute Windows paths. Reject drive-relative paths. */
    if(!((directory[1]==L':' && (directory[2]==L'\\' || directory[2]==L'/')) ||
        (directory[0]==L'\\' && directory[1]==L'\\')))return 0;
    if(directory[count-1]!=L'\\' && directory[count-1]!=L'/')directory[count++]=L'\\';
    memcpy(directory+count,name,sizeof(name));memcpy(out,directory,((size_t)count+sizeof(name)/sizeof(name[0]))*sizeof(wchar_t));return 1;
}
/* HellForge redirects LOCALAPPDATA into its low-writable session environment
 * and copies it back on normal exit. Never write settings beside executable mods.
 * Pure path construction is separate from the one startup directory creation. */
static inline int uss_menu_user_settings_path(const wchar_t* local,wchar_t directory[1024],wchar_t out[1024])
{
    static const wchar_t sub[]=L"UtilitySupportSlotHellForge";
    static const wchar_t file[]=L"\\uss-settings.json";
    size_t length;
    if(!local || !directory || !out)return 0;
    for(length=0;length<900 && local[length];++length){}
    if(length<3 || length==900 || !((local[1]==L':' && (local[2]==L'\\' || local[2]==L'/')) ||
        (local[0]==L'\\' && local[1]==L'\\')))return 0;
    memcpy(directory,local,length*sizeof(wchar_t));
    if(local[length-1]!=L'\\' && local[length-1]!=L'/')directory[length++]=L'\\';
    memcpy(directory+length,sub,sizeof(sub));length+=sizeof(sub)/sizeof(sub[0])-1;
    memcpy(out,directory,length*sizeof(wchar_t));memcpy(out+length,file,sizeof(file));return 1;
}
static inline int uss_menu_user_settings_startup(wchar_t out[1024])
{
    wchar_t local[1024],directory[1024];DWORD count,attributes;
    count=GetEnvironmentVariableW(L"LOCALAPPDATA",local,1024);
    if(!count || count>=1024 || !uss_menu_user_settings_path(local,directory,out))return 0;
    if(CreateDirectoryW(directory,NULL))return 1;
    if(GetLastError()!=ERROR_ALREADY_EXISTS)return 0;
    attributes=GetFileAttributesW(directory);
    return attributes!=INVALID_FILE_ATTRIBUTES && (attributes&FILE_ATTRIBUTE_DIRECTORY) && !(attributes&FILE_ATTRIBUTE_REPARSE_POINT);
}
/* Pin from original ModOptionsMenu native.lua. It nominates inert setting139;
 * constructed selector descriptors copy no pointers from the source record. */
static inline int uss_menu_read_inert(const UssMenuObservationIo* io,unsigned char out[USS_OPTION_DESCRIPTOR_BYTES])
{
    unsigned char first[USS_OPTION_DESCRIPTOR_BYTES],second[USS_OPTION_DESCRIPTOR_BYTES];uint64_t epoch;
    const uintptr_t rva=0x32ecc60;
    if(!io || !out || !io->read || !io->context || io->module_size<rva+sizeof(first) ||
        io->base>UINTPTR_MAX-rva-sizeof(first) || !(epoch=io->context(io->user)))return 0;
    if(!io->read(io->user,io->base+rva,first,sizeof(first)) || !uss_menu_inert_template(first,sizeof(first)) ||
        !io->read(io->user,io->base+rva,second,sizeof(second)) || memcmp(first,second,sizeof(first)) ||
        io->context(io->user)!=epoch)return 0;
    memcpy(out,first,sizeof(first));return 1;
}
#endif
