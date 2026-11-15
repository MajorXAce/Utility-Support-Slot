/* The 32 bytes at game.dll RVA 0x978AC0 that every pair-member reader compares before trusting member fields.
 * One definition shared by Core (core_observation.h), the member reader (pair_member.h) and the recovery reader
 * (uss_recovery_reader.h), so the three copies can no longer drift apart. The bytes, the read size and the places that
 * compare them are exactly what they were when each file carried its own copy.
 * Provenance: the retained prologue of the interaction setter from the Lua version. pair_member.h additionally checks that
 * the RIP-relative load at RVA 0x978AD4 resolves to the global at RVA 0x3326D68. The readers do not call or read from this
 * function; it is a build-identity pin. Unconsumed bytes are deliberately NOT relaxed here (audit F3 is a separate decision). */
#ifndef USS_MEMBER_PIN_H
#define USS_MEMBER_PIN_H
#include <string.h>
enum { USS_MEMBER_PIN_RVA=0x978AC0, USS_MEMBER_PIN_BYTES=32 };
static const unsigned char uss_member_pin[USS_MEMBER_PIN_BYTES]={
    0x40,0x53,0x41,0x54,0x41,0x57,0x48,0x83,0xec,0x20,0x3b,0x15,0x50,0xb1,0xb0,0x02,
    0x45,0x0f,0xb6,0xf9,0x48,0x8b,0x1d,0x8d,0xe2,0x9a,0x02,0x45,0x8b,0xe0,0x0f,0x84};
static inline int uss_member_pin_matches(const unsigned char* bytes)
{return bytes && !memcmp(bytes,uss_member_pin,USS_MEMBER_PIN_BYTES);}
#endif
