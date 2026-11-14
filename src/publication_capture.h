/* One owned, read-only ordinary publication capture. No sender is bound here. */
#ifndef USS_PUBLICATION_CAPTURE_H
#define USS_PUBLICATION_CAPTURE_H
#include "capture_dispatch.h"
#include "weapon_policy.h"
#include "managed_draw_readiness.h"
typedef struct UssCaptureInput {
    uint64_t pair_generation,context_generation;
    Inventory owner;
    uint32_t utility,ordinary,callback_thread;
    unsigned char utility_identity[8],ordinary_identity[8];
    unsigned replacement_queue_checks;
} UssCaptureInput;
typedef struct UssCaptureProducer {
    const HfApi* api;HfMod* mod;
    UssCapturePlatform platform;
    void* user;
    /* Reads caller-owned Core state; not game memory or a permission boolean.
     * Both generations must change on their actual respective lifetime changes. */
    int (*input)(void*,UssCaptureInput*);
} UssCaptureProducer;
typedef struct UssCapturePeers {
    uint32_t count;unsigned is_host;
    unsigned char self[8],host[8],sorted[32],context[48];size_t size;
} UssCapturePeers;
static int capture_peers(UssCaptureReader* reader,uintptr_t session,UssCapturePeers* out)
{
    unsigned char raw[32],zero[8]={0};unsigned i,j,self_seen=0,host_seen=0;
    memset(out,0,sizeof(*out));
    if(!capture_bytes(reader,session+0x162D8,&out->count,4,0) || out->count<1 || out->count>4 ||
        !capture_bytes(reader,session+0xB398,out->self,8,0) || !memcmp(out->self,zero,8) ||
        !capture_bytes(reader,session+0xB3A8,out->host,8,0) ||
        !capture_bytes(reader,session+0x162E0,raw,out->count*8,0)) return 0;
    for(i=0;i<out->count;++i) {
        if(!memcmp(raw+i*8,zero,8))return 0;
        for(j=0;j<i;++j)if(!memcmp(raw+i*8,raw+j*8,8))return 0;
        if(!memcmp(raw+i*8,out->self,8))self_seen=1;
        if(!memcmp(raw+i*8,out->host,8))host_seen=1;
    }
    if(!self_seen || (memcmp(out->host,zero,8) && !host_seen))return 0;
    memcpy(out->sorted,raw,out->count*8);
    for(i=0;i<out->count;++i)for(j=i+1;j<out->count;++j)
        if(memcmp(out->sorted+i*8,out->sorted+j*8,8)>0) {
            unsigned char swap[8];memcpy(swap,out->sorted+i*8,8);
            memcpy(out->sorted+i*8,out->sorted+j*8,8);memcpy(out->sorted+j*8,swap,8);
        }
    memcpy(out->context,out->self,8);memcpy(out->context+8,out->host,8);
    memcpy(out->context+16,out->sorted,out->count*8);out->size=16+out->count*8;
    out->is_host=!memcmp(out->self,out->host,8) || !memcmp(out->host,zero,8);return 1;
}
/* Three-way lookup: only a proven empty bucket/table is absence. */
static int capture_lookup(UssCaptureReader* reader,uintptr_t at,uint32_t key,uint32_t* index)
{
    unsigned char header[20],row[8];uint32_t cap,empty,mult,i;uintptr_t data;
    if(!capture_bytes(reader,at,header,20,0))return -1;
    cap=u32(header+8);if(!cap)return 0;
    data=ptr_value(header);empty=u32(header+12);mult=u32(header+16);
    if(cap>1048576 || (cap&(cap-1)) || data%4 || !address_ok(data,(size_t)cap*8))return -1;
    for(i=0;i<cap && i<32;++i) {
        if(!capture_bytes(reader,data+(uintptr_t)((key*mult+i)&(cap-1))*8,row,8,0))return -1;
        if(u32(row)==key) {
            *index=u32(row+4);if(*index==UINT32_MAX)return 0;
            return *index<=16383?1:-1;
        }
        if(u32(row)==empty)return 0;
    }
    return -1;
}
static int capture_mapping(UssCaptureReader* reader,uintptr_t mapping,uint32_t entity,
                            unsigned char descriptor[24],uint32_t* wire)
{
    uint32_t forward,reverse;unsigned char again[24];
    return publication_id(entity) && capture_lookup(reader,mapping+0xF1AEB0,entity,&forward)==1 &&
        forward<=1048576 && capture_bytes(reader,mapping+0xF32F18+(uintptr_t)forward*24,descriptor,24,0) &&
        u32(descriptor+8)==entity && (*wire=u32(descriptor+16))<=32766 &&
        capture_lookup(reader,mapping+0xF22EC8,*wire,&reverse)==1 && reverse<=1048576 &&
        capture_bytes(reader,mapping+0xF32F18+(uintptr_t)reverse*24,again,24,0) && !memcmp(descriptor,again,24);
}
typedef struct UssLoneCaptureContext {uint32_t target,outgoing;} UssLoneCaptureContext;
/* Only the explicit lone entry opts into survivor/outgoing validation. Ordinary
 * publication retains all three role/parent requirements. */
