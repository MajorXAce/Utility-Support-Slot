/* Explicit diagnostic commands. No continuous pickup detection/input claims. */
#ifndef USS_FORMATION_COMMAND_H
#define USS_FORMATION_COMMAND_H
#include "formation_native.h"
#define USS_FORMATION_MAGIC UINT32_C(0x39344648)
typedef struct UssFormationTicket {
    uint32_t magic,version,pid,sdk;
    uint64_t process_started,nonce;
    uint32_t build,reserved;
} UssFormationTicket;
typedef struct UssFormationRequest {
    UssFormationTicket ticket;
    uint32_t sequence,action,switch_on_pickup,role;
    uint64_t issued_ms;
} UssFormationRequest;
enum {USS_COMMAND_REMEMBER=1,USS_COMMAND_FORM,USS_COMMAND_UTILITY,USS_COMMAND_ORDINARY};
_Static_assert(sizeof(UssFormationTicket)==40,"formation ticket wire size");
_Static_assert(sizeof(UssFormationRequest)==64,"formation command_request wire size");
static inline int formation_request_valid_range(const UssFormationTicket* ticket,const UssFormationRequest* command_request,
    unsigned sequence,uint64_t now,uint32_t magic,unsigned last_action)
{
    return ticket && command_request && ticket->magic==magic && ticket->version==1 &&
        ticket->pid && ticket->process_started && ticket->nonce && ticket->sdk>=11 &&
        ticket->build==25480438 && !ticket->reserved && !memcmp(ticket,&command_request->ticket,sizeof(*ticket)) &&
        sequence>=1 && sequence<=16 && command_request->sequence==sequence && command_request->action>=USS_COMMAND_REMEMBER &&
        command_request->action<=last_action && command_request->switch_on_pickup<=1 &&
        (command_request->role==HF_SESSION_HOST || command_request->role==HF_SESSION_JOINER) &&
        command_request->issued_ms && command_request->issued_ms<=now && now-command_request->issued_ms<=30000;
}
static inline int uss_formation_request_valid(const UssFormationTicket* ticket,const UssFormationRequest* command_request,
    unsigned sequence,uint64_t now)
{return formation_request_valid_range(ticket,command_request,sequence,now,USS_FORMATION_MAGIC,USS_COMMAND_ORDINARY);}
static inline int uss_formation_command(UssFormationNative* native,unsigned action,unsigned pickup,unsigned role)
{
    UssFormationRuntime* runtime=native->runtime;UssCorePlatform platform={0};UssCoreFlags flags;
    UssPairMember member={0};UssCoreCandidateNames names;UssCoreCandidates bundle;
    UssPickupPolicy policy={1,pickup};UssPickupDecision decision;
    if(!runtime)return 0;
    runtime->reason="COMMAND_CHECKING";
    runtime->command_bundle_valid=0;runtime->command_decision=0;
    if(pickup>1){runtime->reason="COMMAND_PICKUP_POLICY";return 0;}
    if(runtime->busy || runtime->work.phase==USS_PAIR_ACTIVE){runtime->reason="COMMAND_WORK_ACTIVE";return 0;}
    if(runtime->life.phase==USS_LIFE_PAUSED){runtime->reason="COMMAND_LIFECYCLE_PAUSED";return 0;}
    if(!formation_refresh(runtime)){runtime->reason="COMMAND_REFRESH_REFUSED";return 0;}
    if(!runtime->latest.mission){runtime->reason="COMMAND_NOT_MISSION";return 0;}
    if(runtime->latest.context.role!=role){runtime->reason="COMMAND_ROLE_MISMATCH";return 0;}
    /* These standing, primary-held test windows do not establish real down/input
     * detection. They are explicit operator setup, not production Core policy. */
    if(!runtime->latest.primary.ready){runtime->reason="COMMAND_PRIMARY_NOT_READY";return 0;}
    if(action==USS_COMMAND_UTILITY || action==USS_COMMAND_ORDINARY){
        if(runtime->life.phase!=USS_LIFE_FORMED){runtime->reason="COMMAND_PAIR_NOT_FORMED";return 0;}
        member=action==USS_COMMAND_UTILITY?runtime->life.owner.utility:runtime->life.owner.ordinary;
        if(!uss_formation_start(runtime,USS_PAIR_DRAW,&member)){runtime->reason="COMMAND_DRAW_START_REFUSED";return 0;}
        return 1;
    }
    if((action!=USS_COMMAND_REMEMBER && action!=USS_COMMAND_FORM) || runtime->life.owner.pair_present ||
        (action==USS_COMMAND_REMEMBER && runtime->history.remembered) ||
        (action==USS_COMMAND_FORM && !runtime->history.remembered)){runtime->reason="COMMAND_HISTORY_OR_ORDER";return 0;}
    member.entity=runtime->latest.inventory.slots[2];memcpy(member.identity,runtime->latest.inventory.identities[2],8);
    if(!uss_core_candidate_names(&runtime->life.owner,&member,1,&names)){runtime->reason="COMMAND_CANDIDATE_NAMES";return 0;}
    flags=uss_lifecycle_flags(&runtime->life,&runtime->policy);
    platform.user=native->platform.user;platform.thread_id=native->platform.thread_id;
    bundle=uss_core_observe_candidates(native->api,native->mod,&runtime->life.owner,&flags,&platform,&names,NULL);
    decision=uss_pickup_step(&runtime->history,&runtime->life,runtime->thread,&bundle,&policy);
    runtime->command_bundle_valid=bundle.valid;runtime->command_decision=decision.kind;
    if(decision.kind==USS_PICKUP_NEED_READ && action==USS_COMMAND_FORM){
        flags=uss_lifecycle_flags(&runtime->life,&runtime->policy);
        bundle=uss_core_observe_candidates(native->api,native->mod,&runtime->life.owner,&flags,&platform,&decision.needed,NULL);
        decision=uss_pickup_step(&runtime->history,&runtime->life,runtime->thread,&bundle,&policy);
        runtime->command_bundle_valid=bundle.valid;runtime->command_decision=decision.kind;
    }
    if(action==USS_COMMAND_REMEMBER){
        if(decision.kind!=USS_PICKUP_REMEMBERED){runtime->reason="COMMAND_REMEMBER_DECISION";return 0;}
        runtime->reason="COMMAND_REMEMBERED";return 1;
    }
    if(decision.kind!=USS_PICKUP_PAIR_PENDING){runtime->reason="COMMAND_FORM_DECISION";return 0;}
    if(!uss_formation_start(runtime,runtime->life.plan.order,NULL)){runtime->reason="COMMAND_FORM_START_REFUSED";return 0;}
    return 1;
}
#endif
