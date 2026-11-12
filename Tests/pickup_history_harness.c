#ifdef NDEBUG
#error Pickup-history assertions required
#endif
#define USS_CANDIDATE_TEST_MAIN candidate_regression_checks
#include "core_candidates_harness.c"
#include "../src/pickup_history.h"
static UssPickupHistory history;
static UssPickupPolicy preference;
static void history_fixture(unsigned owned)
{
    candidate_fixture(owned);memset(&history,0,sizeof(history));preference.known=1;preference.switch_on_pickup=0;
    put32(fixture(0x70000700,144)+48+4,22);
}
static UssCoreCandidates read_support(uint32_t id,unsigned active)
{
    UssPairMember member=candidate_id(id);
    put32(fixture(0x70000300,48)+8,id);put32(fixture(0x70000300,48)+28,active);
    put32(fixture(0x70000500,4),active==3?id:101);put32(fixture(ACTION_MANAGER+0x53DCD0,32)+4,active);
    assert(uss_core_candidate_names(&candidate_life.owner,&member,1,&request));return candidates();
}
static UssPickupDecision step(const UssCoreCandidates* bundle)
{return uss_pickup_step(&history,&candidate_life,7,bundle,&preference);}
static UssPickupDecision complete_read(UssPickupDecision decision)
{
    UssCoreCandidates bundle;assert(decision.kind==USS_PICKUP_NEED_READ);request=decision.needed;
    bundle=candidates();assert(bundle.valid);return step(&bundle);
}
static void recover_scope(void)
{
    UssCorePlatform platform={0};UssCoreFlags flags=uss_lifecycle_flags(&candidate_life,&core_flags);
    UssCoreObservation current;UssPairObservation observation;UssPublicationView view;UssPublicationOwner row;
    platform.thread_id=core_thread;current=uss_core_observe(&fake,MOD,&candidate_life.owner,&flags,&platform,NULL);
    assert(current.valid && uss_lifecycle_observe(&candidate_life,7,&current,&observation,&view,&row));
}
#ifndef USS_HISTORY_TEST_MAIN
#define USS_HISTORY_TEST_MAIN main
#endif
int USS_HISTORY_TEST_MAIN(void)
{
    UssCoreCandidates bundle,stale;UssPickupDecision decision,old,newer;unsigned pickup,order,i;uint64_t generation;
    assert(candidate_regression_checks()==0);
    for(order=0;order<2;++order)for(pickup=0;pickup<2;++pickup){
        history_fixture(0);preference.switch_on_pickup=pickup;
        bundle=read_support(order?103:102,1);decision=step(&bundle);assert(decision.kind==USS_PICKUP_REMEMBERED);
        for(i=0;i<3;++i){bundle=read_support(order?103:102,1);assert(step(&bundle).kind==USS_PICKUP_REMEMBERED);}
        assert(!candidate_life.owner.pair_present);
        put32(fixture(0x70000700,144)+(order?96:48)+4,0);
        bundle=read_support(order?102:103,pickup?3:1);decision=step(&bundle);
        assert(decision.kind==USS_PICKUP_NEED_READ && history.remembered);
        decision=complete_read(decision);assert(decision.kind==USS_PICKUP_PAIR_PENDING);
        assert(candidate_life.owner.utility.entity==103 && candidate_life.owner.ordinary.entity==102);
        assert(candidate_life.phase==USS_LIFE_PENDING && !uss_lifecycle_flags(&candidate_life,&core_flags).formed);
        assert(decision.selected==(pickup?3u:1u));generation=candidate_life.owner.pair_generation;
        bundle=read_support(order?102:103,pickup?3:1);assert(step(&bundle).kind==USS_PICKUP_WAIT_WORK);
        assert(candidate_life.owner.pair_generation==generation);
    }
    /* An arbitrary two-member initial snapshot must not infer pickup history. */
    history_fixture(0);bundle=candidates();assert(step(&bundle).kind==USS_PICKUP_REMEMBERED);
    assert(!candidate_life.owner.pair_present);
    /* Both same-type replacement directions before formation replace memory only. */
    for(order=0;order<2;++order){
        history_fixture(0);extra_candidate();
        if(order)memset(fixture(0x70003900,24),9,8);
        bundle=read_support(order?102:103,1);assert(step(&bundle).kind==USS_PICKUP_REMEMBERED);
        bundle=read_support(104,1);assert(step(&bundle).kind==USS_PICKUP_REMEMBERED && history.last.entity==104);
        put32(fixture(0x70000700,192)+144+4,0);
        bundle=read_support(order?103:102,1);decision=complete_read(step(&bundle));
        assert(decision.kind==USS_PICKUP_PAIR_PENDING);
        assert((order?candidate_life.owner.ordinary.entity:candidate_life.owner.utility.entity)==104);
    }
    history_fixture(0);bundle=read_support(102,1);step(&bundle);stale=bundle;
    fail_at=reads+1;bundle=candidates();assert(!bundle.valid && step(&bundle).kind==USS_PICKUP_UNKNOWN && history.remembered);
    fail_at=0;recover_scope();assert(step(&stale).kind==USS_PICKUP_UNKNOWN && !candidate_life.owner.pair_present);
    put32(fixture(0x70000700,144)+48+4,0);bundle=read_support(103,1);decision=complete_read(step(&bundle));
    assert(decision.kind==USS_PICKUP_PAIR_PENDING);
    history_fixture(0);bundle=read_support(103,1);step(&bundle);preference.known=0;
    put32(fixture(0x70000700,144)+96+4,0);bundle=read_support(102,1);
    assert(complete_read(step(&bundle)).kind==USS_PICKUP_PAIR_PENDING);
    history_fixture(0);bundle=read_support(102,1);step(&bundle);
    memcpy(fixture(0x70001900,24),fixture(0x70002900,24),8);bundle=read_support(102,1);
    assert(step(&bundle).kind==USS_PICKUP_REMEMBERED && history.last_kind==USS_UTILITY_SUPPORT && !candidate_life.owner.pair_present);
    history_fixture(0);bundle=read_support(102,1);step(&bundle);preference.known=0;
    bundle=read_support(103,1);assert(step(&bundle).kind==USS_PICKUP_WAIT_POLICY && history.last.entity==102);
    preference.known=1;core_flags.input=1;bundle=read_support(103,1);assert(step(&bundle).kind==USS_PICKUP_REFUSED);
    history_fixture(0);bundle=read_support(102,1);step(&bundle);core_flags.down=1;
    bundle=read_support(102,1);assert(step(&bundle).kind==USS_PICKUP_REFUSED && !history.remembered);
    /* Confirmed empty support/mission exit clears lone history, never restores. */
    history_fixture(0);bundle=read_support(102,1);step(&bundle);
    put32(fixture(0x70000300,48)+8,0);bundle=candidates();assert(bundle.valid && step(&bundle).kind==USS_PICKUP_RESET && !history.remembered);
    history_fixture(0);bundle=read_support(102,1);step(&bundle);core_state=HF_STATE_SHIP;
    put32(fixture(0xA0000000+0xAC21C,4),3);bundle=candidates();assert(step(&bundle).kind==USS_PICKUP_RESET && !history.bound);
    history_fixture(0);bundle=read_support(102,1);step(&bundle);
    putptr(fixture(BASE+0x346BF98,8),MAPPING+8);recover_scope();bundle=read_support(103,1);
    assert(step(&bundle).kind==USS_PICKUP_REMEMBERED && !candidate_life.owner.pair_present);
    /* Third pickup produces typed, supersedable intent, not membership/effects. */
    history_fixture(1);extra_candidate();bundle=read_support(104,1);generation=candidate_life.owner.pair_generation;
    old=step(&bundle);assert(old.kind==USS_PICKUP_REPLACEMENT_INTENT && old.released.entity==103 && old.kept.entity==102 && old.restore_kept);
    assert(candidate_life.phase==USS_LIFE_FORMED && candidate_life.owner.utility.entity==103 && candidate_life.owner.pair_generation==generation);
    newer=old;newer.context.local.owner++;assert(!uss_pickup_intent_current(&history,&candidate_life,&newer));
    assert(uss_pickup_intent_current(&history,&candidate_life,&old));assert(step(&bundle).kind==USS_PICKUP_WAIT_WORK);
    memset(fixture(0x70003900,24),9,8);bundle=read_support(104,1);newer=step(&bundle);
    assert(newer.kind==USS_PICKUP_REPLACEMENT_INTENT && newer.released.entity==102 && newer.kept.entity==103);
    assert(!uss_pickup_intent_current(&history,&candidate_life,&old) && uss_pickup_intent_current(&history,&candidate_life,&newer));
    assert(uss_lifecycle_replace_begin_candidates(&candidate_life,7,&bundle,newer.selected,0));
    assert(step(&bundle).kind==USS_PICKUP_WAIT_WORK && candidate_life.owner.pair_generation==generation);
    history_fixture(1);bundle=read_support(102,1);assert(step(&bundle).kind==USS_PICKUP_MANAGED);
    put32(fixture(0x70000300,48)+8,0);bundle=candidates();assert(step(&bundle).kind==USS_PICKUP_RECOVERY_REQUIRED && candidate_life.owner.pair_present);
    history_fixture(1);extra_candidate();history.serial=UINT64_MAX;bundle=read_support(104,1);
    assert(step(&bundle).kind==USS_PICKUP_REFUSED && history.disabled);
    puts("Pickup history: both orders/preferences, same-type replacement memory, no poll inference, unknown/reset guards and typed supersedable intents PASS");return 0;
}
