#include "../src/uss_refill_driver.h"
#define main pickup_reader_regressions
#include "uss_pickup_readers_harness.c"
#undef main
static unsigned driver_calls;
static int driver_verify(void* user,const UssRefillEvidence* event,const UssRefillTarget* target)
{
    if(event->route==USS_REFILL_GROUND)return refill_verify(user,event,target);
    return event->route==USS_REFILL_OWN_PACK && event->pack_entity==12 && event->charges_before==3 && event->charges_after==2;
}
static unsigned driver_refill(void* user,const UssRefillTarget* target,float scale)
{(void)user;assert(target->target==refill_current.target && (scale==0.5f || scale==1));++driver_calls;++refill_current.reserve;return USS_REFILL_CALL_RETURNED;}
static UssSupplySnapshot supply_for(UssRefillTarget t)
{
    UssSupplySnapshot s={0};static const unsigned char id[8]={0x58,0x9a,0x23,0x09,0x71,0xa4,0xf9,0x4e};
    s.target=t;s.primary=t.primary;s.secondary=t.secondary;s.backpack=t.backpack;
    s.pack.entity=t.backpack;s.pack.epoch=t.epoch;s.pack.manager=20;s.pack.unit=21;s.pack.kind=3;s.pack.reserve=3;
    memcpy(s.pack.identity,id,8);s.signal_mask=1;s.signals[0]=1;return s;
}
int main(void)
{
    UssRefillDriver state={0};UssRefillMemoryIo memory={NULL,0x10000000,read_memory,context};
    UssRefillServices services={NULL,refill_observe,driver_verify,driver_refill};UssRefillTarget t;UssSupplySnapshot supply;unsigned before;
    (void)pickup_reader_regressions;
    t=fixture();t.primary=10;t.secondary=11;t.backpack=12;refill_current=t;
    word(action_manager+0x53dcd0+0x9ec+8,0);
    assert(uss_refill_driver_tick(&state,&memory,&services,&t,NULL,1000)==USS_REFILL_SKIPPED);
    before=reads;assert(uss_refill_driver_tick(&state,&memory,&services,&t,NULL,1001)==USS_REFILL_SKIPPED && reads==before);
    word(action_manager+0x53dcd0+0x9ec+8,10);
    assert(uss_refill_driver_tick(&state,&memory,&services,&t,NULL,1025)==USS_REFILL_CONFIRMED && driver_calls==1 && state.pickups.serial==1);
    assert(state.transaction.notice.serial==1 && state.transaction.notice.route==USS_REFILL_GROUND &&
        state.transaction.notice.after.reserve-state.transaction.notice.before.reserve==1);
    t=refill_current;supply=supply_for(t);
    assert(uss_refill_driver_tick(&state,&memory,&services,&t,&supply,1050)==USS_REFILL_SKIPPED && !state.supply.known);
    assert(uss_refill_driver_tick(&state,&memory,&services,&t,&supply,1600)==USS_REFILL_SKIPPED && state.supply.known);
    --supply.pack.reserve;++supply.signals[0];
    assert(uss_refill_driver_tick(&state,&memory,&services,&t,&supply,1625)==USS_REFILL_SKIPPED);
    assert(uss_refill_driver_tick(&state,&memory,&services,&t,&supply,1825)==USS_REFILL_CONFIRMED && driver_calls==2 && state.pickups.serial==2);
    assert(state.transaction.notice.serial==2 && state.transaction.notice.route==USS_REFILL_OWN_PACK &&
        state.transaction.notice.after.reserve-state.transaction.notice.before.reserve==1);
    t=refill_current;supply.target=t;
    assert(uss_refill_driver_tick(&state,&memory,&services,&t,&supply,1850)==USS_REFILL_SKIPPED && driver_calls==2);
    state.transaction.disabled=1;
    assert(uss_refill_driver_tick(&state,&memory,&services,&t,&supply,1900)==USS_REFILL_STOPPED);
    ++t.epoch;supply.target=t;assert(uss_refill_driver_tick(&state,&memory,&services,&t,&supply,1925)==USS_REFILL_STOPPED);
    state.transaction.disabled=0;t.direct_amount=0;
    assert(uss_refill_driver_tick(&state,&memory,&services,&t,&supply,1950)==USS_REFILL_SKIPPED && !state.pickups.bound && state.pickups.serial==2);
    puts("USS refill driver: actual completion -> OWN effect then Pack -> OWN effect, shared serial, replay/overlap/25ms guard and disabled lifetime PASS");return 0;
}
