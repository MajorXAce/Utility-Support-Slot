#ifdef NDEBUG
#error Pair owner tests require assertions
#endif
#include "../src/pair_owner.h"
#include <assert.h>
#include <stdio.h>

static UssPairOwner owner;
static UssPairOwnerContext context;
static UssPairMember utility,ordinary;
static UssPairObservation observed;
static UssPublicationOwner row;
static UssPublicationKey key;
static unsigned effects,sends;
static UssPublicationView pub_view;
static UssPublicationCapture pub_capture;
static void word(void* out,uint32_t value){memcpy(out,&value,4);}
static void setup(void)
{
    static const unsigned char silo[8]={0xaa,0x79,0xca,0x92,0x05,0x46,0x13,0xfc};
    memset(&owner,0,sizeof(owner));memset(&context,0,sizeof(context));
    memset(&observed,0,sizeof(observed));memset(&row,0,sizeof(row));memset(&key,0,sizeof(key));
    utility.entity=22;memcpy(utility.identity,silo,8);ordinary.entity=23;memset(ordinary.identity,0,8);ordinary.identity[0]=99;
    context.local=(UssPairContext){0x10000,0x20000,0x30000,10,11,12,13};
    context.session=0x40000;context.mapping=0x50000;context.role=1;context.peer_size=16;
    context.peers[0]=1;context.peers[8]=2;
    assert(uss_pair_owner_update(&owner,7,1,1,&context));
    assert(uss_pair_owner_set_pair(&owner,7,0,&utility,&ordinary));
    observed.valid=observed.mission=observed.host=observed.eligible=observed.formed=observed.members_valid=observed.can_switch=1;
    observed.context=context.local;observed.utility=utility;observed.ordinary=ordinary;
    observed.utility_parent=observed.ordinary_parent=10;observed.support=23;observed.active=3;
    row.owner=context.local.owner;row.inventory=context.local.inventory;row.address=context.local.row_address;row.avatar=10;
    word(row.row,11);word(row.row+4,12);word(row.row+8,23);word(row.row+12,13);word(row.row+28,1);
    key.session=context.session;key.mapping=context.mapping;key.owner=row.owner;key.inventory=row.inventory;
    key.avatar=10;key.utility=22;key.ordinary=23;
    memcpy(key.utility_identity,utility.identity,8);memcpy(key.ordinary_identity,ordinary.identity,8);
    key.peer_size=context.peer_size;memcpy(key.peers,context.peers,context.peer_size);
    assert(uss_pair_owner_publication(&owner,7,1,&row,&key));effects=sends=0;
}
static UssPairObservation observe(void* user)
{UssPairObservation output;(void)user;uss_pair_owner_observation(&owner,7,&observed,&output);return output;}
static UssPairServiceResult select_slot(void* user,const UssPairObservation* before,uint32_t slot)
{(void)user;(void)before;++effects;observed.active=slot;return USS_PAIR_SERVICE_DONE;}
static UssPairServiceResult ready(void* user,const UssPairObservation* before)
{(void)user;(void)before;return USS_PAIR_SERVICE_DONE;}
static UssPairServiceResult switch_member(void* user,const UssPairObservation* before,const UssPairStep* step,unsigned defer)
{(void)user;(void)before;(void)defer;++effects;observed.support=step->member.entity;return USS_PAIR_SERVICE_DONE;}
static UssPairServiceResult publish(void* user,const UssPairObservation* before,const UssPairStep* step)
{(void)user;(void)before;(void)step;++effects;return USS_PAIR_SERVICE_DONE;}
static int verify(void* user,const UssPairObservation* before){(void)user;(void)before;return 1;}
static uint64_t clock_ms(void* user){(void)user;return 100;}
static UssPublicationView view(void* user){(void)user;return pub_view;}
static int capture(void* user,UssPublicationStage stage,UssPublicationCapture* output)
{(void)user;(void)stage;*output=pub_capture;return 1;}
static UssPublicationSend send(void* user,UssPublicationKind kind,const UssPublicationCapture* snapshot,uint64_t metadata)
{(void)user;(void)kind;(void)snapshot;(void)metadata;++sends;return USS_PUBLICATION_SEND_RETURNED;}
static void refresh_publication(void)
{
    assert(uss_pair_owner_publication(&owner,7,1,&row,&key));
    memset(&pub_view,0,sizeof(pub_view));pub_view.valid=pub_view.mission=pub_view.pair_present=pub_view.supported_pair=1;pub_view.key=key;
    memset(&pub_capture,0,sizeof(pub_capture));pub_capture.valid=1;pub_capture.key=key;pub_capture.owner=row;
    pub_capture.thread=7;pub_capture.fingerprint_size=1;pub_capture.fingerprint[0]=7;
    pub_capture.wires[0]=31;pub_capture.wires[1]=32;pub_capture.wires[2]=33;pub_capture.wires[3]=34;
}
int main(void)
{
    UssPairOwner saved;UssPairObservation stamp,before;UssPairTransaction work={0};
    UssPairServices services={NULL,observe,select_slot,ready,switch_member,publish,verify,NULL};
    UssPublicationState publication={0};UssPublicationServices pub_services={NULL,clock_ms,view,capture,send};
    uint64_t pair_generation,guard_generation;unsigned i;
    setup();saved=owner;
    assert(uss_pair_owner_update(&owner,7,1,1,&context));assert(!memcmp(&saved,&owner,sizeof(owner)));
    assert(uss_pair_owner_observation(&owner,7,&observed,&stamp));
    assert(stamp.epoch==1 && stamp.pair_generation==1 && stamp.guard_generation==1);
    assert(uss_pair_owner_observation(&owner,7,&stamp,&stamp)); /* In-place adapter. */
    assert(uss_pair_begin(&work,&services,&stamp,USS_PAIR_DRAW,3,0,&utility,100));before=stamp;
    pair_generation=owner.pair_generation;guard_generation=owner.guard_generation;
    assert(!uss_pair_owner_update(&owner,7,0,0,NULL));
    assert(owner.pair_present && owner.pair_generation==pair_generation && owner.guard_generation>guard_generation);
    assert(!uss_pair_owner_observation(&owner,7,&observed,&stamp) && !stamp.valid);
    assert(uss_pair_owner_update(&owner,7,1,1,&context));assert(owner.pair_generation==pair_generation);
    assert(uss_pair_tick(&work,&services,101)==USS_PAIR_CANCELLED && effects==0);
    work=(UssPairTransaction){0};stamp=observe(NULL);
    assert(stamp.guard_generation!=before.guard_generation);
    assert(uss_pair_begin(&work,&services,&stamp,USS_PAIR_DRAW,3,0,&utility,102));
    assert(uss_pair_tick(&work,&services,103)==USS_PAIR_COMPLETE && effects==4);

    setup();refresh_publication();
    assert(uss_publication_boundary(&publication,&pub_services,&row,0,USS_PUBLICATION_PRE_ATTACH).outcome==USS_PUBLICATION_RETURNED && sends==1);
    assert(!uss_pair_owner_update(&owner,7,0,0,NULL));assert(uss_pair_owner_update(&owner,7,1,1,&context));
    refresh_publication();
    assert(uss_publication_boundary(&publication,&pub_services,&row,0,USS_PUBLICATION_POST_ATTACH).outcome==USS_PUBLICATION_UNCHANGED && sends==1);
    pair_generation=owner.pair_generation;
    assert(uss_pair_owner_set_pair(&owner,7,pair_generation,&utility,&ordinary));
    assert(owner.pair_generation>pair_generation);refresh_publication();
    assert(uss_publication_boundary(&publication,&pub_services,&row,0,USS_PUBLICATION_PRE_ATTACH).outcome==USS_PUBLICATION_RETURNED && sends==2);
    publication.disabled=1;
    assert(uss_pair_owner_clear_pair(&owner,7,owner.pair_generation));
    assert(uss_pair_owner_set_pair(&owner,7,0,&utility,&ordinary));refresh_publication();
    assert(uss_publication_poll(&publication,&pub_services,&row).reason==USS_PUB_DISABLED && sends==2);

    for(i=0;i<7;++i) {
        setup();pair_generation=owner.pair_generation;
        switch(i){case 0:context.local.avatar++;break;case 1:context.local.row_address++;break;
        case 2:context.session++;break;case 3:context.mapping++;break;case 4:context.role=2;break;case 5:context.peers[8]++;break;case 6:context.session_version++;break;}
        assert(uss_pair_owner_update(&owner,7,1,1,&context));
        assert(owner.context_generation==2 && owner.epoch==1);
        if(i>=4){
            assert(owner.pair_present && owner.pair_generation==pair_generation && owner.guard_generation==2);
            assert(!uss_pair_owner_set_pair(&owner,7,0,&utility,&ordinary));
        }else{
            assert(!owner.pair_present);
            assert(!uss_pair_owner_set_pair(&owner,7,pair_generation,&utility,&ordinary));
            assert(uss_pair_owner_set_pair(&owner,7,0,&utility,&ordinary));assert(owner.pair_generation>pair_generation);
        }
    }
    setup();pair_generation=owner.pair_generation;
    assert(uss_pair_owner_clear_pair(&owner,7,pair_generation));
    assert(!uss_pair_owner_clear_pair(&owner,7,pair_generation));
    assert(uss_pair_owner_set_pair(&owner,7,0,&utility,&ordinary));assert(owner.pair_generation>pair_generation);
    assert(uss_pair_owner_update(&owner,7,1,0,NULL));assert(!owner.pair_present && !owner.ready);
    assert(uss_pair_owner_update(&owner,7,1,1,&context));assert(owner.epoch==2 && owner.context_generation==2);
    setup();saved=owner;ordinary.identity[0]=0;
    assert(!uss_pair_owner_set_pair(&owner,7,owner.pair_generation,&utility,&ordinary));assert(!memcmp(&saved,&owner,sizeof(owner)));
    setup();key.peers[0]^=1;saved=owner;
    assert(!uss_pair_owner_publication(&owner,7,1,&row,&key));assert(!memcmp(&saved,&owner,sizeof(owner)));
    setup();observed.utility.entity++;assert(!uss_pair_owner_observation(&owner,7,&observed,&stamp) && !stamp.valid);
    setup();assert(!uss_pair_owner_update(&owner,8,1,1,&context) && owner.disabled);
    assert(!uss_pair_owner_update(&owner,7,1,1,&context));
    setup();owner.pair_generation=UINT64_MAX;
    assert(!uss_pair_owner_set_pair(&owner,7,UINT64_MAX,&utility,&ordinary) && owner.disabled);
    setup();owner.context_generation=UINT64_MAX;context.session++;
    assert(!uss_pair_owner_update(&owner,7,1,1,&context) && owner.disabled);
    setup();owner.epoch=UINT64_MAX;assert(uss_pair_owner_update(&owner,7,1,0,NULL));
    assert(!uss_pair_owner_update(&owner,7,1,1,&context) && owner.disabled);
    setup();owner.guard_generation=UINT64_MAX;
    assert(!uss_pair_owner_update(&owner,7,0,0,NULL) && owner.disabled);
    puts("Core owner: fresh/reused pair lifetimes, unknown/context/mission/thread/overflow guards, coordinator cancellation and publisher dedup PASS");
    return 0;
}
