/* Display-only native child for the standalone Utility Weapon assignment.
 * Caller retains this object at one address for its entire native screen life.
 * No native action mapping, input capture or settings effect is performed here. */
#ifndef USS_NATIVE_ASSIGNMENT_DISPLAY_H
#define USS_NATIVE_ASSIGNMENT_DISPLAY_H
#include "uss_native_menu_entries.h"

enum {USS_ASSIGNMENT_DISPLAY_BYTES=0x8c0,USS_ASSIGNMENT_CAPTION_BYTES=128,
    USS_ASSIGNMENT_IDENTITY=0x870,USS_ASSIGNMENT_TEXT=0x3f0,
    USS_ASSIGNMENT_TEXT_BUFFER=0x50c};
typedef struct UssAssignmentDisplay {
    _Alignas(16) unsigned char widget[USS_ASSIGNMENT_DISPLAY_BYTES];
    char caption[USS_ASSIGNMENT_CAPTION_BYTES];
    uintptr_t identity,parent;
    unsigned entered,attached,stopped,effects;
} UssAssignmentDisplay;
typedef struct UssAssignmentDisplayIo {
    void* user;
    /* Prove current owned parent/screen/thread/pins; unknown/closed refuses.
     * Parent teardown releases this attached child; never release it twice. */
    int (*guard)(void*,uintptr_t parent);
    UssNativeMenuEntries native;
} UssAssignmentDisplayIo;
static inline size_t uss_assignment_caption_length(const char* text)
{
    size_t i;if(!text)return 0;
    for(i=0;i<USS_ASSIGNMENT_CAPTION_BYTES;++i)if(!text[i])return i;
    return 0;
}
static inline int uss_assignment_display_guard(UssAssignmentDisplay* s,const UssAssignmentDisplayIo* io)
{
    if(!s || !io || !io->guard || s->stopped || s->identity!=(uintptr_t)s ||
        !io->guard(io->user,s->parent)){if(s)s->stopped=1;return 0;}return 1;
}
#define USS_ASSIGNMENT_CALL(s,io,call) do { \
    if(!uss_assignment_display_guard((s),(io)))return 0; \
    ++(s)->effects;(call); \
    if(!uss_assignment_display_guard((s),(io)))return 0; \
} while(0)
static inline int uss_assignment_display_caption(UssAssignmentDisplay* s,
    const UssAssignmentDisplayIo* io,const char* text)
{
    char next[USS_ASSIGNMENT_CAPTION_BYTES];size_t length=uss_assignment_caption_length(text);
    if(!s || !s->entered || !length || !io || !io->native.assignment_text ||
        !io->native.assignment_layout || !uss_assignment_display_guard(s,io))return 0;
    memcpy(next,text,length+1);
    /* Native setter can return0 for unchanged text. Verify the inline UTF-8
     * copy instead of confusing that status with a failed native operation. */
    USS_ASSIGNMENT_CALL(s,io,(void)io->native.assignment_text(
        (uintptr_t)s->widget+USS_ASSIGNMENT_TEXT,next));
    if(memcmp(s->widget+USS_ASSIGNMENT_TEXT_BUFFER,next,length+1)){s->stopped=1;return 0;}
    USS_ASSIGNMENT_CALL(s,io,io->native.assignment_layout((uintptr_t)s->widget));
    memcpy(s->caption,next,length+1);return 1;
}
static inline int uss_assignment_display_build(UssAssignmentDisplay* s,
    const UssAssignmentDisplayIo* io,uintptr_t parent,const char* caption)
{
    UssMenuVec2 position={300.0f,0},pivot={0.5f,0.5f};uint64_t native_identity;
    size_t length=uss_assignment_caption_length(caption);
    if(!s || s->entered || !length || !parent || (parent&7) || !io || !io->guard ||
        !io->native.assignment_display_init || !io->native.assignment_text ||
        !io->native.assignment_layout || !io->native.set_opacity || !io->native.add_child)return 0;
    s->parent=parent;s->identity=(uintptr_t)s;
    if(!uss_assignment_display_guard(s,io))return 0;
    /* Consume before entry; never reconstruct after an uncertain native effect. */
    s->entered=1;
    USS_ASSIGNMENT_CALL(s,io,io->native.assignment_display_init(
        (uintptr_t)s->widget,position,pivot,pivot,12));
    memcpy(&native_identity,s->widget+USS_ASSIGNMENT_IDENTITY,8);
    if(native_identity!=UINT64_MAX){s->stopped=1;return 0;}
    /* OWN retained widget storage only. Native17A2950 selects this identity0
     * text-layout branch; avoiding that updater avoids its action lookup too. */
    native_identity=0;memcpy(s->widget+USS_ASSIGNMENT_IDENTITY,&native_identity,8);
    if(!uss_assignment_display_caption(s,io,caption))return 0;
    USS_ASSIGNMENT_CALL(s,io,io->native.set_opacity((uintptr_t)s->widget+USS_ASSIGNMENT_TEXT,1.0f));
    USS_ASSIGNMENT_CALL(s,io,io->native.set_opacity((uintptr_t)s->widget+0x268,1.0f));
    USS_ASSIGNMENT_CALL(s,io,io->native.add_child(parent,(uintptr_t)s->widget));
    s->attached=1;return 1;
}
/* A native category rebuild releases the attached child tree. Only that positive
 * teardown postcondition permits preparing a new view in the retained storage.
 * This never clears a live/partially constructed display to retry its effects. */
static inline int uss_assignment_display_reset_released(UssAssignmentDisplay* s)
{
    uint32_t flags;uintptr_t fields[5];unsigned i;
    static const size_t offsets[5]={0xe0,0xe8,0xf0,0x100,0x108};
    if(!s)return 0;if(!s->entered)return 1;
    memcpy(&flags,s->widget,4);
    for(i=0;i<5;++i)memcpy(&fields[i],s->widget+offsets[i],sizeof(fields[i]));
    if(flags || fields[0] || fields[1] || fields[2] || fields[3] || fields[4])return 0;
    memset(s,0,sizeof(*s));return 1;
}
#undef USS_ASSIGNMENT_CALL
#endif
