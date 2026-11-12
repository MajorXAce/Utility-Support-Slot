#define USS_AUTOMATIC_TEST_MAIN automatic_regressions
#include "uss_automatic_controller_harness.c"
static int replacement_formation_effect(void* user,UssFormationEffect effect,uintptr_t first,uintptr_t second,
    uintptr_t record,uintptr_t inventory,uint32_t avatar,uint32_t value,const UssInventoryMessage* message)
{
    (void)user;(void)second;(void)record;(void)inventory;assert(avatar==22 || effect==USS_EFFECT_SEND);
    if(effect==USS_EFFECT_SELECT){assert(first==BASE+0xa93e90);return runtime_select(NULL,&fixture_runtime.latest,value)==USS_PAIR_SERVICE_DONE;}
    if(effect==USS_EFFECT_ATTACH){assert(first==BASE+0x9aa290 && value>=102 && value<=104);++attachments;
        if(attach_stop)return 0;put32(fixture(0x70000300,48)+8,value);
        put32(fixture(0x70000700,192)+(value-101)*48+4,22);return 1;}
    assert(effect==USS_EFFECT_SEND && first==BASE+0xbe3f40 && message &&
        (message->support==fixture_runtime.life.owner.utility.entity || message->support==fixture_runtime.life.owner.ordinary.entity));
    ++sends;return 1;
}
static void map_incoming(unsigned utility)
{
    const uint32_t handles[]={22,101,103,102,104},positions[]={3,2,0,1,4},wires[]={50,51,52,53,54};
    uintptr_t descriptors[]={0x20000000+0xf31ad8,0x70000900,0x70002900,0x70001900,0x70003900};
    unsigned i,k;unsigned char* data;extra_candidate();
    if(!utility){data=fixture(0x70003900,24);memset(data,0,8);data[0]=8;}
    for(i=0;i<5;++i){data=fixture(descriptors[i],24);put32(data+16,wires[i]);put32(data+20,1);
        if(i==4)memcpy(segment(MAPPING+0xf32f18+4*24,24),data,24);
        else memcpy(fixture(MAPPING+0xf32f18+(uintptr_t)positions[i]*24,24),data,24);}
    data=fixture(0xa3010000,64);memset(data,0xff,64);
    for(i=0;i<5;++i){k=handles[i]&7;while(u32(data+k*8)!=UINT32_MAX)k=(k+1)&7;put32(data+k*8,handles[i]);put32(data+k*8+4,positions[i]);}
    data=fixture(0xa3020000,64);memset(data,0xff,64);
    for(i=0;i<5;++i){k=wires[i]&7;while(u32(data+k*8)!=UINT32_MAX)k=(k+1)&7;put32(data+k*8,wires[i]);put32(data+k*8+4,positions[i]);}
    put32(fixture(0x92000018,20)+8,8);data=fixture(0x92000100,64);memset(data,0xff,64);
    for(i=0;i<4;++i){uint32_t entity=101+i,index=entity==101?2:entity==103?0:entity==102?1:3;
        k=entity&7;put32(data+k*8,entity);put32(data+k*8+4,index);if(i==3)word_segment(0x92001000+index*172,1);}
}
static void replacement_scene(unsigned utility,unsigned pickup,unsigned selected,unsigned role,UssAutomaticNative* native)
{
    unsigned i;UssPairMember target;
    automatic_fixture();initial_role=role;if(role==HF_SESSION_JOINER)fixture(SESSION+0xb3a8,8)[0]=2;
    map_incoming(utility);fixture_native.invoke=replacement_formation_effect;
    (void)update_auto();(void)update_auto();
    put32(fixture(0x70000300,48)+8,103);put32(fixture(0x70000700,192)+48+4,0);put32(fixture(0x70000700,192)+96+4,22);
    assert(update_auto()==USS_CONTROL_WORK);
    for(i=0;i<20 && fixture_runtime.work.phase==USS_PAIR_ACTIVE;++i){now_ms+=25;(void)update_auto();}
    assert(fixture_runtime.life.phase==USS_LIFE_FORMED);
    target=utility?fixture_runtime.life.owner.utility:fixture_runtime.life.owner.ordinary;
    assert(uss_formation_start(&fixture_runtime,USS_PAIR_DRAW,&target));
    for(i=0;i<20 && fixture_runtime.work.phase==USS_PAIR_ACTIVE;++i){now_ms+=25;uss_formation_tick(&fixture_runtime);}
    assert(fixture_runtime.work.phase==USS_PAIR_COMPLETE);
    if(selected==1)assert(runtime_select(NULL,&fixture_runtime.latest,1)==USS_PAIR_SERVICE_DONE);
    assert(uss_automatic_native_init(native,&controller,&fixture_native,pickup_option,NULL,refuse_replacement,NULL));
    pickup_yes=pickup;now_ms+=25;assert(update_auto()==USS_CONTROL_IDLE && controller.origin_valid);
    put32(fixture(0x70000300,48)+8,104);put32(fixture(0x70000700,192)+(utility?96:48)+4,0);
    assert(runtime_select(NULL,&fixture_runtime.latest,pickup?3:selected)==USS_PAIR_SERVICE_DONE);
}
int main(void)
{
    unsigned utility,pickup,selected,role,i,expected_real,expected_selected,before,draw,cases=0;uint64_t generation;UssAutomaticNative native;
    (void)automatic_regressions;
    for(role=HF_SESSION_HOST;role<=HF_SESSION_JOINER;++role)for(utility=0;utility<=1;++utility)
    for(pickup=0;pickup<=1;++pickup)for(selected=1;selected<=3;selected+=2){
        ++cases;replacement_scene(utility,pickup,selected,role,&native);generation=fixture_runtime.life.owner.pair_generation;
        now_ms+=25;assert(update_auto()==USS_CONTROL_WORK && !controller.stopped);
        assert(!strcmp(controller.reason,"ACTIVE_REPLACEMENT_FORMATION"));
        for(i=0;i<20 && fixture_runtime.work.phase==USS_PAIR_ACTIVE;++i){now_ms+=25;(void)update_auto();}
        if(fixture_runtime.life.phase!=USS_LIFE_FORMED)fprintf(stderr,"case role=%u utility=%u pickup=%u selected=%u reason=%s native=%s\n",role,utility,pickup,selected,controller.reason,fixture_native.reason);
        assert(fixture_runtime.life.phase==USS_LIFE_FORMED && fixture_runtime.work.phase==USS_PAIR_COMPLETE && !controller.stopped);
        expected_real=utility && !pickup && selected==1?102:104;
        expected_selected=utility?(pickup || selected==3?3:selected):(pickup?3:selected);
        assert(fixture_runtime.life.owner.pair_generation==generation+1 && fixture_runtime.work.real==expected_real);
        assert(fixture_runtime.latest.inventory.active_selector==expected_selected);
        assert(fixture_runtime.publication.formed && fixture_runtime.publication.record.pair_generation==generation+1);
        assert(fixture_runtime.publication.record.utility==(utility?104u:103u) && fixture_runtime.publication.record.ordinary==(utility?102u:104u));
        assert(u32(fixture(0x70000700,192)+(utility?96:48)+4)==0);
        before=fixture_native.effects;now_ms+=25;assert(update_auto()==USS_CONTROL_IDLE && fixture_native.effects==before);
        for(draw=0;draw<2;++draw){
            UssPairMember target=draw?fixture_runtime.life.owner.ordinary:fixture_runtime.life.owner.utility;
            assert(runtime_select(NULL,&fixture_runtime.latest,1)==USS_PAIR_SERVICE_DONE);
            assert(uss_formation_start(&fixture_runtime,USS_PAIR_DRAW,&target));
            for(i=0;i<20 && fixture_runtime.work.phase==USS_PAIR_ACTIVE;++i){now_ms+=25;(void)update_auto();}
            assert(!controller.stopped && fixture_runtime.work.phase==USS_PAIR_COMPLETE && fixture_runtime.work.real==target.entity);
            assert(fixture_runtime.latest.inventory.active_selector==3 && fixture_runtime.publication.formed &&
                fixture_runtime.publication.record.pair_generation==generation+1);
        }
    }
    assert(cases==16);
    replacement_scene(1,1,3,HF_SESSION_HOST,&native);now_ms+=25;assert(update_auto()==USS_CONTROL_WORK);
    attach_stop=1;
    for(i=0;i<20 && !controller.stopped;++i){now_ms+=25;(void)update_auto();}
    assert(controller.stopped);before=fixture_native.effects;now_ms+=25;
    assert(update_auto()==USS_CONTROL_STOP && fixture_native.effects==before);
    puts("USS automatic active replacement: host/joiner, both types/settings/origins -> full guarded OWN formation/publication, selection, new generation, later BOTH draws and failed effect no repeat PASS");return 0;
}
