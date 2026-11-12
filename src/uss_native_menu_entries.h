/* Exact Options entry contracts from CowboyBingus ModOptionsMenu (0BSD).
 * Retained-image matches recorded in HF055; not loaded-process acceptance.
 * Only resolve on the owning thread with current build/module/context validation. */
#ifndef USS_NATIVE_MENU_ENTRIES_H
#define USS_NATIVE_MENU_ENTRIES_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef struct UssMenuVec2 {float x,y;} UssMenuVec2;
_Static_assert(sizeof(UssMenuVec2)==8,"Windows x64 two-float by-value layout");
typedef void (*UssMenuCall_set_tab_labels)(uint64_t, const uint32_t *, int);
typedef void (*UssMenuCall_select_category)(uint64_t, int);
typedef void (*UssMenuCall_set_content_hidden)(uint64_t, uint8_t, uint8_t);
typedef void (*UssMenuCall_row_reset)(uint64_t, const float *, const float *);
typedef void (*UssMenuCall_row_release)(uint64_t);
typedef void (*UssMenuCall_row_init)(uint64_t, UssMenuVec2, UssMenuVec2, UssMenuVec2, int, uint64_t, const void *, uint8_t);
typedef void (*UssMenuCall_add_child)(uint64_t, uint64_t);
typedef void (*UssMenuCall_set_visible)(uint64_t, uint8_t);
typedef void (*UssMenuCall_set_label)(uint64_t, uint32_t);
typedef void (*UssMenuCall_set_string_arg)(uint64_t, uint32_t, const char *);
typedef uint8_t (*UssMenuCall_clear_args)(uint64_t);
typedef void (*UssMenuCall_set_choice)(uint64_t, uint32_t);
typedef void (*UssMenuCall_set_slider)(uint64_t, float);
typedef void (*UssMenuCall_finish_simple)(uint64_t);
typedef void (*UssMenuCall_finish_access)(uint64_t);
typedef void (*UssMenuCall_scroll_thumb)(uint64_t, float);
typedef void (*UssMenuCall_scroll_extent)(uint64_t, float);
typedef void (*UssMenuCall_set_opacity)(uint64_t, float);
typedef void (*UssMenuCall_scroll_reset)(uint64_t);
typedef void (*UssMenuCall_options_visual)(uint64_t, float);
typedef void (*UssMenuCall_measure_text)(uint64_t);
typedef void (*UssMenuCall_hide_description)(uint64_t, uint8_t, uint8_t);
typedef void (*UssMenuCall_play_sound)(uint64_t, uint32_t);
typedef void (*UssMenuCall_set_position)(uint64_t, UssMenuVec2);
typedef void (*UssMenuCall_set_size)(uint64_t, UssMenuVec2);
/* Display-only native contracts; no game action is reserved or remapped.
 * Direct text is narrow NUL-terminated input, copied/converted into widget
 * storage by143DD60; its byte status is not unconditional failure on zero.
 * Keep own captions below804bytes; validate resulting text, not status alone.
 * Caller must establish parent, screen lifetime and event ownership separately. */
