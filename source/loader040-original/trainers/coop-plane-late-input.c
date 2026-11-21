/* Integer-only observer/correction for one explicitly owned test plane.
 * Private vtable/copies only; no primary-player or shared-code edits.
 * Called before stock engine and normal/VTOL physics input reads. (AI-assisted) */
#include <stdint.h>
#include <stddef.h>
#include "coop-plane-late-layout.h"
#define SECTION(s) __attribute__((section(s),used,visibility("hidden")))
SECTION(".plane.data") volatile PlaneLateInput plane_late;
SECTION(".plane.pedals") volatile PlanePedalLease plane_pedals;
typedef struct {uint32_t head,reserved[3],records[64][20];} PlaneLateTrace;
SECTION(".plane.trace") volatile PlaneLateTrace plane_late_trace;
_Static_assert(sizeof(PlaneLateTrace)<=8192,"Late plane trace allocation");
static uint32_t word(uint64_t p){return *(volatile uint32_t*)(uintptr_t)p;}
static uint64_t wide(uint64_t p){return *(volatile uint64_t*)(uintptr_t)p;}
static uint64_t lookup(uint32_t h){
 if(!h||(int32_t)h<0)return 0;uint64_t pool=wide(0x61a8250ull);if(!pool)return 0;
 uint32_t index=h>>8,count=word(pool+24),stride=word(pool+28);uint64_t entries=wide(pool+8),generations=wide(pool+16);
 if(!entries||!generations||index>=count||stride<16||stride>256||*(volatile uint8_t*)(uintptr_t)(generations+index)!=(uint8_t)h)return 0;
 return wide(entries+(uint64_t)index*stride+8);
}
static void record(void){
 uint32_t serial=__atomic_fetch_add(&plane_late_trace.head,1,__ATOMIC_RELAXED);
 volatile uint32_t *r=plane_late_trace.records[serial&63u];r[0]=0;
 r[1]=plane_late.frame;r[2]=plane_late.phase;r[3]=plane_late.vehicle;r[4]=plane_late.reason;
 for(unsigned n=0;n<4;n++){r[5+n]=plane_late.before[n];r[9+n]=plane_late.desired[n];r[13+n]=plane_late.after[n];}
 r[17]=plane_late.mode;r[18]=plane_late.pilot;r[19]=plane_late.remaining;
 __atomic_store_n(&r[0],serial+1u,__ATOMIC_RELEASE);
}
SECTION(".text") void plane_before_input(uint64_t vehicle,uint32_t phase){
 if(plane_late.magic!=0x3154504e494c5050ull||vehicle!=plane_late.entity||phase>2)return;
 __atomic_add_fetch(&plane_late.busy,1,__ATOMIC_ACQUIRE);plane_late.calls++;plane_late.phase=phase;plane_late.phases[phase]++;
 uint64_t owner=plane_late.owner;uint32_t reason=0;
 if(plane_late.stop||!plane_late.remaining||plane_late.mode>1){reason=1;goto rejected;}
 if(plane_late.remaining!=UINT32_MAX)plane_late.remaining--;
 if(!owner||wide(owner+0x8000)!=0x315353504f4f43ull||word(owner+0x8010)!=2||word(owner+0x8014)||word(owner+0x8018)||word(owner+0x8020)!=6||word(owner+0x8208)!=3||word(owner+0x8218)||*(volatile uint8_t*)(uintptr_t)0x5abefb8ull){reason=2;goto rejected;}
 uint32_t mode=word(owner+0x8948),pilot=plane_late.pilot,h=plane_late.vehicle;
 uint32_t fixture=(mode==5||mode==6)&&word(owner+0x8908)==4&&word(owner+0x8914)==h&&word(owner+0x89d0)==pilot&&word(owner+0x8900)&&word(owner+0x8900)!=word(owner+0x8904)&&pilot!=word(owner+0x8054);
 uint32_t live=word(owner+0x8b80)==1&&word(owner+0x8b84)==1&&word(owner+0x8b8c)==h&&wide(owner+0x8b90)==vehicle&&
  word(owner+0x8b00)==1&&word(owner+0x8b04)==1&&word(owner+0x8b0c)==h&&wide(owner+0x8b10)==vehicle&&word(owner+0x8450)==h&&pilot==word(owner+0x8054)&&
  word(owner+0x8074)&&word(owner+0x80fc)&&word(owner+0x8628)&&word(owner+0x844c)!=5;
 if((!fixture&&!live)||pilot==word(owner+0x8050)||lookup(h)!=vehicle||wide(vehicle)!=plane_late.shadow||word(vehicle+0xbf8)!=1){reason=3;goto rejected;}
 uint32_t frame=word(0x61fbd24ull);
 if(live&&frame-word(owner+0x8030)>4){reason=6;goto rejected;}
 uint64_t ped=lookup(pilot);int32_t seat=*(volatile int8_t*)(uintptr_t)(vehicle+0xc71);uint32_t seats=*(volatile uint8_t*)(uintptr_t)(vehicle+0xc70);
 if(!ped||seat<0||(uint32_t)seat>=seats||wide(vehicle+0xc78+(uint32_t)seat*8)!=ped){reason=4;goto rejected;}
 plane_late.frame=word(owner+0x8030);
 uint32_t publication=live?__atomic_load_n((uint32_t*)(uintptr_t)(owner+0x8b6c),__ATOMIC_ACQUIRE):0,desired[4];
 if(publication&1){reason=7;goto rejected;}
 for(unsigned n=0;n<4;n++){
  desired[n]=word(owner+(live?0x8b28:0x8980)+n*4);if((desired[n]&0x7fffffffu)>0x3f800000u){reason=5;goto rejected;}
 }
 if(live&&publication!=__atomic_load_n((uint32_t*)(uintptr_t)(owner+0x8b6c),__ATOMIC_ACQUIRE)){reason=7;goto rejected;}
 for(unsigned n=0;n<4;n++){plane_late.desired[n]=desired[n];plane_late.before[n]=word(vehicle+0x1ec8+n*4);}
 /* CPlane's dedicated input can be full while the ordinary vehicle pedal
  * fields still contain the NPC's half brake. Publish consistent pedals at
  * the same consumption points, only for the validated live P2 or owned test pilot. (AI-assisted) */
 if(plane_late.mode){
  if(plane_pedals.entity&&plane_pedals.entity!=vehicle){reason=8;goto rejected;}
  if(!plane_pedals.entity){plane_pedals.gas=word(vehicle+0x9b4);plane_pedals.brake=word(vehicle+0x9b8);plane_pedals.entity=vehicle;}
  uint32_t throttle=desired[3],positive=throttle&0x7fffffffu;
  plane_pedals.last_gas=(throttle&0x80000000u)?0:positive;plane_pedals.last_brake=(throttle&0x80000000u)?positive:0;
  *(volatile uint32_t*)(uintptr_t)(vehicle+0x9b4)=plane_pedals.last_gas;
  *(volatile uint32_t*)(uintptr_t)(vehicle+0x9b8)=plane_pedals.last_brake;
 }
 if(plane_late.mode){for(unsigned n=0;n<4;n++)*(volatile uint32_t*)(uintptr_t)(vehicle+0x1ec8+n*4)=plane_late.desired[n];plane_late.applied++;}
 for(unsigned n=0;n<4;n++)plane_late.after[n]=word(vehicle+0x1ec8+n*4);
 plane_late.reason=0;record();__atomic_sub_fetch(&plane_late.busy,1,__ATOMIC_RELEASE);return;
rejected:
 plane_late.reason=reason;plane_late.rejected++;__atomic_sub_fetch(&plane_late.busy,1,__ATOMIC_RELEASE);
}
