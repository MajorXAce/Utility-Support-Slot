/* The real SDK sampler reads semantic support/cycle records in authored memory. */
#define USS_PLAYER_TEST_MAIN original_player_tests_not_run
#include "uss_player_control_harness.c"
static unsigned dedicated_calls;
static int forbidden_dedicated(void* user,const UssSettings* settings,unsigned* down){(void)user;(void)settings;(void)down;++dedicated_calls;return 0;}
int main(void){UssPlayerInputSample raw;unsigned before;player_fixture();player_settings.current.mode=USS_BIND_DEDICATED;
 assert(uss_settings_fallback(&player_settings,1,0));input_source.dedicated=forbidden_dedicated;
 *fixture(INPUT_ACTIONS+24*32,1)=1;put32(fixture(INPUT_ACTIONS+19*32,8)+4,0x3f800000);
 assert(formation_refresh(&fixture_runtime));assert(uss_player_sdk_sample(&input_source,&fixture_runtime.latest,&raw));assert(raw.support_pressed && raw.cycle_held==0x3f800000 && !raw.dedicated_down && !dedicated_calls);
 before=reads;assert(uss_player_sdk_managed_pending(&input_source,0,0));assert(reads>before);
 foreground_ok=0;assert(!uss_player_sdk_sample(&input_source,&fixture_runtime.latest,&raw));puts("DEFAULT_SDK_SAMPLER_PASS semantic keyboard/controller actions; dedicated sampler not called; foreground guard retained");return 0;}
