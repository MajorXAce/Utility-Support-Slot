#define USS_REPLACEMENT_OBSERVATION_TEST_MAIN hf050_observation_regression_checks
#include "replacement_observation_harness.c"
#include "../src/replacement_adapter.h"
static const unsigned char fixture_select_state[64]={0x4c,0x8b,0xdc,0x55,0x57,0x48,0x8b,0xec,0x48,0x83,0xec,0x78,0x48,0x8b,0x05,0x6d,0x81,0xba,0x01,0x48,0x33,0xc4,0x48,0x89,0x45,0xe0,0x8b,0x01,0x48,0x8b,0xf9,0x49,0x89,0x5b,0x10,0x33,0xdb,0x3b,0x05,0x65,0xfd,0x9e,0x02,0x49,0x89,0x73,0x18,0x8b,0xf2,0x4d,0x89,0x63,0x20,0x4d,0x89,0x6b,0xe8,0x4d,0x89,0x73,0xe0,0x4c,0x8b,0x35};
static const unsigned char fixture_select_weapon[64]={0x48,0x8b,0xc4,0x44,0x89,0x40,0x18,0x53,0x55,0x41,0x55,0x48,0x81,0xec,0x80,0x00,0x00,0x00,0x3b,0x15,0xb8,0x91,0xad,0x02,0x41,0x8b,0xe8,0x8b,0xda,0x4c,0x8b,0xe9,0x0f,0x84,0x94,0x06,0x00,0x00,0x44,0x8b,0x49,0x30,0x44,0x8b,0x51,0x38,0x48,0x89,0x70,0x08,0x4c,0x89,0x70,0xd0,0x45,0x33,0xf6,0x44,0x0f,0xaf,0xd3,0x41,0x8d,0x71};
static UssReplacementAdapter adapter;
static unsigned effect_calls,effect_kind[40],typed_calls,fault_effect,fault_mode;
static uint64_t effect_generation[40];
static unsigned publication_drift;
static int adapter_read(uintptr_t address,void* out,size_t n)
{
    if(publication_drift && adapter.pub_busy && adapter.pub_captures==1){fixture(0xA6000000,40)[0]^=1;publication_drift=0;}
    return fake_read(address,out,n);
}
static void own_select_state(void* record,uint32_t slot,uint32_t zero)
{assert((uintptr_t)record==ACTION_MANAGER+0x53DCD0 && slot>=1 && slot<=3 && !zero);++typed_calls;}
static void own_select_weapon(void* inventory,uint32_t avatar,uint32_t slot)
{assert((uintptr_t)inventory==candidate_life.owner.context.local.inventory && avatar==22);++typed_calls;set_selected(slot);}
static void own_attach(void* inventory,uint32_t avatar,uint32_t entity,uint32_t slot)
{UssPairMember member=entity==104?replacement_work.intent.incoming:candidate_id(entity);
 assert((uintptr_t)inventory==candidate_life.owner.context.local.inventory && avatar==22 && slot==3);++typed_calls;attach_fixture(&member);}
static void own_drop(void* inventory,uint32_t avatar,uint32_t slot,uint32_t one)
{assert((uintptr_t)inventory==candidate_life.owner.context.local.inventory && avatar==22 && slot==3 && one==1);++typed_calls;
 put32(fixture(0x70000300,48)+8,UINT32_MAX);put32(fixture(0x70000700,240)+(replacement_work.intent.released.entity==103?96:48)+4,0);}
static void own_send(int64_t target,uint32_t avatar,uint32_t primary,uint32_t secondary,uint32_t support,
    uint32_t backpack,uint32_t field24,uint64_t metadata)
{uint64_t actual;memcpy(&actual,fixture(0x70000300,48)+40,8);
 assert(target==-2 && avatar==22 && primary==101 && secondary==105 && (support==102 || support==103 || support==104));
 assert(backpack==UINT32_MAX && field24==UINT32_MAX && metadata==actual);++typed_calls;}
