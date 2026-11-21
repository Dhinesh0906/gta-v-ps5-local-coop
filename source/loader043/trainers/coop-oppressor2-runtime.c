/* P2-only Mk II hover arguments. Disabled until exact helper staging.
 * No speed/handling/velocity edits or global player-identity changes. */
#include "coop-oppressor2-runtime-layout.h"
#include "coop-oppressor2-probe-layout.h"
COOP_SECTION(".coop.oppressor2") volatile OppressorFlightRuntime oppressor_runtime;
#ifdef GTA_COOP_TEST
#define OP_EXT __attribute__((noinline))
static u32 oppressor_test_ready;
static u64 oppressor_test_entity,oppressor_test_ped,oppressor_test_primary;
static volatile OppressorInput oppressor_test_input;
static volatile OppressorInput *oppressor_helper(void){return &oppressor_test_input;}
static u64 oppressor_lookup(u32 h){return h==session.p1?oppressor_test_primary:h==session.p2?oppressor_test_ped:h==player_controls.vehicle?oppressor_test_entity:0;}
#else
#define OP_EXT __attribute__((section(".coop.extensions"),noinline))
static volatile OppressorInput *oppressor_helper(void){return (volatile OppressorInput*)(OPPRESSOR_PRODUCTION_BASE+4096);}
static u64 oppressor_lookup(u32 h){return hover_lookup(h);}
#endif
static OP_EXT u32 oppressor_ready(void){
#ifdef GTA_COOP_TEST
 return oppressor_test_ready;
#else
 volatile OppressorInput *h=oppressor_helper();
 if(h->magic!=OPPRESSOR_PRODUCTION_MAGIC||h->probe!=(u64)&oppressor_runtime||h->shadow!=OPPRESSOR_PRODUCTION_BASE+0xf010)return 0;
 for(u32 at=0;at<OPPRESSOR_TABLE_BYTES;at+=8){u64 expected=*(volatile u64*)(0x4057b38ULL+at);if(at==16+0x3c0)expected=OPPRESSOR_PRODUCTION_BASE+0x8000;if(*(volatile u64*)(OPPRESSOR_PRODUCTION_BASE+0xf000+at)!=expected)return 0;}
 return 1;
#endif
}
static OP_EXT u32 oppressor_controls_release(void){
 if(!oppressor_runtime.status){oppressor_runtime.vehicle=0;oppressor_runtime.entity=0;oppressor_runtime.pilot=0;return 1;}
 if(!oppressor_ready()){oppressor_runtime.error=1;return 0;}
 volatile OppressorInput *h=oppressor_helper();h->stopped=1;
 u64 v=oppressor_lookup(oppressor_runtime.vehicle);
 if(v&&v==oppressor_runtime.entity){
  u64 table=*(volatile u64*)v,expected=OPPRESSOR_PRODUCTION_BASE+0xf010;
  if(table==expected&&!__atomic_compare_exchange_n((u64*)v,&expected,0x4057b48ULL,0,__ATOMIC_SEQ_CST,__ATOMIC_SEQ_CST))return 0;
  if(table!=OPPRESSOR_PRODUCTION_BASE+0xf010&&table!=0x4057b48ULL){oppressor_runtime.error=2;return 0;}
 }
 if(__atomic_load_n(&h->busy,__ATOMIC_ACQUIRE))return 0;
 if(v&&v==oppressor_runtime.entity&&h->applied){
  i32 seat=(i32)*(volatile signed char*)(v+0xc71);u32 seats=*(volatile u8*)(v+0xc70);u64 primary=oppressor_lookup(session.p1);
  u32 primary_driving=primary&&seat>=0&&(u32)seat<seats&&*(volatile u64*)(v+0xc78+(u32)seat*8)==primary;
  if(!primary_driving){
   for(u32 n=0;n<2;n++)if(*(volatile u32*)(v+0x9b4+n*4)==((volatile u32*)h->after)[n+2])*(volatile u32*)(v+0x9b4+n*4)=((volatile u32*)h->before)[n+2];
   if(*(volatile u8*)(v+0x9bc)==((volatile u32*)h->before)[1])*(volatile u8*)(v+0x9bc)=(u8)((volatile u32*)h->before)[0];
  }
 }
 h->entity=0;oppressor_runtime.status=oppressor_runtime.vehicle=oppressor_runtime.pilot=oppressor_runtime.error=0;oppressor_runtime.entity=0;return 1;
}
static OP_EXT u32 oppressor_controls_owns(u32 v){return oppressor_runtime.enabled&&oppressor_runtime.status==1&&oppressor_runtime.vehicle==v;}
static OP_EXT float oppressor_pressure(u32 binding){
 if(binding==0x100)return session.data[8]/255.0f;
 if(binding==0x200)return session.data[9]/255.0f;
 return (session.input.held&binding)?1:0;
}
static OP_EXT void oppressor_controls_tick(void){
 if(!oppressor_runtime.enabled||session.state!=2||session.mode!=6||session.phase!=3||session.stop||session.error||session.paused||!session.connected||!session.input.armed||network()||!session.p2||session.p2==session.p1||!player_controls.p2_collision||player_controls.action==5||(player_controls.vehicle_model!=2069146067u&&player_controls.vehicle_model!=884483972u)||!N(IS_PED_IN_ANY_VEHICLE,session.p2,0)){
  oppressor_controls_release();return;
 }
 if(player_controls.vehicle_model==884483972u&&(!player_controls.wings_active||!N(IS_ENTITY_IN_AIR,player_controls.vehicle))){oppressor_controls_release();return;}
 u32 v=N(GET_VEHICLE_PED_IS_IN,session.p2,0);
 if(!v||v!=player_controls.vehicle||(u32)N(GET_PED_IN_VEHICLE_SEAT,v,-1,0)!=session.p2||(u32)N(GET_ENTITY_MODEL,v)!=player_controls.vehicle_model){oppressor_controls_release();return;}
 if(oppressor_runtime.vehicle!=v){
  if(!oppressor_controls_release()||!road_runtime_release()||!oppressor_ready())return;
  volatile OppressorInput *h=oppressor_helper();h->stopped=1;if(h->busy)return;
  u64 entity=oppressor_lookup(v),ped=oppressor_lookup(session.p2);if(!entity||!ped||ped==oppressor_lookup(session.p1)||*(volatile u64*)entity!=0x4057b48ULL||*(volatile u32*)(entity+0xbf8)!=11)return;
  i32 seat=(i32)*(volatile signed char*)(entity+0xc71);u32 seats=*(volatile u8*)(entity+0xc70);
  if(seat<0||(u32)seat>=seats||*(volatile u64*)(entity+0xc78+(u32)seat*8)!=ped)return;
  oppressor_runtime.magic=OPPRESSOR_RUNTIME_MAGIC;oppressor_runtime.owner=(u64)&session-0x8000ULL;oppressor_runtime.entity=entity;
  oppressor_runtime.vehicle=v;oppressor_runtime.pilot=session.p2;oppressor_runtime.frame=session.last_frame;oppressor_runtime.status=1;oppressor_runtime.error=0;
  h->entity=entity;h->calls=h->applied=h->rejected=h->reason=0;
  u64 expected=0x4057b48ULL;
  if(!__atomic_compare_exchange_n((u64*)entity,&expected,OPPRESSOR_PRODUCTION_BASE+0xf010,0,__ATOMIC_SEQ_CST,__ATOMIC_SEQ_CST)){oppressor_runtime.status=0;oppressor_runtime.vehicle=0;oppressor_runtime.entity=0;h->entity=0;return;}
  N(CLEAR_PED_TASKS,session.p2);if(player_controls.vehicle_model==2069146067u)N(SET_SPECIAL_FLIGHT_MODE_TARGET_RATIO,v,F(1));N(SET_VEHICLE_ENGINE_ON,v,1,1,0);
 }
 oppressor_runtime.frame=session.last_frame;oppressor_runtime.values[0]=session.input.x;oppressor_runtime.values[1]=-session.input.y;
 oppressor_runtime.values[2]=oppressor_pressure(player_controls.bindings[B_ACCEL]);oppressor_runtime.values[3]=oppressor_pressure(player_controls.bindings[B_BRAKE]);
 oppressor_helper()->stopped=0;player_controls.action=6;
}
#undef OP_EXT
