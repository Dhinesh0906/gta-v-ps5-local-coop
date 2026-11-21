/* Bounded live camera/capture/presentation integration. All image copies and
 * composition execute on the same witnessed render thread. Camera selection
 * stays in the separate leased game-thread camera sidecar. */
#include "coop-frame-capture.h"
#include "coop-view-pair.c"
#include "coop-live-policy.h"
#include "coop-async-output.h"
#include "coop-frame-inbox.h"
static void pipeline_capture(volatile struct coop_frame_capture_mailbox*,
    const struct coop_present_snapshot*,const struct coop_present_record*,unsigned);
#define COOP_LIVE_PIPELINE
#define ticks capture_ticks
#include "coop-frame-capture.c"
#undef ticks
#undef SECTION
#define _start bounded_script_entry
#define flip_entry bounded_flip_entry
#include "coop-split-present.c"
#undef _start
#undef flip_entry

struct live_pipeline {
    uint64_t magic,camera_mailbox;
    uint32_t enabled,error,busy,published[2],rejected,copied,presented;
    uint32_t frame_budget,first_frame,last_frame,skipped,selected_slot;
    uint64_t output_tail[3],thread_tls,compose_ticks;
    struct coop_view_pair pair;
    uint32_t scanout_seen[3],scanout_original,status_errors,status_current;
    uint64_t status_count,status_argument;
    uint32_t session_mode,session_epoch,session_restarts,session_wait_reason;
    uint64_t coop_owner;
    uint32_t async_enabled,worker_started,worker_stop,worker_exited,worker_busy;
    uint32_t worker_frames,worker_skipped,worker_create_result,last_pair_frame;
    uint64_t worker_handle;
    struct coop_async_outputs async_outputs;
    uint32_t worker_view_updates[2],worker_last_serial[2];
    struct coop_frame_inbox inbox;
    uint32_t worker_copy_drops;
    uint32_t source_serial[3];
};
__attribute__((section(".live.data"),used,visibility("hidden")))
volatile struct live_pipeline live_pipeline;
__attribute__((used,visibility("hidden"))) const uint32_t live_layout[]={
    sizeof(struct live_pipeline),offsetof(struct live_pipeline,enabled),offsetof(struct live_pipeline,error),
    offsetof(struct live_pipeline,busy),offsetof(struct live_pipeline,published),offsetof(struct live_pipeline,rejected),
    offsetof(struct live_pipeline,copied),offsetof(struct live_pipeline,presented),offsetof(struct live_pipeline,frame_budget),
    offsetof(struct live_pipeline,first_frame),offsetof(struct live_pipeline,last_frame),offsetof(struct live_pipeline,skipped),
    offsetof(struct live_pipeline,selected_slot),offsetof(struct live_pipeline,output_tail),offsetof(struct live_pipeline,thread_tls),
    offsetof(struct live_pipeline,compose_ticks),offsetof(struct live_pipeline,pair),
    offsetof(struct live_pipeline,scanout_seen),offsetof(struct live_pipeline,scanout_original),
    offsetof(struct live_pipeline,status_errors),offsetof(struct live_pipeline,status_current),
    offsetof(struct live_pipeline,status_count),offsetof(struct live_pipeline,status_argument),
    offsetof(struct live_pipeline,session_mode),offsetof(struct live_pipeline,session_epoch),
    offsetof(struct live_pipeline,session_restarts),offsetof(struct live_pipeline,session_wait_reason),offsetof(struct live_pipeline,coop_owner),
    offsetof(struct live_pipeline,async_enabled),offsetof(struct live_pipeline,worker_started),offsetof(struct live_pipeline,worker_stop),
    offsetof(struct live_pipeline,worker_exited),offsetof(struct live_pipeline,worker_busy),offsetof(struct live_pipeline,worker_frames),
    offsetof(struct live_pipeline,worker_skipped),offsetof(struct live_pipeline,worker_create_result),offsetof(struct live_pipeline,worker_handle),
    offsetof(struct live_pipeline,async_outputs),offsetof(struct live_pipeline,worker_view_updates),offsetof(struct live_pipeline,worker_copy_drops)};
