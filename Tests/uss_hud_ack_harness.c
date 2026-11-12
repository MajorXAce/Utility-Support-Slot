#include <assert.h>
#include <stdio.h>
#include "../src/uss_hud_ack.h"
static UssHudSnapshot current;static unsigned calls,fail_at,other_change;
static UssHudSnapshot observe(void* p,const UssHudEvent* e){(void)p;(void)e;return current;}
static int write_record(void* p,const UssHudSnapshot* before,unsigned offset,const unsigned char* data,unsigned length)
{
    (void)p;assert(!memcmp(before->record,current.record,40));++calls;
    assert((offset==4 && length==12) || (offset==32 && length==4) || (offset==16 && length==4));
    if(calls==fail_at)return 0;memcpy(current.record+offset,data,length);if(other_change)current.record[36]^=1;return 1;
}
static void word(unsigned offset,uint32_t value){memcpy(current.record+offset,&value,4);}
static UssHudEvent fixture(UssHudAckState* state)
{
    UssHudEvent event={1,1,USS_HUD_DRAW,10,11,10};memset(state,0,sizeof(*state));memset(&current,0,sizeof(current));
    current.epoch=current.owner=current.inventory=current.row=current.root=1;current.avatar=2;
    current.valid=current.context_verified=current.private_writable=current.metadata_verified=1;
    word(0,10);word(16,1);word(24,1);word(28,10);word(32,10);current.display[0]=9;
    calls=fail_at=other_change=0;return event;
}
int main(void)
{
    UssHudAckState state;UssHudEvent event;UssHudAckServices io={NULL,observe,write_record};unsigned i;
    event=fixture(&state);assert(uss_hud_acknowledge(&state,&io,&event)==USS_HUD_ACKED && calls==2);
    assert(uss_hud_word(current.record,32)==11 && uss_hud_word(current.record,28)==10 && uss_hud_word(current.record,20)==0);
    assert(uss_hud_acknowledge(&state,&io,&event)==USS_HUD_SKIPPED && calls==2);
    for(i=0;i<6;++i){event=fixture(&state);if(i==0)word(0,1);if(i==1)word(20,2);if(i==2)word(32,20);
        if(i==3)current.metadata_verified=0;if(i==4)current.private_writable=0;if(i==5)word(28,20);
        assert(uss_hud_acknowledge(&state,&io,&event)==USS_HUD_SKIPPED && !calls);}
    event=fixture(&state);word(20,1);assert(uss_hud_acknowledge(&state,&io,&event)==USS_HUD_SKIPPED);
    current.active_index=10;assert(uss_hud_acknowledge(&state,&io,&event)==USS_HUD_ACKED && uss_hud_word(current.record,20)==1);
    event=fixture(&state);event.kind=USS_HUD_RESTORE;event.from=0;word(16,0);word(24,0);word(20,UINT32_MAX);
    assert(uss_hud_acknowledge(&state,&io,&event)==USS_HUD_ACKED && calls==3);
    assert(uss_hud_word(current.record,16)==1 && uss_hud_word(current.record,20)==UINT32_MAX && uss_hud_word(current.record,24)==0);
    for(i=1;i<=2;++i){event=fixture(&state);fail_at=i;assert(uss_hud_acknowledge(&state,&io,&event)==USS_HUD_DISABLED && state.disabled && calls==i);
        event.serial=2;assert(uss_hud_acknowledge(&state,&io,&event)==USS_HUD_SKIPPED && calls==i);}
    event=fixture(&state);other_change=1;assert(uss_hud_acknowledge(&state,&io,&event)==USS_HUD_DISABLED && calls==1);
    puts("USS HUD ack: event-only exact weapon history, preserve resources/queued notices/removal, bounded fields and partial-write stop PASS");return 0;
}
