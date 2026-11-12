/* Read-only original ModBindingsMenu assignment-book admission, 0BSD reference.
 * No expiry, reservation updates, mapping cleanup or native action ownership.
 * Preserve all reservations, even for an absent mod. Unknown never means free. */
#ifndef USS_BINDING_RESERVATIONS_H
#define USS_BINDING_RESERVATIONS_H
#include <stdint.h>
#include <string.h>
#include <stddef.h>
enum {USS_BIND_BOOK_BYTES=65536,USS_BIND_BOOK_LINES=512,USS_BIND_BOOK_ENTRIES=128,USS_BIND_ID_BYTES=128};
enum {USS_BIND_RESERVATION_UNKNOWN=0,USS_BIND_RESERVATION_CANDIDATE,USS_BIND_RESERVATION_SELF,
    USS_BIND_RESERVATION_FOREIGN,USS_BIND_RESERVATION_FIXED,USS_BIND_RESERVATION_CLEANUP};
static const uint32_t uss_binding_dormant_codes[36]={12u*65536u+1u,12u*65536u+0u,10u*65536u+1u,10u*65536u+4u,10u*65536u+8u,10u*65536u+14u,10u*65536u+9u,10u*65536u+0u,10u*65536u+2u,10u*65536u+3u,10u*65536u+5u,10u*65536u+6u,10u*65536u+7u,10u*65536u+10u,10u*65536u+11u,10u*65536u+12u,10u*65536u+13u,10u*65536u+15u,10u*65536u+16u,10u*65536u+17u,11u*65536u+0u,11u*65536u+1u,11u*65536u+2u,11u*65536u+3u,9u*65536u+0u,9u*65536u+1u,9u*65536u+2u,9u*65536u+3u,9u*65536u+4u,9u*65536u+5u,9u*65536u+6u,9u*65536u+7u,9u*65536u+8u,9u*65536u+9u,9u*65536u+10u,9u*65536u+11u};
static const uint32_t uss_binding_dormant_labels[36]={
    0x62ea38bfu,0x82fbdcb2u,0x6218a8bau,0xd46660e4u,0xc60e3a71u,0x45c3ce49u,
    0x54041145u,0x4a681770u,0x03f7407eu,0x533ad91fu,0xc9d1babcu,0xeb8a3b7au,
    0x5daaa6c3u,0xf0a84860u,0xaac2b10eu,0xf884272du,0x6a566af0u,0x3a0b904cu,
    0x484bb4c1u,0x5556ae64u,0x2bf059b1u,0x5f892216u,0x0bf38a13u,0x5b786e6bu,
    0x5d7d7666u,0x020ae5fcu,0x04ee429eu,0x71d71525u,0x026199fau,0x62407e28u,
    0x2a05009fu,0x6479f954u,0x6306672au,0x8e53ce97u,0x6e43f1c5u,0xae71f7a9u
};
typedef struct UssBindingReservation {char id[USS_BIND_ID_BYTES];uint32_t code,seen;} UssBindingReservation;
typedef struct UssBindingReservations {
    unsigned valid,format,count,complete,shared_slot;
    uint32_t session;
    uint64_t action_notes; /* Existing cleanup records require original-owner handling. */
    UssBindingReservation entries[USS_BIND_BOOK_ENTRIES];
} UssBindingReservations;
static inline int uss_binding_dormant_index(uint32_t code)
{unsigned i;for(i=0;i<36;++i)if(uss_binding_dormant_codes[i]==code)return (int)i;return -1;}
static inline int uss_binding_book_number(const char* text,size_t length,uint32_t* output,unsigned base)
{
    size_t i;uint32_t value=0;
    if(!length || length>10 || (base!=10 && base!=16))return 0;
    for(i=0;i<length;++i){unsigned char c=(unsigned char)text[i];unsigned digit;
        if(c>='0' && c<='9')digit=c-'0';else if(base==16 && c>='a' && c<='f')digit=c-'a'+10;
        else if(base==16 && c>='A' && c<='F')digit=c-'A'+10;else return 0;
        if(digit>=base || value>(UINT32_MAX-digit)/base)return 0;value=value*base+digit;
    }
    *output=value;return 1;
}
static inline int uss_binding_book_id(const char* text,size_t length)
{
    size_t i;if(!length || length>=USS_BIND_ID_BYTES)return 0;
    for(i=0;i<length;++i)if((unsigned char)text[i]<32 || (unsigned char)text[i]==127)return 0;return 1;
}
static inline int uss_binding_book_parse(const char* text,size_t length,UssBindingReservations* output)
{
    UssBindingReservations parsed={0};size_t at=0;unsigned lines=0,format_seen=0,session_seen=0;
    if(!text || !output || !length || length>USS_BIND_BOOK_BYTES)return 0;
    while(at<length){
        size_t begin=at,n,i;const char* line;uint32_t group,action,value;unsigned tabs=0;size_t tab[3]={0};
        while(at<length && text[at]!='\n' && text[at]!='\r')++at;n=at-begin;line=text+begin;
        if(at<length && text[at]=='\r'){++at;if(at<length && text[at]=='\n')++at;}else if(at<length)++at;
        if(!n)continue;if(++lines>USS_BIND_BOOK_LINES || n>384 || parsed.complete)return 0;
        for(i=0;i<n;++i){if(line[i]=='\t'){if(tabs==3)return 0;tab[tabs++]=i;}else if((unsigned char)line[i]<32)return 0;}
        if(tabs){
            UssBindingReservation entry={0};unsigned j;
            if((tabs!=2 && tabs!=3) || parsed.count==USS_BIND_BOOK_ENTRIES || !uss_binding_book_id(line,tab[0]) ||
                !uss_binding_book_number(line+tab[0]+1,tab[1]-tab[0]-1,&group,10) || group>=13 ||
                !uss_binding_book_number(line+tab[1]+1,(tabs==3?tab[2]:n)-tab[1]-1,&action,10) || action>=97)return 0;
            entry.code=group*65536+action;
            if(tabs==3 && tab[2]+1<n && !uss_binding_book_number(line+tab[2]+1,n-tab[2]-1,&entry.seen,10))return 0;
            memcpy(entry.id,line,tab[0]);
            for(j=0;j<parsed.count;++j)if(parsed.entries[j].code==entry.code || !strcmp(parsed.entries[j].id,entry.id))return 0;
            parsed.entries[parsed.count++]=entry;continue;
        }
        if(n==3 && !memcmp(line,"end",3)){parsed.complete=1;continue;}
        if(n>7 && !memcmp(line,"format ",7)){
            if(format_seen++ || !uss_binding_book_number(line+7,n-7,&value,10) || value!=2)return 0;
            parsed.format=2;continue;
        }
        if(n>8 && !memcmp(line,"session ",8)){
            if(session_seen++ || !uss_binding_book_number(line+8,n-8,&parsed.session,10))return 0;continue;
        }
        if(n>7 && !memcmp(line,"action ",7)){
            size_t colon=7,space;int index;
            while(colon<n && line[colon]!=':')++colon;space=colon+1;
            while(space<n && line[space]!=' ')++space;
            if(colon>=n || space>=n || !uss_binding_book_number(line+7,colon-7,&group,10) || group>=13 ||
                !uss_binding_book_number(line+colon+1,space-colon-1,&action,10) || action>=97)return 0;
            index=uss_binding_dormant_index(group*65536+action);if(index<0 || (parsed.action_notes&(UINT64_C(1)<<index)))return 0;
            ++space;
            if(n-space==5 && !memcmp(line+space,"clear",5)){}
            else{
                size_t number,found,left;
                if(n-space<8 || memcmp(line+space,"cleared ",8))return 0;number=space+8;found=number;
                while(found<n && line[found]!=' ')++found;
                if(found>=n || !uss_binding_book_number(line+number,found-number,&value,10) || n-found<7 || memcmp(line+found," found ",7))return 0;
                number=found+7;left=number;while(left<n && line[left]!=' ')++left;
                if(left>=n || !uss_binding_book_number(line+number,left-number,&value,16) || n-left<6 || memcmp(line+left," left ",6) ||
                    !uss_binding_book_number(line+left+6,n-left-6,&value,16))return 0;
            }
            parsed.action_notes|=UINT64_C(1)<<index;continue;
        }
        if(n>5 && !memcmp(line,"slot ",5)){
            size_t space=5,last;
            while(space<n && line[space]!=' ')++space;last=space+1;
            while(last<n && line[last]!=' ')++last;
            if(space>=n || last>=n || parsed.shared_slot || !uss_binding_book_number(line+5,space-5,&value,10) || value!=2 ||
                !uss_binding_book_number(line+space+1,last-space-1,&value,10) || !uss_binding_book_id(line+last+1,n-last-1))return 0;
            parsed.shared_slot=1;continue;
        }
        return 0;
    }
    if(parsed.format && (!parsed.complete || !session_seen))return 0;
    if(!parsed.format && (format_seen || session_seen || parsed.complete || parsed.action_notes || parsed.shared_slot || !parsed.count))return 0;
    parsed.valid=1;*output=parsed;return 1;
}
/* CANDIDATE means only no conflicting reservation in this KNOWN text snapshot.
 * It does not prove the file is current, action idle, defaults safe or slot owned. */
static inline unsigned uss_binding_reservation_query(const UssBindingReservations* book,uint32_t code,const char* id)
{
    int index;unsigned i;
    if(!book || !book->valid || !id || !*id || (index=uss_binding_dormant_index(code))<0)return USS_BIND_RESERVATION_UNKNOWN;
    if(index<7)return USS_BIND_RESERVATION_FIXED;
    for(i=0;i<book->count;++i)if(book->entries[i].code==code)
        return strcmp(book->entries[i].id,id)?USS_BIND_RESERVATION_FOREIGN:USS_BIND_RESERVATION_SELF;
    if(book->action_notes&(UINT64_C(1)<<index))return USS_BIND_RESERVATION_CLEANUP;
    return USS_BIND_RESERVATION_CANDIDATE;
}
#endif