static int own_invoke(void* user,UssReplacementEffect effect,uintptr_t first,uintptr_t second,uintptr_t record,
    uintptr_t inventory,uint32_t avatar,uint32_t value,const UssInventoryMessage* message)
{
    UssNativeCalls calls={0};UssCallResult result=USS_CALL_NOT_ENTERED;unsigned mode;(void)user;
    assert(effect_calls<40);effect_generation[effect_calls]=candidate_life.owner.pair_generation;effect_kind[effect_calls++]=(unsigned)effect;mode=effect_calls==fault_effect?fault_mode:1;
    if(mode==0 || mode==3)return mode==3;
    if(effect==USS_REPLACEMENT_SELECT){assert(first==BASE+0xA93E90 && second==BASE+0x9AAA50);
        calls.select_state=own_select_state;calls.select_weapon=own_select_weapon;
        result=uss_select_slot(&calls,(void*)record,(void*)inventory,avatar,value);}
    else if(effect==USS_REPLACEMENT_ATTACH){assert(first==BASE+0x9AA290);calls.attach=own_attach;
        result=uss_attach_support(&calls,(void*)inventory,avatar,value);}
    else if(effect==USS_REPLACEMENT_DROP){assert(first==BASE+0x9AF090);calls.drop_slot=own_drop;
        result=uss_drop_support_slot(&calls,(void*)inventory,avatar);}
    else {assert(effect==USS_REPLACEMENT_SEND && first==BASE+0xBE3F40 && message);calls.send_inventory=own_send;result=uss_send_inventory(&calls,message);}
    if(mode==4)fail_at=reads+1;
    if(mode==5)++hf048_history.last_serial;
    return result==USS_CALL_RETURNED && mode!=2;
}
static void adapter_fixture(unsigned utility,unsigned pickup,unsigned selected,unsigned role)
{
    observation_fixture(utility,pickup,selected,role);control_dependencies();fake.read=adapter_read;publication_drift=0;
    memcpy(control_segment(BASE+0xA93E90,64),fixture_select_state,64);
    memcpy(control_segment(BASE+0x9AAA50,64),fixture_select_weapon,64);
    memcpy(control_segment(BASE+0x9AA290,64),uss_attachment_entry_pin,64);
    assert(uss_replacement_adapter_init(&adapter,&observation_producer,&candidate_life,&replacement_publication,own_invoke,NULL));
    {uint64_t metadata=UINT64_C(0xfedcba9876543210);memcpy(fixture(0x70000300,48)+40,&metadata,8);}
    replacement_services=uss_replacement_adapter_services(&adapter);effect_calls=typed_calls=fault_effect=fault_mode=0;
    memset(effect_kind,0,sizeof(effect_kind));observation_mutate=observation_clock_calls=0;
}
static UssReplacementPhase adapter_complete(void)
{
    unsigned i;for(i=0;i<40 && replacement_work.phase==USS_REPLACE_ACTIVE;++i){replacement_now+=25;tick_replacement(replacement_work.has_tick?replacement_work.last_tick+1:1);}
    if(replacement_work.phase!=USS_REPLACE_COMPLETE && !fault_effect)fprintf(stderr,"adapter failure stage=%u phase=%u effects=%u reason=%s core=%s\n",
        replacement_work.stage,replacement_work.phase,effect_calls,adapter.reason?adapter.reason:"none",replacement_work.reason);
    return replacement_work.phase;
}
#ifndef USS_REPLACEMENT_ADAPTER_TEST_MAIN
#define USS_REPLACEMENT_ADAPTER_TEST_MAIN main
#endif
int USS_REPLACEMENT_ADAPTER_TEST_MAIN(void)
{
    unsigned utility,pickup,selected,role,total,i,mode,baseline_kinds[40];
    assert(hf050_observation_regression_checks()==0);
    adapter_fixture(1,0,1,HF_SESSION_HOST);assert(adapter_complete()==USS_REPLACE_COMPLETE);
    assert(replacement_work.real==102 && candidate_life.phase==USS_LIFE_FORMED);
    total=effect_calls;memcpy(baseline_kinds,effect_kind,sizeof(effect_kind));
    printf("adapter baseline effects=%u typed_calls=%u\n",effect_calls,typed_calls);
    for(utility=0;utility<2;++utility)for(pickup=0;pickup<2;++pickup)for(selected=1;selected<=3;++selected)
    for(role=HF_SESSION_HOST;role<=HF_SESSION_JOINER;++role){adapter_fixture(utility,pickup,selected,role);
        assert(adapter_complete()==USS_REPLACE_COMPLETE && !adapter.failed);
        assert(replacement_work.real==(pickup?104u:(utility?102u:103u)));
        assert(candidate_life.owner.pair_generation==replacement_work.new_generation && replacement_work.new_generation);
    }
    for(i=1;i<=total;++i)for(mode=0;mode<=5;++mode){if(mode==1 || (mode==3 && baseline_kinds[i-1]==USS_REPLACEMENT_SEND))continue;
        adapter_fixture(1,0,1,HF_SESSION_HOST);fault_effect=i;fault_mode=mode;
        assert(adapter_complete()==USS_REPLACE_CANCELLED && adapter.failed && effect_calls==i);
        assert(candidate_life.owner.pair_generation==effect_generation[i-1]);
        {unsigned before=effect_calls;replacement_now+=25;tick_replacement(99);assert(effect_calls==before);}
    }
    /* Exercise actual admission through adapter.observe, not just an active work. */
    adapter_fixture(1,1,2,HF_SESSION_HOST);
    {UssCorePlatform p={0};UssCoreFlags flags;UssCoreObservation current;UssPairObservation pair;UssPublicationView view;UssPublicationOwner row;
        candidate_life.phase=USS_LIFE_FORMED;memset(&candidate_life.plan,0,sizeof(candidate_life.plan));memset(&replacement_work,0,sizeof(replacement_work));
        flags=uss_lifecycle_flags(&candidate_life,&core_flags);p.thread_id=core_thread;
        current=uss_core_observe(&fake,MOD,&candidate_life.owner,&flags,&p,NULL);
        assert(uss_lifecycle_observe(&candidate_life,7,&current,&pair,&view,&row));
        view.key.context_generation=candidate_life.owner.context_generation;view.key.pair_generation=candidate_life.owner.pair_generation;
        replacement_publication.record=view.key;replacement_publication.record_present=replacement_publication.formed=1;
        replacement_publication.member=replacement_origin.real;
        assert(begin_replacement() && adapter.anchor_valid && !effect_calls);
        assert(adapter_complete()==USS_REPLACE_COMPLETE);
    }
    /* Readiness WAIT has no native entry; resuming performs each effect once. */
    adapter_fixture(1,0,1,HF_SESSION_HOST);advance_to(USS_REPLACE_RECOVER);
    put32(fixture(ACTION_MANAGER+0x53DCD0,32)+16,2);
    replacement_now+=25;tick_replacement(replacement_work.last_tick+1);assert(replacement_work.phase==USS_REPLACE_ACTIVE && !effect_calls);
    put32(fixture(ACTION_MANAGER+0x53DCD0,32)+16,0);set_selected(1);assert(adapter_complete()==USS_REPLACE_COMPLETE);
    /* A retained publication WAIT keeps its consumed attachment. */
    adapter_fixture(1,0,3,HF_SESSION_HOST);advance_to(USS_REPLACE_RETAINED_TICK);
    replacement_publication.used=64;replacement_publication.rate_until=10000;
    replacement_now+=25;tick_replacement(replacement_work.last_tick+1);
    assert(replacement_work.phase==USS_REPLACE_ACTIVE && replacement_work.retained.attachment_consumed);
    {unsigned before=effect_calls;replacement_publication.used=0;assert(adapter_complete()==USS_REPLACE_COMPLETE);
        assert(effect_calls==before+2); /* send + final select, no second attach */}
    adapter_fixture(1,1,2,HF_SESSION_HOST);fixture(BASE+0xA93E90,64)[0]^=1;
    assert(adapter_complete()==USS_REPLACE_CANCELLED && !effect_calls);
    adapter_fixture(1,0,1,HF_SESSION_HOST);fixture(BASE+0x9AA290,64)[0]^=1;
    assert(adapter_complete()==USS_REPLACE_CANCELLED && !effect_calls);
    /* Incoming anchor survives legal native-wire changes as metadata, not proof. */
    adapter_fixture(1,0,1,HF_SESSION_HOST);advance_to(USS_REPLACE_RECOVER);++adapter.wires[2];
    assert(adapter_complete()==USS_REPLACE_CANCELLED && !effect_calls);
    adapter_fixture(1,1,3,HF_SESSION_HOST);advance_to(USS_REPLACE_RECOVER);
    {unsigned char* row=fixture(ACTION_MANAGER+0x53DCD0,32);
        put32(row+8,3);put32(row+16,2);put32(row+20,1);put32(row+24,0);put32(row+28,0x3F800000);
        put32(fixture(ACTION_MANAGER+0x53E880,24)+12,0x8000);}
    assert(adapter_complete()==USS_REPLACE_COMPLETE); /* Scoped phase2 through native doubles. */
    adapter_fixture(1,0,3,HF_SESSION_HOST);advance_to(USS_REPLACE_RETAINED_TICK);
    replacement_now=replacement_work.primary_deadline+25;
    assert(replacement_now<replacement_work.deadline_ms && adapter_complete()==USS_REPLACE_COMPLETE);
    adapter_fixture(1,0,1,HF_SESSION_HOST);publication_drift=1;
    assert(adapter_complete()==USS_REPLACE_CANCELLED && effect_calls==1 && effect_kind[0]==USS_REPLACEMENT_ATTACH);
    assert(!replacement_drops && candidate_life.owner.pair_generation==observed_origin.origin.pair_generation);
    puts("replacement adapter: real admission, 24variants, typed faults, phase2/child deadlines, retained WAIT, full publisher trace drift and exact entry/role anchors PASS");
    return 0;
}
