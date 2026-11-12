/* Optional discovery: retain an already-loaded module; never LoadLibrary.
 * Binary-derived ABI is admitted only for the exact reviewed0.2.1 file hash.
 * File hashing/discovery runs once per discovered module, not on input polls. */
#ifndef USS_VPM_WINDOWS_H
#define USS_VPM_WINDOWS_H
#include <windows.h>
#include <bcrypt.h>
#include "uss_vpm_menus.h"
typedef struct UssVpmWindows {
    HMODULE module;UssVpmMenus menus;
    uint64_t next_discovery;unsigned attempted;const char* reason;
} UssVpmWindows;
static inline int uss_vpm_file_reviewed(HMODULE module)
{
    static const unsigned char expected[32]={0x42,0x0d,0x88,0x1f,0xd4,0x96,0xac,0xbc,0x6f,0xa6,0x30,0xb9,0xd8,0xaf,0x6b,0x3e,0x8e,0xe1,0xbc,0x2d,0x5f,0x2c,0x9e,0xe7,0xfa,0xb0,0x06,0x68,0x92,0x02,0x4e,0x1b};
    wchar_t path[32768];unsigned char buffer[8192],digest[32];
    BCRYPT_ALG_HANDLE algorithm=NULL;BCRYPT_HASH_HANDLE hash=NULL;
    HANDLE file=INVALID_HANDLE_VALUE;DWORD length,read=0;uint64_t total=0;LARGE_INTEGER size;int ok=0;
    length=GetModuleFileNameW(module,path,32768);
    if(!length || length>=32768)return 0;
    file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE)return 0;
    if(!GetFileSizeEx(file,&size) || size.QuadPart<=0 || size.QuadPart>16*1024*1024)goto done;
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,NULL,0)<0 ||
        BCryptCreateHash(algorithm,&hash,NULL,0,NULL,0,0)<0)goto done;
    for(;;){
        if(!ReadFile(file,buffer,sizeof(buffer),&read,NULL))goto done;
        if(!read)break;
        total+=read;if(total>16*1024*1024)goto done;
        if(BCryptHashData(hash,buffer,read,0)<0)goto done;
    }
    if(BCryptFinishHash(hash,digest,sizeof(digest),0)<0)goto done;
    ok=memcmp(digest,expected,sizeof(digest))==0;
done:
    if(hash)BCryptDestroyHash(hash);if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);
    CloseHandle(file);return ok;
}
/* ABI getters are called only after exact binary admission. Retaining a module
 * reference keeps code mapped; it does not claim its component stays initialized. */
static inline int uss_vpm_windows_discover(UssVpmWindows* adapter,uint64_t now,unsigned mode)
{
    union {FARPROC raw;const UssVpmOptionsApi* (*get)(uint32_t);} options;
    union {FARPROC raw;const UssVpmBindingsApi* (*get)(uint32_t);} bindings;
    const UssVpmOptionsApi* mom;const UssVpmBindingsApi* mbm;
    if(!adapter)return 0;
    if(adapter->menus.provider)return 1;
    if(adapter->attempted==2)return 0;
    if(now<adapter->next_discovery)return 0;
    adapter->next_discovery=now+1000;
    if(!adapter->attempted){
        if(!GetModuleHandleExW(0,L"vanilla_plus_megapack.dll",&adapter->module))return 0;
        adapter->attempted=1;adapter->reason="BINARY_NOT_REVIEWED";
        if(!uss_vpm_file_reviewed(adapter->module)){adapter->attempted=2;return 0;}
    }
    options.raw=GetProcAddress(adapter->module,"vpm_mom_api");
    bindings.raw=GetProcAddress(adapter->module,"vpm_mbm_api");
    if(!options.raw || !bindings.raw){adapter->reason="EXPORTS_UNAVAILABLE";adapter->attempted=2;return 0;}
    mom=options.get(1);mbm=bindings.get(1);
    if(!mom || !mbm){adapter->reason="WAIT_API";return 0;}
    if(!uss_vpm_menus_bind(&adapter->menus,mom,mbm,(uint64_t)(uintptr_t)adapter->module,mode)){
        adapter->reason="API_UNAVAILABLE";adapter->attempted=2;return 0;
    }
    adapter->reason="BOUND";return 1;
}
#endif
