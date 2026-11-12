#ifdef NDEBUG
#error Capture diagnostic checks require active assertions
#endif
#define USS_CAPTURE_TEST_MAIN producer_regression_checks
#include "publication_capture_harness.c"
#include "../src/capture_diagnostic.h"
static UssCaptureRequest diagnostic_request;
static unsigned request_present,request_reads,identity_writes,identity_write_ok;
static UssCaptureRequest saved_identity;
static int diagnostic_write(void* user,const UssCaptureRequest* out)
{(void)user;++identity_writes;saved_identity=*out;return (int)identity_write_ok;}
static uint64_t diagnostic_now;
static uint32_t diagnostic_thread;
static uint64_t diagnostic_clock(void* user){(void)user;return diagnostic_now;}
static uint32_t diagnostic_thread_id(void* user){(void)user;return diagnostic_thread;}
static int diagnostic_read(void* user,UssCaptureRequest* out)
{(void)user;++request_reads;*out=diagnostic_request;return (int)request_present;}
static UssCaptureDiagnostic diagnostic_fixture(UssCaptureDiagnosticServices* services)
{
    UssCaptureProducer p;UssCaptureDiagnostic out={0};
    capture_fixture();p=producer();memset(services,0,sizeof(*services));
    out.started_ms=100;out.session.magic=USS_CAPTURE_MAGIC;out.session.version=USS_CAPTURE_VERSION;
    out.session.pid=44;out.session.sdk=HF_MOD_SDK_VERSION;out.session.process_started=1234;out.session.nonce=99;out.session.build=USS_CAPTURE_BUILD;
    memset(&diagnostic_request,0,sizeof(diagnostic_request));diagnostic_request.session=out.session;
    diagnostic_request.local_unit=11;diagnostic_request.avatar=22;diagnostic_request.utility=103;diagnostic_request.ordinary=102;
    memcpy(diagnostic_request.utility_identity,capture_input.utility_identity,8);memcpy(diagnostic_request.ordinary_identity,capture_input.ordinary_identity,8);
    diagnostic_request.role=HF_SESSION_HOST;diagnostic_request.issued_ms=100;
    diagnostic_request.local_unit=0;diagnostic_request.mode=1;
    memset(diagnostic_request.utility_identity,0,8);memset(diagnostic_request.ordinary_identity,0,8);
    identity_writes=0;identity_write_ok=1;memset(&saved_identity,0,sizeof(saved_identity));
    diagnostic_thread=7;diagnostic_now=1000;request_present=1;request_reads=0;
    services->clock_ms=diagnostic_clock;services->thread_id=diagnostic_thread_id;
    services->read_request=diagnostic_read;services->write_identity=diagnostic_write;services->platform=p.platform;return out;
}
#ifndef USS_DIAGNOSTIC_TEST_MAIN
#define USS_DIAGNOSTIC_TEST_MAIN main
#endif
static void identity_ready(UssCaptureDiagnostic* control,UssCaptureDiagnosticServices* services)
{
    uss_capture_diagnostic_tick(control,&fake,MOD,services);
    assert(!control->terminal && control->identity_attempted && control->identity_valid && identity_writes==1);
    assert(saved_identity.local_unit==11 && saved_identity.avatar==22 && saved_identity.utility==103 && saved_identity.ordinary==102);
    assert(strstr(last_log,"IDENTITY_READY") && strstr(last_log,"capture_run=0"));
}
static void prepare_full_request(void)
{
    diagnostic_now+=250;diagnostic_request=saved_identity;diagnostic_request.mode=0;
    diagnostic_request.issued_ms=diagnostic_now;
}
int USS_DIAGNOSTIC_TEST_MAIN(void)
{
    UssCaptureDiagnosticServices services;UssCaptureDiagnostic control;unsigned before,i,total;
    assert(producer_regression_checks()==0);
    control=diagnostic_fixture(&services);request_present=0;
    uss_capture_diagnostic_tick(&control,&fake,MOD,&services);assert(!reads && request_reads==1 && !logs);
    diagnostic_now+=1;uss_capture_diagnostic_tick(&control,&fake,MOD,&services);assert(request_reads==1 && !reads);
    diagnostic_now+=250;request_present=1;identity_ready(&control,&services);total=reads;
    before=reads;request_present=0;diagnostic_now+=250;
    uss_capture_diagnostic_tick(&control,&fake,MOD,&services);assert(reads==before && identity_writes==1 && !control.terminal);
    request_present=1;prepare_full_request();uss_capture_diagnostic_tick(&control,&fake,MOD,&services);
    assert(control.terminal && strstr(last_log,"HF034 TERMINAL OBSERVED") && strstr(last_log,"core_lifetime_proven=0"));
    before=reads;uss_capture_diagnostic_tick(&control,&fake,MOD,&services);assert(reads==before && logs==2);
    for(i=1;i<=total;++i) {
        control=diagnostic_fixture(&services);fail_at=i;
        uss_capture_diagnostic_tick(&control,&fake,MOD,&services);
        assert(control.terminal && !control.identity_valid && !identity_writes);
    }
    for(i=0;i<10;++i) {
        control=diagnostic_fixture(&services);
        switch(i){case 0:diagnostic_request.session.nonce++;break;case 1:diagnostic_request.mode=7;break;
        case 2:diagnostic_request.role=0;break;case 3:diagnostic_request.utility=102;break;
        case 4:diagnostic_request.avatar=0;break;case 5:diagnostic_request.utility_identity[0]=1;break;
        case 6:diagnostic_now=30101;break;case 7:diagnostic_request.issued_ms=1001;break;
        case 8:diagnostic_request.issued_ms=99;break;case 9:request_present=(unsigned)-1;break;}
        uss_capture_diagnostic_tick(&control,&fake,MOD,&services);
        assert(control.terminal && !reads && !identity_writes && logs==1 && strstr(last_log,"REFUSED"));
    }
    for(i=0;i<6;++i) {
        control=diagnostic_fixture(&services);identity_ready(&control,&services);prepare_full_request();before=reads;
        switch(i){case 0:diagnostic_request.local_unit++;break;case 1:diagnostic_request.ordinary_identity[0]^=1;break;
        case 2:diagnostic_request.mode=1;break;case 3:diagnostic_now+=120001;diagnostic_request.issued_ms=diagnostic_now;break;
        case 4:diagnostic_request.issued_ms=control.identity.issued_ms-1;break;case 5:diagnostic_request.role=HF_SESSION_JOINER;break;}
        uss_capture_diagnostic_tick(&control,&fake,MOD,&services);
        assert(control.terminal && reads==before && identity_writes==1 && strstr(last_log,"REFUSED"));
    }
    for(i=0;i<4;++i) {
        control=diagnostic_fixture(&services);
        switch(i){case 0:put32(fixture(0x70000700,144)+96+4,23);break;
        case 1:put32(fixture(0x70000700,144)+96+12,1);break;
        case 2:memset(fixture(0x70001900,24),0,8);break;
        case 3:put32(fixture(0x70000300,48)+8,103);break;}
        uss_capture_diagnostic_tick(&control,&fake,MOD,&services);
        assert(control.terminal && !identity_writes && !control.identity_valid);
    }
    control=diagnostic_fixture(&services);identity_ready(&control,&services);prepare_full_request();
    put32(fixture(0x70002900,24),123);
    uss_capture_diagnostic_tick(&control,&fake,MOD,&services);
    assert(control.terminal && strstr(last_log,"REFUSED") && identity_writes==1);
    control=diagnostic_fixture(&services);identity_write_ok=0;
    uss_capture_diagnostic_tick(&control,&fake,MOD,&services);assert(control.terminal && !control.identity_valid && identity_writes==1);
    control=diagnostic_fixture(&services);diagnostic_request.avatar=23;
    uss_capture_diagnostic_tick(&control,&fake,MOD,&services);assert(control.terminal && !identity_writes);
    control=diagnostic_fixture(&services);control.callback_thread=8;
    uss_capture_diagnostic_tick(&control,&fake,MOD,&services);assert(control.terminal && !reads && !request_reads);
    control=diagnostic_fixture(&services);diagnostic_now=1800101;
    uss_capture_diagnostic_tick(&control,&fake,MOD,&services);assert(control.terminal && !reads && !request_reads);
    printf("identity preflight baseline: %u reads; all partial failures refused\n",total);
    puts("two-phase diagnostic: explicit identity then fresh capture, idle no reads, no replay, bound result/age/thread/TTL and I/O refusal PASS");
    return 0;
}
