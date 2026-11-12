/* Internal implementation after UssHudRuntime is declared. Event-owned removal
 * observer and finite two-drop queue from our Lua HUD addon. No inventory writes. */
#ifndef USS_HUD_DROP_H
#define USS_HUD_DROP_H
typedef struct UssHudDropSnapshot {
    UssRecoverySnapshot recovery;unsigned char record[40],selection[24];uint32_t hold;
} UssHudDropSnapshot;
static int hud_drop_capture(UssHudRuntime* hud,UssHudDropSnapshot* output)
{
    UssHudDropSnapshot value={0};UssHudDrop* drop=&hud->drop;UssFormationRuntime* runtime=hud->native->runtime;
    UssRecoveryReader source={0};Reader reader={0};uintptr_t root=0,again=0;uint32_t support;
    if(runtime->life.owner.pair_present || !recovery_lifetime(&drop->work,&runtime->life.owner))return 0;
    source.api=hud->native->api;source.mod=hud->native->mod;source.policy=&runtime->policy;
    source.platform.user=hud->native->platform.user;source.platform.thread_id=hud->native->platform.thread_id;
    if(!uss_recovery_observe(&source,&drop->work,&value.recovery) || value.recovery.death)return 0;
    support=value.recovery.inventory.slots[2];
    if((support!=0 && support!=drop->work.survivor.entity) ||
        value.recovery.survivor.parent!=(support?drop->work.owner.context.local.avatar:0) ||
        (!value.recovery.outgoing_absent && value.recovery.outgoing.parent!=0))return 0;
    reader.api=hud->native->api;
    if(!get_ptr(&reader,hud->windows.base+0x346d538,&root) || root!=drop->root ||
        !get(&reader,root+0x394cd8,value.record,40) || !get(&reader,root+0x394fdc,value.selection,24) ||
        !get(&reader,root+0x394b44,&value.hold,4) || !get_ptr(&reader,hud->windows.base+0x346d538,&again) || root!=again)return 0;
    *output=value;return 1;
}
static int hud_drop_record(const UssHudDrop* drop,const UssHudDropSnapshot* snapshot,uint32_t pending)
{
    return uss_hud_word(snapshot->record,0)==10 && uss_hud_word(snapshot->record,20)==pending &&
        uss_hud_word(snapshot->record,16)==0 && uss_hud_word(snapshot->record,24)==0 &&
        uss_hud_word(snapshot->record,28)==drop->work.survivor.entity &&
        uss_hud_word(snapshot->record,32)==drop->work.survivor.entity && !memcmp(snapshot->record+4,drop->display,12);
}
/* Restricted OS heap write, distinct from ack's allowlist: only this exact
 * queued pair-removal delta or hold timer can be changed, each stage once. */
