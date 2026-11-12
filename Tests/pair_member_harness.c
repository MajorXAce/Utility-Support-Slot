#ifdef NDEBUG
#error Pair-member checks require active assertions
#endif
#define main inventory_regression_checks
#include "inventory_harness.c"
#undef main
#include "../src/pair_member.h"
#define INTERACTION_MANAGER ((uintptr_t)0x90000000)
#define INTERACTION_ROW ((uintptr_t)0x90000300)
static Inventory member_context;
static unsigned member_change,member_hits;
static const unsigned char silo_identity[8]={0xaa,0x79,0xca,0x92,0x05,0x46,0x13,0xfc};
static int member_read(uintptr_t address,void* output,size_t bytes)
{
    int status=fake_read(address,output,bytes);
    uintptr_t target=member_change==1 ? (uintptr_t)0x70000760 : INTERACTION_ROW+0xC9;
    if (!status && member_change && address==target && ++member_hits==2) {
        if(member_change==1) put32((unsigned char*)output+4,23);
        else *(unsigned char*)output=1;
    }
    return status;
}
static void member_fixture(void)
{
    static const unsigned char pin[32]={
        0x40,0x53,0x41,0x54,0x41,0x57,0x48,0x83,0xec,0x20,0x3b,0x15,0x50,0xb1,0xb0,0x02,
        0x45,0x0f,0xb6,0xf9,0x48,0x8b,0x1d,0x8d,0xe2,0x9a,0x02,0x45,0x8b,0xe0,0x0f,0x84};
    unsigned char* b;
    distinct_slots();member_change=member_hits=0;fake.read=member_read;
    /*103 is stored, NOT the current support102 or held101. */
    put32(fixture(0x70000300,48)+8,102);
    b=fixture(0x70000700,144);put32(b+48+12,2);put32(b+96+12,2);put32(b+96+4,22);
    memcpy(fixture(0x70002900,24),silo_identity,8);
    memcpy(segment(BASE+0x978AC0,32),pin,32);
    putptr(segment(BASE+0x3326D68,8),INTERACTION_MANAGER);
    b=segment(INTERACTION_MANAGER+0x14050,40);
    putptr(b,0x90000100);put32(b+8,1);put32(b+12,UINT32_MAX);put32(b+16,1);
    putptr(b+24,0x90000200);putptr(b+32,INTERACTION_ROW);
    b=segment(0x90000100,8);put32(b,103);put32(b+4,0);
    putptr(segment(0x90000200,8),0x70002900);
    put32(segment(INTERACTION_ROW+0xA8,4),UINT32_MAX);
    *segment(INTERACTION_ROW+0xC9,1)=0;
    put32(segment(INTERACTION_ROW+0x360,4),7);
    assert(sample(&member_context));
    reads=module_calls=session_calls=0;memset(&last_reader,0,sizeof(last_reader));
}
static UssMemberEvidence observe_member(void)
{return uss_observe_pair_member(&fake,MOD,&member_context,103,silo_identity,&last_reader);}
static void unknown_member(UssMemberEvidence result)
{
    UssMemberEvidence empty={0};assert(!memcmp(&empty,&result,sizeof(result)));
}
int main(void)
{
    UssMemberEvidence result;unsigned total,i;
    assert(inventory_regression_checks()==0);
    member_fixture();result=observe_member();
    assert(result.status==USS_MEMBER_OBSERVED && result.entity==103 && result.parent==22);
    assert(result.parent_matches && result.classification==USS_UTILITY_SUPPORT);
    assert(result.interaction_admissible && result.gate==0 && result.zone==UINT32_MAX && result.tail==7);
    assert(member_context.slots[2]==102 && member_context.held==101);
    total=reads;assert(last_reader.calls==total && total<=2165 && last_reader.bytes<=20000);
    printf("member reader baseline: %u reads / %u bytes\n",total,last_reader.bytes);
    for(i=1;i<=total;++i){member_fixture();fail_at=i;unknown_member(observe_member());}
    member_fixture();put32(fixture(0x70000700,144)+96+4,23);
    result=observe_member();assert(result.status==USS_MEMBER_OBSERVED && !result.parent_matches);
    member_fixture();put32(fixture(0x70000700,144)+96+4,UINT32_MAX);
    result=observe_member();assert(result.status==USS_MEMBER_OBSERVED && !result.parent_matches);
    member_fixture();put32(fixture(INTERACTION_ROW+0xA8,4),2);
    result=observe_member();assert(result.status==USS_MEMBER_OBSERVED && result.interaction_admissible);
    member_fixture();*fixture(INTERACTION_ROW+0xC9,1)=1;
    result=observe_member();assert(result.status==USS_MEMBER_OBSERVED && result.interaction_admissible);
    put32(fixture(INTERACTION_ROW+0xA8,4),2);memset(&last_reader,0,sizeof(last_reader));
    result=observe_member();assert(result.status==USS_MEMBER_OBSERVED && !result.interaction_admissible);
    member_fixture();*fixture(INTERACTION_ROW+0xC9,1)=2;
    result=observe_member();assert(result.status==USS_MEMBER_OBSERVED && !result.interaction_admissible);
    member_fixture();fixture(0x70002900,24)[0]^=1;unknown_member(observe_member());
    member_fixture();put32(fixture(0x70002900,24)+8,104);unknown_member(observe_member());
    member_fixture();put32(fixture(0x90000100,8),104);unknown_member(observe_member());
    member_fixture();put32(fixture(0x90000100,8)+4,16384);unknown_member(observe_member());
    member_fixture();putptr(fixture(INTERACTION_MANAGER+0x14050,40)+24,0);unknown_member(observe_member());
    member_fixture();put32(fixture(0x70000700,144)+96+12,99);
    result=observe_member();assert(result.status==USS_MEMBER_OBSERVED && result.classification==USS_NOT_SUPPORT);
    /* Interaction descriptor ownership is checked independently of the weapon table. */
    member_fixture();putptr(fixture(0x90000200,8),0x90000900);
    memcpy(segment(0x90000900,24),fixture(0x70002900,24),24);
    fixture(0x90000900,24)[0]^=1;unknown_member(observe_member());
    member_fixture();put32(fixture(INTERACTION_MANAGER+0x14050,40)+8,3);
    unknown_member(observe_member());
    member_fixture();fixture(BASE+0x978AC0,32)[0]^=1;unknown_member(observe_member());
    member_fixture();member_context.avatar=23;unknown_member(observe_member());
    member_fixture();member_change=1;unknown_member(observe_member());
    assert(!strcmp(last_reader.failure,"MEMBER_CONTEXT_CHANGED"));
    member_fixture();member_change=2;unknown_member(observe_member());
    member_fixture();module_change=1;unknown_member(observe_member());
    member_fixture();change_session=1;unknown_member(observe_member());
    member_fixture();change_session=2;unknown_member(observe_member());
    member_fixture();initial_role=HF_SESSION_HOST;unknown_member(observe_member());assert(!reads);
    member_fixture();last_reader.bytes=19999;unknown_member(observe_member());assert(!reads);
    member_fixture();last_reader.bytes=20001;unknown_member(observe_member());assert(!reads);
    member_fixture();last_reader.calls=2165;unknown_member(observe_member());assert(!reads);
    member_fixture();result=uss_observe_pair_member(NULL,NULL,NULL,0,NULL,NULL);
    assert(result.status==USS_MEMBER_EMPTY && !reads);
    result=uss_observe_pair_member(NULL,NULL,NULL,UINT32_MAX,NULL,NULL);
    assert(result.status==USS_MEMBER_EMPTY);
    unknown_member(uss_observe_pair_member(NULL,MOD,&member_context,103,silo_identity,NULL));
    puts("pair-member: stored identity, parent, independent interaction, instability and every partial read failure PASS");
    return 0;
}
