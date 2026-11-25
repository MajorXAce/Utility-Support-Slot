/* Coherent constructor-qualified aliases over authored memory; no game access. */
#define USS_CORE_TEST_MAIN legacy_core_checks
#include "core_observation_harness.c"
#include "../src/uss_member_identity_source.h"
static void relocate_records(int32_t offset)
{
 unsigned i;uint64_t start;assert(eel_record_address(MAPPING,offset,0,&start));
 for(i=0;i<used;++i)if(segments[i].at>=MAPPING+USS_FIXTURE_RECORDS && segments[i].at<MAPPING+USS_FIXTURE_RECORDS+32*24)
  segments[i].at=(uintptr_t)start+(segments[i].at-MAPPING-USS_FIXTURE_RECORDS);
 fixture_avatar_record=(uintptr_t)start+3*24;putptr(fixture(0x70000200,8),fixture_avatar_record);
 for(i=0;i<8;++i){const EelSite* site=&eel_sites[i];int32_t raw=(offset+site->add)/(int32_t)site->scale;
  assert((int64_t)raw*site->scale-site->add==offset);memcpy(fixture(BASE+site->rva,site->size+4)+site->size,&raw,4);}
}
static unsigned observe_layout(int32_t offset)
{
 UssCoreObservation current;core_fixture();relocate_records(offset);reads=0;current=core_read();
 assert(current.valid && current.inventory.owner_records==MAPPING+offset && core_apply(&current));
 assert(current.inventory.avatar==22 && current.inventory.roots[0]==MAPPING+0x1440);return reads;
}
int main(void)
{
 const int32_t offsets[]={0xF32F18,0x8261C78,-0x8CD0E8};unsigned i,j,total;UssCoreObservation current;
 for(i=0;i<3;++i){total=observe_layout(offsets[i]);
  for(j=1;j<=total;++j){core_fixture();relocate_records(offsets[i]);reads=0;fail_at=j;current=core_read();assert(!current.valid);}
  printf("LAYOUT offset=%d every %u partial-read positions refused\n",offsets[i],total);
 }
 core_fixture();relocate_records(offsets[1]);putptr(fixture(BASE+0x3326B90,8),MAPPING+0x1448);
 current=core_read();assert(!current.valid && last_reader.detail && !strcmp(last_reader.detail,"OWNER_ENTITY_MANAGER_MISMATCH"));
 for(i=0;i<8;++i){core_fixture();relocate_records(offsets[1]);fixture(BASE+eel_sites[i].rva,eel_sites[i].size+4)[0]^=1;assert(!core_read().valid);}
 {UssIdentityCache cache={0};UssCorePlatform platform={0};UssIdentityResult result;UssPairMember utility={0},ordinary={0};
  core_fixture();relocate_records(offsets[1]);current=core_read();assert(core_apply(&current));
  utility.entity=103;ordinary.entity=102;memcpy(utility.identity,capture_input.utility_identity,8);memcpy(ordinary.identity,capture_input.ordinary_identity,8);
  assert(uss_pair_owner_set_pair(&core_owner,7,0,&utility,&ordinary));current=core_read();assert(current.valid && current.pair_matched);
  platform.thread_id=core_thread;assert(uss_identity_seed(&cache,&fake,MOD,&platform,&core_owner,&current));assert(cache.entries[0].row_valid && cache.entries[0].row==MAPPING+offsets[1]);
  fixture(0x70002900,24)[0]^=1;
  result=uss_identity_query(&cache,&fake,MOD,&platform,&core_owner,&current.inventory,&core_owner.utility);
  assert(result.row_present && !result.direct_present && result.status==USS_IDENTITY_PRESENT);
  current.inventory.owner_records+=0x100000;
  result=uss_identity_query(&cache,&fake,MOD,&platform,&core_owner,&current.inventory,&core_owner.utility);
  assert(!result.row_present && result.status==USS_IDENTITY_UNKNOWN);
 }
 puts("HELLFORGE_LAYOUT_PASS vanilla/positive/negative shared storage, exact alias/opcode refusal and current-row retention range");return 0;
}
