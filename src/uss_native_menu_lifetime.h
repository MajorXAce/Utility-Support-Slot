/* Fresh native screen bootstrap. Caller supplies a NEW zeroed, session-retained
 * object per screen lifetime. Never recycle text/descriptors after screen close. */
#ifndef USS_NATIVE_MENU_LIFETIME_H
#define USS_NATIVE_MENU_LIFETIME_H
#include "uss_native_menu_frame.h"
typedef struct UssMenuLifetime {
    UssMenuOwnership owner;
    UssMenuChrome chrome;
    UssMenuRows rows;
    UssMenuLifecycle lifecycle;
    UssMenuFrame frame;
    unsigned attempted,prepared,retired;
} UssMenuLifetime;
/* IO construction remains separate: no native setter/write is called here.
 * Resolve code and observe vanilla host twice around descriptor preparation. */
static inline int uss_menu_lifetime_prepare(UssMenuLifetime* lifetime,
    const UssMenuObservationIo* observer,const UssMenuEntryReader* entries,
    const UssMenuRowsIo* io,UssSettingsState* settings,const unsigned char* inert,size_t inert_size,
    const wchar_t* path)
{
    UssMenuHostSnapshot initial={0},again={0};UssNativeMenuEntries resolved;
    if(!lifetime || lifetime->attempted || !observer || !entries || !io || !settings ||
        settings->external || settings->provider || !uss_settings_valid(&settings->current))return 0;
    lifetime->attempted=1;
    if(!uss_native_menu_resolve(entries,observer->base,observer->module_size,&resolved) ||
        memcmp(&io->native,&resolved,sizeof(resolved)) ||
        !uss_menu_observe_host(observer,NULL,&initial,NULL) || !initial.escape_present ||
        !initial.topmost || initial.language_changing || initial.foreign_owner || !initial.vanilla_intact ||
        !uss_menu_chrome_prepare(&lifetime->chrome,&initial,"USS","Utility Support Slot") ||
        !uss_menu_controls_prepare(&lifetime->rows,inert,inert_size,settings))return 0;
    lifetime->owner.observer=*observer;lifetime->owner.entry_reader=*entries;
    lifetime->owner.entries=resolved;lifetime->owner.identity=initial;
    lifetime->owner.chrome=&lifetime->chrome;lifetime->owner.rows=&lifetime->rows;
    lifetime->owner.stage=USS_MENU_PROOF_VANILLA;
    lifetime->lifecycle.owner=&lifetime->owner;lifetime->lifecycle.io=*io;
    lifetime->lifecycle.initialized=1;
    if(!uss_menu_lifecycle_bind_native_cleanup(&lifetime->lifecycle,settings) ||
        !uss_menu_frame_bind(&lifetime->frame,&lifetime->lifecycle,path) ||
        !uss_menu_ownership_observe(&lifetime->owner,&again) || !uss_menu_same_host(&initial,&again))return 0;
    lifetime->prepared=1;return 1;
}
/* Only a positively observed CLOSED Escape screen permits a fresh bootstrap.
 * A failed read/foreign mutation is not permission to reset an uncertain owner. */
static inline int uss_menu_lifetime_retire(UssMenuLifetime* lifetime)
{
    UssMenuHostSnapshot closed={0};
    if(!lifetime || !lifetime->prepared || lifetime->retired ||
        !uss_menu_observe_host(&lifetime->owner.observer,NULL,&closed,NULL) || closed.escape_present)return 0;
    uss_dedicated_capture_cancel(&lifetime->frame.controls.capture,lifetime->lifecycle.settings);
    if(lifetime->lifecycle.settings && !lifetime->lifecycle.settings->external && !lifetime->lifecycle.settings->provider)
        lifetime->lifecycle.settings->pending_present=0;
    lifetime->owner.stopped=1;uss_menu_host_stop(&lifetime->lifecycle.host,"SCREEN_LIFETIME_RETIRED");
    lifetime->retired=1;return 1; /* Retain this whole object unchanged until session ends. */
}
#endif