typedef void (*UssMenuCall_assignment_display_init)(uint64_t,UssMenuVec2,UssMenuVec2,UssMenuVec2,int);
typedef uint8_t (*UssMenuCall_assignment_text)(uint64_t,const char*);
typedef void (*UssMenuCall_assignment_layout)(uint64_t);
typedef void (*UssMenuCall_assignment_highlight)(uint64_t,uint8_t);
typedef struct UssNativeMenuEntries {
    UssMenuCall_set_tab_labels set_tab_labels;
    UssMenuCall_select_category select_category;
    UssMenuCall_set_content_hidden set_content_hidden;
    UssMenuCall_row_reset row_reset;
    UssMenuCall_row_release row_release;
    UssMenuCall_row_init row_init;
    UssMenuCall_add_child add_child;
    UssMenuCall_set_visible set_visible;
    UssMenuCall_set_label set_label;
    UssMenuCall_set_string_arg set_string_arg;
    UssMenuCall_clear_args clear_args;
    UssMenuCall_set_choice set_choice;
    UssMenuCall_set_slider set_slider;
    UssMenuCall_finish_simple finish_simple;
    UssMenuCall_finish_access finish_access;
    UssMenuCall_scroll_thumb scroll_thumb;
    UssMenuCall_scroll_extent scroll_extent;
    UssMenuCall_set_opacity set_opacity;
    UssMenuCall_scroll_reset scroll_reset;
    UssMenuCall_options_visual options_visual;
    UssMenuCall_measure_text measure_text;
    UssMenuCall_hide_description hide_description;
    UssMenuCall_play_sound play_sound;
    UssMenuCall_set_position set_position;
    UssMenuCall_set_size set_size;
    UssMenuCall_assignment_display_init assignment_display_init;
    UssMenuCall_assignment_text assignment_text;
    UssMenuCall_assignment_layout assignment_layout;
    UssMenuCall_assignment_highlight assignment_highlight;
} UssNativeMenuEntries;
typedef struct UssMenuEntryPin {uint32_t rva,length;unsigned char bytes[64];} UssMenuEntryPin;
static const UssMenuEntryPin uss_menu_entry_pins[]={
    {0x17AAC50u,16u,{0x48,0x89,0x54,0x24,0x10,0x53,0x56,0x48,0x83,0xec,0x68,0x0f,0x29,0x74,0x24,0x30}}, /* set_tab_labels */
    {0x19999C0u,16u,{0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9,0x89,0x91,0x58,0x1e,0x14,0x00,0x39}}, /* select_category */
    {0x19984C0u,16u,{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xec,0x50,0x41}}, /* set_content_hidden */
    {0x17FF700u,16u,{0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x48,0x89,0x7c,0x24,0x20,0x55}}, /* row_reset */
    {0x17FF4B0u,16u,{0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20,0x8b,0x81,0xc0,0x7a,0x00,0x00}}, /* row_release */
    {0x17FE8D0u,16u,{0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x70,0x48,0x8b,0xbc,0x24,0xb0,0x00}}, /* row_init */
    {0x144C5C0u,16u,{0x40,0x53,0x48,0x83,0xec,0x20,0x4c,0x8b,0xca,0x48,0x8b,0xd9,0x48,0x8b,0x91,0xe0}}, /* add_child */
    {0x144CFB0u,16u,{0x48,0x83,0xec,0x28,0x44,0x8b,0x01,0x4c,0x8b,0xd1,0x41,0x8b,0xc0,0x45,0x8b,0xc8}}, /* set_visible */
    {0x143BF90u,16u,{0x48,0x83,0xec,0x28,0x4c,0x8b,0xd9,0x39,0x91,0x10,0x01,0x00,0x00,0x0f,0x84,0x80}}, /* set_label */
    {0x143C950u,16u,{0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9,0x48,0x81,0xc1,0x10,0x01,0x00,0x00}}, /* set_string_arg */
    {0x143A0F0u,16u,{0x80,0xb9,0x58,0x01,0x00,0x00,0x00,0xc6,0x81,0x58,0x01,0x00,0x00,0x00,0x0f,0x97}}, /* clear_args */
    {0x17FE7E0u,16u,{0x80,0xb9,0xf8,0x32,0x00,0x00,0x00,0x74,0x28,0x83,0xfa,0x10,0x73,0x23,0x48,0x63}}, /* set_choice */
    {0x17FC2C0u,16u,{0x40,0x53,0x48,0x83,0xec,0x20,0xf3,0x0f,0x10,0x91,0xac,0x08,0x00,0x00,0x48,0x8b}}, /* set_slider */
    {0x1807060u,16u,{0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x30,0x8b,0x81,0x10,0x0b,0x00,0x00}}, /* finish_simple */
    {0x1800FE0u,16u,{0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x30,0x8b,0x81,0x50,0x57,0x00,0x00}}, /* finish_access */
    {0x1793F60u,16u,{0x48,0x89,0x5c,0x24,0x10,0x57,0x48,0x83,0xec,0x40,0x48,0x8b,0xd9,0x0f,0x29,0x74}}, /* scroll_thumb */
    {0x1794600u,16u,{0xf3,0x0f,0x10,0xa1,0xb0,0x07,0x00,0x00,0x0f,0x28,0xd1,0x0f,0x2e,0xe2,0x7a,0x02}}, /* scroll_extent */
    {0x1448AD0u,16u,{0x40,0x57,0x48,0x83,0xec,0x20,0xf3,0x0f,0x10,0x41,0x44,0x48,0x8b,0xf9,0x0f,0x2e}}, /* set_opacity */
    {0x1794570u,16u,{0x80,0xb9,0xb5,0x07,0x00,0x00,0x00,0x74,0x19,0xf3,0x0f,0x10,0x89,0xbc,0x07,0x00}}, /* scroll_reset */
    {0x1998590u,16u,{0x48,0x8b,0xc4,0x57,0x48,0x83,0xec,0x70,0x80,0xb9,0x15,0x22,0x14,0x00,0x00,0x48}}, /* options_visual */
    {0x144E1A0u,16u,{0x40,0x53,0x48,0x83,0xec,0x70,0x48,0x8b,0xd9,0x8b,0x09,0x8b,0xc1,0xc1,0xe8,0x12}}, /* measure_text */
    {0x17FAB90u,16u,{0x48,0x89,0x5c,0x24,0x10,0x57,0x48,0x83,0xec,0x70,0x0f,0xb6,0xda,0x48,0x8b,0xf9}}, /* hide_description */
    {0x1327F50u,16u,{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xec,0x20,0x48}}, /* play_sound */
    {0x14476A0u,28u,{0x48,0x89,0x5c,0x24,0x18,0x48,0x89,0x6c,0x24,0x20,0x48,0x89,0x54,0x24,0x10,0x56,0x57,0x41,0x57,0x48,0x83,0xec,0x20,0xf3,0x0f,0x10,0x41,0x04}}, /* set_position */
    {0x1447160u,28u,{0x48,0x89,0x5c,0x24,0x18,0x48,0x89,0x6c,0x24,0x20,0x48,0x89,0x54,0x24,0x10,0x56,0x57,0x41,0x57,0x48,0x83,0xec,0x20,0xf3,0x0f,0x10,0x41,0x0c}}, /* set_size */
    {0x17A2580u,27u,{0x48,0x83,0xec,0x38,0xf3,0x0f,0x10,0x05,0xe4,0x47,0xc2,0x00,0x8b,0x44,0x24,0x60,0xf3,0x0f,0x11,0x44,0x24,0x28,0x89,0x44,0x24,0x20,0xc6}}, /* assignment_display_init */
    {0x143DD60u,28u,{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xec,0x20,0x48,0x85,0xd2,0x48,0x8d,0x3d,0x5e,0x0d,0xe0,0x00,0x48,0x8b,0xd9}}, /* assignment_text */
    {0x17A2CC0u,23u,{0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x83,0xb9,0x70,0x08,0x00,0x00,0x00,0x48,0x8b,0xd9,0x0f,0x85,0x96,0x00,0x00,0x00}}, /* assignment_layout */
    {0x17A2720u,19u,{0x38,0x91,0x78,0x08,0x00,0x00,0x74,0x0b,0x88,0x91,0x78,0x08,0x00,0x00,0xe9,0x9d,0x06,0x00,0x00}}, /* assignment_highlight */
};
enum {USS_MENU_ENTRY_COUNT=sizeof(uss_menu_entry_pins)/sizeof(uss_menu_entry_pins[0])};
typedef struct UssMenuEntryReader {
    void* user;
    uint64_t (*context)(void*); /* Fresh process/module/thread scope token, zero refuses. */
    int (*read_executable)(void*,uintptr_t,unsigned char*,size_t); /* Exact read plus current executable-image/page checks. */
} UssMenuEntryReader;
static inline int uss_native_menu_resolve_trace(const UssMenuEntryReader* io,uintptr_t base,size_t module_size,UssNativeMenuEntries* out,unsigned* failed_entry)
{
    UssNativeMenuEntries resolved;uint64_t context;unsigned i;unsigned char actual[64];
    if(failed_entry)*failed_entry=USS_MENU_ENTRY_COUNT;
    if(!io || !io->context || !io->read_executable || !out || !base || !module_size || base>UINTPTR_MAX-module_size)return 0;
    context=io->context(io->user);if(!context)return 0;
    for(i=0;i<USS_MENU_ENTRY_COUNT;++i){
        const UssMenuEntryPin* pin=&uss_menu_entry_pins[i];
        if(failed_entry)*failed_entry=i;
        if(pin->rva>module_size || pin->length>module_size-pin->rva ||
            !io->read_executable(io->user,base+pin->rva,actual,pin->length) || memcmp(actual,pin->bytes,pin->length))return 0;
    }
    if(failed_entry)*failed_entry=USS_MENU_ENTRY_COUNT;
    if(io->context(io->user)!=context)return 0;
    resolved.set_tab_labels=(UssMenuCall_set_tab_labels)(base+0x17AAC50u);
    resolved.select_category=(UssMenuCall_select_category)(base+0x19999C0u);
    resolved.set_content_hidden=(UssMenuCall_set_content_hidden)(base+0x19984C0u);
    resolved.row_reset=(UssMenuCall_row_reset)(base+0x17FF700u);
    resolved.row_release=(UssMenuCall_row_release)(base+0x17FF4B0u);
    resolved.row_init=(UssMenuCall_row_init)(base+0x17FE8D0u);
    resolved.add_child=(UssMenuCall_add_child)(base+0x144C5C0u);
    resolved.set_visible=(UssMenuCall_set_visible)(base+0x144CFB0u);
    resolved.set_label=(UssMenuCall_set_label)(base+0x143BF90u);
    resolved.set_string_arg=(UssMenuCall_set_string_arg)(base+0x143C950u);
    resolved.clear_args=(UssMenuCall_clear_args)(base+0x143A0F0u);
    resolved.set_choice=(UssMenuCall_set_choice)(base+0x17FE7E0u);
    resolved.set_slider=(UssMenuCall_set_slider)(base+0x17FC2C0u);
    resolved.finish_simple=(UssMenuCall_finish_simple)(base+0x1807060u);
    resolved.finish_access=(UssMenuCall_finish_access)(base+0x1800FE0u);
    resolved.scroll_thumb=(UssMenuCall_scroll_thumb)(base+0x1793F60u);
    resolved.scroll_extent=(UssMenuCall_scroll_extent)(base+0x1794600u);
    resolved.set_opacity=(UssMenuCall_set_opacity)(base+0x1448AD0u);
    resolved.scroll_reset=(UssMenuCall_scroll_reset)(base+0x1794570u);
    resolved.options_visual=(UssMenuCall_options_visual)(base+0x1998590u);
    resolved.measure_text=(UssMenuCall_measure_text)(base+0x144E1A0u);
    resolved.hide_description=(UssMenuCall_hide_description)(base+0x17FAB90u);
    resolved.play_sound=(UssMenuCall_play_sound)(base+0x1327F50u);
    resolved.set_position=(UssMenuCall_set_position)(base+0x14476A0u);
    resolved.set_size=(UssMenuCall_set_size)(base+0x1447160u);
    resolved.assignment_display_init=(UssMenuCall_assignment_display_init)(base+0x17A2580u);
    resolved.assignment_text=(UssMenuCall_assignment_text)(base+0x143DD60u);
    resolved.assignment_layout=(UssMenuCall_assignment_layout)(base+0x17A2CC0u);
    resolved.assignment_highlight=(UssMenuCall_assignment_highlight)(base+0x17A2720u);
    *out=resolved;return 1;
}
static inline int uss_native_menu_resolve(const UssMenuEntryReader* io,uintptr_t base,size_t module_size,UssNativeMenuEntries* out)
{return uss_native_menu_resolve_trace(io,base,module_size,out,NULL);}
#endif
