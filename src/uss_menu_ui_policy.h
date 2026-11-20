/* Standalone settings UI eligibility is separate from provider input authority.
 * Fresh protocol-2 evidence for the named Lua options API or a negotiated
 * supported C options prefix can
 * defer a new tab. Once observed during a screen lifetime, retain deferral until
 * positive closure; unreadable UI and transport loss are not closure proofs. */
#ifndef USS_MENU_UI_POLICY_H
#define USS_MENU_UI_POLICY_H
#include "uss_lua_menu_mailbox.h"
enum { USS_MENU_SCREEN_UNKNOWN=0, USS_MENU_SCREEN_OPEN, USS_MENU_SCREEN_CLOSED };
typedef struct UssMenuPresence {
    unsigned fresh,present,generation;
} UssMenuPresence;
typedef struct UssMenuUiPolicy {
    unsigned presence,companion_fresh,generation,latched,deferred,close_required,screen_state,c_options_present;
} UssMenuUiPolicy;
/* A malformed post revokes input but cannot erase previously valid presence or
 * renew its age. Only a newer accepted record may update presence evidence. */
static inline UssMenuPresence uss_menu_presence_read(UssLuaMenuMailbox* box,uint64_t now_ms)
{
    UssMenuPresence result={0};UssLuaMenuPost p;uint64_t received,instance;
    if(!box)return result;
    AcquireSRWLockExclusive(&box->lock);
    p=box->slot;received=box->received_ms;instance=box->last_instance;
    ReleaseSRWLockExclusive(&box->lock);
    if(!instance || now_ms<received || now_ms-received>USS_MENU_HANDOVER_TIMEOUT_MS)return result;
    result.fresh=1;result.present=(p.flags&USS_LUA_MENU_OPTIONS_PRESENT)!=0;result.generation=p.generation;
    return result;
}
static inline void uss_menu_ui_update_sources(UssMenuUiPolicy* ui,UssLuaMenuMailbox* box,uint64_t now_ms,
    unsigned screen_state,unsigned has_lifetime,unsigned c_options_present)
{
    UssMenuPresence evidence=uss_menu_presence_read(box,now_ms);
    if(!ui)return;
    ui->c_options_present=c_options_present;ui->presence=evidence.present;ui->companion_fresh=evidence.fresh;ui->generation=evidence.generation;
    if(screen_state!=USS_MENU_SCREEN_UNKNOWN)ui->screen_state=screen_state;
    if(screen_state==USS_MENU_SCREEN_CLOSED){ui->latched=0;ui->close_required=0;}
    else if((evidence.present || c_options_present) && (has_lifetime || ui->screen_state!=USS_MENU_SCREEN_CLOSED)){ui->latched=1;if(has_lifetime)ui->close_required=1;}
    ui->deferred=ui->presence || ui->c_options_present || ui->latched || ui->close_required;
}
/* Existing Lua-only callers use the same lifetime policy. Presence restricts
 * standalone UI only; successful registration/readiness still gates input. */
static inline void uss_menu_ui_update(UssMenuUiPolicy* ui,UssLuaMenuMailbox* box,uint64_t now_ms,
    unsigned screen_state,unsigned has_lifetime)
{ uss_menu_ui_update_sources(ui,box,now_ms,screen_state,has_lifetime,0); }
/* Only a previously refused widget lifetime is recoverable here. Other session
 * stops (hook/storage/bootstrap failures) are never cleared by this policy. */
static inline unsigned uss_menu_ui_recover_after_close(unsigned retired,unsigned widget_refused)
{ return retired && widget_refused; }
#endif
