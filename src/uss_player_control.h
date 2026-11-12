/* Normal player input -> accepted automatic/managed-draw services.
 * Input observations contain values, never native-call authority. A release is
 * required after focus/menu/mode/context changes, including the first sample. */
#ifndef USS_PLAYER_CONTROL_H
#define USS_PLAYER_CONTROL_H
#include "uss_automatic_controller.h"
#include "uss_settings.h"

typedef struct UssPlayerInputSample {
    unsigned known,foreground,ui_clear,support_pressed,dedicated_down;
    uint32_t cycle_held; /* Exact float bits 0x3f800000 mean held, as in Lua. */
    uintptr_t manager;
} UssPlayerInputSample;
typedef struct UssPlayerInput {
    uint64_t epoch,context,pair,guard,revision,last_ms,hold_since;
    uintptr_t manager,row;uint32_t avatar,active,hand,real,utility,ordinary;
    unsigned initialized,support_down,cycle_down,dedicated_down,armed,fired,transitioned;
} UssPlayerInput;
enum {USS_INPUT_WORK_STRICT=1,USS_INPUT_WORK_QUEUE_DRAW=2};
typedef struct UssPlayerGesture {unsigned known,hit,dedicated,interrupted,queue_requested;} UssPlayerGesture;

static inline void uss_player_input_reset(UssPlayerInput* input,UssSettingsState* settings)
{memset(input,0,sizeof(*input));if(settings)settings->release_required=1;}
/* One current Core snapshot supplies selected/held identity. The input reader
 * checks its action manager/record against this same snapshot, avoiding another
 * full inventory scan. Effect services still obtain fresh boundary reads. */
