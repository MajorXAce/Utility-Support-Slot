/* Attachment-refusal attribution over owned memory. Diagnostic-only: each scenario prints its read count, effect count and
 * outcome so the SAME program can be compiled against the previous source and compared line by line (decisions and reads must
 * be identical); only the named reason (skipped when compiled with -DUSS_ATTRIBUTION_BASELINE against the previous source) is new. No game access. */
#define USS_NATIVE_TEST_MAIN formation_native_regression_checks
#include "formation_native_harness.c"
#include <stdarg.h>
static char attribution_log[8][200];static unsigned attribution_logs;
static void attribution_log_line(HfMod* mod,const char* format,...)
{va_list args;(void)mod;va_start(args,format);
 if(attribution_logs<8)vsnprintf(attribution_log[attribution_logs],sizeof(attribution_log[0]),format,args);
 va_end(args);++attribution_logs;}
static unsigned char* attach_interaction_row(uint32_t entity){return fixture(0x90001000,2*0x370)+(entity==102?1:0)*0x370;}
typedef struct AttachScenario {const char* name;const char* expected;void (*corrupt)(void);} AttachScenario;
static void corrupt_entry_pin(void){fixture(BASE+0x9AA290,64)[0]^=1;}
static void corrupt_interaction_gate(void){attach_interaction_row(102)[0xC9]=2;}
static void corrupt_nothing(void){}
static void run_attach_scenario(const AttachScenario* scenario)
{
    unsigned effects_before,reads_before,phase;
    native_fixture();fake.log=attribution_log_line;attribution_logs=0;memset(attribution_log,0,sizeof(attribution_log));
    scenario->corrupt();
    assert(uss_formation_start(&fixture_runtime,USS_PAIR_ORDINARY_FIRST,NULL));
    effects_before=fixture_native.effects;reads_before=reads;
    phase=uss_formation_tick(&fixture_runtime);
    printf("SCENARIO %s phase=%u reads=%u effects=%u attachments=%u\n",scenario->name,phase,reads-reads_before,
        fixture_native.effects-effects_before,attachments);
#ifndef USS_ATTRIBUTION_BASELINE
    if(scenario->expected){
        assert(fixture_native.reason && !strcmp(fixture_native.reason,scenario->expected));
        assert(attribution_logs>=1 && strstr(attribution_log[attribution_logs-1],scenario->expected));
        if(!strcmp(scenario->expected,"ATTACH_INTERACTION_NOT_ADMISSIBLE"))
            assert(strstr(attribution_log[attribution_logs-1],"gate=2") && strstr(attribution_log[attribution_logs-1],"zone=0x"));
        /* A repeated refusal with the same reason does not log again. */
        {unsigned count=attribution_logs;(void)uss_formation_tick(&fixture_runtime);
         assert(attribution_logs==count || strcmp(fixture_native.reason,scenario->expected));}
        assert(fixture_native.attach_logs>=1 && fixture_native.attach_logs<=8);
    }
#else
    (void)scenario->expected;
#endif
}
int main(void)
{
    static const AttachScenario scenarios[]={
        {"accepted_baseline",NULL,corrupt_nothing},
        {"entry_pin",   "ATTACH_ENTRY_PIN",corrupt_entry_pin},
        {"interaction", "ATTACH_INTERACTION_NOT_ADMISSIBLE",corrupt_interaction_gate},
    };
    unsigned i;
    assert(formation_native_regression_checks()==0);
    for(i=0;i<sizeof(scenarios)/sizeof(scenarios[0]);++i)run_attach_scenario(&scenarios[i]);
    puts("Attach attribution: each refusing boundary named; reads, effects and outcomes printed for old/new comparison PASS");
    return 0;
}
