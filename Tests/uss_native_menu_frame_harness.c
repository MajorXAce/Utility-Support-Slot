#include "../src/uss_native_menu_frame.h"
#define USS_MENU_LIFECYCLE_TEST_MAIN lifecycle_tests
#include "uss_native_menu_lifecycle_harness.c"
static int save_ok;
static unsigned samples,consumes,saves;static unsigned key;
static int frame_sample(void* user,uint64_t host,uint64_t epoch,uint64_t now,unsigned page,UssDedicatedCaptureSample* out)
{(void)user;++samples;memset(out,0,sizeof(*out));out->host=host;out->epoch=epoch;out->now_ms=now;out->page=page;out->valid=out->foreground=1;if(key)out->keys[key]=1;return 1;}
static int frame_consume(void* user){(void)user;++consumes;return 1;}
static int frame_save(void* user,const UssSettings* value){(void)user;++saves;assert(value->utility_key==75);return save_ok;}
static int session_focus(void* user){(void)user;return 1;}
static SHORT WINAPI session_key(int code){return code==75?(SHORT)-32768:0;}
static DWORD WINAPI session_pad(DWORD index,XINPUT_STATE* state){(void)index;(void)state;return ERROR_DEVICE_NOT_CONNECTED;}
int main(void){
    UssMenuFrame frame={0};unsigned char inert[216]={0};UssDedicatedWindows input={0};
    UssDedicatedCaptureSample physical;UssSettings before;HANDLE file;DWORD written;
    const wchar_t* path=L"build/uss-menu-session-fixture.json";
    assert(lifecycle_tests()==0);setup_lifecycle();uss_menu_put_word(inert,0,139);
    assert(uss_menu_controls_prepare(&own_rows,inert,sizeof(inert),&fixture_settings));
    assert(uss_menu_lifecycle_bind_native_cleanup(&lifecycle,&fixture_settings));
    assert(uss_menu_frame_bind(&frame,&lifecycle,path));
    DeleteFileW(path);assert(uss_menu_frame_load(&frame,&fixture_settings)==USS_SETTINGS_MISSING);
    assert(frame.controls.save(frame.controls.user,&fixture_settings.current));
    fixture_settings.current.utility_key=76;
    assert(uss_menu_frame_load(&frame,&fixture_settings)==USS_SETTINGS_LOADED && fixture_settings.current.utility_key==54);
    before=fixture_settings.current;
    file=CreateFileW(path,GENERIC_WRITE,0,NULL,TRUNCATE_EXISTING,0,NULL);assert(file!=INVALID_HANDLE_VALUE);
    assert(WriteFile(file,"{}",2,&written,NULL) && written==2 && CloseHandle(file));
    assert(uss_menu_frame_load(&frame,&fixture_settings)==USS_SETTINGS_INVALID && uss_settings_equal(&before,&fixture_settings.current));
    assert(DeleteFileW(path));
    input.foreground=session_focus;input.key_state=session_key;input.get_state=session_pad;
    assert(uss_menu_frame_bind_input(&frame,&input));
    assert(!frame.sample(frame.input_user,1,2,100,1,&physical));
    assert(frame.sample(frame.input_user,1,2,101,1,&physical) && physical.keys[75]);
    frame.sample=frame_sample;frame.controls.consume=frame_consume;frame.controls.save=frame_save;
    assert(uss_menu_lifecycle_open(&lifecycle));
    uss_menu_put_word(screen,1248+57452,3);
    assert(uss_menu_frame_step(&frame,0.016f,100,0)==USS_MENU_STEP_UPDATED && !samples);
    screen[3010456+144000+USS_OPTION_ROW_BYTES+0x3e85]=1;
    screen[3010456+144000+USS_OPTION_ROW_BYTES+0x7aca]=1;key=1;
    assert(uss_menu_frame_step(&frame,0.016f,101,0)==USS_MENU_STEP_UPDATED && samples==1);
    assert(!strcmp(own_rows.assignment.caption,"Release buttons..."));
    key=0;assert(uss_menu_frame_step(&frame,0.016f,102,0)==USS_MENU_STEP_UPDATED);
    assert(!strcmp(own_rows.assignment.caption,"Press a key or button..."));
    key=75;assert(uss_menu_frame_step(&frame,0.016f,103,0)==USS_MENU_STEP_UPDATED);
    assert(!strcmp(own_rows.assignment.caption,"Release to confirm..."));
    key=0;assert(uss_menu_frame_step(&frame,0.016f,104,1)==USS_MENU_STEP_UPDATED && !saves);
    assert(uss_menu_frame_step(&frame,0.016f,105,0)==USS_MENU_STEP_UPDATED && saves==1 && consumes==4 && samples==4 && frame.controls.save_failed && !frame.controls.stopped);
    assert(uss_menu_frame_step(&frame,0.016f,106,0)==USS_MENU_STEP_UPDATED && saves==1 && samples==4);
    assert(frame.controls.capture.phase==USS_DEDICATED_IDLE && !strcmp(own_rows.assignment.caption,"K"));
    {unsigned effects_before=effects;
     assert(uss_menu_frame_step(&frame,1.1f,107,0)==USS_MENU_STEP_REFUSED);
     assert(!strcmp(frame.checkpoint,"FRAME_TIMING") && effects==effects_before && saves==1);}
    {uintptr_t foreign_parent=0x44000000;unsigned effects_before_corruption=effects;
     memcpy(own_rows.assignment.widget+0xf0,&foreign_parent,8);
     assert(uss_menu_frame_step(&frame,0.016f,108,0)==USS_MENU_STEP_REFUSED && effects==effects_before_corruption);}
    puts("USS menu frame: native row command, capture-first frames, no idle sampling, transaction-delayed single save PASS");return 0;
}