static inline UssPlayerGesture uss_player_input_poll(UssPlayerInput* input,UssSettingsState* settings,
    const UssCoreObservation* current,const UssPairOwner* owner,const UssPlayerInputSample* raw,
    uint64_t now,unsigned busy)
{
    UssPlayerGesture result={0};unsigned cycle,changed,edge,valid,dedicated_mode,dedicated_edge,support_enabled;uint32_t support,actual,selected;
    if(!input || !settings || !current || !owner || !raw || !raw->known ||
        !uss_settings_valid(&settings->current) || !current->valid || !current->mission ||
        !raw->foreground || !raw->ui_clear || raw->support_pressed>1 || raw->dedicated_down>1 ||
        !current->flags.known || !current->flags.eligible || current->flags.disabled ||
        current->flags.suspended || current->flags.paused || current->flags.down || current->flags.input){
        if(input)uss_player_input_reset(input,settings);return result;
    }
    result.known=1;dedicated_mode=settings->current.mode==USS_BIND_DEDICATED;
    support_enabled=!dedicated_mode || (owner->pair_present && current->inventory.slots[2]==owner->utility.entity);
    cycle=raw->cycle_held==UINT32_C(0x3f800000);
    support=current->inventory.slots[2];actual=current->inventory.held;
    selected=current->inventory.active_selector;
    changed=!input->initialized || input->epoch!=owner->epoch || input->context!=owner->context_generation ||
        input->pair!=owner->pair_generation || input->guard!=owner->guard_generation || input->revision!=settings->revision ||
        input->manager!=raw->manager || input->row!=current->inventory.row_address || input->avatar!=current->inventory.avatar ||
        now<input->last_ms;
    if(changed){
        uss_player_input_reset(input,settings);input->initialized=1;input->epoch=owner->epoch;
        input->context=owner->context_generation;input->pair=owner->pair_generation;input->guard=owner->guard_generation;
        input->revision=settings->revision;input->manager=raw->manager;input->row=current->inventory.row_address;
        input->avatar=current->inventory.avatar;input->active=selected;input->hand=actual;
        input->support_down=raw->support_pressed;input->cycle_down=cycle;input->dedicated_down=raw->dedicated_down;
    }
    dedicated_edge=dedicated_mode && raw->dedicated_down && !input->dedicated_down;
    edge=dedicated_edge || (support_enabled && ((raw->support_pressed && !input->support_down) || (cycle && !input->cycle_down)));
    if(busy){
        /* An explicitly queueable managed draw accepts another command. Other transactions
         * retain strict interruption semantics. A held initiating key is not
         * another command; native effects themselves are never retried. */
        if(busy==USS_INPUT_WORK_QUEUE_DRAW){
            /* A managed draw accepts ONE extra command. Controller cycling
             * still requires a deliberate hold, not its initial tap edge. */
            result.queue_requested=!changed && (dedicated_edge ||
                (support_enabled && raw->support_pressed && !input->support_down));
            result.dedicated=dedicated_edge;
            if(support_enabled && cycle && !changed){
                if(!input->cycle_down){input->hold_since=now;input->armed=1;input->fired=0;}
                if(input->armed && !input->fired && now-input->hold_since>=250){
                    result.queue_requested=1;input->fired=1;input->armed=0;}
            }
        }else{result.interrupted=!changed && edge;input->armed=0;}
        settings->release_required=1;
    }else if(uss_settings_input_ready(settings,raw->foreground,!raw->ui_clear,0,0,
        (dedicated_mode && raw->dedicated_down) || (support_enabled && (raw->support_pressed || cycle)))){
        valid=current->inventory.slots[0] && current->inventory.slots[1] &&
            selected>=1 && selected<=3;
        if(dedicated_edge){
            result.dedicated=1;
            uint32_t selected_entity=selected==1?current->inventory.slots[0]:
                selected==2?current->inventory.slots[1]:support;
            result.hit=edge && valid && actual==selected_entity;
        }else if(support_enabled && owner->pair_present && (support==owner->utility.entity || support==owner->ordinary.entity)){
            result.hit=raw->support_pressed && !input->support_down && input->active==3 && selected==3 &&
                valid && input->hand==support && actual==support;
            if(cycle && !input->cycle_down){
                input->hold_since=now;input->real=support;input->utility=owner->utility.entity;input->ordinary=owner->ordinary.entity;
                input->armed=input->active==3 && input->hand==support && valid &&
                    (actual==support || (selected==1 && actual==current->inventory.slots[0]) ||
                     (selected==2 && actual==current->inventory.slots[1]));
                input->fired=input->transitioned=0;
            }
            if(cycle && input->armed && !input->fired){
                if(!valid || support!=input->real || owner->utility.entity!=input->utility || owner->ordinary.entity!=input->ordinary)
                    input->armed=0;
                else{
                    if((selected==1 && actual==current->inventory.slots[0]) ||
                        (selected==2 && actual==current->inventory.slots[1]))input->transitioned=1;
                    if(now-input->hold_since>=250 && input->transitioned){
                        result.hit=actual==support || (selected==1 && actual==current->inventory.slots[0]) ||
                            (selected==2 && actual==current->inventory.slots[1]);
                        if(!result.hit)input->armed=0;
                    }
                }
            }
            if(result.hit){input->fired=1;input->armed=0;}
        }
    }
    if(!cycle){input->armed=input->fired=0;}
    input->support_down=raw->support_pressed;input->cycle_down=cycle;input->dedicated_down=raw->dedicated_down;
    input->active=selected;input->hand=actual;input->last_ms=now;return result;
}

typedef struct UssPlayerPendingSwap {
    unsigned present,dedicated;
    uint64_t epoch,context,pair,guard,revision,transaction_started;
    UssPairMember utility,ordinary;
} UssPlayerPendingSwap;

typedef struct UssPlayerControl {
    UssAutomaticController* automatic;UssSettingsState* settings;UssPlayerInput input;
    UssPlayerPendingSwap pending;
    void* user;int (*sample)(void*,const UssCoreObservation*,UssPlayerInputSample*);
    UssPairServiceResult (*select_single)(void*,const UssCoreObservation*);
    const char* reason;unsigned controller_reports;
} UssPlayerControl;
/* Software identity anchor only; starting a queued draw still requires the
 * ordinary fresh inventory/lifecycle/effect checks below. */
