#define USS_NATIVE_TEST_MAIN native_regression_checks
#include "formation_native_harness.c"
#include "../src/formation_command.h"
static void command_fixture(void)
{
    UssFormationIo io;UssCoreLifecycle empty;
    native_fixture();memset(&empty,0,sizeof(empty));io=fixture_runtime.io;
    assert(uss_formation_runtime_init(&fixture_runtime,&io,7,&core_flags));fixture_runtime.life=empty;
    put32(fixture(0x70000300,48)+8,102);put32(fixture(0x70000700,144)+48+4,22);
    put32(fixture(0x70000700,144)+96+4,0);
}
static unsigned hud_calls;static uint32_t hud_incoming;
static void own_hud_ack(void* user,uint32_t outgoing,uint32_t incoming,UssPairLabel label)
{(void)user;assert(outgoing && incoming && outgoing!=incoming && label);++hud_calls;hud_incoming=incoming;}
#ifndef USS_COMMAND_TEST_MAIN
#define USS_COMMAND_TEST_MAIN main
#endif
int USS_COMMAND_TEST_MAIN(void)
{
    UssFormationTicket ticket={USS_FORMATION_MAGIC,1,10,12,20,30,25480438,0};
    UssFormationRequest command_request={0};UssPairMember target;
    assert(native_regression_checks()==0);command_request.ticket=ticket;command_request.sequence=1;
    command_request.action=USS_COMMAND_REMEMBER;command_request.role=HF_SESSION_HOST;command_request.issued_ms=100;
    assert(uss_formation_request_valid(&ticket,&command_request,1,100));
    assert(!uss_formation_request_valid(&ticket,&command_request,2,100));
    assert(!uss_formation_request_valid(&ticket,&command_request,1,99));
    assert(!uss_formation_request_valid(&ticket,&command_request,1,30101));
    command_request.ticket.nonce++;assert(!uss_formation_request_valid(&ticket,&command_request,1,100));command_request.ticket=ticket;
    command_request.role=HF_SESSION_SOLO;assert(!uss_formation_request_valid(&ticket,&command_request,1,100));command_request.role=HF_SESSION_HOST;
    command_request.action=0;assert(!uss_formation_request_valid(&ticket,&command_request,1,100));
    command_fixture();assert(!uss_formation_command(&fixture_native,USS_COMMAND_FORM,0,HF_SESSION_HOST));
    assert(!strcmp(fixture_runtime.reason,"COMMAND_HISTORY_OR_ORDER") && !fixture_native.effects);
    command_fixture();assert(uss_formation_command(&fixture_native,USS_COMMAND_REMEMBER,0,HF_SESSION_HOST));
    assert(fixture_runtime.history.remembered && !fixture_runtime.life.owner.pair_present && !fixture_native.effects);
    assert(!uss_formation_command(&fixture_native,USS_COMMAND_REMEMBER,0,HF_SESSION_HOST));
    put32(fixture(0x70000300,48)+8,103);put32(fixture(0x70000700,144)+48+4,0);
    put32(fixture(0x70000700,144)+96+4,22);
    assert(uss_formation_command(&fixture_native,USS_COMMAND_FORM,0,HF_SESSION_HOST));finish_work();
    assert(fixture_runtime.life.phase==USS_LIFE_FORMED);
    assert(uss_formation_command(&fixture_native,USS_COMMAND_UTILITY,0,HF_SESSION_HOST));finish_work();
    assert(fixture_runtime.work.real==103);
    /* Every command asks for a fresh primary-held operator window. */
    assert(!uss_formation_command(&fixture_native,USS_COMMAND_ORDINARY,0,HF_SESSION_HOST));
    assert(!strcmp(fixture_runtime.reason,"COMMAND_PRIMARY_NOT_READY"));
    assert(runtime_select(NULL,&fixture_runtime.latest,1)==USS_PAIR_SERVICE_DONE);
    assert(uss_formation_command(&fixture_native,USS_COMMAND_ORDINARY,0,HF_SESSION_HOST));finish_work();
    target=fixture_runtime.life.owner.ordinary;assert(fixture_runtime.work.real==target.entity);
    fixture_runtime.io.acknowledge=own_hud_ack;hud_calls=0;
    assert(runtime_select(NULL,&fixture_runtime.latest,1)==USS_PAIR_SERVICE_DONE);
    assert(uss_formation_command(&fixture_native,USS_COMMAND_UTILITY,0,HF_SESSION_HOST));
    assert(hud_calls==0);finish_work();
    assert(hud_calls==1 && hud_incoming==fixture_runtime.life.owner.utility.entity);
    puts("Formation commands: nonce/sequence/TTL/role, remember/form/draw, explicit primary window PASS");return 0;
}
