#include <assert.h>
#include <stdio.h>
#include "../src/uss_binding_capture.h"
static UssCaptureSample sample(void)
{
    UssCaptureSample s={0};s.host=100;s.epoch=1;s.now_ms=1000;s.valid=s.foreground=1;
    s.page=USS_CAPTURE_KEYBOARD_PAGE;return s;
}
static void listen(UssBindingCapture* c,UssCaptureSample* s)
{memset(c,0,sizeof(*c));assert(uss_binding_capture_begin(c,s,1));assert(uss_binding_capture_step(c,s)==USS_CAPTURE_NONE && c->phase==USS_CAPTURE_LISTENING);}
int main(void)
{
    UssBindingCapture c={0};UssCaptureSample s=sample();unsigned variant;
    assert(!uss_binding_capture_begin(&c,&s,0));s.activator_down=1;
    assert(uss_binding_capture_begin(&c,&s,1));s.pressed_present=1;s.pressed.device=1;s.pressed.button=6;
    assert(uss_binding_capture_step(&c,&s)==USS_CAPTURE_NONE && c.phase==USS_CAPTURE_ARMING);
    s.activator_down=0;assert(uss_binding_capture_step(&c,&s)==USS_CAPTURE_NONE && !c.choice.count);
    s.modifier_count=1;s.modifiers[0].device=1;s.modifiers[0].button=42;
    assert(uss_binding_capture_step(&c,&s)==USS_CAPTURE_NONE && c.phase==USS_CAPTURE_RELEASING && c.choice.count==2);
    s.chosen_inputs_down=1;assert(uss_binding_capture_step(&c,&s)==USS_CAPTURE_NONE);
    s.chosen_inputs_down=0;assert(uss_binding_capture_step(&c,&s)==USS_CAPTURE_CHOSEN && !c.guard_owned && c.release_required);
    for(variant=0;variant<7;++variant){
        s=sample();listen(&c,&s);
        if(variant==0)s.foreground=0;if(variant==1)s.host=101;if(variant==2)s.epoch=2;
        if(variant==3)s.page=2;if(variant==4)s.valid=0;if(variant==5)s.now_ms=16001;if(variant==6)s.back_pressed=1;
        assert(uss_binding_capture_step(&c,&s)==USS_CAPTURE_CANCELLED && !c.guard_owned && !c.choice.count);
    }
    s=sample();listen(&c,&s);s.clear_pressed=1;assert(uss_binding_capture_step(&c,&s)==USS_CAPTURE_NONE);
    s.clear_pressed=0;assert(uss_binding_capture_step(&c,&s)==USS_CAPTURE_CLEARED && !c.choice.count);
    s=sample();listen(&c,&s);s.pressed_present=s.pressed_modifier=s.pending_down=1;s.pressed.device=1;s.pressed.button=42;
    assert(uss_binding_capture_step(&c,&s)==USS_CAPTURE_NONE && c.pending_present);
    s.pressed_present=s.pending_down=0;assert(uss_binding_capture_step(&c,&s)==USS_CAPTURE_NONE && c.phase==USS_CAPTURE_RELEASING);
    assert(uss_binding_capture_step(&c,&s)==USS_CAPTURE_CHOSEN && c.choice.count==1 && c.choice.inputs[0].button==42);
    s=sample();s.page=2;listen(&c,&s);s.pressed_present=1;s.pressed.device=1;s.pressed.button=6;
    assert(uss_binding_capture_step(&c,&s)==USS_CAPTURE_NONE && !c.choice.count);
    s.pressed.device=3;s.pressed.button=7;assert(uss_binding_capture_step(&c,&s)==USS_CAPTURE_NONE);
    assert(uss_binding_capture_step(&c,&s)==USS_CAPTURE_CHOSEN && c.choice.inputs[0].device==3);
    s=sample();listen(&c,&s);s.pressed_present=1;s.pressed.device=2;s.pressed.button=0;s.modifier_count=1;s.modifiers[0].device=1;s.modifiers[0].button=42;
    assert(uss_binding_capture_step(&c,&s)==USS_CAPTURE_NONE && c.choice.count==2);
    puts("USS capture: opening release, keyboard/mouse/controller chords, modifier alone, cancel/clear/focus/epoch/deadline and final release PASS");return 0;
}
