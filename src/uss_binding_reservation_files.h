/* Original assignment-book file snapshots, Windows filesystem only.
 * Explicit absolute source path; no guessed loader directory or file writes.
 * Both primary and .bak reservations are respected, never restored/expired here. */
#ifndef USS_BINDING_RESERVATION_FILES_H
#define USS_BINDING_RESERVATION_FILES_H
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wchar.h>
#include "uss_binding_reservations.h"
enum {USS_BIND_FILE_ERROR=0,USS_BIND_FILE_MISSING,USS_BIND_FILE_VALID,USS_BIND_FILE_INVALID};
typedef struct UssBindingReservationFile {
    unsigned status;DWORD length;BY_HANDLE_FILE_INFORMATION identity;
    wchar_t canonical[1024];char text[USS_BIND_BOOK_BYTES];UssBindingReservations book;
} UssBindingReservationFile;
typedef struct UssBindingReservationFiles {
    wchar_t source[1024];UssBindingReservationFile primary,backup;unsigned valid;
} UssBindingReservationFiles;
static inline int uss_binding_reservation_path(const wchar_t* path)
{
    size_t length;if(!path)return 0;length=wcslen(path);
    return length>=3 && length<=980 && ((path[0]>=L'A' && path[0]<=L'Z') || (path[0]>=L'a' && path[0]<=L'z')) &&
        path[1]==L':' && (path[2]==L'\\' || path[2]==L'/');
}
static inline int uss_binding_reservation_file_identity(const BY_HANDLE_FILE_INFORMATION* a,const BY_HANDLE_FILE_INFORMATION* b)
{
    /* Last-access time may change because WE read the file; it is not content
     * identity. Compare the stable identity, length and last-write fields. */
    return a->dwVolumeSerialNumber==b->dwVolumeSerialNumber && a->nFileIndexHigh==b->nFileIndexHigh &&
        a->nFileIndexLow==b->nFileIndexLow && a->nFileSizeHigh==b->nFileSizeHigh && a->nFileSizeLow==b->nFileSizeLow &&
        !memcmp(&a->ftLastWriteTime,&b->ftLastWriteTime,sizeof(FILETIME));
}
static inline unsigned uss_binding_reservation_file(const wchar_t* path,UssBindingReservationFile* output)
{
    UssBindingReservationFile file={0};BY_HANDLE_FILE_INFORMATION after={0};DWORD got=0,path_length;
    HANDLE handle;LARGE_INTEGER size;
    if(!output || !uss_binding_reservation_path(path))return USS_BIND_FILE_ERROR;
    /* Deny write/delete sharing for the short read; never keep this handle across
     * game callbacks or hold another mod's settings file hostage. */
    handle=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(handle==INVALID_HANDLE_VALUE){
        if(GetLastError()==ERROR_FILE_NOT_FOUND)file.status=USS_BIND_FILE_MISSING;
        *output=file;return file.status;
    }
    if(GetFileType(handle)!=FILE_TYPE_DISK || !GetFileInformationByHandle(handle,&file.identity) ||
        (file.identity.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) || !GetFileSizeEx(handle,&size) ||
        size.QuadPart<1 || size.QuadPart>USS_BIND_BOOK_BYTES)goto invalid;
    path_length=GetFinalPathNameByHandleW(handle,file.canonical,1024,FILE_NAME_NORMALIZED|VOLUME_NAME_DOS);
    if(!path_length || path_length>=1024)goto failed;
    file.length=(DWORD)size.QuadPart;
    if(!ReadFile(handle,file.text,file.length,&got,NULL) || got!=file.length ||
        !GetFileInformationByHandle(handle,&after) || !uss_binding_reservation_file_identity(&file.identity,&after))goto failed;
    file.status=uss_binding_book_parse(file.text,file.length,&file.book)?USS_BIND_FILE_VALID:USS_BIND_FILE_INVALID;
    if(!CloseHandle(handle))file.status=USS_BIND_FILE_ERROR;
    *output=file;return file.status;
invalid:
    file.status=USS_BIND_FILE_INVALID;
    if(!CloseHandle(handle))file.status=USS_BIND_FILE_ERROR;
    *output=file;return file.status;
failed:
    CloseHandle(handle);file.status=USS_BIND_FILE_ERROR;*output=file;return file.status;
}
static inline int uss_binding_reservation_files_read(const wchar_t* path,UssBindingReservationFiles* output)
{
    UssBindingReservationFiles files={0};wchar_t backup[1024];size_t length;
    if(!output || !uss_binding_reservation_path(path))return 0;
    length=wcslen(path);memcpy(files.source,path,(length+1)*sizeof(wchar_t));
    memcpy(backup,path,length*sizeof(wchar_t));memcpy(backup+length,L".bak",5*sizeof(wchar_t));
    uss_binding_reservation_file(path,&files.primary);uss_binding_reservation_file(backup,&files.backup);
    /* Invalid main is NOT silently replaced by valid backup. Both are evidence;
     * admission refuses unknown data rather than forgetting a possible owner. */
    files.valid=(files.primary.status==USS_BIND_FILE_VALID || files.primary.status==USS_BIND_FILE_MISSING) &&
        (files.backup.status==USS_BIND_FILE_VALID || files.backup.status==USS_BIND_FILE_MISSING);
    *output=files;return files.valid!=0;
}
static inline int uss_binding_reservation_file_equal(const UssBindingReservationFile* a,const UssBindingReservationFile* b)
{
    if(a->status!=b->status)return 0;if(a->status==USS_BIND_FILE_MISSING)return 1;
    return a->status==USS_BIND_FILE_VALID && a->length==b->length &&
        uss_binding_reservation_file_identity(&a->identity,&b->identity) &&
        !wcscmp(a->canonical,b->canonical) && !memcmp(a->text,b->text,a->length);
}
static inline int uss_binding_reservation_files_current(const UssBindingReservationFiles* expected)
{
    UssBindingReservationFiles current;
    return expected && expected->valid && uss_binding_reservation_files_read(expected->source,&current) &&
        !wcscmp(expected->source,current.source) &&
        uss_binding_reservation_file_equal(&expected->primary,&current.primary) &&
        uss_binding_reservation_file_equal(&expected->backup,&current.backup);
}
/* Conservative union: even a stale backup's foreign/cleanup reservation blocks
 * reuse. A stable filesystem snapshot still does not prove allocator RAM state. */
static inline unsigned uss_binding_reservation_files_query(const UssBindingReservationFiles* files,uint32_t code,const char* id)
{
    unsigned result=USS_BIND_RESERVATION_CANDIDATE,i;const UssBindingReservationFile* books[2];
    if(!files || !files->valid || !id || !*id)return USS_BIND_RESERVATION_UNKNOWN;
    books[0]=&files->primary;books[1]=&files->backup;
    for(i=0;i<2;++i){unsigned found;
        if(books[i]->status==USS_BIND_FILE_MISSING)continue;
        if(books[i]->status!=USS_BIND_FILE_VALID)return USS_BIND_RESERVATION_UNKNOWN;
        found=uss_binding_reservation_query(&books[i]->book,code,id);
        if(found==USS_BIND_RESERVATION_SELF)result=found;
        else if(found!=USS_BIND_RESERVATION_CANDIDATE)return found;
    }
    /* Exclude fixed/unknown codes even when both files are positively absent. */
    {int index=uss_binding_dormant_index(code);if(index<0)return USS_BIND_RESERVATION_UNKNOWN;
        if(index<7)return USS_BIND_RESERVATION_FIXED;}
    return result;
}
#endif
