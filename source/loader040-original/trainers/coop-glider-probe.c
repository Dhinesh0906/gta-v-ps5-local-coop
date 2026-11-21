/* All natives execute on the existing script thread. The request owns one
 * invisible bike/pilot for <=7 seconds and cleans up on pause/host loss.
 * It forwards the current co-op/display/audio chain unchanged. (AI-assisted) */
#include "coop-glider-probe-layout.h"
#include <stddef.h>
#include "coop-native-handlers.h"
#define COOP_H_SET_PED_CAN_BE_KNOCKED_OFF_VEHICLE 0x1BAC7F0ULL
typedef uint64_t u64;typedef uint32_t u32;typedef uint8_t u8;
typedef struct {float x,y,z,w;} Vec4;
typedef struct{u64 *result;u32 count,pad0;u64 *args;u32 vectors,pad1;float *dest[4];Vec4 temp[4];} Context;
__attribute__((section(".probe.data"),used)) volatile GliderProbe glider_probe;
__attribute__((section(".probe.layout"),used)) const u32 glider_probe_layout[]={sizeof(GliderProbe),offsetof(GliderProbe,stop),offsetof(GliderProbe,request),offsetof(GliderProbe,model),offsetof(GliderProbe,vehicle),offsetof(GliderProbe,pilot),offsetof(GliderProbe,mode),offsetof(GliderProbe,steps),offsetof(GliderProbe,axis),offsetof(GliderProbe,frames),offsetof(GliderProbe,value),offsetof(GliderProbe,values),offsetof(GliderProbe,rotation_start),offsetof(GliderProbe,rotation_end),offsetof(GliderProbe,velocity_start),offsetof(GliderProbe,velocity_end),offsetof(GliderProbe,entity)};
static u64 floating(float f){union{float f;u32 i;}v;v.f=f;return v.i;}
static float as_float(u64 n){union{float f;u32 i;}v;v.i=n;return v.f;}
static u64 pointer(const void *p){u64 a=(u64)p;__asm__ volatile("":"+r"(a));return a;}
__attribute__((noinline)) static u64 call(u64 address,u32 count,u64 *args,u64 *out){
 u64 local[4]={0},padded[32]={0};Context c={0};if(count>32)return 0;for(u32 i=0;i<count;i++)padded[i]=args[i];
 c.result=out?out:local;c.count=count;c.args=padded;((void(*)(Context*))address)(&c);
 if(c.vectors<=4)for(u32 i=0;i<c.vectors;i++)if(c.dest[i]){c.dest[i][0]=c.temp[i].x;c.dest[i][2]=c.temp[i].y;c.dest[i][4]=c.temp[i].z;}return c.result[0];
}
#define N(name,...) call(COOP_H_##name,sizeof((u64[]){0,##__VA_ARGS__})/8-1,((u64[]){0,##__VA_ARGS__})+1,0)
#define V(name,out,...) call(COOP_H_##name,sizeof((u64[]){0,##__VA_ARGS__})/8-1,((u64[]){0,##__VA_ARGS__})+1,out)
#define F(n) floating((float)(n))
#define P(p) pointer(p)
static u32 word(u64 p){return *(volatile u32*)p;}
static u64 wide(u64 p){return *(volatile u64*)p;}
static u64 lookup(u32 h){
 if(!h||(int32_t)h<0)return 0;u64 pool=wide(0x61a8250);if(!pool)return 0;u32 n=h>>8,count=word(pool+24),stride=word(pool+28);u64 entries=wide(pool+8),gen=wide(pool+16);
 if(!entries||!gen||n>=count||stride<16||stride>256||*(volatile u8*)(gen+n)!=(u8)h)return 0;return wide(entries+(u64)n*stride+8);
}
static u32 owner_ready(void){u64 o=glider_probe.owner;return o&&wide(o+0x8000)==0x315353504f4f43ULL&&word(o+0x8010)==2&&!word(o+0x8014)&&!word(o+0x8018)&&word(o+0x8208)==3&&!word(o+0x8218)&&!*(volatile u8*)0x5abefb8ULL;}
static u32 real_player(u32 h){u64 o=glider_probe.owner;return h&&(h==word(o+0x8050)||h==word(o+0x8054));}
static volatile GliderInput *helper(void){return (volatile GliderInput*)(GLIDER_COPY_BASE+4096);}
static u32 restore_table(void){
 volatile GliderInput *h=helper();if(h->magic!=GLIDER_HELPER_MAGIC||h->probe!=(u64)&glider_probe-8192)return 0;h->stopped=1;
 u64 v=lookup(glider_probe.vehicle);if(v&&v==glider_probe.entity){u64 expected=GLIDER_COPY_BASE+0xf010;
  if(wide(v)==expected&&!__atomic_compare_exchange_n((u64*)v,&expected,0x4057b48ULL,0,__ATOMIC_SEQ_CST,__ATOMIC_SEQ_CST))return 0;
  if(wide(v)!=0x4057b48ULL)return 0;
 }
 return !__atomic_load_n(&h->busy,__ATOMIC_ACQUIRE);
}
static u32 publish(u64 entity){
 volatile GliderInput *h=helper();if(h->magic!=GLIDER_HELPER_MAGIC||h->probe!=(u64)&glider_probe-8192||h->busy||!h->stopped)return 0;
 for(u32 at=0;at<GLIDER_TABLE_BYTES;at+=8){u64 expected=wide(0x4057b38ULL+at);if(at==16+0x3c0)expected=GLIDER_COPY_BASE;if(wide(GLIDER_COPY_BASE+0xf000+at)!=expected)return 0;}
 h->entity=entity;h->shadow=GLIDER_COPY_BASE+0xf010;h->calls=h->applied=h->rejected=h->reason=0;
 u64 expected=0x4057b48ULL;if(!__atomic_compare_exchange_n((u64*)entity,&expected,h->shadow,0,__ATOMIC_SEQ_CST,__ATOMIC_SEQ_CST))return 0;h->stopped=0;return 1;
}
static void sample(u32 end){u64 r[4]={0},v[4]={0};V(GET_ENTITY_ROTATION,r,glider_probe.vehicle,2);V(GET_ENTITY_VELOCITY,v,glider_probe.vehicle);
 if(glider_probe.entity){glider_probe.original[end?2:0]=(float)*(volatile u32*)(glider_probe.entity+0x30c);glider_probe.original[end?3:1]=*(volatile float*)(glider_probe.entity+0x310);}
 for(u32 n=0;n<3;n++)if(end){glider_probe.rotation_end[n]=as_float(r[n]);glider_probe.velocity_end[n]=as_float(v[n]);}else{glider_probe.rotation_start[n]=as_float(r[n]);glider_probe.velocity_start[n]=as_float(v[n]);}}
