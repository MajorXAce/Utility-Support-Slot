/* Actual Core/member/publication readers over owned buffers, no game access.
 * Custom support weapons (runtime kind 0) are accepted only through slot-residency proof of their identity. */
#define USS_CORE_TEST_MAIN original_core_checks
#include "core_observation_harness.c"
#include "../src/core_lifecycle.h"
#define SUPPORT_ROW_KIND(kind) put32(fixture(0x70000700,144)+48+12,(kind))
#define SLOT2_ENTITY(entity) put32(fixture(0x70000300,48)+8,(entity))
/* Entity 102 is the ordinary member and occupies the native support slot; entity 103 is the utility member. */
static void custom_fixture(void)
{
    core_fixture();core_pair();
    SUPPORT_ROW_KIND(0);
    uss_support_proof_reset();
}
/* Fake SDK content list: clones of Silo and C4 (utilities) and of EAT-17 (expendable), an unrelated entity, a stratagem
 * with a Silo base, and an empty id. */
static unsigned content_calls;
static int fake_content(HfMod* mod,uint32_t index,HfContent* out)
{
    uint64_t silo,c4,eat17;
    memcpy(&silo,uss_known_support[0].identity,8);memcpy(&c4,uss_known_support[1].identity,8);memcpy(&eat17,uss_known_support[3].identity,8);
    assert(mod==MOD && out->size==sizeof *out);++content_calls;
    switch(index){
    case 0:out->kind=HF_KIND_ENTITY;out->id=0x1122334455667788ull;out->base=silo;out->name="test/clone_silo";return 1;
    case 1:out->kind=HF_KIND_ENTITY;out->id=0x8877665544332211ull;out->base=c4;return 1;
    case 2:out->kind=HF_KIND_ENTITY;out->id=0x0102030405060708ull;out->base=0x9999ull;return 1;
    case 3:out->kind=HF_KIND_STRATAGEM;out->id=0x1111ull;out->base=silo;return 1;
    case 4:out->kind=HF_KIND_ENTITY;out->id=0;out->base=silo;return 1;
    case 5:out->kind=HF_KIND_ENTITY;out->id=0x5566778899aabbccull;out->base=eat17;out->name="test/clone_eat17";return 1;
    default:return 0;}
}
static void id_bytes(uint64_t id,unsigned char out[8]){memcpy(out,&id,8);}
int main(void)
{
    UssCoreObservation current;UssPublicationCapture captured;UssPairMember member;
    unsigned i,total;
    (void)uss_lifecycle_flags;(void)uss_lifecycle_observe;(void)uss_lifecycle_create;
    (void)uss_lifecycle_replace_begin;(void)uss_lifecycle_replace_commit;(void)uss_lifecycle_complete;
    assert(original_core_checks()==0);

    /* Kind-0 occupant of the support slot is accepted; raw kind is retained and nothing is written. */
    custom_fixture();current=core_read();
    assert(current.valid && current.pair_matched && current.support_class==USS_ORDINARY_SUPPORT);
    assert(current.inventory.native_kinds[2]==0 && current.inventory.observed_kinds[2]==2);
    assert(current.ordinary.native_kind==0 && current.ordinary.kind==2);
    assert(lifecycle_member(&current.ordinary,USS_ORDINARY_SUPPORT,&member) && member.entity==102);
    assert(u32(fixture(0x70000700,144)+48+12)==0); /* No native category write. */
    assert(uss_support_proof_count==1 && support_proof_has(capture_input.ordinary_identity));
    total=reads;
    /* Failed observations leave no lasting proof, whichever read fails. */
    for(i=1;i<=total;++i){custom_fixture();fail_at=i;assert(!core_read().valid && !uss_support_proof_count);}
    fail_at=0;
    /* The inventory sample alone (no accepted observation) must not make the proof lasting, though it still classifies
     * the slot occupant for that one sample. */
    custom_fixture();
    {Inventory snapshot;assert(sample(&snapshot) && snapshot.observed_kinds[2]==2 && snapshot.native_kinds[2]==0);
     assert(!uss_support_proof_count);}
    /* Incoherent observations (state changes between the two samples/scopes) leave nothing behind. */
    custom_fixture();change_session=1;assert(!core_read().valid && !uss_support_proof_count);change_session=0;
    custom_fixture();module_change=1;assert(!core_read().valid && !uss_support_proof_count);module_change=0;
    custom_fixture();core_thread_change=1;assert(!core_read().valid && !uss_support_proof_count);core_thread_change=0;
    custom_fixture();core_flag_change=1;assert(!core_read().valid && !uss_support_proof_count);core_flag_change=0;

    /* Native kinds other than 0 are never promoted; native kind 2 needs no proof. */
    custom_fixture();SUPPORT_ROW_KIND(1);assert(!core_read().valid && !uss_support_proof_count);
    custom_fixture();SUPPORT_ROW_KIND(3);assert(!core_read().valid && !uss_support_proof_count);
    custom_fixture();SUPPORT_ROW_KIND(2);current=core_read();assert(current.valid && !uss_support_proof_count);

    /* The same weapon, once seen in the support slot, stays recognised after it is stored (not in a slot). */
    custom_fixture();assert(core_read().valid);
    SLOT2_ENTITY(103);
    current=core_read();
    assert(current.valid && current.pair_matched && current.support_class==USS_UTILITY_SUPPORT);
    assert(current.ordinary.native_kind==0 && current.ordinary.kind==2);

    /* A kind-0 weapon never seen in the support slot is not support, even if a pair names it. */
    custom_fixture();SLOT2_ENTITY(103);assert(!core_read().valid);

    /* Held support uses the same proof; primary/secondary retain native categories. */
    custom_fixture();put32(fixture(0x70000300,48)+28,3);put32(fixture(0x70000500,4),102);
    {Inventory snapshot;assert(sample(&snapshot));
     assert(snapshot.native_kinds[4]==0 && snapshot.observed_kinds[4]==2);
     assert(snapshot.observed_kinds[0]==snapshot.native_kinds[0]);
     assert(snapshot.observed_kinds[1]==snapshot.native_kinds[1]);}

    /* The proof table is bounded, least-recently-used, and never evicts the identity of the current pair's members. */
    {unsigned char id[8],first[8]={1,0,0,0,0,0,0,0},second[8]={2,0,0,0,0,0,0,0},zero[8]={0};unsigned n;
     /* Plain LRU: a lookup refreshes an entry, so the least recently used one is the one replaced. */
     uss_support_proof_reset();
     for(n=1;n<=USS_SUPPORT_PROOF_CAPACITY;++n){memset(id,0,8);id[0]=(unsigned char)n;assert(support_proof_note(id));}
     assert(uss_support_proof_count==USS_SUPPORT_PROOF_CAPACITY);
     assert(support_proof_use(first));                       /* refresh entry 1; entry 2 is now the oldest */
     memset(id,0,8);id[0]=99;assert(support_proof_note(id));
     assert(support_proof_has(first) && !support_proof_has(second) && support_proof_has(id));
     assert(uss_support_proof_count==USS_SUPPORT_PROOF_CAPACITY);
     assert(!support_proof_note(zero) && uss_support_proof_count==USS_SUPPORT_PROOF_CAPACITY);
     /* Protected identities survive any amount of replacement pressure. */
     uss_support_proof_reset();
     {unsigned char member_a[8]={0xA1,0,0,0,0,0,0,0},member_b[8]={0xB2,0,0,0,0,0,0,0};
      assert(support_proof_note(member_a) && support_proof_note(member_b));
      uss_support_proof_protect(member_a,member_b);
      for(n=1;n<=40;++n){memset(id,0,8);id[0]=(unsigned char)n;id[1]=7;(void)support_proof_note(id);}
      assert(support_proof_has(member_a) && support_proof_has(member_b));
      assert(uss_support_proof_count==USS_SUPPORT_PROOF_CAPACITY);
      /* Once the pair is gone they are ordinary entries again and can be replaced. */
      uss_support_proof_protect(NULL,NULL);
      for(n=1;n<=40;++n){memset(id,0,8);id[0]=(unsigned char)n;id[1]=8;(void)support_proof_note(id);}
      assert(!support_proof_has(member_a) && !support_proof_has(member_b));}
     /* If everything were protected, a new identity is refused (fail closed), never evicting a protected one. */
     uss_support_proof_reset();
     for(n=1;n<=USS_SUPPORT_PROOF_CAPACITY;++n){memset(id,0,8);id[0]=(unsigned char)n;(void)support_proof_note(id);}
     {unsigned k;for(k=0;k<uss_support_proof_count;++k)uss_support_proof[k].protect=1;}
     memset(id,0,8);id[0]=200;assert(!support_proof_note(id) && !support_proof_has(id) && support_proof_has(first));
     uss_support_proof_reset();}
    /* Active stored member through the real Core path: the stored ordinary member (entity 102, kind 0) is proven while it
     * occupies the slot, then the utility is drawn so it is stored; later observations see many other kind-0 weapons pass
     * through the slot, and the stored member is still recognised afterwards. */
    custom_fixture();assert(core_read().valid && support_proof_has(capture_input.ordinary_identity));
    SLOT2_ENTITY(103);
    {unsigned n;
     for(n=1;n<=3*USS_SUPPORT_PROOF_CAPACITY;++n){unsigned char other[8]={0};other[0]=(unsigned char)n;other[1]=9;(void)support_proof_note(other);
        current=core_read();assert(current.valid && current.pair_matched);}
     assert(support_proof_has(capture_input.ordinary_identity));
     assert(current.ordinary.native_kind==0 && current.ordinary.kind==2);}

    /* Lineage: mod-added clones of a vanilla utility are utilities; nothing else is, and the scan runs once. */
    {unsigned char clone_silo[8],clone_c4[8],clone_eat17[8],unrelated[8],stratagem[8],silo_base[8];
     id_bytes(0x1122334455667788ull,clone_silo);id_bytes(0x8877665544332211ull,clone_c4);
     id_bytes(0x0102030405060708ull,unrelated);id_bytes(0x1111ull,stratagem);
     memcpy(silo_base,uss_known_support[0].identity,8);
     id_bytes(0x5566778899aabbccull,clone_eat17);
     /* No callback: fixed table only, no crash, and the single pass is spent. */
     uss_lineage_reset();fake.content=NULL;
     assert(uss_support_lineage_scan_once(&fake,MOD)==0 && !uss_derived_support_count);
     fake.content=fake_content;content_calls=0;
     assert(uss_support_lineage_scan_once(&fake,MOD)==0 && !content_calls); /* once only */
     /* With a callback: exactly the three clones are added. */
     uss_lineage_reset();content_calls=0;
     assert(uss_support_lineage_scan_once(&fake,MOD)==3 && uss_derived_support_count==3 && content_calls==7);
     assert(uss_classify_support(clone_silo,2)==USS_UTILITY_SUPPORT);
     assert(uss_classify_support(clone_c4,2)==USS_UTILITY_SUPPORT);
     /* The EAT-17 clone is an ordinary support weapon (not a utility) that inherits the expendable type. */
     assert(uss_classify_support(clone_eat17,2)==USS_ORDINARY_SUPPORT && uss_is_expendable_support(clone_eat17));
     assert(!uss_is_expendable_support(clone_silo) && !uss_is_expendable_support(clone_c4));
     assert(!uss_is_expendable_support(unrelated) && !uss_is_expendable_support(stratagem));
     assert(uss_classify_support(unrelated,2)==USS_ORDINARY_SUPPORT);
     assert(uss_classify_support(stratagem,2)==USS_ORDINARY_SUPPORT);
     assert(uss_classify_support(clone_silo,0)==USS_NOT_SUPPORT && uss_classify_support(clone_silo,1)==USS_NOT_SUPPORT);
     assert(uss_classify_support(silo_base,2)==USS_UTILITY_SUPPORT);
     assert(uss_support_lineage_scan_once(&fake,MOD)==0 && uss_derived_support_count==3);
     /* Table is bounded. */
     uss_derived_support_reset();
     for(i=0;i<USS_DERIVED_SUPPORT_CAPACITY+3;++i){unsigned char id[8]={0};id[0]=(unsigned char)(i+1);(void)uss_derived_support_add(id,USS_SUPPORT_TYPE_UTILITY);}
     assert(uss_derived_support_count==USS_DERIVED_SUPPORT_CAPACITY);
     {unsigned char zero[8]={0};assert(!uss_derived_support_add(zero,USS_SUPPORT_TYPE_UTILITY));}
     /* The real Core read triggers the scan once, after a mission has begun. */
     custom_fixture();uss_lineage_reset();fake.content=fake_content;content_calls=0; /* the fixture's own reads already spent a scan */
     assert(core_read().valid && uss_derived_support_count==3 && content_calls==7);
     content_calls=0;assert(core_read().valid && !content_calls);
     fake.content=NULL;uss_lineage_reset();}

    /* Publication uses the same proof after the pair is attached. */
    capture_fixture();SUPPORT_ROW_KIND(0);uss_support_proof_reset();
    assert(sample(&capture_input.owner));reads=module_calls=session_calls=0;
    /* observe() runs after a Core observation was accepted earlier; emulate that lasting proof. */
    assert(support_proof_note(capture_input.ordinary_identity));
    assert(observe(&captured) && captured.valid);
    /* A non-zero native kind is never promoted, even with a lasting proof for the identity. */
    SUPPORT_ROW_KIND(1);uss_support_proof_reset();(void)support_proof_note(capture_input.ordinary_identity);assert(!observe(&captured));
    puts("custom support: native0 accepted only by support-slot identity proof, utility clones found by lineage once, bounded tables, stored member kept, unproven/other native kinds refused, bounded table, Core/member/lifecycle/publication, every read failure PASS");
    return 0;
}
