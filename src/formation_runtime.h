/* Integrated initial formation/managed draw. No production addresses here.
 * The effect seams must validate their exact native entry and never auto-retry. */
#ifndef USS_FORMATION_RUNTIME_H
#define USS_FORMATION_RUNTIME_H
#include "pickup_history.h"
#include "local_pair_state.h"
typedef struct UssFormationRuntime UssFormationRuntime;
typedef struct UssFormationIo {
    void* user;
    uint64_t (*clock_ms)(void*);
    UssCoreObservation (*observe)(void*,const UssPairOwner*,const UssCoreFlags*);
    int (*capture)(void*,const UssFormationRuntime*,UssPublicationStage,UssPublicationCapture*);
    UssPairServiceResult (*select)(void*,const UssCoreObservation*,uint32_t);
    UssPairServiceResult (*attach)(void*,const UssCoreObservation*,const UssPairMember*);
    UssPublicationSend (*send)(void*,const UssPublicationCapture*,uint64_t);
    void (*acknowledge)(void*,uint32_t,uint32_t,UssPairLabel); /* Optional event-bound HUD handling. */
} UssFormationIo;
struct UssFormationRuntime {
    UssCoreLifecycle life;
    UssPickupHistory history;
    UssPublicationState publication;
    UssLocalPairState local;
    UssPairTransaction work;
    UssCoreObservation latest;
    UssPairObservation observed;
    UssPublicationView view;
    UssPublicationOwner row;
    UssCoreFlags policy;
    UssFormationIo io;
    unsigned thread,busy;
    unsigned controller_draw; /* Set only by recognized Default-controller input. */
    unsigned command_bundle_valid,command_decision; /* Last command diagnostics only. */
    const char* reason;
};
/* Lua's local switch_entity does not wait for primary's draw timer. Admit
 * only this observed primary-drawing shape on an already-owned solo pair.
 * Formation, replacement, storage and every network path retain settled checks. */
