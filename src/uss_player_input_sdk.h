/* Concrete read-only SDK input/UI/pickup preference producer for build25480438.
 * Windows focus and dedicated key/button sampling are supplied separately. */
#ifndef USS_PLAYER_INPUT_SDK_H
#define USS_PLAYER_INPUT_SDK_H
#include "uss_player_control.h"
#include "formation_native.h"

typedef struct UssPlayerInputSdk {
    const HfApi* api;HfMod* mod;UssCorePlatform platform;
    UssSettingsState* settings;UssFormationRuntime* runtime;UssFormationNative* native;
    void* input_user;int (*foreground)(void*);
    int (*dedicated)(void*,const UssSettings*,unsigned*);
    uintptr_t base;size_t size;unsigned thread,pickup_pins_checked,pickup_pins_valid,binding_capture;
    const char* reason;
} UssPlayerInputSdk;
static inline UssPairServiceResult uss_player_sdk_select_single(void* user,const UssCoreObservation* current)
{
    UssPlayerInputSdk* source=user;
    return source && source->native?formation_native_select_single(source->native,current):USS_PAIR_SERVICE_STOP;
}
static inline int player_sdk_scope(UssPlayerInputSdk* source)
{
    uintptr_t base;size_t size=0;const char* build;unsigned thread;
    if(!source || !uss_sdk_compatible(source->api) || !source->mod || !source->api->read ||
        !source->api->module_base || !source->api->build_id || !source->api->game_state ||
        !source->platform.thread_id || !source->settings || !source->runtime)return 0;
    thread=source->platform.thread_id(source->platform.user);
    if(!thread || thread!=source->runtime->thread || (source->thread && source->thread!=thread))return 0;
    build=source->api->build_id();if(!build || strcmp(build,"25480438"))return 0;
    base=source->api->module_base("game.dll",&size);
    if(!base || size<0x347ce30 || size>0x20000000 || !address_ok(base,size))return 0;
    if(source->base!=base || source->size!=size){source->pickup_pins_checked=source->pickup_pins_valid=0;}
    source->base=base;source->size=size;source->thread=thread;return 1;
}
/* Chat and escape-stack fields are the same read-only layout used by Lua's
 * dedicated input. Two equal samples bind a result to the current UI instance. */
static inline int player_sdk_ui(UssPlayerInputSdk* source,unsigned* clear)
{
    Reader memory={0};uintptr_t ui=0,again_ui=0;unsigned char chat[8],again_chat[8],stack[24],again_stack[24];
    uint32_t depth;memory.api=source->api;memory.slot=-1;
    if(!get_ptr(&memory,source->base+0x347ce28,&ui) || !get(&memory,ui+0x10,chat,8) ||
        !get(&memory,ui+0x429c,stack,24) || !get(&memory,ui+0x10,again_chat,8) || memcmp(chat,again_chat,8) ||
        !get(&memory,ui+0x429c,again_stack,24) || memcmp(stack,again_stack,24) ||
        !get_ptr(&memory,source->base+0x347ce28,&again_ui) || ui!=again_ui)return 0;
    depth=u32(stack+20);
    *clear=ptr_value(chat)==0 && !(depth>=1 && depth<=5 && u32(stack)==1);return 1;
}
/* Real readiness is produced here, never copied from a diagnostic setup flag.
 * Down/empty-inventory handling remains a post-Core recovery concern; ordinary
 * action services independently require both standard weapons and valid held
 * context. Outside a mission, allow Core to observe/reset the ended session. */
