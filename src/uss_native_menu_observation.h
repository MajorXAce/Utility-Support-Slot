/* Bounded native Escape/Options reader. Layouts from CowboyBingus menus (0BSD).
 * No writes, calls, hooks or inferred ownership from a mod's mere installation. */
#ifndef USS_NATIVE_MENU_OBSERVATION_H
#define USS_NATIVE_MENU_OBSERVATION_H
#include "uss_native_menu_host.h"
#include "uss_native_menu_data.h"
enum {USS_MENU_OBSERVE_CALLS=256,USS_MENU_OBSERVE_BYTES=8192,USS_MENU_OWN_ROWS=3};
typedef struct UssMenuExpectedRows {
    uintptr_t tab_text,category_text,row_text[USS_MENU_OWN_ROWS];
    uintptr_t assignment_display[USS_MENU_OWN_ROWS];
    unsigned require_tab;
    unsigned building_tab,building_category; /* Only the active owning operation may nominate these transitions. */
    unsigned count;uint32_t choices[USS_MENU_OWN_ROWS],first_choice[USS_MENU_OWN_ROWS];
} UssMenuExpectedRows;
typedef struct UssMenuObservationIo {
    void* user;uintptr_t base;size_t module_size;
    uint64_t (*context)(void*); /* Current validated process/module/thread epoch. */
    int (*language_state)(void*); /* 0 stable, 1 switching; absent/unknown blocks entry. */
    int (*read)(void*,uintptr_t,void*,size_t); /* Exact-size readable copy; 1 success. SDK adapter must translate read()==0. */
} UssMenuObservationIo;
typedef struct UssMenuReadBudget {const UssMenuObservationIo* io;unsigned calls,bytes,failed;const char* checkpoint;unsigned index;uint32_t actual,expected;} UssMenuReadBudget;
static inline int uss_menu_pointer(uintptr_t value)
{return value>=0x10000 && value<UINT64_C(0x800000000000) && !(value&7);}
static inline int uss_menu_read(UssMenuReadBudget* b,uintptr_t at,void* out,size_t length)
{
    if(b->failed || !length || length>512 || at<0x10000 || at>UINTPTR_MAX-length ||
        b->calls>=USS_MENU_OBSERVE_CALLS || length>USS_MENU_OBSERVE_BYTES-b->bytes){b->failed=1;return 0;}
    ++b->calls;b->bytes+=(unsigned)length;
    if(!b->io->read(b->io->user,at,out,length)){b->failed=1;return 0;}return 1;
}
static inline int uss_menu_u32(UssMenuReadBudget* b,uintptr_t at,uint32_t* out)
{return uss_menu_read(b,at,out,4);}
static inline int uss_menu_ptr(UssMenuReadBudget* b,uintptr_t at,uintptr_t* out)
{return uss_menu_read(b,at,out,8) && uss_menu_pointer(*out);}
/* Ownership compares retained text-pointer identity, not merely translated words. */
static inline int uss_menu_text_identity(UssMenuReadBudget* b,uintptr_t widget,uintptr_t expected)
{
    uint32_t label;unsigned char count;unsigned i;
    if(!expected || !uss_menu_u32(b,widget+272,&label) || label!=UINT32_C(0xc67c7faf) ||
        !uss_menu_read(b,widget+616,&count,1) || count>14)return 0;
    for(i=0;i<count;++i){
        unsigned char record[24];uintptr_t pointer;
        if(!uss_menu_read(b,widget+280+24*i,record,sizeof(record)))return 0;
        if(uss_menu_word(record,0)!=UINT32_C(0xab2a7b35))continue;
        memcpy(&pointer,record+8,8);return uss_menu_word(record,4)==1 && pointer==expected;
    }
    return 0;
}
/* Keep the first structural mismatch from reads we already performed. This
 * is diagnostic only: no rereads after refusal and no weaker ownership proof. */
static inline void uss_menu_row_mismatch(UssMenuReadBudget* b,const char** reason,
    const char* stage,unsigned index,uint32_t actual,uint32_t expected)
{
    if(!*reason){*reason=stage;b->index=index;b->actual=actual;b->expected=expected;}
}
/* A native set_label -> set_string_arg pair can briefly have the nominated
 * template and NO arguments. A nonempty foreign argument is never ours. */
