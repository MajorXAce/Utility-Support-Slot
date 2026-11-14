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

/* Support weapon "types" that USS keys by identity. A weapon can carry more than one. Everything else is an ordinary
 * support weapon. Identities are the 8-byte weapon resource id in memory byte order, exactly as Lua unhex()/string
 * equality; they are weapon identity bytes, not stratagem ids or entity instances. */
#define USS_SUPPORT_TYPE_UTILITY 1u    /* Solo Silo, C4: paired with an ordinary support weapon */
#define USS_SUPPORT_TYPE_EXPENDABLE 2u /* single-use weapons: never refilled by the resupply logic */
typedef struct UssKnownSupport {
    unsigned char identity[8];
    unsigned types;
} UssKnownSupport;
static const UssKnownSupport uss_known_support[] = {
    {{0xaa,0x79,0xca,0x92,0x05,0x46,0x13,0xfc}, USS_SUPPORT_TYPE_UTILITY},    /* Solo Silo */
    {{0x3d,0x2f,0xf5,0x21,0x63,0x0d,0xf5,0x51}, USS_SUPPORT_TYPE_UTILITY},    /* C4 */
    /* Compendium Shredder remote (skyeshade/compendium/shredder_remote), printed id a541f9b628593d0d, a copy of the
     * Solo Silo weapon. A fixed fallback only: the lineage scan below also finds it, and any other clone. */
    {{0x0d,0x3d,0x59,0x28,0xb6,0xf9,0x41,0xa5}, USS_SUPPORT_TYPE_UTILITY},
    {{0xd3,0x01,0x69,0xed,0xa0,0x2f,0x93,0x80}, USS_SUPPORT_TYPE_EXPENDABLE}, /* EAT-17 Expendable Anti-Tank */
    {{0xc7,0x38,0xac,0x65,0x27,0x64,0x17,0x76}, USS_SUPPORT_TYPE_EXPENDABLE}, /* EAT-411 Leveller */
    {{0x9e,0x5f,0x60,0x85,0xd1,0xe0,0xb5,0xb2}, USS_SUPPORT_TYPE_EXPENDABLE}, /* EAT-700 Expendable Napalm */
    {{0xcb,0x16,0x2b,0x14,0x3d,0x12,0x90,0x59}, USS_SUPPORT_TYPE_EXPENDABLE}, /* MLS-4X Commando */
    {{0x77,0x9b,0xa5,0x0a,0x49,0x9d,0x6c,0xb1}, USS_SUPPORT_TYPE_EXPENDABLE}, /* MGX-42 Bullet Storm */
    {{0xfc,0x31,0xaa,0x4a,0x1a,0xe8,0x54,0x5c}, USS_SUPPORT_TYPE_EXPENDABLE}  /* not in the session arsenal list */
};
#define USS_KNOWN_SUPPORT_COUNT (sizeof(uss_known_support)/sizeof(uss_known_support[0]))

/* Mod-added clones of a known type, found at run time (utility_lineage.h). Filled once per session from the SDK's
 * content list: an entity whose recorded base is a known weapon inherits that weapon's types. Not a name list, so a
 * later mod that clones a Silo, C4 or an expendable weapon is handled without a code change. */
#define USS_DERIVED_SUPPORT_CAPACITY 32
typedef struct UssDerivedSupport {
    unsigned char identity[8];
    unsigned types;
} UssDerivedSupport;
static UssDerivedSupport uss_derived_support[USS_DERIVED_SUPPORT_CAPACITY];
static unsigned uss_derived_support_count;
static inline void uss_derived_support_reset(void)
{
    memset(uss_derived_support,0,sizeof uss_derived_support);
    uss_derived_support_count=0;
}
/* 1 when the identity now carries `types` (added or merged), 0 for an empty identity/types or a full table. */
static inline int uss_derived_support_add(const unsigned char identity[8],unsigned types)
{
    static const unsigned char zero[8]={0};
    unsigned i;
    if(!identity || !types || !memcmp(identity,zero,8))return 0;
    for(i=0;i<uss_derived_support_count;++i)
        if(!memcmp(uss_derived_support[i].identity,identity,8)){uss_derived_support[i].types|=types;return 1;}
    if(uss_derived_support_count>=USS_DERIVED_SUPPORT_CAPACITY)return 0;
    memcpy(uss_derived_support[uss_derived_support_count].identity,identity,8);
    uss_derived_support[uss_derived_support_count++].types=types;
    return 1;
}
/* The types of a built-in weapon whose resource id is `base` (the u64 the SDK reports as an entity's base), else 0. */
static inline unsigned uss_known_base_types(uint64_t base)
{
    unsigned i,types=0;uint64_t known;
    for(i=0;i<USS_KNOWN_SUPPORT_COUNT;++i){memcpy(&known,uss_known_support[i].identity,8);if(known==base)types|=uss_known_support[i].types;}
    return types;
}
/* Every type a weapon identity carries: built-in table plus run-time clones. 0 means an ordinary support weapon. */
static inline unsigned uss_support_types(const unsigned char identity[8])
{
    unsigned i,types=0;
    if(!identity)return 0;
    for(i=0;i<USS_KNOWN_SUPPORT_COUNT;++i)if(!memcmp(identity,uss_known_support[i].identity,8))types|=uss_known_support[i].types;
    for(i=0;i<uss_derived_support_count;++i)if(!memcmp(identity,uss_derived_support[i].identity,8))types|=uss_derived_support[i].types;
    return types;
}
static inline int uss_is_expendable_support(const unsigned char identity[8])
{
    return (uss_support_types(identity)&USS_SUPPORT_TYPE_EXPENDABLE)!=0;
}

static UssSupportClass uss_classify_support(const unsigned char identity[8],
                                           uint32_t observed_kind)
{
    /* Kind2 is Lua's support-candidate predicate, not a universal weapon rule.
     * Primary/secondary/backpack records remain readable by the generic reader. */
    if (!identity || observed_kind != 2) return USS_NOT_SUPPORT;
    return (uss_support_types(identity)&USS_SUPPORT_TYPE_UTILITY)!=0 ? USS_UTILITY_SUPPORT : USS_ORDINARY_SUPPORT;
}
#endif
