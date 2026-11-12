#include "../src/uss_supply_reader.h"
#define main amount_regression_checks
#include "uss_refill_amount_harness.c"
#undef main
static unsigned char inventory_header[96],inventory_units[8],avatar[24],counts_row[8];
static unsigned char pack_header[88],pack_hash[32],pack_units[8],pack_descriptor[24],pack_row[8];
static int supply_memory(void* p,uintptr_t at,void* out,size_t size)
{
    const struct {uintptr_t at;void* data;size_t size;} extra[]={
        {0x29000000,inventory_header,96},{0x29100000,inventory_units,8},{0x29300000,counts_row,8},{0x29400000,avatar,24},
        {0x2a000000,pack_header,88},{0x2a100000,pack_hash,32},{0x2a200000,pack_row,8},{0x2a300000,pack_units,8},{0x2a400000,pack_descriptor,24}};
    uintptr_t pointer_value=0;unsigned i;
    if(at==0x10000000+0x3326738)pointer_value=0x29000000;
    if(at==0x10000000+0x33265e8)pointer_value=0x2a000000;
    if(pointer_value && size==8){if(++reads==fail_at)return 0;memcpy(out,&pointer_value,8);return 1;}
    for(i=0;i<9;++i)if(at>=extra[i].at && at-extra[i].at<=extra[i].size && size<=extra[i].size-(at-extra[i].at)){
        if(++reads==fail_at)return 0;memcpy(out,(unsigned char*)extra[i].data+at-extra[i].at,size);return 1;}
    return read_memory(p,at,out,size);
}
static UssRefillTarget supply_fixture(void)
{
    UssRefillTarget t={0};static const unsigned char id[8]={0x58,0x9a,0x23,0x09,0x71,0xa4,0xf9,0x4e};
    amount_fixture(1,0);memset(inventory_header,0,96);memset(avatar,0,24);memset(pack_header,0,88);
    memset(pack_hash,0xff,32);memset(pack_descriptor,0,24);
    pointer(inventory_header+0x40,0x29100000);pointer(inventory_header+0x50,0x29200000);pointer(inventory_header+0x58,0x29300000);
    pointer(inventory_units,0x29400000);word(avatar+8,2);word(counts_row,2);word(counts_row+4,4);
    pointer(pack_header+0x20,0x2a100000);word(pack_header+0x28,4);word(pack_header+0x2c,UINT32_MAX);word(pack_header+0x30,1);
    pointer(pack_header+0x38,0x2a300000);pointer(pack_header+0x50,0x2a200000);pointer(pack_units,0x2a400000);
    word(pack_hash,12);word(pack_hash+4,0);memcpy(pack_descriptor,id,8);word(pack_descriptor+8,12);word(pack_row,3);word(pack_row+4,17);
    t.epoch=t.context_generation=t.pair_generation=t.guard_generation=1;t.owner=1;t.inventory=0x29000000;
    t.ammo_manager=0x20000000;t.ammo_unit=0x23000000;t.avatar=2;t.target=5;t.real_support=4;
    t.primary=10;t.secondary=11;t.backpack=12;t.kind=1;t.reserve=12;t.direct_amount=3;t.identity[0]=7;
    t.valid=t.eligible=t.stored=t.non_expendable=t.ordinary=1;return t;
}
int main(void)
{
    UssRefillMemoryIo io={NULL,0x10000000,supply_memory,context};UssRefillTarget t;
    UssSupplySnapshot snapshot,before;UssSupplyObserver observer={0};UssRefillEvidence event;
    unsigned char ids[3][8]={{1},{2},{3}};uint32_t counts[2];unsigned baseline,i;uint64_t serial=10;
    (void)amount_regression_checks;
    t=supply_fixture();assert(uss_supply_counts(&io,&t,0x29200000,counts) && counts[0]==2 && counts[1]==4);baseline=reads;
    for(i=1;i<=baseline;++i){t=supply_fixture();fail_at=i;counts[0]=42;counts[1]=43;
        assert(!uss_supply_counts(&io,&t,0x29200000,counts) && counts[0]==42 && counts[1]==43);}
    t=supply_fixture();assert(!uss_supply_counts(&io,&t,0x29200001,counts));
    word(counts_row+4,101);assert(!uss_supply_counts(&io,&t,0x29200000,counts));
    t=supply_fixture();assert(uss_supply_read(&io,&t,0x29200000,ids,&snapshot) && snapshot.signal_mask==24 && snapshot.pack.reserve==3);
    assert(!uss_supply_observe(&observer,&snapshot,1000,1,0,&serial,&event));
    word(pack_row,2);word(counts_row,3);
    assert(uss_supply_read(&io,&t,0x29200000,ids,&snapshot));
    assert(!uss_supply_observe(&observer,&snapshot,1025,1,0,&serial,&event));
    assert(uss_supply_read(&io,&t,0x29200000,ids,&snapshot));
    assert(uss_supply_observe(&observer,&snapshot,1225,1,0,&serial,&event) && serial==11);
    assert(event.pack_entity==12 && event.charges_before==3 && event.charges_after==2);
    before=snapshot;word(row,13);assert(!uss_supply_read(&io,&t,0x29200000,ids,&snapshot) && !memcmp(&before,&snapshot,sizeof(snapshot)));
    t=supply_fixture();pack_descriptor[0]=0;assert(!uss_supply_read(&io,&t,0x29200000,ids,&snapshot));
    t=supply_fixture();changed=1;assert(!uss_supply_read(&io,&t,0x29200000,ids,&snapshot));
    puts("USS concrete Supply Pack: charges/counts, every counts read fault, unknown optional ammo, actual snapshot-to-event, target/pack identity/context refusal PASS");return 0;
}
