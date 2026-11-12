/* Native menu effect assembly. No SDK registration or runtime scheduling.
 * Keep this object, owner, chrome, rows, IO and all text at stable addresses. */
#ifndef USS_NATIVE_MENU_LIFECYCLE_H
#define USS_NATIVE_MENU_LIFECYCLE_H
#include "uss_native_menu_ownership.h"
#include "uss_settings.h"
typedef struct UssMenuLifecycle {
    UssMenuHost host;
    UssMenuOwnership* owner;
    UssMenuRowsIo io;
    UssMenuVec2 pivot; /* Native reference: (0.5,1), pinned original initializer. */
    UssSettingsState* settings; /* Session-owned; external providers never discarded here. */
    float dt;
    void* cleanup_user;
    unsigned (*cleanup)(void*); /* DONE only after pending edits/description clear. */
    unsigned initialized;
    const char* checkpoint;UssMenuReadBudget last_read;UssMenuHostSnapshot last_observed;
} UssMenuLifecycle;
static inline int uss_menu_lifecycle_cleanup_guard(UssMenuLifecycle* menu)
{
    return menu && menu->settings && !menu->settings->external && !menu->settings->provider &&
        menu->owner && menu->owner->stage==USS_MENU_PROOF_OWNED && menu->owner->chrome &&
        uss_menu_chrome_guard(menu->owner->chrome,&menu->io);
}
/* Mirrors the original leave_view/release_description sequence. Pending UI
 * edits are discarded, not committed. A partially completed native sequence is
 * consumed once and stops; completed effects are never rolled back. */
