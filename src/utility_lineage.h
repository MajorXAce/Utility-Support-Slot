/* Run-time recognition of mod-added clones of known support weapon types (utility, expendable). Read-only: one pass
 * over the SDK's list of content mods added. */
#ifndef USS_UTILITY_LINEAGE_H
#define USS_UTILITY_LINEAGE_H
#include "sdk_compat.h"
#include "hellforge_content.h"
#include "weapon_policy.h"
#define USS_LINEAGE_CONTENT_CAP 2048u
static unsigned uss_lineage_done;
static inline void uss_lineage_reset(void){uss_lineage_done=0;uss_derived_support_reset();}
/* api->content lists what mods added (SDK 7, inside our required SDK11 prefix), complete once every mod has initialised.
 * Call it only once a mission is running. An ENTITY whose `base` is a known weapon (a Silo or C4 utility, or one of the
 * expendable support weapons) is a clone of it and inherits its types: for example Compendium's Shredder remote is
 * {id a541f9b628593d0d, base fc13460592ca79aa}, a utility, and its EAT-77 is a clone of the EAT-17, an expendable.
 * The id is stored in memory byte order, like the fixed table. One pass per session; a missing callback just leaves the
 * fixed table in charge. Returns how many identities this call added.
 * Limits: direct base only (the recorded base must itself be a built-in identity); no clone-of-clone chains and no
 * order independence; content added after this pass is not seen; unknown stays ordinary. Ids are copied as primitives at
 * once (the name pointer is used only for the log line); the content index is not a durable identity. */
static inline unsigned uss_support_lineage_scan_once(const HfApi* api,HfMod* mod)
{
    HfContent entry;unsigned index,seen=0,added=0,types;
    if(uss_lineage_done)return 0;
    uss_lineage_done=1;
    if(!uss_sdk_compatible(api) || !mod || !api->content)return 0;
    for(index=0;index<USS_LINEAGE_CONTENT_CAP;++index){
        memset(&entry,0,sizeof entry);entry.size=(uint32_t)sizeof entry;
        if(!api->content(mod,index,&entry))break;
        ++seen;
        types=uss_known_base_types(entry.base);
        if(entry.kind!=HF_KIND_ENTITY || !entry.id || !types)continue;
        {unsigned char identity[8];memcpy(identity,&entry.id,8);
         if(uss_derived_support_add(identity,types)){
            ++added;
            if(api->log)api->log(mod,"USS support clone recognised id=%016llx base=%016llx types=%u name=%s",
                (unsigned long long)entry.id,(unsigned long long)entry.base,types,entry.name?entry.name:"?");
         }}
    }
    if(api->log)api->log(mod,"USS support lineage scan: content=%u clones=%u",seen,uss_derived_support_count);
    return added;
}
#endif
