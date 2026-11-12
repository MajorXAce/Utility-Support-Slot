#include <assert.h>
#include <stdio.h>
#include "../src/uss_binding_reservations.h"
static UssBindingReservations book;
static int parse(const char* text){return uss_binding_book_parse(text,strlen(text),&book);}
int main(void)
{
    UssBindingReservations before;uint32_t auto_code=10*65536u;size_t i;
    const char* valid="format 2\r\nsession 90\r\nOther.Mod\t10\t0\t1\r\nUSS.Utility\t10\t2\t89\r\nslot 2 90 Fixed.Owner\r\naction 10:3 cleared 5 found abcdef01 left 00000000\r\naction 10:5 clear\r\nend\r\n";
    const char* invalid[]={"format 3\nsession 1\nend\n","format 2\nsession 1\n","format 2\nend\n",
        "format 2\nsession 1\nend\nextra\n","format 2\nsession 1\nsession 2\nend\n",
        "format 2\nsession 1\nA\t10\t0\t1\nB\t10\t0\t2\nend\n",
        "format 2\nsession 1\nA\t10\t0\t1\nA\t10\t2\t2\nend\n",
        "format 2\nsession 1\naction 10:3 cleared 1 found 100000000 left 0\nend\n",
        "format 2\nsession 4294967296\nend\n","A\t13\t0\n","A\t10\t97\n",
        "format 2\nsession 1\nslot 1 1 Bad\nend\n","garbage\n",""};
    memset(&book,0,sizeof(book));assert(uss_binding_reservation_query(&book,auto_code,"USS.Utility")==USS_BIND_RESERVATION_UNKNOWN);
    assert(parse(valid) && book.format==2 && book.session==90 && book.count==2 && book.complete && book.shared_slot);
    assert(uss_binding_reservation_query(&book,auto_code,"USS.Utility")==USS_BIND_RESERVATION_FOREIGN); /* Never expire absent owner. */
    assert(uss_binding_reservation_query(&book,10*65536u+2,"USS.Utility")==USS_BIND_RESERVATION_SELF);
    assert(uss_binding_reservation_query(&book,10*65536u+3,"USS.Utility")==USS_BIND_RESERVATION_CLEANUP);
    assert(uss_binding_reservation_query(&book,10*65536u+6,"USS.Utility")==USS_BIND_RESERVATION_CANDIDATE);
    assert(uss_binding_reservation_query(&book,12*65536u+1,"USS.Utility")==USS_BIND_RESERVATION_FIXED);
    before=book;
    for(i=0;i<sizeof(invalid)/sizeof(invalid[0]);++i){book=before;assert(!parse(invalid[i]) && !memcmp(&book,&before,sizeof(book)));}
    for(i=0;i<strlen(valid);++i){book=before; /* Truncation before end never yields a known format2book. */
        if(i<strlen(valid)-5)assert(!uss_binding_book_parse(valid,i,&book) && !memcmp(&book,&before,sizeof(book)));}
    assert(parse("Legacy.Mod\t10\t0\n") && book.format==0 && book.count==1);
    assert(uss_binding_reservation_query(&book,auto_code,"USS.Utility")==USS_BIND_RESERVATION_FOREIGN);
    assert(parse("format 2\nsession 0\nend\n") && !book.count);
    assert(uss_binding_reservation_query(&book,auto_code,"USS.Utility")==USS_BIND_RESERVATION_CANDIDATE);
    puts("USS binding reservations: original legacy/format2records, persistent foreign ownership, fixed slots, cleanup notes, truncation and ambiguous/future refusal PASS");return 0;
}
