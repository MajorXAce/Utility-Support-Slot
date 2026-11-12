#include <assert.h>
#include <stdio.h>
#include "../src/uss_refill_native_calls.h"
static UssRefillTarget current;
static unsigned calls,reads,allow=1,gain=2,call_result=USS_REFILL_CALL_RETURNED;
static UssRefillTarget observe(void* p){(void)p;++reads;return current;}
static int verify(void* p,const UssRefillEvidence* e,const UssRefillTarget* t){(void)p;(void)e;(void)t;return (int)allow;}
static unsigned refill(void* p,const UssRefillTarget* t,float scale){(void)p;(void)t;assert(uss_refill_scale_valid(scale));++calls;current.reserve+=gain;return call_result;}
static UssRefillEvidence fixture(UssRefillState* s)
{
    UssRefillEvidence e={0};memset(s,0,sizeof(*s));memset(&current,0,sizeof(current));
    current.epoch=current.owner=current.inventory=current.ammo_manager=current.ammo_unit=1;current.avatar=2;
    current.target=3;current.real_support=4;current.kind=1;current.reserve=1;current.direct_amount=2;current.identity[0]=5;
    current.valid=current.eligible=current.stored=current.non_expendable=current.ordinary=1;
    e.target=current;e.serial=1;e.time_ms=e.policy_time_ms=1000;e.route=1;e.source=10;e.source_kind=4;
    e.source_identity[0]=6;e.completed=e.source_verified=e.policy_verified=1;calls=reads=0;gain=2;allow=1;call_result=1;return e;
}
static void own_refill(void* manager,uint32_t entity,float scale)
{assert(manager==NULL && entity==current.target && scale==0.5f);++calls;}
static unsigned notices;static uint64_t notified_serial;
static void own_notice(void* user,const UssRefillNotice* notice)
{
    (void)user;assert(notified_serial==notice->serial && notice->serial==1 && notice->route==USS_REFILL_GROUND &&
        notice->before.reserve==1 && notice->after.reserve==3);++notices;
}
int main(void)
{
    UssRefillState state;UssRefillEvidence e;UssRefillServices io={NULL,observe,verify,refill};float scale;unsigned i;
    e=fixture(&state);assert(uss_refill_apply(&state,&io,&e,1000)==USS_REFILL_CONFIRMED && calls==1);
    assert(uss_refill_notify(&state,USS_REFILL_CONFIRMED,&notified_serial,NULL,own_notice) && notices==1 && calls==1);
    assert(!uss_refill_notify(&state,USS_REFILL_CONFIRMED,&notified_serial,NULL,own_notice) && notices==1 && calls==1);
    notified_serial=0;assert(!uss_refill_notify(&state,USS_REFILL_FULL,&notified_serial,NULL,own_notice) && !notified_serial);
    assert(!uss_refill_notify(&state,USS_REFILL_STOPPED,&notified_serial,NULL,own_notice) && !notified_serial);
    assert(uss_refill_apply(&state,&io,&e,1000)==USS_REFILL_SKIPPED && calls==1);
    for(i=0;i<7;++i){e=fixture(&state);
        if(i==0)e.completed=0;if(i==1)e.source_verified=0;if(i==2)e.policy_verified=0;
        if(i==3)e.source_kind=1;if(i==4)e.time_ms=3000;if(i==5)e.policy_time_ms=1001;if(i==6)e.target.stored=0;
        assert(uss_refill_apply(&state,&io,&e,1000)==USS_REFILL_SKIPPED && !calls);}
    e=fixture(&state);e.source_kind=7;e.source_scale=9999;assert(uss_refill_policy(&e,1000,&scale) && scale==9999);
    e.source_scale=0;assert(!uss_refill_policy(&e,1000,&scale));
    e=fixture(&state);e.route=2;e.pack_entity=8;e.pack_manager=9;e.pack_unit=10;e.charges_before=2;e.charges_after=1;e.resource_gain=1;e.pack_spent_ms=800;
    assert(uss_refill_policy(&e,1000,&scale) && scale==1);e.ground_activity_near=1;assert(!uss_refill_policy(&e,1000,&scale));
    e.ground_activity_near=0;e.target.direct_amount=0;assert(!uss_refill_policy(&e,1000,&scale));
    e=fixture(&state);assert(!uss_refill_policy(&e,1501,&scale));
    e=fixture(&state);e.target.direct_amount=0;assert(!uss_refill_policy(&e,1000,&scale));
    assert(uss_refill_apply(&state,&io,&e,1000)==USS_REFILL_SKIPPED && !calls);
    e=fixture(&state);++current.ammo_unit;assert(uss_refill_apply(&state,&io,&e,1000)==USS_REFILL_SKIPPED && !calls);
    e=fixture(&state);allow=0;assert(uss_refill_apply(&state,&io,&e,1000)==USS_REFILL_SKIPPED && !calls && state.last_serial==1);
    e=fixture(&state);gain=0;assert(uss_refill_apply(&state,&io,&e,1000)==USS_REFILL_FULL);
    e=fixture(&state);gain=201;assert(uss_refill_apply(&state,&io,&e,1000)==USS_REFILL_STOPPED && state.disabled);
    e=fixture(&state);call_result=2;assert(uss_refill_apply(&state,&io,&e,1000)==USS_REFILL_STOPPED && calls==1);
    e.serial=2;assert(uss_refill_apply(&state,&io,&e,1000)==USS_REFILL_SKIPPED && calls==1);
    {UssRefillNativeCalls native={own_refill,own_refill};e=fixture(&state);
        assert(uss_refill_native_call(&native,&e.target,0.5f)==USS_REFILL_CALL_RETURNED && calls==1);
        e.target.kind=USS_REFILL_ROUNDS;assert(uss_refill_native_call(&native,&e.target,0.5f)==USS_REFILL_CALL_RETURNED && calls==2);
        e.target.direct_amount=0;assert(uss_refill_native_call(&native,&e.target,0.5f)==USS_REFILL_CALL_NOT_ENTERED && calls==2);
        e.target.direct_amount=2;native.rounds=NULL;assert(uss_refill_native_call(&native,&e.target,0.5f)==USS_REFILL_CALL_NOT_ENTERED && calls==2);}
    puts("USS refill: source completion, half/native scales, pack spend/gain, overlap exclusion, identity recheck, serial dedup and no retry PASS");return 0;
}
