/* One bounded experiment body only; never a player-controlled entity.
 * Integer-only observer/correction before the stock four-axis load.
 * (AI-assisted) */
#include "coop-blimp-probe-layout.h"
#include <stddef.h>
#define SECTION(s) __attribute__((section(s),used,visibility("hidden")))
SECTION(".blimp.data") volatile BlimpInput blimp_input;
static uint32_t word(uint64_t p){return *(volatile uint32_t*)(uintptr_t)p;}
static uint64_t wide(uint64_t p){return *(volatile uint64_t*)(uintptr_t)p;}
static uint64_t lookup(uint32_t h){
 if(!h||(int32_t)h<0)return 0;uint64_t pool=wide(0x61a8250);if(!pool)return 0;
 uint32_t n=h>>8,count=word(pool+24),stride=word(pool+28);uint64_t entries=wide(pool+8),gen=wide(pool+16);
 if(!entries||!gen||n>=count||stride<16||stride>256||*(volatile uint8_t*)(gen+n)!=(uint8_t)h)return 0;return wide(entries+(uint64_t)n*stride+8);
}
SECTION(".text") void blimp_before_input(uint64_t v){
 if(blimp_input.magic!=BLIMP_HELPER_MAGIC||!blimp_input.probe||blimp_input.entity!=v)return;
 __atomic_add_fetch(&blimp_input.busy,1,__ATOMIC_ACQUIRE);blimp_input.calls++;uint32_t reason=0;
 volatile BlimpProbe *p=(volatile BlimpProbe*)(blimp_input.probe+8192);
 if(blimp_input.stopped||p->magic!=BLIMP_PROBE_MAGIC||p->stop||p->state!=4||!p->request||p->request==p->completed||p->mode>1){reason=1;goto rejected;}
 uint64_t owner=p->owner;
 if(!owner||wide(owner+0x8000)!=0x315353504f4f43ULL||word(owner+0x8010)!=2||word(owner+0x8014)||word(owner+0x8018)||word(owner+0x8208)!=3||word(owner+0x8218)||*(volatile uint8_t*)0x5abefb8ULL){reason=2;goto rejected;}
 if(!p->pilot||p->pilot==word(owner+0x8050)||p->pilot==word(owner+0x8054)||lookup(p->vehicle)!=v||wide(v)!=blimp_input.shadow||word(v+0xbf8)!=9){reason=3;goto rejected;}
 uint64_t ped=lookup(p->pilot);int32_t seat=*(volatile int8_t*)(v+0xc71);uint32_t seats=*(volatile uint8_t*)(v+0xc70);
 if(!ped||seat<0||(uint32_t)seat>=seats||wide(v+0xc78+(uint32_t)seat*8)!=ped){reason=4;goto rejected;}
 uint32_t desired[4];
 for(unsigned n=0;n<4;n++){desired[n]=((volatile uint32_t*)p->values)[n];if((desired[n]&0x7fffffffu)>0x3f800000u){reason=5;goto rejected;}}
 for(unsigned n=0;n<4;n++)((volatile uint32_t*)blimp_input.before)[n]=word(v+0x1a80+n*4);
 if(p->mode){for(unsigned n=0;n<4;n++)*(volatile uint32_t*)(v+0x1a80+n*4)=desired[n];blimp_input.applied++;}
 for(unsigned n=0;n<4;n++)((volatile uint32_t*)blimp_input.after)[n]=word(v+0x1a80+n*4);
 blimp_input.reason=0;__atomic_sub_fetch(&blimp_input.busy,1,__ATOMIC_RELEASE);return;
rejected:
 blimp_input.reason=reason;blimp_input.rejected++;__atomic_sub_fetch(&blimp_input.busy,1,__ATOMIC_RELEASE);
}
