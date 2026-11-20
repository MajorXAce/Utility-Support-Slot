#include "../src/uss_native_menu_session.h"
#define USS_MENU_LIFECYCLE_TEST_MAIN lifecycle_tests
#include "uss_native_menu_lifecycle_harness.c"
static UssMenuSessionNode nodes[2];static unsigned allocations;
static void* own_allocate(void* user,size_t size){(void)user;assert(size==sizeof(nodes[0]));return allocations<2?&nodes[allocations++]:NULL;}
int main(void){
    UssMenuSession session={0};UssMenuSdkContext context={0};UssMenuSessionNode* first;UssMenuSessionNode* second;
    unsigned char inert[216]={0};unsigned before;
    assert(lifecycle_tests()==0);setup_lifecycle();uss_menu_put_word(inert,0,139);
    session.allocate=own_allocate;first=uss_menu_session_allocate(&session,&context);assert(first && allocations==1);
    assert(!uss_menu_session_allocate(&session,&context));before=effects;
    assert(uss_menu_session_prepare_node(first,&owner.observer,&owner.entry_reader,&fixture_settings,inert,sizeof(inert),L"build/session-settings.json"));
    assert(effects==before && first->writer.owner==&first->menu.owner && first->writer.sdk==&first->sdk);
    assert(first->menu.lifecycle.io.user==&first->writer && first->writer.owned==uss_menu_ownership_guard);
    assert(uss_menu_ownership_stage(&first->menu.owner,USS_MENU_PROOF_TAB) && effects==before);
    bad_pin=1;{UssMenuHostSnapshot unused;assert(!uss_menu_ownership_observe(&first->menu.owner,&unused));}
    assert(!strcmp(first->menu.owner.failure,"ENTRY_PINS") && first->menu.owner.failed_entry==0 && effects==before);bad_pin=0;
    assert(!uss_menu_session_retire(&session));uss_menu_put_word(ui,0x429c+20,0);
    assert(uss_menu_session_retire(&session) && !session.current && session.retained==first);
    setup_lifecycle();second=uss_menu_session_allocate(&session,&context);assert(second && second!=first && second->next==first);
    assert(uss_menu_session_prepare_node(second,&owner.observer,&owner.entry_reader,&fixture_settings,inert,sizeof(inert),L"build/session-settings.json"));
    {unsigned values[3]={0};put_pointer(module+0x347cf18,0x23000100);
     assert(uss_menu_lifecycle_step(&second->menu.lifecycle,0.016f,values,3)==USS_MENU_STEP_REFUSED);
     assert(!strcmp(second->menu.lifecycle.checkpoint,"INITIAL_IDENTITY") && !effects);
     assert(second->menu.lifecycle.last_observed.input!=second->menu.owner.identity.input);}
    assert(first->menu.retired && !strcmp(first->menu.rows.rows[0].title,"Binding mode") && session.count==2);
    uss_menu_put_word(ui,0x429c+20,0);assert(uss_menu_session_retire(&session));
    assert(!uss_menu_session_allocate(&session,&context) && session.stopped && session.count==2);
    /* A long sequence of retired screen lifetimes cannot retain unbounded nodes.
     * These nodes never enter native UI, so this fixture may free them afterward. */
    {
        UssMenuSession bounded={0};unsigned i,limit=USS_MENU_SESSION_MAX_BYTES/sizeof(UssMenuSessionNode);
        if(limit>USS_MENU_SESSION_MAX_LIFETIMES)limit=USS_MENU_SESSION_MAX_LIFETIMES;
        assert(limit && sizeof(UssMenuSessionNode)<=USS_MENU_SESSION_MAX_BYTES);
        bounded.allocate=uss_menu_session_heap;
        for(i=0;i<limit;++i){assert(uss_menu_session_allocate(&bounded,&context));bounded.current=NULL;}
        assert(!uss_menu_session_allocate(&bounded,&context)&&bounded.stopped&&bounded.storage_limited&&bounded.count==limit);
        for(i=0;i<10000;++i)assert(!uss_menu_session_allocate(&bounded,&context)&&bounded.count==limit);
        while(bounded.retained){UssMenuSessionNode* old=bounded.retained;bounded.retained=old->next;assert(HeapFree(GetProcessHeap(),0,old));}
        printf("USS retained menu budget: node=%zu bytes, max=%u lifetimes, max_retained=%zu bytes; 10000 overflow attempts allocate nothing\n",sizeof(UssMenuSessionNode),limit,(size_t)limit*sizeof(UssMenuSessionNode));
    }
    puts("USS menu session: stable allocation, SDK writer ownership, closed-only retirement, retained old storage and allocation-failure stop PASS");return 0;
}
