#include <assert.h>
#include "../src/uss_menu_handover.h"
int main(void)
{
    UssMenuHandover h;UssMenuHandoverView v;UssSettings local=uss_settings_defaults();
    UssMenuProviderSample p={USS_MENU_HANDOVER_PROTOCOL,sizeof(p),9,1,1,1,1,USS_BIND_DEDICATED,1,1,0};
    uss_menu_handover_init(&h);
    v=uss_menu_handover_step(&h,NULL,0,0,0,&local,0);assert(!v.external && !v.utility_down);
    p.bindings_registered=0;v=uss_menu_handover_step(&h,&p,1,0,0,&local,0);assert(!v.external);
    p.bindings_registered=1;v=uss_menu_handover_step(&h,&p,2,1,0,&local,0);assert(!v.external);
    v=uss_menu_handover_step(&h,NULL,3,0,0,&local,0);
    assert(v.external && v.mode==USS_BIND_DEDICATED && !v.utility_down && v.release_required);
    p.sequence++;p.utility_down=0;v=uss_menu_handover_step(&h,&p,4,0,0,&local,0);assert(!v.utility_down);
    p.sequence++;p.utility_down=1;v=uss_menu_handover_step(&h,&p,5,0,0,&local,0);assert(v.utility_down);
    /* Existing provider input remains usable for the accepted pending-swap path. */
    v=uss_menu_handover_step(&h,NULL,5,1,0,&local,0);assert(v.utility_down);
    /* Applied mode stays stable for an active weapon transaction. */
    p.sequence++;p.mode=USS_BIND_DEFAULT;
    v=uss_menu_handover_step(&h,&p,5,1,0,&local,0);assert(v.mode==USS_BIND_DEDICATED);
    v=uss_menu_handover_step(&h,NULL,5,0,0,&local,0);assert(v.mode==USS_BIND_DEFAULT && !v.utility_down);
    /* Blocked input needs an observed release before accepting another press. */
    v=uss_menu_handover_step(&h,NULL,6,0,1,&local,0);assert(!v.utility_down);
    v=uss_menu_handover_step(&h,NULL,7,0,0,&local,0);assert(!v.utility_down);
    p.sequence++;p.utility_down=0;uss_menu_handover_step(&h,&p,8,0,0,&local,0);
    p.sequence++;p.utility_down=1;v=uss_menu_handover_step(&h,&p,9,0,0,&local,0);assert(v.utility_down);
    /* Replaying a cached pressed sample cannot keep a dead provider alive. */
    v=uss_menu_handover_step(&h,&p,1010,1,0,&local,1);assert(v.external && !v.utility_down);
    v=uss_menu_handover_step(&h,NULL,1011,0,0,&local,1);assert(!v.external && !v.utility_down);
    uss_menu_handover_step(&h,NULL,1012,0,0,&local,0);
    v=uss_menu_handover_step(&h,NULL,1013,0,0,&local,1);assert(v.utility_down);
    /* A fresh provider generation can restart at sequence one. */
    p.provider++;p.sequence=1;p.utility_down=1;
    v=uss_menu_handover_step(&h,&p,1014,0,0,&local,0);assert(v.external && !v.utility_down);
    p.sequence++;p.utility_down=0;uss_menu_handover_step(&h,&p,1015,0,0,&local,0);
    p.sequence++;p.utility_down=1;v=uss_menu_handover_step(&h,&p,1016,0,0,&local,0);assert(v.utility_down);
    /* ABI mismatch and receiver clock reversal revoke input immediately. */
    p.size--;v=uss_menu_handover_step(&h,&p,1017,0,0,&local,1);assert(!v.external && !v.utility_down);
    p.size++;p.sequence++;uss_menu_handover_step(&h,&p,1018,0,0,&local,0);
    v=uss_menu_handover_step(&h,NULL,1000,0,0,&local,1);assert(!v.external && !v.utility_down);
    assert(local.mode==USS_BIND_DEFAULT && local.utility_key==0x36);
    puts("Menu handover: partial providers, idle handover, semantic input, stale lease, fallback and release PASS");
    return 0;
}
