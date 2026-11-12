#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>
#include "../ResearchSource/inventory_probe.c"
#ifndef USS_FIXTURE_BYTES
#define USS_FIXTURE_BYTES 160
#endif
typedef struct Segment { uintptr_t at; size_t n; unsigned char data[USS_FIXTURE_BYTES]; } Segment;
#ifndef USS_FIXTURE_SEGMENTS
#define USS_FIXTURE_SEGMENTS 40
#endif
static Segment segments[USS_FIXTURE_SEGMENTS];
static unsigned used,reads,fail_at,change_at,logs,subscribes;
static HfApi fake;
static HfListener listener;
static int registration_ok;
static unsigned session_calls,change_session,initial_role;
static Reader last_reader;
static char last_log[1024];
static unsigned changing_kind,kind_reads;
static unsigned module_calls,module_change;
static char token;
#define MOD ((HfMod*)&token)
#define BASE ((uintptr_t)0x10000000)
static void put32(void* p,uint32_t v){memcpy(p,&v,4);}
static void putptr(void* p,uintptr_t v){memcpy(p,&v,8);}
static unsigned char* segment(uintptr_t at,size_t n)
{
    Segment* s=&segments[used++]; assert(used<=USS_FIXTURE_SEGMENTS && n<=USS_FIXTURE_BYTES);
    s->at=at;s->n=n;memset(s->data,0,USS_FIXTURE_BYTES);return s->data;
}
static int fake_read(uintptr_t a,void* out,size_t n)
{
    unsigned i;++reads;
    if(reads==fail_at){memset(out,0xAB,n/2);return 1;}
    for(i=0;i<used;++i) if(a>=segments[i].at && a-segments[i].at<=segments[i].n && n<=segments[i].n-(a-segments[i].at)) {
        memcpy(out,segments[i].data+(a-segments[i].at),n);
        if(changing_kind && a==0x70000700 && n==20 && ++kind_reads>=3) put32((unsigned char*)out+12,99);
        if(reads==change_at) memset(out,0,n);
        return 0;
    }
    return 1;
}
static uintptr_t module(const char* name,size_t* size){assert(!strcmp(name,"game.dll"));++module_calls;*size=0x4000000+(module_calls>1 && (module_change&2)?4096:0);return BASE+(module_calls>1 && (module_change&1)?4096:0);}
static uint32_t game_state(HfMod* m){assert(m==MOD);return HF_STATE_MISSION;}
static const char* build_id(void){return "25480438";}
static void log_line(HfMod* m,const char* fmt,...){va_list args;assert(m==MOD);va_start(args,fmt);vsnprintf(last_log,sizeof(last_log),fmt,args);va_end(args);++logs;}
static void session(HfMod* m,HfSession* out){assert(m==MOD);memset(out,0,sizeof(*out));out->role=initial_role;++session_calls;if(change_session && session_calls>1){if(change_session==1)out->version=1;else out->role=HF_SESSION_HOST;}}
static int subscribe(HfMod* m,uint32_t kind,HfListener callback,void* user,int32_t priority)
{assert(m==MOD && kind==HF_ON_PLAYER_SPAWN && !user && priority==0);listener=callback;++subscribes;return registration_ok;}
static void table(uintptr_t at,uintptr_t data,uint32_t key)
{
    unsigned char* h=segment(at,20);putptr(h,data);put32(h+8,1);put32(h+12,UINT32_MAX);put32(h+16,1);
    h=segment(data,8);put32(h,key);put32(h+4,0);
}
static void setup(void)
{
    static const uint32_t rvas[]={0x518C2A,0x3A4BDE,0x3A9E00,0x4AABCA,0xC30B8};
    static const uint32_t targets[]={0x3326B90,0x3326468,0x3326738,0x3326420,0x3326DC0};
    static const char* patterns[]={"488b1d????????488bf9448b4308443b4328720c488bcbe8ca490d00448b4308","488b05????????488d542448488b88e8000000448b4108e8361a2600488b0d2f","4c8b05????????33db8bcb458b4830458b5038440fafd2418d71ff4585c9744c","4c8b15????????7517b8ffffffff8bc84869c1d0010000490342604883c408c3","488b3d????????4c89742438448b4f28448b5f30440fafd8458d71ff4585c90f"};
    unsigned i,j;unsigned char *b,*h;uintptr_t roots[]={0x20000000,0x30000000,0x40000000,0x50000000,0x60000000};
    used=reads=fail_at=change_at=logs=subscribes=session_calls=change_session=0;
    module_calls=module_change=initial_role=0;listener=NULL;registration_ok=1;
    changing_kind=kind_reads=0;
    atomic_store(&attempt,0);memset(&fake,0,sizeof(fake));
    fake.version=HF_MOD_SDK_VERSION;fake.size=(uint32_t)sizeof(fake);fake.read=fake_read;fake.module_base=module;
    fake.game_state=game_state;fake.log=log_line;fake.build_id=build_id;fake.session=session;fake.subscribe=subscribe;
    for(i=0;i<5;++i){size_t n=strlen(patterns[i])/2;b=segment(BASE+rvas[i],n);
        for(j=0;j<n;++j)if(patterns[i][2*j]!='?'){unsigned x=16*hex_digit(patterns[i][j*2])+hex_digit(patterns[i][j*2+1]);b[j]=(unsigned char)x;}
        put32(b+3,targets[i]-rvas[i]-7);putptr(segment(BASE+targets[i],8),roots[i]);}
    put32(segment(roots[1]+0x3A8,4),11);
    table(roots[0]+0xF21A88,0x70000000,11);
    b=segment(roots[0]+0xF31AD8,24);put32(b,123);put32(b+8,22);put32(b+12,33);put32(b+16,5);put32(b+20,1);
    table(roots[2]+40,0x70000100,22);
    b=segment(roots[2]+64,24);putptr(b,0x70000200);putptr(b+16,0x70000300);
    putptr(segment(0x70000200,8),roots[0]+0xF31AD8);
    b=segment(0x70000300,48);put32(b,101);put32(b+8,UINT32_MAX);put32(b+28,0);
    table(roots[3]+0x30,0x70000400,22);putptr(segment(roots[3]+0x60,8),0x70000500);put32(segment(0x70000500,4),101);
    h=segment(roots[4],72);put32(h,1);put32(h+12,1);putptr(h+56,0x70000600);putptr(h+64,0x70000700);
    /* Table header is a subrange of this same 72-byte manager. */
    putptr(h+32,0x70000800);put32(h+40,1);put32(h+44,UINT32_MAX);put32(h+48,1);
    b=segment(0x70000800,8);put32(b,101);put32(b+4,0);
    putptr(segment(0x70000600,8),0x70000900);b=segment(0x70000900,24);put32(b,987);put32(b+8,101);
    put32(segment(0x70000700,20)+12,2);
}
static int sample(Inventory* out){Reader r={0};int ok;r.api=&fake;ok=inventory_sample(&r,out);last_reader=r;assert(r.calls<=2165 && r.bytes<=20000);return ok;}
static unsigned char* fixture(uintptr_t at,size_t length)
{
    unsigned i;for(i=0;i<used;++i)if(segments[i].at==at){segments[i].n=length;return segments[i].data;}
    assert(0);return NULL;
}
static void distinct_slots(void)
{
    unsigned char* b;unsigned k;
    setup();b=fixture(0x70000300,48);put32(b,101);put32(b+4,102);put32(b+8,103);put32(b+12,0);
    b=fixture(0x60000000,72);put32(b,3);put32(b+12,3);put32(b+40,4);
    b=fixture(0x70000800,32);memset(b,0xFF,32);
    for(k=0;k<3;++k){put32(b+8*(k+1),101+k);put32(b+8*(k+1)+4,k);}
    b=fixture(0x70000600,24);for(k=0;k<3;++k)putptr(b+8*k,0x70000900+(uintptr_t)k*0x1000);
    b=fixture(0x70000700,144);memset(b,0,144);put32(b+12,1);put32(b+48+12,7);put32(b+96+12,UINT32_MAX);
    b=segment(0x70001900,24);put32(b,998);put32(b+8,102);
    b=segment(0x70002900,24);put32(b,999);put32(b+8,103);
}
int main(void)
{
    Inventory out;unsigned i,total;HfPlayerEvent e={{sizeof(e),HF_ON_PLAYER_SPAWN,1},99,22,1};
    /* Each final guard is tested independently and together using actual logic. */
    {
        Inventory first={0},second={0}; Reader rr={0}; unsigned bit;
        first.avatar=second.avatar=22; first.local_unit=second.local_unit=11;
        assert(final_context(&rr,&first,&second,BASE,BASE,4096,4096));
        assert(rr.final_valid && !rr.final_reasons && !rr.difference_mask);
        assert(rr.first_unit==11 && rr.first_avatar==22);
        assert(!final_context(&rr,&first,&second,BASE,BASE+1,4096,4096)); assert(rr.final_reasons==4);
        assert(!final_context(&rr,&first,&second,BASE,BASE,4096,8192)); assert(rr.final_reasons==8);
        for(bit=0;bit<10;++bit) {
            second=first;
            switch(bit) {
            case 0: second.roots[0]=1; break; case 1: second.avatar_header[0]=1; break;
            case 2: second.local_unit=12; break; case 3: second.row[0]=1; break;
            case 4: second.slots[0]=1; break; case 5: second.held=1; break;
            case 6: second.identities[0][0]=1; break; case 7: second.observed_kinds[0]=1; break;
            case 8: second.avatar=23; break; case 9: second.row_address=1; break;
            }
            assert(!final_context(&rr,&first,&second,BASE,BASE,4096,4096));
            assert(rr.final_reasons==2 && rr.difference_mask==(1u<<bit));
        }
        assert(!final_context(&rr,&first,&second,BASE,BASE+1,4096,8192)); assert(rr.final_reasons==14);
        assert(!rr.predicate_valid && !rr.expected_entity && !rr.observed_entity && !rr.observed_kind);
    }
    setup();module_change=1;assert(!sample(&out));assert(module_calls==2 && last_reader.final_reasons==4);
    setup();module_change=2;assert(!sample(&out));assert(module_calls==2 && last_reader.final_reasons==8);
    setup(); {Reader rr={0};rr.api=&fake;assert(inventory_sample(&rr,&out));
        assert(module_calls==2 && rr.final_reasons==0 && rr.difference_mask==0);
        assert(rr.descriptor_u12[0]==33 && rr.descriptor_u12[1]==33 &&
               rr.descriptor_u16[0]==5 && rr.descriptor_u16[1]==5 &&
               rr.descriptor_u20[0]==1 && rr.descriptor_u20[1]==1);}
    if(HF_USS_INVENTORY_ENABLED) {
        HfPlayerEvent metadata=e;
        /* Differing and unknown event IDs do not select the local inventory. */
        for(i=0;i<2;++i) {
            metadata.entity=i?0:5;
            setup();assert(hellforge_mod_init(&fake,MOD)==0);listener(MOD,&metadata.info,NULL);
            assert(strstr(last_log,"HF019 OBSERVED") && strstr(last_log,"avatar=22 slots=101,"));
            assert(strstr(last_log,"scope=LOCAL_INVENTORY event_binding=UNVERIFIED"));
            assert(strstr(last_log,i?"event_entity=0 ":"event_entity=5 "));
            total=reads;listener(MOD,&metadata.info,NULL);assert(reads==total && logs==1);
        }
        /* Rejected callbacks must not consume the local one-shot attempt. */
        setup();assert(hellforge_mod_init(&fake,MOD)==0);
        listener(MOD,NULL,NULL);
        metadata=e;metadata.info.kind=HF_ON_PLAYER_DOWN;listener(MOD,&metadata.info,NULL);
        metadata=e;metadata.info.size=sizeof(HfEventInfo);listener(MOD,&metadata.info,NULL);
        metadata=e;metadata.local=0;listener(MOD,&metadata.info,NULL);
        assert(!reads && !logs && atomic_load(&attempt)==1);
        listener(MOD,&e.info,NULL);assert(reads && logs==1 && atomic_load(&attempt)==0);
    }
    setup();assert(sample(&out));assert(out.avatar==22 && out.slots[0]==101 && out.held==101);total=reads;
    /* First graph reaches its occupied held predicate; fail the next graph's
     * first root read. Prior predicate evidence must not survive that boundary. */
    setup();fail_at=5+(total-5)/2+1;assert(!sample(&out));
    assert(last_reader.pass==2 && !strcmp(last_reader.stage,"MANAGER_ROOTS") && last_reader.slot==-1);
    assert(last_reader.predicate_valid==0 && last_reader.expected_entity==0 &&
           last_reader.observed_entity==0 && last_reader.observed_kind==0);
    /* Every exact request can fail AFTER partly modifying its scratch buffer. */
    for(i=1;i<=total;++i){setup();fail_at=i;memset(&out,0xCC,sizeof(out));assert(!sample(&out));assert(((unsigned char*)&out)[0]==0xCC);}
    setup();putptr(segments[1].data,0);assert(!sample(&out));
    setup();put32(segments[11].data+8,3);assert(!sample(&out)); /* owner hash capacity */
    setup();put32(segments[11].data+8,2097152);assert(!sample(&out));
    setup();for(i=0;i<used;++i)if(segments[i].at==0x70000300){unsigned k;for(k=0;k<4;++k)put32(segments[i].data+4*k,101);}assert(sample(&out));
    setup();put32(segment(0x70000A00,24)+8,23);for(i=0;i<used;++i)if(segments[i].at==0x70000200)putptr(segments[i].data,0x70000A00);assert(!sample(&out));
    setup();for(i=0;i<used;++i)if(segments[i].at==0x60000000)put32(segments[i].data+12,16385);assert(!sample(&out));
    setup();for(i=0;i<used;++i)if(segments[i].at==0x20000000+0xF31AD8)put32(segments[i].data+8,0);assert(!sample(&out));
    setup();for(i=0;i<used;++i)if(segments[i].at==0x70000900)put32(segments[i].data+8,102);assert(!sample(&out));
    assert(!strcmp(last_reader.stage,"WEAPON_PREDICATE") && last_reader.slot==0 && last_reader.predicate_valid && last_reader.observed_entity==102 && last_reader.calls==30 && last_reader.bytes==580);
    /* The identical footprint also occurs with secondary/support first occupied. */
    for(i=0;i<3;++i) {
        unsigned j;setup();
        for(j=0;j<used;++j) {
            if(segments[j].at==0x70000300){memset(segments[j].data,0,16);put32(segments[j].data+4*i,101);}
            if(segments[j].at==0x70000700)put32(segments[j].data+12,7);
        }
        assert(sample(&out));assert(out.observed_kinds[i]==7);
    }
    setup();put32(segments[11].data+8,2);
    segments[12].n=16;put32(segments[12].data+8,999);put32(segments[12].data+12,0);
    for(i=0;i<used;++i)if(segments[i].at==0x70000700)put32(segments[i].data+12,7);
    assert(sample(&out));assert(out.observed_kinds[0]==7 && last_reader.calls==total+2);
    setup();changing_kind=1;assert(!sample(&out));assert(!strcmp(last_reader.failure,"CONTEXT_CHANGED"));
    distinct_slots();assert(sample(&out));assert(out.slots[0]==101 && out.slots[1]==102 && out.slots[2]==103 && out.slots[3]==0);
    assert(out.observed_kinds[0]==1 && out.observed_kinds[1]==7 && out.observed_kinds[2]==UINT32_MAX && out.observed_kinds[3]==0 && out.observed_kinds[4]==1);
    if(HF_USS_INVENTORY_ENABLED){distinct_slots();assert(hellforge_mod_init(&fake,MOD)==0);listener(MOD,&e.info,NULL);assert(strstr(last_log,"OBSERVED category=UNCLASSIFIED gameplay_approved=0") && strstr(last_log,"present_mask=23"));}
    setup();change_at=total;assert(!sample(&out));
    setup();assert(hellforge_mod_init(&fake,MOD)==0);
    if(HF_USS_INVENTORY_ENABLED){assert(listener && subscribes==1);listener(MOD,&e.info,NULL);assert(logs==1);assert(strstr(last_log,"event_entity=22 local=1 state=") && strstr(last_log,"update=1"));total=reads;listener(MOD,&e.info,NULL);assert(reads==total && logs==1);
        setup();registration_ok=0;assert(hellforge_mod_init(&fake,MOD)==0);assert(listener);listener(MOD,&e.info,NULL);assert(reads==0 && logs==1);
        for(i=1;i<=2;++i){setup();change_session=i;assert(hellforge_mod_init(&fake,MOD)==0);listener(MOD,&e.info,NULL);assert(logs==1 && reads>0 && atomic_load(&attempt)==0 && strstr(last_log,"REFUSED SESSION_CHANGED"));}
        setup();initial_role=HF_SESSION_HOST;assert(hellforge_mod_init(&fake,MOD)==0);listener(MOD,&e.info,NULL);
        assert(!reads && logs==1 && atomic_load(&attempt)==0 && strstr(last_log,"REFUSED session role"));
    }else assert(!listener && logs==1 && reads==0);
    setup();fake.size=(uint32_t)USS_REQUIRED_API_BYTES-1;assert(hellforge_mod_init(&fake,MOD)!=0 && !listener);
    setup();fake.version=USS_MIN_RUNTIME_SDK-1;assert(hellforge_mod_init(&fake,MOD)!=0 && !listener);
    puts("inventory actual reader: valid, every partial failure, missing root, bad capacity, stale identity, changed context, one-shot/lifetime PASS");
    return 0;
}


