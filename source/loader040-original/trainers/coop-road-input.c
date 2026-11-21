#include <stdint.h>
#include <stddef.h>
#define SECT(s) __attribute__((section(s),used,visibility("hidden")))
struct road_input {
 uint64_t magic,forward,owner,entity,shadow,original_table;
 uint32_t mode,stop,busy,remaining,calls,applied,rejected,restored;
 uint32_t last_frame,ped,vehicle,reason;
 uint32_t steer_before,steer_after,gas_before,gas_after,brake_before,brake_after,handbrake,pad_x;
};
SECT(".road.data") volatile struct road_input road_mailbox;
struct road_trace{uint32_t head,reserved[3],records[64][15];};
SECT(".road.trace") volatile struct road_trace road_trace;
_Static_assert(sizeof(struct road_trace)<=4096,"Trace ring allocation");
SECT(".rodata") const uint32_t road_layout[]={sizeof(struct road_input),offsetof(struct road_input,mode),offsetof(struct road_input,calls),offsetof(struct road_input,steer_before)};
static uint32_t word(uint64_t p){return *(volatile uint32_t*)(uintptr_t)p;}
static uint64_t wide(uint64_t p){return *(volatile uint64_t*)(uintptr_t)p;}
static uint64_t lookup(uint32_t h){
 if(!h||(int32_t)h<0)return 0;uint64_t pool=wide(0x61a8250ull);if(!pool)return 0;
 uint32_t n=h>>8,count=word(pool+24),stride=word(pool+28);uint64_t entries=wide(pool+8),generations=wide(pool+16);
 if(!entries||!generations||n>=count||stride<16||stride>256||*(volatile uint8_t*)(uintptr_t)(generations+n)!=(uint8_t)h)return 0;
 return wide(entries+(uint64_t)n*stride+8);
}
static void restore(uint64_t v){
 uint64_t expected=road_mailbox.shadow;
 if(__atomic_compare_exchange_n((uint64_t*)(uintptr_t)v,&expected,road_mailbox.original_table,0,__ATOMIC_SEQ_CST,__ATOMIC_SEQ_CST))road_mailbox.restored=1;
}
/* Exact [0,255] trigger pressures converted to IEEE-754 without using SIMD
 * on the physics thread. All arithmetic below is on integer bit patterns. */
#include "coop-road-trigger-bits.h"
static uint32_t pressure(uint64_t owner,uint32_t binding,uint32_t held){
 if(binding==0x100)return road_trigger_bits[*(volatile uint8_t*)(uintptr_t)(owner+0x8110)];
 if(binding==0x200)return road_trigger_bits[*(volatile uint8_t*)(uintptr_t)(owner+0x8111)];
 return held&binding?0x3f800000u:0;
}
static void trace_sample(uint32_t phase){
 uint32_t serial=__atomic_fetch_add(&road_trace.head,1,__ATOMIC_RELAXED);
 volatile uint32_t *r=road_trace.records[serial&63u];r[0]=0;
 uint64_t owner=road_mailbox.owner;
 r[1]=road_mailbox.last_frame;r[2]=owner?word(owner+0x8030):0;r[3]=phase;r[4]=road_mailbox.vehicle;r[5]=road_mailbox.pad_x;
 r[6]=road_mailbox.steer_before;r[7]=road_mailbox.steer_after;r[8]=road_mailbox.gas_before;r[9]=road_mailbox.gas_after;
 r[10]=road_mailbox.brake_before;r[11]=road_mailbox.brake_after;r[12]=owner?word(owner+0x810c):0;r[13]=owner?word(owner+0x8104):0;r[14]=road_mailbox.reason;
 __atomic_store_n(&r[0],serial+1u,__ATOMIC_RELEASE);
}
/* Mode zero only observes. Mode one replaces the pedal/steering values
 * immediately before the remaining stock automobile simulation. Never alter
 * speed limits, velocity, handling, another driver, or any shared game code. */
