/* Optional P2-only assists. Vehicle settings are leased only while occupied
 * by P2 and restored on exit, toggle-off, cleanup, or a changed vehicle. */
typedef struct {
 u32 enabled,vehicle,original_damage,original_tyres;
 u64 vehicle_entity;
 u32 target,scans,locks,updates,vehicle_checks,vehicle_restores,error,last_scan;
 u32 last_vehicle_check,reserved[3];
} PlayerAssists;
_Static_assert(sizeof(PlayerAssists)==72,"Player assists ABI");
COOP_SECTION(".coop.assists") volatile PlayerAssists player_assists;
#ifdef GTA_COOP_TEST
#define ASSIST_EXT __attribute__((noinline))
static u64 assist_entity(u32);
#else
#define ASSIST_EXT __attribute__((section(".coop.extensions"),noinline))
static ASSIST_EXT u64 assist_entity(u32 handle){return hover_lookup(handle);}
#endif
#ifdef GTA_COOP_TEST
static u32 motorcycle_test_store(u32,float,float);
#endif
static ASSIST_EXT u32 player_motorcycle_table_owned(u32 vehicle,u64 entity,u64 table){
 return table==0x4057b48ULL||(road_runtime_owns(vehicle)&&road_runtime.selected==1&&
  entity==road_runtime.entity&&table==road_selected_helper()+0x4010ULL);
}
static ASSIST_EXT void player_motorcycle_tick(u32 vehicle){
 if(!vehicle||session.p2==session.p1||session.paused||!session.input.armed||
  (u32)N(GET_PED_IN_VEHICLE_SEAT,vehicle,-1,0)!=session.p2)return;
 float lean=-session.input.x,balance=-session.input.y;
 if(lean<-1||lean>1||balance<-1||balance>1)return;
#ifdef GTA_COOP_TEST
 if(!motorcycle_test_store(vehicle,lean,balance))return;
#else
 u64 entity=assist_entity(vehicle);if(!entity||!player_motorcycle_table_owned(vehicle,entity,*(volatile u64*)entity))return;
 /* Exact bike balance reader 0x26c7de0 returns these caches unchanged for
  * non-player riders. They must follow P2 input, including neutral. Preserve
  * the engine's cache flag/reset timing; only publish the two input floats. */
 *(volatile float*)(entity+0x1554)=lean;*(volatile float*)(entity+0x1550)=balance;
#endif
 player_assists.reserved[1]++;
}
static ASSIST_EXT u32 player_assists_release(void){
 u32 v=player_assists.vehicle;
 if(v&&N(DOES_ENTITY_EXIST,v)&&assist_entity(v)==player_assists.vehicle_entity){
  /* Do not leave a lean command cached for the next NPC rider. A primary
   * player who already took the seat owns its inputs and is never reset. */
  if((u32)N(GET_VEHICLE_CLASS,v)==8&&(u32)N(GET_PED_IN_VEHICLE_SEAT,v,-1,0)!=session.p1){
#ifdef GTA_COOP_TEST
   motorcycle_test_store(v,0,0);
#else
   u64 entity=player_assists.vehicle_entity;
   if(player_motorcycle_table_owned(v,entity,*(volatile u64*)entity)){*(volatile float*)(entity+0x1554)=0;*(volatile float*)(entity+0x1550)=0;}
#endif
  }
  N(SET_ENTITY_INVINCIBLE,v,!player_assists.original_damage);
  N(SET_VEHICLE_TYRES_CAN_BURST,v,player_assists.original_tyres);
  if((u32)N(GET_ENTITY_CAN_BE_DAMAGED,v)!=player_assists.original_damage||
   (u32)N(GET_VEHICLE_TYRES_CAN_BURST,v)!=player_assists.original_tyres){player_assists.error=1;return 0;}
 }
 if(v)player_assists.vehicle_restores++;
 player_assists.vehicle=0;player_assists.vehicle_entity=0;return 1;
}
static ASSIST_EXT void player_vehicle_god_tick(void){
 u32 valid=(player_assists.enabled&1)&&session.state==2&&!session.stop&&!session.error&&session.mode==6&&session.phase==3&&
  session.p2&&session.p2!=session.p1&&!network()&&N(DOES_ENTITY_EXIST,session.p2)&&!N(IS_ENTITY_DEAD,session.p2,0);
 u32 v=valid&&N(IS_PED_IN_ANY_VEHICLE,session.p2,0)?N(GET_VEHICLE_PED_IS_IN,session.p2,0):0;
 if(v!=player_assists.vehicle){
  if(!player_assists_release())return;
  if(v&&N(DOES_ENTITY_EXIST,v)){
   u64 e=assist_entity(v);if(!e)return;
   player_assists.vehicle=v;player_assists.vehicle_entity=e;
   player_assists.original_damage=!!N(GET_ENTITY_CAN_BE_DAMAGED,v);
   player_assists.original_tyres=!!N(GET_VEHICLE_TYRES_CAN_BURST,v);
   player_assists.last_vehicle_check=0;
  }
 }
 if(!v)return;
 u32 now=N(GET_GAME_TIMER);
 if(player_assists.last_vehicle_check&&now-player_assists.last_vehicle_check<250)return;
 if(assist_entity(v)!=player_assists.vehicle_entity){player_assists.vehicle=0;player_assists.vehicle_entity=0;return;}
 N(SET_ENTITY_INVINCIBLE,v,1);N(SET_VEHICLE_TYRES_CAN_BURST,v,0);
 player_assists.last_vehicle_check=now;player_assists.vehicle_checks++;
 if(N(GET_ENTITY_CAN_BE_DAMAGED,v)||N(GET_VEHICLE_TYRES_CAN_BURST,v))player_assists.error=2;
}
static ASSIST_EXT u32 assist_target_position(u32 ped,float at[3]){
 if(!ped||ped==session.p1||ped==session.p2||!N(DOES_ENTITY_EXIST,ped)||N(IS_ENTITY_DEAD,ped,0)||!coords(ped,at))return 0;
 at[2]+=.4f;return coop_position_valid(at);
}
static ASSIST_EXT float assist_target_score(const float origin[3],const float direction[3],const float at[3],float threshold){
 float d[3],distance=0,dot=0,length=0;
 for(u32 k=0;k<3;k++){d[k]=at[k]-origin[k];distance+=d[k]*d[k];dot+=d[k]*direction[k];length+=direction[k]*direction[k];}
 if(distance<1||distance>10000||dot<=0||length<1||dot*dot<distance*length*threshold)return -1;
 return dot*dot/(distance*length)-distance*.000001f;
}
static ASSIST_EXT void player_assist_aim(u32 now,const float pos[3]){
 if(!(player_assists.enabled&2)||session.paused||!session.input.armed||!action_down(B_AIM)||action_down(B_WEAPON)||
  player_controls.vehicle||player_controls.weapon_index>=player_controls.weapon_count||
  player_controls.weapon_groups[player_controls.weapon_index]>5){player_assists.target=0;return;}
 float origin[3]={pos[0],pos[1],pos[2]+1.3f},direction[3],at[3];
 for(u32 k=0;k<3;k++)direction[k]=player_controls.aim[k]-origin[k];
 u32 locked=player_assists.target;
 if(locked&&(!assist_target_position(locked,at)||assist_target_score(origin,direction,at,.65f)<0||
  !N(HAS_ENTITY_CLEAR_LOS_TO_ENTITY,session.p2,locked,17)||coop_abs(coop_axis(session.data[6]))>.6f||coop_abs(coop_axis(session.data[7]))>.6f)){
  player_assists.target=0;locked=0;
 }
 if(!locked&&(!player_assists.last_scan||now-player_assists.last_scan>=120)){
  /* Exact PS5 handler consumes capacity at +0 and writes 32-bit handles at
   * +8, +16, ... (8-byte script slots). Never use a PC packed-int layout. */
  u64 nearby[33]={0};nearby[0]=32;
  i32 count=(i32)N(GET_PED_NEARBY_PEDS,session.p2,P(nearby),-1);
  player_assists.scans++;player_assists.last_scan=now;
  if(count<0||count>32){player_assists.error=3;return;}
  float best=-1;
  for(i32 i=0;i<count;i++){
   u32 ped=(u32)nearby[i+1];float candidate[3];if(!assist_target_position(ped,candidate))continue;
   float score=assist_target_score(origin,direction,candidate,.88f);
   if(score>best&&N(HAS_ENTITY_CLEAR_LOS_TO_ENTITY,session.p2,ped,17)){best=score;locked=ped;for(u32 k=0;k<3;k++)at[k]=candidate[k];}
  }
  if(locked){player_assists.target=locked;player_assists.locks++;}
 }
 if(locked){for(u32 k=0;k<3;k++)player_controls.aim[k]=at[k];player_assists.updates++;}
}
