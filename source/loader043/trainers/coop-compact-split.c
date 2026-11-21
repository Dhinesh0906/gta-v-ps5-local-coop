/* Single-render-thread 1080p split output. No background threads. GPU fence
 * association occurs AFTER GTA submits its DCB, in the Present-return hook.
 * A later actually displayed tag, not a guessed queue count, retires a slot. */
#include "coop-frame-capture.h"
#include "coop-compact-pixels.h"
#include "coop-tagged-output.h"
#include "coop-display-preference.h"
#ifdef COOP_COMPACT_P2_RETICLE
#include "coop-aim-reticle.h"
#endif
static void compact_observe_submission(const struct coop_present_snapshot*);
static void compact_pace(void);
static int camera_ready(void);
static void compact_capture(volatile struct coop_frame_capture_mailbox*,const struct coop_present_snapshot*,const struct coop_present_record*,unsigned);
#define COOP_COMPACT_PIPELINE
#include "coop-frame-capture.c"
#define SECT(s) __attribute__((section(s),used,visibility("hidden")))
#define COMPACT_MAGIC 0x3154504d4f435053ull
#define COMPACT_ALLOC_BYTES (5ull*COOP_COMPACT_OUTPUT_BYTES)
struct compact_buffer {uint64_t data,reserved[3];};
struct compact_split {
 uint64_t magic,forward,owner,camera,flip_forward;
 uint64_t register_fn,unregister_fn,status_fn,video_start,video_end;
 uint32_t state,error,stop,busy,enabled,handle,cookie,sequence;
 uint32_t p1,p2,epoch,registered,mapped,physical_owned,latest,composed_frame;
 uint64_t physical,address,bytes,attribute[10];
 struct compact_buffer buffers[5];struct coop_tagged_slot slots[5];
 uint32_t pending_slot,pending_ticket,pending_frame,last_owned,last_associated;
 uint32_t have[2],view_frame[2],copied[2],composed,flips,scanout,original,waits,association_errors;
 uint64_t last_token,last_tag,copy_ticks,compose_ticks,display_count;
 uint32_t current_buffer,last_render_frame,setup_result,cleanup_result,tag_only,tag_seen,verify_copy;
 uint32_t period_us,queue_count,pending_count,max_pending,held_frames,displayed_updates[2],displayed_frame[2],output_frames[5][2];
 uint64_t next_present_us,sleep_us;
 uint32_t history_remaining,history_resets,history_before_zero,history_before_one,history_error;
};
SECT(".compact.data") volatile struct compact_split compact;
SECT(".rodata") const uint32_t compact_layout[]={
 sizeof(struct compact_split),offsetof(struct compact_split,state),offsetof(struct compact_split,error),offsetof(struct compact_split,stop),
 offsetof(struct compact_split,busy),offsetof(struct compact_split,enabled),offsetof(struct compact_split,handle),offsetof(struct compact_split,cookie),
 offsetof(struct compact_split,p1),offsetof(struct compact_split,p2),offsetof(struct compact_split,registered),offsetof(struct compact_split,mapped),
 offsetof(struct compact_split,physical_owned),offsetof(struct compact_split,latest),offsetof(struct compact_split,physical),offsetof(struct compact_split,address),
 offsetof(struct compact_split,slots),offsetof(struct compact_split,pending_ticket),offsetof(struct compact_split,last_owned),offsetof(struct compact_split,last_associated),
 offsetof(struct compact_split,have),offsetof(struct compact_split,view_frame),offsetof(struct compact_split,copied),offsetof(struct compact_split,composed),
 offsetof(struct compact_split,flips),offsetof(struct compact_split,scanout),offsetof(struct compact_split,waits),offsetof(struct compact_split,association_errors),
 offsetof(struct compact_split,last_token),offsetof(struct compact_split,last_tag),offsetof(struct compact_split,copy_ticks),offsetof(struct compact_split,compose_ticks),
 offsetof(struct compact_split,display_count),offsetof(struct compact_split,current_buffer),offsetof(struct compact_split,last_render_frame),
 offsetof(struct compact_split,setup_result),offsetof(struct compact_split,cleanup_result),offsetof(struct compact_split,tag_only),offsetof(struct compact_split,tag_seen),offsetof(struct compact_split,verify_copy),
 offsetof(struct compact_split,period_us),offsetof(struct compact_split,queue_count),offsetof(struct compact_split,pending_count),offsetof(struct compact_split,max_pending),offsetof(struct compact_split,held_frames),offsetof(struct compact_split,displayed_updates),offsetof(struct compact_split,next_present_us),offsetof(struct compact_split,sleep_us),
 offsetof(struct compact_split,history_remaining),offsetof(struct compact_split,history_resets),offsetof(struct compact_split,history_before_zero),offsetof(struct compact_split,history_before_one),offsetof(struct compact_split,history_error)};
