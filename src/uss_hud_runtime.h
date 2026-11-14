/* Event-bound Core/recovery -> HUD bridge. HUD failures are cosmetic only.
 * No idle HUD reads, second game callback or gameplay ownership.
 * Explicit presentation invoker is cosmetic and independently fail-closed. */
#ifndef USS_HUD_RUNTIME_H
#define USS_HUD_RUNTIME_H
#include "uss_recovery_native.h"
#include "uss_hud_windows.h"
#include "uss_hud_metadata.h"
#include "uss_hud_presentation.h"
typedef struct UssHudHandoff {
    unsigned active,checks;uint64_t started,next,deadline;uintptr_t root;
    unsigned char display[12];
} UssHudHandoff;
typedef struct UssHudPopup {
    UssPairOwner owner;Inventory inventory;uintptr_t root;
    uint64_t serial,started,next;uint32_t displayed,presented;
    unsigned active,checks,wait_pending;
} UssHudPopup;
typedef struct UssHudDrop {
    UssRecoveryWork work;uintptr_t root;unsigned char display[12],outgoing_display[12];
    uint64_t started,deadline,next,next_hint,drop_since;uint32_t last_support;
    unsigned active,disabled,baseline,stage,consumed,first_done,second_done,checks,fast_checks;
} UssHudDrop;
typedef struct UssHudRuntime {
    UssFormationNative* native;UssHudWindows windows;UssHudAckState ack;UssHudMetadata metadata;
    UssPairOwner owner;UssRecoveryWork recovery;UssHudEvent event;UssHudHandoff handoff;
    UssHudPresentation presentation;UssHudPopup popup;UssHudDrop drop;
    uint64_t serial,finished_pair;unsigned active,restore,pins_checked,pins_valid,handoff_read,last_result;
    unsigned ack_reports,popup_reports,native_removal_valid;unsigned char native_removal[12];
    const char* reason;UssHudPinDiagnostic pin_diagnostic;
} UssHudRuntime;
#include "uss_hud_drop.h"
static inline int hud_runtime_same_owner(const UssHudRuntime* hud)
{
    const UssPairOwner* now=&hud->native->runtime->life.owner;const UssPairOwner* old=&hud->owner;
    return now->thread==old->thread && !now->disabled && now->epoch==old->epoch &&
        now->context_generation==old->context_generation && now->pair_generation==old->pair_generation &&
        now->guard_generation==old->guard_generation && pair_owner_context_equal(&now->context,&old->context);
}
static int hud_runtime_context(void* user,const UssHudEvent* requested,UssHudSnapshot* output)
{
    UssHudRuntime* hud=user;UssFormationNative* native=hud->native;UssFormationRuntime* runtime=native->runtime;
    UssPairMember allowed[2],display;UssHudSnapshot result={0};UssCoreObservation current;
    UssRecoverySnapshot recovery_snapshot;UssRecoveryReader reader={0};Reader memory={0};
    uintptr_t root=0;uint32_t selected;unsigned outgoing_absent=0;
    if(!hud->active || hud->ack.disabled || !hud_runtime_same_owner(hud) ||
        (requested && memcmp(requested,&hud->event,sizeof(*requested))) ||
        native->platform.thread_id(native->platform.user)!=hud->owner.thread)return 0;
    if(hud->restore){
        reader.api=native->api;reader.mod=native->mod;reader.policy=&runtime->policy;
        reader.platform.user=native->platform.user;reader.platform.thread_id=native->platform.thread_id;
        if(runtime->life.owner.pair_present || !uss_recovery_observe(&reader,&hud->recovery,&recovery_snapshot) ||
            recovery_snapshot.death || recovery_snapshot.inventory.slots[2]!=hud->recovery.survivor.entity ||
            recovery_snapshot.survivor.parent!=hud->owner.context.local.avatar ||
            (!recovery_snapshot.outgoing_absent && recovery_snapshot.outgoing.parent!=0))return 0;
        selected=recovery_snapshot.inventory.active_selector;outgoing_absent=recovery_snapshot.outgoing_absent;
        if(selected!=1 && selected!=2)return 0;
    }else{
        UssCoreFlags flags=uss_lifecycle_flags(&runtime->life,&runtime->policy);
        current=formation_native_observe(native,&runtime->life.owner,&flags);
        if(!lifecycle_current(&runtime->life,runtime->thread,&current) || !current.pair_matched ||
            current.utility.parent!=hud->owner.context.local.avatar || current.ordinary.parent!=hud->owner.context.local.avatar ||
            current.inventory.slots[2]!=hud->event.target)return 0;
        selected=current.inventory.active_selector;
        if((hud->event.kind==USS_HUD_DRAW && selected!=3) ||
            ((hud->event.kind==USS_HUD_BACKGROUND || hud->event.kind==USS_HUD_NEW_UTILITY) && selected!=1 && selected!=2))return 0;
    }
    memory.api=native->api;
    if(!get_ptr(&memory,hud->windows.base+0x346d538,&root) || !get(&memory,root+0x394cd8,result.record,40))return 0;
    result.root=root;result.epoch=hud->owner.epoch;result.owner=hud->owner.context.local.owner;
    result.inventory=hud->owner.context.local.inventory;result.row=hud->owner.context.local.row_address;
    result.avatar=hud->owner.context.local.avatar;
    allowed[0]=hud->owner.utility;allowed[1]=hud->owner.ordinary;
    display=hud->event.target==allowed[0].entity?allowed[0]:allowed[1];
    if(hud->restore && !hud->handoff_read && uss_hud_word(result.record,20)==UINT32_MAX){
        display=hud->recovery.outgoing;
        if(outgoing_absent){
            unsigned char zero[8]={0};
            uint32_t previous=uss_hud_word(result.record,32);
            if(uss_hud_word(result.record,28)!=display.entity || !uss_hud_word(result.record,4) || !memcmp(result.record+8,zero,8))return 0;
            if(!hud->native_removal_valid){
                if(previous!=display.entity)return 0;
                memcpy(hud->native_removal,result.record+4,12);hud->native_removal_valid=1;
            }else if((previous!=display.entity && previous!=hud->event.target) || memcmp(hud->native_removal,result.record+4,12))return 0;
            memcpy(result.display,hud->native_removal,12); /* Exact event-owned cache survives the acknowledged previous-ID write. */
            goto verified;
        }
    }
    if(!uss_hud_metadata(&hud->metadata,native->api,hud->windows.base,root,allowed,&display,result.display))return 0;
verified:
    result.context_verified=result.metadata_verified=result.record_verified=1;*output=result;return 1;
}
static inline int hud_runtime_prepare(UssHudRuntime* hud)
{
    Reader memory={0};uintptr_t base;size_t size=0;
    if(!hud || !hud->native || hud->ack.disabled || !(base=formation_native_module(hud->native)))return 0;
    if(hud->native->api->module_base("game.dll",&size)!=base)return 0;
    if(hud->windows.base && (hud->windows.base!=base || hud->windows.module_size!=size))return 0;
    hud->windows.base=base;hud->windows.module_size=size;hud->windows.thread=GetCurrentThreadId();
    if(!hud->pins_checked){memory.api=hud->native->api;hud->pins_checked=1;
        hud->pins_valid=(unsigned)uss_hud_validate_pins_diagnose(&memory,base,&hud->pin_diagnostic);
        if(!hud->pins_valid && hud->native->api->log){
            const UssHudPinDiagnostic* d=&hud->pin_diagnostic;
            hud->native->api->log(hud->native->mod,
                "USS HUD pin stage=%s index=%u rva=0x%x size=%u byte=%u expected=0x%x actual=0x%x read_reason=%s",
                d->stage?d->stage:"NONE",d->index,d->rva,d->size,d->offset,d->expected,d->actual,memory.failure?memory.failure:"NONE");
        }
    }
    if(!hud->pins_valid){hud->reason="HUD_PINS_UNAVAILABLE";return 0;}
    if(hud->serial==UINT64_MAX){hud->ack.disabled=1;hud->reason="HUD_EVENT_SERIAL_EXHAUSTED";return 0;}
    return 1;
}
static inline void uss_hud_runtime_cancel(UssHudRuntime* hud)
{hud->active=0;hud->handoff.active=0;hud->handoff_read=0;hud->popup.active=0;hud->popup.serial=0;hud->drop.active=0;}
static inline void uss_hud_runtime_retire(UssHudRuntime* hud)
{
    uss_hud_runtime_cancel(hud);memset(&hud->metadata,0,sizeof(hud->metadata));hud->finished_pair=0;
    hud->native_removal_valid=0;hud->ack.epoch=0; /* Preserve serial, write counters and disable after uncertainty. */
}
static inline unsigned hud_runtime_event(UssHudRuntime* hud,unsigned kind,uint32_t from,uint32_t target,uint32_t outgoing)
{
    UssHudAckServices services;unsigned result;
    uss_hud_runtime_cancel(hud);hud->native_removal_valid=0;if(!hud_runtime_prepare(hud))return USS_HUD_SKIPPED;
    hud->event.serial=++hud->serial;hud->event.epoch=hud->owner.epoch;hud->event.kind=kind;
    hud->event.from=from;hud->event.target=target;hud->event.outgoing=outgoing;hud->active=1;
    services=uss_hud_windows_services(&hud->windows);result=uss_hud_acknowledge(&hud->ack,&services,&hud->event);
    hud->active=0;hud->last_result=result;hud->reason=hud->ack.reason;return result;
}
static inline void hud_runtime_ack_apply(void* user,uint32_t from,uint32_t target,UssPairLabel label)
{
    UssHudRuntime* hud=user;UssFormationRuntime* runtime=hud->native->runtime;unsigned kind;
    UssCoreObservation current;UssCoreFlags flags=uss_lifecycle_flags(&runtime->life,&runtime->policy);
    if(label!=USS_PAIR_STORE_NEW_UTILITY && label!=USS_PAIR_DRAW_UTILITY && label!=USS_PAIR_DRAW_ORDINARY)return;
    hud->reason="HUD_ACK_PAIR_ADMISSION";
    if(!from || from==target || !pair_owner_ready(&runtime->life.owner,runtime->thread))return;
    current=formation_native_observe(hud->native,&runtime->life.owner,&flags);
    hud->reason="HUD_ACK_CURRENT_CONTEXT";
    if(!lifecycle_current(&runtime->life,runtime->thread,&current) || !current.pair_matched)return;
    hud->owner=runtime->life.owner;hud->restore=0;
    hud->reason="HUD_ACK_MEMBER_DIRECTION";
    if(!((from==hud->owner.utility.entity && target==hud->owner.ordinary.entity) ||
        (from==hud->owner.ordinary.entity && target==hud->owner.utility.entity)))return;
    kind=label==USS_PAIR_STORE_NEW_UTILITY?USS_HUD_NEW_UTILITY:
        (current.inventory.active_selector==3?USS_HUD_DRAW:USS_HUD_BACKGROUND);
    (void)hud_runtime_event(hud,kind,from,target,from);
}
/* Bounded event-level diagnostics; no extra HUD reads or effect attempts. */
static inline void uss_hud_runtime_ack(void* user,uint32_t from,uint32_t target,UssPairLabel label)
{
    UssHudRuntime* hud=user;
    if(label!=USS_PAIR_STORE_NEW_UTILITY && label!=USS_PAIR_DRAW_UTILITY && label!=USS_PAIR_DRAW_ORDINARY)return;
    hud->last_result=USS_HUD_SKIPPED;hud_runtime_ack_apply(user,from,target,label);
    if(hud->ack_reports<64 && hud->native->api->log){
        ++hud->ack_reports;
        hud->native->api->log(hud->native->mod,
            "USS swap HUD label=%u from=%u target=%u result=%u reason=%s disabled=%u writes=%llu",
            label,from,target,hud->last_result,hud->reason?hud->reason:"NONE",hud->ack.disabled,(unsigned long long)hud->ack.writes);
    }
}
static inline void uss_hud_runtime_restored(UssHudRuntime* hud,const UssRecoveryWork* work,uint64_t now)
{
    UssHudSnapshot after;UssHudAckServices services;
    if(!work || (work->phase!=USS_RECOVERY_PRIMARY && work->phase!=USS_RECOVERY_COMPLETE) || work->owner.thread!=hud->native->runtime->thread ||
        (work->consumed&(1u<<USS_RECOVERY_DROP_STORED)) || now>UINT64_MAX-6500)return;
    hud->last_result=USS_HUD_SKIPPED;
    hud->owner=work->owner;hud->recovery=*work;hud->restore=1;
    if(hud_runtime_event(hud,USS_HUD_RESTORE,0,work->survivor.entity,work->outgoing.entity)!=USS_HUD_ACKED)return;
    hud->active=1;services=uss_hud_windows_services(&hud->windows);after=services.observe(services.user,&hud->event);
    if(after.valid && uss_hud_word(after.record,20)==UINT32_MAX){
        hud->handoff_read=1;
        if(hud_runtime_context(hud,&hud->event,&after)){
            hud->handoff.active=1;hud->handoff.checks=0;hud->handoff.root=(uintptr_t)after.root;
            memcpy(hud->handoff.display,after.display,12);hud->handoff.started=now;hud->handoff.next=now+1;hud->handoff.deadline=now+6500;
            hud_drop_arm(hud,(uintptr_t)after.root,after.display,after.record+4,now);
        }
    }
    hud->active=0;hud->handoff_read=0;
}
typedef struct UssHudPopupSnapshot {
    uint64_t serial;UssCoreObservation current;UssHudSnapshot hud;uint32_t hold;
    unsigned char expected[12],desired[12];
} UssHudPopupSnapshot;
/* Keep the acquisition's exact inventory anchor, except that a previously
 * held weapon may finish yielding to the SAME selected slot. This bounded
 * visual follow-up owns no gameplay and must not freeze an animation snapshot.
 * Actual presentation entry still rechecks the complete current snapshot. */
