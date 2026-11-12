#include <assert.h>
#include <stdio.h>
#include "../src/uss_native_assignment_display.h"
static UssAssignmentDisplay* active;
static unsigned guards,deny_guard,calls,copy_text,closed;
static int guard(void* user,uintptr_t parent)
{(void)user;assert(parent==0x10000);return !closed && ++guards!=deny_guard;}
static void init(uint64_t at,UssMenuVec2 pos,UssMenuVec2 pivot,UssMenuVec2 anchor,int layer)
{
    uint64_t identity=UINT64_MAX;(void)pos;(void)pivot;(void)anchor;assert(layer==12);
    assert(at==(uintptr_t)active->widget);memcpy(active->widget+USS_ASSIGNMENT_IDENTITY,&identity,8);++calls;
}
static uint8_t text(uint64_t at,const char* value)
{
    assert(at==(uintptr_t)active->widget+USS_ASSIGNMENT_TEXT);++calls;
    if(copy_text)memcpy(active->widget+USS_ASSIGNMENT_TEXT_BUFFER,value,strlen(value)+1);
    return 0; /* Unchanged/native zero status still requires postcondition proof. */
}
static void layout(uint64_t at){assert(at==(uintptr_t)active->widget);++calls;}
static void opacity(uint64_t at,float value)
{assert(at==(uintptr_t)active->widget+USS_ASSIGNMENT_TEXT || at==(uintptr_t)active->widget+0x268);assert(value==1.0f);++calls;}
static void attach(uint64_t parent,uint64_t child)
{assert(parent==0x10000 && child==(uintptr_t)active->widget);++calls;}
static UssAssignmentDisplayIo io(void)
{
    UssAssignmentDisplayIo value={0};value.guard=guard;
    value.native.assignment_display_init=init;value.native.assignment_text=text;
    value.native.assignment_layout=layout;value.native.set_opacity=opacity;value.native.add_child=attach;return value;
}
static void reset(UssAssignmentDisplay* display)
{memset(display,0,sizeof(*display));active=display;guards=deny_guard=calls=closed=0;copy_text=1;}
int main(void)
{
    UssAssignmentDisplay display,relocated;UssAssignmentDisplayIo access=io();unsigned before,i;
    char overlong[USS_ASSIGNMENT_CAPTION_BYTES+1];memset(overlong,'x',sizeof(overlong));overlong[sizeof(overlong)-1]=0;
    reset(&display);
    assert(!uss_assignment_display_build(&display,&access,0x10000,overlong) && calls==0 && !display.entered);
    assert(uss_assignment_display_build(&display,&access,0x10000,"Mouse 4"));
    assert(display.attached && !strcmp(display.caption,"Mouse 4"));
    before=calls;assert(!uss_assignment_display_build(&display,&access,0x10000,"6") && calls==before);
    assert(uss_assignment_display_caption(&display,&access,"Controller A"));
    assert(!strcmp(display.caption,"Controller A"));
    {uintptr_t parent=0x10000;memcpy(display.widget+0xf0,&parent,8);
     assert(!uss_assignment_display_reset_released(&display));}
    /* Native widgets retain addresses. A copied owner may not mutate them. */
    relocated=display;before=calls;
    assert(!uss_assignment_display_caption(&relocated,&access,"6") && calls==before);
    closed=1;assert(!uss_assignment_display_caption(&display,&access,"6") && calls==before);
    /* Stop after any uncertain effect; a second build never retries it. */
    for(i=2;i<=14;++i){
        reset(&display);deny_guard=i;
        assert(!uss_assignment_display_build(&display,&access,0x10000,"6"));
        before=calls;assert(!uss_assignment_display_build(&display,&access,0x10000,"6") && calls==before);
    }
    reset(&display);copy_text=0;
    assert(!uss_assignment_display_build(&display,&access,0x10000,"6") && display.stopped && !display.attached);
    assert(!display.caption[0]);
    puts("USS assignment display: caption postconditions, closure/relocation refusal and no uncertain-effect retry PASS");return 0;
}