static uint64_t ptr(const volatile void *p){uint64_t a=(uint64_t)p;__asm__ volatile("":"+r"(a));return a;}
static void setword(uint64_t p,uint32_t v){__atomic_store_n((uint32_t*)(uintptr_t)p,v,__ATOMIC_RELEASE);}
static uint64_t tagged(uint32_t ticket){return coop_output_tag(compact.cookie,ticket);}
static int fence_done(uint64_t token){
 if(!token||token==UINT64_MAX||(token>>56)>=8||loadq(CAPTURE_LABELS)!=capture_mailbox.expected_labels)return 0;
 return (loadq(capture_mailbox.expected_labels+(token>>56)*32u)&COOP_FENCE_MASK)>=(token&COOP_FENCE_MASK);
}
static int status(uint64_t *s){
 int result=((int(*)(int,void*))compact.status_fn)((int)compact.handle,s);
 if(!result){compact.last_tag=s[3];compact.display_count=s[0];compact.current_buffer=*(uint32_t*)((unsigned char*)s+56);
  compact.queue_count=*(uint32_t*)((unsigned char*)s+48);compact.pending_count=*(uint32_t*)((unsigned char*)s+52);
  if(compact.pending_count>compact.max_pending)compact.max_pending=compact.pending_count;
  if((uint32_t)(s[3]>>32)==compact.cookie&&(uint32_t)s[3]){
   compact.tag_seen++;
   if(compact.current_buffer>=3&&compact.current_buffer<8)for(unsigned p=0;p<2;p++){
    uint32_t frame=compact.output_frames[compact.current_buffer-3u][p],delta=frame-compact.displayed_frame[p];
    if(frame&&delta&&delta<0x80000000u){compact.displayed_frame[p]=frame;compact.displayed_updates[p]++;}
   }
  }
  if(compact.current_buffer>=3&&compact.current_buffer<8)compact.scanout++;else if(compact.current_buffer<3)compact.original++;
 }
 return result;
}
/* Preserve headroom below a 60 Hz scanout. Faster CPU capture must not turn
 * original Present into an unpaced producer after its buffers stop scanning.
 * No worker, spin loop, or guessed queue-completion condition is used. */
