/* Short metadata leases between a compositor worker and the flip thread.
 * Pixel writes and external calls never occur while holding this gate. */
#ifndef COOP_ASYNC_OUTPUT_H
#define COOP_ASYNC_OUTPUT_H
#include <stdint.h>
#include <stdatomic.h>
enum {COOP_ASYNC_FREE,COOP_ASYNC_WRITING,COOP_ASYNC_READY};
struct coop_async_outputs {
    _Atomic uint32_t gate;
    uint32_t latest,state[3];
    uint64_t tail[3];
};
static int async_lock(struct coop_async_outputs *p){uint32_t free=0;return atomic_compare_exchange_strong_explicit(&p->gate,&free,1,memory_order_acq_rel,memory_order_relaxed);}
static void async_unlock(struct coop_async_outputs *p){atomic_store_explicit(&p->gate,0,memory_order_release);}
/* Caller must own the stopped/idle worker before configuring a new epoch. */
static void async_output_reset(struct coop_async_outputs *p){p->latest=3;for(unsigned n=0;n<3;n++){p->state[n]=COOP_ASYNC_FREE;p->tail[n]=0;}atomic_store(&p->gate,0);}
static int async_output_begin(struct coop_async_outputs *p,uint64_t completed,uint32_t current){
    if(!async_lock(p))return -2;
    int selected=-1;
    for(unsigned n=0;n<3;n++)if(n!=p->latest&&p->state[n]!=COOP_ASYNC_WRITING&&
        (!p->tail[n]||(completed>p->tail[n]&&current!=n+3u))){p->state[n]=COOP_ASYNC_WRITING;selected=(int)n;break;}
    async_unlock(p);return selected;
}
static int async_output_finish(struct coop_async_outputs *p,unsigned slot,int publish){
    if(slot>=3)return 0;if(!async_lock(p))return -1;
    if(p->state[slot]!=COOP_ASYNC_WRITING){async_unlock(p);return 0;}
    p->state[slot]=publish?COOP_ASYNC_READY:COOP_ASYNC_FREE;
    if(publish)p->latest=slot;
    async_unlock(p);return 1;
}
static int async_output_reserve(struct coop_async_outputs *p,uint64_t tail){
    if(!tail)return -1;if(!async_lock(p))return -2;
    uint32_t slot=p->latest;
    if(slot>=3||p->state[slot]!=COOP_ASYNC_READY||tail<p->tail[slot]){async_unlock(p);return -1;}
    p->tail[slot]=tail;async_unlock(p);return (int)slot;
}
#endif