static int hud_drop_write(UssHudRuntime* hud,const UssHudDropSnapshot* expected,unsigned timer,uint32_t value,unsigned bit)
{
    UssHudDropSnapshot entry,after,wanted=*expected;MEMORY_BASIC_INFORMATION page;uintptr_t at;
    SIZE_T written=0;UssHudDrop* drop=&hud->drop;
    if(drop->disabled || (drop->consumed&bit) || !uss_hud_windows_scope(&hud->windows) ||
        !hud_drop_capture(hud,&entry) || memcmp(expected,&entry,sizeof(entry)))return 0;
    if(timer){
        if(value || bit!=2 || drop->stage!=1 || !hud_drop_record(drop,expected,UINT32_MAX) ||
            expected->hold<0x00800000u || expected->hold>0x40a00000u ||
            uss_hud_word(expected->selection,0)!=10 || expected->selection[20] ||
            uss_hud_word(expected->selection,4)!=UINT32_MAX || uss_hud_word(expected->selection,16)!=UINT32_MAX)return 0;
        wanted.hold=value;at=drop->root+0x394b44;
    }else{
        if(value!=UINT32_MAX || !drop->baseline || expected->recovery.inventory.slots[2]!=0 || expected->selection[20] ||
            !((bit==1 && drop->stage==1 && hud_drop_record(drop,expected,UINT32_MAX-1) && uss_hud_word(expected->selection,0)==10) ||
              (bit==4 && drop->stage==2 && hud_drop_record(drop,expected,0) && uss_hud_word(expected->selection,0)==UINT32_MAX)))return 0;
        memcpy(wanted.record+20,&value,4);at=drop->root+0x394cd8+20;
    }
    if(VirtualQuery((const void*)at,&page,sizeof(page))!=sizeof(page) || page.State!=MEM_COMMIT || page.Type!=MEM_PRIVATE ||
        (page.Protect!=PAGE_READWRITE && page.Protect!=PAGE_WRITECOPY) || at<(uintptr_t)page.BaseAddress ||
        (uintptr_t)page.BaseAddress>UINTPTR_MAX-page.RegionSize || at+4>(uintptr_t)page.BaseAddress+page.RegionSize ||
        !hud_drop_capture(hud,&entry) || memcmp(expected,&entry,sizeof(entry)))return 0;
    drop->consumed|=bit; /* Never retry, including returned write followed by unknown readback. */
    if(!WriteProcessMemory(GetCurrentProcess(),(void*)at,&value,4,&written) || written!=4 ||
        !hud_drop_capture(hud,&after) || memcmp(&wanted,&after,sizeof(after))){drop->disabled=1;drop->active=0;hud->reason="DROP_QUEUE_WRITE_UNCERTAIN";return 0;}
    return 1;
}
typedef struct UssHudDropRecheck {UssHudRuntime* hud;const UssHudDropSnapshot* expected;} UssHudDropRecheck;
static int hud_drop_unchanged(void* user)
{
    UssHudDropRecheck* check=user;UssHudDropSnapshot current;
    return check->hud->drop.active && hud_drop_capture(check->hud,&current) && !memcmp(check->expected,&current,sizeof(current));
}
static void hud_drop_present(UssHudRuntime* hud,const UssHudDropSnapshot* snapshot,unsigned second)
{
    UssHudDrop* drop=&hud->drop;unsigned* done=second?&drop->second_done:&drop->first_done;
    uint32_t target=second?drop->work.survivor.entity:drop->work.outgoing.entity;UssHudMetadataRead reader={0};
    const unsigned char* expected=second?drop->outgoing_display:drop->display;
    const unsigned char* desired=second?drop->display:drop->outgoing_display;unsigned char name[8],icon[4];UssHudDropRecheck check;
    if(*done || !hud->presentation.invoke || hud->presentation.disabled || snapshot->recovery.outgoing_absent ||
        snapshot->recovery.inventory.slots[2]!=(second?0:drop->work.survivor.entity) ||
        uss_hud_word(snapshot->record,20)!=UINT32_MAX || uss_hud_word(snapshot->selection,0)!=10 || snapshot->selection[20] ||
        uss_hud_word(snapshot->selection,4)!=UINT32_MAX || uss_hud_word(snapshot->selection,16)!=UINT32_MAX ||
        uss_hud_word(snapshot->selection,12)!=target || hud->serial==UINT64_MAX)return;
    reader.api=hud->native->api;
    if(!hud_metadata_read(&reader,drop->root+0x392828+0x13a8+0x150,name,8) ||
        !hud_metadata_read(&reader,drop->root+0x392828+0x1cc0+0x110,icon,4))return;
    if(!memcmp(name,desired+4,8) && !memcmp(icon,desired,4)){*done=1;return;}
    if(memcmp(name,expected+4,8) || memcmp(icon,expected,4))return;
    *done=1;check.hud=hud;check.expected=snapshot;
    (void)uss_hud_present(&hud->presentation,++hud->serial,drop->root,expected,desired,hud_drop_unchanged,&check);
}
static inline void hud_drop_arm(UssHudRuntime* hud,uintptr_t root,const unsigned char desired[12],const unsigned char outgoing[12],uint64_t now)
{
    UssHudDrop drop={0};
    if(hud->drop.disabled || now>UINT64_MAX-20000)return;
    drop.active=1;drop.root=root;drop.work=hud->recovery;memcpy(drop.display,desired,12);memcpy(drop.outgoing_display,outgoing,12);
    drop.started=now;drop.deadline=now+8000;drop.next=drop.next_hint=now+1;drop.last_support=drop.work.survivor.entity;hud->drop=drop;
}
static inline void hud_drop_poll(UssHudRuntime* hud,uint64_t now)
{
    UssHudDrop* drop=&hud->drop;UssHudDropSnapshot snapshot;Reader reader={0};uint32_t hinted,pending,active;unsigned changed,urgent;
    if(!drop->active)return;
    if(hud->ack.disabled || drop->disabled || now<drop->started || now>=drop->deadline || drop->checks>=250 || drop->fast_checks>=320){drop->active=0;return;}
    if(now<drop->next_hint)return;
    drop->next_hint=now+(drop->stage?100:25);++drop->fast_checks;reader.api=hud->native->api;
    if(!get(&reader,drop->work.owner.context.local.row_address+8,&hinted,4)){drop->active=0;return;}
    changed=hinted!=drop->last_support;drop->last_support=hinted;
    if(changed && !hinted && drop->baseline)drop->drop_since=now;
    urgent=!drop->stage && drop->drop_since && now-drop->drop_since<500;
    if(now<drop->next && !changed && !urgent)return;
    drop->next=now+100;++drop->checks;
    if(!hud_drop_capture(hud,&snapshot)){drop->active=0;return;}
    pending=uss_hud_word(snapshot.record,20);active=uss_hud_word(snapshot.selection,0);
    if(drop->stage){
        if(!hud_drop_record(drop,&snapshot,pending) || snapshot.recovery.inventory.slots[2]!=0 || (pending && pending!=UINT32_MAX)){drop->active=0;return;}
        if(drop->stage==1){
            if(!(drop->consumed&2) && pending==UINT32_MAX && active==10 && !snapshot.selection[20])
                (void)hud_drop_write(hud,&snapshot,1,0,2);
            if(drop->disabled)return;
            if(!pending && active==UINT32_MAX && !snapshot.selection[20]){
                drop->stage=2;
                if(!hud_drop_write(hud,&snapshot,0,UINT32_MAX,4))drop->active=0;
            }
        }else if(active==10 && pending==UINT32_MAX && !snapshot.selection[20]){
            hud_drop_present(hud,&snapshot,1);drop->active=0;
        }else if(!pending)drop->active=0; /* Never resubmit a second time. */
        return;
    }
    if(!drop->baseline && snapshot.recovery.inventory.slots[2]==drop->work.survivor.entity && active==10 && pending==UINT32_MAX &&
        uss_hud_word(snapshot.record,0)==10 && uss_hud_word(snapshot.record,16)==1 && uss_hud_word(snapshot.record,24)==1 &&
        uss_hud_word(snapshot.record,28)==drop->work.survivor.entity && uss_hud_word(snapshot.record,32)==drop->work.survivor.entity &&
        !memcmp(snapshot.record+4,drop->display,12) && !snapshot.selection[20])drop->baseline=1;
    if(drop->baseline && snapshot.recovery.inventory.slots[2]==drop->work.survivor.entity)hud_drop_present(hud,&snapshot,0);
    if((pending!=UINT32_MAX && pending!=UINT32_MAX-1) || (drop->baseline && active!=10)){drop->active=0;return;}
    if(now<=UINT64_MAX-20000 && drop->baseline && snapshot.recovery.inventory.slots[2]==0 && active==10 && !snapshot.selection[20] &&
        hud_drop_record(drop,&snapshot,UINT32_MAX-1)){
        drop->stage=1;
        if(hud_drop_write(hud,&snapshot,0,UINT32_MAX,1)){drop->deadline=now+20000;drop->checks=drop->fast_checks=0;drop->next=drop->next_hint=now+100;}
        else drop->active=0;
    }
}
#endif
