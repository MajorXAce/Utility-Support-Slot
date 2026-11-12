#define USS_REPLACEMENT_ADAPTER_TEST_MAIN adapter_regression_checks
#include "replacement_adapter_harness.c"
#include "../src/replacement_command.h"
static UssReplacementDiagnostic diagnostic;
static UssFormationRuntime diagnostic_runtime;
static UssFormationNative diagnostic_native;
static uint64_t diagnostic_clock(void* user){(void)user;return replacement_now;}
static int diagnostic_formation_invoke(void* user,UssFormationEffect effect,uintptr_t first,uintptr_t second,
    uintptr_t record,uintptr_t inventory,uint32_t avatar,uint32_t value,const UssInventoryMessage* message)
{return own_invoke(user,effect==USS_EFFECT_SELECT?USS_REPLACEMENT_SELECT:effect==USS_EFFECT_ATTACH?USS_REPLACEMENT_ATTACH:USS_REPLACEMENT_SEND,
    first,second,record,inventory,avatar,value,message);}
static void diagnostic_fixture(void)
{
    adapter_fixture(1,0,1,HF_SESSION_HOST);memset(&diagnostic,0,sizeof(diagnostic));memset(&diagnostic_runtime,0,sizeof(diagnostic_runtime));
    diagnostic_runtime.life=candidate_life;diagnostic_runtime.life.phase=USS_LIFE_FORMED;memset(&diagnostic_runtime.life.plan,0,sizeof(diagnostic_runtime.life.plan));
    diagnostic_runtime.history=hf048_history;diagnostic_runtime.history.last_serial=0;
    diagnostic_runtime.policy=core_flags;diagnostic_runtime.thread=7;
    memset(&diagnostic_native,0,sizeof(diagnostic_native));diagnostic_native.api=&fake;diagnostic_native.mod=MOD;
    diagnostic_native.platform=producer().platform;diagnostic_native.runtime=&diagnostic_runtime;diagnostic_native.invoke=diagnostic_formation_invoke;
    diagnostic_runtime.io=uss_formation_native_io(&diagnostic_native,diagnostic_clock);
    put32(fixture(0x70000300,48)+8,102);put32(fixture(0x70000700,240)+48+4,22);put32(fixture(0x70000700,240)+144+4,0);set_selected(1);
    assert(formation_refresh(&diagnostic_runtime) && diagnostic_runtime.view.supported_pair);
    diagnostic_runtime.publication.record=diagnostic_runtime.view.key;diagnostic_runtime.publication.record_present=diagnostic_runtime.publication.formed=1;
    diagnostic_runtime.publication.member=102;effect_calls=typed_calls=0;
}
static int diagnostic_command(unsigned action,unsigned sequence,uint32_t incoming)
{
    UssReplacementRequest r={0};r.base.action=action;r.base.sequence=sequence;r.base.role=HF_SESSION_HOST;r.incoming=incoming;
    if(incoming)memcpy(r.identity,fixture(0x70003900,24),8);
    return uss_replacement_diagnostic_command(&diagnostic,&diagnostic_native,&r,own_invoke,NULL);
}
#ifndef USS_REPLACEMENT_COMMAND_TEST_MAIN
#define USS_REPLACEMENT_COMMAND_TEST_MAIN main
#endif
int USS_REPLACEMENT_COMMAND_TEST_MAIN(void)
{
    UssFormationTicket ticket={USS_REPLACEMENT_MAGIC,1,44,13,1234,99,25480438,0};UssReplacementRequest command_request={0};unsigned i;
    assert(adapter_regression_checks()==0);
    command_request.base.ticket=ticket;command_request.base.sequence=1;command_request.base.action=USS_COMMAND_ORIGIN;command_request.base.role=HF_SESSION_HOST;command_request.base.issued_ms=99;
    assert(uss_replacement_request_valid(&ticket,&command_request,1,100));
    assert(!uss_formation_request_valid(&ticket,&command_request.base,1,100));
    command_request.base.action=USS_COMMAND_REPLACE;assert(!uss_replacement_request_valid(&ticket,&command_request,1,100));
    command_request.incoming=104;command_request.identity[0]=1;assert(uss_replacement_request_valid(&ticket,&command_request,1,100));
    command_request.reserved=1;assert(!uss_replacement_request_valid(&ticket,&command_request,1,100));command_request.reserved=0;
    diagnostic_fixture();assert(!diagnostic_command(USS_COMMAND_REPLACE,1,104) && !effect_calls);
    diagnostic_fixture();assert(diagnostic_command(USS_COMMAND_ORIGIN,1,0) && !effect_calls && diagnostic.origin.origin.real==102);
    assert(!diagnostic_command(USS_COMMAND_ORIGIN,2,0) && !effect_calls);
    assert(!diagnostic_command(USS_COMMAND_UTILITY,2,0) && !effect_calls);
    put32(fixture(0x70000300,48)+8,104);put32(fixture(0x70000700,240)+48+4,0);put32(fixture(0x70000700,240)+144+4,22);set_selected(1);
    assert(!diagnostic_command(USS_COMMAND_REPLACE,2,104) && !effect_calls);
    assert(diagnostic_command(USS_COMMAND_STATUS,2,0) && diagnostic.status.entity==104 && !effect_calls);
    assert(diagnostic_command(USS_COMMAND_REPLACE,3,104) && diagnostic.work.phase==USS_REPLACE_ACTIVE && !effect_calls);
    for(i=0;i<40 && diagnostic.work.phase==USS_REPLACE_ACTIVE;++i){replacement_now+=25;
        uss_replacement_tick(&diagnostic.work,&diagnostic_runtime.life,7,&diagnostic_runtime.history,&diagnostic_runtime.publication,
            &diagnostic.services,replacement_now,i+1);}
    assert(diagnostic.work.phase==USS_REPLACE_COMPLETE && diagnostic.work.real==102 && diagnostic_runtime.life.owner.ordinary.entity==102);
    assert(diagnostic_runtime.life.owner.utility.entity==104 && effect_calls>0);
    assert(!diagnostic_command(USS_COMMAND_ORIGIN,4,0));
    /* Merely referencing the production invoker does not call or bind a game target. */
    {int (*invoker)(void*,UssReplacementEffect,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uint32_t,uint32_t,const UssInventoryMessage*)=replacement_runtime_invoke;assert(invoker);}
    puts("HF052 command: separate80byte schema, fresh origin/status/named intent, real diagnostic path with OWN effects PASS");return 0;
}