static void compact_pace(void){
 if(compact.magic!=COMPACT_MAGIC||compact.state!=1||!compact.enabled||compact.stop||compact.tag_only||!camera_ready()){
  compact.next_present_us=0;return;
 }
 /* Bounded diagnostic only. This exact byte is initialized to zero by the
  * temporal pass, gates its previous-frame path, then becomes one after its
  * history ping-pong swap. Clear it on the validated Present thread so the
  * next pass takes the engine's normal first-frame path. Never patch code or
  * invent a texture. With the budget exhausted the engine resumes normal
  * history automatically. The default budget is zero. */
 if(compact.history_remaining){
  volatile uint8_t *ready=(volatile uint8_t*)(uintptr_t)0x4628f59ull;
  uint8_t before=*ready;
  if(before>1||capture_mailbox.thread_changes||!capture_mailbox.render_context){compact.history_error=1;compact.history_remaining=0;}
  else{
   if(before)compact.history_before_one++;else compact.history_before_zero++;
   *ready=0;compact.history_resets++;if(compact.history_remaining!=UINT32_MAX)compact.history_remaining--;
  }
 }
 uint32_t period=compact.period_us;if(period<16000||period>33333)return;
 uint64_t now=((uint64_t(*)(void))0x37bd370ull)(),deadline=compact.next_present_us;
 if(deadline>now&&deadline-now<=period){
  uint32_t wait=(uint32_t)(deadline-now);((int(*)(unsigned))0x37ba740ull)(wait);compact.sleep_us+=wait;
  now=((uint64_t(*)(void))0x37bd370ull)();
 }
 compact.next_present_us=now+period;
}
static void fault(uint32_t error){uint32_t empty=0;__atomic_compare_exchange_n(&compact.error,&empty,error,0,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED);compact.enabled=0;compact.state=2;setword(compact.camera+32,1);}
static int camera_ready(void){return loadl(compact.camera+24)==3&&!loadl(compact.camera+28)&&loadl(compact.camera+64)&&loadl(compact.camera+140)==2;}
static int64_t fixed(uint32_t bits){uint32_t exponent=(bits>>23)&255u;uint64_t m=(bits&0x7fffffu)|(exponent?0x800000u:0);int shift=(int)exponent-134;uint64_t v=shift>=0?m<<(unsigned)shift:(-shift<64?m>>(unsigned)-shift:0);return bits>>31?-(int64_t)v:(int64_t)v;}
static int near_position(uint32_t a,uint32_t b){if((a&0x7fffffffu)>0x471c4000u||(b&0x7fffffffu)>0x471c4000u)return 0;int64_t d=fixed(a)-fixed(b);if(d<0)d=-d;return d<=1310;}
static uint32_t identify(const struct coop_render_view *view,uint32_t frame){
 uint64_t a=compact.camera;uint32_t head=loadl(a+160),count=loadl(a+164),cam=loadl(a+60),mask=0;
 if(!camera_ready()||!coop_render_view_valid(view)||count>128)return 0;
 for(uint32_t n=0;n<count;n++){
  uint64_t s=a+168+((head-n-1u)&127u)*24u;uint32_t delta=loadl(s+4)-frame,which=loadl(s+20);
  if((delta>4&&delta<0xfffffffcu)||(which!=0xffffffffu&&which!=cam))continue;
  int match=1;for(unsigned k=0;k<3;k++)if(!near_position(loadl(s+8+k*4),view->pose[12+k])){match=0;break;}
  if(match)mask|=which==0xffffffffu?1:2;
 }
 if(head!=loadl(a+160))return 0;return mask==1?1:mask==2?2:0;
}
static void compact_observe_submission(const struct coop_present_snapshot *s){
 if(compact.magic!=COMPACT_MAGIC||!compact.pending_ticket)return;
 uint64_t token=s->tokens[(s->write_serial-1u)&1u];uint32_t ticket=compact.pending_ticket,slot=compact.pending_slot;
 if(s->frame-1u!=compact.pending_frame||slot>=5||!token||token==UINT64_MAX||(token>>56)>=8||!(token&COOP_FENCE_MASK)){
  compact.association_errors++;fault(21);return;
 }
 if(compact.slots[slot].ticket!=ticket){compact.association_errors++;fault(22);return;}
 compact.slots[slot].token=token;__atomic_store_n(&compact.slots[slot].associated,ticket,__ATOMIC_RELEASE);
 if(ticket==compact.last_owned){compact.last_token=token;compact.last_associated=ticket;}
 compact.pending_ticket=0;
}
#ifdef COOP_COMPACT_P2_RETICLE
/* The controller already resolves configured aim/attack buttons and the
 * current weapon. Never read the pad again or change its combat target. */
