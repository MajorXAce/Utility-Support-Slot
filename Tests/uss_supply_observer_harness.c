#include <assert.h>
#include <stdio.h>
#include "../src/uss_supply_observer.h"
static UssRefillTarget live_target;static unsigned calls;
static UssRefillTarget read_target(void* user){(void)user;return live_target;}
static int verify_event(void* user,const UssRefillEvidence* e,const UssRefillTarget* t)
{(void)user;(void)e;(void)t;return 1;}
static unsigned apply_refill(void* user,const UssRefillTarget* t,float scale)
{(void)user;assert(t->target==live_target.target && scale==1);++calls;live_target.reserve+=2;return USS_REFILL_CALL_RETURNED;}
static UssSupplySnapshot snapshot(void)
{
    UssSupplySnapshot s={0};static const unsigned char id[8]={0x58,0x9a,0x23,0x09,0x71,0xa4,0xf9,0x4e};
    s.target.epoch=s.target.owner=s.target.inventory=s.target.ammo_manager=s.target.ammo_unit=1;
    s.target.avatar=2;s.target.target=3;s.target.real_support=4;s.target.kind=1;s.target.reserve=1;s.target.direct_amount=2;s.target.identity[0]=5;
    s.target.valid=s.target.eligible=s.target.stored=s.target.non_expendable=s.target.ordinary=1;
    s.primary=10;s.secondary=11;s.backpack=12;s.pack.entity=12;s.pack.manager=13;s.pack.unit=14;
    s.pack.kind=3;s.pack.epoch=1;s.pack.reserve=3;memcpy(s.pack.identity,id,8);s.signal_mask=31;s.signals[0]=1;return s;
}
int main(void)
{
    UssSupplyObserver state={0};UssSupplySnapshot s=snapshot();UssRefillEvidence e,before;uint64_t serial=7;float scale;unsigned i;
    assert(!uss_supply_observe(&state,&s,1000,1,0,&serial,&e));
    --s.pack.reserve;assert(!uss_supply_observe(&state,&s,1025,1,0,&serial,&e));
    ++s.signals[0];assert(!uss_supply_observe(&state,&s,1224,1,0,&serial,&e));
    assert(uss_supply_observe(&state,&s,1225,1,0,&serial,&e) && serial==8 && e.pack_spent_ms==1025);
    assert(uss_refill_policy(&e,1225,&scale) && scale==1);
    {UssRefillState transaction={0};UssRefillServices io={NULL,read_target,verify_event,apply_refill};
        live_target=s.target;assert(uss_refill_apply(&transaction,&io,&e,1225)==USS_REFILL_CONFIRMED && calls==1);
        assert(uss_refill_apply(&transaction,&io,&e,1225)==USS_REFILL_SKIPPED && calls==1);}
    assert(!uss_supply_observe(&state,&s,1250,1,0,&serial,&e) && serial==8);
    /* Resource gain may precede the charge drop, but cannot create an event alone. */
    memset(&state,0,sizeof(state));s=snapshot();assert(!uss_supply_observe(&state,&s,1000,1,0,&serial,&e));
    ++s.signals[0];assert(!uss_supply_observe(&state,&s,1025,1,0,&serial,&e));
    --s.pack.reserve;assert(!uss_supply_observe(&state,&s,1300,1,0,&serial,&e));
    assert(uss_supply_observe(&state,&s,1500,1,0,&serial,&e));
    for(i=0;i<8;++i){
        memset(&state,0,sizeof(state));s=snapshot();assert(!uss_supply_observe(&state,&s,1000,1,0,&serial,&e));
        --s.pack.reserve;assert(!uss_supply_observe(&state,&s,1025,1,0,&serial,&e));++s.signals[0];
        if(i==0)++s.target.reserve;if(i==1)++s.pack.unit;if(i==2)++s.primary;if(i==3)s.pack.reserve=0;
        memset(&e,0xa5,sizeof(e));before=e;
        assert(!uss_supply_observe(&state,&s,i==6?1526:i==7?1000:1225,i==4?0:1,i==5?1:0,&serial,&e));
        assert(!memcmp(&e,&before,sizeof(e)));
    }
    memset(&state,0,sizeof(state));s=snapshot();assert(!uss_supply_observe(&state,&s,1000,1,0,&serial,&e));
    s.pack.reserve=1;++s.signals[0];assert(!uss_supply_observe(&state,&s,1025,1,0,&serial,&e));
    assert(!uss_supply_observe(&state,&s,1225,1,0,&serial,&e));
    s=snapshot();s.signals[4]=101;assert(!uss_supply_valid(&s));
    s=snapshot();s.signal_mask=0;memset(&state,0,sizeof(state));
    assert(!uss_supply_observe(&state,&s,1000,1,0,&serial,&e));--s.pack.reserve;++s.signals[0];
    assert(!uss_supply_observe(&state,&s,1025,1,0,&serial,&e));
    assert(!uss_supply_observe(&state,&s,1225,1,0,&serial,&e)); /* unknown signals cannot establish gain */
    puts("USS own Pack: spend/gain ordering, settle/expiry, shared serial, duplicate, ambiguity and context cancellation PASS");return 0;
}
