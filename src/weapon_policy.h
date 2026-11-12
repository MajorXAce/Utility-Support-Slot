/* Pure policy ported from Lua UTILITY_WEAPONS/is_utility and identity().
 * Caller supplies identity bytes from an already validated descriptor.
 * This classifies a support candidate; it does not validate a live entity. */
#ifndef USS_WEAPON_POLICY_H
#define USS_WEAPON_POLICY_H
#include <stdint.h>
#include <string.h>

typedef enum UssSupportClass {
    USS_NOT_SUPPORT = 0,
    USS_UTILITY_SUPPORT,
    USS_ORDINARY_SUPPORT
} UssSupportClass;

static UssSupportClass uss_classify_support(const unsigned char identity[8],
                                           uint32_t observed_kind)
{
    /* Preserve memory byte order, exactly as Lua unhex()/string equality.
     * These are weapon identity bytes, not stratagem ids or entity instances. */
    static const unsigned char utilities[][8] = {
        {0xaa,0x79,0xca,0x92,0x05,0x46,0x13,0xfc}, /* Solo Silo */
        {0x3d,0x2f,0xf5,0x21,0x63,0x0d,0xf5,0x51}  /* C4 */
    };
    unsigned i;
    /* Kind2 is Lua's support-candidate predicate, not a universal weapon rule.
     * Primary/secondary/backpack records remain readable by the generic reader. */
    if (!identity || observed_kind != 2) return USS_NOT_SUPPORT;
    for (i = 0; i < sizeof(utilities)/sizeof(utilities[0]); ++i)
        if (!memcmp(identity, utilities[i], 8)) return USS_UTILITY_SUPPORT;
    return USS_ORDINARY_SUPPORT;
}
#endif
