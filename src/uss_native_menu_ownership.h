/* Current native menu ownership producer. Read-only; explicit finite stages
 * nominate exactly which retained widgets may be under construction. */
#ifndef USS_NATIVE_MENU_OWNERSHIP_H
#define USS_NATIVE_MENU_OWNERSHIP_H
#include "uss_native_menu_chrome.h"
enum {USS_MENU_PROOF_VANILLA=1,USS_MENU_PROOF_TAB,USS_MENU_PROOF_CATEGORY,
    USS_MENU_PROOF_ROWS,USS_MENU_PROOF_OWNED,USS_MENU_PROOF_RESTORE};
typedef struct UssMenuOwnership {
    UssMenuObservationIo observer;
    UssMenuEntryReader entry_reader;
    UssNativeMenuEntries entries; /* Exact table resolved for this owning module. */
    UssMenuChrome* chrome;
    UssMenuRows* rows;
    UssMenuHostSnapshot identity;
    unsigned stage,stopped;
    const char* failure;unsigned failed_entry;UssMenuReadBudget last_read;UssMenuHostSnapshot last_observed;
} UssMenuOwnership;
static inline int uss_menu_ownership_observe(UssMenuOwnership* owner,UssMenuHostSnapshot* out)
{
    UssMenuExpectedRows expected={0};UssMenuHostSnapshot current={0};UssNativeMenuEntries resolved;
    if(!owner || owner->stopped || !owner->chrome || !owner->rows || !out ||
        owner->stage<USS_MENU_PROOF_VANILLA || owner->stage>USS_MENU_PROOF_RESTORE)return 0;
    expected.tab_text=(uintptr_t)owner->chrome->tab_title;
    expected.require_tab=owner->chrome->tab_ready;
    expected.category_text=(uintptr_t)owner->chrome->category_title;
    expected.building_tab=owner->stage==USS_MENU_PROOF_TAB;
    expected.building_category=owner->stage==USS_MENU_PROOF_CATEGORY || owner->stage==USS_MENU_PROOF_RESTORE;
    if(owner->stage==USS_MENU_PROOF_OWNED)uss_menu_rows_expected(owner->rows,&expected);
    owner->failure="ENTRY_PINS";
    if(!uss_native_menu_resolve_trace(&owner->entry_reader,owner->observer.base,owner->observer.module_size,&resolved,&owner->failed_entry))goto stopped;
    owner->failure="ENTRY_TABLE";
    if(memcmp(&resolved,&owner->entries,sizeof(resolved)))goto stopped;
    owner->failure="HOST_READ";
    if(!uss_menu_observe_host(&owner->observer,&expected,&current,&owner->last_read))goto stopped;
    owner->last_observed=current;if(current.uss_tab!=UINT32_MAX)owner->chrome->tab_index=current.uss_tab;owner->failure="HOST_IDENTITY";
    if(!uss_menu_same_host(&owner->identity,&current))goto stopped;
    owner->failure="ESCAPE_ABSENT";if(!current.escape_present)goto stopped;
    owner->failure="NOT_TOPMOST";if(!current.topmost)goto stopped;
    owner->failure="LANGUAGE_CHANGING";if(current.language_changing)goto stopped;
    owner->failure="FOREIGN_OWNER";if(current.foreign_owner)goto stopped;
    owner->failure="VANILLA_CHANGED";
    if(owner->stage==USS_MENU_PROOF_VANILLA && !current.vanilla_intact)goto stopped;
    owner->failure="OWN_ROWS_CHANGED";
    if(owner->stage==USS_MENU_PROOF_OWNED && (!owner->rows->built || !current.ours_intact))goto stopped;
    owner->failure=NULL;
    *out=current;return 1;
stopped:
    owner->stopped=1;return 0;
}
/* Adapter for UssMenuSdkWriter.owned. A stopped proof cannot authorize another
 * native effect; a verified screen retirement must create a new owner lifetime. */
static inline int uss_menu_ownership_guard(void* user,const UssMenuHostSnapshot* expected)
{
    UssMenuOwnership* owner=(UssMenuOwnership*)user;UssMenuHostSnapshot current;
    return owner && expected && uss_menu_same_host(&owner->identity,expected) &&
        uss_menu_ownership_observe(owner,&current);
}
static inline int uss_menu_ownership_stage(UssMenuOwnership* owner,unsigned stage)
{
    UssMenuHostSnapshot current;unsigned from;
    if(!owner || owner->stopped)return 0;from=owner->stage;
    /* The operation owner advances only at reviewed call-sequence boundaries.
     * No OWNED -> construction escape when a live page has been overwritten. */
    if(!((from==USS_MENU_PROOF_VANILLA && stage==USS_MENU_PROOF_TAB) ||
        (from==USS_MENU_PROOF_TAB && stage==USS_MENU_PROOF_CATEGORY && owner->chrome && owner->chrome->tab_ready) ||
        (from==USS_MENU_PROOF_TAB && stage==USS_MENU_PROOF_VANILLA && owner->chrome && owner->chrome->tab_ready) ||
        (from==USS_MENU_PROOF_CATEGORY && stage==USS_MENU_PROOF_ROWS && owner->chrome && owner->chrome->categories_owned) ||
        (from==USS_MENU_PROOF_ROWS && stage==USS_MENU_PROOF_OWNED && owner->rows && owner->rows->built) ||
        (from==USS_MENU_PROOF_OWNED && stage==USS_MENU_PROOF_RESTORE) ||
        (from==USS_MENU_PROOF_RESTORE && stage==USS_MENU_PROOF_VANILLA && owner->chrome && !owner->chrome->categories_owned) ||
        (from==USS_MENU_PROOF_VANILLA && stage==USS_MENU_PROOF_CATEGORY && owner->chrome && owner->chrome->tab_ready)))return 0;
    if(!uss_menu_ownership_observe(owner,&current))return 0;
    owner->stage=stage;return 1;
}
#endif
