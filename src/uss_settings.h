/* Menu-owned settings. Pure data/validation: no renderer, SDK calls or game effects. */
#ifndef USS_SETTINGS_H
#define USS_SETTINGS_H
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

enum { USS_BIND_DEFAULT=1, USS_BIND_DEDICATED=2 };
/* XInput button bits; triggers have distinct logical codes above the button word. */
enum { USS_BIND_LEFT_TRIGGER=0x10000, USS_BIND_RIGHT_TRIGGER=0x20000 };
typedef struct UssSettings {
    uint32_t schema,mode,utility_key,utility_controller,menu_key;
} UssSettings;
typedef struct UssSettingsState {
    UssSettings current,pending;
    uint64_t revision,provider;
    unsigned pending_present,external,release_required;
} UssSettingsState;
typedef struct UssMenuConnection {
    uint64_t provider; /* Identity changes when a menu/bridge instance is replaced. */
    unsigned options_registered,bindings_registered,subscribed,values_valid;
    UssSettings applied;
} UssMenuConnection;

static inline UssSettings uss_settings_defaults(void)
{
    UssSettings s={1,USS_BIND_DEFAULT,0x36,0,0x76}; /* 6; F7 opens USS. Never F8. */
    return s;
}
static inline int uss_settings_valid(const UssSettings* s)
{
    uint32_t button;
    if(!s || s->schema!=1 || (s->mode!=USS_BIND_DEFAULT && s->mode!=USS_BIND_DEDICATED) ||
        (!s->utility_key && !s->utility_controller) || s->utility_key>254 || s->menu_key<1 || s->menu_key>254 ||
        s->utility_key==0x77 || s->menu_key==0x77 || s->utility_key==s->menu_key)return 0;
    button=s->utility_controller;
    return !button || button==USS_BIND_LEFT_TRIGGER || button==USS_BIND_RIGHT_TRIGGER ||
        ((button&0xF3FFu)==button && !(button&(button-1)));
}
static inline int uss_settings_equal(const UssSettings* a,const UssSettings* b)
{
    return a->schema==b->schema && a->mode==b->mode && a->utility_key==b->utility_key &&
        a->utility_controller==b->utility_controller && a->menu_key==b->menu_key;
}
/* A deliberately small, bounded JSON schema. Unknown/duplicate/missing keys and
 * future schemas refuse without changing the caller's existing settings/file. */
static inline void uss_settings_space(const char* data,size_t length,size_t* at)
{while(*at<length && (data[*at]==' ' || data[*at]=='\t' || data[*at]=='\r' || data[*at]=='\n'))++*at;}
static inline int uss_settings_decode(const char* data,size_t length,UssSettings* output)
{
    static const char* names[]={"schema","mode","utility_key","utility_controller","menu_key"};
    uint32_t values[5]={0},seen=0;size_t at=0;
    if(!data || !output || !length || length>512)return 0;
    uss_settings_space(data,length,&at);if(at>=length || data[at++]!='{')return 0;
    for(;;){
        size_t start;unsigned key;uint32_t value=0;
        uss_settings_space(data,length,&at);if(at>=length || data[at++]!='"')return 0;
        start=at;while(at<length && data[at]!='"')++at;if(at==length)return 0;
        for(key=0;key<5;++key)if(strlen(names[key])==at-start && !memcmp(names[key],data+start,at-start))break;
        if(key==5 || (seen&(1u<<key)))return 0;
        ++at;uss_settings_space(data,length,&at);if(at>=length || data[at++]!=':')return 0;
        uss_settings_space(data,length,&at);if(at>=length || data[at]<'0' || data[at]>'9')return 0;
        start=at;
        while(at<length && data[at]>='0' && data[at]<='9'){
            unsigned digit=(unsigned)(data[at++]-'0');if(value>(UINT32_MAX-digit)/10)return 0;
            value=value*10+digit;
        }
        if(at-start>1 && data[start]=='0')return 0;
        values[key]=value;seen|=1u<<key;
        uss_settings_space(data,length,&at);if(at>=length)return 0;
        if(data[at]=='}'){++at;break;}
        if(data[at++]!=',')return 0;
    }
    uss_settings_space(data,length,&at);
    if(at!=length || seen!=31)return 0;
    {UssSettings parsed={values[0],values[1],values[2],values[3],values[4]};
        if(!uss_settings_valid(&parsed))return 0;
        *output=parsed;return 1;}
}
static inline int uss_settings_encode(const UssSettings* s,char* data,size_t size)
{
    int used;if(!uss_settings_valid(s) || !data || !size)return 0;
    used=snprintf(data,size,"{\n  \"schema\": 1,\n  \"mode\": %u,\n  \"utility_key\": %u,\n  \"utility_controller\": %u,\n  \"menu_key\": %u\n}\n",
        s->mode,s->utility_key,s->utility_controller,s->menu_key);
    return used>0 && (size_t)used<size?used:0;
}
static inline void uss_settings_init(UssSettingsState* state,const UssSettings* saved)
{
    memset(state,0,sizeof(*state));state->current=uss_settings_valid(saved)?*saved:uss_settings_defaults();
    state->revision=1;state->release_required=1;
}
/* UI edits queue a validated draft. A game-thread idle boundary commits it; the
 * storage adapter must save that committed snapshot, never an unvalidated draft. */
static inline int uss_settings_queue(UssSettingsState* state,const UssSettings* draft)
{
    if(!state || !uss_settings_valid(draft))return 0;
    state->pending=*draft;state->pending_present=1;return 1;
}
static inline int uss_settings_commit(UssSettingsState* state,unsigned transaction_active)
{
    if(!state || transaction_active || !state->pending_present || state->revision==UINT64_MAX)return 0;
    state->pending_present=0;if(uss_settings_equal(&state->current,&state->pending))return 0;
    state->current=state->pending;++state->revision;state->release_required=1;return 1;
}
/* Installation alone never wins. Partial/failed registration stays retryable.
 * A disconnect retains the last committed choices; it does not reset defaults. */
static inline int uss_settings_connect(UssSettingsState* state,const UssMenuConnection* connection)
{
    if(!state)return 0;
    if(!connection || !connection->provider || !connection->options_registered || !connection->bindings_registered ||
        !connection->subscribed || !connection->values_valid || !uss_settings_valid(&connection->applied)){
        if(state->external){state->release_required=1;state->pending_present=0;}
        state->external=0;state->provider=0;return 0;
    }
    if(!state->external || state->provider!=connection->provider){
        state->release_required=1;state->external=1;state->provider=connection->provider;
        return uss_settings_queue(state,&connection->applied);
    }
    return 1;
}
static inline int uss_settings_external_change(UssSettingsState* state,uint64_t provider,const UssSettings* applied)
{
    return state && state->external && provider==state->provider && uss_settings_queue(state,applied);
}
/* Gate both dedicated and vanilla-action sampling through this function. Opening
 * a menu/capture, losing focus, or finishing work cannot synthesize a held action. */
static inline int uss_settings_input_ready(UssSettingsState* state,unsigned foreground,unsigned menu_open,
    unsigned binding_capture,unsigned transaction_active,unsigned any_managed_input_down)
{
    if(!state)return 0;
    if(!foreground || menu_open || binding_capture || transaction_active){state->release_required=1;return 0;}
    if(state->release_required){if(!any_managed_input_down)state->release_required=0;return 0;}
    return 1;
}
#endif