static inline int uss_player_pending_current(const UssPlayerControl* player,const UssCoreLifecycle* life)
{
    const UssPlayerPendingSwap* p=&player->pending;const UssPairOwner* o=&life->owner;
    const UssPairTransaction* work=&player->automatic->formation->work;
    return p->present && o->pair_present && o->ready && !o->disabled && !player->automatic->stopped &&
        p->epoch==o->epoch && p->context==o->context_generation && p->pair==o->pair_generation &&
        p->guard==o->guard_generation && p->revision==player->settings->revision &&
        pair_member_equal(&p->utility,&o->utility) && pair_member_equal(&p->ordinary,&o->ordinary) &&
        p->transaction_started==work->started_ms && work->order==USS_PAIR_DRAW &&
        (work->phase==USS_PAIR_ACTIVE || work->phase==USS_PAIR_COMPLETE) &&
        player->automatic->replacement.phase!=USS_REPLACE_ACTIVE;
}
static inline void uss_player_queue_swap(UssPlayerControl* player,const UssPairOwner* owner,unsigned dedicated)
{
    UssPlayerPendingSwap* p=&player->pending;
    if(p->present)return; /* Coalesce spam; never a backlog or parity toggle. */
    p->present=1;p->dedicated=dedicated;p->epoch=owner->epoch;p->context=owner->context_generation;
    p->pair=owner->pair_generation;p->guard=owner->guard_generation;p->revision=player->settings->revision;
    p->transaction_started=player->automatic->formation->work.started_ms;
    p->utility=owner->utility;p->ordinary=owner->ordinary;
}
static inline unsigned uss_player_control(void* user,UssCoreLifecycle* life,const UssCoreObservation* current,uint64_t now)
{
    UssPlayerControl* player=(UssPlayerControl*)user;UssAutomaticController* automatic;
    UssFormationRuntime* runtime;UssPlayerInputSample raw={0};UssPlayerGesture gesture;unsigned busy,status,queueable,previous_cycle,previous_transition;
    UssPairMember target;
    if(!player || !player->automatic || !player->settings || !player->sample)return USS_CONTROL_STOP;
    automatic=player->automatic;runtime=automatic->formation;if(!runtime || life!=&runtime->life)return USS_CONTROL_STOP;
    busy=runtime->work.phase==USS_PAIR_ACTIVE || automatic->replacement.phase==USS_REPLACE_ACTIVE;
    queueable=runtime->work.phase==USS_PAIR_ACTIVE && runtime->work.order==USS_PAIR_DRAW &&
        automatic->replacement.phase!=USS_REPLACE_ACTIVE;
    if(player->pending.present && (!uss_player_pending_current(player,life) || now<runtime->work.started_ms ||
        now>=runtime->work.deadline_ms || (runtime->work.phase==USS_PAIR_COMPLETE && current->inventory.active_selector!=3)))
        memset(&player->pending,0,sizeof(player->pending));
    if(!player->sample(player->user,current,&raw))raw.known=0;
    previous_cycle=player->input.cycle_down;previous_transition=player->input.transitioned;
    gesture=uss_player_input_poll(&player->input,player->settings,current,&life->owner,&raw,now,
        queueable?USS_INPUT_WORK_QUEUE_DRAW:busy);
    /* Capture semantic recognition milestones from this already-owned snapshot.
     * No input mutation or extra game read; onset/transition/hit are separate. */
    if(gesture.known && player->controller_reports<64 && automatic->api->log &&
        ((raw.cycle_held==UINT32_C(0x3f800000) && !previous_cycle) ||
         (!previous_transition && player->input.transitioned) || (gesture.hit && !gesture.dedicated && raw.cycle_held==UINT32_C(0x3f800000)))){
        ++player->controller_reports;
        automatic->api->log(automatic->mod,
            "USS input milestone tick_ms=%llu hold_ms=%llu cycle=%u armed=%u transitioned=%u hit=%u queued=%u selected=%u held=%u primary=%u phase=%u prior=%u timer=%u block=%u local_only=%u",
            (unsigned long long)now,(unsigned long long)(now>=player->input.hold_since?now-player->input.hold_since:0),
            player->input.cycle_down,player->input.armed,player->input.transitioned,gesture.hit,gesture.queue_requested,
            current->inventory.active_selector,current->inventory.held,current->inventory.slots[0],
            u32(current->primary.fields.row+16),u32(current->primary.fields.row+8),
            u32(current->primary.fields.row+24),u32(current->primary.fields.row+28),current->context.local_only);
    }
    if(gesture.known && gesture.queue_requested && queueable)
        uss_player_queue_swap(player,&life->owner,gesture.dedicated);
    if(!gesture.known || gesture.interrupted){
        memset(&player->pending,0,sizeof(player->pending));
        automatic->origin_valid=0;player->reason=gesture.interrupted?"PLAYER_INTERRUPTED_WORK":"INPUT_CONTEXT_UNAVAILABLE";
        if(busy){pair_owner_interrupt(&life->owner);return automatic_stop(automatic,player->reason);}
        return USS_CONTROL_WORK;
    }
    status=uss_automatic_control(automatic,life,current,now);
    if(status!=USS_CONTROL_IDLE){
        if(status==USS_CONTROL_STOP || runtime->work.phase==USS_PAIR_CANCELLED)memset(&player->pending,0,sizeof(player->pending));
        player->reason=automatic->reason;return status;
    }
    if(player->pending.present){
        if(!uss_player_pending_current(player,life))memset(&player->pending,0,sizeof(player->pending));
        else if(runtime->work.phase==USS_PAIR_COMPLETE && current->pair_matched &&
            lifecycle_current(life,runtime->thread,current) && current->inventory.active_selector==3 &&
            current->inventory.held==current->inventory.slots[2]){
            gesture.hit=1;gesture.dedicated=player->pending.dedicated;
            memset(&player->pending,0,sizeof(player->pending)); /* Consume before new work. */
        }
    }
    if(!gesture.hit){
        if(player->settings->current.mode==USS_BIND_DEDICATED && life->owner.pair_present && current->pair_matched &&
            current->inventory.slots[2]==life->owner.utility.entity &&
            (current->inventory.active_selector==1 || current->inventory.active_selector==2) &&
            current->inventory.held==current->inventory.slots[current->inventory.active_selector-1]){
            automatic->origin_valid=0;
            if(!uss_formation_store(runtime,&life->owner.ordinary,current->inventory.active_selector))
                return automatic_stop(automatic,"UTILITY_EXIT_START_REFUSED");
            player->reason="UTILITY_EXIT_STARTED";return USS_CONTROL_WORK;
        }
        player->reason=automatic->reason;return status;
    }
    if(!life->owner.pair_present){
        /* A dedicated key selects a lone utility; no attachment/publication or
         * pair is invented. Ordinary support remains vanilla. */
        if(!gesture.dedicated || current->support_class!=USS_UTILITY_SUPPORT || current->inventory.active_selector==3)
            return USS_CONTROL_IDLE;
        if(!player->select_single || player->select_single(player->user,current)!=USS_PAIR_SERVICE_DONE)
            return automatic_stop(automatic,"SINGLE_UTILITY_SELECTION_FAILED");
        player->reason="SINGLE_UTILITY_SELECTED";return USS_CONTROL_WORK;
    }
    if(!current->pair_matched || !lifecycle_current(life,runtime->thread,current))return USS_CONTROL_WORK;
    target=life->owner.utility;
    if(gesture.dedicated && current->inventory.slots[2]==target.entity){
        if(current->inventory.active_selector==3)return USS_CONTROL_IDLE;
        if(runtime->io.select(runtime->io.user,current,3)!=USS_PAIR_SERVICE_DONE)
            return automatic_stop(automatic,"UTILITY_SELECTION_FAILED");
        player->reason="UTILITY_SELECTED";return USS_CONTROL_WORK;
    }
    if(!gesture.dedicated && current->inventory.slots[2]==target.entity)target=life->owner.ordinary;
    automatic->origin_valid=0;
    if(!uss_formation_start(runtime,USS_PAIR_DRAW,&target))return automatic_stop(automatic,"PLAYER_DRAW_START_REFUSED");
    runtime->controller_draw=player->settings->current.mode==USS_BIND_DEFAULT && !gesture.dedicated &&
        raw.cycle_held==UINT32_C(0x3f800000) && player->input.transitioned;
    player->reason="PLAYER_DRAW_STARTED";return USS_CONTROL_WORK;
}
#endif
