/* Session-owned native menu storage and SDK writer assembly. Nodes never move
 * or free while native widgets/SDK callbacks can retain their addresses. */
#ifndef USS_NATIVE_MENU_SESSION_H
#define USS_NATIVE_MENU_SESSION_H
#include "uss_native_menu_lifetime.h"
#include "uss_native_menu_sdk.h"
typedef struct UssMenuSessionNode {
    struct UssMenuSessionNode* next;
    UssMenuSdkContext sdk;
    UssMenuSdkWriter writer;
    UssMenuLifetime menu;
} UssMenuSessionNode;
typedef struct UssMenuSession {
    UssMenuSessionNode* retained;
    UssMenuSessionNode* current;
    void* allocation_user;
    void* (*allocate)(void*,size_t); /* NEW zeroed memory; no recycling. */
    uint64_t count;
    unsigned stopped;
} UssMenuSession;
static inline void* uss_menu_session_heap(void* user,size_t size)
{(void)user;return HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,size);}
/* Pure binding except caller-directed read-only pin/host verification. The
 * observer/entry arguments permit own-memory fixtures; SDK startup uses the
 * readers made from this node's stable sdk context. No native setter here. */
static inline int uss_menu_session_prepare_node(UssMenuSessionNode* node,
    const UssMenuObservationIo* observer,const UssMenuEntryReader* entries,
    UssSettingsState* settings,const unsigned char* inert,size_t size,const wchar_t* path)
{
    UssMenuHostSnapshot initial={0};UssNativeMenuEntries resolved;UssMenuRowsIo io;
    if(!node || node->menu.attempted || !observer || !entries ||
        !uss_menu_observe_host(observer,NULL,&initial,NULL) || !initial.escape_present ||
        !initial.topmost || initial.language_changing || initial.foreign_owner || !initial.vanilla_intact ||
        !uss_native_menu_resolve(entries,observer->base,observer->module_size,&resolved))return 0;
    node->writer.sdk=&node->sdk;node->writer.host=initial;node->writer.owner=&node->menu.owner;
    node->writer.owned=uss_menu_ownership_guard;io=uss_menu_sdk_rows_io(&node->writer,&resolved);
    return uss_menu_lifetime_prepare(&node->menu,observer,entries,&io,settings,inert,size,path);
}
static inline UssMenuSessionNode* uss_menu_session_allocate(UssMenuSession* session,const UssMenuSdkContext* context)
{
    UssMenuSessionNode* node;
    if(!session || session->stopped || session->current || !context || !session->allocate || session->count==UINT64_MAX)return NULL;
    node=(UssMenuSessionNode*)session->allocate(session->allocation_user,sizeof(*node));
    if(!node){session->stopped=1;return NULL;}
    /* The allocator's explicit contract is zeroed storage. Keep even refused
     * bootstrap nodes retained; never turn uncertainty into reuse. */
    node->sdk=*context;node->next=session->retained;session->retained=node;
    session->current=node;++session->count;return node;
}
static inline int uss_menu_session_retire(UssMenuSession* session)
{
    if(!session || !session->current || !uss_menu_lifetime_retire(&session->current->menu))return 0;
    session->current=NULL;return 1;
}
/* Actual SDK read-only bootstrap. Invoke only on the owning game callback with
 * validated build/thread context. Failure stops this session's menu, not weapons. */
static inline int uss_menu_session_bootstrap(UssMenuSession* session,const UssMenuSdkContext* context,
    UssSettingsState* settings,const unsigned char* inert,size_t size,const wchar_t* path,UssDedicatedWindows* input)
{
    UssMenuSessionNode* node;UssMenuObservationIo observer;UssMenuEntryReader entries;UssMenuHostSnapshot initial={0};
    if(!context || !uss_menu_sdk_scope((void*)context))return 0;
    /* No Options screen is normal idle state, not a failed allocation/lifetime. */
    observer=uss_menu_sdk_observer((UssMenuSdkContext*)context);
    if(!uss_menu_observe_host(&observer,NULL,&initial,NULL) || !initial.escape_present ||
        !initial.topmost || initial.language_changing || initial.foreign_owner || !initial.vanilla_intact)return 0;
    node=uss_menu_session_allocate(session,context);if(!node)return 0;
    observer=uss_menu_sdk_observer(&node->sdk);entries=uss_menu_sdk_entries(&node->sdk);
    if(!uss_menu_session_prepare_node(node,&observer,&entries,settings,inert,size,path) ||
        !uss_menu_frame_bind_input(&node->menu.frame,input)){session->stopped=1;return 0;}
    return 1;
}
#endif