static inline int uss_player_sdk_policy(void* user,UssCoreFlags* flags)
{
    UssPlayerInputSdk* source=(UssPlayerInputSdk*)user;unsigned clear=0;uint32_t state;
    if(flags)memset(flags,0,sizeof(*flags));
    if(!flags || !player_sdk_scope(source))return 0;
    state=source->api->game_state(source->mod);
    if(state<HF_STATE_SPLASH || state>HF_STATE_PREPARE_MISSION)return 0;
    flags->disabled=source->runtime->life.owner.disabled;
    flags->paused=source->runtime->life.phase==USS_LIFE_PAUSED;
    if(state!=HF_STATE_MISSION){
        /* Let the read-only lifecycle path verify a mission exit even when the
         * previous actor's gameplay was paused. No native work runs outside. */
        flags->known=flags->eligible=1;flags->paused=0;return 1;
    }
    if(!source->foreground || !source->foreground(source->input_user) || source->binding_capture ||
        !player_sdk_ui(source,&clear) || !clear){
        source->settings->release_required=1;source->reason="PLAYER_UI_OR_FOCUS_BLOCKED";return 0;
    }
    flags->known=flags->eligible=1;source->reason="PLAYER_POLICY_CURRENT";return 1;
}
static inline int uss_player_sdk_sample(void* user,const UssCoreObservation* current,UssPlayerInputSample* output)
{
    UssPlayerInputSdk* source=(UssPlayerInputSdk*)user;UssPlayerInputSample result={0};uintptr_t sampled_base;size_t sampled_size;
    Reader memory={0};const UssActionState* action;uintptr_t manager=0,again_manager=0,at;
    unsigned char support=0,again_support=0,cycle[8],again_cycle[8],row[48],record[24];unsigned clear=0;
    if(output)memset(output,0,sizeof(*output));
    if(!output || !current || !current->valid || !current->mission || !current->primary.valid ||
        !player_sdk_scope(source) || source->binding_capture || !source->foreground ||
        !source->foreground(source->input_user) || source->api->game_state(source->mod)!=HF_STATE_MISSION)return 0;
    sampled_base=source->base;sampled_size=source->size;
    action=&current->primary.fields.action;
    if(action->index>=8 || action->avatar!=current->inventory.avatar || !action->manager ||
        action->record!=action->manager+0x53dcd0+(uintptr_t)action->index*0x1238)return 0;
    memory.api=source->api;memory.slot=-1;
    if(!get_ptr(&memory,source->base+0x3326d20,&manager) || manager!=action->manager ||
        !get(&memory,action->record,record,24) || memcmp(record,action->row,24))return 0;
    at=manager+0x150+(uintptr_t)action->index*0xa7aec+0x1b68;
    if(!get(&memory,at+24*32,&support,1) || !get(&memory,at+19*32,cycle,8) ||
        !get(&memory,current->inventory.row_address,row,48) || memcmp(row,current->inventory.row,48) ||
        !get(&memory,at+24*32,&again_support,1) || support!=again_support ||
        !get(&memory,at+19*32,again_cycle,8) || memcmp(cycle,again_cycle,8) ||
        !get(&memory,action->record,record,24) || memcmp(record,action->row,24) ||
        !get_ptr(&memory,source->base+0x3326d20,&again_manager) || manager!=again_manager || support>1 ||
        !player_sdk_ui(source,&clear) || !clear || !source->foreground(source->input_user) ||
        !player_sdk_scope(source) || source->base!=sampled_base || source->size!=sampled_size)return 0;
    if(source->settings->current.mode==USS_BIND_DEDICATED &&
        (!source->dedicated || !source->dedicated(source->input_user,&source->settings->current,&result.dedicated_down)))return 0;
    result.known=result.foreground=result.ui_clear=1;result.manager=manager;
    result.support_pressed=support;result.cycle_held=u32(cycle+4);*output=result;return 1;
}
/* The support action's first byte is a one-frame semantic press. A25ms
 * inventory cadence can miss it, or miss the release between two presses.
 * Read only a small current hint each frame; wake the ordinary guarded update,
 * never latch/queue a request or authorize an effect from cached inventory. */
