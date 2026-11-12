#include <stddef.h>
#include <stdio.h>
#include "hellforge_mod.h"
_Static_assert(HF_MOD_SDK_VERSION==11,"reviewed SDK11");
_Static_assert(sizeof(HfApi)==552,"reviewed x64 SDK11 API size");
_Static_assert(sizeof(HfSession)==72,"retained session ABI");
_Static_assert(offsetof(HfApi,mods)==424,"SDK9 prefix retained");
_Static_assert(offsetof(HfApi,heal)==432,"SDK11 appended after SDK10");
int main(void) { printf("SDK11 ABI: api=%zu session=%zu mods=%zu heal=%zu\n",sizeof(HfApi),sizeof(HfSession),offsetof(HfApi,mods),offsetof(HfApi,heal)); return 0; }
