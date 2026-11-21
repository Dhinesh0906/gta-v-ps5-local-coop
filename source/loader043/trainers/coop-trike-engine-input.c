/* Fixture-only trike_engine input and stock airborne-bike branch. (AI-assisted) */
#include "coop-trike-engine-probe-layout.h"
__attribute__((section(".flight.data"),used)) volatile TrikeEngineInput trike_engine_input;
#include "coop-motorcycle-catalogue.h"
static unsigned allowed_model(unsigned model){for(unsigned n=0;n<sizeof(motorcycle_models)/sizeof(motorcycle_models[0]);n++)if(motorcycle_models[n]==model)return 1;return 0;}
static uint32_t word(uint64_t a){return *(volatile uint32_t*)a;}
static uint64_t wide(uint64_t a){return *(volatile uint64_t*)a;}
static uint64_t lookup(uint32_t h){
 if(!h||(int32_t)h<0)return 0;uint64_t p=wide(0x61a8250);if(!p)return 0;
 uint32_t n=h>>8,count=word(p+24),stride=word(p+28);uint64_t entries=wide(p+8),gen=wide(p+16);
 if(!entries||!gen||n>=count||stride<16||stride>256||*(volatile uint8_t*)(gen+n)!=(uint8_t)h)return 0;
 return wide(entries+(uint64_t)n*stride+8);
}
__attribute__((section(".flight.trace"),used)) volatile TrikeEngineTrace trike_engine_trace;
static void trike_engine_record(uint64_t v,uint32_t phase){
 uint32_t serial=__atomic_fetch_add(&trike_engine_trace.head,1,__ATOMIC_RELAXED);
 volatile uint32_t *r=trike_engine_trace.records[serial&63u];r[0]=0;
 r[1]=word(0x61fbd24);r[2]=phase;r[3]=word(v+0x9ac);r[4]=word(v+0x9b4);r[5]=word(v+0x9b8);
 r[6]=*(volatile uint8_t*)(v+0x9bc);r[7]=word(v+0x958);r[8]=word(v+0xa70);r[9]=word(v+0x1070);
 r[10]=word(trike_engine_input.probe+8192+116);r[11]=word(trike_engine_input.probe+8192+48);
 __atomic_store_n(&r[0],serial+1u,__ATOMIC_RELEASE);
}
__attribute__((visibility("hidden"))) uint32_t trike_engine_apply(uint64_t v,uint32_t phase){
 volatile TrikeEngineInput *h=&trike_engine_input;h->busy=1;h->calls++;
 if(h->magic!=TRIKE_ENGINE_HELPER_MAGIC||h->stopped||!h->probe||h->entity!=v){h->reason=1;goto reject;}
 volatile TrikeEngineProbe *p=(volatile TrikeEngineProbe*)(h->probe+8192);uint64_t o=p->owner;
 if(p->magic!=TRIKE_ENGINE_PROBE_MAGIC||p->stop||p->state!=4||p->entity!=v||p->request==p->completed||!p->request||!allowed_model(p->model)||word(0x61fbd24)-p->started_frame>=p->frames){h->reason=2;goto reject;}
 if(!o||wide(o+0x8000)!=0x315353504f4f43ULL||word(o+0x8010)!=2||word(o+0x8014)||word(o+0x8018)||word(o+0x8208)!=3||word(o+0x8218)||*(volatile uint8_t*)0x5abefb8ULL){h->reason=3;goto reject;}
 if(!p->pilot||p->pilot==word(o+0x8050)||p->pilot==word(o+0x8054)||lookup(p->vehicle)!=v||wide(v)!=h->shadow||word(v+0xbf8)!=3){h->reason=4;goto reject;}
 uint64_t pilot=lookup(p->pilot);int32_t seat=*(volatile int8_t*)(v+0xc71);uint32_t seats=*(volatile uint8_t*)(v+0xc70);
 if(!pilot||seat<0||(uint32_t)seat>=seats||wide(v+0xc78+(uint32_t)seat*8)!=pilot){h->reason=5;goto reject;}
 for(uint32_t n=0;n<4;n++){uint32_t bits=((volatile uint32_t*)p->values)[n];if((bits&0x7fffffffu)>0x3f800000u){h->reason=6;goto reject;}((volatile uint32_t*)h->after)[n]=bits;}
 if(p->mode>1){h->reason=7;goto reject;}
 if(!p->mode){h->reason=0;h->busy=0;return 0;}
 if(phase>2){h->reason=8;goto reject;}
 if(!phase){
 *(volatile uint32_t*)(v+0x9a8)=0;
 *(volatile uint32_t*)(v+0x9ac)=0;
 *(volatile uint32_t*)(v+0x9b4)=((volatile uint32_t*)p->values)[2];
 *(volatile uint32_t*)(v+0x9b8)=((volatile uint32_t*)p->values)[3];
 *(volatile uint8_t*)(v+0x9bc)=0;
 }
 trike_engine_record(v,phase);
 h->applied++;h->reason=0;h->busy=0;return 1;
reject:h->rejected++;h->busy=0;return 0;
}
