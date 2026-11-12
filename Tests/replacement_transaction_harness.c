#define USS_HISTORY_TEST_MAIN history_regression_checks
#define history hf048_history
#include "pickup_history_harness.c"
#undef history
#include "../src/replacement_transaction.h"
static UssReplacementWork replacement_work;
static UssReplacementServices replacement_services;
static UssPublicationState replacement_publication;
static UssPickupDecision replacement_intent;
static UssReplacementOrigin replacement_origin;
static uint64_t replacement_now;
static unsigned replacement_selects,replacement_switches,replacement_drops,replacement_sends,ready_waits;
static unsigned stop_switch,stop_drop,stop_select,uncertain_send,publication_waits,omit_send,shape_change;
static unsigned trace_values[40],trace_count,empty_known;
static UssReplacementSnapshot last_snapshot;
static UssPairObservation last_pair;
static UssPublicationView last_view;
static UssPublicationOwner last_row;
static void record_trace(unsigned value){assert(trace_count<40);trace_values[trace_count++]=value;}
static UssReplacementSnapshot replacement_observe_fixture(void* user,const UssReplacementWork* work)
{
    UssReplacementSnapshot result={0};UssCoreCandidateNames names;UssCorePlatform platform={0};
    UssCoreFlags flags=uss_lifecycle_flags(&candidate_life,&core_flags);UssPairMember members[3];(void)user;
    members[0]=work->intent.incoming;members[1]=work->intent.kept;members[2]=work->intent.released;
    if(!uss_core_candidate_names(&candidate_life.owner,members,3,&names))return result;
    platform.thread_id=core_thread;
    result.bundle=uss_core_observe_candidates(&fake,MOD,&candidate_life.owner,&flags,&platform,&names,NULL);
    result.empty_valid=empty_known;result.native_empty=UINT32_MAX;
    if(shape_change)result.bundle.members[2].parent=99;
    last_snapshot=result;return result;
}
static int refresh_replacement(void)
{
    UssReplacementSnapshot snapshot=replacement_observe_fixture(NULL,&replacement_work);
    last_snapshot=snapshot;
    return uss_lifecycle_observe(&candidate_life,7,&snapshot.bundle.current,&last_pair,&last_view,&last_row);
}
static uint64_t replacement_clock_fixture(void* user){(void)user;return replacement_now;}
static UssPublicationView replacement_view_fixture(void* user)
{(void)user;assert(refresh_replacement());return last_view;}
static int replacement_capture_fixture(void* user,UssPublicationStage stage,UssPublicationCapture* captured)
{
    (void)user;(void)stage;
    /* Own service simulation, NOT the native dispatch capture producer. Actual
     * coherent Core reads still bracket each stage; binding capture is future work. */
    if(publication_waits){--publication_waits;return 0;}
    assert(refresh_replacement());memset(captured,0,sizeof(*captured));
    captured->valid=1;captured->key=last_view.key;captured->owner=last_row;captured->thread=7;
    captured->fingerprint_size=48;memcpy(captured->fingerprint,last_row.row,48);
    captured->wires[0]=22;captured->wires[1]=101;captured->wires[2]=candidate_life.owner.utility.entity;
    captured->wires[3]=candidate_life.owner.ordinary.entity;return 1;
}
static UssPublicationSend replacement_send_fixture(void* user,UssPublicationKind kind,const UssPublicationCapture* captured,uint64_t metadata)
{
    (void)user;(void)kind;(void)metadata;assert(captured->valid);++replacement_sends;
    record_trace(1000+publication_u32(captured->owner.row+8));
    return uncertain_send?USS_PUBLICATION_SEND_UNCERTAIN:USS_PUBLICATION_SEND_RETURNED;
}
static UssPublicationResult publish_fixture(UssPublicationStage stage)
{
    UssPublicationServices services={0};UssPublicationOwner row;
    services.clock_ms=replacement_clock_fixture;services.view=replacement_view_fixture;
    services.capture=replacement_capture_fixture;services.send=replacement_send_fixture;
    assert(refresh_replacement());row=last_row;
    return uss_publication_boundary(&replacement_publication,&services,&row,0,stage);
}
static void set_selected(uint32_t slot)
{
    uint32_t support=u32(fixture(0x70000300,48)+8);put32(fixture(0x70000300,48)+28,slot);
    put32(fixture(0x70000500,4),slot==3?support:slot==2?105:101);put32(fixture(ACTION_MANAGER+0x53DCD0,32)+4,slot);
}
static UssPairServiceResult replacement_select_fixture(void* user,const UssReplacementSnapshot* expected,uint32_t slot)
{
    (void)user;assert(expected->bundle.current.valid);++replacement_selects;record_trace(2000+slot);
    if(stop_select==replacement_selects)return USS_PAIR_SERVICE_STOP;set_selected(slot);return USS_PAIR_SERVICE_DONE;
}
static UssPairServiceResult replacement_ready_fixture(void* user,const UssReplacementWork* work,const UssReplacementSnapshot* snapshot,uint64_t now)
{
    (void)user;if(ready_waits){--ready_waits;return USS_PAIR_SERVICE_WAIT;}
    return uss_replacement_primary_ready(work,snapshot,now)?USS_PAIR_SERVICE_DONE:USS_PAIR_SERVICE_STOP;
}
static void attach_fixture(const UssPairMember* member)
{
    put32(fixture(0x70000300,48)+8,member->entity);
    put32(fixture(0x70000700,240)+(member->entity==104?144:member->entity==103?96:48)+4,22);
}
static UssPairServiceResult replacement_switch_fixture(void* user,const UssReplacementSnapshot* expected,const UssPairMember* member,UssPublicationStage stage)
{
    UssPublicationResult sent;(void)user;assert(expected->bundle.current.valid);++replacement_switches;record_trace(member->entity);
    if(stop_switch==replacement_switches)return USS_PAIR_SERVICE_STOP;
    attach_fixture(member);
    if(omit_send)return USS_PAIR_SERVICE_DONE;
    sent=publish_fixture(stage);return sent.outcome==USS_PUBLICATION_RETURNED || sent.outcome==USS_PUBLICATION_UNCHANGED?USS_PAIR_SERVICE_DONE:USS_PAIR_SERVICE_STOP;
}
static UssPairServiceResult replacement_drop_fixture(void* user,const UssReplacementSnapshot* expected)
{
    (void)user;assert(expected->bundle.current.inventory.slots[2]==replacement_work.intent.released.entity);
    ++replacement_drops;record_trace(3000+replacement_work.intent.released.entity);
    if(stop_drop)return USS_PAIR_SERVICE_STOP;
    put32(fixture(0x70000300,48)+8,UINT32_MAX);
    put32(fixture(0x70000700,240)+(replacement_work.intent.released.entity==103?96:48)+4,0);return USS_PAIR_SERVICE_DONE;
}
static UssPairObservation retained_observe_fixture(void* user)
{(void)user;assert(refresh_replacement());return last_pair;}
static UssPairServiceResult retained_select_fixture(void* user,const UssPairObservation* expected,uint32_t slot)
{(void)expected;assert(refresh_replacement());return replacement_select_fixture(user,&last_snapshot,slot);}
static UssPairServiceResult retained_ready_fixture(void* user,const UssPairObservation* expected)
{(void)user;(void)expected;assert(refresh_replacement());return last_snapshot.bundle.current.primary.ready?USS_PAIR_SERVICE_DONE:USS_PAIR_SERVICE_WAIT;}
static UssPairServiceResult retained_switch_fixture(void* user,const UssPairObservation* expected,const UssPairStep* step,unsigned defer_ack)
{
    (void)user;(void)expected;(void)defer_ack;assert(refresh_replacement());++replacement_switches;
    record_trace(step->member.entity);if(stop_switch==replacement_switches)return USS_PAIR_SERVICE_STOP;
    attach_fixture(&step->member);return USS_PAIR_SERVICE_DONE;
}
static UssPairServiceResult retained_publish_fixture(void* user,const UssPairObservation* expected,const UssPairStep* step)
{
    UssPublicationResult sent;(void)user;(void)expected;(void)step;sent=publish_fixture(USS_PUBLICATION_POST_ATTACH);
    if(sent.outcome==USS_PUBLICATION_REFUSED && sent.reason==USS_PUB_CAPTURE)return USS_PAIR_SERVICE_WAIT;
    return sent.outcome==USS_PUBLICATION_RETURNED || sent.outcome==USS_PUBLICATION_UNCHANGED?USS_PAIR_SERVICE_DONE:USS_PAIR_SERVICE_STOP;
}
static int retained_verify_fixture(void* user,const UssPairObservation* expected)
{(void)user;(void)expected;return refresh_replacement() && last_snapshot.bundle.members[1].parent==22 && last_snapshot.bundle.members[0].parent==22;}
static void replacement_fixture(unsigned utility,unsigned pickup,unsigned selected,unsigned role)
{
    UssCoreCandidates bundle;UssPairObservation pair;UssPublicationView view;UssPublicationOwner row;
    UssCoreObservation current;UssCorePlatform platform={0};UssCoreFlags flags;
    candidate_fixture_role(1,role);extra_candidate();
    /* A real secondary descriptor/held entity, not a selected-index-only stand-in. */
    {unsigned i;unsigned char* header=fixture(0x60000000,72);unsigned char* rows;
        put32(header,5);put32(header+12,5);put32(header+40,8);
        rows=fixture(0x70000800,64);memset(rows,0xff,64);
        for(i=0;i<5;++i){unsigned id=101+i;put32(rows+(id&7)*8,id);put32(rows+(id&7)*8+4,i);}
        putptr(fixture(0x70000600,40)+32,0x70004900);
        rows=segment(0x70004900,24);memset(rows,4,8);put32(rows+8,105);
        rows=fixture(0x70000700,240);put32(rows+192+4,22);put32(rows+192+12,1);
        put32(fixture(0x70000300,48)+4,105);
    }memset(&hf048_history,0,sizeof(hf048_history));preference.known=1;preference.switch_on_pickup=pickup;
    if(!utility)memset(fixture(0x70003900,24),9,8);
    /* Context owns the secondary identity too; bootstrap after adding it. */
    memset(&candidate_life,0,sizeof(candidate_life));recover_scope();
    {UssPairMember u=candidate_id(103),o=candidate_id(102);
        assert(uss_pair_owner_set_pair(&candidate_life.owner,7,0,&u,&o));
        candidate_life.generation=candidate_life.owner.pair_generation;candidate_life.phase=USS_LIFE_FORMED;}
    memset(&replacement_work,0,sizeof(replacement_work));memset(&replacement_services,0,sizeof(replacement_services));memset(&replacement_publication,0,sizeof(replacement_publication));
    replacement_origin.known=1;replacement_origin.real=utility?102:103;replacement_origin.selected=selected;
    replacement_origin.utility_drawn=selected==3 && !utility;
    replacement_origin.epoch=candidate_life.owner.epoch;replacement_origin.context_generation=candidate_life.owner.context_generation;
    replacement_origin.pair_generation=candidate_life.owner.pair_generation;replacement_origin.guard_generation=candidate_life.owner.guard_generation;
    /* Seed the existing lifetime's returned publisher record before pickup. */
    put32(fixture(0x70000300,48)+8,replacement_origin.real);set_selected(1);
    flags=uss_lifecycle_flags(&candidate_life,&core_flags);platform.thread_id=core_thread;
    current=uss_core_observe(&fake,MOD,&candidate_life.owner,&flags,&platform,NULL);
    assert(uss_lifecycle_observe(&candidate_life,7,&current,&pair,&view,&row) && view.supported_pair);
    replacement_publication.record=view.key;replacement_publication.record_present=replacement_publication.formed=1;
    replacement_publication.member=replacement_origin.real;replacement_publication.game_thread=7;
    replacement_publication.attempted=replacement_publication.returned=9;replacement_publication.used=9;replacement_publication.rate_until=10000;
    put32(fixture(0x70000700,240)+(utility?48:96)+4,0);
    bundle=read_support(104,pickup?3:selected);set_selected(pickup?3:selected);bundle=candidates();replacement_intent=step(&bundle);assert(replacement_intent.kind==USS_PICKUP_REPLACEMENT_INTENT);
    replacement_work.intent=replacement_intent;
    replacement_services.observe=replacement_observe_fixture;replacement_services.select_slot=replacement_select_fixture;
    replacement_services.primary_ready=replacement_ready_fixture;replacement_services.switch_member=replacement_switch_fixture;replacement_services.drop_slot=replacement_drop_fixture;
    replacement_services.retained=(UssPairServices){NULL,retained_observe_fixture,retained_select_fixture,retained_ready_fixture,retained_switch_fixture,retained_publish_fixture,retained_verify_fixture,NULL};
    replacement_selects=replacement_switches=replacement_drops=replacement_sends=ready_waits=0;
    stop_switch=stop_drop=stop_select=uncertain_send=publication_waits=omit_send=shape_change=0;empty_known=1;trace_count=0;replacement_now=100;
    memset(&replacement_work,0,sizeof(replacement_work));
}
static int begin_replacement(void)
{return uss_replacement_begin(&replacement_work,&candidate_life,7,&hf048_history,&replacement_intent,&replacement_origin,&replacement_publication,&replacement_services,replacement_now);}
static UssReplacementPhase tick_replacement(uint64_t tick)
{return uss_replacement_tick(&replacement_work,&candidate_life,7,&hf048_history,&replacement_publication,&replacement_services,replacement_now,tick);}
static void complete_replacement(void)
{
    unsigned i;for(i=0;i<30 && replacement_work.phase==USS_REPLACE_ACTIVE;++i){replacement_now+=25;tick_replacement(replacement_work.has_tick?replacement_work.last_tick+1:1);}
    if(replacement_work.phase!=USS_REPLACE_COMPLETE)fprintf(stderr,"Replacement failed stage=%u reason=%s switches=%u drops=%u sends=%u\n",replacement_work.stage,replacement_work.reason,replacement_switches,replacement_drops,replacement_sends);
    assert(replacement_work.phase==USS_REPLACE_COMPLETE);
}
#ifndef USS_REPLACEMENT_TEST_MAIN
#define USS_REPLACEMENT_TEST_MAIN main
#endif
int USS_REPLACEMENT_TEST_MAIN(void)
{
    unsigned utility,pickup,selected,role;uint64_t generation;
    assert(history_regression_checks()==0);
    for(role=HF_SESSION_HOST;role<=HF_SESSION_JOINER;++role)for(utility=0;utility<2;++utility)
    for(pickup=0;pickup<2;++pickup)for(selected=1;selected<=3;++selected){
        replacement_fixture(utility,pickup,selected,role);generation=candidate_life.owner.pair_generation;
        assert(begin_replacement());assert(candidate_life.phase==USS_LIFE_REPLACING && candidate_life.owner.pair_generation==generation);
        assert(!replacement_publication.formed && replacement_publication.attempted==9 && replacement_publication.used==9);
        complete_replacement();assert(candidate_life.owner.pair_generation==generation+1 && candidate_life.phase==USS_LIFE_FORMED);
        assert(replacement_work.real==(pickup?104:replacement_intent.kept.entity));assert(replacement_work.selected==(pickup?3:selected));
        assert(replacement_switches==(pickup?3u:4u) && replacement_drops==1);
        assert(replacement_publication.attempted==9+replacement_sends);
        assert(replacement_work.utility_drawn==(unsigned)((utility && pickup) || (!utility && !pickup && selected==3)));
    }
    /* Readiness WAITs consume neither attachment nor selection again. */
    replacement_fixture(0,0,3,HF_SESSION_HOST);ready_waits=3;assert(begin_replacement());
    replacement_now+=25;assert(tick_replacement(1)==USS_REPLACE_ACTIVE && replacement_selects==1 && !replacement_switches);
    {unsigned calls=reads;assert(tick_replacement(1)==USS_REPLACE_ACTIVE && reads==calls);}
    replacement_now+=25;tick_replacement(2);assert(!replacement_switches);
    complete_replacement();
    /* Each uncertain effect is consumed and terminal; completion cannot be replayed. */
    for(utility=1;utility<=4;++utility){
        replacement_fixture(0,0,3,HF_SESSION_HOST);stop_switch=utility;assert(begin_replacement());
        for(selected=0;selected<30 && replacement_work.phase==USS_REPLACE_ACTIVE;++selected){replacement_now+=25;tick_replacement(selected+1);}
        assert(replacement_work.phase==USS_REPLACE_CANCELLED && replacement_switches==utility);
        generation=replacement_switches;assert(tick_replacement(40)==USS_REPLACE_CANCELLED && replacement_switches==generation);
        assert(!begin_replacement());
    }
    replacement_fixture(0,0,3,HF_SESSION_HOST);stop_drop=1;assert(begin_replacement());
    for(selected=0;selected<20 && replacement_work.phase==USS_REPLACE_ACTIVE;++selected){replacement_now+=25;tick_replacement(selected+1);}
    assert(replacement_work.phase==USS_REPLACE_CANCELLED && replacement_drops==1 && (replacement_work.consumed_mask&8));
    for(utility=1;utility<=2;++utility){
        replacement_fixture(1,0,3,HF_SESSION_HOST);stop_select=utility;assert(begin_replacement());
        for(selected=0;selected<25 && replacement_work.phase==USS_REPLACE_ACTIVE;++selected){replacement_now+=25;tick_replacement(selected+1);}
        assert(replacement_work.phase==USS_REPLACE_CANCELLED && replacement_selects==utility);
    }
    replacement_fixture(0,0,3,HF_SESSION_HOST);uncertain_send=1;assert(begin_replacement());
    for(selected=0;selected<20 && replacement_work.phase==USS_REPLACE_ACTIVE;++selected){replacement_now+=25;tick_replacement(selected+1);}
    assert(replacement_work.phase==USS_REPLACE_CANCELLED && replacement_publication.disabled && replacement_sends==1 && !replacement_drops);
    /* A service claiming success without publication cannot progress to drop. */
    replacement_fixture(1,0,1,HF_SESSION_HOST);omit_send=1;assert(begin_replacement());
    for(selected=0;selected<20 && replacement_work.phase==USS_REPLACE_ACTIVE;++selected){replacement_now+=25;tick_replacement(selected+1);}
    assert(replacement_work.phase==USS_REPLACE_CANCELLED && !replacement_drops && candidate_life.owner.pair_generation==replacement_intent.pair_generation);
    /* Retained publication wait after attachment does not repeat that attachment. */
    replacement_fixture(0,0,2,HF_SESSION_HOST);assert(begin_replacement());
    for(selected=1;replacement_work.stage!=USS_REPLACE_RETAINED_TICK && selected<20;++selected){replacement_now+=25;tick_replacement(selected);}
    assert(replacement_work.stage==USS_REPLACE_RETAINED_TICK);publication_waits=2;
    replacement_now+=25;tick_replacement(30);assert(replacement_switches==4 && replacement_work.retained.attachment_consumed);
    replacement_now+=25;tick_replacement(31);assert(replacement_switches==4);
    for(selected=32;replacement_work.phase==USS_REPLACE_ACTIVE && selected<45;++selected){replacement_now+=25;tick_replacement(selected);}
    assert(replacement_work.phase==USS_REPLACE_COMPLETE && replacement_switches==4 && replacement_work.selected==2);
    /* Source-derived phase2 admission is bounded to the replacement lifetime. */
    replacement_fixture(0,0,1,HF_SESSION_HOST);assert(begin_replacement());
    {UssReplacementSnapshot snap=replacement_observe_fixture(NULL,&replacement_work);UssPrimaryReadiness* primary=&snap.bundle.current.primary;
        primary->ready=0;primary->bit47=1;put32(primary->fields.flags+12,1u<<15);put32(primary->fields.row+8,3);put32(primary->fields.row+16,2);
        put32(primary->fields.row+20,1);put32(primary->fields.row+24,0);put32(primary->fields.row+28,0x3f800000);
        assert(uss_replacement_primary_ready(&replacement_work,&snap,replacement_now));
        assert(!uss_replacement_primary_ready(&replacement_work,&snap,replacement_work.primary_deadline));
        primary->fields.state_byte=1;assert(!uss_replacement_primary_ready(&replacement_work,&snap,replacement_now));}
    /* No new lifetime before verified old detachment/empty-slot handover. */
    replacement_fixture(0,0,1,HF_SESSION_HOST);assert(begin_replacement());generation=candidate_life.owner.pair_generation;
    for(selected=1;replacement_work.stage!=USS_REPLACE_HANDOVER && selected<20;++selected){replacement_now+=25;tick_replacement(selected);}
    assert(replacement_work.stage==USS_REPLACE_HANDOVER && candidate_life.owner.pair_generation==generation);
    empty_known=0;replacement_now+=25;assert(tick_replacement(25)==USS_REPLACE_CANCELLED && candidate_life.owner.pair_generation==generation);
    /* Stale intent/origin/publication and forged role proposals refuse before effects. */
    replacement_fixture(1,0,1,HF_SESSION_HOST);replacement_origin.real=104;assert(!begin_replacement() && !replacement_switches);
    replacement_fixture(1,0,1,HF_SESSION_HOST);replacement_origin.pair_generation++;assert(!begin_replacement());
    replacement_fixture(1,0,1,HF_SESSION_HOST);replacement_intent.kept=replacement_intent.released;assert(!begin_replacement());
    replacement_fixture(1,0,1,HF_SESSION_HOST);replacement_publication.record.pair_generation++;assert(!begin_replacement());
    replacement_fixture(1,0,1,HF_SESSION_HOST);replacement_publication.disabled=1;assert(!begin_replacement() && replacement_publication.disabled);
    replacement_fixture(1,0,1,HF_SESSION_HOST);assert(begin_replacement());hf048_history.last_serial++;
    replacement_now+=25;assert(tick_replacement(1)==USS_REPLACE_CANCELLED && !replacement_switches);
    replacement_fixture(1,0,1,HF_SESSION_HOST);assert(begin_replacement());core_flags.input=1;
    replacement_now+=25;assert(tick_replacement(1)==USS_REPLACE_CANCELLED && !replacement_switches);
    replacement_fixture(1,0,1,HF_SESSION_HOST);assert(begin_replacement());fail_at=reads+1;
    replacement_now+=25;assert(tick_replacement(1)==USS_REPLACE_CANCELLED && !replacement_switches);
    replacement_fixture(1,0,1,HF_SESSION_HOST);assert(begin_replacement());shape_change=1;
    replacement_now+=25;assert(tick_replacement(1)==USS_REPLACE_CANCELLED);
    replacement_fixture(1,0,1,HF_SESSION_HOST);assert(begin_replacement());replacement_now=replacement_work.deadline_ms;
    assert(tick_replacement(1)==USS_REPLACE_CANCELLED && !replacement_selects && !replacement_switches);
    /* Identity drift, lifecycle cancellation and mission disposal cancel old work. */
    replacement_fixture(1,0,1,HF_SESSION_HOST);assert(begin_replacement());fixture(0x70003900,24)[0]^=1;
    replacement_now+=25;assert(tick_replacement(1)==USS_REPLACE_CANCELLED && !replacement_switches);
    replacement_fixture(1,0,1,HF_SESSION_HOST);assert(begin_replacement());candidate_life.phase=USS_LIFE_PAUSED;
    replacement_now+=25;assert(tick_replacement(1)==USS_REPLACE_CANCELLED && !replacement_switches);
    replacement_fixture(1,0,1,HF_SESSION_HOST);assert(begin_replacement());
    assert(uss_pair_owner_update(&candidate_life.owner,7,1,0,NULL));candidate_life.phase=USS_LIFE_EMPTY;
    replacement_now+=25;assert(tick_replacement(1)==USS_REPLACE_CANCELLED && candidate_life.phase==USS_LIFE_EMPTY);
    /* New-member publication failure after handover retains the new lifetime,
     * paused and unformed, instead of silently reverting ownership. */
    replacement_fixture(1,0,1,HF_SESSION_HOST);assert(begin_replacement());generation=candidate_life.owner.pair_generation;
    for(selected=1;replacement_work.stage!=USS_REPLACE_RESTORE_NEW && selected<20;++selected){replacement_now+=25;tick_replacement(selected);}
    assert(replacement_work.stage==USS_REPLACE_RESTORE_NEW && candidate_life.owner.pair_generation==generation+1);
    omit_send=1;replacement_now+=25;tick_replacement(30);replacement_now+=25;tick_replacement(31);
    assert(replacement_work.phase==USS_REPLACE_CANCELLED && candidate_life.phase==USS_LIFE_PAUSED && candidate_life.owner.pair_generation==generation+1);
    puts("Replacement:24role/type/preference/origin cases, coherent real reads, handover, retained coordinator, wait/uncertain/stale/context failures PASS");return 0;
}