static void stop_pipeline(uint32_t error){
    uint32_t none=0;if(error)__atomic_compare_exchange_n(&live_pipeline.error,&none,error,0,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED);
    __atomic_store_n(&live_pipeline.enabled,0,__ATOMIC_RELEASE);
}
static uint64_t current_tls(void){uint64_t tls;__asm__ volatile("movq %%fs:0,%0":"=r"(tls));return tls;}
static int ready_camera(void){
    uint64_t a=live_pipeline.camera_mailbox;
    return a&&loadq(a)==0x31444c4f484d4143ull&&loadl(a+24)==3&&!loadl(a+28)&&
        !loadl(a+32)&&loadl(a+64)&&loadl(a+140)==2;
}
/* Convert finite camera coordinates to fixed point without touching SIMD/FP
 * registers. Bound is inherited from the camera probe, tolerance <= 0.02m. */
static uint32_t classify_camera(const struct coop_render_view *v,uint32_t frame){
    uint64_t a=live_pipeline.camera_mailbox;if(!ready_camera()||!coop_render_view_valid(v))return 0;
    uint32_t head=loadl(a+160),count=loadl(a+164),cam=loadl(a+60),mask=0;
    if(count>128||!count||!cam||cam==0xffffffffu)return 0;
    for(uint32_t n=0;n<count;n++){
        uint64_t sample=a+168u+((head-1u-n)&127u)*24u;
        uint32_t sample_frame=loadl(sample+4),delta=sample_frame-frame;
        if(delta>4u&&delta<0xfffffffcu)continue;
        uint32_t which=loadl(sample+20);if(which!=0xffffffffu&&which!=cam)continue;
        int matches=1;
        for(uint32_t k=0;k<3;k++){
            uint32_t b=loadl(sample+8u+k*4u),p=v->pose[12+k];
            if(!live_position_matches(b,p)){matches=0;break;}
        }
        if(matches)mask|=which==0xffffffffu?1u:2u;
    }
    if(head!=loadl(a+160)||!ready_camera())return 0;
    return mask==1u?1u:mask==2u?2u:0u;
}
static struct coop_view_pair *pair_state(void){return (struct coop_view_pair*)pointer(&live_pipeline.pair);}
static struct coop_async_outputs *async_outputs(void){return (struct coop_async_outputs*)pointer(&live_pipeline.async_outputs);}
static struct coop_frame_inbox *frame_inbox(void){return (struct coop_frame_inbox*)pointer(&live_pipeline.inbox);}
static void copy_central(uint64_t,uint64_t);
static int same_central(uint64_t,uint64_t);
static void process_worker_input(void){
    volatile struct live_pipeline *l=&live_pipeline;volatile struct coop_frame_capture_mailbox *m=&capture_mailbox;
    struct coop_inbox_frame input;
    if(!inbox_take_latest(frame_inbox(),&input))return;
    uint32_t epoch=l->session_mode?l->session_epoch:1u;
    if(input.epoch!=epoch||input.index>=3||input.player<1||input.player>2||input.source!=m->sources[input.index]){__atomic_fetch_add(&l->worker_copy_drops,1,__ATOMIC_RELAXED);return;}
    if((uint32_t)(loadl(CAPTURE_FRAME)-input.frame)>3u||__atomic_load_n(&l->source_serial[input.index],__ATOMIC_ACQUIRE)!=input.serial||
       loadl(CAPTURE_CURRENT_BUFFER)==input.index){__atomic_fetch_add(&l->worker_copy_drops,1,__ATOMIC_RELAXED);return;}
    if(l->pair.epoch!=epoch){
        uint64_t cameras[2]={0xffffffffu,loadl(l->camera_mailbox+60)},addresses[4];
        for(unsigned n=0;n<4;n++)addresses[n]=m->data+n*COOP_FRAME_BYTES;
        if(coop_pair_configure(pair_state(),epoch,cameras,addresses)!=COOP_PAIR_OK){stop_pipeline(5);return;}
    }
    struct coop_pair_lease lease;if(coop_pair_begin_write(pair_state(),input.player,&lease)!=COOP_PAIR_OK){__atomic_fetch_add(&l->worker_copy_drops,1,__ATOMIC_RELAXED);return;}
    uint64_t destination=l->pair.slots[lease.slot].address,scratch=m->data+4u*COOP_FRAME_BYTES,start=ticks();
    copy_central(destination,input.source);copy_central(scratch,input.source);int equal=same_central(destination,scratch);m->copy_ticks=ticks()-start;
    if(!equal||(uint32_t)(loadl(CAPTURE_FRAME)-input.frame)>3u||loadl(CAPTURE_CURRENT_BUFFER)==input.index||__atomic_load_n(&l->source_serial[input.index],__ATOMIC_ACQUIRE)!=input.serial||
       !__atomic_load_n(&l->enabled,__ATOMIC_ACQUIRE)||!ready_camera()){
        coop_pair_abort_write(pair_state(),&lease);__atomic_fetch_add(&l->worker_copy_drops,1,__ATOMIC_RELAXED);return;
    }
    struct coop_pair_frame f={l->pair.cameras[input.player-1],input.token,epoch,input.player,input.frame,input.serial,COOP_PAIR_WITNESSES|COOP_PAIR_CENTRAL_COPY};
    if(coop_pair_publish(pair_state(),&lease,&f)!=COOP_PAIR_OK){coop_pair_abort_write(pair_state(),&lease);stop_pipeline(7);return;}
    l->published[input.player-1]++;l->copied++;
}
static void worker_sleep(void){((int(*)(unsigned))0x37ba740ull)(500);}
static void finish_async_output(unsigned slot,int publish){
    int result;do{result=async_output_finish(async_outputs(),slot,publish);if(result<0)worker_sleep();}while(result<0);
    if(!result)stop_pipeline(21);
}
static void *compositor_worker(void *unused){
    (void)unused;volatile struct live_pipeline *l=&live_pipeline;volatile struct split_present *m=&split_present;
    while(!__atomic_load_n(&l->worker_stop,__ATOMIC_ACQUIRE)){
        if(!__atomic_load_n(&l->enabled,__ATOMIC_ACQUIRE)||!ready_camera()||m->state!=2){worker_sleep();continue;}
        uint32_t free=0;if(!__atomic_compare_exchange_n(&l->worker_busy,&free,1,0,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED)){worker_sleep();continue;}
        if(!__atomic_load_n(&l->enabled,__ATOMIC_ACQUIRE)||!ready_camera()||m->state!=2){__atomic_store_n(&l->worker_busy,0,__ATOMIC_RELEASE);continue;}
        process_worker_input();
        struct coop_pair pair;uint32_t frame=loadl(CAPTURE_FRAME);
        if(coop_pair_acquire(pair_state(),frame,6,3,&pair)!=COOP_PAIR_OK){__atomic_store_n(&l->worker_busy,0,__ATOMIC_RELEASE);worker_sleep();continue;}
        uint32_t newest=(int32_t)(pair.frame[0].frame-pair.frame[1].frame)>0?pair.frame[0].frame:pair.frame[1].frame;
        if(newest==l->last_pair_frame){coop_pair_release(pair_state(),&pair);__atomic_store_n(&l->worker_busy,0,__ATOMIC_RELEASE);worker_sleep();continue;}
        uint64_t status[16]={0};int checked=platform_status(m->status_function,(int)m->handle,(void*)pointer(status));
        uint32_t current=*(uint32_t*)((unsigned char*)status+56);
        int selected=checked?-1:async_output_begin(async_outputs(),status[0],current);
        if(selected<0){coop_pair_release(pair_state(),&pair);l->worker_skipped++;__atomic_store_n(&l->worker_busy,0,__ATOMIC_RELEASE);worker_sleep();continue;}
        struct coop_split_capture captures[2];
        for(unsigned n=0;n<2;n++)captures[n]=(struct coop_split_capture){
            {(uint32_t*)(uintptr_t)pair.address[n],COOP_SPLIT_BYTES,3840,2160,2176,COOP_SPLIT_FORMAT_RGB10A2},
            pair.frame[n].camera,pair.frame[n].frame,n+1u,COOP_CAPTURE_OWNED|COOP_CAPTURE_COMPLETE};
        struct coop_split_surface destination={(uint32_t*)(uintptr_t)(m->output_address+(unsigned)selected*COOP_FRAME_BYTES),COOP_SPLIT_BYTES,3840,2160,2176,COOP_SPLIT_FORMAT_RGB10A2};
        uint64_t start=ticks();uint32_t result=coop_split_compose_workspace_age(captures,&destination,frame,6,(uint32_t*)pointer(m->workspace));
        l->compose_ticks=ticks()-start;m->compose_ticks=l->compose_ticks;m->compose_result=result;
        if(coop_pair_release(pair_state(),&pair)!=COOP_PAIR_OK)stop_pipeline(9);
        WRITE_BARRIER();
        int publish=!result&&__atomic_load_n(&l->enabled,__ATOMIC_ACQUIRE)&&ready_camera();
        finish_async_output((unsigned)selected,publish);
        if(result)stop_pipeline(10);else if(publish){
            l->last_pair_frame=newest;l->worker_frames++;
            for(unsigned n=0;n<2;n++)if(pair.frame[n].serial!=l->worker_last_serial[n]){
                l->worker_last_serial[n]=pair.frame[n].serial;l->worker_view_updates[n]++;
            }
        }
        __atomic_store_n(&l->worker_busy,0,__ATOMIC_RELEASE);
    }
    __atomic_store_n(&l->worker_exited,1,__ATOMIC_RELEASE);return 0;
}
static void start_compositor(void){
    volatile struct live_pipeline *l=&live_pipeline;
    if(!l->async_enabled||l->worker_started||l->worker_stop)return;
    async_output_reset(async_outputs());inbox_reset(frame_inbox());
    int result=((int(*)(uint64_t*,const void*,void*(*)(void*),void*,const char*))0x37ba6f0ull)
        ((uint64_t*)pointer(&l->worker_handle),0,compositor_worker,0,"gta-coop-compose");
    l->worker_create_result=(uint32_t)result;l->worker_started=result?3:1;if(result)stop_pipeline(20);
}
static void forward_script(void *context){
    volatile struct live_pipeline *l=&live_pipeline;
    if(l->async_enabled&&split_present.mode==2){
        uint32_t free=0;if(!__atomic_compare_exchange_n(&l->worker_busy,&free,2,0,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED)){platform_forward(context);return;}
        bounded_script_entry(context);__atomic_store_n(&l->worker_busy,0,__ATOMIC_RELEASE);
    }else bounded_script_entry(context);
}
/* Complete aligned tile rows covering the compositor's source y=540..1619.
 * No consumer reads outside this declared central capture region. */
