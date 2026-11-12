/* HellForge documents an append-only HfApi. Require our SDK11 prefix, not the
 * full table of whichever newer header compiled this build. This does not relax
 * game build, native pins, session/schema or required-function checks. */
#ifndef USS_SDK_COMPAT_H
#define USS_SDK_COMPAT_H
#include "hellforge_mod.h"
#include <stddef.h>
#define USS_MIN_RUNTIME_SDK 11u
#define USS_REQUIRED_API_BYTES (offsetof(HfApi,stop_shots)+sizeof(((HfApi*)0)->stop_shots))
_Static_assert(USS_REQUIRED_API_BYTES==552,"Reviewed x64 SDK11 prefix changed");
#define USS_READ_CODE_API_BYTES (offsetof(HfApi,read_code)+sizeof(((HfApi*)0)->read_code))
_Static_assert(USS_READ_CODE_API_BYTES==584,"Reviewed x64 SDK13 read_code boundary changed");
static inline int uss_sdk_has_read_code(const HfApi* api)
{
    /* Size/version checked BEFORE touching the optional function pointer. */
    return api && api->version>=13 && api->size>=USS_READ_CODE_API_BYTES && api->read_code;
}
static inline int uss_sdk_compatible(const HfApi* api)
{
    return api && api->version>=USS_MIN_RUNTIME_SDK && api->size>=USS_REQUIRED_API_BYTES;
}
#endif
