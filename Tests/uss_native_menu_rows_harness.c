#include <assert.h>
#include <stdio.h>
#include "../src/uss_native_menu_rows.h"
static _Alignas(16) unsigned char memory[1400000];
static UssMenuRows state;
static unsigned calls,guard_calls,fail_guard,fail_write,resets,releases,initializes,finishes;
static uintptr_t first_row,panel;
static UssMenuHostSnapshot host;
static int read_memory(void* user,uintptr_t at,void* out,size_t length)
{
    (void)user;if(at<(uintptr_t)memory || at>(uintptr_t)memory+sizeof(memory) ||
        length>(uintptr_t)memory+sizeof(memory)-at)return 0;
    memcpy(out,(const void*)at,length);return 1;
}
static int write_memory(void* user,uintptr_t at,const void* data,size_t length)
{
    unsigned char copy[4];(void)user;assert(length==4);
    assert(at==panel+2832 || at==panel+2836 || at==panel+2840 || at==panel+2844 || at==panel+2872);
    ++calls;if(fail_write)return 0;
    assert(read_memory(NULL,at,copy,length));memcpy((void*)at,data,length);return 1;
}
static int guard(void* user,const UssMenuHostSnapshot* expected,unsigned built)
{
    unsigned i;(void)user;++guard_calls;
    if(guard_calls==fail_guard || !uss_menu_same_host(expected,&host))return 0;
    if(built)for(i=0;i<state.count;++i){uintptr_t row=first_row+(uintptr_t)i*USS_OPTION_ROW_BYTES;
        if(uss_menu_word((const unsigned char*)row,31428)!=139)return 0;}
    return 1;
}
static void reset(uint64_t row,const float* a,const float* b)
{assert(row==first_row+(uintptr_t)(31-resets)*USS_OPTION_ROW_BYTES);assert(!((uintptr_t)a&15) && a==b && a[0]==0);++resets;++calls;}
static void release(uint64_t row)
{assert(row==first_row+(uintptr_t)(32-releases-1)*USS_OPTION_ROW_BYTES);++releases;++calls;}
static void init_row(uint64_t row,UssMenuVec2 position,UssMenuVec2 a,UssMenuVec2 b,int kind,uint64_t settings,const void* descriptor,uint8_t enabled)
{
    const unsigned char* d=(const unsigned char*)descriptor;
    assert(row==first_row+(uintptr_t)initializes*USS_OPTION_ROW_BYTES && position.y==-64.0f*initializes);
    assert(a.x==0.5f && b.y==0.5f && kind==10 && !settings && enabled==1 && !((uintptr_t)descriptor&15));
    uss_menu_put_word((unsigned char*)row,31428,uss_menu_word(d,0));++initializes;++calls;
}
static void child(uint64_t parent,uint64_t row)
{assert(parent==panel+544 && row>=first_row);++calls;}
static void label(uint64_t widget,uint32_t value)
{assert(widget>=first_row && value==UINT32_C(0xc67c7faf));++calls;}
static void text(uint64_t widget,uint32_t key,const char* value)
{assert(widget>=first_row && key==UINT32_C(0xab2a7b35) && value && *value);++calls;}
static void choice(uint64_t selector,uint32_t index)
{uss_menu_put_word((unsigned char*)(uintptr_t)(selector-16016),29068,index);++calls;}
static void finish(uint64_t value){assert(value==panel);++finishes;++calls;}
static UssMenuRowsIo setup(void)
{
    UssMenuRowsIo io={0};unsigned char inert[216]={0};uint32_t selected=31;uintptr_t null_pointer=0;
    UssMenuSelectorSpec specs[2]={{"Binding mode",2,0,{"Default","Dedicated"}},{"Input page",2,1,{"Keyboard","Controller"}}};
    memset(memory,0,sizeof(memory));memset(&state,0,sizeof(state));memset(&host,0,sizeof(host));
    calls=guard_calls=fail_guard=fail_write=resets=releases=initializes=finishes=0;
    host.valid=host.escape_present=host.topmost=1;host.screen=0x10000;host.input=0x20000;host.epoch=1;
    host.content=(uintptr_t)memory;panel=(uintptr_t)memory+1150848;host.container=panel+544;
    first_row=(uintptr_t)memory+144000;
    memcpy((void*)(panel+2816),&first_row,8);memcpy((void*)(panel+2856),&null_pointer,8);memcpy((void*)(panel+2872),&selected,4);
    uss_menu_put_word(inert,0,139);assert(uss_menu_rows_prepare(&state,inert,sizeof(inert),specs,2));
    io.guard=guard;io.read=read_memory;io.write=write_memory;
    io.native.row_reset=reset;io.native.row_release=release;io.native.row_init=init_row;io.native.add_child=child;
    io.native.set_label=label;io.native.set_string_arg=text;io.native.set_choice=choice;io.native.finish_simple=finish;return io;
}
int main(void)
{
    UssMenuRowsIo io;UssMenuVec2 pivot={0.5f,0.5f};unsigned values[3]={77,88,99},i,total,before;
    UssMenuExpectedRows expected={0};
    io=setup();assert(uss_menu_rows_build(&state,&io,&host,pivot));
    assert(resets==32 && releases==32 && initializes==2 && finishes==1);
    assert(uss_menu_word(memory,1150848+2832)==2 && uss_menu_word(memory,1150848+2872)==0);
    uss_menu_rows_expected(&state,&expected);assert(expected.count==2 && expected.row_text[0]==(uintptr_t)state.rows[0].title);
    assert(!uss_menu_rows_build(&state,&io,&host,pivot));
    assert(uss_menu_rows_poll(&state,&io,values,3) && values[0]==0 && values[1]==1 && values[2]==99);
    uss_menu_put_word((unsigned char*)first_row,29068,1);
    assert(uss_menu_rows_poll(&state,&io,values,3) && values[0]==1);
    uss_menu_put_word((unsigned char*)first_row,29068,0);
    uss_menu_put_word((unsigned char*)(first_row+USS_OPTION_ROW_BYTES),29068,2);
    values[0]=77;assert(!uss_menu_rows_poll(&state,&io,values,3) && values[0]==77 && state.rows[0].selected==1 && state.stopped);
    before=calls;assert(!uss_menu_rows_poll(&state,&io,values,3) && calls==before);
    io=setup();assert(uss_menu_rows_build(&state,&io,&host,pivot));total=guard_calls;
    for(i=1;i<=total;++i){
        io=setup();fail_guard=i;assert(!uss_menu_rows_build(&state,&io,&host,pivot));before=calls;
        assert(!uss_menu_rows_build(&state,&io,&host,pivot) && calls==before);
    }
    io=setup();fail_write=1;assert(!uss_menu_rows_build(&state,&io,&host,pivot) && state.stopped);
    io=setup();host.category=1;assert(!uss_menu_rows_build(&state,&io,&host,pivot) && !calls);
    io=setup();io.native.set_choice=NULL;assert(!uss_menu_rows_build(&state,&io,&host,pivot) && !calls);
    puts("USS native selector rows: retained storage, native call order, exact fields, edits and every guard refusal PASS");return 0;
}
