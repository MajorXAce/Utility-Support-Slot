#ifdef NDEBUG
#error Native interface checks require active assertions
#endif
#include "../src/native_calls.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static char inventory_token,action_token;
#define INVENTORY ((void*)&inventory_token)
#define ACTION ((void*)&action_token)
static unsigned events[8], count;
static void event(unsigned value) { assert(count<8);events[count++]=value; }
static void attach_stub(void* p,uint32_t avatar,uint32_t entity,uint32_t slot)
{ assert(p==INVENTORY && avatar==0xf1234567u && entity==0xe2345678u && slot==3);event(1); }
static void state_stub(void* p,uint32_t slot,uint32_t zero)
{ assert(p==ACTION && slot==2 && zero==0);event(2); }
static void weapon_stub(void* p,uint32_t avatar,uint32_t slot)
{ assert(p==INVENTORY && avatar==0xf1234567u && slot==2);event(3); }
static void entity_stub(void* p,uint32_t avatar,uint32_t entity,uint8_t zero,void* optional,uint8_t one)
{ assert(p==INVENTORY && avatar==0xf1234567u && entity==0xe2345678u && zero==0 && !optional && one==1);event(4); }
static void slot_stub(void* p,uint32_t avatar,uint32_t slot,uint32_t publish)
{ assert(p==INVENTORY && avatar==0xf1234567u && slot==3 && publish==1);event(5); }
static void send_stub(int64_t route,uint32_t avatar,uint32_t primary,uint32_t secondary,
                      uint32_t support,uint32_t backpack,uint32_t field24,uint64_t metadata)
{
    assert(route==INT64_C(-2) && avatar==0xf1234567u && primary==11 && secondary==22);
    assert(support==33 && backpack==44 && field24==0xfabcdef0u);
    assert(metadata==UINT64_C(0xfedcba9876543211));event(6);
}
int main(void)
{
    UssNativeCalls calls={attach_stub,state_stub,weapon_stub,entity_stub,slot_stub,send_stub};
    UssNativeCalls missing;
    UssInventoryMessage message={0xf1234567u,11,22,33,44,0xfabcdef0u,UINT64_C(0xfedcba9876543211)};
    const unsigned expected[]={1,2,3,4,5,6};
    assert(uss_attach_support(&calls,INVENTORY,message.avatar,0xe2345678u)==USS_CALL_RETURNED);
    assert(uss_select_slot(&calls,ACTION,INVENTORY,message.avatar,2)==USS_CALL_RETURNED);
    assert(uss_drop_stored_entity(&calls,INVENTORY,message.avatar,0xe2345678u)==USS_CALL_RETURNED);
    assert(uss_drop_support_slot(&calls,INVENTORY,message.avatar)==USS_CALL_RETURNED);
    assert(uss_send_inventory(&calls,&message)==USS_CALL_RETURNED);
    assert(count==6 && !memcmp(events,expected,sizeof(expected)));
    count=0;
    /* Missing either selection half must prevent BOTH calls. */
    missing=calls;missing.select_state=NULL;
    assert(uss_select_slot(&missing,ACTION,INVENTORY,1,2)==USS_CALL_NOT_ENTERED);
    missing=calls;missing.select_weapon=NULL;
    assert(uss_select_slot(&missing,ACTION,INVENTORY,1,2)==USS_CALL_NOT_ENTERED);
    assert(uss_select_slot(&calls,NULL,INVENTORY,1,2)==USS_CALL_NOT_ENTERED);
    assert(uss_select_slot(&calls,ACTION,NULL,1,2)==USS_CALL_NOT_ENTERED);
    memset(&missing,0,sizeof(missing));
    assert(uss_attach_support(&missing,INVENTORY,1,2)==USS_CALL_NOT_ENTERED);
    assert(uss_drop_stored_entity(&missing,INVENTORY,1,2)==USS_CALL_NOT_ENTERED);
    assert(uss_drop_support_slot(&missing,INVENTORY,1)==USS_CALL_NOT_ENTERED);
    assert(uss_send_inventory(&missing,&message)==USS_CALL_NOT_ENTERED);
    assert(uss_attach_support(NULL,INVENTORY,1,2)==USS_CALL_NOT_ENTERED);
    assert(uss_attach_support(&calls,NULL,1,2)==USS_CALL_NOT_ENTERED);
    assert(uss_drop_stored_entity(&calls,NULL,1,2)==USS_CALL_NOT_ENTERED);
    assert(uss_drop_support_slot(&calls,NULL,1)==USS_CALL_NOT_ENTERED);
    assert(uss_send_inventory(&calls,NULL)==USS_CALL_NOT_ENTERED);
    assert(!count);
    puts("native interface: six test targets, exact arguments/order/metadata, no entry on missing dependencies PASS");
    return 0;
}
