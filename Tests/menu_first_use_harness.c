#include <assert.h>
#include <stdio.h>
#include "../src/uss_menu_first_use.h"
int main(void){
 UssMenuFirstUse s={0};
 assert(!uss_menu_first_use_step(&s,0,1,0,0,0));
 assert(uss_menu_first_use_step(&s,10,1,0,1,1)==1&&!s.taken);
 assert(uss_menu_first_use_step(&s,20,1,0,0,1)==0);
 assert(uss_menu_first_use_step(&s,200,1,1,0,1)==1&&s.taken);
 assert(uss_menu_first_use_step(&s,210,1,0,0,1)==0);
 assert(uss_menu_first_use_step(&s,220,1,0,1,1)==0);
 assert(uss_menu_first_use_step(&s,400,0,0,1,1)==1);
 assert(uss_menu_first_use_step(&s,410,1,1,0,0)==1); /* taken source: gameplay release latch remains authoritative */
 s=(UssMenuFirstUse){0};
 assert(!uss_menu_first_use_step(&s,0,1,1,0,0)&&!s.taken);
 assert(!uss_menu_first_use_step(&s,10,1,1,0,1)&&!s.taken);
 assert(!uss_menu_first_use_step(&s,20,1,0,0,1));
 assert(uss_menu_first_use_step(&s,30,1,1,0,1)&&s.taken);
 s=(UssMenuFirstUse){0};uss_menu_first_use_step(&s,0,1,0,0,1);
 assert(uss_menu_first_use_step(&s,10,1,0,1,1));
 assert(!uss_menu_first_use_step(&s,20,1,1,0,1)&&s.taken);
 assert(!uss_menu_first_use_step(&s,21,1,1,0,1));
 assert(!uss_menu_first_use_step(&s,30,1,0,0,1));
 assert(uss_menu_first_use_step(&s,200,1,1,0,1));
 s=(UssMenuFirstUse){0};uss_menu_first_use_step(&s,0,1,0,0,1);
 assert(uss_menu_first_use_step(&s,10,1,1,0,1));
 assert(!uss_menu_first_use_step(&s,20,0,0,1,1));
 assert(!uss_menu_first_use_step(&s,21,0,0,1,1));
 assert(!uss_menu_first_use_step(&s,30,0,0,0,1));
 assert(uss_menu_first_use_step(&s,200,0,0,1,1));
 puts("MENU FIRST USE: PASS fallback6, first-use takeover, held-start, release admission, loss and duplicate-source merge");return 0;
}
