/* Event-bound weapon notification acknowledgment, translated from Lua
 * hud_notification_ack.lua. No idle polling or blanket pickup suppression.
 * Metadata/target/attachment/root/private-writable proofs belong to the reader. */
#ifndef USS_HUD_ACK_H
#define USS_HUD_ACK_H
#include <stdint.h>
#include <string.h>
enum {USS_HUD_DRAW=1,USS_HUD_BACKGROUND,USS_HUD_NEW_UTILITY,USS_HUD_RESTORE};
enum {USS_HUD_SKIPPED=0,USS_HUD_ACKED,USS_HUD_DISABLED};
typedef struct UssHudEvent {uint64_t serial,epoch;uint32_t kind,from,target,outgoing;} UssHudEvent;
typedef struct UssHudSnapshot {
    uint64_t epoch,owner,inventory,row,root;
    uint32_t avatar,active_index;
    unsigned valid,context_verified,private_writable,metadata_verified,record_verified;
    unsigned char record[40],display[12],selection[24];
} UssHudSnapshot;
typedef struct UssHudAckState {uint64_t epoch,last_event,writes;unsigned disabled,busy;const char* reason;} UssHudAckState;
typedef struct UssHudAckServices {
    void* user;
    UssHudSnapshot (*observe)(void*,const UssHudEvent*);
    int (*write)(void*,const UssHudSnapshot*,unsigned,const unsigned char*,unsigned);
} UssHudAckServices;
static inline uint32_t uss_hud_word(const unsigned char* bytes,unsigned at)
{uint32_t value;memcpy(&value,bytes+at,4);return value;}
static inline int uss_hud_context_same(const UssHudSnapshot* a,const UssHudSnapshot* b)
{
    return a->valid && b->valid && a->context_verified && b->context_verified && a->private_writable && b->private_writable &&
        a->epoch && a->epoch==b->epoch && a->owner && a->owner==b->owner && a->inventory && a->inventory==b->inventory &&
        a->row && a->row==b->row && a->root && a->root==b->root && a->avatar && a->avatar==b->avatar;
}
static inline int uss_hud_record_eligible(const UssHudEvent* event,const UssHudSnapshot* s)
{
    uint32_t count=uss_hud_word(s->record,16),pending=uss_hud_word(s->record,20),current=uss_hud_word(s->record,28),previous=uss_hud_word(s->record,32);
    if(!event || !event->serial || event->epoch!=s->epoch || !event->target || event->target==UINT32_MAX || !event->outgoing ||
        event->outgoing==UINT32_MAX || !uss_hud_context_same(s,s) || !s->metadata_verified || uss_hud_word(s->record,0)!=10)return 0;
    if(event->kind==USS_HUD_RESTORE){
        unsigned removal=pending==UINT32_MAX && count==0 && uss_hud_word(s->record,24)==0 && current==event->outgoing && previous==event->outgoing;
        unsigned current_member=current==event->outgoing || current==event->target || (!count && !current);
        unsigned previous_member=previous==event->outgoing || previous==event->target || (!count && !previous);
        return !event->from && (pending==0 || removal) && count<=1 && uss_hud_word(s->record,24)==count && current_member && previous_member;
    }
    if(event->kind!=USS_HUD_DRAW && event->kind!=USS_HUD_BACKGROUND && event->kind!=USS_HUD_NEW_UTILITY)return 0;
    return event->from && count==1 && uss_hud_word(s->record,24)==1 &&
        (pending==0 || (pending==1 && s->active_index==10)) && previous==current &&
        (current==event->from || (event->kind==USS_HUD_NEW_UTILITY && current==event->target));
}
static inline unsigned uss_hud_acknowledge(UssHudAckState* state,const UssHudAckServices* io,const UssHudEvent* event)
{
    UssHudSnapshot initial,current,after;unsigned char expected[40],next[40];unsigned index,count,offsets[3]={4,32,16},lengths[3]={12,4,4};
    if(!state || !io || !io->observe || !io->write || !event || state->disabled || state->busy)return USS_HUD_SKIPPED;
    if(event->serial<=state->last_event || (state->epoch && state->epoch!=event->epoch))return USS_HUD_SKIPPED;
    initial=io->observe(io->user,event);if(!uss_hud_record_eligible(event,&initial)){
        state->reason=initial.valid?"HUD_RECORD_INELIGIBLE":"HUD_OBSERVATION_UNAVAILABLE";
        return USS_HUD_SKIPPED;
    }
    state->epoch=event->epoch;state->last_event=event->serial;state->busy=1;
    memcpy(expected,initial.record,40);count=event->kind==USS_HUD_RESTORE && !uss_hud_word(expected,16)?3:2;
    for(index=0;index<count;++index){
        unsigned offset=offsets[index],length=lengths[index];uint32_t value=index==1?event->target:1;
        memcpy(next,expected,40);if(index==0)memcpy(next+offset,initial.display,length);else memcpy(next+offset,&value,4);
        if(!memcmp(next,expected,40))continue;
        current=io->observe(io->user,event);
        if(!uss_hud_context_same(&initial,&current) || memcmp(current.record,expected,40)){
            state->reason="HUD_CONTEXT_CHANGED";state->busy=0;return USS_HUD_SKIPPED;}
        ++state->writes; /* Mark before write; partial/uncertain effects never retry. */
        if(!io->write(io->user,&current,offset,next+offset,length))goto disable;
        after=io->observe(io->user,event);
        if(!uss_hud_context_same(&initial,&after) || memcmp(after.record,next,40))goto disable;
        memcpy(expected,next,40);
    }
    state->busy=0;state->reason="HUD_ACKED";return USS_HUD_ACKED;
disable:
    state->busy=0;state->disabled=1;state->reason="HUD_WRITE_OR_READBACK_FAILED";return USS_HUD_DISABLED;
}
#endif
