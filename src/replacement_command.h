/* HF052 controlled host experiment. No automatic input/pickup discovery. */
#ifndef USS_REPLACEMENT_COMMAND_H
#define USS_REPLACEMENT_COMMAND_H
#include "formation_command.h"
#include "replacement_adapter.h"
#define USS_REPLACEMENT_MAGIC UINT32_C(0x32354648)
enum {USS_COMMAND_ORIGIN=5,USS_COMMAND_REPLACE,USS_COMMAND_STATUS};
typedef struct UssReplacementRequest {
    UssFormationRequest base;
    uint32_t incoming;
    unsigned char identity[8];
    uint32_t reserved;
} UssReplacementRequest;
_Static_assert(sizeof(UssReplacementRequest)==80,"replacement command_request wire size");
typedef struct UssReplacementDiagnostic {
    UssReplacementOriginEvidence origin;
    UssReplacementWork work;
    UssReplacementAdapter adapter;
    UssReplacementServices services;
    UssPairMember status;
    unsigned origin_taken,started,status_sequence;
} UssReplacementDiagnostic;
static inline int uss_replacement_request_valid(const UssFormationTicket* ticket,const UssReplacementRequest* command_request,
    unsigned sequence,uint64_t now)
{
    const unsigned char zero[8]={0};
    return command_request && !command_request->reserved && formation_request_valid_range(ticket,&command_request->base,sequence,now,
        USS_REPLACEMENT_MAGIC,USS_COMMAND_STATUS) &&
        (command_request->base.action<USS_COMMAND_ORIGIN || command_request->base.role==HF_SESSION_HOST) &&
        (command_request->base.action==USS_COMMAND_REPLACE?
            publication_id(command_request->incoming) && memcmp(command_request->identity,zero,8):
            !command_request->incoming && !memcmp(command_request->identity,zero,8));
}
/* Explicit production invoker. Construction/startup does not call it. Every
 * address comes from HF051's checked entry; no independent address discovery. */
#include "uss_replacement_invoke.h"
static UssReplacementProducer replacement_diagnostic_producer(UssReplacementDiagnostic* d,UssFormationNative* native)
{
    UssReplacementProducer p={0};UssFormationRuntime* r=native->runtime;
    p.api=native->api;p.mod=native->mod;p.platform=native->platform;p.life=&r->life;p.history=&r->history;
    p.flags=&r->policy;p.work=&d->work;p.origin=&d->origin;p.clock_ms=r->io.clock_ms;p.user=r->io.user;return p;
}
static int uss_replacement_diagnostic_command(UssReplacementDiagnostic* d,UssFormationNative* native,
    const UssReplacementRequest* command_request,
    int (*invoke)(void*,UssReplacementEffect,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uint32_t,uint32_t,const UssInventoryMessage*),void* invoke_user)
{
    UssFormationRuntime* r=native->runtime;UssReplacementProducer producer;
    UssCoreCandidateNames names;UssCoreCandidates bundle;UssCoreFlags flags;UssCorePlatform platform;
    UssPickupDecision decision;UssPickupPolicy policy={1,command_request->base.switch_on_pickup};UssPairMember incoming;
    unsigned action=command_request->base.action;
    if(!d || !r || r->busy || r->work.phase==USS_PAIR_ACTIVE || d->work.phase==USS_REPLACE_ACTIVE)return 0;
    if(action<=USS_COMMAND_ORDINARY){
        if(d->origin_taken && !d->started)return 0;
        return uss_formation_command(native,action,command_request->base.switch_on_pickup,command_request->base.role);
    }
    if(!formation_refresh(r) || !r->latest.mission || command_request->base.role!=HF_SESSION_HOST ||
        r->latest.context.role!=HF_SESSION_HOST || r->life.phase!=USS_LIFE_FORMED)return 0;
    producer=replacement_diagnostic_producer(d,native);
    if(action==USS_COMMAND_ORIGIN){
        if(d->origin_taken || d->started || !r->latest.primary.ready ||
            !uss_replacement_read_origin(&producer,&d->origin))return 0;
        d->origin_taken=1;return 1;
    }
    if(action==USS_COMMAND_STATUS){
        d->status.entity=r->latest.inventory.slots[2];memcpy(d->status.identity,r->latest.inventory.identities[2],8);
        d->status_sequence=command_request->base.sequence;return publication_id(d->status.entity);
    }
    if(action!=USS_COMMAND_REPLACE || !d->origin_taken || d->started || !d->status_sequence ||
        command_request->incoming!=d->status.entity || memcmp(command_request->identity,d->status.identity,8) ||
        r->latest.inventory.slots[2]!=command_request->incoming || memcmp(r->latest.inventory.identities[2],command_request->identity,8))return 0;
    incoming.entity=command_request->incoming;memcpy(incoming.identity,command_request->identity,8);
    if(!uss_core_candidate_names(&r->life.owner,&incoming,1,&names))return 0;
    flags=uss_lifecycle_flags(&r->life,&r->policy);platform.user=native->platform.user;platform.thread_id=native->platform.thread_id;
    bundle=uss_core_observe_candidates(native->api,native->mod,&r->life.owner,&flags,&platform,&names,NULL);
    decision=uss_pickup_step(&r->history,&r->life,r->thread,&bundle,&policy);
    d->started=1; /* One attempt, including an admission refusal. Never refresh origin. */
    if(decision.kind!=USS_PICKUP_REPLACEMENT_INTENT ||
        !uss_replacement_adapter_init(&d->adapter,&producer,&r->life,&r->publication,invoke,invoke_user))return 0;
    d->services=uss_replacement_adapter_services(&d->adapter);
    return uss_replacement_begin(&d->work,&r->life,r->thread,&r->history,&decision,&d->origin.origin,
        &r->publication,&d->services,r->io.clock_ms(r->io.user));
}
#endif
