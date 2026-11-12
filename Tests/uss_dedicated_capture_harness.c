#include <assert.h>
#include <stdio.h>
#include "../src/uss_dedicated_capture.h"
static UssSettingsState settings;
static UssDedicatedCapture capture;
static UssDedicatedCaptureSample sample;
static void fixture(unsigned page)
{
    uss_settings_init(&settings,NULL);memset(&capture,0,sizeof(capture));memset(&sample,0,sizeof(sample));
    sample.host=100;sample.epoch=2;sample.now_ms=1000;sample.valid=sample.foreground=1;sample.page=page;
}
static void listening(unsigned page)
{fixture(page);assert(uss_dedicated_capture_begin(&capture,&settings,&sample));assert(!uss_dedicated_capture_step(&capture,&settings,&sample));}
int main(void)
{
    UssSettings previous;
    fixture(USS_DEDICATED_KEYBOARD);sample.keys[1]=1;
    assert(uss_dedicated_capture_begin(&capture,&settings,&sample));assert(!uss_dedicated_capture_step(&capture,&settings,&sample) && capture.phase==USS_DEDICATED_ARMING);
    sample.keys[1]=0;assert(!uss_dedicated_capture_step(&capture,&settings,&sample));
    settings.pending=settings.current;settings.pending.mode=USS_BIND_DEDICATED;settings.pending_present=1;previous=settings.current;
    sample.keys[0x4b]=1;assert(!uss_dedicated_capture_step(&capture,&settings,&sample) && capture.phase==USS_DEDICATED_RELEASING);
    assert(!uss_dedicated_capture_step(&capture,&settings,&sample) && !memcmp(&settings.current,&previous,sizeof(previous)));
    sample.keys[0x4b]=0;assert(uss_dedicated_capture_step(&capture,&settings,&sample)==USS_DEDICATED_QUEUED);
    assert(settings.pending.utility_key==0x4b && settings.pending.mode==USS_BIND_DEDICATED && settings.current.utility_key==0x36 && settings.release_required);
    assert(!uss_settings_commit(&settings,1));assert(uss_settings_commit(&settings,0) && settings.current.utility_key==0x4b);
    listening(USS_DEDICATED_CONTROLLER);sample.controller=USS_BIND_RIGHT_TRIGGER;
    assert(!uss_dedicated_capture_step(&capture,&settings,&sample));sample.controller=0;
    assert(uss_dedicated_capture_step(&capture,&settings,&sample)==USS_DEDICATED_QUEUED && settings.pending.utility_key==0x36 && settings.pending.utility_controller==USS_BIND_RIGHT_TRIGGER);
    listening(USS_DEDICATED_KEYBOARD);sample.keys[0x77]=1;assert(uss_dedicated_capture_step(&capture,&settings,&sample)==USS_DEDICATED_CANCELLED && !settings.pending_present);
    listening(USS_DEDICATED_KEYBOARD);sample.keys[27]=1;assert(uss_dedicated_capture_step(&capture,&settings,&sample)==USS_DEDICATED_CANCELLED);
    listening(USS_DEDICATED_KEYBOARD);sample.keys[65]=sample.keys[66]=1;assert(uss_dedicated_capture_step(&capture,&settings,&sample)==USS_DEDICATED_CANCELLED);
    listening(USS_DEDICATED_CONTROLLER);sample.controller=3;assert(uss_dedicated_capture_step(&capture,&settings,&sample)==USS_DEDICATED_CANCELLED);
    listening(USS_DEDICATED_KEYBOARD);sample.foreground=0;assert(uss_dedicated_capture_step(&capture,&settings,&sample)==USS_DEDICATED_CANCELLED);
    listening(USS_DEDICATED_KEYBOARD);sample.epoch=3;assert(uss_dedicated_capture_step(&capture,&settings,&sample)==USS_DEDICATED_CANCELLED);
    listening(USS_DEDICATED_KEYBOARD);sample.now_ms=999;assert(uss_dedicated_capture_step(&capture,&settings,&sample)==USS_DEDICATED_CANCELLED);
    listening(USS_DEDICATED_KEYBOARD);sample.now_ms=16001;assert(uss_dedicated_capture_step(&capture,&settings,&sample)==USS_DEDICATED_CANCELLED);
    listening(USS_DEDICATED_KEYBOARD);settings.external=1;settings.pending_present=1;
    assert(uss_dedicated_capture_step(&capture,&settings,&sample)==USS_DEDICATED_CANCELLED && settings.pending_present);
    listening(USS_DEDICATED_ANY);sample.controller=USS_BIND_RIGHT_TRIGGER;
    assert(!uss_dedicated_capture_step(&capture,&settings,&sample));sample.controller=0;
    assert(uss_dedicated_capture_step(&capture,&settings,&sample)==USS_DEDICATED_QUEUED);
    assert(settings.pending.utility_key==0 && settings.pending.utility_controller==USS_BIND_RIGHT_TRIGGER);
    assert(uss_settings_commit(&settings,0));
    {char encoded[512];UssSettings decoded;int length=uss_settings_encode(&settings.current,encoded,sizeof(encoded));
     assert(length && uss_settings_decode(encoded,(size_t)length,&decoded) && !decoded.utility_key && decoded.utility_controller==USS_BIND_RIGHT_TRIGGER);}
    sample.keys[75]=0;assert(uss_dedicated_capture_begin(&capture,&settings,&sample));
    assert(!uss_dedicated_capture_step(&capture,&settings,&sample));sample.keys[75]=1;
    assert(!uss_dedicated_capture_step(&capture,&settings,&sample));sample.keys[75]=0;
    assert(uss_dedicated_capture_step(&capture,&settings,&sample)==USS_DEDICATED_QUEUED);
    assert(settings.pending.utility_key==75 && !settings.pending.utility_controller);
    listening(USS_DEDICATED_ANY);sample.keys[75]=1;sample.controller=USS_BIND_RIGHT_TRIGGER;
    assert(uss_dedicated_capture_step(&capture,&settings,&sample)==USS_DEDICATED_CANCELLED && !settings.pending_present);
    puts("USS standalone capture: Windows/XInput namespace, activator/release gates, isolated queued settings, reserved/chord/focus/context/timeout/provider refusal PASS");return 0;
}