static unsigned uss_menu_lifecycle_native_cleanup(void* user)
{
    UssMenuLifecycle* menu=(UssMenuLifecycle*)user;UssMenuChrome* chrome;
    uintptr_t content,box;uint32_t no_setting=156;unsigned char unapplied=0;unsigned before;
    if(!menu || !menu->owner || !(chrome=menu->owner->chrome) || !menu->settings ||
        menu->settings->external || menu->settings->provider || menu->owner->stage!=USS_MENU_PROOF_OWNED ||
        !menu->io.native.hide_description || !menu->io.native.clear_args ||
        !uss_menu_lifecycle_cleanup_guard(menu))return USS_MENU_EFFECT_NOT_ENTERED;
    content=(uintptr_t)menu->owner->identity.content;box=content+1315800;before=chrome->effects;
    menu->settings->pending_present=0;menu->settings->release_required=1;
    if(!uss_menu_chrome_write(chrome,&menu->io,content+1319449,&unapplied,1))goto stopped;
    if(!uss_menu_lifecycle_cleanup_guard(menu))goto stopped;
    ++chrome->effects;menu->io.native.hide_description(box,1,0);
    if(!uss_menu_lifecycle_cleanup_guard(menu))goto stopped;
    ++chrome->effects;(void)menu->io.native.clear_args(box+936+272);
    if(!uss_menu_lifecycle_cleanup_guard(menu))goto stopped;
    ++chrome->effects;(void)menu->io.native.clear_args(box+1632+272);
    if(!uss_menu_lifecycle_cleanup_guard(menu) ||
        !uss_menu_chrome_write(chrome,&menu->io,content+1318500,&no_setting,4) ||
        !uss_menu_lifecycle_cleanup_guard(menu) ||
        !uss_menu_chrome_write(chrome,&menu->io,content+1318476,&no_setting,4) ||
        !uss_menu_lifecycle_cleanup_guard(menu))goto stopped;
    return USS_MENU_EFFECT_DONE;
stopped:
    menu->owner->stopped=1;
    return chrome->effects!=before?USS_MENU_EFFECT_UNCERTAIN:USS_MENU_EFFECT_NOT_ENTERED;
}
static inline int uss_menu_lifecycle_bind_native_cleanup(UssMenuLifecycle* menu,UssSettingsState* settings)
{
    if(!menu || !settings || !uss_settings_valid(&settings->current) || settings->external || settings->provider ||
        menu->host.phase!=USS_MENU_CLOSED)return 0;
    menu->settings=settings;menu->cleanup_user=menu;menu->cleanup=uss_menu_lifecycle_native_cleanup;
    menu->pivot.x=0.5f;menu->pivot.y=1.0f;return 1; /* Retained native row pivot/anchor, no effect. */
}
static UssMenuHostSnapshot uss_menu_lifecycle_observe(void* user)
{
    UssMenuLifecycle* menu=(UssMenuLifecycle*)user;UssMenuHostSnapshot result={0};
    if(menu && menu->owner)uss_menu_ownership_observe(menu->owner,&result);return result;
}
static unsigned uss_menu_lifecycle_effect(void* user,unsigned action,const UssMenuHostSnapshot* expected,uint32_t value)
{
    UssMenuLifecycle* menu=(UssMenuLifecycle*)user;UssMenuOwnership* owner;UssMenuChrome* chrome;
    UssMenuHostSnapshot current;unsigned before,ok=0,result;
    if(!menu || !(owner=menu->owner) || !(chrome=owner->chrome) ||
        !uss_menu_ownership_guard(owner,expected))return USS_MENU_EFFECT_NOT_ENTERED;
    before=chrome->effects+owner->rows->effects;
    switch(action){
    case USS_MENU_SHOW_OPTIONS:
        if(value!=2 || !menu->io.native.set_content_hidden)return USS_MENU_EFFECT_NOT_ENTERED;
        if(uss_menu_chrome_write(chrome,&menu->io,(uintptr_t)expected->screen+8,&value,4) &&
            uss_menu_chrome_guard(chrome,&menu->io)){
            ++chrome->effects;menu->io.native.set_content_hidden(expected->content,0,1);
            ok=uss_menu_chrome_guard(chrome,&menu->io);
        }break;
    case USS_MENU_LAYOUT_VANILLA:
        if(!menu->io.native.options_visual)return USS_MENU_EFFECT_NOT_ENTERED;
        if(uss_menu_chrome_guard(chrome,&menu->io)){
            ++chrome->effects;menu->io.native.options_visual(expected->content,0);
            ok=uss_menu_chrome_guard(chrome,&menu->io);
        }break;
    case USS_MENU_LABEL_USS:
        ok=owner->stage==USS_MENU_PROOF_CATEGORY && uss_menu_chrome_categories(chrome,&menu->io,0) &&
            uss_menu_chrome_select_category(chrome,&menu->io,0);break;
    case USS_MENU_BUILD_USS:
        ok=uss_menu_ownership_stage(owner,USS_MENU_PROOF_ROWS) && uss_menu_ownership_observe(owner,&current) &&
            uss_menu_rows_build(owner->rows,&menu->io,&current,menu->pivot) &&
            uss_menu_ownership_stage(owner,USS_MENU_PROOF_OWNED) && uss_menu_ownership_observe(owner,&current);break;
    case USS_MENU_EXPOSE_OPTIONS:
        ok=value==2 && uss_menu_chrome_present(chrome,&menu->io,0,0);break;
    case USS_MENU_PRESENT_USS:
        ok=value==chrome->tab_index && uss_menu_chrome_present(chrome,&menu->io,1,menu->dt);break;
    case USS_MENU_CLEAR_PENDING:
        if(!menu->cleanup)return USS_MENU_EFFECT_NOT_ENTERED;
        /* Callback may itself enter native UI. Preserve its explicit uncertainty. */
        result=menu->cleanup(menu->cleanup_user);
        if(result!=USS_MENU_EFFECT_DONE)owner->stopped=1;
        return result<=USS_MENU_EFFECT_UNCERTAIN?result:USS_MENU_EFFECT_UNCERTAIN;
    case USS_MENU_RESTORE_LABELS:
        ok=uss_menu_ownership_stage(owner,USS_MENU_PROOF_RESTORE) && uss_menu_chrome_categories(chrome,&menu->io,1);break;
    case USS_MENU_RESTORE_CATEGORY:
        ok=uss_menu_chrome_select_category(chrome,&menu->io,value) &&
            uss_menu_ownership_stage(owner,USS_MENU_PROOF_VANILLA) && uss_menu_ownership_observe(owner,&current);
        if(ok && !uss_assignment_display_reset_released(&owner->rows->assignment))ok=0;
        if(ok){
            /* Native category rebuilding has released our previous row tree.
             * Reuse descriptor/text bytes unchanged on reopen; never re-prepare. */
            owner->rows->entered=0;owner->rows->built=0;
        }break;
    default:return USS_MENU_EFFECT_NOT_ENTERED;
    }
    if(ok)return USS_MENU_EFFECT_DONE;
    owner->stopped=1;
    return chrome->effects+owner->rows->effects!=before?USS_MENU_EFFECT_UNCERTAIN:USS_MENU_EFFECT_NOT_ENTERED;
}
static inline UssMenuHostServices uss_menu_lifecycle_services(UssMenuLifecycle* menu)
{
    UssMenuHostServices io={menu,uss_menu_lifecycle_observe,uss_menu_lifecycle_effect};return io;
}
static inline int uss_menu_lifecycle_open(UssMenuLifecycle* menu)
{
    UssMenuOwnership* owner;UssMenuHostServices services;
    if(!menu || !menu->initialized || !(owner=menu->owner) || !owner->chrome || !owner->rows ||
        !owner->rows->prepared || !menu->cleanup || menu->host.phase!=USS_MENU_CLOSED ||
        !isfinite(menu->pivot.x) || !isfinite(menu->pivot.y) || menu->pivot.x<0 || menu->pivot.x>1 ||
        menu->pivot.y<0 || menu->pivot.y>1)return 0;
    if(!owner->chrome->tab_ready){
        if(!uss_menu_ownership_stage(owner,USS_MENU_PROOF_TAB) ||
            !uss_menu_chrome_add_tab(owner->chrome,&menu->io))goto stopped;
    }
    if(!uss_menu_ownership_stage(owner,USS_MENU_PROOF_CATEGORY))goto stopped;
    services=uss_menu_lifecycle_services(menu);
    if(!uss_menu_host_begin(&menu->host,&services))goto stopped;
    if(!uss_menu_chrome_present(owner->chrome,&menu->io,1,0))goto stopped;
    return 1;
stopped:
    owner->stopped=1;uss_menu_host_stop(&menu->host,"LIFECYCLE_OPEN_STOPPED");return 0;
}
static inline int uss_menu_lifecycle_before_update(UssMenuLifecycle* menu)
{
    UssMenuHostServices services;UssMenuExpectedRows expected={0};UssMenuHostSnapshot current;
    if(!menu || !menu->owner || menu->host.phase!=USS_MENU_OWNED)return 0;
    /* Covered/language-changing menus suspend without effect or forgetting.
     * A changed identity still goes through the host's permanent refusal path. */
    expected.tab_text=(uintptr_t)menu->owner->chrome->tab_title;
    expected.category_text=(uintptr_t)menu->owner->chrome->category_title;
    uss_menu_rows_expected(menu->owner->rows,&expected);
    if(uss_menu_observe_host(&menu->owner->observer,&expected,&current,NULL) &&
        uss_menu_same_host(&menu->owner->identity,&current) && current.escape_present &&
        (!current.topmost || current.language_changing))return 0;
    services=uss_menu_lifecycle_services(menu);return uss_menu_host_before_update(&menu->host,&services);
}
static inline int uss_menu_lifecycle_after_update(UssMenuLifecycle* menu,float dt)
{
    UssMenuHostServices services;
    if(!menu || !isfinite(dt) || dt<0 || dt>1)return 0;
    menu->dt=dt;services=uss_menu_lifecycle_services(menu);return uss_menu_host_after_update(&menu->host,&services,menu->owner->chrome->tab_index);
}
static inline int uss_menu_lifecycle_close(UssMenuLifecycle* menu)
{
    UssMenuHostServices services;if(!menu)return 0;
    services=uss_menu_lifecycle_services(menu);return uss_menu_host_close(&menu->host,&services);
}

