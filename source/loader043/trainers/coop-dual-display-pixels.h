/* Experimental dual-display pixel routing. Not used by loader040.
 * Reuses the attributed tiled-address equations in coop-compact-pixels.h.
 * Each camera retains its entire 16:9 frame. The Mac receives P2's own frame;
 * TV routing selects either full P1 or the existing central split framing.
 * GPU completion, publication, networking and mode transitions belong to the
 * caller; these pure pixel functions do not establish those contracts.
 */
#ifndef COOP_DUAL_DISPLAY_PIXELS_H
#define COOP_DUAL_DISPLAY_PIXELS_H
#include "coop-compact-pixels.h"
#define COOP_DUAL_WIDTH 1280u
#define COOP_DUAL_HEIGHT 720u
#define COOP_DUAL_VIEW_BYTES (1280u*720u*4u)
enum coop_dual_tv_mode { COOP_DUAL_TV_SPLIT=0, COOP_DUAL_TV_P1=1 };
static inline int coop_dual_sample_full(const uint32_t *source,uint32_t *destination){
 if(!source||!destination)return 0;
 for(uint32_t y=0;y<COOP_DUAL_HEIGHT;y++)for(uint32_t x=0;x<COOP_DUAL_WIDTH;x++)
  destination[(size_t)y*COOP_DUAL_WIDTH+x]=source[compact_word(x*3u,y*3u,3840)];
 return 1;
}
static inline int coop_dual_compose_tv(const uint32_t *p1,const uint32_t *p2,uint32_t *output,unsigned mode){
 if(!p1||!p2||!output||mode>COOP_DUAL_TV_P1)return 0;
 for(uint32_t y=0;y<1080;y++){
  const uint32_t *view=p1;uint32_t sy=y*2u/3u;
  if(mode==COOP_DUAL_TV_SPLIT){view=y<540?p2:p1;sy=180u+(y%540)*2u/3u;}
  for(uint32_t x=0;x<1920;x++)output[compact_word(x,y,1920)]=view[(size_t)sy*COOP_DUAL_WIDTH+x*2u/3u];
 }
 return 1;
}
#endif