static int formation_solo_draw_ready(const UssFormationRuntime* r,const UssCoreObservation* current)
{
    const UssPairTransaction* work=&r->work;const unsigned char* row=current->primary.fields.row;
    uint32_t block=u32(row+28);UssLocalPairState proof=r->local;
    return work->phase==USS_PAIR_ACTIVE && work->order==USS_PAIR_DRAW && work->step_count==1 &&
        work->origin.local_only && current->valid && current->pair_matched && current->context.local_only &&
        uss_local_pair_owned(&r->local,&r->life.owner) &&
        uss_local_pair_accept(&proof,&r->life.owner,current) && current->primary.valid &&
        current->inventory.active_selector==1 && current->inventory.held==current->inventory.slots[0] &&
        u32(row)==current->inventory.avatar && u32(row+4)==1 && u32(row+8)==3 &&
        u32(row+16)==2 && u32(row+20)==1 && !u32(row+24) && block>0 && block<0x7f800000 &&
        current->primary.bit47 && !current->primary.fields.global_guard && !current->primary.fields.state_byte;
}
static int formation_controller_draw_ready(const UssFormationRuntime* r,const UssCoreObservation* current)
{
    const UssPairObservation* origin=&r->work.origin;
    return r->controller_draw && current->valid && current->pair_matched && !current->context.local_only &&
        r->life.phase==USS_LIFE_FORMED && lifecycle_current(&r->life,r->thread,current) &&
        current->expected_epoch==origin->epoch && current->expected_pair==origin->pair_generation &&
        current->expected_guard==origin->guard_generation &&
        pair_member_equal(&r->life.owner.utility,&origin->utility) && pair_member_equal(&r->life.owner.ordinary,&origin->ordinary) &&
        current->utility.parent_matches && current->ordinary.parent_matches &&
        current->utility.interaction_admissible && current->ordinary.interaction_admissible &&
        r->view.supported_pair && r->publication.formed && r->publication.record_present &&
        !r->publication.disabled && !r->publication.suspended &&
        publication_key_equal(&r->view.key,&r->publication.record) &&
        uss_managed_draw_primary(&r->work,&current->inventory,&current->primary,r->io.clock_ms(r->io.user));
}
static int formation_draw_ready(const UssFormationRuntime* r,const UssCoreObservation* current)
{return current->primary.ready || formation_solo_draw_ready(r,current) || formation_controller_draw_ready(r,current);}
static int formation_refresh(UssFormationRuntime* runtime)
{
    UssCoreFlags flags=uss_lifecycle_flags(&runtime->life,&runtime->policy);
    runtime->latest=runtime->io.observe(runtime->io.user,&runtime->life.owner,&flags);
    {int ok=uss_lifecycle_observe(&runtime->life,runtime->thread,&runtime->latest,
        &runtime->observed,&runtime->view,&runtime->row);
        if(!uss_local_pair_owned(&runtime->local,&runtime->life.owner))uss_local_pair_invalidate(&runtime->local);
        if(ok && runtime->latest.context.local_only)uss_local_pair_accept(&runtime->local,&runtime->life.owner,&runtime->latest);
        return ok;}
}
static int formation_match(const UssPairObservation* a,const UssPairObservation* b)
{
    return pair_scope(a) && pair_scope(b) && a->epoch==b->epoch && a->pair_generation==b->pair_generation &&
        a->guard_generation==b->guard_generation && pair_context_equal(&a->context,&b->context) &&
        pair_member_equal(&a->utility,&b->utility) && pair_member_equal(&a->ordinary,&b->ordinary) &&
        a->local_only==b->local_only && a->support==b->support && a->active==b->active;
}
static uint64_t formation_clock(void* user)
{UssFormationRuntime* r=(UssFormationRuntime*)user;return r->io.clock_ms(r->io.user);}
static UssPublicationView formation_view(void* user)
{UssFormationRuntime* r=(UssFormationRuntime*)user;formation_refresh(r);return r->view;}
static int formation_capture(void* user,UssPublicationStage stage,UssPublicationCapture* output)
{
    UssFormationRuntime* r=(UssFormationRuntime*)user;
    return !r->life.owner.context.local_only && pair_owner_ready(&r->life.owner,r->thread) && r->io.capture(r->io.user,r,stage,output);
}
static UssPublicationSend formation_send(void* user,UssPublicationKind kind,const UssPublicationCapture* capture,uint64_t metadata)
{UssFormationRuntime* r=(UssFormationRuntime*)user;(void)kind;return r->io.send(r->io.user,capture,metadata);}
static UssPublicationResult formation_publish(UssFormationRuntime* r,unsigned only,UssPublicationStage stage)
{
    UssPublicationServices services={0};UssPublicationOwner caller;
    if(!formation_refresh(r) || r->latest.context.local_only)return publication_result(USS_PUBLICATION_REFUSED,USS_PUB_SCOPE);
    caller=r->row;services.user=r;services.clock_ms=formation_clock;services.view=formation_view;
    services.capture=formation_capture;services.send=formation_send;
    return uss_publication_boundary(&r->publication,&services,&caller,only,stage);
}
static UssPairObservation formation_observe_service(void* user)
{UssFormationRuntime* r=(UssFormationRuntime*)user;formation_refresh(r);return r->observed;}
static UssPairServiceResult formation_select(void* user,const UssPairObservation* expected,uint32_t slot)
{
    UssFormationRuntime* r=(UssFormationRuntime*)user;UssPairServiceResult selected;
    if(!formation_refresh(r) || !formation_match(expected,&r->observed))return USS_PAIR_SERVICE_STOP;
    selected=r->io.select(r->io.user,&r->latest,slot);
    if(selected==USS_PAIR_SERVICE_WAIT && slot==1)return USS_PAIR_SERVICE_WAIT; /* Proven pre-entry only. */
    if(selected!=USS_PAIR_SERVICE_DONE || !formation_refresh(r) ||
        !pair_context_equal(&expected->context,&r->observed.context) ||
        r->observed.pair_generation!=expected->pair_generation || r->observed.guard_generation!=expected->guard_generation ||
        r->observed.support!=expected->support || r->observed.active!=slot)return USS_PAIR_SERVICE_STOP;
    return USS_PAIR_SERVICE_DONE;
}
static UssPairServiceResult formation_ready(void* user,const UssPairObservation* expected)
{
    UssFormationRuntime* r=(UssFormationRuntime*)user;
    if(!formation_refresh(r) || !formation_match(expected,&r->observed) || !r->latest.primary.valid)return USS_PAIR_SERVICE_STOP;
    return formation_draw_ready(r,&r->latest)?USS_PAIR_SERVICE_DONE:USS_PAIR_SERVICE_WAIT;
}
static UssPairServiceResult formation_switch(void* user,const UssPairObservation* expected,const UssPairStep* step,unsigned defer_ack)
{
    UssFormationRuntime* r=(UssFormationRuntime*)user;UssPublicationResult pre;UssMemberEvidence* target;
    if(!formation_refresh(r) || !formation_match(expected,&r->observed) || !formation_draw_ready(r,&r->latest))return USS_PAIR_SERVICE_STOP;
    /* Same Lua pre-attach formation attempt. Pending ordinary/parent readiness
     * may refuse before entry; only uncertain/disabled publication is terminal. */
    if(!r->latest.context.local_only){
    pre=formation_publish(r,1,USS_PUBLICATION_PRE_ATTACH);
    if(pre.outcome==USS_PUBLICATION_UNCERTAIN || pre.outcome==USS_PUBLICATION_NOT_ENTERED || r->publication.disabled ||
        !formation_refresh(r) || !formation_match(expected,&r->observed) || !formation_draw_ready(r,&r->latest))return USS_PAIR_SERVICE_STOP;
    }
    if(r->io.attach(r->io.user,&r->latest,&step->member)!=USS_PAIR_SERVICE_DONE)return USS_PAIR_SERVICE_STOP;
    if(!formation_refresh(r) || !pair_context_equal(&expected->context,&r->observed.context) ||
        r->observed.pair_generation!=expected->pair_generation || r->observed.guard_generation!=expected->guard_generation ||
        r->observed.support!=step->member.entity || r->observed.active!=1)return USS_PAIR_SERVICE_STOP;
    target=step->member.entity==r->life.owner.utility.entity?&r->latest.utility:&r->latest.ordinary;
    if(target->entity!=step->member.entity || !target->parent_matches || !target->interaction_admissible)return USS_PAIR_SERVICE_STOP;
    /* The outer coordinator publishes at the exact step stage before final draw.
     * A pre-entry publication WAIT never repeats this consumed attachment. */
    if(!defer_ack && r->io.acknowledge)r->io.acknowledge(r->io.user,expected->support,step->member.entity,step->label);
    return USS_PAIR_SERVICE_DONE;
}
static UssPairServiceResult formation_boundary(void* user,const UssPairObservation* expected,const UssPairStep* step)
{
    UssFormationRuntime* r=(UssFormationRuntime*)user;UssPublicationResult published;UssPublicationStage stage;
    if(!formation_refresh(r) || !formation_match(expected,&r->observed))return USS_PAIR_SERVICE_STOP;
    if(r->latest.context.local_only)
        return uss_local_pair_accept(&r->local,&r->life.owner,&r->latest)?USS_PAIR_SERVICE_DONE:USS_PAIR_SERVICE_STOP;
    stage=step->publication_stage==USS_PAIR_UTILITY_FIRST_CAPTURE?USS_PUBLICATION_UTILITY_FIRST:USS_PUBLICATION_POST_ATTACH;
    published=formation_publish(r,0,stage);
    if(published.outcome==USS_PUBLICATION_RETURNED || published.outcome==USS_PUBLICATION_UNCHANGED)return USS_PAIR_SERVICE_DONE;
    if(published.outcome==USS_PUBLICATION_REFUSED && !r->publication.disabled && !r->publication.suspended &&
        (published.reason==USS_PUB_CAPTURE || published.reason==USS_PUB_ENTRY_CHANGED ||
         published.reason==USS_PUB_CALLER_CHANGED || published.reason==USS_PUB_RATE_LIMIT))return USS_PAIR_SERVICE_WAIT;
    return USS_PAIR_SERVICE_STOP;
}
static int formation_verify(void* user,const UssPairObservation* expected)
{
    UssFormationRuntime* r=(UssFormationRuntime*)user;
    return formation_refresh(r) && formation_match(expected,&r->observed) &&
        r->latest.utility.parent_matches && r->latest.ordinary.parent_matches;
}
static void formation_acknowledge(void* user,uint32_t outgoing,uint32_t incoming,UssPairLabel label)
{
    UssFormationRuntime* r=(UssFormationRuntime*)user;
    if(r->io.acknowledge)r->io.acknowledge(r->io.user,outgoing,incoming,label);
}
static UssPairServices formation_services(UssFormationRuntime* r)
{
    UssPairServices services={0};services.user=r;services.observe=formation_observe_service;
    services.select_slot=formation_select;services.primary_ready=formation_ready;services.switch_member=formation_switch;
    services.publish_boundary=formation_boundary;services.verify_pair=formation_verify;
    if(r->io.acknowledge)services.acknowledge=formation_acknowledge;return services;
}
static inline int uss_formation_runtime_init(UssFormationRuntime* r,const UssFormationIo* io,unsigned thread,const UssCoreFlags* policy)
{
    if(!r || !io || !thread || !policy || !policy->known || !io->clock_ms || !io->observe ||
        !io->capture || !io->select || !io->attach || !io->send)return 0;
    memset(r,0,sizeof(*r));r->io=*io;r->thread=thread;r->policy=*policy;r->reason="IDLE";return 1;
}
static inline int uss_formation_start(UssFormationRuntime* r,UssPairOrder order,const UssPairMember* target)
{
    UssPairServices services;unsigned pickup;uint32_t selected;
    if(!r || r->busy || r->work.phase==USS_PAIR_ACTIVE || !formation_refresh(r))return 0;
    if(order==USS_PAIR_DRAW){selected=3;pickup=0;}
    else {if(r->life.phase!=USS_LIFE_PENDING || r->life.plan.order!=order)return 0;
        selected=r->life.plan.selected;pickup=r->life.plan.switch_on_pickup;}
    if(r->work.phase==USS_PAIR_CANCELLED || r->life.phase==USS_LIFE_PAUSED)return 0;
    r->controller_draw=0;memset(&r->work,0,sizeof(r->work));services=formation_services(r);
    return uss_pair_begin(&r->work,&services,&r->observed,order,selected,pickup,target,r->io.clock_ms(r->io.user));
}
/* Dedicated-mode primary/secondary exit: same guarded publication boundary,
 * retaining the player's selected slot rather than drawing support afterward.
 * RETAINED is already the finite transaction recipe for this final selection. */
