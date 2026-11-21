/* Publish exact private plane physics only to the owned P2 pilot.
 * Mappings stay allocated until game exit; release/drain precedes actor
 * deletion and changing vehicle ownership. (AI-assisted) */
#include "coop-plane-late-layout.h"
typedef struct {u32 enabled,active,error,vehicle;u64 entity;u32 restores,installs,rejected,last_frame;} PlaneRuntime;
_Static_assert(sizeof(PlaneRuntime)==40,"Plane runtime ABI");
COOP_SECTION(".coop.plane_runtime") volatile PlaneRuntime plane_runtime;
#ifdef GTA_COOP_TEST
static u32 plane_runtime_publish(u32,u64);
static u32 plane_runtime_release(void);
static u32 plane_runtime_table_matches(u32,u64);
#else
#ifndef PR_EXT
#define PR_EXT __attribute__((section(".coop.extensions"),noinline))
#endif
static volatile PlaneLateInput *plane_runtime_helper(void){return (volatile PlaneLateInput*)(PLANE_LATE_BASE+4096);}
static PR_EXT u32 plane_runtime_ready(void){
 volatile PlaneLateInput *p=plane_runtime_helper();
 return p->magic==PLANE_LATE_MAGIC&&p->owner==(u64)&session-0x8000ULL&&p->shadow==PLANE_LATE_SHADOW&&p->original==0x405ad50ULL&&p->engine==PLANE_LATE_BASE+0x4000;
}
static PR_EXT u32 plane_runtime_table_matches(u32 h,u64 entity){
 if(!plane_runtime.enabled||!plane_runtime.active||h!=plane_runtime.vehicle||entity!=plane_runtime.entity||!plane_runtime_ready())return 0;
 volatile PlaneLateInput *p=plane_runtime_helper();return p->entity==entity&&p->vehicle==h&&p->pilot==session.p2&&*(volatile u64*)entity==PLANE_LATE_SHADOW;
}
static PR_EXT void plane_restore_pedal_lease(u64 v){
 volatile PlanePedalLease *lease=(volatile PlanePedalLease*)PLANE_PEDAL_LEASE;
 if(v&&lease->entity==v){
  i32 seat=(i32)*(volatile signed char*)(v+0xc71);u32 seats=*(volatile u8*)(v+0xc70);
  u64 primary=hover_lookup(session.p1);
  u32 primary_driving=primary&&seat>=0&&(u32)seat<seats&&*(volatile u64*)(v+0xc78+(u32)seat*8)==primary;
  if(!primary_driving){
   if(*(volatile u32*)(v+0x9b4)==lease->last_gas)*(volatile u32*)(v+0x9b4)=lease->gas;
   if(*(volatile u32*)(v+0x9b8)==lease->last_brake)*(volatile u32*)(v+0x9b8)=lease->brake;
  }
 }
 lease->entity=0;
}
static PR_EXT u32 plane_runtime_release(void){
 if(!plane_runtime.active)return 1;
 if(!plane_runtime_ready()){plane_runtime.error=1;return 0;}
 volatile PlaneLateInput *p=plane_runtime_helper();p->stop=1;
 u64 v=hover_lookup(plane_runtime.vehicle);
 if(v==plane_runtime.entity){
  u64 expected=PLANE_LATE_SHADOW,table=*(volatile u64*)v;
  if(table==PLANE_LATE_SHADOW&&!__atomic_compare_exchange_n((u64*)v,&expected,0x405ad50ULL,0,__ATOMIC_SEQ_CST,__ATOMIC_SEQ_CST))return 0;
  else if(table!=PLANE_LATE_SHADOW&&table!=0x405ad50ULL){plane_runtime.error=2;return 0;}
 }
 if(__atomic_load_n(&p->busy,__ATOMIC_ACQUIRE))return 0;
 plane_restore_pedal_lease(v==plane_runtime.entity?v:0);
 plane_runtime.active=plane_runtime.vehicle=0;plane_runtime.entity=0;plane_runtime.restores++;plane_runtime.error=0;return 1;
}
static PR_EXT u32 plane_runtime_publish(u32 h,u64 entity){
 if(!plane_runtime.enabled||!plane_runtime_ready()||plane_runtime.active||!entity||hover_lookup(h)!=entity||*(volatile u64*)entity!=0x405ad50ULL||*(volatile u32*)(entity+0xbf8)!=1)return 0;
 volatile PlaneLateInput *p=plane_runtime_helper();p->stop=1;if(p->busy)return 0;
 u64 ped=hover_lookup(session.p2);i32 seat=(i32)*(volatile signed char*)(entity+0xc71);u32 seats=*(volatile u8*)(entity+0xc70);
 if(!ped||session.p2==session.p1||seat<0||(u32)seat>=seats||*(volatile u64*)(entity+0xc78+(u32)seat*8)!=ped)return 0;
 for(u32 at=0;at<0x6a0;at+=8){
  u64 expected=*(volatile u64*)(0x405ad40ULL+at);
  if(at==16+0x498)expected=PLANE_LATE_BASE+0x800;
  else if(at==16+0x670)expected=PLANE_LATE_BASE+0x6000;
  if(*(volatile u64*)(PLANE_LATE_BASE+0x10000+at)!=expected)return 0;
 }
 p->entity=entity;p->vehicle=h;p->pilot=session.p2;p->mode=1;p->remaining=0xffffffffu;
 plane_runtime.vehicle=h;plane_runtime.entity=entity;plane_runtime.active=1;
 u64 expected=0x405ad50ULL;
 if(!__atomic_compare_exchange_n((u64*)entity,&expected,PLANE_LATE_SHADOW,0,__ATOMIC_SEQ_CST,__ATOMIC_SEQ_CST)){
  plane_runtime.active=plane_runtime.vehicle=0;plane_runtime.entity=0;return 0;
 }
 p->stop=0;plane_runtime.installs++;plane_runtime.error=0;return 1;
}
#undef PR_EXT
#endif
