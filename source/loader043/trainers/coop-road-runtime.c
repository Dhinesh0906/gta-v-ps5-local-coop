/* Script-thread ownership of the private automobile input helper. Disabled
 * until its exact code/table are staged; never modifies the global AI. */
typedef struct {
 u64 helper,entity;
 u32 enabled,vehicle,active,error,installs,restores,rejected,last_task_ms,task,selected;
 u64 bike_helper;
} RoadRuntime;
_Static_assert(sizeof(RoadRuntime)==64,"Road runtime mailbox ABI");
COOP_SECTION(".coop.road") volatile RoadRuntime road_runtime;
#ifdef GTA_COOP_TEST
#define ROAD_EXT __attribute__((noinline))
#else
#define ROAD_EXT __attribute__((section(".coop.extensions"),noinline))
#endif
#ifdef GTA_COOP_TEST
static u32 road_test_publish(u32,u32,u64*);
static u32 road_test_restore(u32,u64);
#endif
static ROAD_EXT u64 road_selected_helper(void){return road_runtime.selected?road_runtime.bike_helper:road_runtime.helper;}
static ROAD_EXT u64 road_selected_table(void){return road_runtime.selected?0x4057b48ULL:0x4056e28ULL;}
static ROAD_EXT u32 road_runtime_ready(void){
 if(road_runtime.selected>1)return 0;
#ifdef GTA_COOP_TEST
 return road_selected_helper()!=0;
#else
 u64 h=road_selected_helper();
 return h&&*(volatile u64*)(h+8192)==0x3154504e49444152ULL&&
  *(volatile u64*)(h+8200)==(road_runtime.selected?0x26c8520ULL:0x263c900ULL)&&
  *(volatile u64*)(h+8208)==(u64)&session-0x8000ULL&&
  *(volatile u64*)(h+8232)==road_selected_table();
#endif
}
static ROAD_EXT u32 road_runtime_release(void){
 if(!road_runtime.active){road_runtime.vehicle=0;road_runtime.entity=0;return 1;}
#ifdef GTA_COOP_TEST
 if(!road_test_restore(road_runtime.vehicle,road_runtime.entity))return 0;
#else
 if(!road_runtime_ready()){road_runtime.error=1;return 0;}
 u64 h=road_selected_helper(),v=hover_lookup(road_runtime.vehicle);
 *(volatile u32*)(h+8192+48)=0; /* Stop only our input publication first. */
 if(v&&v==road_runtime.entity){
  u64 expected=h+0x4010ULL;
  if(*(volatile u64*)v==expected&&!__atomic_compare_exchange_n((u64*)v,&expected,road_selected_table(),0,__ATOMIC_SEQ_CST,__ATOMIC_SEQ_CST))return 0;
 }
 *(volatile u64*)(h+8192+24)=0;
#endif
 if(session.p2&&road_runtime.vehicle&&N(DOES_ENTITY_EXIST,road_runtime.vehicle)&&
   (u32)N(GET_PED_IN_VEHICLE_SEAT,road_runtime.vehicle,-1,0)==session.p2)N(CLEAR_PED_TASKS,session.p2);
 road_runtime.active=road_runtime.vehicle=0;road_runtime.entity=0;road_runtime.restores++;
 return 1;
}
static ROAD_EXT u32 road_runtime_owns(u32 vehicle){
 return road_runtime.enabled&&road_runtime.active&&road_runtime.vehicle==vehicle&&road_runtime_ready();
}
static ROAD_EXT u32 road_runtime_publish(u32 vehicle){
 u64 entity=0;
#ifdef GTA_COOP_TEST
 road_runtime.selected=N(GET_VEHICLE_CLASS,vehicle)==8;
 if(!road_runtime_ready()||!road_test_publish(vehicle,session.p2,&entity))return 0;
#else
 u64 v=hover_lookup(vehicle),p=hover_lookup(session.p2);if(!v||!p||p==hover_lookup(session.p1))return 0;
 u64 table=*(volatile u64*)v;
 if(table==0x4056e28ULL){if(*(volatile u32*)(v+0xbf8))return 0;road_runtime.selected=0;}
 else if(table==0x4057b48ULL)road_runtime.selected=1;else return 0;
 if(!road_runtime_ready())return 0;u64 h=road_selected_helper();
 i32 seat=(i32)*(volatile signed char*)(v+0xc71);u32 seats=*(volatile u8*)(v+0xc70);
 if(seat<0||(u32)seat>=seats||*(volatile u64*)(v+0xc78+(u32)seat*8)!=p)return 0;
 *(volatile u32*)(h+8192+48)=0;*(volatile u64*)(h+8192+24)=v;
 *(volatile u32*)(h+8192+52)=0;*(volatile u32*)(h+8192+60)=0xffffffffu;
 *(volatile u32*)(h+8192+48)=1;
 u64 expected=road_selected_table();
 if(!__atomic_compare_exchange_n((u64*)v,&expected,h+0x4010ULL,0,__ATOMIC_SEQ_CST,__ATOMIC_SEQ_CST)){*(volatile u32*)(h+8192+48)=0;return 0;}
 entity=v;
#endif
 road_runtime.entity=entity;road_runtime.vehicle=vehicle;road_runtime.active=1;road_runtime.installs++;road_runtime.error=0;
 /* Remove any retained directional child task once, then use only straight
  * pedal/handbrake tasks to retain the engine's seated driver animation. */
 N(CLEAR_PED_TASKS,session.p2);road_runtime.task=0;road_runtime.last_task_ms=0;
 return 1;
}
#ifdef GTA_COOP_TEST
__attribute__((noinline))
#else
__attribute__((section(".coop.extensions"),noinline))
#endif
static void road_runtime_tick(void){
 if(!road_runtime.enabled||!road_runtime_ready()||session.state!=2||session.mode!=6||session.phase!=3||session.stop||session.error||session.paused||!session.connected||!session.input.armed||network()||!session.p2||session.p2==session.p1||!player_controls.p2_collision||player_controls.action==5||!N(IS_PED_IN_ANY_VEHICLE,session.p2,0)){
  road_runtime_release();return;
 }
 u32 v=N(GET_VEHICLE_PED_IS_IN,session.p2,0);
 if(!v||v!=player_controls.vehicle||oppressor_controls_owns(v)||(u32)N(GET_PED_IN_VEHICLE_SEAT,v,-1,0)!=session.p2||N(GET_VEHICLE_CLASS,v)==16||
   (player_controls.vehicle_model==model_hash("deluxo")&&hover_controls.enabled&&player_controls.hover_active)){
  road_runtime_release();return;
 }
 if(road_runtime.vehicle!=v){if(!road_runtime_release())return;if(!road_runtime_publish(v)){road_runtime.rejected++;return;}}
 u32 action=1;
 if(action_down(B_HANDBRAKE)&&!(player_controls.current_vehicle_weapon&&vehicle_down(VB_FIRE)))action=6;
 else if(action_down(B_ACCEL)&&action_down(B_BRAKE))action=30;
 else if(action_down(B_BRAKE))action=3;
 else if(action_down(B_ACCEL))action=32;
 u32 now=player_controls.last_ms;
 if(action!=road_runtime.task||now-road_runtime.last_task_ms>=100){
  N(TASK_VEHICLE_TEMP_ACTION,session.p2,v,action,1000);road_runtime.task=action;road_runtime.last_task_ms=now;
 }
 player_controls.action=6;player_controls.vehicle_action=action;
}
