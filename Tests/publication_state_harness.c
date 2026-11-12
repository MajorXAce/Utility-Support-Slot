#ifdef NDEBUG
#error Publication state checks require active assertions
#endif
#include <assert.h>
#include <stdio.h>
#include "../src/publication_state.h"
typedef struct World {
    UssPublicationState state;
    UssPublicationServices services;
    UssPublicationView view;
    UssPublicationCapture snapshot;
    uint64_t now;
    unsigned captures,sends,views,fail_capture,change_entry,reenter,reenter_clock;
    UssPublicationSend send_result;
    char trace[2048];size_t trace_size;
} World;
static void put32(void* bytes,uint32_t value){memcpy(bytes,&value,4);}
static void trace(World* world,char value)
{assert(world->trace_size+1<sizeof(world->trace));world->trace[world->trace_size++]=value;world->trace[world->trace_size]=0;}
static uint64_t clock_ms(void* user)
{
    World* world=user;
    if(world->reenter_clock) {
        UssPublicationResult nested;
        world->reenter_clock=0;
        nested=uss_publication_poll(&world->state,&world->services,&world->snapshot.owner);
        assert(nested.reason==USS_PUB_REENTRANT);
    }
    return world->now;
}
static UssPublicationView view(void* user)
{World* world=user;++world->views;return world->view;}
static int capture(void* user,UssPublicationStage stage,UssPublicationCapture* output)
{
    World* world=user;++world->captures;
    trace(world,stage==USS_PUBLICATION_ENTRY_FORMATION ? 'f' :
        stage==USS_PUBLICATION_ENTRY_INCOMING ? 'i' : stage==USS_PUBLICATION_UTILITY_FIRST ? 'U' :
        stage==USS_PUBLICATION_PRE_ATTACH ? 'F' : stage==USS_PUBLICATION_POLL ? 'P' : 'I');
    if(world->captures==world->fail_capture) return 0;
    *output=world->snapshot;
    if(stage==USS_PUBLICATION_ENTRY_FORMATION || stage==USS_PUBLICATION_ENTRY_INCOMING) {
        switch(world->change_entry) {
        case 1: output->fingerprint[0]^=1;break;
        case 2: ++output->key.pair_generation;break;
        case 3: ++output->key.context_generation;break;
        case 4: ++output->key.session;break;
        case 5: ++output->key.mapping;break;
        case 6: output->key.peers[0]^=1;break;
        case 7: output->owner.row[40]^=1;break;
        case 8: ++output->owner.address;break;
        case 9: ++output->thread;break;
        case 10: ++output->wires[0];break;
        case 11: ++output->fingerprint_size;break;
        case 12: ++output->key.owner;break;
        case 13: ++output->key.inventory;break;
        case 14: ++output->key.avatar;break;
        case 15: ++output->key.utility;break;
        case 16: ++output->key.ordinary;break;
        case 17: output->key.utility_identity[0]^=1;break;
        case 18: output->key.ordinary_identity[0]^=1;break;
        case 19: output->key.peer_size=USS_PUBLICATION_PEER_BYTES+1;break;
        case 20: output->fingerprint_size=USS_PUBLICATION_FINGERPRINT_BYTES+1;break;
        }
    }
    return 1;
}
static UssPublicationSend send_snapshot(void* user,UssPublicationKind kind,
    const UssPublicationCapture* snapshot,uint64_t metadata)
{
    World* world=user;uint64_t expected;
    memcpy(&expected,world->snapshot.owner.row+40,8);
    assert(metadata==expected && metadata>UINT64_C(9007199254740992));
    assert(world->state.used && world->state.attempted==world->sends+1);
    assert(world->state.game_thread==snapshot->thread);
    if(kind==USS_PUBLICATION_FORMATION) assert(world->state.formation_attempted);
    if(world->reenter) {
        UssPublicationResult nested=uss_publication_boundary(&world->state,&world->services,
            &world->snapshot.owner,0,USS_PUBLICATION_POST_ATTACH);
        assert(nested.reason==USS_PUB_REENTRANT);
        assert(!uss_publication_reset(&world->state) && !uss_publication_suspend(&world->state));
    }
    ++world->sends;trace(world,kind==USS_PUBLICATION_FORMATION ? 'S' : 's');
    return world->send_result;
}
static void fixture(World* world)
{
    UssPublicationKey* key;
    uint64_t metadata=UINT64_C(0xfedcba9876543210);
    memset(world,0,sizeof(*world));world->now=100;world->send_result=USS_PUBLICATION_SEND_RETURNED;
    world->services.user=world;world->services.clock_ms=clock_ms;world->services.view=view;
    world->services.capture=capture;world->services.send=send_snapshot;
    world->view.valid=world->view.mission=world->view.pair_present=world->view.supported_pair=1;
    key=&world->snapshot.key;key->pair_generation=1;key->context_generation=1;
    key->session=100;key->mapping=200;key->owner=300;key->inventory=400;
    key->avatar=22;key->utility=103;key->ordinary=102;
    key->utility_identity[0]=1;key->ordinary_identity[0]=2;key->peer_size=3;
    memcpy(key->peers,"abc",3);world->view.key=*key;
    world->snapshot.valid=1;world->snapshot.thread=7;
    world->snapshot.owner.owner=key->owner;world->snapshot.owner.inventory=key->inventory;
    world->snapshot.owner.address=500;world->snapshot.owner.avatar=key->avatar;
    put32(world->snapshot.owner.row,101);put32(world->snapshot.owner.row+4,104);
    put32(world->snapshot.owner.row+8,key->ordinary);put32(world->snapshot.owner.row+28,1);
    memcpy(world->snapshot.owner.row+40,&metadata,8);
    world->snapshot.fingerprint_size=4;memcpy(world->snapshot.fingerprint,"pins",4);
    world->snapshot.wires[0]=1;world->snapshot.wires[1]=2;
    world->snapshot.wires[2]=3;world->snapshot.wires[3]=4;
}
static UssPublicationResult boundary(World* world,unsigned formation,UssPublicationStage stage)
{return uss_publication_boundary(&world->state,&world->services,&world->snapshot.owner,formation,stage);}
static UssPublicationResult poll(World* world)
{return uss_publication_poll(&world->state,&world->services,&world->snapshot.owner);}
static void member(World* world,uint32_t entity){put32(world->snapshot.owner.row+8,entity);}
static void expect_trace(World* world,const char* expected)
{assert(!strcmp(world->trace,expected));}
int main(void)
{
    World world;UssPublicationResult result;unsigned i,captures;uint64_t epoch;
    fixture(&world);
    assert(boundary(&world,1,USS_PUBLICATION_PRE_ATTACH).outcome==USS_PUBLICATION_RETURNED);
    assert(world.state.formed && world.state.member==102 && world.state.epoch==1);
    assert(boundary(&world,0,USS_PUBLICATION_POST_ATTACH).outcome==USS_PUBLICATION_UNCHANGED);
    member(&world,103);assert(boundary(&world,0,USS_PUBLICATION_POST_ATTACH).outcome==USS_PUBLICATION_RETURNED);
    assert(boundary(&world,0,USS_PUBLICATION_POST_ATTACH).outcome==USS_PUBLICATION_UNCHANGED);
    member(&world,102);assert(boundary(&world,0,USS_PUBLICATION_POST_ATTACH).outcome==USS_PUBLICATION_RETURNED);
    expect_trace(&world,"FfSIIisIIis");assert(world.sends==3 && world.state.returned==3);
    /* Utility-first capture and restoration publish the actual members in order. */
    fixture(&world);member(&world,103);
    assert(boundary(&world,0,USS_PUBLICATION_POST_ATTACH).reason==USS_PUB_PENDING_ORDINARY);
    assert(boundary(&world,0,USS_PUBLICATION_UTILITY_FIRST).outcome==USS_PUBLICATION_RETURNED);
    member(&world,102);assert(boundary(&world,0,USS_PUBLICATION_POST_ATTACH).outcome==USS_PUBLICATION_RETURNED);
    expect_trace(&world,"IUfSIis");
    fixture(&world);assert(boundary(&world,1,USS_PUBLICATION_PRE_ATTACH).outcome==USS_PUBLICATION_RETURNED);
    member(&world,103);assert(boundary(&world,1,USS_PUBLICATION_PRE_ATTACH).outcome==USS_PUBLICATION_UNCHANGED);
    assert(world.state.member==102 && world.sends==1);
    assert(boundary(&world,0,USS_PUBLICATION_POST_ATTACH).outcome==USS_PUBLICATION_RETURNED);
    /* Same entity handles, new pair lifetime must not inherit formed status. */
    fixture(&world);boundary(&world,1,USS_PUBLICATION_PRE_ATTACH);
    ++world.snapshot.key.pair_generation;world.view.key=world.snapshot.key;
    assert(boundary(&world,1,USS_PUBLICATION_PRE_ATTACH).outcome==USS_PUBLICATION_RETURNED);assert(world.sends==2);
    world.snapshot.key.peers[0]^=1;world.view.key=world.snapshot.key;
    assert(boundary(&world,1,USS_PUBLICATION_PRE_ATTACH).outcome==USS_PUBLICATION_RETURNED);assert(world.sends==3);
    for(i=1;i<=20;++i) {
        fixture(&world);world.change_entry=i;result=boundary(&world,1,USS_PUBLICATION_PRE_ATTACH);
        assert(result.outcome==USS_PUBLICATION_REFUSED && !world.sends && !world.state.attempted);
    }
    for(i=1;i<=2;++i) {
        fixture(&world);world.fail_capture=i;
        assert(boundary(&world,1,USS_PUBLICATION_PRE_ATTACH).outcome==USS_PUBLICATION_REFUSED);
        assert(!world.sends && !world.state.attempted && !world.state.formation_attempted);
    }
    fixture(&world);{
        UssPublicationOwner caller=world.snapshot.owner;
        ++caller.address;
        assert(uss_publication_boundary(&world.state,&world.services,&caller,1,
            USS_PUBLICATION_PRE_ATTACH).reason==USS_PUB_CALLER_CHANGED);
        assert(!world.sends && !world.state.attempted);
    }
    fixture(&world);world.view.key.context_generation=2;
    assert(boundary(&world,1,USS_PUBLICATION_PRE_ATTACH).reason==USS_PUB_CALLER_CHANGED);
    fixture(&world);world.snapshot.fingerprint_size=USS_PUBLICATION_FINGERPRINT_BYTES+1;
    assert(boundary(&world,1,USS_PUBLICATION_PRE_ATTACH).reason==USS_PUB_CAPTURE);
    fixture(&world);world.snapshot.key.peer_size=USS_PUBLICATION_PEER_BYTES+1;
    assert(boundary(&world,1,USS_PUBLICATION_PRE_ATTACH).reason==USS_PUB_CAPTURE);
    fixture(&world);put32(world.snapshot.owner.row+28,2);
    assert(boundary(&world,1,USS_PUBLICATION_PRE_ATTACH).reason==USS_PUB_NOT_APPLICABLE && !world.captures);
    fixture(&world);assert(boundary(&world,1,USS_PUBLICATION_ENTRY_FORMATION).reason==USS_PUB_CONTRACT);
    fixture(&world);world.snapshot.valid=0;assert(boundary(&world,1,USS_PUBLICATION_PRE_ATTACH).reason==USS_PUB_CAPTURE);
    fixture(&world);world.snapshot.thread=0;assert(boundary(&world,1,USS_PUBLICATION_PRE_ATTACH).reason==USS_PUB_CAPTURE);
    fixture(&world);world.view.mission=0;assert(boundary(&world,1,USS_PUBLICATION_PRE_ATTACH).outcome==USS_PUBLICATION_REFUSED);
    for(i=0;i<=2;i+=2) {
        fixture(&world);world.send_result=(UssPublicationSend)i;world.reenter=1;
        result=boundary(&world,1,USS_PUBLICATION_PRE_ATTACH);
        assert(result.outcome==(i ? USS_PUBLICATION_UNCERTAIN : USS_PUBLICATION_NOT_ENTERED));
        assert(world.state.disabled && world.state.formation_attempted && world.state.used==1 && !world.state.returned);
        captures=world.captures;assert(uss_publication_reset(&world.state));
        assert(boundary(&world,1,USS_PUBLICATION_PRE_ATTACH).reason==USS_PUB_DISABLED);
        assert(world.captures==captures && world.sends==1);assert(poll(&world).reason==USS_PUB_DISABLED);
    }
    fixture(&world);boundary(&world,1,USS_PUBLICATION_PRE_ATTACH);member(&world,103);
    world.send_result=USS_PUBLICATION_SEND_UNCERTAIN;
    assert(boundary(&world,0,USS_PUBLICATION_POST_ATTACH).outcome==USS_PUBLICATION_UNCERTAIN);
    assert(world.state.member==102 && world.state.returned==1);
    fixture(&world);world.reenter=world.reenter_clock=1;assert(poll(&world).outcome==USS_PUBLICATION_RETURNED);
    /* Hidden pair preserves deduplication. Only confirmed mission exit clears it. */
    fixture(&world);assert(poll(&world).outcome==USS_PUBLICATION_RETURNED);captures=world.captures;
    assert(poll(&world).reason==USS_PUB_THROTTLED);world.now+=250;world.view.pair_present=0;
    assert(poll(&world).outcome==USS_PUBLICATION_REFUSED && world.state.formed);
    world.now+=250;world.view.pair_present=1;assert(poll(&world).outcome==USS_PUBLICATION_UNCHANGED);
    assert(world.captures==captures);epoch=world.state.epoch;
    world.now+=250;world.view.valid=0;assert(poll(&world).reason==USS_PUB_SCOPE);
    assert(world.state.record_present && world.state.formed);
    world.now+=250;world.view.valid=1;world.view.mission=0;poll(&world);assert(!world.state.record_present);
    world.now+=250;world.view.mission=1;assert(poll(&world).outcome==USS_PUBLICATION_RETURNED);
    assert(world.state.epoch==epoch+1 && world.sends==2);
    fixture(&world);assert(uss_publication_suspend(&world.state));assert(uss_publication_reset(&world.state));
    assert(boundary(&world,1,USS_PUBLICATION_PRE_ATTACH).reason==USS_PUB_SUSPENDED);
    assert(poll(&world).reason==USS_PUB_SUSPENDED && !world.captures);
    /* 64 attempts per10 seconds, shared across reset/new pair lifetimes. */
    fixture(&world);
    for(i=0;i<64;++i){member(&world,(i&1)?103:102);assert(boundary(&world,0,USS_PUBLICATION_POST_ATTACH).outcome==USS_PUBLICATION_RETURNED);}
    member(&world,102);assert(boundary(&world,0,USS_PUBLICATION_POST_ATTACH).reason==USS_PUB_RATE_LIMIT);
    assert(world.sends==64 && world.state.used==64);assert(uss_publication_reset(&world.state));
    assert(boundary(&world,1,USS_PUBLICATION_PRE_ATTACH).reason==USS_PUB_RATE_LIMIT);
    world.now=10100;assert(boundary(&world,1,USS_PUBLICATION_PRE_ATTACH).outcome==USS_PUBLICATION_RETURNED);
    assert(world.state.used==1 && world.sends==65);
    fixture(&world);boundary(&world,1,USS_PUBLICATION_PRE_ATTACH);assert(uss_publication_reset(&world.state));
    ++world.snapshot.thread;assert(boundary(&world,1,USS_PUBLICATION_PRE_ATTACH).reason==USS_PUB_CAPTURE);
    fixture(&world);world.now=UINT64_MAX-100;assert(poll(&world).reason==USS_PUB_CLOCK_RANGE && !world.state.busy);
    fixture(&world);world.now=UINT64_MAX-100;assert(boundary(&world,1,USS_PUBLICATION_PRE_ATTACH).reason==USS_PUB_CLOCK_RANGE);
    fixture(&world);world.state.epoch=UINT64_MAX;assert(boundary(&world,1,USS_PUBLICATION_PRE_ATTACH).outcome==USS_PUBLICATION_REFUSED);
    fixture(&world);world.state.formation_attempted=world.state.record_present=1;world.state.last_mission=1;
    world.state.record=world.snapshot.key;assert(boundary(&world,1,USS_PUBLICATION_PRE_ATTACH).reason==USS_PUB_ALREADY_ATTEMPTED);
    fixture(&world);world.services.capture=NULL;assert(boundary(&world,1,USS_PUBLICATION_PRE_ATTACH).reason==USS_PUB_CONTRACT);
    assert(!uss_publication_reset(NULL) && !uss_publication_suspend(NULL));
    puts("publication state: literal formation/incoming traces, lifetime/dedup, capture changes, exact metadata, no uncertain replay, rate/poll/reset PASS");
    return 0;
}
