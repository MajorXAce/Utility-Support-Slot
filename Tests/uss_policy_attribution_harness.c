/* Player-policy refusal attribution over owned memory. Diagnostic-only: SCENARIO lines (result, reads) must match the previous
 * source exactly (compile with -DUSS_ATTRIBUTION_BASELINE against it); only the named reason text is new. No game access. */
#define USS_PLAYER_TEST_MAIN player_control_regression_checks
#include "uss_player_control_harness.c"
#include <stdarg.h>
typedef struct PolicyScenario {const char* name;const char* expected;int accepted;void (*apply)(void);} PolicyScenario;
static void policy_clear(void){}
static void policy_not_foreground(void){foreground_ok=0;}
static void policy_binding(void){input_source.binding_capture=1;}
static void policy_chat(void){*fixture(INPUT_UI+0x10,8)=1;}
static void policy_escape(void){put32(fixture(INPUT_UI+0x429c,24),1);put32(fixture(INPUT_UI+0x429c,24)+20,1);}
static void policy_ui_unreadable(void){fail_at=reads+1;} /* the first UI read fails: unknown, never chat or escape */
static void policy_foreground_and_chat(void){foreground_ok=0;*fixture(INPUT_UI+0x10,8)=1;} /* first executed branch wins; later ones stay unknown */
static void run_policy_scenario(const PolicyScenario* scenario)
{
    UssCoreFlags flags;int result;unsigned reads_before;
    player_fixture();fail_at=0;foreground_ok=1;input_source.binding_capture=0;
    scenario->apply();reads_before=reads;result=uss_player_sdk_policy(&input_source,&flags);fail_at=0;
    printf("SCENARIO %s result=%d reads=%u\n",scenario->name,result,reads-reads_before);
    assert(!!result==scenario->accepted);
#ifndef USS_ATTRIBUTION_BASELINE
    assert(input_source.reason && !strcmp(input_source.reason,scenario->expected));
#else
    (void)scenario->expected;
#endif
    foreground_ok=1;input_source.binding_capture=0;put32(fixture(INPUT_UI+0x429c,24),0);put32(fixture(INPUT_UI+0x429c,24)+20,0);*fixture(INPUT_UI+0x10,8)=0;
}
static char watch_log[8][200];static unsigned watch_logs;static const char* watch_reason_text="PLAYER_CHAT_ACTIVE";
static void watch_log_line(HfMod* mod,const char* format,...)
{va_list args;(void)mod;va_start(args,format);if(watch_logs<8)vsnprintf(watch_log[watch_logs],sizeof(watch_log[0]),format,args);va_end(args);++watch_logs;}
static const char* watch_reason(void* user){(void)user;return watch_reason_text;}
#ifndef USS_ATTRIBUTION_BASELINE
static void sustained_watch_checks(void)
{
    UssGameplayLoop loop;HfApi api=fake;unsigned episode;uint64_t base=1000;
    memset(&loop,0,sizeof(loop));api.log=watch_log_line;loop.api=&api;loop.mod=MOD;loop.io.policy_reason=watch_reason;
    watch_logs=0;
    /* A normal pause (menu, chat, alt-tab) shorter than the threshold never logs, however often it is sampled. */
    for(episode=0;episode<5;++episode){
        uss_gameplay_loop_policy_watch(&loop,base,1);uss_gameplay_loop_policy_watch(&loop,base+USS_POLICY_SUSTAINED_MS-1,1);
        uss_gameplay_loop_policy_watch(&loop,base+USS_POLICY_SUSTAINED_MS+5,0);base+=100000; /* ends before the threshold: reset */
    }
    assert(!watch_logs);
    /* One unbroken pause logs once, with its duration and the named reason, then stays quiet. */
    uss_gameplay_loop_policy_watch(&loop,base,1);uss_gameplay_loop_policy_watch(&loop,base+USS_POLICY_SUSTAINED_MS,1);
    assert(watch_logs==1 && strstr(watch_log[0],"30000 ms") && strstr(watch_log[0],"PLAYER_CHAT_ACTIVE"));
    uss_gameplay_loop_policy_watch(&loop,base+90000,1);uss_gameplay_loop_policy_watch(&loop,base+900000,1);assert(watch_logs==1);
    /* A new episode may log again, up to the session cap; a missing reason callback is stated, not guessed. */
    loop.io.policy_reason=NULL;
    for(episode=0;episode<10;++episode){
        uss_gameplay_loop_policy_watch(&loop,base+1000000,0);
        uss_gameplay_loop_policy_watch(&loop,base+1000000+episode*200000,1);
        uss_gameplay_loop_policy_watch(&loop,base+1000000+episode*200000+USS_POLICY_SUSTAINED_MS,1);
    }
    assert(watch_logs==USS_POLICY_SUSTAINED_LOG_CAP && loop.policy_block_logs==USS_POLICY_SUSTAINED_LOG_CAP);
    assert(strstr(watch_log[1],"REASON_UNAVAILABLE"));
    /* A clock that moves backwards never produces a negative or huge duration. */
    memset(&loop,0,sizeof(loop));loop.api=&api;loop.mod=MOD;watch_logs=0;
    uss_gameplay_loop_policy_watch(&loop,5000,1);uss_gameplay_loop_policy_watch(&loop,100,1);assert(!watch_logs);
}
#endif
int main(void)
{
    static const PolicyScenario scenarios[]={
        {"clear",NULL,1,policy_clear},
        {"not_foreground","PLAYER_NOT_FOREGROUND",0,policy_not_foreground},
        {"binding_capture","PLAYER_BINDING_CAPTURE",0,policy_binding},
        {"chat","PLAYER_CHAT_ACTIVE",0,policy_chat},
        {"escape","PLAYER_ESCAPE_ACTIVE",0,policy_escape},
        {"ui_unreadable","PLAYER_UI_READ_UNKNOWN",0,policy_ui_unreadable},
        {"foreground_and_chat","PLAYER_NOT_FOREGROUND",0,policy_foreground_and_chat},
    };
    unsigned i;
    assert(player_control_regression_checks()==0);
    for(i=0;i<sizeof(scenarios)/sizeof(scenarios[0]);++i){
        PolicyScenario scenario=scenarios[i];
        if(!scenario.accepted && !scenario.expected)return 1;
        if(scenario.accepted){
#ifndef USS_ATTRIBUTION_BASELINE
            scenario.expected="PLAYER_POLICY_CURRENT";
#endif
        }
        run_policy_scenario(&scenario);
    }
#ifndef USS_ATTRIBUTION_BASELINE
    sustained_watch_checks();
    {UssCoreFlags flags;unsigned n;player_fixture();input_source.reason="PLAYER_CHAT_ACTIVE";
     input_source.mod=NULL;n=reads;assert(!uss_player_sdk_policy(&input_source,&flags));
     assert(!strcmp(input_source.reason,"PLAYER_POLICY_UNKNOWN")&&reads==n);}

#endif
    puts("Policy attribution: each executed branch named (foreground, capture, UI unknown, chat, escape); sustained pause logged once, capped, never for short pauses PASS");
    return 0;
}