static __attribute__((noinline)) int compact_p2_aim(uint32_t target[3]){
 uint64_t owner=compact.owner,c=owner+0x8400;uint32_t index=loadl(c+228),count=loadl(c+224),held=loadl(owner+0x80f0);
 if(loadl(owner+0x8218)||!loadl(owner+0x80fc)||loadl(c+80)||index>=count||index>=32||loadl(c+304+index*4)>5||
  (held&loadl(c+16+6*4))||!(held&(loadl(c+16+4*4)|loadl(c+16+3*4))))return 0;
 for(unsigned k=0;k<3;k++)target[k]=loadl(c+288+k*4);return 1;
}
#endif
static void compact_capture(volatile struct coop_frame_capture_mailbox *m,const struct coop_present_snapshot *s,const struct coop_present_record *records,unsigned source){
 if(compact.magic!=COMPACT_MAGIC||compact.state!=1||!compact.enabled||compact.stop||!camera_ready())return;
 uint32_t free=0;if(!__atomic_compare_exchange_n(&compact.busy,&free,1,0,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED))return;
 compact.last_render_frame=s->frame;
 if(m->thread_changes||!m->render_context||loadq(CAPTURE_DEVICE)!=m->expected_device||m->data_bytes!=3u*COOP_COMPACT_VIEW_BYTES||
    !m->data||(m->data&63u)||m->data>UINT64_MAX-m->data_bytes||source>=3||loadl(CAPTURE_BUFFER_COUNT)!=3||
    loadl(CAPTURE_WIDTH)!=3840||loadl(CAPTURE_HEIGHT)!=2160||*(volatile unsigned char*)CAPTURE_FORMAT!=24){fault(23);goto done;}
 for(unsigned n=0;n<3;n++){
  uint64_t p=loadq(CAPTURE_BUFFERS+n*32u);
  if(!p||p!=m->sources[n]||p>UINT64_MAX-COOP_FRAME_BYTES||loadq(CAPTURE_BUFFERS+n*32u+8)||loadq(CAPTURE_BUFFERS+n*32u+16)||
     loadq(CAPTURE_BUFFERS+n*32u+24)||(m->data<p+COOP_FRAME_BYTES&&p<m->data+m->data_bytes)){fault(23);goto done;}
 }
 struct coop_render_view view=m->views[source];uint32_t player=identify(&view,records[source].frame);
 if(!player||(compact.have[player-1u]&&compact.view_frame[player-1u]==records[source].frame))goto done;
 uint64_t display[16]={0};if(status(display)){fault(25);goto done;}
 /* Prove opaque tags survive the actual display path before substituting any
  * owned surface. Registration success alone cannot establish that contract. */
 if(!compact.tag_seen)goto done;
 uint32_t *destination=(uint32_t*)(uintptr_t)(m->data+(player-1u)*COOP_COMPACT_VIEW_BYTES),*scratch=(uint32_t*)(uintptr_t)(m->data+2u*COOP_COMPACT_VIEW_BYTES);
 uint64_t start=ticks();compact_sample_4k((const uint32_t*)(uintptr_t)m->sources[source],destination);
#ifdef COOP_COMPACT_P1_MINIMAP
 if(player==1)compact_sample_p1_minimap((const uint32_t*)(uintptr_t)m->sources[source],destination);
#endif
#ifdef COOP_COMPACT_P2_RETICLE
 uint32_t aim[3];int aiming=player==2&&compact_p2_aim(aim);
 if(aiming)compact_draw_p2_reticle(destination,view.pose,aim);
#endif
 unsigned char equal=1;
 if(compact.verify_copy){
  compact_sample_4k((const uint32_t*)(uintptr_t)m->sources[source],scratch);
#ifdef COOP_COMPACT_P1_MINIMAP
  if(player==1)compact_sample_p1_minimap((const uint32_t*)(uintptr_t)m->sources[source],scratch);
#endif
#ifdef COOP_COMPACT_P2_RETICLE
  if(aiming)compact_draw_p2_reticle(scratch,view.pose,aim);
#endif
  size_t count=COOP_COMPACT_VIEW_BYTES/8u;uint64_t first=(uint64_t)destination,second=(uint64_t)scratch;
  __asm__ volatile("cld; repe cmpsq; sete %0":"=qm"(equal),"+D"(first),"+S"(second),"+c"(count)::"memory","cc");
 }
 compact.copy_ticks=ticks()-start;
 if(!equal||loadl(CAPTURE_FRAME)!=s->frame||loadl(CAPTURE_CURRENT_BUFFER)!=s->next_buffer){fault(24);goto done;}
 compact.have[player-1u]=1;compact.view_frame[player-1u]=records[source].frame;compact.copied[player-1u]++;
 if(!compact.have[0]||!compact.have[1]||(uint32_t)(s->frame-compact.view_frame[0])>6u||(uint32_t)(s->frame-compact.view_frame[1])>6u)goto done;
 if(status(display)){fault(25);goto done;}
 int selected=-1;for(unsigned n=0;n<5;n++){
  struct coop_tagged_slot slot=compact.slots[n];
  if(n!=compact.latest&&coop_tagged_retired(&slot,compact.cookie,display[3],compact.current_buffer,n,fence_done(slot.token))){selected=(int)n;break;}
 }
 if(selected<0){compact.waits++;goto done;}
 start=ticks();compact_compose((const uint32_t*)(uintptr_t)m->data,(const uint32_t*)(uintptr_t)(m->data+COOP_COMPACT_VIEW_BYTES),
  (uint32_t*)(uintptr_t)(compact.address+(unsigned)selected*COOP_COMPACT_OUTPUT_BYTES));
 compact.compose_ticks=ticks()-start;__asm__ volatile("sfence":::"memory");
 for(unsigned p=0;p<2;p++)compact.output_frames[selected][p]=compact.view_frame[p];
 compact.composed_frame=s->frame;__atomic_store_n(&compact.latest,(unsigned)selected,__ATOMIC_RELEASE);compact.composed++;
done:__atomic_store_n(&compact.busy,0,__ATOMIC_RELEASE);
}
static int setup_output(void){
 if(!compact.handle||compact.registered||compact.mapped||compact.physical_owned)return 0;
 uint64_t physical=UINT64_MAX,address=0;
 int result=((int(*)(uint64_t,uint64_t,int,uint64_t*))0x37ba870ull)(COMPACT_ALLOC_BYTES,0x20000,3,&physical);
 compact.setup_result=(uint32_t)result;if(result)return 0;compact.physical=physical;compact.physical_owned=1;
 result=((int(*)(uint64_t*,uint64_t,int,int,uint64_t,uint64_t))0x37ba7e0ull)(&address,COMPACT_ALLOC_BYTES,0x33,0,physical,0x20000);
 compact.setup_result=(uint32_t)result;if(result)return 0;compact.address=address;compact.mapped=1;
 if(!address||(address&0x1ffffu)||address>UINT64_MAX-COMPACT_ALLOC_BYTES)return 0;
 for(unsigned n=0;n<5;n++){compact.buffers[n].data=address+n*COOP_COMPACT_OUTPUT_BYTES;for(unsigned k=0;k<3;k++)compact.buffers[n].reserved[k]=0;}
 ((void(*)(void*,uint64_t,uint32_t,uint32_t,uint32_t,uint64_t,uint32_t,uint64_t))0x37ba890ull)((void*)ptr(compact.attribute),0x8100070422000000ull,0,COOP_COMPACT_OUTPUT_WIDTH,COOP_COMPACT_OUTPUT_HEIGHT,0,0,0);
 result=((int(*)(int,int,int,void*,int,void*,int,void*))0x37ba8a0ull)((int)compact.handle,1,3,(void*)ptr(compact.buffers),5,(void*)ptr(compact.attribute),0,0);
 compact.setup_result=(uint32_t)result;if(result)return 0;compact.registered=1;return 1;
}
static int cleanup_output(void){
 int result=0;
 if(compact.registered){result=((int(*)(int,int))compact.unregister_fn)((int)compact.handle,1);if(result)goto fail;compact.registered=0;}
 if(compact.mapped){result=((int(*)(void*,uint64_t))0x37ba7b0ull)((void*)(uintptr_t)compact.address,COMPACT_ALLOC_BYTES);if(result)goto fail;compact.mapped=0;}
 if(compact.physical_owned){result=((int(*)(uint64_t,uint64_t))0x37ba7f0ull)(compact.physical,COMPACT_ALLOC_BYTES);if(result)goto fail;compact.physical_owned=0;}
 compact.state=0;return 1;
fail:compact.cleanup_result=(uint32_t)result;return 0;
}
static void reset_camera(uint32_t p1,uint32_t p2){
 for(uint32_t n=24;n<3280;n+=4)setword(compact.camera+n,0);
 setword(compact.camera+52,p1);setword(compact.camera+56,p2);setword(compact.camera+140,2);
 setword(compact.camera+3240,1);setword(compact.camera+3272,1);setword(compact.camera+3276,compact.epoch);setword(compact.camera+24,1);
}
SECT(".compact.script") void compact_script_entry(void *context){
 if(compact.magic==COMPACT_MAGIC){
  uint64_t owner=compact.owner;uint32_t p1=loadl(owner+0x8050),p2=loadl(owner+0x8054);
  uint64_t menu=loadq(owner+0x8008);
  int display=menu&&loadq(menu+0x10000)==0x325654474e5350ull&&coop_display_split_enabled(loadl(menu+COOP_DISPLAY_WORD_OFFSET));
  int wanted=display&&!compact.stop&&loadq(owner+0x8000)==0x315353504f4f43ull&&loadl(owner+0x8010)==2&&!loadl(owner+0x8014)&&!loadl(owner+0x8018)&&loadl(owner+0x8020)==6&&loadl(owner+0x8208)==3&&p1&&p2&&p1!=p2;
  if(!wanted||p1!=compact.p1||p2!=compact.p2||loadl(compact.camera+28)){
   compact.enabled=0;setword(compact.camera+32,1);if(compact.state)compact.state=2;
  }
  if(compact.state==2){
   uint64_t s[16]={0};
   int all_done=1;for(unsigned n=0;n<5;n++)if(compact.slots[n].ticket&&(compact.slots[n].associated!=compact.slots[n].ticket||!fence_done(compact.slots[n].token)))all_done=0;
   if(all_done&&!status(s)&&coop_tagged_can_release(compact.cookie,compact.last_owned,compact.last_associated,compact.last_token,s[3],compact.current_buffer,fence_done(compact.last_token),compact.pending_ticket)){
    uint32_t free=0;if(__atomic_compare_exchange_n(&compact.busy,&free,2,0,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED)){cleanup_output();__atomic_store_n(&compact.busy,0,__ATOMIC_RELEASE);}
   }
  }
  if(wanted&&compact.state==0&&!loadl(compact.camera+64)&&loadl(compact.camera+24)==5){
   if((p2!=compact.p2||p1!=compact.p1))compact.error=0;
   if(!compact.error){
    uint32_t free=0;if(__atomic_compare_exchange_n(&compact.busy,&free,2,0,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED)){
     compact.p1=p1;compact.p2=p2;compact.epoch++;compact.latest=5;compact.last_owned=compact.last_associated=compact.pending_ticket=0;compact.last_token=0;
     for(unsigned n=0;n<5;n++)compact.slots[n]=(struct coop_tagged_slot){0};compact.have[0]=compact.have[1]=0;
     if(setup_output()){if(!compact.tag_only)reset_camera(p1,p2);compact.state=1;compact.enabled=1;}else{fault(26);}
     __atomic_store_n(&compact.busy,0,__ATOMIC_RELEASE);
    }
   }
  }
  if(compact.tag_only&&compact.state==1){uint64_t s[16]={0};if(status(s))fault(25);}
 }
 if(compact.forward)((void(*)(void*))compact.forward)(context);
}
SECT(".compact.flip") uint64_t compact_flip_entry(void *command,uint32_t handle,int32_t index,uint32_t mode,int64_t argument){
 int32_t chosen=index;int64_t arg=argument;
 if(compact.magic==COMPACT_MAGIC&&handle==compact.handle&&index>=0&&index<3&&compact.registered&&
    (uint64_t)__builtin_return_address(0)==0x30745b3ull){
  uint32_t free=0;if(__atomic_compare_exchange_n(&compact.busy,&free,1,0,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED)){
   if(compact.sequence==UINT32_MAX){fault(27);}else{
    uint32_t ticket=++compact.sequence;arg=(int64_t)tagged(ticket);
    if(compact.state==1&&compact.enabled&&!compact.stop&&camera_ready()&&compact.latest<5&&!compact.pending_ticket){
     /* Keep the last owned image during a brief output-pool delay. Switching
      * to an original buffer here produces a visible full-screen flash. */
     if((uint32_t)(loadl(CAPTURE_FRAME)-compact.composed_frame)>6u)compact.held_frames++;
     uint32_t slot=compact.latest;chosen=(int32_t)slot+3;compact.slots[slot].ticket=ticket;compact.slots[slot].associated=0;
     compact.pending_slot=slot;compact.pending_frame=loadl(CAPTURE_FRAME);compact.pending_ticket=ticket;compact.last_owned=ticket;compact.last_associated=0;compact.flips++;
    }
   }
   __atomic_store_n(&compact.busy,0,__ATOMIC_RELEASE);
  }
 }
 return ((uint64_t(*)(void*,uint32_t,int32_t,uint32_t,int64_t))compact.flip_forward)(command,handle,chosen,mode,arg);
}
