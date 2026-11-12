/* Coherent native admission inputs. Read-only, no claim or mapping writes. */
#ifndef USS_NATIVE_BINDING_CONTEXT_H
#define USS_NATIVE_BINDING_CONTEXT_H
#include "uss_native_binding_reader.h"
#include "uss_binding_reservations.h"
#include "uss_binding_reservation_files.h"
typedef struct UssNativeBindingContext {
    UssNativeBindingSnapshot live,defaults;uint32_t label;unsigned valid;
} UssNativeBindingContext;
static inline int uss_native_binding_context_read(const UssMenuObservationIo* io,unsigned group,unsigned action,
    UssNativeBindingCache* live_cache,UssNativeBindingCache* default_cache,UssNativeBindingContext* output)
{
    UssNativeBindingContext snapshot={0};UssNativeBindingSnapshot after_live,after_default;
    uint64_t epoch;uint32_t label,again;uintptr_t at;
    if(!io || !io->context || !io->read || !live_cache || !default_cache || live_cache==default_cache ||
        !output || group>=13 || action>=97 || io->module_size<0x347cf20 ||
        !uss_menu_pointer(io->base) || io->base>UINTPTR_MAX-io->module_size)return 0;
    epoch=io->context(io->user);if(!epoch)return 0;at=io->base+0x26438a0+4*(97*group+action);
    if(!io->read(io->user,at,&label,4) || !label ||
        !uss_native_binding_observe_map(io,live_cache,group,action,0,&snapshot.live,NULL) ||
        !uss_native_binding_observe_map(io,default_cache,group,action,1,&snapshot.defaults,NULL) ||
        snapshot.live.epoch!=epoch || snapshot.defaults.epoch!=epoch || snapshot.live.owner!=snapshot.defaults.owner ||
        !uss_native_binding_observe_map(io,live_cache,group,action,0,&after_live,NULL) ||
        !uss_native_binding_observe_map(io,default_cache,group,action,1,&after_default,NULL) ||
        memcmp(&snapshot.live,&after_live,sizeof(after_live)) || memcmp(&snapshot.defaults,&after_default,sizeof(after_default)) ||
        !io->read(io->user,at,&again,4) || label!=again || io->context(io->user)!=epoch)return 0;
    snapshot.label=label;snapshot.valid=1;*output=snapshot;return 1;
}
/* Original label identity is another necessary input, NOT a slot claim. */
static inline int uss_native_binding_context_original(const UssNativeBindingContext* context)
{
    int index;
    if(!context || !context->valid || !context->live.valid || !context->defaults.valid ||
        context->live.code!=context->defaults.code || (index=uss_binding_dormant_index(context->live.code))<7)return 0;
    return context->label==uss_binding_dormant_labels[index];
}
/* One bounded admission inspection. This returns a CANDIDATE snapshot only:
 * allocator RAM state/claim persistence are separate, and no native effect is
 * authorized. Caller must supply the exact resolved reservation source path. */
static inline int uss_native_binding_candidate(const UssBindingReservationFiles* files,
    const UssMenuObservationIo* io,unsigned group,unsigned action,const char* id,
    UssNativeBindingCache* live_cache,UssNativeBindingCache* default_cache,UssNativeBindingContext* output)
{
    UssNativeBindingContext context;unsigned reservation;
    if(!io || !io->context || !output || group>=13 || action>=97)return 0;
    reservation=uss_binding_reservation_files_query(files,group*65536u+action,id);
    if(reservation!=USS_BIND_RESERVATION_CANDIDATE && reservation!=USS_BIND_RESERVATION_SELF)return 0;
    if(!uss_native_binding_context_read(io,group,action,live_cache,default_cache,&context) ||
        !uss_native_binding_context_original(&context) || !uss_binding_reservation_files_current(files) ||
        io->context(io->user)!=context.live.epoch)return 0;
    *output=context;return 1;
}
#endif
