/* Mode selector plus one native Utility Weapon assignment box. */
#ifndef USS_NATIVE_MENU_CONTROLS_H
#define USS_NATIVE_MENU_CONTROLS_H
#include "uss_native_menu_rows.h"
#include "uss_menu_settings_controller.h"
static inline int uss_menu_assignment_caption(const UssSettings* settings,char* out,size_t size)
{
    const char* name=NULL;int used;uint32_t key;
    if(!settings || !out || !size || !uss_settings_valid(settings))return 0;
    key=settings->utility_key;
    if(key){
        switch(key){case 1:name="Mouse left";break;case 2:name="Mouse right";break;
        case 4:name="Mouse middle";break;case 5:name="Mouse 4";break;case 6:name="Mouse 5";break;
        case 32:name="Space";break;case 9:name="Tab";break;case 13:name="Enter";break;
        case 16:name="Shift";break;case 17:name="Ctrl";break;case 18:name="Alt";break;}
        if(name)used=snprintf(out,size,"%s",name);
        else if((key>=48 && key<=57) || (key>=65 && key<=90))used=snprintf(out,size,"%c",(int)key);
        else if(key>=112 && key<=135)used=snprintf(out,size,"F%u",key-111);
        else used=snprintf(out,size,"Key 0x%02X",key);
    }else{
        switch(settings->utility_controller){
        case 1:name="D-pad up";break;case 2:name="D-pad down";break;case 4:name="D-pad left";break;
        case 8:name="D-pad right";break;case 16:name="Start";break;case 32:name="Back";break;
        case 64:name="Left stick";break;case 128:name="Right stick";break;
        case 256:name="Left shoulder";break;case 512:name="Right shoulder";break;
        case 4096:name="A";break;case 8192:name="B";break;case 16384:name="X";break;case 32768:name="Y";break;
        case USS_BIND_LEFT_TRIGGER:name="Left trigger";break;case USS_BIND_RIGHT_TRIGGER:name="Right trigger";break;
        }
        if(!name)return 0;used=snprintf(out,size,"Controller %s",name);
    }
    return used>0 && (size_t)used<size;
}
static inline int uss_menu_controls_prepare(UssMenuRows* rows,const unsigned char* inert,size_t length,
    const UssSettingsState* settings)
{
    char caption[USS_MENU_TEXT_CAP];
    return settings && uss_menu_assignment_caption(&settings->current,caption,sizeof(caption)) &&
        uss_menu_rows_prepare_assignment(rows,inert,length,settings->current.mode,caption);
}
static inline int uss_menu_controls_present(UssMenuRows* rows,const UssMenuRowsIo* io,
    const UssMenuSettingsController* controller,const UssSettingsState* settings)
{
    char caption[USS_MENU_TEXT_CAP];const char* prompt=NULL;
    if(!rows || !controller || !settings)return 0;
    switch(controller->capture.phase){
    case USS_DEDICATED_ARMING:prompt="Release buttons...";break;
    case USS_DEDICATED_LISTENING:prompt="Press a key or button...";break;
    case USS_DEDICATED_RELEASING:prompt="Release to confirm...";break;
    }
    if(prompt){if(!uss_menu_copy_text(caption,prompt))return 0;}
    else if(!uss_menu_assignment_caption(settings->pending_present?&settings->pending:&settings->current,caption,sizeof(caption)))return 0;
    return !strcmp(rows->assignment.caption,caption) || uss_menu_rows_assignment_caption(rows,io,caption);
}
static inline int uss_menu_controls_update(UssMenuRows* rows,const UssMenuRowsIo* io,
    UssMenuSettingsController* controller,UssSettingsState* settings,const unsigned values[3],
    const UssDedicatedCaptureSample* sample)
{
    (void)io;
    if(!rows || rows->count!=2 || rows->rows[1].count || !controller || controller->stopped || !settings ||
        settings->external || settings->provider || !values || values[0]>1 || values[1]>1)return 0;
    if(controller->capture.phase!=USS_DEDICATED_IDLE)
        return uss_menu_settings_capture(controller,settings,sample)!=USS_DEDICATED_CANCELLED;
    if((settings->pending_present?settings->pending.mode:settings->current.mode)!=values[0]+1 &&
        !uss_menu_settings_mode(controller,settings,values[0]+1))return 0;
    if(!values[1])return 1;
    if(!sample || sample->page!=USS_DEDICATED_ANY || !sample->valid || !sample->foreground)return 0;
    return uss_menu_settings_begin(controller,settings,sample);
}
#endif