static inline int uss_menu_text_transition(UssMenuReadBudget* b,uintptr_t widget)
{
    uint32_t label;unsigned char count;
    return uss_menu_u32(b,widget+272,&label) && label==UINT32_C(0xc67c7faf) &&
        uss_menu_read(b,widget+616,&count,1) && count==0;
}
static inline int uss_menu_observe_host(const UssMenuObservationIo* io,const UssMenuExpectedRows* expected,
    UssMenuHostSnapshot* output,UssMenuReadBudget* statistics)
{
    UssMenuReadBudget b={0};UssMenuHostSnapshot result={0};uint64_t epoch;
    uintptr_t ui,menu,screen,input,again,rows;uint32_t stack[6],after_stack[6],tab_count,count;
    unsigned i,has_escape=0,own_tab=0,own_category=0;const char* row_issue=NULL;
    b.checkpoint="ARGUMENTS";if(statistics)*statistics=b;
    if(!io || !io->context || !io->read || !output || !uss_menu_pointer(io->base) ||
        io->module_size<0x347cf20 || io->base>UINTPTR_MAX-io->module_size ||
        (expected && expected->count>USS_MENU_OWN_ROWS))return 0;
    b.io=io;b.checkpoint="CONTEXT";epoch=io->context(io->user);if(!epoch)goto refused;
    b.checkpoint="ESCAPE_STACK";
    if(!uss_menu_ptr(&b,io->base+0x347ce28,&ui) || !uss_menu_read(&b,ui+0x429c,stack,sizeof(stack)) || stack[5]>5)goto refused;
    for(i=0;i<stack[5];++i)if(stack[i]==1)has_escape=1;
    if(!has_escape){result.valid=1;result.epoch=epoch;goto commit;} /* Closed, without chasing a stale screen. */
    b.checkpoint="SCREEN_ROOTS";
    if(!uss_menu_ptr(&b,io->base+0x347ce38,&menu) || !uss_menu_ptr(&b,menu+200,&screen) ||
        !uss_menu_ptr(&b,io->base+0x347cf18,&input) || screen>UINTPTR_MAX-5000000)goto refused;
    result.screen=screen;result.content=screen+3010456;result.input=input;result.epoch=epoch;
    result.container=result.content+1150848+544; /* Actual GAMEPLAY native parent, not a guessed GUI stack. */
    result.escape_present=1;result.topmost=stack[stack[5]-1]==1;
    result.language_changing=!io->language_state || io->language_state(io->user)!=0;
    b.checkpoint="SCREEN_FIELDS";
    if(!uss_menu_u32(&b,screen+8,&result.tab) || !uss_menu_u32(&b,result.content+1318488,&result.category) || result.category>=9 ||
        !uss_menu_u32(&b,screen+1248+57448,&tab_count) || tab_count<3 || tab_count>8 || result.tab>=tab_count ||
        !uss_menu_u32(&b,screen+1248+57452,&result.selected_tab) || result.selected_tab>=tab_count)goto refused;
    b.checkpoint="USS_TAB_IDENTITY";
    result.tab_count=tab_count;result.uss_tab=UINT32_MAX;
    result.vanilla_intact=1; /* Legacy field: structurally valid shared host, not vanilla labels. */
    if(expected && expected->tab_text){
        for(i=0;i<tab_count;++i){
            uintptr_t title=screen+1248+8296+(uintptr_t)3400*i;
            if(uss_menu_text_identity(&b,title,expected->tab_text)){
                if(own_tab){result.foreign_owner=1;break;}
                own_tab=1;result.uss_tab=i;
            }
            if(b.failed)goto refused;
        }
        /* During append, absence is allowed until our retained title is set.
         * Never report a provisional slot as positively owned USS identity. */
        if(expected->require_tab && !own_tab)result.foreign_owner=1;
    }
    own_category=own_tab; /* No ownership claim over other menus' category labels. */
    if(own_tab && own_category && expected->count){
        if(!uss_menu_ptr(&b,result.content+1150848+2816,&rows) || rows!=result.content+144000 ||
            !uss_menu_u32(&b,result.content+1150848+2832,&count))goto refused;
        result.ours_intact=count==expected->count && result.category==0;
        if(count!=expected->count)uss_menu_row_mismatch(&b,&row_issue,"ROWS_COUNT_CHANGED",0,count,expected->count);
        if(result.category!=0)uss_menu_row_mismatch(&b,&row_issue,"ROWS_CATEGORY_CHANGED",0,result.category,0);
        for(i=0;result.ours_intact && i<expected->count;++i){
            uintptr_t row=rows+(uintptr_t)i*USS_OPTION_ROW_BYTES;uint32_t setting,choices,first;
            if(!uss_menu_u32(&b,row+31428,&setting))goto refused;
            if(expected->assignment_display[i]){
                uint32_t kind;uintptr_t parent,child;uint64_t identity;unsigned depth,found=0;
                uintptr_t display=expected->assignment_display[i];
                if(!uss_menu_u32(&b,row+0x7ac0,&kind) ||
                    !uss_menu_ptr(&b,display+0xf0,&parent) || !uss_menu_read(&b,display+0x870,&identity,8))goto refused;
                if(!uss_menu_read(&b,row+0xe0,&child,8))goto refused;
                for(depth=0;child && depth<8;++depth){
                    if(!uss_menu_pointer(child))goto refused;
                    if(child==display){found=1;break;}
                    if(!uss_menu_read(&b,child+0xe8,&child,8))goto refused;
                }
                if(kind!=5)uss_menu_row_mismatch(&b,&row_issue,"ASSIGNMENT_KIND_CHANGED",i,kind,5);
                if(parent!=row)uss_menu_row_mismatch(&b,&row_issue,"ASSIGNMENT_PARENT_CHANGED",i,parent==row,1);
                if(identity!=0)uss_menu_row_mismatch(&b,&row_issue,"ASSIGNMENT_INPUT_CHANGED",i,identity==0,1);
                if(!found)uss_menu_row_mismatch(&b,&row_issue,"ASSIGNMENT_CHILD_MISSING",i,found,1);
                if(kind!=5 || parent!=row || identity!=0 || !found)result.ours_intact=0;
            }else{
                if(!uss_menu_u32(&b,row+29072,&choices) || !uss_menu_u32(&b,row+29080,&first))goto refused;
                if(choices!=expected->choices[i])uss_menu_row_mismatch(&b,&row_issue,"ROW_CHOICES_CHANGED",i,choices,expected->choices[i]);
                if(first!=expected->first_choice[i])uss_menu_row_mismatch(&b,&row_issue,"ROW_FIRST_CHOICE_CHANGED",i,first,expected->first_choice[i]);
                if(choices!=expected->choices[i] || first!=expected->first_choice[i])result.ours_intact=0;
            }
            if(setting!=139){uss_menu_row_mismatch(&b,&row_issue,"ROW_SETTING_CHANGED",i,setting,139);result.ours_intact=0;}
            else if(!uss_menu_text_identity(&b,row+3992,expected->row_text[i])){
                uss_menu_row_mismatch(&b,&row_issue,"ROW_TITLE_CHANGED",i,0,1);result.ours_intact=0;}
        }
        if(b.failed)goto refused;
    }
    /* Re-read roots/stack; evidence never crosses a changed screen/input epoch. */
    b.checkpoint="ROOTS_RECHECK";
    if(!uss_menu_ptr(&b,io->base+0x347ce28,&again) || again!=ui || !uss_menu_read(&b,ui+0x429c,after_stack,sizeof(after_stack)) ||
        memcmp(stack,after_stack,sizeof(stack)) || !uss_menu_ptr(&b,io->base+0x347ce38,&again) || again!=menu ||
        !uss_menu_ptr(&b,menu+200,&again) || again!=screen || !uss_menu_ptr(&b,io->base+0x347cf18,&again) || again!=input)goto refused;
    result.valid=1;
commit:
    b.checkpoint="CONTEXT_RECHECK";
    if(io->context(io->user)!=epoch)goto refused;
    b.checkpoint=row_issue?row_issue:"COMPLETE";if(statistics)*statistics=b;*output=result;return 1;
refused:
    if(statistics)*statistics=b;return 0; /* Caller output remains unchanged. */
}
#endif
