#ifdef NDEBUG
#error Assertions required
#endif
#include <assert.h>
#include <stdio.h>
#include "../src/sdk_compat.h"
int main(void){HfApi api={0};assert(!uss_sdk_compatible(NULL));
api.version=11;api.size=552;assert(uss_sdk_compatible(&api));
api.version=12;api.size=560;assert(uss_sdk_compatible(&api));
api.version=13;api.size=568;assert(uss_sdk_compatible(&api));
api.version=99;api.size=4096;assert(uss_sdk_compatible(&api));
api.size=551;assert(!uss_sdk_compatible(&api));
api.size=560;api.version=10;assert(!uss_sdk_compatible(&api));
api.version=13;api.size=USS_READ_CODE_API_BYTES-1;assert(!uss_sdk_has_read_code(&api));
api.size=USS_READ_CODE_API_BYTES;assert(!uss_sdk_has_read_code(&api));
puts("SDK11 prefix: old/short refusal and append-only newer table compatibility PASS");return 0;}
