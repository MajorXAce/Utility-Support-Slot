#include "../src/uss_active_replacement.h"
#define USS_HISTORY_TEST_MAIN active_history_regressions
#include "pickup_history_harness.c"
static UssPickupDecision intent;static UssReplacementOrigin origin;static UssPublicationState publication;static UssCoreCandidates active_bundle;
static void active_fixture(unsigned utility,unsigned pickup,unsigned selected)
{
    UssCoreCandidates before;UssPairObservation pair;UssPublicationView view;UssPublicationOwner row;UssPairMember names[3];unsigned char* descriptor;
    history_fixture(1);extra_candidate();
    if(!utility){descriptor=fixture(0x70003900,24);memset(descriptor,0,8);descriptor[0]=8;}
    before=read_support(utility?103:102,selected);assert(step(&before).kind==USS_PICKUP_MANAGED);
    assert(uss_lifecycle_observe(&candidate_life,7,&before.current,&pair,&view,&row));
    memset(&publication,0,sizeof(publication));publication.formed=publication.record_present=1;publication.record=view.key;
    memset(&origin,0,sizeof(origin));origin.known=1;origin.real=utility?103:102;origin.selected=selected;origin.utility_drawn=utility && selected==3;
    origin.epoch=candidate_life.owner.epoch;origin.context_generation=candidate_life.owner.context_generation;
    origin.pair_generation=candidate_life.owner.pair_generation;origin.guard_generation=candidate_life.owner.guard_generation;
    put32(fixture(0x70000700,192)+(utility?96:48)+4,0);
    preference.switch_on_pickup=pickup;before=read_support(104,pickup?3u:selected);intent=step(&before);
    assert(intent.kind==USS_PICKUP_REPLACEMENT_INTENT);
    names[0]=utility?intent.incoming:intent.kept;names[1]=utility?intent.kept:intent.incoming;names[2]=intent.released;
    assert(uss_core_candidate_names(&candidate_life.owner,names,3,&request));active_bundle=candidates();assert(active_bundle.valid);
}
int main(void)
{
    unsigned utility,pickup,selected;uint64_t generation;(void)active_history_regressions;
    for(utility=0;utility<=1;++utility)for(pickup=0;pickup<=1;++pickup)for(selected=1;selected<=3;selected+=2){
        active_fixture(utility,pickup,selected);generation=candidate_life.owner.pair_generation;
        assert(uss_active_replacement_prepare(&candidate_life,7,&history,&intent,&origin,&active_bundle,&publication));
        assert(candidate_life.owner.pair_generation==generation+1 && candidate_life.phase==USS_LIFE_PENDING && !publication.formed);
        assert(candidate_life.plan.selected==(utility?((pickup || selected==3)?3u:selected):(pickup?3u:selected)));
        assert(candidate_life.plan.switch_on_pickup==(unsigned)(utility && (pickup || selected==3)));
        assert(candidate_life.owner.utility.entity==(utility?104u:103u) && candidate_life.owner.ordinary.entity==(utility?102u:104u));
    }
    active_fixture(1,0,3);active_bundle.members[2].parent=22;generation=candidate_life.owner.pair_generation;
    assert(!uss_active_replacement_prepare(&candidate_life,7,&history,&intent,&origin,&active_bundle,&publication));
    assert(candidate_life.phase==USS_LIFE_FORMED && candidate_life.owner.pair_generation==generation && publication.formed);
    active_fixture(0,1,1);++origin.guard_generation;
    assert(!uss_active_replacement_prepare(&candidate_life,7,&history,&intent,&origin,&active_bundle,&publication));
    puts("USS active replacement: both classes/settings/primary-support origins, verified detached-old handover, selection, generation and stale/attached refusal; no native effect PASS");return 0;
}
