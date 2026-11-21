#ifndef COOP_AIM_RETICLE_H
#define COOP_AIM_RETICLE_H
/* Project the actual scripted shooting coordinate through the captured P2
 * inverse-view pose. Integer-only: preserve the renderer's SIMD/FPU state.
 * The owned P2 camera uses the fixed native 60-degree vertical FOV. */
static int64_t coop_aim_fixed(uint32_t bits){
 uint32_t exponent=(bits>>23)&255u;uint64_t value=(bits&0x7fffffu)|(exponent?0x800000u:0);
 int shift=(int)exponent-134;
 if(shift>=0)value<<=(unsigned)shift;else value=(-shift<64)?value>>(unsigned)-shift:0;
 return bits>>31?-(int64_t)value:(int64_t)value;
}
static int coop_aim_project(const uint32_t pose[16],const uint32_t aim[3],int *x,int *y){
 int64_t delta[3],axis[3]={0};
 for(unsigned k=0;k<3;k++){
  if((aim[k]&0x7fffffffu)>0x471c4000u||(pose[12+k]&0x7fffffffu)>0x471c4000u)return 0;
  delta[k]=coop_aim_fixed(aim[k])-coop_aim_fixed(pose[12+k]);
 }
 for(unsigned a=0;a<3;a++)for(unsigned k=0;k<3;k++){
  if((pose[a*4+k]&0x7fffffffu)>0x40000000u)return 0;
  axis[a]+=(delta[k]*coop_aim_fixed(pose[a*4+k]))>>16;
 }
 int64_t depth=-axis[2];if(depth<6554)return 0;
 /* 2160/(2*tan(30 degrees)) = 1870.615 source pixels. */
 int64_t sx=1920+axis[0]*1871/depth,sy=1080-axis[1]*1871/depth;
 if(sx<24||sx>=3816||sy<564||sy>=1596)return 0;
 *x=(int)(sx/COOP_COMPACT_STEP);*y=(int)((sy-540)/COOP_COMPACT_STEP);return 1;
}
static __attribute__((noinline)) int compact_draw_p2_reticle(uint32_t *view,const uint32_t pose[16],const uint32_t aim[3]){
 int x,y;if(!coop_aim_project(pose,aim,&x,&y))return 0;
 for(int dy=-2;dy<=2;dy++)for(int dx=-2;dx<=2;dx++){
  uint32_t color=(dx>=-1&&dx<=1&&dy>=-1&&dy<=1)?0xffffffffu:0xc0000000u;
  view[(size_t)(y+dy)*COOP_COMPACT_WIDTH+(unsigned)(x+dx)]=color;
 }
 return 1;
}
#endif
