/* Missing lookup is distinct from destruction, in both real/stored roles. */
#define USS_RECOVERY_TEST_MAIN loss_regressions
#include "uss_recovery_harness.c"
static void omit_lookup(uint32_t entity)
{
 unsigned i;unsigned char* data=fixture(0x70000800,64);
 for(i=0;i<8;++i)if(u32(data+i*8)==entity){put32(data+i*8,UINT32_MAX);put32(data+i*8+4,UINT32_MAX);return;}
 assert(0);
}
int main(void)
{
 unsigned utility,source,i,scenes=0;for(utility=0;utility<2;++utility)for(source=0;source<3;++source){
  uint32_t stored=utility?102:103;uintptr_t direct=utility?0x70001900:0x70002900,row=MAPPING+USS_FIXTURE_RECORDS+(utility?1:0)*24;
  recovery_scene(utility,1,HF_SESSION_HOST);assert(recovery.identity.valid && recovery.identity.entries[0].row_valid && recovery.identity.entries[1].row_valid);
  omit_lookup(stored);
  if(source==1)fixture(direct,24)[0]^=1; /* native row only */
  if(source==2)fixture(row,24)[0]^=1;    /* direct only */
  for(i=0;i<10;++i){replacement_now+=100;assert(recovery_update()==USS_CONTROL_WORK);}
  assert(automatic_runtime.life.owner.pair_present && !recovery_effects && !recovery_sends && !recovery_drops);
  assert(recovery.work.phase==USS_RECOVERY_IDLE && !strcmp(recovery.reason,"STORED_IDENTITY_RETAINED"));++scenes;
 }
 printf("IDENTITY_RETENTION_PASS %u full-loop lookup-only/direct-only/row-only scenes; pair retained, zero native effects\n",scenes);return 0;
}
