/* Tiled-address equations adapted from John Tornblom's zlib-licensed
 * SDL_ps5tilemap.c. See the full attribution/license in coop-split-compositor.c.
 * This altered implementation downsamples 4K central camera regions to linear
 * 1920x540 views, then packs two such views into a tiled 1920x1080 surface. */
#ifndef COOP_COMPACT_PIXELS_H
#define COOP_COMPACT_PIXELS_H
#include <stdint.h>
#include <stddef.h>
#ifdef COOP_COMPACT_720
#include "coop-compact-columns.h"
#endif
#ifdef COOP_COMPACT_720
#define COOP_COMPACT_WIDTH 1280u
#define COOP_COMPACT_VIEW_HEIGHT 360u
#define COOP_COMPACT_STEP 3u
#else
#define COOP_COMPACT_WIDTH 1920u
#define COOP_COMPACT_VIEW_HEIGHT 540u
#define COOP_COMPACT_STEP 2u
#endif
#define COOP_COMPACT_HEIGHT (COOP_COMPACT_VIEW_HEIGHT*2u)
#define COOP_COMPACT_VIEW_BYTES (COOP_COMPACT_WIDTH*COOP_COMPACT_VIEW_HEIGHT*4u)
/* The running game's VideoOut accepts 1080p, but rejected 1280x720 with
 * 0x80290005. Lower-detail snapshots are expanded into a validated carrier. */
#define COOP_COMPACT_OUTPUT_WIDTH 1920u
#define COOP_COMPACT_OUTPUT_HEIGHT 1080u
#define COOP_COMPACT_OUTPUT_BYTES 8912896u
static inline uint32_t compact_x(uint32_t x){return (x>>9)*65536u+(x&3u)+((x&28u)<<3)+((x&64u)<<2)+((x&32u)<<4)+((x&16u)<<7)+((x&448u)<<7);}
static inline uint32_t compact_y(uint32_t y){return ((y&7u)<<2)|((y&56u)<<3)|((y&64u)<<3)|((y&8u)<<7)|((y&64u)<<6);}
static inline size_t compact_word(uint32_t x,uint32_t y,uint32_t width){return (size_t)(y>>7)*(128u*width)+(compact_x(x)^compact_y(y));}
static inline void compact_sample_4k(const uint32_t *source,uint32_t *destination){
#ifdef COOP_COMPACT_720
 for(uint32_t y=0;y<COOP_COMPACT_VIEW_HEIGHT;y++){
  uint32_t sy=540+y*3u,base=(sy>>7)*(128u*3840u),yo=compact_y(sy);
  for(uint32_t x=0;x<COOP_COMPACT_WIDTH;x++)destination[(size_t)y*COOP_COMPACT_WIDTH+x]=source[base+(compact_columns_720[x]^yo)];
 }
#else
 for(uint32_t y=0;y<540;y++){
  uint32_t sy=540+y*2u,base=(sy>>7)*(128u*3840u),yo=compact_y(sy);
  for(uint32_t group=0;group<960;group++){
   const uint32_t *p=source+base+(compact_x(group*4u)^yo);
   uint64_t first=*(const uint64_t*)(const void*)p,second=*(const uint64_t*)(const void*)(p+2);
   *(uint64_t*)(void*)(destination+(size_t)y*1920u+group*2u)=(first&0xffffffffu)|(second<<32);
  }
 }
#endif
}
/* Copy the native radar only from an identified P1 frame. Coordinates match
 * the observed 4K default-safe-zone HUD outside the central camera crop. */
#ifdef COOP_COMPACT_P1_MINIMAP
#define COOP_MAP_SOURCE_X 176u
#define COOP_MAP_SOURCE_Y 1648u
#define COOP_MAP_SOURCE_WIDTH 576u
#define COOP_MAP_SOURCE_HEIGHT 408u
#define COOP_MAP_WIDTH (COOP_MAP_SOURCE_WIDTH/COOP_COMPACT_STEP)
#define COOP_MAP_HEIGHT (COOP_MAP_SOURCE_HEIGHT/COOP_COMPACT_STEP)
#define COOP_MAP_LEFT 12u
#define COOP_MAP_TOP (COOP_COMPACT_VIEW_HEIGHT-COOP_MAP_HEIGHT-12u)
static __attribute__((noinline)) void compact_sample_p1_minimap(const uint32_t *source,uint32_t *destination){
 for(uint32_t y=0;y<COOP_MAP_HEIGHT;y++){
  uint32_t sy=COOP_MAP_SOURCE_Y+y*COOP_COMPACT_STEP;
  for(uint32_t x=0;x<COOP_MAP_WIDTH;x++){
   uint32_t sx=COOP_MAP_SOURCE_X+x*COOP_COMPACT_STEP;
   destination[(size_t)(COOP_MAP_TOP+y)*COOP_COMPACT_WIDTH+COOP_MAP_LEFT+x]=source[compact_word(sx,sy,3840)];
  }
 }
}
#endif
static inline void compact_compose(const uint32_t *first,const uint32_t *second,uint32_t *destination){
#ifdef COOP_COMPACT_P2_TOP
 const uint32_t *views[2]={second,first};
#else
 const uint32_t *views[2]={first,second};
#endif
#ifdef COOP_COMPACT_720
 for(uint32_t player=0;player<2;player++)for(uint32_t row=0;row<540;row+=4){
  uint32_t y=row+player*540u,base=(y>>7)*(128u*1920u),yo=compact_y(y);
  for(uint32_t group=0;group<480;group++){
   uint32_t x=group*4u,*to=destination+base+(compact_x(x)^yo);
   for(uint32_t k=0;k<4;k++){
    const uint32_t *from=views[player]+(size_t)((row+k)*2u/3u)*1280u;
    uint64_t *target=(uint64_t*)(void*)(to+k*4u);
    target[0]=(uint64_t)from[x*2u/3u]|((uint64_t)from[(x+1u)*2u/3u]<<32);
    target[1]=(uint64_t)from[(x+2u)*2u/3u]|((uint64_t)from[(x+3u)*2u/3u]<<32);
   }
  }
 }
#else
 for(uint32_t player=0;player<2;player++)for(uint32_t row=0;row<COOP_COMPACT_VIEW_HEIGHT;row+=4){
  uint32_t y=row+player*COOP_COMPACT_VIEW_HEIGHT,base=(y>>7)*(128u*COOP_COMPACT_WIDTH),yo=compact_y(y);
  for(uint32_t group=0;group<COOP_COMPACT_WIDTH/4u;group++){
   uint32_t *to=destination+base+(compact_x(group*4u)^yo);
   for(uint32_t k=0;k<4;k++){
    const uint64_t *from=(const uint64_t*)(const void*)(views[player]+(size_t)(row+k)*COOP_COMPACT_WIDTH+group*4u);
    uint64_t *target=(uint64_t*)(void*)(to+k*4u);target[0]=from[0];target[1]=from[1];
   }
  }
 }
#endif
}
#endif