static inline int uss_player_sdk_managed_pending(UssPlayerInputSdk* source,unsigned last_down,unsigned last_cycle)
{
    const UssCoreObservation* current;const UssActionState* action;Reader memory={0};
    uintptr_t manager=0,again=0,at;uint32_t avatar=0,cycle=0,again_cycle=0;unsigned char pressed=0,second=0;
    if(!source || !source->runtime || !source->settings || !source->runtime->life.owner.pair_present ||
        !source->foreground || !source->foreground(source->input_user) || source->binding_capture ||
        !player_sdk_scope(source) || source->api->game_state(source->mod)!=HF_STATE_MISSION)return 0;
    current=&source->runtime->latest;action=&current->primary.fields.action;
    if(!current->valid || !current->mission || !current->primary.valid || action->index>=8 ||
        action->avatar!=source->runtime->life.owner.context.local.avatar || !action->manager ||
        action->record!=action->manager+0x53dcd0+(uintptr_t)action->index*0x1238)return 0;
    if(source->settings->current.mode==USS_BIND_DEDICATED &&
        current->inventory.slots[2]!=source->runtime->life.owner.utility.entity)return 0;
    memory.api=source->api;memory.slot=-1;
    at=action->manager+0x150+(uintptr_t)action->index*0xa7aec+0x1b68+24*32;
    if(!get_ptr(&memory,source->base+0x3326d20,&manager) || manager!=action->manager ||
        !get(&memory,action->record,&avatar,4) || avatar!=action->avatar ||
        !get(&memory,at,&pressed,1) || pressed>1 || !get(&memory,at,&second,1) || pressed!=second ||
        !get(&memory,at-5*32+4,&cycle,4) || !get(&memory,at-5*32+4,&again_cycle,4) || cycle!=again_cycle ||
        !get_ptr(&memory,source->base+0x3326d20,&again) || again!=manager)return 0;
    return pressed || last_down || cycle==UINT32_C(0x3f800000) || last_cycle;
}
/* Retain the old caller signature for existing users/tests. */
static inline int uss_player_sdk_support_pending(UssPlayerInputSdk* source,unsigned last_down)
{return uss_player_sdk_managed_pending(source,last_down,0);}
static inline int player_sdk_pickup_pins(UssPlayerInputSdk* source)
{
    static const struct {uintptr_t rva;const char* hex;} pins[]={
        {0x102fb73,"488d1566f92201440fb683f7000000488bce4c8b"},
        {0x1030260,"48895c2408574883ec20488d99bc0000004c8bd2488bcb"},
        {0x1030631,"488d15a8ee220188433a488b05c65c2f02488b8828010000488b8190000000488bcfffd0440fb6433f488d1597ee220188433b"},
        {0x97ca43,"488b4580488b802840010080b8f700000000750c4585e47407443b6424507507c644"},
        {0x13779a7,"488b0d92e9fa01458bc14881c1dcc30a00498bd2e8a088cbff4c8b0579"}
    };
    Reader memory={0};unsigned i,j;unsigned char expected[64],actual[64];memory.api=source->api;memory.slot=-1;
    if(source->pickup_pins_checked)return (int)source->pickup_pins_valid;
    source->pickup_pins_checked=1;source->pickup_pins_valid=0;
    for(i=0;i<sizeof(pins)/sizeof(pins[0]);++i){
        size_t length=strlen(pins[i].hex)/2;
        for(j=0;j<length;++j)expected[j]=(unsigned char)((hex_digit(pins[i].hex[2*j])<<4)|hex_digit(pins[i].hex[2*j+1]));
        if(!get(&memory,source->base+pins[i].rva,actual,length) || memcmp(expected,actual,length))return 0;
    }
    source->pickup_pins_valid=1;return 1;
}
static inline int uss_player_sdk_pickup(void* user,UssPickupPolicy* output)
{
    UssPlayerInputSdk* source=(UssPlayerInputSdk*)user;Reader memory={0};uintptr_t owner,manager,active,again;
    unsigned char value=2,again_value=2;UssPickupPolicy result={0};uintptr_t sampled_base;size_t sampled_size;
    if(output)memset(output,0,sizeof(*output));
    if(!output || !player_sdk_scope(source) || !player_sdk_pickup_pins(source))return 0;
    sampled_base=source->base;sampled_size=source->size;memory.api=source->api;memory.slot=-1;
    if(!get_ptr(&memory,source->base+0x3326340,&owner) || !get_ptr(&memory,source->base+0x3326d68,&manager) ||
        !get_ptr(&memory,manager+0x14028,&active) || active!=owner+0xac3dc ||
        !get(&memory,active+0xf7,&value,1) || value>1 || !get(&memory,active+0xf7,&again_value,1) || value!=again_value ||
        !get_ptr(&memory,manager+0x14028,&again) || active!=again ||
        !get_ptr(&memory,source->base+0x3326340,&again) || owner!=again ||
        !get_ptr(&memory,source->base+0x3326d68,&again) || manager!=again || !player_sdk_scope(source) || source->base!=sampled_base || source->size!=sampled_size)return 0;
    result.known=1;result.switch_on_pickup=value;*output=result;return 1;
}
#endif
