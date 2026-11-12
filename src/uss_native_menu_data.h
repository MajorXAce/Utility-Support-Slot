/* Native menu data formats, from CowboyBingus's 0BSD ModOptionsMenu/ModBindingsMenu.
 * Building these OWN bytes does not validate an address or authorize a native call.
 * Options rows and binding rows have different storage and constructors. */
#ifndef USS_NATIVE_MENU_DATA_H
#define USS_NATIVE_MENU_DATA_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <limits.h>

enum {USS_OPTION_DESCRIPTOR_BYTES=216,USS_OPTION_ROW_BYTES=31464,USS_OPTION_ROWS_MAX=32,
    USS_BINDING_ROW_BYTES=24784,USS_BINDING_ROWS_MAX=97,USS_SELECTOR_CHOICES_MAX=16};
typedef struct UssOptionDescriptor {
    _Alignas(16) unsigned char bytes[USS_OPTION_DESCRIPTOR_BYTES];
} UssOptionDescriptor;
typedef struct UssNativeBindingRow {uint32_t group,action,label;} UssNativeBindingRow;
_Static_assert(sizeof(UssNativeBindingRow)==12,"binding builder consumes triples of u32, not Options descriptors");
_Static_assert(_Alignof(UssOptionDescriptor)>=16,"descriptor storage must remain aligned and retained");
static inline uint32_t uss_menu_word(const unsigned char* bytes,size_t at)
{uint32_t value;memcpy(&value,bytes+at,4);return value;}
static inline void uss_menu_put_word(unsigned char* bytes,size_t at,uint32_t value)
{memcpy(bytes+at,&value,4);}
/* A current reader must provide the full nominated inert descriptor. Later native
 * host handlers still need checking: value_type zero alone is not a safety proof. */
static inline int uss_menu_inert_template(const unsigned char* bytes,size_t length)
{
    return bytes && length==USS_OPTION_DESCRIPTOR_BYTES && uss_menu_word(bytes,0)==139 &&
        uss_menu_word(bytes,12)==0 && uss_menu_word(bytes,32)==0;
}
static inline int uss_menu_selector(UssOptionDescriptor* out,const unsigned char* template_bytes,size_t length,
    uint32_t label,const uint32_t* choices,unsigned count)
{
    UssOptionDescriptor built;unsigned i;
    if(!out || !uss_menu_inert_template(template_bytes,length) || !label || !choices || count<2 || count>USS_SELECTOR_CHOICES_MAX)return 0;
    for(i=0;i<count;++i)if(!choices[i])return 0;
    memset(&built,0,sizeof(built));
    /* Fields match the original Options menu's independently constructed descriptor.
     * No copied live pointer/choice payload or native settings persistence type. */
    uss_menu_put_word(built.bytes,0,139);uss_menu_put_word(built.bytes,4,10);
    uss_menu_put_word(built.bytes,8,2);uss_menu_put_word(built.bytes,16,label);
    uss_menu_put_word(built.bytes,64,count);
    for(i=0;i<count;++i)uss_menu_put_word(built.bytes,68+4*i,choices[i]);
    *out=built;return 1;
}
/* Kind5 has a press edge without a selector child. On the reviewed build,
 * caller1802200 skips native settings dispatch for verified ID139/value_type0;
 * ID139 is also a no-op jump-table case. This prepares OWN descriptor bytes only.
 * Rendering, event ownership and live pins must be established before use. */
static inline int uss_menu_assignment_descriptor(UssOptionDescriptor* out,
    const unsigned char* template_bytes,size_t length,uint32_t label)
{
    UssOptionDescriptor built;
    if(!out || !label || !uss_menu_inert_template(template_bytes,length))return 0;
    memset(&built,0,sizeof(built));
    uss_menu_put_word(built.bytes,0,139);uss_menu_put_word(built.bytes,4,10);
    uss_menu_put_word(built.bytes,8,5);uss_menu_put_word(built.bytes,16,label);
    *out=built;return 1;
}
/* Explicit host-specific bounds prevent the two row arrays from being treated as
 * interchangeable merely because both use native widgets. */
static inline int uss_menu_option_address(uintptr_t first,unsigned index,uintptr_t* out)
{
    uintptr_t offset;if(!first || !out || index>=USS_OPTION_ROWS_MAX)return 0;
    offset=(uintptr_t)index*USS_OPTION_ROW_BYTES;if(first>UINTPTR_MAX-offset)return 0;
    *out=first+offset;return 1;
}
static inline int uss_menu_binding_address(uintptr_t first,unsigned index,uintptr_t* out)
{
    uintptr_t offset;if(!first || !out || index>=USS_BINDING_ROWS_MAX)return 0;
    offset=(uintptr_t)index*USS_BINDING_ROW_BYTES;if(first>UINTPTR_MAX-offset)return 0;
    *out=first+offset;return 1;
}
#endif
