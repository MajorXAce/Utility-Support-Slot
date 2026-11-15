/* Internal bounded Core observation. No callback, native binding or effects.
 * Existing public action/member/readiness APIs retain their SOLO-only contracts. */
#ifndef USS_CORE_OBSERVATION_H
#define USS_CORE_OBSERVATION_H
#include "pair_owner.h"
#include "publication_capture.h"
#include "uss_member_pin.h"
#include "pair_member.h"
#include "utility_lineage.h"

typedef struct UssCoreFlags {
    unsigned known,eligible,formed,formation_pending,disabled,suspended,paused,down,input;
} UssCoreFlags;
typedef struct UssCoreScope {
    uint32_t sdk_state,role,version,state_value,mode_value;
    uintptr_t game,state,mode,session,mapping;
    size_t game_size;
    unsigned mission,local_only;
    UssCapturePeers peers;
} UssCoreScope;
typedef struct UssCoreObservation {
    unsigned valid,mission,pair_matched,thread,expected_pair_present;
    uint64_t expected_epoch,expected_context,expected_pair,expected_guard;
    UssPairOwnerContext context;
    Inventory inventory;
    UssPrimaryReadiness primary;
    UssMemberEvidence utility,ordinary;
    UssCoreFlags flags;
    UssSupportClass support_class;
} UssCoreObservation;
typedef struct UssCorePlatform {
    void* user;
    uint32_t (*thread_id)(void*);
} UssCorePlatform;

/* Log-only: name the sub-check of core_scope that refused (Reader.detail). Returns 0 so a caller can write return core_refuse(...). */
static int core_refuse(Reader* memory,const char* detail){memory->detail=detail;return 0;}
static int core_scope(Reader* memory,const HfApi* api,HfMod* mod,UssCoreScope* scope)
{
    HfSession session={0};UssCaptureReader reader={0};unsigned char pin[7];
    static const unsigned char state_pin[7]={0x48,0x8b,0x0d,0x15,0x23,0x1e,0x03};
    memset(scope,0,sizeof(*scope));scope->sdk_state=api->game_state(mod);
    if(scope->sdk_state<HF_STATE_SPLASH || scope->sdk_state>HF_STATE_PREPARE_MISSION)return core_refuse(memory,"SDK_STATE");
    api->session(mod,&session);scope->role=session.role;scope->version=session.version;
    scope->game=api->module_base("game.dll",&scope->game_size);memory->stage="CORE_SCOPE";
    /* Same checks, same order and same reads as the single combined test this replaced; each refusal now names itself. */
    if(!scope->game)return core_refuse(memory,"MODULE_BASE");
    if(scope->game_size<0x3484570 || scope->game_size>0x20000000 || !address_ok(scope->game,scope->game_size))
        return core_refuse(memory,"MODULE_SIZE");
    if(!get(memory,scope->game+0x144024,pin,7))return core_refuse(memory,"STATE_PIN_READ");
    if(memcmp(pin,state_pin,7))return core_refuse(memory,"STATE_PIN_BYTES");
    if(!get_ptr(memory,scope->game+0x3326340,&scope->state))return core_refuse(memory,"STATE_POINTER");
    if(!get_ptr(memory,scope->game+0x33266A0,&scope->mode))return core_refuse(memory,"MODE_POINTER");
    if(!get(memory,scope->state+0xAC21C,&scope->state_value,4))return core_refuse(memory,"STATE_VALUE");
    if(!get(memory,scope->mode+8,&scope->mode_value,4))return core_refuse(memory,"MODE_VALUE");
    scope->mission=scope->state_value==4 && scope->mode_value==1;
    if(scope->mission!=(unsigned)(scope->sdk_state==HF_STATE_MISSION))return core_refuse(memory,"MISSION_MISMATCH");
    if(!scope->mission)return 1; /* Confirmed outside, not an inventory read failure. */
    if(scope->role!=HF_SESSION_SOLO && scope->role!=HF_SESSION_HOST && scope->role!=HF_SESSION_JOINER)
        return core_refuse(memory,"SESSION_ROLE");
    if(!get_ptr(memory,scope->game+0x347CEF0,&scope->session))return core_refuse(memory,"SESSION_POINTER");
    if(!get_ptr(memory,scope->game+0x346BF98,&scope->mapping))return core_refuse(memory,"MAPPING_POINTER");
    reader.memory=*memory;
    {int ok=capture_peers(&reader,scope->session,&scope->peers);*memory=reader.memory;
        if(!ok)return 0; /* capture_peers already named the failing peer check */
        scope->local_only=scope->peers.count==1;
        if(scope->local_only){
            if(scope->role==HF_SESSION_JOINER || !scope->peers.is_host)return core_refuse(memory,"PEER_ROLE_LOCAL");
        }else if(scope->role==HF_SESSION_SOLO || scope->peers.is_host!=(unsigned)(scope->role==HF_SESSION_HOST))
            return core_refuse(memory,"PEER_ROLE_REMOTE");}

    return 1;
}
static UssPairOwnerContext core_context(const Inventory* inventory,const UssCoreScope* scope)
{
    UssPairOwnerContext context={0};
    /* roots[0] is owner_manager; roots[1] is player_manager (local unit).
     * Match Lua sample_inventory and the publication producer. */
    context.local.owner=inventory->roots[0];context.local.inventory=inventory->roots[2];
    context.local.row_address=inventory->row_address;context.local.avatar=inventory->avatar;
    context.local.primary=inventory->slots[0];context.local.secondary=inventory->slots[1];context.local.backpack=inventory->slots[3];
    context.local_only=scope->local_only;context.session=scope->session;context.mapping=scope->mapping;context.role=scope->role;context.session_version=scope->version;
    context.peer_size=scope->peers.size;memcpy(context.peers,scope->peers.context,context.peer_size);return context;
}
/* Caller explicitly names at most new utility/ordinary/released members. The
 * request is bound to the context/lifetime in which Core remembered them. */
