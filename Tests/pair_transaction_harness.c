#ifdef NDEBUG
#error Pair transaction checks require active assertions
#endif
#include "../src/pair_transaction.h"
#include <assert.h>
#include <stdio.h>

typedef struct PairFixture {
    UssPairObservation state;
    unsigned events[32],values[32],labels[2],stages[2],deferred[2],count,observations,attachments,publish_calls,sends,selections,acks;
    unsigned ready_waits,publish_waits,published_by_switch,published_member;
    unsigned select_stop,attach_stop,attach_wait,publish_stop,verify_ok,change_after_attach;
} PairFixture;
static void trace(PairFixture* fixture,unsigned event,uint32_t value)
{assert(fixture->count<32);fixture->events[fixture->count]=event;fixture->values[fixture->count++]=value;}
static UssPairObservation observe_pair(void* user)
{PairFixture* fixture=user;++fixture->observations;return fixture->state;}
static UssPairServiceResult choose_slot(void* user,const UssPairObservation* expected,uint32_t slot)
{
    PairFixture* fixture=user;assert(pair_context_equal(&expected->context,&fixture->state.context));
    trace(fixture,1,slot);++fixture->selections;fixture->state.active=slot;
    return fixture->select_stop==slot?USS_PAIR_SERVICE_STOP:USS_PAIR_SERVICE_DONE;
}
static UssPairServiceResult ready_pair(void* user,const UssPairObservation* expected)
{
    PairFixture* fixture=user;assert(expected->active==1);
    if(fixture->ready_waits){--fixture->ready_waits;return USS_PAIR_SERVICE_WAIT;}
    return USS_PAIR_SERVICE_DONE;
}
static UssPairServiceResult switch_pair_member(void* user,const UssPairObservation* expected,
                                               const UssPairStep* step,unsigned defer_ack)
{
    PairFixture* fixture=user;assert(expected->active==1 && expected->support==fixture->state.support);
    assert(defer_ack<=1 && fixture->attachments<2);
    fixture->labels[fixture->attachments]=(unsigned)step->label;
    fixture->stages[fixture->attachments]=(unsigned)step->publication_stage;
    fixture->deferred[fixture->attachments]=defer_ack;
    trace(fixture,2,step->member.entity);++fixture->attachments;
    fixture->state.support=step->member.entity;
    if(step->member.entity==fixture->state.utility.entity)fixture->state.utility_parent=fixture->state.context.avatar;
    else fixture->state.ordinary_parent=fixture->state.context.avatar;
    if(fixture->published_by_switch){fixture->published_member=step->member.entity;++fixture->sends;}
    if(fixture->change_after_attach)++fixture->state.context.row_address;
    if(fixture->attach_wait)return USS_PAIR_SERVICE_WAIT;
    return fixture->attach_stop?USS_PAIR_SERVICE_STOP:USS_PAIR_SERVICE_DONE;
}
static UssPairServiceResult publish_pair(void* user,const UssPairObservation* expected,const UssPairStep* step)
{
    PairFixture* fixture=user;assert(expected->active==1 && expected->support==step->member.entity);
    trace(fixture,3,step->member.entity);++fixture->publish_calls;
    if(fixture->publish_waits){--fixture->publish_waits;return USS_PAIR_SERVICE_WAIT;}
    if(fixture->published_member!=step->member.entity){++fixture->sends;fixture->published_member=step->member.entity;}
    return fixture->publish_stop?USS_PAIR_SERVICE_STOP:USS_PAIR_SERVICE_DONE;
}
static int verify_pair_fixture(void* user,const UssPairObservation* expected)
{PairFixture* fixture=user;assert(expected->active==1);return (int)fixture->verify_ok;}
static void acknowledge_pair(void* user,uint32_t outgoing,uint32_t incoming,UssPairLabel label)
{
    PairFixture* fixture=user;assert(outgoing!=incoming && fixture->state.active==3);
    assert(label==USS_PAIR_DRAW_UTILITY || label==USS_PAIR_DRAW_ORDINARY);
    trace(fixture,4,incoming);++fixture->acks;
}
static UssPairServices fixture_services(PairFixture* fixture,UssPairOrder order)
{
    static const unsigned char silo[]={0xaa,0x79,0xca,0x92,0x05,0x46,0x13,0xfc};
    static const unsigned char ordinary[]={1,2,3,4,5,6,7,8};
    UssPairServices services={fixture,observe_pair,choose_slot,ready_pair,switch_pair_member,publish_pair,verify_pair_fixture,acknowledge_pair};
    memset(fixture,0,sizeof(*fixture));fixture->verify_ok=1;
    fixture->state.valid=fixture->state.mission=fixture->state.host=fixture->state.eligible=1;
    fixture->state.can_switch=fixture->state.members_valid=1;
    fixture->state.formed=order==USS_PAIR_DRAW;fixture->state.formation_pending=order!=USS_PAIR_DRAW;
    fixture->state.pair_generation=7;fixture->state.epoch=2;
    fixture->state.context=(UssPairContext){100,200,300,10,20,30,0};
    fixture->state.utility.entity=40;memcpy(fixture->state.utility.identity,silo,8);
    fixture->state.ordinary.entity=50;memcpy(fixture->state.ordinary.identity,ordinary,8);
    fixture->state.utility_parent=fixture->state.ordinary_parent=10;
    fixture->state.active=3;
    fixture->state.support=order==USS_PAIR_ORDINARY_FIRST?40:50;
    return services;
}
static void begin_pair(UssPairTransaction* work,UssPairServices* services,PairFixture* fixture,
                        UssPairOrder order,uint32_t selected,unsigned pickup)
{
    memset(work,0,sizeof(*work));
    assert(uss_pair_begin(work,services,&fixture->state,order,selected,pickup,&fixture->state.utility,100));
}
static void complete_pair(UssPairTransaction* work,UssPairServices* services)
{
    unsigned i;for(i=0;i<8 && work->phase==USS_PAIR_ACTIVE;++i)uss_pair_tick(work,services,100+i*25);
    assert(work->phase==USS_PAIR_COMPLETE);
}
static void terminal_no_replay(UssPairTransaction* work,UssPairServices* services,PairFixture* fixture)
{
    unsigned effects=fixture->count,observations=fixture->observations;
    assert(work->phase==USS_PAIR_CANCELLED);
    uss_pair_tick(work,services,150);assert(fixture->count==effects && fixture->observations==observations);
}
int main(void)
{
    PairFixture fixture;UssPairServices services;UssPairTransaction work;unsigned role,selected,order,i;
    /* Literal step traces follow Lua queue_formation(): S1, switch, boundary, final selection. */
    for(role=0;role<2;++role)for(order=1;order<=2;++order)for(selected=1;selected<=3;++selected) {
        services=fixture_services(&fixture,(UssPairOrder)order);
        fixture.state.host=!role;fixture.state.joined_client=role;
        begin_pair(&work,&services,&fixture,(UssPairOrder)order,selected,0);complete_pair(&work,&services);
        assert(fixture.state.active==selected && work.real==50 && !work.utility_drawn && !fixture.acks);
        assert(fixture.values[0]==1 && fixture.events[0]==1);
        if(order==1) {assert(fixture.attachments==1 && fixture.events[1]==2 && fixture.values[1]==50);
            assert(fixture.labels[0]==USS_PAIR_STORE_NEW_UTILITY && fixture.stages[0]==USS_PAIR_POST_ATTACH && !fixture.deferred[0]);}
        else {assert(fixture.attachments==2 && fixture.values[1]==40 && fixture.events[2]==3 && fixture.values[2]==40 && fixture.values[3]==50);
            assert(fixture.labels[0]==USS_PAIR_CAPTURE_UTILITY_FIRST && fixture.stages[0]==USS_PAIR_UTILITY_FIRST_CAPTURE);
            assert(fixture.labels[1]==USS_PAIR_RESTORE_UTILITY_FIRST && fixture.stages[1]==USS_PAIR_POST_ATTACH); }
    }
    services=fixture_services(&fixture,USS_PAIR_ORDINARY_FIRST);
    begin_pair(&work,&services,&fixture,USS_PAIR_ORDINARY_FIRST,3,1);complete_pair(&work,&services);
    assert(fixture.attachments==2 && fixture.values[1]==50 && fixture.values[3]==40 && work.real==40 && work.utility_drawn);
    for(role=0;role<2;++role)for(i=0;i<2;++i) {
        services=fixture_services(&fixture,USS_PAIR_DRAW);
        fixture.state.host=!role;fixture.state.joined_client=role;
        if(i)fixture.state.support=40;
        memset(&work,0,sizeof(work));
        assert(uss_pair_begin(&work,&services,&fixture.state,USS_PAIR_DRAW,3,0,i?&fixture.state.ordinary:&fixture.state.utility,100));
        complete_pair(&work,&services);assert(fixture.acks==1 && fixture.attachments==1);
        assert(fixture.events[fixture.count-2]==1 && fixture.events[fixture.count-1]==4);
        assert(fixture.labels[0]==(unsigned)(i?USS_PAIR_DRAW_ORDINARY:USS_PAIR_DRAW_UTILITY));
        assert(fixture.stages[0]==USS_PAIR_POST_ATTACH && fixture.deferred[0]);
    }
    services=fixture_services(&fixture,USS_PAIR_ORDINARY_FIRST);fixture.ready_waits=2;
    begin_pair(&work,&services,&fixture,USS_PAIR_ORDINARY_FIRST,3,0);
    uss_pair_tick(&work,&services,100);uss_pair_tick(&work,&services,125);
    assert(fixture.selections==1 && !fixture.attachments);complete_pair(&work,&services);assert(fixture.attachments==1);
    services=fixture_services(&fixture,USS_PAIR_ORDINARY_FIRST);fixture.publish_waits=1;
    begin_pair(&work,&services,&fixture,USS_PAIR_ORDINARY_FIRST,3,0);
    uss_pair_tick(&work,&services,100);assert(work.phase==USS_PAIR_ACTIVE && work.attachment_consumed && fixture.attachments==1 && !fixture.sends);
    complete_pair(&work,&services);assert(fixture.attachments==1 && fixture.publish_calls==2 && fixture.sends==1);
    services=fixture_services(&fixture,USS_PAIR_ORDINARY_FIRST);fixture.published_by_switch=1;
    begin_pair(&work,&services,&fixture,USS_PAIR_ORDINARY_FIRST,3,0);complete_pair(&work,&services);assert(fixture.sends==1);
    /* Cancel known stale/invalid evidence before any operation, not just a failed native result. */
    for(i=0;i<17;++i) {
        services=fixture_services(&fixture,USS_PAIR_DRAW);begin_pair(&work,&services,&fixture,USS_PAIR_DRAW,3,0);
        switch(i){
        case 0:fixture.state.epoch++;break;case 1:fixture.state.pair_generation++;break;
        case 2:fixture.state.context.row_address++;break;case 3:fixture.state.context.avatar++;break;
        case 4:fixture.state.utility.identity[0]^=1;break;case 5:fixture.state.ordinary.entity++;break;
        case 6:fixture.state.support=60;break;case 7:fixture.state.utility_parent=99;break;
        case 8:fixture.state.ordinary_parent=0;break;case 9:fixture.state.input=1;break;
        case 10:fixture.state.disabled=1;break;case 11:fixture.state.host=0;fixture.state.joined_client=1;break;
        case 12:fixture.state.mission=0;break;case 13:fixture.state.eligible=0;break;
        case 14:fixture.state.down=1;break;case 15:fixture.state.members_valid=0;break;
        case 16:fixture.state.formed=0;break;
        }
        uss_pair_tick(&work,&services,100);assert(!fixture.count);terminal_no_replay(&work,&services,&fixture);
    }
    for(i=0;i<7;++i) {
        services=fixture_services(&fixture,USS_PAIR_ORDINARY_FIRST);
        switch(i){case 0:fixture.select_stop=1;break;case 1:fixture.attach_stop=1;break;
        case 2:fixture.attach_wait=1;break;case 3:fixture.publish_stop=1;break;
        case 4:fixture.verify_ok=0;break;case 5:fixture.select_stop=3;break;case 6:fixture.change_after_attach=1;break;}
        begin_pair(&work,&services,&fixture,USS_PAIR_ORDINARY_FIRST,3,0);uss_pair_tick(&work,&services,100);
        terminal_no_replay(&work,&services,&fixture);
    }
    services=fixture_services(&fixture,USS_PAIR_ORDINARY_FIRST);fixture.ready_waits=2;
    begin_pair(&work,&services,&fixture,USS_PAIR_ORDINARY_FIRST,3,0);uss_pair_tick(&work,&services,100);
    fixture.state.active=2;uss_pair_tick(&work,&services,125);assert(fixture.selections==1);terminal_no_replay(&work,&services,&fixture);
    services=fixture_services(&fixture,USS_PAIR_ORDINARY_FIRST);begin_pair(&work,&services,&fixture,USS_PAIR_ORDINARY_FIRST,3,0);
    uss_pair_tick(&work,&services,2100);assert(!fixture.count);terminal_no_replay(&work,&services,&fixture);
    services=fixture_services(&fixture,USS_PAIR_DRAW);fixture.state.utility_parent=0;memset(&work,0,sizeof(work));
    assert(!uss_pair_begin(&work,&services,&fixture.state,USS_PAIR_DRAW,3,0,&fixture.state.utility,100));
    services=fixture_services(&fixture,USS_PAIR_ORDINARY_FIRST);memset(&work,0,sizeof(work));
    assert(!uss_pair_begin(&work,&services,&fixture.state,(UssPairOrder)99,3,0,NULL,100));
    services=fixture_services(&fixture,USS_PAIR_ORDINARY_FIRST);fixture.state.formation_pending=0;memset(&work,0,sizeof(work));
    assert(!uss_pair_begin(&work,&services,&fixture.state,USS_PAIR_ORDINARY_FIRST,3,0,NULL,100));
    assert(uss_pair_tick(NULL,&services,100)==USS_PAIR_IDLE && !fixture.count);
    services=fixture_services(&fixture,USS_PAIR_ORDINARY_FIRST);begin_pair(&work,&services,&fixture,USS_PAIR_ORDINARY_FIRST,3,0);
    assert(uss_pair_tick(&work,NULL,100)==USS_PAIR_CANCELLED && !fixture.count);
    services=fixture_services(&fixture,USS_PAIR_ORDINARY_FIRST);memset(&work,0,sizeof(work));
    assert(!uss_pair_begin(&work,&services,&fixture.state,USS_PAIR_ORDINARY_FIRST,3,2,NULL,100));
    assert(!uss_pair_begin(&work,&services,&fixture.state,USS_PAIR_ORDINARY_FIRST,3,0,NULL,UINT64_MAX-100));
    puts("pair coordinator: both formation orders/pickup settings/roles, draw directions, wait/consume/publication order, cancellation/no replay PASS");
    return 0;
}
