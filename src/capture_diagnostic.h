/* One read-only request per process/session. No native target binding. */
#ifndef USS_CAPTURE_DIAGNOSTIC_H
#define USS_CAPTURE_DIAGNOSTIC_H
#include "publication_capture.h"
#define USS_CAPTURE_MAGIC UINT32_C(0x34334648)
#define USS_CAPTURE_VERSION 2u
#define USS_CAPTURE_BUILD 25480438u
typedef struct UssCaptureSession {
    uint32_t magic,version,pid,sdk;
    uint64_t process_started,nonce;
    uint32_t build,reserved;
} UssCaptureSession;
typedef struct UssCaptureRequest {
    UssCaptureSession session;
    uint32_t local_unit,avatar,utility,ordinary;
    unsigned char utility_identity[8],ordinary_identity[8];
    uint32_t role,mode; /* 0 full capture, 1 explicit identity preflight */
    uint64_t issued_ms;
} UssCaptureRequest;
_Static_assert(sizeof(UssCaptureSession)==40,"capture session wire size");
_Static_assert(sizeof(UssCaptureRequest)==88,"capture request wire size");
typedef struct UssCaptureDiagnostic {
    UssCaptureSession session;
    unsigned terminal,identity_attempted,identity_valid;
    UssCaptureRequest identity;
    uint32_t callback_thread;
    uint64_t started_ms,next_poll;
} UssCaptureDiagnostic;
typedef struct UssCaptureDiagnosticServices {
    void* user;
    uint64_t (*clock_ms)(void*);
    uint32_t (*thread_id)(void*);
    /* absent0, exact file1, malformed/I/O -1. No game read on an absent file. */
    int (*read_request)(void*,UssCaptureRequest*);
    int (*write_identity)(void*,const UssCaptureRequest*);
    UssCapturePlatform platform;
} UssCaptureDiagnosticServices;
static void capture_diagnostic_refuse(UssCaptureDiagnostic* control,const HfApi* api,HfMod* mod,const char* reason)
{
    control->terminal=1;
    api->log(mod,"HF034 TERMINAL REFUSED reason=%s native_calls=0 retry=0 scope=CANDIDATE_PAIR core_lifetime_proven=0",reason);
}
static int capture_diagnostic_input(void* user,UssCaptureInput* out)
{*out=*(const UssCaptureInput*)user;return 1;}
/* Resolve only the caller-named support entities; no census or candidate scan. */
static int capture_identity_member(Reader* reader,const Inventory* owner,uintptr_t base,
    uint32_t entity,unsigned char descriptor[24],uint32_t* parent)
{
    uint32_t kind,index;unsigned char header[72];uintptr_t rows;
    if(!weapon(reader,base+0x3326DC0,owner->roots[4],entity,descriptor,&kind) || kind!=2 ||
        !get(reader,owner->roots[4],header,72) ||
        !lookup(reader,owner->roots[4]+0x20,entity,&index) || index>=u32(header+12) || index>=16384)return 0;
    rows=ptr_value(header+64);
    return get(reader,rows+(uintptr_t)index*48+4,parent,4) && *parent==owner->avatar;
}
static int capture_identity_preflight(Reader* reader,const HfApi* api,HfMod* mod,
    const UssCaptureRequest* request,UssCaptureRequest* result)
{
    Inventory before={0},after={0};HfSession start={0},end={0};
    unsigned char first[2][24],second[2][24],zero[8]={0};uint32_t parents[2],again[2];
    uintptr_t base,end_base;size_t size=0,end_size=0;unsigned i;
    uint32_t entities[2]={request->utility,request->ordinary};
    api->session(mod,&start);reader->api=api;
    base=api->module_base("game.dll",&size);
    if(start.role!=request->role || !inventory_sample(reader,&before) ||
        before.avatar!=request->avatar || before.active_selector!=1 ||
        !valid_id(before.slots[0]) || before.held!=before.slots[0] ||
        before.slots[2]!=request->ordinary)return 0;
    for(i=0;i<2;++i)if(!capture_identity_member(reader,&before,base,entities[i],first[i],&parents[i]))return 0;
    if(uss_classify_support(first[0],2)!=USS_UTILITY_SUPPORT ||
        uss_classify_support(first[1],2)!=USS_ORDINARY_SUPPORT || !memcmp(first[1],zero,8))return 0;
    for(i=0;i<2;++i)if(!capture_identity_member(reader,&before,base,entities[i],second[i],&again[i]) ||
        memcmp(first[i],second[i],24) || parents[i]!=again[i])return 0;
    if(!inventory_sample(reader,&after) || memcmp(&before,&after,sizeof(before)))return 0;
    api->session(mod,&end);end_base=api->module_base("game.dll",&end_size);
    if(base!=end_base || size!=end_size || start.role!=end.role || start.version!=end.version ||
        api->game_state(mod)!=HF_STATE_MISSION)return 0;
    *result=*request;result->local_unit=before.local_unit;
    memcpy(result->utility_identity,first[0],8);memcpy(result->ordinary_identity,first[1],8);
    return 1;
}
static void capture_identity_hex(const unsigned char* bytes,char out[17])
{
    static const char hex[]="0123456789abcdef";unsigned i;
    for(i=0;i<8;++i){out[i*2]=hex[bytes[i]>>4];out[i*2+1]=hex[bytes[i]&15];}out[16]=0;
}
static void uss_capture_diagnostic_tick(UssCaptureDiagnostic* control,const HfApi* api,HfMod* mod,
    const UssCaptureDiagnosticServices* services)
{
    UssCaptureRequest request={0};UssCaptureInput input={0};UssCaptureProducer capture_reader={0};
    UssPublicationCapture observation={0};Reader budget={0};HfSession role={0};
    uint64_t now;uint32_t thread;int present;
    if(control->terminal)return;
    thread=services->thread_id(services->user);
    if(!thread || (control->callback_thread && control->callback_thread!=thread)) {
        capture_diagnostic_refuse(control,api,mod,"CALLBACK_THREAD_CHANGED");return;
    }
    control->callback_thread=thread;now=services->clock_ms(services->user);
    if(now<control->started_ms || now-control->started_ms>1800000 || now>UINT64_MAX-250) {
        capture_diagnostic_refuse(control,api,mod,"EXPIRED");return;
    }
    if(now<control->next_poll)return;
    control->next_poll=now+250;
    present=services->read_request(services->user,&request);
    if(!present)return;
    control->terminal=1; /* Consume the requested phase before any game reads. */
    if(present!=1 || memcmp(&request.session,&control->session,sizeof(request.session)) ||
        !valid_id(request.avatar) || !valid_id(request.utility) || !valid_id(request.ordinary) ||
        request.utility==request.ordinary || request.utility==request.avatar || request.ordinary==request.avatar ||
        (request.role!=HF_SESSION_HOST && request.role!=HF_SESSION_JOINER) ||
        request.issued_ms<control->started_ms || now<request.issued_ms || now-request.issued_ms>30000) {
        capture_diagnostic_refuse(control,api,mod,"REQUEST_IDENTITY_OR_TTL");return;
    }
    if(!control->identity_attempted) {
        unsigned char zero[8]={0};char utility_hex[17],ordinary_hex[17];UssCaptureRequest identity={0};
        control->identity_attempted=1;
        if(request.mode!=1 || request.local_unit || memcmp(request.utility_identity,zero,8) ||
            memcmp(request.ordinary_identity,zero,8) || !services->write_identity) {
            capture_diagnostic_refuse(control,api,mod,"IDENTITY_PREFLIGHT_REQUIRED");return;
        }
        if(api->game_state(mod)!=HF_STATE_MISSION ||
            !capture_identity_preflight(&budget,api,mod,&request,&identity)) {
            capture_diagnostic_refuse(control,api,mod,budget.failure?budget.failure:"IDENTITY_PREFLIGHT");return;
        }
        identity.issued_ms=now;
        if(!services->write_identity(services->user,&identity)) {
            capture_diagnostic_refuse(control,api,mod,"IDENTITY_RESULT_IO");return;
        }
        control->identity=identity;control->identity_valid=1;control->terminal=0;
        capture_identity_hex(identity.utility_identity,utility_hex);capture_identity_hex(identity.ordinary_identity,ordinary_hex);
        api->log(mod,"HF034 IDENTITY_READY unit=%u avatar=%u utility=%u ordinary=%u utility_identity=%s ordinary_identity=%s reads=%u bytes=%u capture_run=0 native_calls=0 next=SEPARATE_CAPTURE_CUE",
            identity.local_unit,identity.avatar,identity.utility,identity.ordinary,utility_hex,ordinary_hex,budget.calls,budget.bytes);
        return;
    }
    if(!control->identity_valid || request.mode || request.issued_ms<control->identity.issued_ms || now<control->identity.issued_ms ||
        now-control->identity.issued_ms>120000 || request.local_unit!=control->identity.local_unit ||
        request.avatar!=control->identity.avatar || request.utility!=control->identity.utility ||
        request.ordinary!=control->identity.ordinary || request.role!=control->identity.role ||
        memcmp(request.utility_identity,control->identity.utility_identity,8) ||
        memcmp(request.ordinary_identity,control->identity.ordinary_identity,8)) {
        capture_diagnostic_refuse(control,api,mod,"CAPTURE_IDENTITY_OR_PREFLIGHT_AGE");return;
    }
    if(api->game_state(mod)!=HF_STATE_MISSION) {
        capture_diagnostic_refuse(control,api,mod,"MISSION_REQUIRED");return;
    }
    api->session(mod,&role);
    if(role.role!=request.role) {capture_diagnostic_refuse(control,api,mod,"REQUEST_ROLE_CHANGED");return;}
    budget.api=api;
    if(!inventory_sample(&budget,&input.owner) || input.owner.local_unit!=request.local_unit ||
        input.owner.avatar!=request.avatar || input.owner.active_selector!=1 ||
        input.owner.held!=input.owner.slots[0] || !valid_id(input.owner.slots[0])) {
        capture_diagnostic_refuse(control,api,mod,budget.failure?budget.failure:"REQUEST_OWNER_OR_PRIMARY");return;
    }
    input.pair_generation=control->session.nonce;input.context_generation=control->session.process_started;
    input.callback_thread=thread;input.utility=request.utility;input.ordinary=request.ordinary;
    memcpy(input.utility_identity,request.utility_identity,8);memcpy(input.ordinary_identity,request.ordinary_identity,8);
    capture_reader.api=api;capture_reader.mod=mod;capture_reader.platform=services->platform;
    capture_reader.user=&input;capture_reader.input=capture_diagnostic_input;
    if(!uss_capture_publication(&capture_reader,&observation,&budget)) {
        api->log(mod,"HF034 TERMINAL REFUSED reason=%s stage=%s reads=%u bytes=%u proof_bytes=%zu native_calls=0 retry=0 scope=CANDIDATE_PAIR core_lifetime_proven=0",
            budget.failure?budget.failure:"CAPTURE",budget.stage?budget.stage:"NONE",budget.calls,budget.bytes,budget.evidence_size);
        return;
    }
    api->log(mod,"HF034 TERMINAL OBSERVED role=%u avatar=%u unit=%u primary=%u support=%u utility=%u ordinary=%u wires=%u,%u,%u,%u reads=%u bytes=%u proof_bytes=%zu callback_thread=%u native_calls=0 scope=CANDIDATE_PAIR core_lifetime_proven=0 delivery_proven=0",
        role.role,observation.key.avatar,input.owner.local_unit,input.owner.slots[0],input.owner.slots[2],
        request.utility,request.ordinary,observation.wires[0],observation.wires[1],observation.wires[2],observation.wires[3],
        budget.calls,budget.bytes,observation.fingerprint_size,thread);
}
#endif
