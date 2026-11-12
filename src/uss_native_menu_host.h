/* USS's native Options-host lifecycle. Own services must check the concrete
 * screen/context at each effect. No raw addresses, renderer or game calls here.
 * Design reference: CowboyBingus ModOptionsMenu view.lua (0BSD). */
#ifndef USS_NATIVE_MENU_HOST_H
#define USS_NATIVE_MENU_HOST_H
#include <stdint.h>
#include <string.h>

enum {USS_MENU_CLOSED=0,USS_MENU_OPENING,USS_MENU_OWNED,USS_MENU_CLOSING,USS_MENU_STOPPED};
enum {USS_MENU_EFFECT_NOT_ENTERED=0,USS_MENU_EFFECT_DONE=1,USS_MENU_EFFECT_UNCERTAIN=2};
enum {USS_MENU_SHOW_OPTIONS=1,USS_MENU_LAYOUT_VANILLA,USS_MENU_LABEL_USS,USS_MENU_BUILD_USS,
    USS_MENU_EXPOSE_OPTIONS,USS_MENU_PRESENT_USS,USS_MENU_CLEAR_PENDING,USS_MENU_RESTORE_LABELS,USS_MENU_RESTORE_CATEGORY};
typedef struct UssMenuHostSnapshot {
    uint64_t screen,content,epoch,input,container;
    uint32_t tab_count,uss_tab; /* USS index is discovered by retained title identity. */
    uint32_t category,tab,selected_tab; /* shown content vs actual tab-bar selection are distinct. */
    /* Legacy vanilla_intact means structurally valid shared host, not vanilla
     * labels. foreign_owner now means lost/duplicated USS identity only. */
    unsigned valid,escape_present,topmost,language_changing,vanilla_intact,ours_intact,foreign_owner;
} UssMenuHostSnapshot;
typedef struct UssMenuHost {
    unsigned phase,effects,uncertain,exposed;
    UssMenuHostSnapshot identity;
    uint32_t saved_category,saved_tab;
    const char* reason;
} UssMenuHost;
typedef struct UssMenuHostServices {
    void* user;
    UssMenuHostSnapshot (*observe)(void*);
    unsigned (*effect)(void*,unsigned,const UssMenuHostSnapshot*,uint32_t);
} UssMenuHostServices;
static inline int uss_menu_same_host(const UssMenuHostSnapshot* a,const UssMenuHostSnapshot* b)
{
    return a->valid && b->valid && a->screen && a->content && a->epoch && a->input && a->container &&
        a->screen==b->screen && a->content==b->content && a->epoch==b->epoch && a->input==b->input && a->container==b->container;
}
static inline void uss_menu_host_stop(UssMenuHost* host,const char* reason)
{host->phase=USS_MENU_STOPPED;host->reason=reason;}
static inline int uss_menu_host_effect(UssMenuHost* host,const UssMenuHostServices* io,unsigned action,uint32_t value,unsigned ownership)
{
    UssMenuHostSnapshot current;unsigned result;
    current=io->observe(io->user);
    if(!uss_menu_same_host(&host->identity,&current) || !current.escape_present || !current.topmost ||
        current.language_changing || current.foreign_owner || (ownership && !current.ours_intact)){
        uss_menu_host_stop(host,"HOST_OR_OWNERSHIP_CHANGED");return 0;
    }
    /* Mark the attempted effect before entry. A failure never silently retries. */
    ++host->effects;result=io->effect(io->user,action,&current,value);
    if(result!=USS_MENU_EFFECT_DONE){
        host->uncertain=result!=USS_MENU_EFFECT_NOT_ENTERED;
        uss_menu_host_stop(host,host->uncertain?"NATIVE_MENU_EFFECT_UNCERTAIN":"NATIVE_MENU_EFFECT_NOT_ENTERED");return 0;
    }
    return 1;
}
static inline int uss_menu_host_begin(UssMenuHost* host,const UssMenuHostServices* io)
{
    UssMenuHostSnapshot current,after;
    if(!host || !io || !io->observe || !io->effect || host->phase!=USS_MENU_CLOSED)return 0;
    current=io->observe(io->user);
    if(!uss_menu_same_host(&current,&current) || !current.escape_present || !current.topmost || current.language_changing ||
        current.foreign_owner || !current.vanilla_intact || current.category>=9)return 0;
    memset(host,0,sizeof(*host));host->identity=current;host->saved_category=current.category<8?current.category:0;host->saved_tab=current.tab;
    host->phase=USS_MENU_OPENING;host->reason="OPENING";
    /* Layout vanilla buttons BEFORE hiding/relabeling: otherwise unlaid-out
     * native category buttons can retain zero positions when Options returns. */
    if(!uss_menu_host_effect(host,io,USS_MENU_SHOW_OPTIONS,2,0) ||
        !uss_menu_host_effect(host,io,USS_MENU_LAYOUT_VANILLA,0,0) ||
        !uss_menu_host_effect(host,io,USS_MENU_LABEL_USS,0,0) ||
        !uss_menu_host_effect(host,io,USS_MENU_BUILD_USS,0,0))return 0;
    after=io->observe(io->user);
    if(!uss_menu_same_host(&host->identity,&after) || !after.ours_intact || after.foreign_owner){
        uss_menu_host_stop(host,"BUILT_MENU_NOT_OWNED");return 0;
    }
    host->phase=USS_MENU_OWNED;host->reason="OWNED";return 1;
}
/* Surround the vanilla update: expose Options for native input/update, then
 * present USS. Do not return to the host through a changed screen or row owner. */
