/* Bounded empty-vehicle inventory, forwarded on the existing script thread.
 * No seating, custom physics, player or shared-code changes. (AI-assisted) */
#include <stdint.h>
#include <stddef.h>
#include "coop-native-handlers.h"
#include "coop-motorcycle-catalogue.h"
typedef uint64_t u64;typedef uint32_t u32;typedef uint8_t u8;
typedef struct{float x,y,z,w;} Vec4;
typedef struct{u64 *result;u32 count,pad0;u64 *args;u32 vectors,pad1;float *dest[4];Vec4 temp[4];} Context;
typedef struct{
 u64 magic,forward,owner;
 u32 stop,busy,state,error,request,completed,model,vehicle,model_owned,started,last_frame,created,deleted;
 u64 entity,table,physics;
 u32 physical_type,vehicle_class;
 u32 rocket,jump,parachute,retractable;
 u64 handling;
 u32 sub_count,reserved;
 struct {u64 table,type_method;u32 mode,reserved;float values[4];} sub[8];
} Inventory;
__attribute__((section(".probe.data"),used)) volatile Inventory inventory;
__attribute__((section(".probe.layout"),used)) const u32 inventory_layout[]={sizeof(Inventory),offsetof(Inventory,stop),offsetof(Inventory,busy),offsetof(Inventory,state),offsetof(Inventory,error),offsetof(Inventory,request),offsetof(Inventory,completed),offsetof(Inventory,model),offsetof(Inventory,vehicle),offsetof(Inventory,model_owned),offsetof(Inventory,created),offsetof(Inventory,deleted),offsetof(Inventory,entity),offsetof(Inventory,table),offsetof(Inventory,physics),offsetof(Inventory,physical_type),offsetof(Inventory,vehicle_class),offsetof(Inventory,rocket),offsetof(Inventory,jump),offsetof(Inventory,parachute),offsetof(Inventory,retractable),offsetof(Inventory,handling),offsetof(Inventory,sub_count),offsetof(Inventory,sub),sizeof(inventory.sub[0])};
static u64 pointer(const void *p){u64 a=(u64)p;__asm__ volatile("":"+r"(a));return a;}
static u64 floating(float f){union{float f;u32 u;}n;n.f=f;return n.u;}
__attribute__((noinline)) static u64 call(u64 at,u32 count,u64 *args){
 u64 result[4]={0},padded[32]={0};Context c={0};if(count>32)return 0;
 for(u32 n=0;n<count;n++)padded[n]=args[n];c.result=result;c.count=count;c.args=padded;
 ((void(*)(Context*))at)(&c);return result[0];
}
#define N(name,...) call(COOP_H_##name,sizeof((u64[]){0,##__VA_ARGS__})/8-1,((u64[]){0,##__VA_ARGS__})+1)
#define F(n) floating((float)(n))
#define P(p) pointer(p)
static u32 word(u64 at){return *(volatile u32*)at;}
static u64 wide(u64 at){return *(volatile u64*)at;}
static u64 lookup(u32 h){
 if(!h||(int32_t)h<0)return 0;u64 p=wide(0x61a8250);if(!p)return 0;
 u32 index=h>>8,count=word(p+24),stride=word(p+28);u64 entries=wide(p+8),gen=wide(p+16);
 if(!entries||!gen||index>=count||stride<16||stride>256||*(volatile u8*)(gen+index)!=(u8)h)return 0;
 return wide(entries+(u64)index*stride+8);
}
static int ready(void){u64 o=inventory.owner;return o&&wide(o+0x8000)==0x315353504f4f43ULL&&word(o+0x8010)==2&&!word(o+0x8014)&&!word(o+0x8018)&&word(o+0x8208)==3&&!word(o+0x8218)&&!*(volatile u8*)0x5abefb8ULL;}
static int allowed(u32 model){for(u32 n=0;n<sizeof(motorcycle_models)/4;n++)if(motorcycle_models[n]==model)return 1;return 0;}
static void finish(void){if(inventory.model_owned){N(SET_MODEL_AS_NO_LONGER_NEEDED,inventory.model);inventory.model_owned=0;}inventory.state=3;inventory.completed=inventory.request;}
static void tick(void){
 u32 frame=N(GET_FRAME_COUNT);if(frame==inventory.last_frame)return;inventory.last_frame=frame;
 u32 now=N(GET_GAME_TIMER);
 if(inventory.vehicle){
  u32 v=inventory.vehicle;u64 o=inventory.owner;
  if(v==word(o+0x8050)||v==word(o+0x8054)){inventory.error=10;return;}
  if(N(DOES_ENTITY_EXIST,v)){
   if((u32)N(GET_ENTITY_MODEL,v)!=inventory.model||N(GET_PED_IN_VEHICLE_SEAT,v,(u64)-1,0)){inventory.error=11;return;}
   N(DELETE_VEHICLE,P(&v));
  }
  if(!N(DOES_ENTITY_EXIST,inventory.vehicle)){inventory.vehicle=0;inventory.deleted++;finish();}return;
 }
 if(!inventory.request||inventory.request==inventory.completed)return;
 if(inventory.stop||!ready()){inventory.error=1;finish();return;}
 if(inventory.state!=1){
  inventory.created=inventory.deleted=inventory.error=0;inventory.entity=inventory.table=inventory.physics=0;inventory.physical_type=inventory.vehicle_class=0;
  inventory.rocket=inventory.jump=inventory.parachute=inventory.retractable=0;inventory.handling=0;inventory.sub_count=0;
  for(u32 n=0;n<8;n++){inventory.sub[n].table=inventory.sub[n].type_method=0;inventory.sub[n].mode=0;for(u32 k=0;k<4;k++)inventory.sub[n].values[k]=0;}
  if(!allowed(inventory.model)||!N(IS_MODEL_VALID,inventory.model)||!N(IS_MODEL_IN_CDIMAGE,inventory.model)){inventory.error=2;finish();return;}
  inventory.started=now;inventory.state=1;if(!N(HAS_MODEL_LOADED,inventory.model)){N(REQUEST_MODEL,inventory.model);inventory.model_owned=1;}
 }
 if(!N(HAS_MODEL_LOADED,inventory.model)){if(now-inventory.started>5000){inventory.error=3;finish();}return;}
 u32 v=N(CREATE_VEHICLE,inventory.model,F(-420),F(-580),F(500),F(0),0,0,0);
 if(!v){inventory.error=4;finish();return;}
 inventory.vehicle=v;inventory.created++;inventory.state=2;
 N(SET_ENTITY_AS_MISSION_ENTITY,v,1,1);N(FREEZE_ENTITY_POSITION,v,1);N(SET_ENTITY_VISIBLE,v,0,0);
 u64 entity=lookup(v);inventory.entity=entity;if(!entity){inventory.error=5;return;}
 inventory.table=wide(entity);inventory.physical_type=word(entity+0xbf8);inventory.vehicle_class=N(GET_VEHICLE_CLASS,v);
 inventory.physics=wide(inventory.table+0x3c0);
 inventory.rocket=N(GET_HAS_ROCKET_BOOST,v);inventory.jump=N(GET_CAR_HAS_JUMP,v);
 inventory.parachute=N(GET_VEHICLE_HAS_PARACHUTE,v);inventory.retractable=N(GET_HAS_RETRACTABLE_WHEELS,v);
 /* Same exact-build array fields read by SET_SPECIAL_FLIGHT_MODE_TARGET_RATIO.
  * Record concrete type methods; never call an unknown virtual function. */
 u64 handling=wide(entity+0x930);inventory.handling=handling;
 if(handling){
  u32 count=*(volatile uint16_t*)(handling+0x160);u64 array=wide(handling+0x158);
  inventory.sub_count=count;
  if(array&&count<=8)for(u32 n=0;n<count;n++){
   u64 sub=wide(array+n*8);if(!sub)continue;u64 table=wide(sub);
   inventory.sub[n].table=table;inventory.sub[n].type_method=wide(table+0x18);
   /* Type 10's b0 mode selects the stock hover adapter. Only record it
    * for that fully identified layout, not shorter subhandling objects. */
   if(table==0x4059080ULL&&inventory.sub[n].type_method==0x27d1d60ULL){
    inventory.sub[n].mode=word(sub+0xb0);
    for(u32 k=0;k<4;k++)inventory.sub[n].values[k]=*(volatile float*)(sub+0x50+k*4);
   }
  }
 }
}
__attribute__((section(".entry"),used)) void _start(Context *context){
 if(inventory.forward)((void(*)(Context*))inventory.forward)(context);
 if(inventory.magic!=0x31564e494b494243ULL)return;inventory.busy=1;tick();inventory.busy=0;
}