#define LIVE_COPY_FIRST_ROW 512u
#define LIVE_COPY_ROW_COUNT 1152u
#define LIVE_COPY_OFFSET (512ull*3840u*4u)
#define LIVE_COPY_BYTES (1152ull*3840u*4u)
_Static_assert(LIVE_COPY_FIRST_ROW%128u==0&&LIVE_COPY_ROW_COUNT%128u==0,"whole tile rows");
_Static_assert(COOP_SPLIT_HEIGHT/4u>=LIVE_COPY_FIRST_ROW&&COOP_SPLIT_HEIGHT*3u/4u<=LIVE_COPY_FIRST_ROW+LIVE_COPY_ROW_COUNT,"complete compositor source coverage");
static void copy_central(uint64_t to,uint64_t from){
    to+=LIVE_COPY_OFFSET;from+=LIVE_COPY_OFFSET;size_t count=LIVE_COPY_BYTES/8u;
    __asm__ volatile("cld; rep movsq":"+D"(to),"+S"(from),"+c"(count)::"memory","cc");
}
static int same_central(uint64_t first,uint64_t second){
    first+=LIVE_COPY_OFFSET;second+=LIVE_COPY_OFFSET;size_t count=LIVE_COPY_BYTES/8u;unsigned char equal;
    __asm__ volatile("cld; repe cmpsq; sete %0":"=qm"(equal),"+D"(first),"+S"(second),"+c"(count)::"memory","cc");
    return equal;
}
static void pipeline_capture(volatile struct coop_frame_capture_mailbox *m,
    const struct coop_present_snapshot *s,const struct coop_present_record *records,unsigned source){
    volatile struct live_pipeline *l=&live_pipeline;
    if(l->magic!=0x31455049504c564cull||!__atomic_load_n(&l->enabled,__ATOMIC_ACQUIRE))return;
    if(!ready_camera())return;
    uint32_t free=0;if(!__atomic_compare_exchange_n(&l->busy,&free,1,0,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED))return;
    if(l->thread_tls&&l->thread_tls!=m->tls){stop_pipeline(1);goto done;}
    for(unsigned n=0;n<3;n++)__atomic_store_n(&l->source_serial[n],records[n].serial,__ATOMIC_RELEASE);
    l->thread_tls=m->tls;l->last_frame=s->frame;
    if(l->session_mode?(l->session_mode!=1||l->frame_budget):(l->frame_budget<8||l->frame_budget>1800)){stop_pipeline(2);goto done;}
    if(!l->first_frame)l->first_frame=s->frame;
    if(!l->session_mode&&(uint32_t)(s->frame-l->first_frame)>=l->frame_budget){stop_pipeline(0);goto done;}
    if(output_network()||loadq(CAPTURE_DEVICE)!=m->expected_device||m->thread_changes||!m->render_context||
       m->data_bytes!=5u*COOP_FRAME_BYTES||!m->data||(m->data&63u)||m->data>UINT64_MAX-m->data_bytes||
       loadl(CAPTURE_WIDTH)!=3840||loadl(CAPTURE_HEIGHT)!=2160||loadl(CAPTURE_BUFFER_COUNT)!=3||
       *(volatile unsigned char*)CAPTURE_FORMAT!=24){stop_pipeline(3);goto done;}
    for(unsigned n=0;n<3;n++)if(loadq(CAPTURE_BUFFERS+n*32u)!=m->sources[n]||
        (m->data<m->sources[n]+COOP_FRAME_BYTES&&m->sources[n]<m->data+m->data_bytes)){
        stop_pipeline(4);goto done;
    }
    if(l->async_enabled){
        struct coop_render_view view=m->views[source];uint32_t player=classify_camera(&view,records[source].frame);
        if(!player){l->rejected++;goto done;}
        struct coop_inbox_frame input={m->sources[source],records[source].token,source,records[source].frame,records[source].serial,player,l->session_mode?l->session_epoch:1u};
        if(!inbox_post(frame_inbox(),&input))__atomic_fetch_add(&l->worker_copy_drops,1,__ATOMIC_RELAXED);
        goto done;
    }
    uint32_t epoch=l->session_mode?l->session_epoch:1u;
    if(l->pair.epoch!=epoch){
        uint64_t cameras[2]={0xffffffffu,loadl(l->camera_mailbox+60)},addresses[4];
        for(unsigned n=0;n<4;n++)addresses[n]=m->data+n*COOP_FRAME_BYTES;
        if(coop_pair_configure(pair_state(),epoch,cameras,addresses)!=COOP_PAIR_OK){stop_pipeline(5);goto done;}
    }
    struct coop_render_view view=m->views[source];uint32_t player=classify_camera(&view,records[source].frame);
    if(!player){l->rejected++;goto done;}
    struct coop_pair_lease lease;
    if(coop_pair_begin_write(pair_state(),player,&lease)!=COOP_PAIR_OK){l->skipped++;goto done;}
    uint64_t destination=l->pair.slots[lease.slot].address,scratch=m->data+4u*COOP_FRAME_BYTES;
    uint64_t start=capture_ticks();copy_central(destination,m->sources[source]);copy_central(scratch,m->sources[source]);
    int equal=same_central(destination,scratch);m->copy_ticks=capture_ticks()-start;
    if(!equal||s->frame!=loadl(CAPTURE_FRAME)||s->next_buffer!=loadl(CAPTURE_CURRENT_BUFFER)){
        coop_pair_abort_write(pair_state(),&lease);stop_pipeline(6);goto done;
    }
    struct coop_pair_frame f={l->pair.cameras[player-1u],records[source].token,epoch,player,
        records[source].frame,records[source].serial,COOP_PAIR_WITNESSES|COOP_PAIR_CENTRAL_COPY};
    if(coop_pair_publish(pair_state(),&lease,&f)!=COOP_PAIR_OK){coop_pair_abort_write(pair_state(),&lease);stop_pipeline(7);goto done;}
    l->published[player-1u]++;l->copied++;
done:__atomic_store_n(&l->busy,0,__ATOMIC_RELEASE);
}
static void camera_word(uint32_t offset,uint32_t value){
    __atomic_store_n((uint32_t*)(uintptr_t)(live_pipeline.camera_mailbox+offset),value,__ATOMIC_RELEASE);
}
static void supervise_session(void){
    volatile struct live_pipeline *l=&live_pipeline;volatile struct split_present *m=&split_present;
    uint64_t owner=l->coop_owner,a=l->camera_mailbox;
    if(!owner||!a||loadq(owner+0x8000u)!=0x315353504f4f43ull){stop_pipeline(13);m->mode=2;camera_word(32,1);return;}
    uint32_t p1=loadl(owner+0x8050u),p2=loadl(owner+0x8054u),phase=loadl(owner+0x8208u);
    int wanted=loadl(owner+0x8010u)==2&&!loadl(owner+0x8014u)&&!loadl(owner+0x8018u)&&
        loadl(owner+0x8020u)==6&&phase==3&&p1&&p2&&p1!=p2;
    uint32_t camera_state=loadl(a+24),same=loadl(a+52)==p1&&loadl(a+56)==p2;
    struct live_session_observation observation={(uint32_t)wanted,same,camera_state,loadl(a+28),loadl(a+36),
        m->state,m->error,m->busy,m->registered||m->mapped||m->physical_owned,l->error};
    enum live_session_action action=live_session_plan(&observation);
    if(action==LIVE_SESSION_STOP||action==LIVE_SESSION_FAULT){
        __atomic_store_n(&l->enabled,0,__ATOMIC_RELEASE);camera_word(32,1);m->mode=2;
        l->session_wait_reason=action==LIVE_SESSION_FAULT?4:wanted?2:1;return;
    }
    if(action==LIVE_SESSION_KEEP){
        if(same&&wanted&&!loadl(a+32))l->session_wait_reason=camera_state==6||camera_state==7?3:0;
        return;
    }
    uint32_t free=0;if(!__atomic_compare_exchange_n(&l->busy,&free,1,0,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED))return;
    uint32_t idle=0;if(l->async_enabled&&!__atomic_compare_exchange_n(&l->worker_busy,&idle,2,0,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED)){
        __atomic_store_n(&l->busy,0,__ATOMIC_RELEASE);return;
    }
    for(uint32_t n=24;n<3280;n+=4)camera_word(n,0);
    camera_word(52,p1);camera_word(56,p2);camera_word(140,2);camera_word(3240,1);camera_word(3272,1);
    uint32_t epoch=l->session_epoch+1u;if(!epoch)epoch=1;
    l->session_epoch=epoch;camera_word(3276,epoch);
    m->error=0;m->submission_recorded=0;m->submitted_tail=0;m->tracking_error=0;m->owned_scanout_seen=0;
    for(unsigned n=0;n<3;n++)l->output_tail[n]=0;
    l->first_frame=0;l->session_restarts++;l->session_wait_reason=0;
    l->last_pair_frame=0;if(l->async_enabled){async_output_reset(async_outputs());inbox_reset(frame_inbox());}
    camera_word(24,1);__atomic_store_n(&m->state,0,__ATOMIC_RELEASE);m->mode=1;
    __atomic_store_n(&l->enabled,1,__ATOMIC_RELEASE);if(l->async_enabled)__atomic_store_n(&l->worker_busy,0,__ATOMIC_RELEASE);__atomic_store_n(&l->busy,0,__ATOMIC_RELEASE);
}
__attribute__((section(".live.script"),used,visibility("hidden")))
void live_script_entry(void *context){
    start_compositor();
    if(live_pipeline.magic==0x31455049504c564cull&&live_pipeline.session_mode==1){
        supervise_session();forward_script(context);supervise_session();return;
    }
    uint64_t a=live_pipeline.camera_mailbox;
    if(live_pipeline.magic==0x31455049504c564cull&&a){
        if(loadl(a+24)==5||loadl(a+32))stop_pipeline(0);
        if(!__atomic_load_n(&live_pipeline.enabled,__ATOMIC_ACQUIRE))split_present.mode=2;
    }
    forward_script(context);
    if(split_present.state==2&&split_present.output_address){
        uint64_t o=split_present.output_address,d=capture_mailbox.data;
        if(o<d+capture_mailbox.data_bytes&&d<o+SPLIT_OUTPUT_BYTES)stop_pipeline(12);
    }
}
static uint64_t async_flip(void *command,uint32_t handle,int32_t index,uint32_t mode,int64_t argument){
    volatile struct live_pipeline *l=&live_pipeline;volatile struct split_present *m=&split_present;
    uint32_t idle=0;if(!__atomic_compare_exchange_n(&m->busy,&idle,1,0,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED))return platform_flip(command,handle,index,mode,argument);
    uint32_t ready=2;if(!__atomic_compare_exchange_n(&m->state,&ready,3,0,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED)){
        __atomic_store_n(&m->busy,0,__ATOMIC_RELEASE);return platform_flip(command,handle,index,mode,argument);
    }
    uint64_t status[16]={0},result;int selected=-1;
    int checked=platform_status(m->status_function,(int)handle,(void*)pointer(status));
    uint32_t pending=*(uint32_t*)((unsigned char*)status+52),current=*(uint32_t*)((unsigned char*)status+56);
    if(checked||pending>16||status[0]>UINT64_MAX-(uint64_t)pending-1u){l->status_errors++;stop_pipeline(8);}
    else{
        l->status_current=current;l->status_count=status[0];l->status_argument=status[3];
        if(current>=3&&current<6)l->scanout_seen[current-3]++;else if(current<3)l->scanout_original++;
        uint64_t tail=status[0]+pending+1u;
        selected=async_output_reserve(async_outputs(),tail);
        if(selected>=0){m->submitted_tail=tail;m->submission_recorded=1;}
    }
    if(selected>=0){
        result=platform_flip(command,handle,selected+3,mode,argument);m->flip_result=result;m->presented++;l->presented++;
        checked=platform_status(m->status_function,(int)handle,(void*)pointer(status));pending=*(uint32_t*)((unsigned char*)status+52);
        if(checked||pending>16||status[0]>UINT64_MAX-pending||status[0]+pending!=m->submitted_tail){
            m->tracking_error=1;stop_pipeline(11);
            if(!checked&&pending<=16&&status[0]<=UINT64_MAX-pending&&status[0]+pending>m->submitted_tail)m->submitted_tail=status[0]+pending;
        }
    }else{l->skipped++;result=platform_flip(command,handle,index,mode,argument);}
    __atomic_store_n(&m->state,2,__ATOMIC_RELEASE);__atomic_store_n(&m->busy,0,__ATOMIC_RELEASE);return result;
}
__attribute__((section(".live.flip"),used,visibility("hidden")))
uint64_t live_flip_entry(void *command,uint32_t handle,int32_t index,uint32_t mode,int64_t argument){
    volatile struct live_pipeline *l=&live_pipeline;volatile struct split_present *m=&split_present;
    if(l->magic!=0x31455049504c564cull||!__atomic_load_n(&l->enabled,__ATOMIC_ACQUIRE)||!ready_camera()||
       (l->session_mode&&l->pair.epoch!=l->session_epoch)||
       m->state!=2||!m->output_address||handle!=m->handle||index<0||index>2)
        return platform_flip(command,handle,index,mode,argument);
    if(l->async_enabled)return async_flip(command,handle,index,mode,argument);
    uint32_t free=0;if(!__atomic_compare_exchange_n(&l->busy,&free,1,0,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED))
        return platform_flip(command,handle,index,mode,argument);
    uint64_t result;int substituted=0;
    if(l->thread_tls&&l->thread_tls!=current_tls()){stop_pipeline(1);goto fallback;}
    l->thread_tls=current_tls();
    uint32_t idle=0;
    if(!__atomic_compare_exchange_n(&m->busy,&idle,1,0,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED))goto fallback;
    uint32_t ready=2;
    if(!__atomic_compare_exchange_n(&m->state,&ready,3,0,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED)){
        __atomic_store_n(&m->busy,0,__ATOMIC_RELEASE);goto fallback;
    }
    uint64_t status[16]={0};
    int checked=platform_status(m->status_function,(int)handle,(void*)pointer(status));
    uint32_t pending=*(uint32_t*)((unsigned char*)status+52),current=*(uint32_t*)((unsigned char*)status+56),selected=3;
    if(checked)l->status_errors++;
    else{
        l->status_current=current;l->status_count=status[0];l->status_argument=status[3];
        if(current>=3u&&current<6u)l->scanout_seen[current-3u]++;
        else if(current<3u)l->scanout_original++;
    }
    if(checked||pending>16||status[0]>UINT64_MAX-(uint64_t)pending-1u){stop_pipeline(8);goto release_output;}
    for(unsigned k=0;k<3;k++){
        unsigned slot=(l->selected_slot+k)%3u;
        if(live_output_available(l->output_tail[slot],status[0],current,slot)){selected=slot;break;}
    }
    if(selected==3){l->skipped++;goto release_output;}
    struct coop_pair pair;uint32_t frame=loadl(CAPTURE_FRAME);
    if(coop_pair_acquire(pair_state(),frame,6,3,&pair)!=COOP_PAIR_OK){l->skipped++;goto release_output;}
    struct coop_split_capture captures[2];
    for(unsigned n=0;n<2;n++)captures[n]=(struct coop_split_capture){
        {(uint32_t*)(uintptr_t)pair.address[n],COOP_SPLIT_BYTES,3840,2160,2176,COOP_SPLIT_FORMAT_RGB10A2},
        pair.frame[n].camera,pair.frame[n].frame,n+1u,COOP_CAPTURE_OWNED|COOP_CAPTURE_COMPLETE};
    struct coop_split_surface destination={(uint32_t*)(uintptr_t)(m->output_address+selected*COOP_FRAME_BYTES),COOP_SPLIT_BYTES,
        3840,2160,2176,COOP_SPLIT_FORMAT_RGB10A2};
    uint64_t start=ticks();m->compose_result=coop_split_compose_workspace_age(captures,&destination,frame,6,(uint32_t*)pointer(m->workspace));
    l->compose_ticks=ticks()-start;m->compose_ticks=l->compose_ticks;
    if(coop_pair_release(pair_state(),&pair)!=COOP_PAIR_OK)stop_pipeline(9);
    if(m->compose_result){stop_pipeline(10);goto release_output;}
    WRITE_BARRIER();
    uint64_t tail=status[0]+pending+1u;m->submitted_tail=tail;m->submission_recorded=1;l->output_tail[selected]=tail;
    result=platform_flip(command,handle,selected+3u,mode,argument);substituted=1;
    l->selected_slot=(selected+1u)%3u;l->presented++;m->presented++;m->flip_result=result;
    checked=platform_status(m->status_function,(int)handle,(void*)pointer(status));pending=*(uint32_t*)((unsigned char*)status+52);
    if(checked||pending>16||status[0]>UINT64_MAX-pending||status[0]+pending!=tail){
        m->tracking_error=1;stop_pipeline(11);
        if(!checked&&pending<=16&&status[0]<=UINT64_MAX-pending&&status[0]+pending>m->submitted_tail)m->submitted_tail=status[0]+pending;
    }
release_output:
    __atomic_store_n(&m->state,2,__ATOMIC_RELEASE);__atomic_store_n(&m->busy,0,__ATOMIC_RELEASE);
fallback:
    __atomic_store_n(&l->busy,0,__ATOMIC_RELEASE);
    if(!substituted)result=platform_flip(command,handle,index,mode,argument);
    return result;
}