static int capture_members_internal(UssCaptureReader* reader,const UssCaptureInput* input,
                            const Inventory* owner,uintptr_t mapping,uint32_t wires[4],const UssLoneCaptureContext* lone)
{
    uintptr_t registry,component=0,relationships,units,unit_pointers,unit_rows;
    unsigned char header[72],descriptor[24],unit_descriptor[24],row[20];
    uint32_t count,i,roles[3]={owner->slots[0],input->utility,input->ordinary},sentinel,role_count=lone?2u:3u;
    if(lone)roles[1]=lone->target;
    if(!capture_pointer(reader,reader->game+0x3326E68,&registry,0) ||
        !capture_bytes(reader,registry+0x6C0,&count,4,0) || count>32) return 0;
    for(i=0;i<count;++i) {
        uintptr_t at=registry+0x6C8+(uintptr_t)i*16;uint32_t kind;
        if(!capture_bytes(reader,at+8,&kind,4,0))return 0;
        if(kind==141) {if(component || !capture_pointer(reader,at,&component,0))return 0;}
    }
    if(!component || !capture_pointer(reader,reader->game+0x3326698,&relationships,0) ||
        !capture_pointer(reader,reader->game+0x3326DC0,&units,0) || units!=owner->roots[4] ||
        !capture_bytes(reader,units,header,72,0))return 0;
    count=u32(header+12);unit_pointers=ptr_value(header+56);unit_rows=ptr_value(header+64);
    if(!count || count>16384 || count>u32(header) || unit_pointers%8 || unit_rows%8 ||
        !address_ok(unit_pointers,(size_t)count*8) ||
        !address_ok(unit_rows,(size_t)count*48))return 0;
    if(lone){
        uint32_t outgoing_index=0;int found=capture_lookup(reader,units+0x20,lone->outgoing,&outgoing_index);
        const unsigned char* identity=lone->outgoing==input->utility?input->utility_identity:input->ordinary_identity;
        uintptr_t unit;
        if(found<0 || reader->memory.failure)return 0;
        if(found && (outgoing_index>=count || !capture_pointer(reader,unit_pointers+(uintptr_t)outgoing_index*8,&unit,0) ||
            !capture_bytes(reader,unit,unit_descriptor,24,0) || u32(unit_descriptor+8)!=lone->outgoing || memcmp(unit_descriptor,identity,8) ||
            !capture_bytes(reader,unit_rows+(uintptr_t)outgoing_index*48,row,20,0) || u32(row+4)!=0))return 0;
    }
    for(i=0;i<role_count;++i) {
        uint32_t index,relation,active,absent;uintptr_t unit;unsigned j;
        if(!publication_id(roles[i]) || roles[i]==owner->avatar)return 0;
        for(j=0;j<i;++j)if(roles[i]==roles[j])return 0;
        if(!capture_mapping(reader,mapping,roles[i],descriptor,&wires[i+1]) ||
            capture_lookup(reader,units+0x20,roles[i],&index)!=1 || index>=count ||
            !capture_pointer(reader,unit_pointers+(uintptr_t)index*8,&unit,0) ||
            !capture_bytes(reader,unit,unit_descriptor,24,0) ||
            !capture_bytes(reader,unit_rows+(uintptr_t)index*48,row,20,0) ||
            memcmp(descriptor,unit_descriptor,8) || u32(unit_descriptor+8)!=roles[i] ||
            u32(row+4)!=owner->avatar || !(descriptor[20]&1))return 0;
        if(i && (weapon_support_kind(&reader->memory,unit_descriptor,u32(row+12))!=2 || reader->memory.failure || memcmp(descriptor,roles[i]==input->utility?input->utility_identity:input->ordinary_identity,8)))return 0;
        /* Missing component is required. A read/limit failure is never absence. */
        if(capture_lookup(reader,component+0x20,roles[i],&absent)!=0 || reader->memory.failure ||
            capture_lookup(reader,relationships+0x18,roles[i],&relation)!=1 || relation>1048576 ||
            !capture_pointer(reader,relationships+0x38,&unit,0) ||
            !capture_bytes(reader,unit+(uintptr_t)relation*172,&active,4,0) || !active)return 0;
    }
    if(!capture_bytes(reader,reader->game+0x348456C,&sentinel,4,0) ||
        !capture_bytes(reader,reader->game+0x2154544,&count,4,0) || count!=32767)return 0;
    {const unsigned offsets[]={0,4,8,12,24};uint32_t seen[5];unsigned payload_count=0;
        for(i=0;i<5;++i) {
            uint32_t entity=u32(owner->row+offsets[i]),wire;unsigned j;
            if(entity==sentinel){if(i==0 || i==2)return 0;continue;}
            if(!publication_id(entity) || entity==owner->avatar || (lone && entity==lone->outgoing))return 0;
            for(j=0;j<payload_count;++j)if(seen[j]==entity)return 0;
            seen[payload_count++]=entity;
            if(!capture_mapping(reader,mapping,entity,descriptor,&wire))return 0;
        }
    }
    return 1;
}
static inline int capture_members(UssCaptureReader* reader,const UssCaptureInput* input,
    const Inventory* owner,uintptr_t mapping,uint32_t wires[4])
{return capture_members_internal(reader,input,owner,mapping,wires,NULL);}
static int capture_input_equal(const UssCaptureInput* a,const UssCaptureInput* b)
{
    return a->pair_generation==b->pair_generation && a->context_generation==b->context_generation &&
        !memcmp(&a->owner,&b->owner,sizeof(a->owner)) && a->utility==b->utility && a->ordinary==b->ordinary &&
        a->callback_thread==b->callback_thread && a->replacement_queue_checks==b->replacement_queue_checks &&
        !memcmp(a->utility_identity,b->utility_identity,8) && !memcmp(a->ordinary_identity,b->ordinary_identity,8);
}
static int capture_publication_internal(const UssCaptureProducer* producer,UssPublicationCapture* output,Reader* budget,const UssLoneCaptureContext* lone,const UssPairTransaction* draw,uint64_t draw_now)
{
    UssCaptureReader reader={0};UssPublicationCapture result={0};
    UssCaptureInput input={0},after_input={0};Inventory owner,after;
    UssPrimaryReadiness readiness={0};UssCapturePeers peers,after_peers;
    HfSession start={0},end={0};uintptr_t state,mode,session,mapping,end_base;size_t end_size;
    unsigned char descriptor[24];uint32_t value;unsigned i;int ok=0;
    const HfApi* api=producer?producer->api:NULL;const char* build;UssLoneCaptureContext lone_copy={0};
    UssPairTransaction draw_copy={0};
    if(lone)lone_copy=*lone;
    if(draw)draw_copy=*draw;
    if(budget){reader.memory.calls=budget->calls;reader.memory.bytes=budget->bytes;}
    reader.memory.api=api;reader.memory.slot=-1;reader.memory.stage="CAPTURE_CONTRACT";
    if(!output)goto done;
    memset(output,0,sizeof(*output));
    reader.memory.evidence=result.fingerprint;reader.memory.evidence_capacity=sizeof(result.fingerprint);
    if(reader.memory.calls>2165 || reader.memory.bytes>20000 || !producer || !producer->mod ||
        !api || !uss_sdk_compatible(api) || !api->read || !api->module_base ||
        !api->build_id || !api->session || !producer->input || !producer->platform.query ||
        !producer->platform.path || !producer->platform.export_address || !producer->platform.thread_id)goto done;
    reader.platform=&producer->platform;
    build=api->build_id();
    if(!build || strcmp(build,"25480438") || !producer->input(producer->user,&input) ||
        !input.pair_generation || !input.context_generation || !input.callback_thread || input.replacement_queue_checks ||
        uss_classify_support(input.utility_identity,2)!=USS_UTILITY_SUPPORT ||
        uss_classify_support(input.ordinary_identity,2)!=USS_ORDINARY_SUPPORT)goto done;
    if(lone && (!publication_id(lone->target) || lone->target==lone->outgoing ||
        (lone->target!=input.utility && lone->target!=input.ordinary) ||
        (lone->outgoing!=input.utility && lone->outgoing!=input.ordinary)))goto done;
    api->session(producer->mod,&start);
    if(!reader_evidence(&reader.memory,0,&start,8))goto done;
    if(start.role!=HF_SESSION_HOST && start.role!=HF_SESSION_JOINER)goto done;
    reader.game=api->module_base("game.dll",&reader.game_size);
    reader.memory.stage="CAPTURE_SCOPE";
    if(!reader.game || reader.game_size<0x3484570 || reader.game_size>0x20000000 ||
        !address_ok(reader.game,reader.game_size) ||
        !capture_pointer(&reader,reader.game+0x3326340,&state,0) ||
        !capture_pointer(&reader,reader.game+0x33266A0,&mode,0) ||
        !capture_pointer(&reader,reader.game+0x347CEF0,&session,0) ||
        !capture_bytes(&reader,state+0xAC21C,&value,4,0) || value!=4 ||
        !capture_bytes(&reader,mode+8,&value,4,0) || value!=1 ||
        !capture_peers(&reader,session,&peers) || peers.count<2 ||
        peers.is_host!=(unsigned)(start.role==HF_SESSION_HOST) ||
        !inventory_sample(&reader.memory,&owner) || memcmp(&owner,&input.owner,sizeof(owner)))goto done;
    if((lone?owner.slots[2]!=lone->target:(owner.slots[2]!=input.utility && owner.slots[2]!=input.ordinary)) ||
        !primary_fields(&reader.memory,reader.game,&owner,&readiness.fields))goto done;
    primary_predicate(&readiness,&owner);
    readiness.valid=1;
    if(readiness.fields.game_state!=state ||
        (draw?!uss_managed_draw_primary(&draw_copy,&owner,&readiness,draw_now):!readiness.ready))goto done;
    if(draw && (lone || draw_copy.origin.pair_generation!=input.pair_generation ||
        draw_copy.origin.utility.entity!=input.utility))goto done;
    if(draw && (draw_copy.origin.ordinary.entity!=input.ordinary ||
        memcmp(draw_copy.origin.utility.identity,input.utility_identity,8) ||
        memcmp(draw_copy.origin.ordinary.identity,input.ordinary_identity,8) ||
        !reader_evidence(&reader.memory,0,&draw_copy,sizeof(draw_copy))))goto done;
    /* Existing action and GameState roots are pinned independently of dispatch. */
    {static const unsigned char action[]={0x4c,0x8b,0x35,0x4c,0x2e,0x89,0x02};
     static const unsigned char state_load[]={0x48,0x8b,0x0d,0x15,0x23,0x1e,0x03};
     if(!capture_pin(&reader,reader.game+0xA93ECD,action,7) ||
        !capture_pin(&reader,reader.game+0x144024,state_load,7))goto done;}
    if(!capture_dispatch(&reader,session))goto done;
    reader.memory.stage="CAPTURE_MEMBERS";
    if(!capture_pointer(&reader,reader.game+0x346BF98,&mapping,0) ||
        !capture_mapping(&reader,mapping,owner.avatar,descriptor,&result.wires[0]) ||
        !capture_members_internal(&reader,&input,&owner,mapping,result.wires,lone))goto done;
    for(i=0;i<sizeof(capture_pins)/sizeof(capture_pins[0]);++i) {
        const UssCapturePin* pin=&capture_pins[i];
        if(!pin->exe && !capture_pin(&reader,reader.game+pin->rva,pin->bytes,pin->size))goto done;
    }
    reader.memory.stage="CAPTURE_FINAL";
    if(!inventory_sample(&reader.memory,&after) || memcmp(&owner,&after,sizeof(owner)) ||
        !capture_peers(&reader,session,&after_peers) || peers.size!=after_peers.size ||
        memcmp(peers.context,after_peers.context,peers.size) ||
        !capture_pointer(&reader,reader.game+0x347CEF0,&end_base,0) || end_base!=session ||
        !producer->input(producer->user,&after_input) || !capture_input_equal(&input,&after_input))goto done;
    api->session(producer->mod,&end);
    end_base=api->module_base("game.dll",&end_size);
    result.thread=producer->platform.thread_id(producer->platform.user);
    if(start.role!=end.role || start.version!=end.version || end_base!=reader.game ||
        end_size!=reader.game_size || result.thread!=input.callback_thread || (lone && memcmp(lone,&lone_copy,sizeof(lone_copy))) ||
        (draw && memcmp(draw,&draw_copy,sizeof(draw_copy))))goto done;
    result.key.pair_generation=input.pair_generation;result.key.context_generation=input.context_generation;
    result.key.session=session;result.key.mapping=mapping;result.key.owner=owner.roots[0];
    result.key.inventory=owner.roots[2];result.key.avatar=owner.avatar;
    result.key.utility=input.utility;result.key.ordinary=input.ordinary;
    memcpy(result.key.utility_identity,input.utility_identity,8);memcpy(result.key.ordinary_identity,input.ordinary_identity,8);
    result.key.peer_size=peers.size;memcpy(result.key.peers,peers.context,peers.size);
    result.owner.owner=owner.roots[0];result.owner.inventory=owner.roots[2];
    result.owner.address=owner.row_address;result.owner.avatar=owner.avatar;memcpy(result.owner.row,owner.row,48);
    result.fingerprint_size=reader.memory.evidence_size;result.valid=1;*output=result;ok=1;
done:
    if(!ok && !reader.memory.failure)reader.memory.failure=reader.memory.stage;
    reader.memory.evidence=NULL;reader.memory.evidence_capacity=0;
    if(budget)*budget=reader.memory;
    return ok;
}
static inline int uss_capture_publication(const UssCaptureProducer* producer,UssPublicationCapture* output,Reader* budget)
{return capture_publication_internal(producer,output,budget,NULL,NULL,0);}
/* Only the managed-draw owner calls this after verifying its controller input,
 * published pair receipt and deadline. All ordinary capture dependencies stay. */
static inline int uss_capture_managed_draw_publication(const UssCaptureProducer* producer,
    const UssPairTransaction* draw,uint64_t now,UssPublicationCapture* output,Reader* budget)
{
    if(!draw){if(output)memset(output,0,sizeof(*output));return 0;}
    return capture_publication_internal(producer,output,budget,NULL,draw,now);
}
static inline int uss_capture_lone_publication(const UssCaptureProducer* producer,const UssLoneCaptureContext* lone,
    UssPublicationCapture* output,Reader* budget)
{if(!lone){if(output)memset(output,0,sizeof(*output));return 0;}return capture_publication_internal(producer,output,budget,lone,NULL,0);}
#endif
