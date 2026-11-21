/* Fixture-only stock hover arguments; no player identity or shared code
 * changes. The owned probe is the only accepted driver. (AI-assisted) */
#include "coop-oppressor2-probe-layout.h"
#include "coop-oppressor2-runtime-layout.h"
#include <stddef.h>
__attribute__((section(".flight.data"),used)) volatile OppressorInput oppressor_input;
static uint32_t word(uint64_t a){return *(volatile uint32_t*)a;}
static uint64_t wide(uint64_t a){return *(volatile uint64_t*)a;}
static uint64_t lookup(uint32_t h){
 if(!h||(int32_t)h<0)return 0;uint64_t p=wide(0x61a8250);if(!p)return 0;
 uint32_t n=h>>8,count=word(p+24),stride=word(p+28);uint64_t entries=wide(p+8),gen=wide(p+16);
 if(!entries||!gen||n>=count||stride<16||stride>256||*(volatile uint8_t*)(gen+n)!=(uint8_t)h)return 0;
 return wide(entries+(uint64_t)n*stride+8);
}
__attribute__((visibility("hidden"))) uint32_t oppressor_apply(uint64_t v){
 volatile OppressorInput *h=&oppressor_input;h->busy=1;h->calls++;
#ifdef GTA_OPPRESSOR_PRODUCTION
 if(h->magic!=OPPRESSOR_PRODUCTION_MAGIC||h->stopped||!h->probe||h->entity!=v){h->reason=1;goto reject;}
 volatile OppressorFlightRuntime *p=(volatile OppressorFlightRuntime*)h->probe;uint64_t o=p->owner;
 if(p->magic!=OPPRESSOR_RUNTIME_MAGIC||p->enabled!=1||p->status!=1||p->entity!=v||word(0x61fbd24)-p->frame>4){h->reason=2;goto reject;}
 if(!o||wide(o+0x8000)!=0x315353504f4f43ULL||word(o+0x8010)!=2||word(o+0x8014)||word(o+0x8018)||word(o+0x8208)!=3||word(o+0x8218)||!word(o+0x8074)||!word(o+0x80fc)||*(volatile uint8_t*)0x5abefb8ULL){h->reason=3;goto reject;}
 if(!p->pilot||p->pilot==word(o+0x8050)||p->pilot!=word(o+0x8054)||lookup(p->vehicle)!=v||wide(v)!=h->shadow||word(v+0xbf8)!=11||(word(o+0x85d4)!=2069146067u&&word(o+0x85d4)!=884483972u)){h->reason=4;goto reject;}
#else
 if(h->magic!=OPPRESSOR_HELPER_MAGIC||h->stopped||!h->probe||h->entity!=v){h->reason=1;goto reject;}
 volatile OppressorProbe *p=(volatile OppressorProbe*)(h->probe+8192);uint64_t o=p->owner;
 if(p->magic!=OPPRESSOR_PROBE_MAGIC||p->stop||p->state!=4||p->entity!=v||p->request==p->completed||!p->request||p->model!=2069146067u||word(0x61fbd24)-p->started_frame>=p->frames){h->reason=2;goto reject;}
 if(!o||wide(o+0x8000)!=0x315353504f4f43ULL||word(o+0x8010)!=2||word(o+0x8014)||word(o+0x8018)||word(o+0x8208)!=3||word(o+0x8218)||*(volatile uint8_t*)0x5abefb8ULL){h->reason=3;goto reject;}
 if(!p->pilot||p->pilot==word(o+0x8050)||p->pilot==word(o+0x8054)||lookup(p->vehicle)!=v||wide(v)!=h->shadow||word(v+0xbf8)!=11){h->reason=4;goto reject;}
#endif
 uint64_t pilot=lookup(p->pilot);int32_t seat=*(volatile int8_t*)(v+0xc71);uint32_t seats=*(volatile uint8_t*)(v+0xc70);
 if(!pilot||seat<0||(uint32_t)seat>=seats||wide(v+0xc78+(uint32_t)seat*8)!=pilot){h->reason=5;goto reject;}
 for(uint32_t n=0;n<4;n++){uint32_t bits=((volatile uint32_t*)p->values)[n];if((bits&0x7fffffffu)>0x3f800000u){h->reason=6;goto reject;}((volatile uint32_t*)h->after)[n]=bits;}
#ifndef GTA_OPPRESSOR_PRODUCTION
 if(p->mode>1){h->reason=7;goto reject;}
 if(!p->mode){h->reason=0;h->busy=0;return 0;}
#else
 if(!h->applied){((volatile uint32_t*)h->before)[0]=*(volatile uint8_t*)(v+0x9bc);((volatile uint32_t*)h->before)[2]=word(v+0x9b4);((volatile uint32_t*)h->before)[3]=word(v+0x9b8);}
#endif
 *(volatile uint32_t*)(v+0x9b4)=((volatile uint32_t*)p->values)[2];
 *(volatile uint32_t*)(v+0x9b8)=((volatile uint32_t*)p->values)[3];
#ifdef GTA_OPPRESSOR_PRODUCTION
 uint32_t handbrake=(word(o+0x80f0)&word(o+0x843c))!=0;
 ((volatile uint32_t*)h->before)[1]=handbrake;
 *(volatile uint8_t*)(v+0x9bc)=handbrake;
#else
 *(volatile uint8_t*)(v+0x9bc)=0;
#endif
 h->applied++;h->reason=0;h->busy=0;return 1;
reject:h->rejected++;h->busy=0;return 0;
}
