/* Pure coordinator port of Lua formation_transaction.lua.
 * Services own validated reads/effects; this header has no SDK/game addresses. */
#ifndef USS_PAIR_TRANSACTION_H
#define USS_PAIR_TRANSACTION_H
#include "weapon_policy.h"

typedef struct UssPairMember { uint32_t entity; unsigned char identity[8]; } UssPairMember;
typedef struct UssPairContext {
    uintptr_t owner,inventory,row_address;
    uint32_t avatar,primary,secondary,backpack;
} UssPairContext;
typedef struct UssPairObservation {
    unsigned valid,mission,host,joined_client,eligible,formed,formation_pending,disabled,suspended,paused,down,input,can_switch;
    unsigned members_valid,local_only; /* Explicit single-peer route, not network publication. */
    uint64_t epoch,pair_generation,guard_generation;
    UssPairContext context;
    UssPairMember utility,ordinary;
    uint32_t utility_parent,ordinary_parent,support,active;
} UssPairObservation;
typedef enum UssPairOrder {
    USS_PAIR_ORDINARY_FIRST=1,USS_PAIR_UTILITY_FIRST,USS_PAIR_DRAW,USS_PAIR_RETAINED
} UssPairOrder;
typedef enum UssPairPhase {
    USS_PAIR_IDLE=0,USS_PAIR_ACTIVE,USS_PAIR_COMPLETE,USS_PAIR_CANCELLED
} UssPairPhase;
typedef enum UssPairServiceResult {
    USS_PAIR_SERVICE_STOP=0,
    USS_PAIR_SERVICE_DONE,
    USS_PAIR_SERVICE_WAIT /* Only pre-entry readiness/publication refusal may wait. */
} UssPairServiceResult;
typedef enum UssPairLabel {
    USS_PAIR_STORE_NEW_UTILITY=1,USS_PAIR_CAPTURE_UTILITY_FIRST,
    USS_PAIR_RESTORE_UTILITY_FIRST,USS_PAIR_DRAW_UTILITY,USS_PAIR_DRAW_ORDINARY
} UssPairLabel;
typedef enum UssPairPublicationStage {
    USS_PAIR_POST_ATTACH=1,USS_PAIR_UTILITY_FIRST_CAPTURE
} UssPairPublicationStage;
typedef struct UssPairStep {
    UssPairMember member;
    UssPairLabel label;
    UssPairPublicationStage publication_stage;
} UssPairStep;
typedef struct UssPairTransaction {
    UssPairPhase phase;
    UssPairOrder order;
    UssPairObservation origin;
    UssPairStep steps[2];
    unsigned step_count,index,primary_requested,attachment_consumed;
    uint32_t selected,expected_support,outgoing,real;
    unsigned utility_drawn;
    uint64_t started_ms,deadline_ms;
    const char* reason;
} UssPairTransaction;
typedef struct UssPairServices {
    void* user;
    UssPairObservation (*observe)(void*);
    /* DONE includes local post-read verification. WAIT only for primary-slot
     * readiness before either selection call; an entered selection never retries. */
    UssPairServiceResult (*select_slot)(void*,const UssPairObservation*,uint32_t);
    UssPairServiceResult (*primary_ready)(void*,const UssPairObservation*);
    /* Complete Lua switch_entity boundary, NOT merely a raw attach function.
     * It owns formation-once/pre/post checks and may already publish incoming.
     * Pair draw defers HUD acknowledgement until final draw. WAIT is forbidden. */
    UssPairServiceResult (*switch_member)(void*,const UssPairObservation*,const UssPairStep*,unsigned defer_ack);
    /* Must deduplicate a returned switch publication; WAIT only before entry.
     * Uncertain send is STOP, never WAIT. Actual publisher is not ported here. */
    UssPairServiceResult (*publish_boundary)(void*,const UssPairObservation*,const UssPairStep*);
    int (*verify_pair)(void*,const UssPairObservation*);
    void (*acknowledge)(void*,uint32_t outgoing,uint32_t incoming,UssPairLabel);
} UssPairServices;