static inline int uss_formation_store(UssFormationRuntime* r,const UssPairMember* target,uint32_t selected)
{
    UssPairServices services;
    if(!r || selected<1 || selected>2 || r->busy || r->work.phase==USS_PAIR_ACTIVE ||
        r->work.phase==USS_PAIR_CANCELLED || r->life.phase!=USS_LIFE_FORMED || !formation_refresh(r))return 0;
    r->controller_draw=0;memset(&r->work,0,sizeof(r->work));services=formation_services(r);
    return uss_pair_begin(&r->work,&services,&r->observed,USS_PAIR_RETAINED,selected,0,target,r->io.clock_ms(r->io.user));
}
static inline UssPairPhase uss_formation_tick(UssFormationRuntime* r)
{
    UssPairServices services;UssPairPhase phase;
    if(!r || r->busy)return USS_PAIR_CANCELLED;
    if(r->work.phase!=USS_PAIR_ACTIVE)return r->work.phase;
    r->busy=1;services=formation_services(r);phase=uss_pair_tick(&r->work,&services,r->io.clock_ms(r->io.user));
    if(phase==USS_PAIR_COMPLETE && r->work.order!=USS_PAIR_DRAW && r->work.order!=USS_PAIR_RETAINED){
        if(!formation_refresh(r) || !uss_lifecycle_complete(&r->life,r->thread,&r->latest,&r->work)){
            pair_cancel(&r->work,"CORE_COMPLETION_CHANGED");phase=USS_PAIR_CANCELLED;
        }
    }
    if(phase==USS_PAIR_CANCELLED){r->life.phase=USS_LIFE_PAUSED;pair_owner_interrupt(&r->life.owner);}
    r->reason=r->work.reason;r->busy=0;return phase;
}
#endif
