#include "coop-view-pair.h"
static int same_config(const struct coop_view_pair *p,const struct coop_pair_lease *l){
    return !(l->sequence&1u)&&atomic_load_explicit(&p->sequence,memory_order_acquire)==l->sequence&&p->epoch==l->epoch;
}
static int valid_lease(struct coop_view_pair *p,const struct coop_pair_lease *l,uint32_t state){
    return l->slot<4&&same_config(p,l)&&atomic_load_explicit(&p->slots[l->slot].state,memory_order_acquire)==state&&
        atomic_load_explicit(&p->slots[l->slot].generation,memory_order_relaxed)==l->generation;
}
int coop_pair_configure(struct coop_view_pair *p,uint32_t epoch,const uint64_t cameras[2],const uint64_t addresses[4]){
    if(!p||!epoch||!cameras||!addresses||!cameras[0]||!cameras[1]||cameras[0]==cameras[1])return COOP_PAIR_BAD_INPUT;
    for(uint32_t n=0;n<4;n++){
        if(!addresses[n]||(addresses[n]&63u)||addresses[n]>UINT64_MAX-COOP_PAIR_FRAME_BYTES)return COOP_PAIR_BAD_INPUT;
        for(uint32_t k=0;k<n;k++)if(addresses[n]<addresses[k]+COOP_PAIR_FRAME_BYTES&&addresses[k]<addresses[n]+COOP_PAIR_FRAME_BYTES)return COOP_PAIR_BAD_INPUT;
    }
    uint32_t seq=atomic_load_explicit(&p->sequence,memory_order_acquire);
    if((seq&1u)||!atomic_compare_exchange_strong_explicit(&p->sequence,&seq,seq+1u,memory_order_acq_rel,memory_order_acquire))return COOP_PAIR_BUSY;
    for(uint32_t n=0;n<4;n++){
        uint32_t state=atomic_load_explicit(&p->slots[n].state,memory_order_acquire);
        if(state==COOP_PAIR_WRITING||state==COOP_PAIR_HELD){atomic_store_explicit(&p->sequence,seq,memory_order_release);return COOP_PAIR_BUSY;}
    }
    if(p->epoch==epoch){atomic_store_explicit(&p->sequence,seq,memory_order_release);return COOP_PAIR_BAD_INPUT;}
    p->epoch=epoch;p->cameras[0]=cameras[0];p->cameras[1]=cameras[1];
    for(uint32_t n=0;n<4;n++){
        p->slots[n].address=addresses[n];p->slots[n].frame=(struct coop_pair_frame){0};
        atomic_store_explicit(&p->slots[n].state,COOP_PAIR_FREE,memory_order_release);
    }
    atomic_store_explicit(&p->cursor[0],0,memory_order_relaxed);atomic_store_explicit(&p->cursor[1],0,memory_order_relaxed);
    atomic_store_explicit(&p->sequence,seq+2u,memory_order_release);return COOP_PAIR_OK;
}
int coop_pair_begin_write(struct coop_view_pair *p,uint32_t player,struct coop_pair_lease *l){
    if(!p||!l||player<1||player>2)return COOP_PAIR_BAD_INPUT;
    uint32_t seq=atomic_load_explicit(&p->sequence,memory_order_acquire);if((seq&1u)||!p->epoch)return COOP_PAIR_WAIT;
    uint32_t epoch=p->epoch,base=(player-1u)*2u,start=atomic_fetch_add_explicit(&p->cursor[player-1u],1,memory_order_relaxed)&1u;
    for(uint32_t pass=0;pass<2;pass++)for(uint32_t n=0;n<2;n++){
        uint32_t index=base+((start+n)&1u),state=pass?COOP_PAIR_READY:COOP_PAIR_FREE;
        if(!atomic_compare_exchange_strong_explicit(&p->slots[index].state,&state,COOP_PAIR_WRITING,memory_order_acq_rel,memory_order_acquire))continue;
        if(atomic_load_explicit(&p->sequence,memory_order_acquire)!=seq||p->epoch!=epoch){atomic_store_explicit(&p->slots[index].state,COOP_PAIR_FREE,memory_order_release);return COOP_PAIR_WAIT;}
        uint32_t generation=atomic_fetch_add_explicit(&p->slots[index].generation,1,memory_order_relaxed)+1u;
        *l=(struct coop_pair_lease){index,generation,epoch,seq};return COOP_PAIR_OK;
    }
    return COOP_PAIR_BUSY;
}
int coop_pair_publish(struct coop_view_pair *p,const struct coop_pair_lease *l,const struct coop_pair_frame *f){
    if(!p||!l||!f)return COOP_PAIR_BAD_INPUT;
    if(!valid_lease(p,l,COOP_PAIR_WRITING))return COOP_PAIR_STALE_LEASE;
    uint32_t player=l->slot/2u+1u;
    if(f->epoch!=l->epoch||f->player!=player||f->camera!=p->cameras[player-1u]||!f->gpu_token||f->gpu_token==UINT64_MAX||
       (f->witnesses&COOP_PAIR_WITNESSES)!=COOP_PAIR_WITNESSES)return COOP_PAIR_BAD_INPUT;
    p->slots[l->slot].frame=*f;atomic_store_explicit(&p->slots[l->slot].state,COOP_PAIR_READY,memory_order_release);return COOP_PAIR_OK;
}
int coop_pair_abort_write(struct coop_view_pair *p,const struct coop_pair_lease *l){
    if(!p||!l)return COOP_PAIR_BAD_INPUT;if(!valid_lease(p,l,COOP_PAIR_WRITING))return COOP_PAIR_STALE_LEASE;
    atomic_store_explicit(&p->slots[l->slot].state,COOP_PAIR_FREE,memory_order_release);return COOP_PAIR_OK;
}
static void release_slot(struct coop_view_pair *p,uint32_t index){atomic_store_explicit(&p->slots[index].state,COOP_PAIR_READY,memory_order_release);}
int coop_pair_acquire(struct coop_view_pair *p,uint32_t now,uint32_t max_age,uint32_t max_skew,struct coop_pair *pair){
    if(!p||!pair||max_age>16u||max_skew>max_age)return COOP_PAIR_BAD_INPUT;
    uint32_t seq=atomic_load_explicit(&p->sequence,memory_order_acquire);if((seq&1u)||!p->epoch)return COOP_PAIR_WAIT;
    uint32_t held[4],count=0,best[2]={4,4},age[2]={UINT32_MAX,UINT32_MAX};
    for(uint32_t n=0;n<4;n++){uint32_t ready=COOP_PAIR_READY;if(atomic_compare_exchange_strong_explicit(&p->slots[n].state,&ready,COOP_PAIR_HELD,memory_order_acq_rel,memory_order_acquire))held[count++]=n;}
    if(atomic_load_explicit(&p->sequence,memory_order_acquire)!=seq){for(uint32_t n=0;n<count;n++)release_slot(p,held[n]);return COOP_PAIR_WAIT;}
    for(uint32_t n=0;n<count;n++){
        uint32_t index=held[n],view=index/2u;const struct coop_pair_frame *f=&p->slots[index].frame;uint32_t delta=now-f->frame;
        if(f->epoch==p->epoch&&f->player==view+1u&&f->camera==p->cameras[view]&&(f->witnesses&COOP_PAIR_WITNESSES)==COOP_PAIR_WITNESSES&&delta<=max_age&&delta<age[view]){best[view]=index;age[view]=delta;}
    }
    uint32_t skew=age[0]>age[1]?age[0]-age[1]:age[1]-age[0];
    if(best[0]==4||best[1]==4||skew>max_skew){for(uint32_t n=0;n<count;n++)release_slot(p,held[n]);return COOP_PAIR_WAIT;}
    for(uint32_t n=0;n<count;n++)if(held[n]!=best[0]&&held[n]!=best[1])release_slot(p,held[n]);
    for(uint32_t view=0;view<2;view++){
        struct coop_pair_slot *s=&p->slots[best[view]];pair->lease[view]=(struct coop_pair_lease){best[view],atomic_load_explicit(&s->generation,memory_order_relaxed),p->epoch,seq};
        pair->address[view]=s->address;pair->frame[view]=s->frame;
    }
    return COOP_PAIR_OK;
}
int coop_pair_release(struct coop_view_pair *p,const struct coop_pair *pair){
    if(!p||!pair)return COOP_PAIR_BAD_INPUT;
    for(uint32_t n=0;n<2;n++)if(!valid_lease(p,&pair->lease[n],COOP_PAIR_HELD))return COOP_PAIR_STALE_LEASE;
    if(pair->lease[0].slot/2u!=0||pair->lease[1].slot/2u!=1)return COOP_PAIR_BAD_INPUT;
    release_slot(p,pair->lease[0].slot);release_slot(p,pair->lease[1].slot);return COOP_PAIR_OK;
}
