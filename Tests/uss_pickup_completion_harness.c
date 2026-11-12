#include <assert.h>
#include <stdio.h>
#include "../src/uss_pickup_completion.h"
static UssRefillTarget target(void)
{
    UssRefillTarget t={0};t.epoch=t.owner=t.inventory=t.ammo_manager=t.ammo_unit=1;t.avatar=2;t.target=3;t.real_support=4;
    t.kind=1;t.identity[0]=5;t.valid=t.eligible=t.stored=t.non_expendable=t.ordinary=1;return t;
}
int main(void)
{
    UssPickupCompletions state={0};UssRefillTarget t=target();UssPickupSource source={0};UssPickupAction action={0};UssRefillEvidence event={0};unsigned i;
    assert(uss_pickup_completion_bind(&state,&t));source.entity=10;source.identity[0]=9;source.time_ms=1000;
    source.kind[0]=source.kind[1]=4;source.scale[0]=source.scale[1]=0.5f;source.verified_zones=3;
    assert(uss_pickup_cache_source(&state,&source,1000) && !state.serial);
    action.valid=1;action.epoch=1;action.avatar=2;
    assert(!uss_pickup_publish_completion(&state,&action,1000,NULL,&event));
    assert(!strcmp(state.reason,"PICKUP_COMPLETION_BASELINE"));
    action.entity=10;assert(uss_pickup_publish_completion(&state,&action,1100,NULL,&event));
    assert(!strcmp(state.reason,"PICKUP_COMPLETION_PUBLISHED"));
    assert(event.source==10 && event.zone==0 && event.source_kind==4 && event.serial==1 && event.source_scale==0.5f);
    assert(!uss_pickup_publish_completion(&state,&action,1200,NULL,&event));
    assert(!strcmp(state.reason,"PICKUP_COMPLETION_UNCHANGED"));
    action.zone=1;assert(!uss_pickup_publish_completion(&state,&action,1200,NULL,&event)); /* Sibling zone spent; no live fallback provided. */
    assert(!strcmp(state.reason,"PICKUP_COMPLETION_SOURCE_UNAVAILABLE"));
    action.entity=20;assert(!uss_pickup_publish_completion(&state,&action,1300,NULL,&event) && state.activity_ms==1300);
    t.target+=2;assert(uss_pickup_completion_bind(&state,&t) && !state.have_previous && !state.activity_valid && state.serial==1);
    source.time_ms=2000;assert(uss_pickup_cache_source(&state,&source,2000));
    action.entity=0;action.zone=0;assert(!uss_pickup_publish_completion(&state,&action,2000,NULL,&event));
    action.entity=10;assert(!uss_pickup_publish_completion(&state,&action,3501,NULL,&event)); /* Expired cache can't grant. */
    source.time_ms=4000;assert(uss_pickup_cache_source(&state,&source,4000));action.entity=0;
    assert(!uss_pickup_publish_completion(&state,&action,4000,NULL,&event));action.entity=10;action.zone=8;
    assert(!uss_pickup_publish_completion(&state,&action,4100,NULL,&event));
    for(i=0;i<17;++i){source.entity=100+i;assert(uss_pickup_cache_source(&state,&source,4000));}
    assert(state.serial==1);action.epoch=2;assert(!uss_pickup_publish_completion(&state,&action,4200,NULL,&event) && !state.bound);
    puts("USS pickup classifier: cache never triggers, initial baseline, exact entity/zone, whole-source consumption, unknown activity, TTL/context and capacity PASS");return 0;
}