enum { USS_CORE_CANDIDATE_CAP=3 };
typedef struct UssCoreCandidateNames {
    unsigned count,pair_present;
    uint64_t epoch,context_generation,pair_generation,guard_generation;
    UssPairOwnerContext context;
    UssPairMember members[USS_CORE_CANDIDATE_CAP];
} UssCoreCandidateNames;
typedef struct UssCoreCandidates {
    unsigned valid;
    UssCoreObservation current;
    UssCoreCandidateNames names;
    UssMemberEvidence members[USS_CORE_CANDIDATE_CAP];
} UssCoreCandidates;
static inline int core_candidate_names_valid(const UssPairOwner* owner,const UssCoreCandidateNames* names)
{
    unsigned i,j;unsigned char zero[8]={0};
    if(!owner || !names || !owner->ready || !owner->mission || !owner->context_valid || owner->disabled ||
        !names->count || names->count>USS_CORE_CANDIDATE_CAP || names->pair_present!=owner->pair_present ||
        names->epoch!=owner->epoch || names->context_generation!=owner->context_generation ||
        names->pair_generation!=owner->pair_generation || names->guard_generation!=owner->guard_generation ||
        !pair_owner_context_equal(&names->context,&owner->context))return 0;
    for(i=0;i<names->count;++i){
        if(!publication_id(names->members[i].entity) || names->members[i].entity==owner->context.local.avatar ||
            !memcmp(names->members[i].identity,zero,8))return 0;
        for(j=0;j<i;++j)if(names->members[i].entity==names->members[j].entity)return 0;
    }
    return 1;
}
/* Explicit request creation only; not a scan or pair inference. */
static inline int uss_core_candidate_names(const UssPairOwner* owner,const UssPairMember* members,
    unsigned count,UssCoreCandidateNames* output)
{
    UssCoreCandidateNames names={0};unsigned i;
    if(output)memset(output,0,sizeof(*output));
    if(!owner || !members || !output || count>USS_CORE_CANDIDATE_CAP)return 0;
    names.count=count;names.context=owner->context;names.pair_present=owner->pair_present;
    names.epoch=owner->epoch;names.context_generation=owner->context_generation;
    names.pair_generation=owner->pair_generation;names.guard_generation=owner->guard_generation;
    for(i=0;i<count;++i)names.members[i]=members[i];
    if(!core_candidate_names_valid(owner,&names))return 0;
    *output=names;return 1;
}

