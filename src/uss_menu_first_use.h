/* Native menu APIs expose action state, not assignment count. Preserve the
 * legacy session-local first-use rule without changing provider mappings. */
#ifndef USS_MENU_FIRST_USE_H
#define USS_MENU_FIRST_USE_H
#include <stdint.h>
typedef struct UssMenuFirstUse {
 unsigned taken,menu_known,menu_down,local_known,local_down,suppress_menu,suppress_local;
 unsigned local_edge_known,menu_edge_known;uint64_t local_edge_ms,menu_edge_ms;
} UssMenuFirstUse;
/* Reset edge evidence across an unavailable source; the session's choice stays. */
static inline void uss_menu_first_use_reset(UssMenuFirstUse* s)
{
 if(!s)return;s->menu_known=s->local_known=s->suppress_menu=s->suppress_local=0;
 s->local_edge_known=s->menu_edge_known=0;
}
static inline unsigned uss_menu_first_use_step(UssMenuFirstUse* s,uint64_t now,
 unsigned live,unsigned menu_down,unsigned local_down,unsigned may_take)
{
 unsigned menu_edge,local_edge;
 if(!s || live>1 || menu_down>1 || local_down>1 || may_take>1)return 0;
 if(!live){s->menu_known=0;s->suppress_menu=0;}
 menu_edge=live && s->menu_known && !s->menu_down && menu_down;
 local_edge=s->local_known && !s->local_down && local_down;
 s->menu_known=live;s->menu_down=menu_down;s->local_known=1;s->local_down=local_down;
 if(!menu_down)s->suppress_menu=0;
 if(!local_down)s->suppress_local=0;
 if(local_edge && (!s->taken || !live)){s->local_edge_known=1;s->local_edge_ms=now;}
 if(menu_edge && (s->taken || may_take)){
  s->taken=1;
  /* Same physical key may reach the two sources one frame apart. */
  if(s->local_edge_known && now>=s->local_edge_ms && now-s->local_edge_ms<=150)s->suppress_menu=1;
  else{s->menu_edge_known=1;s->menu_edge_ms=now;}
 }
 if(s->taken && live)return s->suppress_menu?0:menu_down;
 if(local_edge && s->menu_edge_known && now>=s->menu_edge_ms && now-s->menu_edge_ms<=150)s->suppress_local=1;
 return s->suppress_local?0:local_down;
}
#endif
