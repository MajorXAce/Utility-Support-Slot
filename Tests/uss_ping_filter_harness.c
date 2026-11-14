/* Ping-filter state machine (uss_ping_filter.h) with a fake engine: queued commands take effect only when the test
 * advances the engine, as the real read-back does on a later poll. No game access. */
#ifdef NDEBUG
#error Ping filter checks require active assertions
#endif
#include "../src/uss_ping_filter.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct FakeHelper { unsigned exists;uint32_t entity,actor,body_id,index;unsigned char identity[8]; } FakeHelper;
static FakeHelper helpers[4];
static struct {
    unsigned ready_ok,ready_calls,send_ok,sends,resolve_calls,resolve_blocked;
    uint32_t last_actor,last_hash;unsigned pending;uint32_t pending_actor,pending_index;
    char events[64][40];unsigned event_count;
} fake;
static const unsigned char ID_A[8]={0xAA,0x79,0xCA,0x92,0x05,0x46,0x13,0xFC},ID_B[8]={9,8,7,6,5,4,3,2};

static int io_ready(void* user){(void)user;++fake.ready_calls;return fake.ready_ok;}
static int io_resolve(void* user,uint32_t entity,const unsigned char identity[8],UssPingHelper* out)
{
    unsigned i;(void)user;++fake.resolve_calls;
    if(fake.resolve_blocked)return 0;
    for(i=0;i<4;++i)if(helpers[i].exists && helpers[i].entity==entity && !memcmp(helpers[i].identity,identity,8)){
        memset(out,0,sizeof(*out));out->entity=entity;out->actor=helpers[i].actor;out->body_id=helpers[i].body_id;
        out->packed=0x00AB0000u|helpers[i].index;memcpy(out->identity,identity,8);return 1;}
    return 0;
}
static int io_send(void* user,uint32_t actor,uint32_t hash)
{
    (void)user;if(!fake.send_ok)return 0;
    ++fake.sends;fake.last_actor=actor;fake.last_hash=hash;fake.pending=1;fake.pending_actor=actor;
    fake.pending_index=hash==USS_PING_EXCLUDED_HASH?USS_PING_EXCLUDED_INDEX:hash==USS_PING_NORMAL_HASH?USS_PING_NORMAL_INDEX:999;
    return 1;
}
static void io_note(void* user,const char* event,uint32_t entity,uint32_t value)
{
    (void)user;(void)entity;(void)value;
    if(fake.event_count<64){snprintf(fake.events[fake.event_count],40,"%s",event);++fake.event_count;}
}
static int saw(const char* event){unsigned i;for(i=0;i<fake.event_count;++i)if(!strcmp(fake.events[i],event))return 1;return 0;}
static void engine_tick(void)
{
    unsigned i;
    if(!fake.pending)return;
    for(i=0;i<4;++i)if(helpers[i].exists && helpers[i].actor==fake.pending_actor)helpers[i].index=fake.pending_index;
    fake.pending=0;
}
static UssPingFilter f;
static void scene(void)
{
    UssPingFilterIo io={0};
    memset(&fake,0,sizeof(fake));memset(helpers,0,sizeof(helpers));
    fake.ready_ok=fake.send_ok=1;
    helpers[0].exists=1;helpers[0].entity=101;helpers[0].actor=0x40000011u;helpers[0].body_id=0x500;helpers[0].index=26;memcpy(helpers[0].identity,ID_A,8);
    helpers[1].exists=1;helpers[1].entity=102;helpers[1].actor=0x40000012u;helpers[1].body_id=0x501;helpers[1].index=26;memcpy(helpers[1].identity,ID_B,8);
    io.ready=io_ready;io.resolve=io_resolve;io.send=io_send;io.note=io_note;
    uss_ping_filter_init(&f,&io);
}
static UssPingDesired want(uint32_t entity,uint32_t owner)
{
    UssPingDesired d={0};d.valid=1;d.entity=entity;d.owner=owner;memcpy(d.identity,entity==101?ID_A:ID_B,8);return d;
}
#define NONE ((const UssPingDesired*)NULL)
int main(void)
{
    UssPingDesired a,b,off={0};unsigned i,cycles;uint64_t t=1000;
    /* Nothing desired and nothing tracked: no verification, no read, no call. */
    scene();assert(!uss_ping_filter_step(&f,NONE,7,t) && !uss_ping_filter_step(&f,&off,7,t) && !fake.ready_calls && !fake.resolve_calls && !fake.sends);

    /* Unverified engine/catalog: latch once, never call, never ask again. */
    scene();fake.ready_ok=0;a=want(101,7);
    assert(!uss_ping_filter_step(&f,&a,7,t) && f.failed && fake.ready_calls==1 && !fake.sends && saw("FAILED_ENGINE_OR_CATALOG"));
    assert(!uss_ping_filter_step(&f,&a,7,t+10) && fake.ready_calls==1 && !fake.sends && !fake.resolve_calls);

    /* Normal cycle: apply once, confirm on a later poll, then stay quiet. */
    scene();a=want(101,7);
    assert(uss_ping_filter_step(&f,&a,7,t)==1 && fake.last_actor==0x40000011u && fake.last_hash==USS_PING_EXCLUDED_HASH && saw("APPLY_QUEUED"));
    assert(uss_ping_filter_step(&f,&a,7,t+10)==0 && fake.sends==1 && !f.old.confirmed);   /* queued, not yet applied: no second call */
    engine_tick();
    assert(uss_ping_filter_step(&f,&a,7,t+20)==0 && f.old.confirmed && saw("APPLY_CONFIRMED") && fake.sends==1);
    for(i=0;i<20;++i)assert(uss_ping_filter_step(&f,&a,7,t+30+i)==0);
    assert(fake.sends==1 && helpers[0].index==37 && uss_ping_filter_active(&f));

    /* The stored weapon changes (swap): the first is restored, then the second applied. */
    b=want(102,7);
    assert(uss_ping_filter_step(&f,&b,7,t+100)==1 && fake.last_hash==USS_PING_NORMAL_HASH && fake.last_actor==0x40000011u && saw("RESTORE_QUEUED"));
    assert(uss_ping_filter_step(&f,&b,7,t+110)==0 && fake.sends==2);                     /* restore queued once only */
    engine_tick();
    /* The restore is confirmed and, in the SAME pass (as in the Lua version), the new stored weapon is applied. */
    assert(uss_ping_filter_step(&f,&b,7,t+120)==1 && saw("RESTORE_CONFIRMED") && helpers[0].index==26);
    assert(f.old.valid && f.old.helper.entity==102 && !f.old.confirmed && fake.last_actor==0x40000012u && fake.last_hash==USS_PING_EXCLUDED_HASH);
    engine_tick();assert(uss_ping_filter_step(&f,&b,7,t+140)==0 && f.old.confirmed);

    /* The pair ends or the state is not managed (nothing desired): restore, then forget. */
    assert(uss_ping_filter_step(&f,&off,7,t+200)==1 && fake.last_hash==USS_PING_NORMAL_HASH && fake.last_actor==0x40000012u);
    engine_tick();assert(uss_ping_filter_step(&f,&off,7,t+210)==0 && !f.old.valid && helpers[1].index==26 && !f.failed);

    /* A helper that cannot be read any more (weapon deleted, world replaced) is forgotten and never "restored". */
    scene();a=want(101,7);(void)uss_ping_filter_step(&f,&a,7,t);engine_tick();(void)uss_ping_filter_step(&f,&a,7,t+10);
    helpers[0].exists=0;
    assert(uss_ping_filter_step(&f,&a,7,t+20)==0 && !f.old.valid && saw("FORGET_STALE_HELPER") && fake.sends==1);
    /* The same helper replaced by a different instance (new body id): forgotten, no command to the new object. */
    scene();a=want(101,7);(void)uss_ping_filter_step(&f,&a,7,t);engine_tick();(void)uss_ping_filter_step(&f,&a,7,t+10);
    helpers[0].body_id=0x999;
    assert(uss_ping_filter_step(&f,&a,7,t+20)==0 && fake.sends==1 && saw("FORGET_STALE_HELPER"));
    scene();a=want(101,7);(void)uss_ping_filter_step(&f,&a,7,t);engine_tick();(void)uss_ping_filter_step(&f,&a,7,t+10);
    helpers[0].actor=0x40000099u;
    assert(uss_ping_filter_step(&f,&a,7,t+20)==0 && fake.sends==1 && saw("FORGET_STALE_HELPER"));
    /* A new owner token: the old helper belongs to a retired actor. Forgotten silently, nothing sent for it. */
    scene();a=want(101,7);(void)uss_ping_filter_step(&f,&a,7,t);engine_tick();(void)uss_ping_filter_step(&f,&a,7,t+10);
    assert(uss_ping_filter_step(&f,&off,8,t+20)==0 && !f.old.valid && saw("FORGET_OWNER_CHANGED") && fake.sends==1);

    /* Somebody else changed the filter: leave it alone, stop for the session. */
    scene();a=want(101,7);(void)uss_ping_filter_step(&f,&a,7,t);engine_tick();(void)uss_ping_filter_step(&f,&a,7,t+10);
    helpers[0].index=55;
    assert(uss_ping_filter_step(&f,&a,7,t+20)==0 && !f.old.valid && f.failed && saw("FORGET_EXTERNAL_CHANGE") && fake.sends==1);
    assert(uss_ping_filter_step(&f,&a,7,t+30)==0 && fake.sends==1);
    /* A restore that meets a foreign filter value (neither normal nor excluded) leaves it alone and stops for the session. */
    scene();a=want(101,7);(void)uss_ping_filter_step(&f,&a,7,t);engine_tick();(void)uss_ping_filter_step(&f,&a,7,t+10);
    helpers[0].index=55;
    assert(uss_ping_filter_step(&f,&off,7,t+20)==0 && !f.old.valid && f.failed && saw("FORGET_EXTERNAL_CHANGE") && fake.sends==1);
    scene();a=want(101,7);b=want(102,7);(void)uss_ping_filter_step(&f,&a,7,t);engine_tick();(void)uss_ping_filter_step(&f,&a,7,t+10);
    helpers[0].index=55;
    assert(uss_ping_filter_step(&f,&b,7,t+20)==0 && !f.old.valid && f.failed && fake.sends==1);    /* the swap's restore finds a foreign value */
    /* At apply time the helper must have the normal filter; anything else is rejected. */
    scene();helpers[0].index=55;a=want(101,7);
    assert(uss_ping_filter_step(&f,&a,7,t)==0 && f.failed && !fake.sends && saw("FAILED_UNEXPECTED_FILTER"));
    scene();helpers[0].index=37;a=want(101,7);
    assert(uss_ping_filter_step(&f,&a,7,t)==0 && f.failed && !fake.sends);
    /* The weapon has no readable helper yet: wait, call nothing, no latch. */
    scene();fake.resolve_blocked=1;a=want(101,7);
    assert(uss_ping_filter_step(&f,&a,7,t)==0 && !f.failed && !f.old.valid && !fake.sends);
    fake.resolve_blocked=0;assert(uss_ping_filter_step(&f,&a,7,t+10)==1);

    /* A queued command that never takes effect: deadline, latch, no second apply, and a late effect is still undone. */
    scene();a=want(101,7);(void)uss_ping_filter_step(&f,&a,7,t);
    assert(uss_ping_filter_step(&f,&a,7,t+1000)==0 && !f.failed);                          /* exactly at the deadline */
    assert(uss_ping_filter_step(&f,&a,7,t+1001)==0 && f.failed && saw("FAILED_TIMEOUT") && fake.sends==1);
    for(i=0;i<5;++i)assert(uss_ping_filter_step(&f,&a,7,t+1100+i*100)==0 && fake.sends==1);
    engine_tick();                                                                          /* the engine finally applies it */
    assert(uss_ping_filter_step(&f,&a,7,t+2000)==1 && fake.last_hash==USS_PING_NORMAL_HASH && helpers[0].index==37);
    engine_tick();assert(uss_ping_filter_step(&f,&a,7,t+2100)==0 && helpers[0].index==26 && !f.old.valid);
    /* A restore that never takes effect also times out, once. */
    scene();a=want(101,7);(void)uss_ping_filter_step(&f,&a,7,t);engine_tick();(void)uss_ping_filter_step(&f,&a,7,t+10);
    assert(uss_ping_filter_step(&f,&off,7,t+100)==1);
    assert(uss_ping_filter_step(&f,&off,7,t+1101)==0 && f.failed && saw("FAILED_TIMEOUT") && fake.sends==2);

    /* Rate limit: 32 applies per 10 s window, then latch. */
    scene();
    for(cycles=0;cycles<USS_PING_WINDOW_CAP;++cycles){
        a=want(cycles&1?102:101,7);
        while(!f.old.valid || !f.old.confirmed){(void)uss_ping_filter_step(&f,&a,7,t);engine_tick();t+=5;if(f.failed)break;}
        assert(!f.failed);
        (void)uss_ping_filter_step(&f,&off,7,t);engine_tick();(void)uss_ping_filter_step(&f,&off,7,t+5);engine_tick();(void)uss_ping_filter_step(&f,&off,7,t+10);t+=15;
        assert(!f.old.valid);
    }
    assert(f.window_used==USS_PING_WINDOW_CAP && !f.failed);
    fake.event_count=0; /* the recorder keeps only 64 events and the cycles above produced more */
    a=want(101,7);assert(uss_ping_filter_step(&f,&a,7,t)==0 && f.failed && saw("FAILED_RATE_LIMIT"));
    /* A new window forgives the count (before the latch). */
    scene();t=1000;
    for(cycles=0;cycles<USS_PING_WINDOW_CAP;++cycles){
        a=want(101,7);(void)uss_ping_filter_step(&f,&a,7,t);engine_tick();(void)uss_ping_filter_step(&f,&a,7,t+1);
        (void)uss_ping_filter_step(&f,&off,7,t+2);engine_tick();(void)uss_ping_filter_step(&f,&off,7,t+3);t+=300;
        assert(!f.failed);
    }
    t+=USS_PING_WINDOW_MS;a=want(101,7);assert(uss_ping_filter_step(&f,&a,7,t)==1 && !f.failed && f.window_used==1);

    /* A send the engine refuses to enter is not counted as a call, and the helper is still tracked for restore. */
    scene();fake.send_ok=0;a=want(101,7);
    assert(uss_ping_filter_step(&f,&a,7,t)==0 && !f.calls && f.old.valid);
    /* Forget (new actor): everything including the latch is cleared; the io stays. */
    scene();fake.ready_ok=0;a=want(101,7);(void)uss_ping_filter_step(&f,&a,7,t);assert(f.failed);
    uss_ping_filter_forget(&f);assert(!f.failed && !f.checked && f.io.resolve==io_resolve && !f.old.valid);
    fake.ready_ok=1;assert(uss_ping_filter_step(&f,&a,7,t)==1);
    /* Missing io: inert. */
    memset(&f,0,sizeof(f));a=want(101,7);assert(!uss_ping_filter_step(&f,&a,7,t) && !uss_ping_filter_step(NULL,&a,7,t));
    puts("USS ping filter: apply/confirm/restore, swap, pair end, stale/replaced helper, external change, timeout, rate limit, latch, unverified engine PASS");
    return 0;
}