static inline int hud_popup_inventory_same(const Inventory* before,const Inventory* current)
{
    Inventory settled=*before;
    if(current->held!=before->held){
        unsigned slot=current->active_selector;
        if(slot<1 || slot>3 || current->held!=current->slots[slot-1] ||
            memcmp(current->identities[4],current->identities[slot-1],24) ||
            current->observed_kinds[4]!=current->observed_kinds[slot-1] ||
            current->native_kinds[4]!=current->native_kinds[slot-1])return 0;
        settled.held=current->held;memcpy(settled.identities[4],current->identities[4],24);
        settled.observed_kinds[4]=current->observed_kinds[4];
        settled.native_kinds[4]=current->native_kinds[4];
    }
    return !memcmp(&settled,current,sizeof(settled));
}
static int hud_popup_capture(UssHudRuntime* hud,UssHudPopupSnapshot* output)
{
    UssHudPopupSnapshot result={0};UssHudPopup* popup=&hud->popup;UssFormationRuntime* runtime=hud->native->runtime;
    UssPairOwner* owner=&runtime->life.owner;UssCoreFlags flags;UssPairMember allowed[2],displayed,presented;Reader reader={0};uintptr_t root=0,again=0;
    if(!popup->serial || !pair_owner_ready(owner,runtime->thread) || owner->epoch!=popup->owner.epoch ||
        owner->context_generation!=popup->owner.context_generation || owner->pair_generation!=popup->owner.pair_generation ||
        owner->guard_generation!=popup->owner.guard_generation || !pair_owner_context_equal(&owner->context,&popup->owner.context) ||
        !pair_member_equal(&owner->utility,&popup->owner.utility) || !pair_member_equal(&owner->ordinary,&popup->owner.ordinary))return 0;
    flags=uss_lifecycle_flags(&runtime->life,&runtime->policy);result.current=formation_native_observe(hud->native,owner,&flags);
    if(!lifecycle_current(&runtime->life,runtime->thread,&result.current) || !result.current.pair_matched ||
        result.current.utility.parent!=owner->context.local.avatar || result.current.ordinary.parent!=owner->context.local.avatar ||
        !hud_popup_inventory_same(&popup->inventory,&result.current.inventory))return 0;
    reader.api=hud->native->api;
    if(!get_ptr(&reader,hud->windows.base+0x346d538,&root) || root!=popup->root ||
        !get(&reader,root+0x394cd8,result.hud.record,40) || !get(&reader,root+0x394fdc,result.hud.selection,24) ||
        !get(&reader,root+0x394b44,&result.hold,4))return 0;
    allowed[0]=owner->utility;allowed[1]=owner->ordinary;
    displayed=popup->displayed==owner->utility.entity?owner->utility:owner->ordinary;
    presented=popup->presented==owner->utility.entity?owner->utility:owner->ordinary;
    if(displayed.entity!=popup->displayed || presented.entity!=popup->presented ||
        !uss_hud_metadata(&hud->metadata,hud->native->api,hud->windows.base,root,allowed,&displayed,result.expected) ||
        !uss_hud_metadata(&hud->metadata,hud->native->api,hud->windows.base,root,allowed,&presented,result.desired) ||
        !get_ptr(&reader,hud->windows.base+0x346d538,&again) || root!=again)return 0;
    result.serial=popup->serial;result.hud.root=root;*output=result;return 1;
}
typedef struct UssHudPopupRecheck {UssHudRuntime* runtime;const UssHudPopupSnapshot* expected;} UssHudPopupRecheck;
static int hud_popup_unchanged(void* user)
{
    UssHudPopupRecheck* check=user;UssHudPopupSnapshot current;
    return hud_popup_capture(check->runtime,&current) && !memcmp(&current,check->expected,sizeof(current));
}
/* Event endpoints only, capped for the process; no extra game reads. */
static inline void hud_popup_report(UssHudRuntime* hud,const char* stage)
{
    const HfApi* api=hud->native->api;const UssHudPopup* popup=&hud->popup;
    if(hud->popup_reports>=64 || !api->log)return;
    ++hud->popup_reports;
    api->log(hud->native->mod,
        "USS pickup HUD stage=%s reason=%s displayed=%u presented=%u support=%u selected=%u held_at_arm=%u wait_pending=%u checks=%u effects=%llu",
        stage,hud->reason?hud->reason:"NONE",popup->displayed,popup->presented,popup->inventory.slots[2],
        popup->inventory.active_selector,popup->inventory.held,popup->wait_pending,popup->checks,
        (unsigned long long)hud->presentation.effects);
}
static inline void hud_popup_arm(UssHudRuntime* hud,const UssCoreObservation* current,uint32_t displayed,uint32_t presented,
    unsigned wait_pending,uint64_t now)
{
    UssHudPopup popup={0};Reader reader={0};
    if(!hud->presentation.invoke || hud->presentation.disabled || hud->ack.disabled || !current ||
        !lifecycle_current(&hud->native->runtime->life,hud->native->runtime->thread,current) || !current->pair_matched ||
        !hud_runtime_prepare(hud) || now>UINT64_MAX-1500 || displayed==presented)return;
    reader.api=hud->native->api;
    if(!get_ptr(&reader,hud->windows.base+0x346d538,&popup.root))return;
    popup.owner=hud->native->runtime->life.owner;popup.inventory=current->inventory;
    popup.displayed=displayed;popup.presented=presented;popup.wait_pending=wait_pending;
    popup.serial=++hud->serial;popup.started=now;popup.next=now+25;popup.active=1;
    hud->popup=popup;hud->reason="PICKUP_FOLLOWUP_ARMED";hud_popup_report(hud,"ARMED");
}
static inline void hud_popup_poll(UssHudRuntime* hud,uint64_t now)
{
    UssHudPopupSnapshot snapshot;UssHudPopupRecheck recheck;unsigned result;uint32_t pending;
    if(!hud->popup.active)return;
    if(hud->ack.disabled || hud->presentation.disabled || now<hud->popup.started || now-hud->popup.started>=1500 ||
        hud->popup.checks>=60){
        hud->popup.active=0;
        hud->reason=hud->ack.disabled || hud->presentation.disabled?"PICKUP_DISABLED":
            now<hud->popup.started?"PICKUP_CLOCK_BACKWARDS":"PICKUP_EXPIRED";
        hud_popup_report(hud,"END");return;
    }
    if(now<hud->popup.next)return;
    hud->popup.next=now+25;++hud->popup.checks;
    if(!hud_popup_capture(hud,&snapshot)){hud->popup.active=0;hud->reason="PICKUP_CONTEXT_CHANGED";hud_popup_report(hud,"END");return;}
    pending=uss_hud_word(snapshot.hud.record,20);
    if(!pending){if(!hud->popup.wait_pending){hud->popup.active=0;hud->reason="PICKUP_NO_PENDING";hud_popup_report(hud,"END");}return;}
    if(pending!=1){hud->popup.active=0;hud->reason="PICKUP_OTHER_NOTIFICATION";hud_popup_report(hud,"END");return;}
    if(uss_hud_word(snapshot.hud.selection,0)!=10 || snapshot.hud.selection[20] ||
        uss_hud_word(snapshot.hud.record,0)!=10 || uss_hud_word(snapshot.hud.record,16)!=1 || uss_hud_word(snapshot.hud.record,24)!=1 ||
        uss_hud_word(snapshot.hud.record,28)!=hud->popup.displayed || uss_hud_word(snapshot.hud.record,32)!=hud->popup.displayed ||
        uss_hud_word(snapshot.hud.selection,4)!=1 || uss_hud_word(snapshot.hud.selection,16)!=1 ||
        uss_hud_word(snapshot.hud.selection,12)!=hud->popup.displayed || memcmp(snapshot.hud.record+4,snapshot.expected,12))return;
    if(snapshot.hold<0x00800000u || snapshot.hold>0x40a00000u){hud->popup.active=0;hud->reason="PICKUP_HOLD_INACTIVE";hud_popup_report(hud,"END");return;}
    hud->popup.active=0; /* One native sequence per acquisition; no partial retry or fallback write. */
    recheck.runtime=hud;recheck.expected=&snapshot;
    result=uss_hud_present(&hud->presentation,snapshot.serial,(uintptr_t)snapshot.hud.root,snapshot.expected,snapshot.desired,hud_popup_unchanged,&recheck);
    hud->reason=result==USS_HUD_PRESENT_DONE?"PICKUP_PRESENTATION_CORRECTED":hud->presentation.reason;
    hud_popup_report(hud,"RESULT");
}
static inline void hud_handoff_cancel(UssHudRuntime* hud)
{hud->active=0;hud->handoff.active=0;hud->handoff_read=0;}
/* Called by the existing game loop only. Idle returns before any reads. */
static inline void hud_handoff_poll(UssHudRuntime* hud,uint64_t now)
{
    UssHudSnapshot before,current,after;UssHudAckServices services;uint32_t pending;
    if(!hud->handoff.active)return;
    if(hud->ack.disabled || now<hud->handoff.started || now>=hud->handoff.deadline || hud->handoff.checks>=130){
        hud_handoff_cancel(hud);return;}
    if(now<hud->handoff.next)return;
    hud->handoff.next=now+50;++hud->handoff.checks;hud->active=hud->handoff_read=1;
    services=uss_hud_windows_services(&hud->windows);before=services.observe(services.user,&hud->event);
    if(!before.valid || before.root!=hud->handoff.root){hud_handoff_cancel(hud);return;}
    pending=uss_hud_word(before.record,20);
    if(pending!=0 && pending!=UINT32_MAX){hud_handoff_cancel(hud);return;}
    if(before.selection[20] || (pending && before.active_index!=10) || uss_hud_word(before.record,0)!=10 ||
        uss_hud_word(before.record,16)!=1 || uss_hud_word(before.record,24)!=1 ||
        uss_hud_word(before.record,28)!=hud->event.target || uss_hud_word(before.record,32)!=hud->event.target)goto wait;
    if(!memcmp(before.record+4,hud->handoff.display,12)){hud_handoff_cancel(hud);return;}
    current=services.observe(services.user,&hud->event);
    if(!uss_hud_context_same(&before,&current) || memcmp(before.record,current.record,40) ||
        memcmp(before.selection,current.selection,24)){hud_handoff_cancel(hud);return;}
    hud->handoff.active=0;++hud->ack.writes; /* Consume before the single cache write. */
    if(!services.write(services.user,&current,4,hud->handoff.display,12))goto disable;
    after=services.observe(services.user,&hud->event);memcpy(current.record+4,hud->handoff.display,12);
    if(!uss_hud_context_same(&current,&after) || memcmp(current.record,after.record,40) ||
        memcmp(current.selection,after.selection,24))goto disable;
    hud->reason="HUD_HANDOFF_COMPLETE";hud_handoff_cancel(hud);return;
disable:
    hud->ack.disabled=1;hud->reason="HUD_HANDOFF_UNCERTAIN";hud_handoff_cancel(hud);return;
wait:
    hud->active=hud->handoff_read=0;
}
/* All cosmetic follow-ups share the existing serialized update. */
static inline void uss_hud_runtime_poll(UssHudRuntime* hud,uint64_t now)
{hud_popup_poll(hud,now);hud_handoff_poll(hud,now);hud_drop_poll(hud,now);}
static inline void uss_hud_runtime_completed_pair(UssHudRuntime* hud)
{
    UssFormationRuntime* r=hud->native->runtime;uint32_t from,target;UssCoreFlags flags;UssCoreObservation current;
    unsigned replacement,incoming_utility;uint64_t now;
    if(r->life.phase!=USS_LIFE_FORMED || hud->finished_pair==r->life.owner.pair_generation)return;
    hud->finished_pair=r->life.owner.pair_generation;
    replacement=publication_id(r->life.plan.released.entity);
    if(!replacement && r->life.plan.order!=USS_PAIR_ORDINARY_FIRST)return;
    flags=uss_lifecycle_flags(&r->life,&r->policy);current=formation_native_observe(hud->native,&r->life.owner,&flags);
    if(!lifecycle_current(&r->life,r->thread,&current) || !current.pair_matched)return;
    target=current.inventory.slots[2];
    if(target!=r->life.owner.utility.entity && target!=r->life.owner.ordinary.entity)return;
    from=target==r->life.owner.utility.entity?r->life.owner.ordinary.entity:r->life.owner.utility.entity;
    incoming_utility=r->life.plan.order==USS_PAIR_ORDINARY_FIRST;
    /* A background ordinary acquisition can already have a rendered Silo+1.
     * Do not acknowledge its history as a support draw here: Lua's COMPLETE
     * callback only arms label correction, preserving the real pickup notice. */
    if(replacement && (incoming_utility || current.inventory.active_selector==3))
        uss_hud_runtime_ack(hud,from,target,target==r->life.owner.utility.entity?USS_PAIR_DRAW_UTILITY:USS_PAIR_DRAW_ORDINARY);
    now=r->io.clock_ms(r->io.user);
    if(incoming_utility)hud_popup_arm(hud,&current,r->life.owner.ordinary.entity,r->life.owner.utility.entity,0,now);
    else if(replacement && ((target==r->life.owner.utility.entity && current.inventory.active_selector==3) ||
        current.inventory.active_selector==1 || current.inventory.active_selector==2)){
        /* Actual acquisition was ordinary; Silo may have been restored only
         * internally, including while primary/secondary stays selected.
         * Require the explicitly released ordinary member still detached. */
        UssPairMember names[1]={r->life.plan.released};UssCoreCandidateNames request;UssCoreCandidates named;
        if(!uss_core_candidate_names(&r->life.owner,names,1,&request))return;
        {UssCorePlatform platform={hud->native->platform.user,hud->native->platform.thread_id};
            named=uss_core_observe_candidates(hud->native->api,hud->native->mod,&r->life.owner,&flags,&platform,&request,NULL);}
        if(named.valid && named.members[0].parent==0 && named.members[0].classification==USS_ORDINARY_SUPPORT)
            hud_popup_arm(hud,&named.current,r->life.owner.utility.entity,r->life.owner.ordinary.entity,1,now);
    }
}
static inline void uss_hud_runtime_init(UssHudRuntime* hud,UssFormationNative* native)
{
    memset(hud,0,sizeof(*hud));hud->native=native;hud->presentation.native=native;hud->windows.api=native->api;
    hud->windows.user=hud;hud->windows.context=hud_runtime_context;
}
#endif
