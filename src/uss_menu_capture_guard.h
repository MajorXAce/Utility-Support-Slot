/* Consume native Menu actions during standalone key capture, following the
 * original 0BSD ModBindingsMenu panel.guard_page. No action-slot reservation.
 * Only active Menu group0actions are consumed, each until release (-1seconds). */
#ifndef USS_MENU_CAPTURE_GUARD_H
#define USS_MENU_CAPTURE_GUARD_H
#include "uss_native_menu_ownership.h"
typedef void (*UssMenuConsumeCall)(uintptr_t,uint64_t,float);
typedef int (*UssMenuConsumeInvoke)(void*,uintptr_t,uintptr_t,uint64_t,float);
typedef struct UssMenuCaptureGuard {
    UssMenuOwnership* owner;void* user;UssMenuConsumeInvoke invoke;unsigned stopped,consumed;
} UssMenuCaptureGuard;
static const unsigned char uss_menu_consume_pin[20]={0x40,0x53,0x48,0x83,0xec,0x20,0x4c,0x8b,0xd1,0x4c,0x8b,0xca,0x48,0x8b,0xca,0xe8,0xdc,0x7c,0x28,0xff};
static inline int uss_menu_capture_guard_current(UssMenuCaptureGuard* guard)
{
    unsigned char code[20];UssMenuOwnership* owner;
    if(!guard || guard->stopped || !(owner=guard->owner) || owner->stage!=USS_MENU_PROOF_OWNED ||
        !uss_menu_ownership_guard(owner,&owner->identity) || !owner->entry_reader.read_executable ||
        !owner->entry_reader.read_executable(owner->entry_reader.user,owner->observer.base+0x12fde90,code,sizeof(code)) ||
        memcmp(code,uss_menu_consume_pin,sizeof(code)))return 0;
    return uss_menu_ownership_guard(owner,&owner->identity);
}
static inline int uss_menu_capture_guard_consume(UssMenuCaptureGuard* guard)
{
    unsigned char actions[97*32];size_t at;unsigned i;UssMenuOwnership* owner;
    if(!guard || !guard->invoke || !uss_menu_capture_guard_current(guard))goto stopped;
    owner=guard->owner;
    for(at=0;at<sizeof(actions);at+=512){
        size_t size=sizeof(actions)-at;if(size>512)size=512;
        if(!owner->observer.read(owner->observer.user,owner->identity.input+808+at,actions+at,size))goto stopped;
    }
    if(!uss_menu_capture_guard_current(guard))goto stopped;
    for(i=0;i<97;++i)if(actions[32*i]){
        if(!uss_menu_capture_guard_current(guard))goto stopped;
        ++guard->consumed; /* Consume before entry: never retry uncertain effects. */
        /* Native consume uses {group low32, action high32}; this is DIFFERENT
         * from the binding-map hash key {group<<16 | action}. */
        if(guard->invoke(guard->user,owner->observer.base+0x12fde90,owner->identity.input,(uint64_t)i<<32,-1.0f)!=1 ||
            !uss_menu_capture_guard_current(guard))goto stopped;
    }
    return 1;
stopped:
    if(guard){guard->stopped=1;if(guard->owner)guard->owner->stopped=1;}return 0;
}
/* Saved-image17AA950 is the shared tab-bar input handler: uint32 result,
 * tab bar, input owner and cursor/context. Direct shortcuts53..5A and previous/
 * next all converge here before selection17ABFC0. Never affect another bar. */
typedef uint32_t (*UssMenuTabInputCall)(uintptr_t,uintptr_t,uintptr_t);
static const unsigned char uss_menu_tab_input_pin[24]={
    0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x10,0x48,0x89,
    0x74,0x24,0x18,0x57,0x48,0x83,0xec,0x20,0x83,0xb9,0x68,0xe0};
static inline uint32_t uss_menu_tab_input_dispatch(void** next,UssMenuOwnership* owner,
    unsigned capturing,uintptr_t bar,uintptr_t input,uintptr_t cursor)
{
    UssMenuTabInputCall original=NULL;
    /* HellForge may replace the chain's next pointer. Read the retained slot
     * on every call; do not cache a private copy between invocations. */
    if(!next || !*next)return 0;
    memcpy(&original,next,sizeof(original));
    if(capturing && owner && !owner->stopped && owner->stage==USS_MENU_PROOF_OWNED &&
        owner->identity.screen<=UINTPTR_MAX-1248 && bar==owner->identity.screen+1248 &&
        input==owner->identity.input && uss_menu_ownership_guard(owner,&owner->identity) &&
        owner->last_observed.selected_tab==owner->last_observed.uss_tab)
        return 0; /* No navigation handled; capture owns these inputs. */
    return original(bar,input,cursor);
}
static inline int uss_menu_consume_invoke(void* user,uintptr_t entry,uintptr_t input,uint64_t action,float seconds)
{
    (void)user;if(!entry || !input || (uint32_t)action || (action>>32)>=97 || seconds!=-1.0f)return 0;
    ((UssMenuConsumeCall)entry)(input,action,seconds);return 1;
}
#endif