/* Fresh per-call budget; the output owns all bytes. No cross-tick observation cache. */
static UssCoreObservation core_observe_extended(const HfApi* api,HfMod* mod,const UssPairOwner* owner,
    const UssCoreFlags* flags,const UssCorePlatform* platform,Reader* diagnostics,
    const UssCoreCandidateNames* requested,UssCoreCandidates* named)
{
    UssCoreObservation result={0},candidate={0};Reader memory={0};
    UssCoreScope before={0},after={0};Inventory inventory={0},final_inventory={0};
    UssPrimaryFields primary_before={0},primary_after={0};
    UssMemberEvidence utility_before={0},ordinary_before={0},utility_after={0},ordinary_after={0};
    uint32_t thread=0,end_thread=0;unsigned char pin[32];unsigned i;
    UssCoreCandidateNames names={0};
    UssMemberEvidence named_before[USS_CORE_CANDIDATE_CAP]={{0}},named_after[USS_CORE_CANDIDATE_CAP]={{0}};
    static const unsigned char action_pin[7]={0x4c,0x8b,0x35,0x4c,0x2e,0x89,0x02};
    memory.api=api;memory.slot=-1;memory.stage="CORE_CONTRACT";
    if(named)memset(named,0,sizeof(*named));
    if(requested){names=*requested;if(!named || !core_candidate_names_valid(owner,&names)){memory.failure="CANDIDATE_NAMES";goto done;}}
    if(owner){result.expected_pair_present=candidate.expected_pair_present=owner->pair_present;
        result.expected_epoch=candidate.expected_epoch=owner->epoch;
        result.expected_context=candidate.expected_context=owner->context_generation;
        result.expected_pair=candidate.expected_pair=owner->pair_generation;
        result.expected_guard=candidate.expected_guard=owner->guard_generation;}
    if(!owner || owner->disabled || !platform || !platform->thread_id || !api || !mod ||
        !uss_sdk_compatible(api) || !api->read ||
        !api->module_base || !api->session || !api->game_state || !api->build_id)goto done;
    {const char* build=api->build_id();if(!build || strcmp(build,"25480438"))goto done;}
    thread=platform->thread_id(platform->user);result.thread=candidate.thread=thread;
    if(!thread || (owner->thread && owner->thread!=thread))goto done;
    if(!core_scope(&memory,api,mod,&before))goto done;
    if(!before.mission) {
        if(!core_scope(&memory,api,mod,&after) || memcmp(&before,&after,sizeof(before)))goto done;
        candidate.valid=1;goto accepted;
    }
    memory.stage="CORE_FLAGS";
    if(!flags || !flags->known)goto done;
    uss_support_lineage_scan_once(api,mod); /* once per session, only after a mission has begun */
    candidate.flags=*flags;candidate.mission=1;
    memory.stage="CORE_INVENTORY";
    if(!inventory_sample(&memory,&inventory))goto done;
    memory.stage="CORE_ACTION_PIN";
    if(!get(&memory,before.game+0xA93ECD,pin,7) || memcmp(pin,action_pin,7))goto done;
    memory.stage="CORE_PRIMARY_BEFORE";
    if(!primary_fields(&memory,before.game,&inventory,&primary_before))goto done;
    memory.stage="CORE_PRIMARY_STATE";
    if(primary_before.game_state!=before.state)goto done;
    candidate.context=core_context(&inventory,&before);
    if(requested && !pair_owner_context_equal(&candidate.context,&names.context)){memory.failure="CANDIDATE_CONTEXT";goto done;}
    candidate.pair_matched=owner->pair_present && owner->context_valid &&
        pair_owner_context_equal(&candidate.context,&owner->context);
    if(candidate.pair_matched || requested){
        memory.stage="CORE_MEMBER_PIN"; /* a refusal here used to carry the stale stage CORE_PRIMARY_STATE */
        if(!get(&memory,before.game+USS_MEMBER_PIN_RVA,pin,USS_MEMBER_PIN_BYTES)){memory.detail="MEMBER_PIN_READ";goto done;}
        if(!uss_member_pin_matches(pin)){memory.detail="MEMBER_PIN_BYTES";goto done;}
    }
    if(candidate.pair_matched) {
        if(!member_fields(&memory,before.game,&inventory,owner->utility.entity,owner->utility.identity,&utility_before) ||
            !member_fields(&memory,before.game,&inventory,owner->ordinary.entity,owner->ordinary.identity,&ordinary_before) ||
            utility_before.classification!=USS_UTILITY_SUPPORT || ordinary_before.classification!=USS_ORDINARY_SUPPORT)goto done;
    }
    if(requested)for(i=0;i<names.count;++i){
        if(candidate.pair_matched && pair_member_equal(&names.members[i],&owner->utility))named_before[i]=utility_before;
        else if(candidate.pair_matched && pair_member_equal(&names.members[i],&owner->ordinary))named_before[i]=ordinary_before;
        else if(!member_fields(&memory,before.game,&inventory,names.members[i].entity,names.members[i].identity,&named_before[i]))goto done;
        if(named_before[i].classification!=uss_classify_support(names.members[i].identity,2)){
            memory.diag_native_kind=named_before[i].native_kind;memory.diag_effective_kind=named_before[i].kind;
            memory.failure="CANDIDATE_CLASS";goto done;}
    }
    memory.stage="CORE_PRIMARY_AFTER";
    if(!primary_fields(&memory,before.game,&inventory,&primary_after))goto done;
    memory.stage="CORE_PRIMARY_CHANGED";
    if(memcmp(&primary_before,&primary_after,sizeof(primary_before)))goto done;
    if(candidate.pair_matched &&
        (!member_fields(&memory,before.game,&inventory,owner->utility.entity,owner->utility.identity,&utility_after) ||
         !member_fields(&memory,before.game,&inventory,owner->ordinary.entity,owner->ordinary.identity,&ordinary_after) ||
         memcmp(&utility_before,&utility_after,sizeof(utility_before)) || memcmp(&ordinary_before,&ordinary_after,sizeof(ordinary_before))))goto done;
    if(requested)for(i=0;i<names.count;++i){
        if(candidate.pair_matched && pair_member_equal(&names.members[i],&owner->utility))named_after[i]=utility_after;
        else if(candidate.pair_matched && pair_member_equal(&names.members[i],&owner->ordinary))named_after[i]=ordinary_after;
        else if(!member_fields(&memory,before.game,&inventory,names.members[i].entity,names.members[i].identity,&named_after[i]))goto done;
        if(memcmp(&named_before[i],&named_after[i],sizeof(named_before[i]))){memory.failure="CANDIDATE_CHANGED";goto done;}
    }
    memory.stage="CORE_INVENTORY_AFTER";
    if(!inventory_sample(&memory,&final_inventory))goto done;
    memory.stage="CORE_INVENTORY_CHANGED";
    if(memcmp(&inventory,&final_inventory,sizeof(inventory)))goto done;
    if(!core_scope(&memory,api,mod,&after))goto done;
    memory.stage="CORE_SCOPE_CHANGED";
    if(memcmp(&before,&after,sizeof(before)))goto done;
    candidate.inventory=inventory;candidate.primary.fields=primary_after;
    primary_predicate(&candidate.primary,&inventory);candidate.primary.valid=1;
    candidate.utility=utility_after;candidate.ordinary=ordinary_after;
    candidate.support_class=uss_classify_support(inventory.identities[2],inventory.observed_kinds[2]);candidate.valid=1;
accepted:
    end_thread=platform->thread_id(platform->user);
    if(requested && (memcmp(requested,&names,sizeof(names)) || !core_candidate_names_valid(owner,&names))){
        memory.failure="CANDIDATE_LIFETIME_OR_NAMES_CHANGED";goto done;}
    if((candidate.mission && memcmp(flags,&candidate.flags,sizeof(candidate.flags))) ||
        end_thread!=thread || owner->pair_present!=candidate.expected_pair_present || owner->epoch!=candidate.expected_epoch ||
        owner->context_generation!=candidate.expected_context || owner->pair_generation!=candidate.expected_pair ||
        owner->guard_generation!=candidate.expected_guard)goto done;
    result=candidate;
    /* Every coherence, re-read and lifetime check above passed: only now may a slot-residency proof become lasting,
     * and the current pair's members are protected from replacement in the proof table. */
    uss_support_proof_commit(&memory);
    if(owner->pair_present)uss_support_proof_protect(owner->utility.identity,owner->ordinary.identity);
    else uss_support_proof_protect(NULL,NULL);
    if(named){named->current=result;if(result.mission){named->names=names;named->valid=1;
        for(i=0;i<names.count;++i)named->members[i]=named_after[i];}}
done:
    if(!result.valid && !memory.failure)memory.failure=memory.stage;
    if(diagnostics)*diagnostics=memory;return result;
}
/* Existing observation contract is unchanged; no extra candidates or reads. */
static UssCoreObservation uss_core_observe(const HfApi* api,HfMod* mod,const UssPairOwner* owner,
    const UssCoreFlags* flags,const UssCorePlatform* platform,Reader* diagnostics)
{
    return core_observe_extended(api,mod,owner,flags,platform,diagnostics,NULL,NULL);
}
static inline UssCoreCandidates uss_core_observe_candidates(const HfApi* api,HfMod* mod,const UssPairOwner* owner,
    const UssCoreFlags* flags,const UssCorePlatform* platform,const UssCoreCandidateNames* names,Reader* diagnostics)
{
    UssCoreCandidates result={0};
    if(!names)return result;
    result.current=core_observe_extended(api,mod,owner,flags,platform,diagnostics,names,&result);
    return result;
}