static void finish(void){if(glider_probe.model_owned){N(SET_MODEL_AS_NO_LONGER_NEEDED,glider_probe.model);glider_probe.model_owned=0;}glider_probe.state=3;glider_probe.completed=glider_probe.request;}
static void tick(void){
 u32 now=N(GET_GAME_TIMER),frame=N(GET_FRAME_COUNT);if(frame==glider_probe.last_frame)return;glider_probe.last_frame=frame;
 if(glider_probe.vehicle){
  u32 v=glider_probe.vehicle,p=glider_probe.pilot;
  if(real_player(p)||real_player(v)){glider_probe.error=10;return;}
  if(N(DOES_ENTITY_EXIST,v)){
   u32 driver=N(GET_PED_IN_VEHICLE_SEAT,v,-1,0);
   if((u32)N(GET_ENTITY_MODEL,v)!=glider_probe.model||(driver&&driver!=p)){glider_probe.error=11;return;}
   if(glider_probe.state==4&&!glider_probe.stop&&glider_probe.request!=glider_probe.completed&&glider_probe.request&&owner_ready()&&frame-glider_probe.started_frame<glider_probe.frames&&now-glider_probe.started<7000){glider_probe.steps++;return;}
   if(!restore_table()){glider_probe.error=12;return;}
   if(glider_probe.state==4){sample(1);glider_probe.restored++;glider_probe.state=2;}
   if(p&&N(DOES_ENTITY_EXIST,p)){if((u32)N(GET_ENTITY_MODEL,p)!=glider_probe.pilot_model){glider_probe.error=13;return;}N(DELETE_PED,P(&p));}
   if(N(DOES_ENTITY_EXIST,glider_probe.pilot))return;if(glider_probe.pilot){glider_probe.pilot=0;glider_probe.pilots_deleted++;}
   N(DELETE_VEHICLE,P(&v));
  }
  if(!N(DOES_ENTITY_EXIST,glider_probe.vehicle)){glider_probe.vehicle=0;glider_probe.deleted++;finish();}return;
 }
 if(!glider_probe.request||glider_probe.request==glider_probe.completed)return;
 if(glider_probe.stop||!owner_ready()){glider_probe.error=1;finish();return;}
 if(glider_probe.state!=1){
  glider_probe.error=0;glider_probe.created=glider_probe.deleted=glider_probe.pilots_created=glider_probe.pilots_deleted=glider_probe.restored=glider_probe.steps=0;
  u32 model=glider_probe.model;
  if((model!=884483972u)||glider_probe.mode>1||glider_probe.axis>4||glider_probe.value!=glider_probe.value||glider_probe.value>1||glider_probe.value<-1||glider_probe.frames<30||glider_probe.frames>180||!N(IS_MODEL_VALID,model)||!N(IS_MODEL_IN_CDIMAGE,model)){glider_probe.error=2;finish();return;}
  glider_probe.started=now;glider_probe.state=1;if(!N(HAS_MODEL_LOADED,model)){N(REQUEST_MODEL,model);glider_probe.model_owned=1;}
 }
 if(!N(HAS_MODEL_LOADED,glider_probe.model)){if(now-glider_probe.started>5000){glider_probe.error=3;finish();}return;}
 u32 v=N(CREATE_VEHICLE,glider_probe.model,F(-420),F(-580),F(350),F(0),0,0,0);if(!v){glider_probe.error=4;finish();return;}
 glider_probe.vehicle=v;glider_probe.created++;glider_probe.state=2;N(SET_ENTITY_AS_MISSION_ENTITY,v,1,1);N(FREEZE_ENTITY_POSITION,v,1);N(SET_ENTITY_VISIBLE,v,0,0);
 u64 entity=lookup(v);glider_probe.entity=entity;
 if(!entity||wide(entity)!=0x4057b48ULL||word(entity+0xbf8)!=11){glider_probe.error=5;return;}
 glider_probe.pilot_model=N(GET_ENTITY_MODEL,word(glider_probe.owner+0x8050));
 u32 p=N(CREATE_PED_INSIDE_VEHICLE,v,4,glider_probe.pilot_model,(u64)-1,0,0);if(!p||real_player(p)){glider_probe.error=6;return;}
 glider_probe.pilot=p;glider_probe.pilots_created++;N(SET_ENTITY_AS_MISSION_ENTITY,p,1,1);N(SET_BLOCKING_OF_NON_TEMPORARY_EVENTS,p,1);N(SET_ENTITY_VISIBLE,p,0,0);
 /* Keep the temporary test rider seated through intentionally steep axes;
  * an ejection would make the ownership guard reject later samples. */
 N(SET_PED_CAN_RAGDOLL,p,0);N(SET_ENTITY_INVINCIBLE,p,1);N(SET_PED_CAN_BE_KNOCKED_OFF_VEHICLE,p,1);
 for(u32 n=0;n<4;n++){glider_probe.original[n]=0;glider_probe.values[n]=0;}
 if(glider_probe.full_rotor)glider_probe.values[2]=1; /* Powered fixture profile. */
 if(glider_probe.axis<4)glider_probe.values[glider_probe.axis]=glider_probe.value;
 if(!publish(entity)){glider_probe.error=7;return;}
 N(SET_VEHICLE_ENGINE_ON,v,1,1,0);N(SET_GLIDER_ACTIVE,v,1);
 N(FREEZE_ENTITY_POSITION,v,0);N(ACTIVATE_PHYSICS,v);N(SET_VEHICLE_FORWARD_SPEED,v,F(glider_probe.full_rotor?40:10));sample(0);
 glider_probe.started=now;glider_probe.started_frame=frame;glider_probe.state=4;
}
__attribute__((section(".entry"),used)) void _start(Context *context){
 if(glider_probe.forward)((void(*)(Context*))glider_probe.forward)(context);
 if(glider_probe.magic!=GLIDER_PROBE_MAGIC)return;glider_probe.busy=1;tick();glider_probe.busy=0;
}
