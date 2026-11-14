#include "../src/weapon_policy.h"
#ifdef NDEBUG
#error Policy checks require active assertions
#endif
#include <assert.h>
#include <stdio.h>

int main(void)
{
    const unsigned char silo[8] = {0xaa,0x79,0xca,0x92,0x05,0x46,0x13,0xfc};
    const unsigned char c4[8] = {0x3d,0x2f,0xf5,0x21,0x63,0x0d,0xf5,0x51};
    /* Compendium shredder_remote: printed id a541f9b628593d0d, stored here in memory (reversed) order. */
    const unsigned char shredder[8] = {0x0d,0x3d,0x59,0x28,0xb6,0xf9,0x41,0xa5};
    const unsigned char shredder_printed_order[8] = {0xa5,0x41,0xf9,0xb6,0x28,0x59,0x3d,0x0d};
    const unsigned char ordinary[8] = {1,2,3,4,5,6,7,8};
    unsigned char changed[8];
    unsigned i;
    assert(uss_classify_support(silo,2)==USS_UTILITY_SUPPORT);
    assert(uss_classify_support(c4,2)==USS_UTILITY_SUPPORT);
    assert(uss_classify_support(ordinary,2)==USS_ORDINARY_SUPPORT);
    for(i=0;i<8;++i) {
        memcpy(changed,silo,8); changed[i]^=1;
        assert(uss_classify_support(changed,2)==USS_ORDINARY_SUPPORT);
    }
    for(i=0;i<8;++i) changed[i]=silo[7-i];
    assert(uss_classify_support(changed,2)==USS_ORDINARY_SUPPORT);
    for(i=0;i<8;++i) changed[i]=c4[7-i];
    assert(uss_classify_support(changed,2)==USS_ORDINARY_SUPPORT);
    assert(uss_classify_support(shredder,2)==USS_UTILITY_SUPPORT);
    assert(uss_classify_support(shredder_printed_order,2)==USS_ORDINARY_SUPPORT);
    assert(uss_classify_support(shredder,0)==USS_NOT_SUPPORT);
    assert(uss_classify_support(shredder,1)==USS_NOT_SUPPORT);
    for(i=0;i<8;++i) {
        memcpy(changed,shredder,8); changed[i]^=1;
        assert(uss_classify_support(changed,2)==USS_ORDINARY_SUPPORT);
    }
    assert(uss_classify_support(NULL,2)==USS_NOT_SUPPORT);
    assert(uss_classify_support(silo,0)==USS_NOT_SUPPORT);
    assert(uss_classify_support(silo,1)==USS_NOT_SUPPORT);
    assert(uss_classify_support(c4,3)==USS_NOT_SUPPORT);
    assert(uss_classify_support(ordinary,UINT32_MAX)==USS_NOT_SUPPORT);
    puts("support policy: Solo Silo, C4 and Shredder remote exact byte identity, support admission, unrelated and reversed types PASS");
    return 0;
}
