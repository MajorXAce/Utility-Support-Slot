#include <assert.h>
#include <stdio.h>
#include "../src/uss_pickup_completion.h"
#include "../src/uss_refill_native_calls.h"
static unsigned char action_manager[0x53f000],action_hash[32],avatar_unit[24],interaction[0x14080],source_hash[32],units[8],source_unit[24],source_row[880];
static unsigned char selected_manager[0x50],selected_hash[32],selected_units[8],selected_unit[24],selected_row[0x880],winner[24];
static unsigned reads,fail_at,scopes,changed,refill_calls,source_mutation,type_reads;
static UssRefillTarget refill_current;
static UssRefillTarget refill_observe(void* p){(void)p;return refill_current;}
static int refill_verify(void* p,const UssRefillEvidence* e,const UssRefillTarget* t)
{(void)p;return e->source==10 && e->source_kind==4 && e->source_identity[0]==9 && t->target==3;}
static void own_native_refill(void* manager,uint32_t entity,float scale)
{assert(manager==NULL && entity==refill_current.target && scale==0.5f);++refill_calls;++refill_current.reserve;}
static unsigned refill_forward(void* p,const UssRefillTarget* t,float scale)
{UssRefillNativeCalls calls={own_native_refill,own_native_refill};(void)p;return uss_refill_native_call(&calls,t,scale);}
static uint64_t context(void* p){(void)p;return ++scopes>1 && changed?2:1;}
static int read_memory(void* p,uintptr_t at,void* out,size_t size)
{
    const struct {uintptr_t at;void* data;size_t size;} regions[]={
        {0x20000000,action_manager,sizeof(action_manager)},{0x21000000,action_hash,32},{0x22000000,avatar_unit,24},
        {0x23000000,interaction,sizeof(interaction)},{0x24000000,source_hash,32},{0x25000000,units,8},{0x26000000,source_unit,24},{0x27000000,source_row,880},{0x29000000,selected_manager,sizeof(selected_manager)},{0x2a000000,selected_hash,32},{0x2b000000,selected_units,8},{0x2c000000,selected_unit,24},{0x2d000000,selected_row,sizeof(selected_row)},{0x2e000000,winner,sizeof(winner)}};
    unsigned i;uintptr_t pointer=0;(void)p;if(++reads==fail_at)return 0;
    if(at>=0x27000000 && at<0x27000000+880){
        if(source_mutation==4)return 0; /* Source destroyed after cached selection. */
        if(source_mutation==3 && size>4)return 0; /* Only relevant type remains readable. */
        if(at==0x27000044 && size==4 && ++type_reads==2){
            if(source_mutation==1)source_row[879]^=1;
            if(source_mutation==2){uint32_t next_kind=7;memcpy(source_row+0x44,&next_kind,4);}
        }
    }
    if(at==0x10000000+0x3326d20)pointer=0x20000000;if(at==0x10000000+0x3326d68)pointer=0x23000000;
    if(at==0x10000000+0x33269a0)pointer=0x29000000;
    if(at==0x10000000+0x3326b90)pointer=0x28000000;
    if(pointer && size==8){memcpy(out,&pointer,8);return 1;}
    for(i=0;i<sizeof(regions)/sizeof(regions[0]);++i)if(at>=regions[i].at && at-regions[i].at<=regions[i].size && size<=regions[i].size-(at-regions[i].at)){
        memcpy(out,(unsigned char*)regions[i].data+at-regions[i].at,size);return 1;}
    return 0;
}
static void word(unsigned char* p,uint32_t value){memcpy(p,&value,4);}
static void pointer(unsigned char* p,uintptr_t value){memcpy(p,&value,8);}
static UssRefillTarget fixture(void)
{
    UssRefillTarget t={0};source_mutation=type_reads=0;memset(action_manager,0,sizeof(action_manager));memset(action_hash,0,32);memset(avatar_unit,0,24);
    memset(interaction,0,sizeof(interaction));memset(source_hash,0,32);memset(source_row,0,880);memset(source_unit,0,24);
    pointer(action_manager+0xf8,0x21000000);word(action_manager+0x100,4);word(action_manager+0x104,UINT32_MAX);word(action_manager+0x108,1);
    word(action_hash+16,2);word(action_hash+20,0);word(action_manager+0x70,1);pointer(action_manager+0x110,0x22000000);word(avatar_unit+8,2);
    word(action_manager+0x53dcd0,2);word(action_manager+0x53dcd0+0x9ec,2);word(action_manager+0x53dcd0+0x9ec+8,10);
    pointer(interaction+0x14050,0x24000000);word(interaction+0x14058,4);word(interaction+0x1405c,UINT32_MAX);word(interaction+0x14060,1);
    word(source_hash+16,10);word(source_hash+20,0);pointer(interaction+0x14068,0x25000000);pointer(interaction+0x14070,0x27000000);
    pointer(units,0x26000000);source_unit[0]=9;word(source_unit+8,10);word(source_row+0x44,4);
    t.epoch=1;t.owner=0x28000000;t.inventory=t.ammo_manager=t.ammo_unit=0x29000000;t.avatar=2;t.target=3;t.real_support=4;
    t.direct_amount=2;t.kind=1;t.identity[0]=5;t.valid=t.eligible=t.stored=t.non_expendable=t.ordinary=1;
    memset(selected_manager,0,sizeof(selected_manager));memset(selected_hash,0,32);memset(selected_unit,0,24);memset(winner,0,20);
    word(selected_manager+0x1c,1);pointer(selected_manager+0x28,0x2a000000);word(selected_manager+0x30,4);word(selected_manager+0x34,UINT32_MAX);word(selected_manager+0x38,1);
    word(selected_hash+16,2);word(selected_hash+20,0);pointer(selected_manager+0x40,0x2b000000);pointer(selected_manager+0x48,0x2d000000);
    pointer(selected_units,0x2c000000);word(selected_unit+8,2);pointer(selected_row,0x2e000000);word(interaction+0xe000,10);word(interaction+0xe004,0);
    reads=fail_at=scopes=changed=0;return t;
}
int main(void)
{
    UssRefillMemoryIo io={NULL,0x10000000,read_memory,context};UssRefillTarget t;UssPickupAction action={0},old_action;
    UssPickupSource source={0},old_source;unsigned baseline,i;
    t=fixture();assert(uss_pickup_read_action(&io,&t,&action) && action.avatar==2 && action.entity==10 && action.zone==0);baseline=reads;old_action=action;
    for(i=1;i<=baseline;++i){t=fixture();fail_at=i;assert(!uss_pickup_read_action(&io,&t,&action) && !memcmp(&action,&old_action,sizeof(action)));}
    t=fixture();changed=1;assert(!uss_pickup_read_action(&io,&t,&action));
    {UssPickupActionDiagnostic diagnostic={0};unsigned original_reads;
        t=fixture();assert(uss_pickup_read_action(&io,&t,&action));original_reads=reads;
        t=fixture();assert(uss_pickup_read_action_diagnose(&io,&t,&action,&diagnostic));
        assert(reads==original_reads && !strcmp(diagnostic.stage,"ACTION_READY"));
        assert(!strcmp(diagnostic.hash.stage,"HASH_FOUND") && diagnostic.hash.capacity==4 && diagnostic.hash.key==2);
        /* Live action hash buckets are uint32 pairs at a four-byte base. */
        t=fixture();memmove(action_hash+4,action_hash,28);pointer(action_manager+0xf8,0x21000004);
        assert(uss_pickup_read_action_diagnose(&io,&t,&action,&diagnostic));
        assert(!strcmp(diagnostic.stage,"ACTION_READY") && !strcmp(diagnostic.hash.stage,"HASH_FOUND") && diagnostic.hash.alignment==4);
        assert(action.avatar==2 && action.entity==10 && action.zone==0);
        t=fixture();pointer(action_manager+0xf8,0x21000002);
        assert(!uss_pickup_read_action_diagnose(&io,&t,&action,&diagnostic));
        assert(!strcmp(diagnostic.hash.stage,"HASH_ARRAY_ALIGNMENT") && diagnostic.hash.alignment==2);
        t=fixture();word(action_manager+0x100,3);
        assert(!uss_pickup_read_action_diagnose(&io,&t,&action,&diagnostic));
        assert(!strcmp(diagnostic.hash.stage,"HASH_CAPACITY"));
        old_action=action;t=fixture();word(action_manager+0x53dcd0+0x9ec,0);
        assert(!uss_pickup_read_action_diagnose(&io,&t,&action,&diagnostic));
        assert(!strcmp(diagnostic.stage,"ACTION_COMPLETION_OWNER") && diagnostic.actual==0 && diagnostic.expected==2);
        assert(!memcmp(&action,&old_action,sizeof(action)));
        t=fixture();word(action_manager+0x53dcd0,9);
        assert(!uss_pickup_read_action_diagnose(&io,&t,&action,&diagnostic));
        assert(!strcmp(diagnostic.stage,"ACTION_ROW_OWNER") && diagnostic.actual==9 && diagnostic.expected==2);
    }
    t=fixture();assert(uss_pickup_read_source(&io,&t,10,1000,&source));assert(source.entity==10 && source.verified_zones==1 && source.scale[0]==0.5f);
    baseline=reads;old_source=source;
    for(i=1;i<=baseline;++i){t=fixture();fail_at=i;assert(!uss_pickup_read_source(&io,&t,10,1000,&source) && !memcmp(&source,&old_source,sizeof(source)));}
    t=fixture();word(source_row+0x68+0x44,7);assert(uss_pickup_read_source(&io,&t,10,1000,&source) && source.verified_zones==1);
    t=fixture();word(source_unit+8,11);assert(!uss_pickup_read_source(&io,&t,10,1000,&source));
    t=fixture();changed=1;assert(!uss_pickup_read_source(&io,&t,10,1000,&source));
    /* Completion fallback owns one type, not the whole consumable record. */
    {const char* reason=NULL;UssPickupSource exact={0},unchanged;
        t=fixture();source_mutation=1;
        assert(uss_pickup_read_source_scope(&io,&t,10,1000,0,&exact,&reason));
        assert(exact.verified_zones==1 && exact.kind[0]==4 && source_row[879]==1);
        unchanged=exact;t=fixture();source_mutation=2;
        assert(!uss_pickup_read_source_scope(&io,&t,10,1000,0,&exact,&reason));
        assert(!strcmp(reason,"SOURCE_TYPE_RECHECK") && !memcmp(&exact,&unchanged,sizeof(exact)));
        t=fixture();source_mutation=3;
        assert(!uss_pickup_read_source(&io,&t,10,1000,&exact));
        assert(uss_pickup_read_source_scope(&io,&t,10,1000,0,&exact,&reason));
    }
    {UssSelectedPickup selected={0},old;UssPickupCompletions completions={0};UssRefillEvidence event;
        t=fixture();memmove(winner+4,winner,20);pointer(selected_row,0x2e000004);
        assert(uss_pickup_read_selected(&io,&t,&selected) && selected.winner==0x2e000004);
        t=fixture();pointer(selected_row,0x2e000002);assert(!uss_pickup_read_selected(&io,&t,&selected));
        /* Cache exact metadata before disappearance; no fresh source is invented. */
        t=fixture();word(action_manager+0x53dcd0+0x9ec+8,0);
        assert(!uss_pickup_completion_poll(&completions,&io,&t,1000,&event));source_mutation=4;
        word(action_manager+0x53dcd0+0x9ec+8,10);
        assert(uss_pickup_completion_poll(&completions,&io,&t,1025,&event) && event.source_kind==4);
        assert(!uss_pickup_completion_poll(&completions,&io,&t,1050,&event));
        memset(&completions,0,sizeof(completions));

        t=fixture();assert(uss_pickup_read_selected(&io,&t,&selected) && selected.entity==10 && selected.zone==0 && selected.winner==0x2e000000);
        baseline=reads;old=selected;
        for(i=1;i<=baseline;++i){t=fixture();fail_at=i;assert(!uss_pickup_read_selected(&io,&t,&selected) && !memcmp(&selected,&old,sizeof(old)));}
        t=fixture();word(action_manager+0x53dcd0+0x9ec+8,0);
        assert(!uss_pickup_completion_poll(&completions,&io,&t,1000,&event));
        word(action_manager+0x53dcd0+0x9ec+8,10);
        assert(uss_pickup_completion_poll(&completions,&io,&t,1025,&event) && event.source==10 && event.source_kind==4);
        {UssRefillState refill_state={0};UssRefillServices refill_io={NULL,refill_observe,refill_verify,refill_forward};
            refill_current=t;refill_calls=0;
            assert(uss_refill_apply(&refill_state,&refill_io,&event,1025)==USS_REFILL_CONFIRMED && refill_calls==1);
            assert(uss_refill_apply(&refill_state,&refill_io,&event,1050)==USS_REFILL_SKIPPED && refill_calls==1);}
        assert(!uss_pickup_completion_poll(&completions,&io,&t,1050,&event));
        assert(uss_pickup_activity_near(&completions,1100) && !uss_pickup_activity_near(&completions,1526));
    }
    puts("USS pickup readers: actual action/source layouts, complete split zone row, all read failures, owner/identity/scope and unchanged-output PASS");return 0;
}
