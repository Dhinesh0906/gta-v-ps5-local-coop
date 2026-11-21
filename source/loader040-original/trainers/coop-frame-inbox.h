#ifndef COOP_FRAME_INBOX_H
#define COOP_FRAME_INBOX_H
#include <stdint.h>
#include <stdatomic.h>
struct coop_inbox_frame {
    uint64_t source,token;
    uint32_t index,frame,serial,player,epoch;
};
struct coop_inbox_slot {_Atomic uint32_t state;struct coop_inbox_frame frame;};
struct coop_frame_inbox {uint32_t cursor,last_serial,has_last;struct coop_inbox_slot slots[3];};
static void inbox_reset(struct coop_frame_inbox *p){p->cursor=p->last_serial=p->has_last=0;for(unsigned n=0;n<3;n++)atomic_store(&p->slots[n].state,0);}
static int inbox_post(struct coop_frame_inbox *p,const struct coop_inbox_frame *f){
 for(unsigned pass=0;pass<2;pass++)for(unsigned k=0;k<3;k++){
  unsigned n=(p->cursor+k)%3;uint32_t state=pass?2:0;
  if(!atomic_compare_exchange_strong_explicit(&p->slots[n].state,&state,1,memory_order_acq_rel,memory_order_relaxed))continue;
  p->slots[n].frame=*f;p->cursor=(n+1)%3;atomic_store_explicit(&p->slots[n].state,2,memory_order_release);return 1;
 }
 return 0;
}
static int inbox_take_latest(struct coop_frame_inbox *p,struct coop_inbox_frame *f){
 int best=-1;uint32_t serial=0;
 for(unsigned n=0;n<3;n++){
  uint32_t ready=2;if(!atomic_compare_exchange_strong_explicit(&p->slots[n].state,&ready,3,memory_order_acq_rel,memory_order_relaxed))continue;
  if(best<0||(int32_t)(p->slots[n].frame.serial-serial)>0){
   if(best>=0)atomic_store_explicit(&p->slots[best].state,0,memory_order_release);
   best=(int)n;serial=p->slots[n].frame.serial;
  }else atomic_store_explicit(&p->slots[n].state,0,memory_order_release);
 }
 if(best<0)return 0;*f=p->slots[best].frame;atomic_store_explicit(&p->slots[best].state,0,memory_order_release);
 if(p->has_last&&(int32_t)(f->serial-p->last_serial)<=0)return 0;
 p->last_serial=f->serial;p->has_last=1;return 1;
}
#endif