/* Apply only on the serialized owner thread. This never creates/replaces a pair. */
static int uss_core_apply(UssPairOwner* owner,unsigned thread,const UssCoreObservation* current,
    UssPairObservation* observation,UssPublicationView* view,UssPublicationOwner* row)
{
    UssPairObservation raw={0};
    if(observation)memset(observation,0,sizeof(*observation));
    if(view)memset(view,0,sizeof(*view));if(row)memset(row,0,sizeof(*row));
    if(!owner || !current || !observation || !view || !row ||
        current->expected_pair_present!=owner->pair_present || current->expected_epoch!=owner->epoch || current->expected_context!=owner->context_generation ||
        current->expected_pair!=owner->pair_generation || current->expected_guard!=owner->guard_generation)return 0;
    if(!current->valid || current->thread!=thread) {
        uss_pair_owner_update(owner,thread,0,0,NULL);return 0;
    }
    if(!uss_pair_owner_update(owner,thread,1,current->mission,current->mission?&current->context:NULL))return 0;
    view->valid=1;view->mission=current->mission;
    if(!current->mission || !owner->pair_present || !current->pair_matched)return 1;
    raw.valid=raw.mission=raw.members_valid=1;
    raw.local_only=current->context.local_only;raw.host=current->context.role==HF_SESSION_HOST;raw.joined_client=current->context.role==HF_SESSION_JOINER;
    raw.eligible=current->flags.eligible;raw.formed=current->flags.formed;raw.formation_pending=current->flags.formation_pending;
    raw.disabled=current->flags.disabled;raw.suspended=current->flags.suspended;raw.paused=current->flags.paused;
    raw.down=current->flags.down;raw.input=current->flags.input;
    raw.context=current->context.local;raw.utility=owner->utility;raw.ordinary=owner->ordinary;
    raw.utility_parent=current->utility.parent;raw.ordinary_parent=current->ordinary.parent;
    raw.support=current->inventory.slots[2];raw.active=current->inventory.active_selector;
    raw.can_switch=current->inventory.slots[0]!=0 && current->inventory.slots[1]!=0 && raw.active>=1 && raw.active<=3;
    if(!uss_pair_owner_observation(owner,thread,&raw,observation)){view->valid=0;return 0;}
    row->owner=raw.context.owner;row->inventory=raw.context.inventory;row->address=raw.context.row_address;
    row->avatar=raw.context.avatar;memcpy(row->row,current->inventory.row,48);
    view->pair_present=1;
    if(current->context.local_only)return 1; /* Pair observed; no network publication key. */
    view->key.session=current->context.session;view->key.mapping=current->context.mapping;
    view->key.owner=row->owner;view->key.inventory=row->inventory;view->key.avatar=row->avatar;
    view->key.utility=owner->utility.entity;view->key.ordinary=owner->ordinary.entity;
    memcpy(view->key.utility_identity,owner->utility.identity,8);memcpy(view->key.ordinary_identity,owner->ordinary.identity,8);
    view->key.peer_size=current->context.peer_size;memcpy(view->key.peers,current->context.peers,view->key.peer_size);
    view->supported_pair=uss_pair_owner_publication(owner,thread,current->context.role,row,&view->key);
    return 1;
}
#endif