static int pair_services_valid(const UssPairServices* services)
{
    return services && services->observe && services->select_slot && services->primary_ready &&
        services->switch_member && services->publish_boundary && services->verify_pair;
}
static int pair_member_equal(const UssPairMember* a,const UssPairMember* b)
{return a->entity==b->entity && !memcmp(a->identity,b->identity,8);}
static int pair_context_equal(const UssPairContext* a,const UssPairContext* b)
{
    return a->owner==b->owner && a->inventory==b->inventory && a->row_address==b->row_address &&
        a->avatar==b->avatar && a->primary==b->primary && a->secondary==b->secondary && a->backpack==b->backpack;
}
static int pair_scope(const UssPairObservation* observed)
{
    return observed->valid && observed->mission && (observed->host || observed->joined_client || observed->local_only) &&
        !observed->disabled && !observed->suspended && !observed->paused && !observed->down &&
        observed->can_switch && observed->members_valid;
}
static int pair_guard(const UssPairTransaction* work,const UssPairObservation* observed)
{
    const UssPairObservation* origin=&work->origin;
    if(!pair_scope(observed) || observed->epoch!=origin->epoch ||
       observed->host!=origin->host || observed->joined_client!=origin->joined_client ||
       observed->local_only!=origin->local_only || observed->pair_generation!=origin->pair_generation ||
       observed->guard_generation!=origin->guard_generation ||
       !pair_context_equal(&origin->context,&observed->context) ||
       !pair_member_equal(&origin->utility,&observed->utility) ||
       !pair_member_equal(&origin->ordinary,&observed->ordinary) || observed->support!=work->expected_support)
        return 0;
    if(observed->utility_parent && observed->utility_parent!=observed->context.avatar) return 0;
    if(observed->ordinary_parent && observed->ordinary_parent!=observed->context.avatar) return 0;
    if((work->order==USS_PAIR_DRAW || work->order==USS_PAIR_RETAINED) && (!observed->eligible || !observed->formed || observed->input ||
       observed->utility_parent!=observed->context.avatar || observed->ordinary_parent!=observed->context.avatar ||
       (!work->primary_requested && observed->active!=origin->active))) return 0;
    return 1;
}
static inline int uss_pair_begin(UssPairTransaction* work,const UssPairServices* services,
    const UssPairObservation* origin,UssPairOrder order,uint32_t selected,unsigned switch_on_pickup,
    const UssPairMember* draw_target,uint64_t now)
{
    UssPairTransaction candidate={0};
    UssPairStep ordinary,utility;
    if(!work || work->phase!=USS_PAIR_IDLE || !pair_services_valid(services) || !origin || !pair_scope(origin) ||
       !origin->pair_generation || !origin->context.avatar || origin->context.avatar==UINT32_MAX || !origin->context.owner ||
       !origin->context.inventory || !origin->context.row_address ||
       selected<1 || selected>3 || switch_on_pickup>1 || now>UINT64_MAX-2000 ||
       !origin->utility.entity || origin->utility.entity==UINT32_MAX ||
       !origin->ordinary.entity || origin->ordinary.entity==UINT32_MAX ||
       origin->utility.entity==origin->ordinary.entity ||
       uss_classify_support(origin->utility.identity,2)!=USS_UTILITY_SUPPORT ||
       uss_classify_support(origin->ordinary.identity,2)!=USS_ORDINARY_SUPPORT) return 0;
    candidate.phase=USS_PAIR_ACTIVE;candidate.order=order;candidate.origin=*origin;
    candidate.selected=selected;candidate.expected_support=origin->support;
    candidate.started_ms=now;candidate.deadline_ms=now+2000;candidate.reason="ACTIVE";
    ordinary.member=origin->ordinary;ordinary.label=USS_PAIR_STORE_NEW_UTILITY;
    ordinary.publication_stage=USS_PAIR_POST_ATTACH;
    utility.member=origin->utility;utility.label=USS_PAIR_DRAW_UTILITY;
    utility.publication_stage=USS_PAIR_POST_ATTACH;
    if(order==USS_PAIR_ORDINARY_FIRST) {
        if(!origin->formation_pending || origin->support!=origin->utility.entity || (switch_on_pickup && selected!=3)) return 0;
        candidate.steps[0]=ordinary;candidate.step_count=1;
        if(switch_on_pickup) {candidate.steps[1]=utility;candidate.step_count=2;}
    } else if(order==USS_PAIR_UTILITY_FIRST) {
        if(!origin->formation_pending || origin->support!=origin->ordinary.entity) return 0;
        utility.label=USS_PAIR_CAPTURE_UTILITY_FIRST;utility.publication_stage=USS_PAIR_UTILITY_FIRST_CAPTURE;
        ordinary.label=USS_PAIR_RESTORE_UTILITY_FIRST;
        candidate.steps[0]=utility;candidate.steps[1]=ordinary;candidate.step_count=2;
    } else if(order==USS_PAIR_DRAW || order==USS_PAIR_RETAINED) {
        if((order==USS_PAIR_DRAW && selected!=3) || !draw_target || !origin->eligible ||
           (origin->support!=origin->ordinary.entity && origin->support!=origin->utility.entity)) return 0;
        if(pair_member_equal(draw_target,&origin->utility)) candidate.steps[0]=utility;
        else if(pair_member_equal(draw_target,&origin->ordinary)) {
            ordinary.label=USS_PAIR_DRAW_ORDINARY;candidate.steps[0]=ordinary;
        } else return 0;
        candidate.step_count=1;
    } else return 0;
    if(!pair_guard(&candidate,origin)) return 0;
    *work=candidate;return 1;
}
static void pair_cancel(UssPairTransaction* work,const char* reason)
{work->phase=USS_PAIR_CANCELLED;work->reason=reason;}
static inline UssPairPhase uss_pair_tick(UssPairTransaction* work,const UssPairServices* services,uint64_t now)
{
    UssPairObservation current,after;
    const UssPairStep* step;
    UssPairServiceResult status;
    if(!work) return USS_PAIR_IDLE;
    if(work->phase!=USS_PAIR_ACTIVE) return work->phase;
    if(!pair_services_valid(services)) {pair_cancel(work,"SERVICES_CHANGED");return work->phase;}
    if(now<work->started_ms || now>=work->deadline_ms) {pair_cancel(work,"TIMEOUT");return work->phase;}
    current=services->observe(services->user);
    if(!pair_guard(work,&current)) {pair_cancel(work,"CONTEXT_OR_PICKUP_CHANGED");return work->phase;}
    if(!work->primary_requested) {
        if(current.active!=1){
            status=services->select_slot(services->user,&current,1);
            if(status==USS_PAIR_SERVICE_WAIT)return work->phase; /* No selection entered; bounded by deadline. */
            if(status!=USS_PAIR_SERVICE_DONE){pair_cancel(work,"PRIMARY_SELECTION_FAILED");return work->phase;}
        }
        work->primary_requested=1; /* Only after accepted selection or already-primary. */
        current=services->observe(services->user);
    }
    if(!pair_guard(work,&current) || current.active!=1) {pair_cancel(work,"SELECTION_CHANGED");return work->phase;}
    status=services->primary_ready(services->user,&current);
    if(status==USS_PAIR_SERVICE_WAIT) return work->phase;
    if(status!=USS_PAIR_SERVICE_DONE) {pair_cancel(work,"PRIMARY_GUARD_FAILED");return work->phase;}
    step=&work->steps[work->index];
    if(!work->attachment_consumed) {
        work->attachment_consumed=1;work->outgoing=current.support;
        if(services->switch_member(services->user,&current,step,work->order==USS_PAIR_DRAW || work->order==USS_PAIR_RETAINED)!=USS_PAIR_SERVICE_DONE) {
            pair_cancel(work,"ATTACH_FAILED_OR_UNCERTAIN");return work->phase;
        }
        work->expected_support=step->member.entity;
    }
    after=services->observe(services->user);
    if(!pair_guard(work,&after) || after.active!=1 || after.support!=step->member.entity) {
        pair_cancel(work,"ATTACH_CONTEXT_CHANGED");return work->phase;
    }
    status=services->publish_boundary(services->user,&after,step);
    if(status==USS_PAIR_SERVICE_WAIT) return work->phase;
    if(status!=USS_PAIR_SERVICE_DONE) {pair_cancel(work,"PUBLICATION_FAILED_OR_UNCERTAIN");return work->phase;}
    ++work->index;work->attachment_consumed=0;
    if(work->index<work->step_count) return work->phase;
    if(!services->verify_pair(services->user,&after)) {pair_cancel(work,"FINAL_PAIR_CHANGED");return work->phase;}
    if(after.active!=work->selected && services->select_slot(services->user,&after,work->selected)!=USS_PAIR_SERVICE_DONE) {
        pair_cancel(work,"FINAL_SELECTION_FAILED");return work->phase;
    }
    work->real=step->member.entity;
    work->utility_drawn=work->real==work->origin.utility.entity && work->selected==3;
    work->phase=USS_PAIR_COMPLETE;work->reason="COMPLETE";
    if((work->order==USS_PAIR_DRAW || work->order==USS_PAIR_RETAINED) && services->acknowledge)
        services->acknowledge(services->user,work->outgoing,work->real,step->label);
    return work->phase;
}
#endif
