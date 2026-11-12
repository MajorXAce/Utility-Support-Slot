#include "../src/uss_native_binding_context.h"
#define USS_NATIVE_BINDING_READER_TEST_MAIN reader_tests
#include "uss_native_binding_reader_harness.c"
static unsigned label_reads,change_label;static uint32_t native_label;
static int read_context(void* user,uintptr_t at,void* out,size_t length)
{
    if(at==0x10000000+0x26438a0+4*(97*10) && length==4){
        uint32_t value=native_label;if(++label_reads>1 && change_label)++value;memcpy(out,&value,4);return 1;}
    return read_memory(user,at,out,length);
}
static UssMenuObservationIo context_fixture(void)
{
    UssMenuObservationIo io=fixture();io.read=read_context;label_reads=change_label=0;native_label=uss_binding_dormant_labels[7];
    uss_menu_put_word(buckets,328*3,10*65536);uss_menu_put_word(default_buckets,328*3,10*65536);return io;
}
#ifndef USS_BIND_CONTEXT_TEST_MAIN
#define USS_BIND_CONTEXT_TEST_MAIN main
#endif
int USS_BIND_CONTEXT_TEST_MAIN(void)
{
    UssNativeBindingContext result={0},before;UssNativeBindingCache live={0},defaults={0};UssMenuObservationIo io;
    assert(reader_tests()==0);io=context_fixture();
    assert(uss_native_binding_context_read(&io,10,0,&live,&defaults,&result));
    assert(result.live.owner==result.defaults.owner && result.live.mappings[0]!=result.defaults.mappings[0] && uss_native_binding_context_original(&result));
    before=result;io=context_fixture();change_label=1;
    assert(!uss_native_binding_context_read(&io,10,0,&live,&defaults,&result) && !memcmp(&result,&before,sizeof(result)));
    io=context_fixture();native_label=123;assert(uss_native_binding_context_read(&io,10,0,&live,&defaults,&result) && !uss_native_binding_context_original(&result));
    io=context_fixture();change_record=1;assert(!uss_native_binding_context_read(&io,10,0,&live,&defaults,&result));
    io=context_fixture();assert(!uss_native_binding_context_read(&io,10,0,&live,&live,&result));
    puts("USS native binding context: coherent label/live/default rereads, original label comparison, payload/label mutation and separate-cache refusal PASS");return 0;
}