SECT(".text") void road_after_input(uint64_t v,uint32_t phase){
 if(road_mailbox.magic!=0x3154504e49444152ull||v!=road_mailbox.entity)return;
 road_mailbox.busy=1;road_mailbox.calls++;
 if(road_mailbox.stop||!road_mailbox.remaining){restore(v);road_mailbox.busy=0;return;}
 if(road_mailbox.remaining!=UINT32_MAX)road_mailbox.remaining--;
 uint64_t owner=road_mailbox.owner;
 if(!owner||wide(owner+0x8000)!=0x315353504f4f43ull||word(owner+0x8010)!=2||word(owner+0x8014)||word(owner+0x8018)||word(owner+0x8020)!=6){road_mailbox.reason=1;goto reject;}
 road_mailbox.last_frame=word(0x61fbd24ull);road_mailbox.ped=word(owner+0x8054);road_mailbox.vehicle=word(owner+0x8450);
 if(word(owner+0x8208)!=3||lookup(road_mailbox.vehicle)!=v||road_mailbox.ped==word(owner+0x8050)){road_mailbox.reason=2;goto reject;}
 uint64_t p=lookup(road_mailbox.ped);int32_t seat=*(volatile int8_t*)(uintptr_t)(v+0xc71);uint32_t seats=*(volatile uint8_t*)(uintptr_t)(v+0xc70);
 if(!p||seat<0||(uint32_t)seat>=seats||wide(v+0xc78+(uint32_t)seat*8)!=p){road_mailbox.reason=3;goto reject;}
 road_mailbox.steer_before=road_mailbox.steer_after=word(v+0x9ac);
 road_mailbox.gas_before=road_mailbox.gas_after=word(v+0x9b4);
 road_mailbox.brake_before=road_mailbox.brake_after=word(v+0x9b8);
 road_mailbox.handbrake=*(volatile uint8_t*)(uintptr_t)(v+0x9bc);road_mailbox.pad_x=word(owner+0x8100);
 if(road_mailbox.mode>1){road_mailbox.reason=4;goto reject;}
 if(road_mailbox.mode==1){
  if(word(owner+0x8218)||!word(owner+0x8074)||!word(owner+0x80fc)||(road_mailbox.last_frame-word(owner+0x8030))>4){road_mailbox.reason=5;goto reject;}
  uint32_t stick=road_mailbox.pad_x,magnitude=stick&0x7fffffffu;
  if(magnitude>0x3f800000u){road_mailbox.reason=6;goto reject;}
  /* A half-radian at full stick is bounded below the stock full AI turn;
   * physics continues to apply the vehicle's existing handling and tyres. */
  uint32_t steering=magnitude>=0x00800000u?((magnitude-0x00800000u)|((stick^0x80000000u)&0x80000000u)):0;
  uint32_t held=word(owner+0x80f0),gas=pressure(owner,word(owner+0x8434),held),brake=pressure(owner,word(owner+0x8438),held);
  *(volatile uint32_t*)(uintptr_t)(v+0x9a8)=0;
  *(volatile uint32_t*)(uintptr_t)(v+0x9ac)=steering;
  if(!brake){
   *(volatile uint32_t*)(uintptr_t)(v+0x9b4)=gas;
   *(volatile uint32_t*)(uintptr_t)(v+0x9b8)=0;
  }
  *(volatile uint8_t*)(uintptr_t)(v+0x9bc)=word(owner+0x845c)==6;
  road_mailbox.steer_after=word(v+0x9ac);road_mailbox.gas_after=word(v+0x9b4);road_mailbox.brake_after=word(v+0x9b8);road_mailbox.applied++;
 }
 road_mailbox.reason=0;trace_sample(phase);road_mailbox.busy=0;return;
reject:
 road_mailbox.rejected++;trace_sample(phase);road_mailbox.busy=0;
}