enum {USS_MENU_STEP_REFUSED=0,USS_MENU_STEP_IDLE,USS_MENU_STEP_SUSPENDED,
    USS_MENU_STEP_TAB_ADDED,USS_MENU_STEP_OPENED,USS_MENU_STEP_UPDATED,USS_MENU_STEP_CLOSED,USS_MENU_STEP_ENDED};
/* Canonical one-callback driver, matching the original Lua step. The older
 * before/after helpers remain fixture interfaces, not an engine hook contract.
 * Native tab selection controls entry/exit; SHOWN_CONTENT stays Options while
 * USS is selected. No hook, registration or assumed post-update callback here. */
static inline unsigned uss_menu_lifecycle_step(UssMenuLifecycle* menu,float dt,unsigned* values,unsigned capacity)
{
    UssMenuOwnership* owner;UssMenuExpectedRows expected={0};UssMenuHostSnapshot current;
    if(!menu || !menu->initialized || !(owner=menu->owner) || !owner->chrome || !owner->rows ||
        owner->stopped || menu->host.phase==USS_MENU_STOPPED || !isfinite(dt) || dt<0 || dt>1 ||
        (menu->host.phase!=USS_MENU_CLOSED && menu->host.phase!=USS_MENU_OWNED))return USS_MENU_STEP_REFUSED;
    expected.tab_text=(uintptr_t)owner->chrome->tab_title;
    expected.require_tab=owner->chrome->tab_ready;
    expected.category_text=(uintptr_t)owner->chrome->category_title;
    menu->checkpoint="INITIAL_OBSERVE";
    if(!uss_menu_observe_host(&owner->observer,&expected,&current,&menu->last_read))goto stopped;
    menu->last_observed=current;
    if(!current.escape_present){
        /* Positive closed-screen evidence: discard own draft without touching
         * stale native widgets. Stable text storage must still remain alive. */
        if(menu->settings && !menu->settings->external && !menu->settings->provider){
            menu->settings->pending_present=0;menu->settings->release_required=1;
        }
        owner->stopped=1;uss_menu_host_stop(&menu->host,"SCREEN_ENDED_NO_TOUCH");return USS_MENU_STEP_ENDED;
    }
    menu->checkpoint="INITIAL_IDENTITY";
    if(!uss_menu_same_host(&owner->identity,&current))goto stopped;
    menu->checkpoint="INITIAL_FOREIGN";if(current.foreign_owner)goto stopped;
    if(!current.topmost || current.language_changing)return USS_MENU_STEP_SUSPENDED;
    if(!owner->chrome->tab_ready){
        menu->checkpoint="TAB_STAGE_ENTER";
        if(!uss_menu_ownership_stage(owner,USS_MENU_PROOF_TAB))goto stopped;
        menu->checkpoint="TAB_CONSTRUCT";
        if(!uss_menu_chrome_add_tab(owner->chrome,&menu->io))goto stopped;
        menu->checkpoint="TAB_STAGE_COMPLETE";
        if(!uss_menu_ownership_stage(owner,USS_MENU_PROOF_VANILLA))goto stopped;
        return USS_MENU_STEP_TAB_ADDED;
    }
    if(menu->host.phase==USS_MENU_CLOSED){
        if(current.selected_tab!=current.uss_tab)return USS_MENU_STEP_IDLE;
        return uss_menu_lifecycle_open(menu)?USS_MENU_STEP_OPENED:USS_MENU_STEP_REFUSED;
    }
    if(current.selected_tab!=current.uss_tab)return uss_menu_lifecycle_close(menu)?USS_MENU_STEP_CLOSED:USS_MENU_STEP_REFUSED;
    if(!values || capacity<owner->rows->count || !uss_menu_rows_poll(owner->rows,&menu->io,values,capacity) ||
        !uss_menu_chrome_present(owner->chrome,&menu->io,1,dt))goto stopped;
    return USS_MENU_STEP_UPDATED;
stopped:
    owner->stopped=1;uss_menu_host_stop(&menu->host,"MENU_STEP_STOPPED");return USS_MENU_STEP_REFUSED;
}
#endif