static inline int uss_menu_host_before_update(UssMenuHost* host,const UssMenuHostServices* io)
{
    UssMenuHostSnapshot current;
    if(!host || !io || !io->observe || !io->effect || host->phase!=USS_MENU_OWNED || host->exposed)return 0;
    current=io->observe(io->user);
    if(!uss_menu_same_host(&host->identity,&current) || !current.escape_present){uss_menu_host_stop(host,"SCREEN_ENDED");return 0;}
    if(!current.topmost || current.language_changing){host->reason="COVERED_OR_LANGUAGE";return 0;}
    if(!uss_menu_host_effect(host,io,USS_MENU_EXPOSE_OPTIONS,2,1))return 0;
    host->exposed=1;return 1;
}
static inline int uss_menu_host_after_update(UssMenuHost* host,const UssMenuHostServices* io,uint32_t uss_tab)
{
    if(!host || !io || !io->observe || !io->effect || host->phase!=USS_MENU_OWNED || !host->exposed)return 0;
    if(!uss_menu_host_effect(host,io,USS_MENU_PRESENT_USS,uss_tab,1))return 0;
    host->exposed=0;host->reason="OWNED";return 1;
}
static inline int uss_menu_host_close(UssMenuHost* host,const UssMenuHostServices* io)
{
    UssMenuHostSnapshot current,after;
    if(!host || !io || !io->observe || !io->effect || host->phase!=USS_MENU_OWNED || host->exposed)return 0;
    current=io->observe(io->user);
    if(!uss_menu_same_host(&host->identity,&current) || !current.escape_present){
        /* Forget only: the old native host is gone, so do not restore stale pointers. */
        uss_menu_host_stop(host,"SCREEN_ENDED_NO_RESTORE");return 0;
    }
    host->phase=USS_MENU_CLOSING;
    if(!uss_menu_host_effect(host,io,USS_MENU_CLEAR_PENDING,0,1) ||
        !uss_menu_host_effect(host,io,USS_MENU_RESTORE_LABELS,0,1) ||
        !uss_menu_host_effect(host,io,USS_MENU_RESTORE_CATEGORY,host->saved_category,0))return 0;
    after=io->observe(io->user);
    if(!uss_menu_same_host(&host->identity,&after) || !after.vanilla_intact || after.foreign_owner){
        uss_menu_host_stop(host,"VANILLA_RESTORE_NOT_CONFIRMED");return 0;
    }
    host->phase=USS_MENU_CLOSED;host->reason="CLOSED_RESTORED";return 1;
}
#endif
