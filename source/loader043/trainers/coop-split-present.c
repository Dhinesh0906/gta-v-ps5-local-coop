/* Bounded end-to-end split-frame presentation probe.
 *
 * The resident script entry registers an owned three-buffer scanout set. The
 * AGC flip entry then composes two caller-owned completed frames into owned
 * buffer index 3 and substitutes exactly one game flip. The next ordinary GTA
 * frame returns presentation to the original buffers. No camera or game state
 * is changed. This is not yet a continuous split-screen renderer.
 */
#include <stdint.h>
#include <stddef.h>
#include "coop-split-compositor.c"

#define SPLIT_PRESENT_MAGIC 0x31534c5053544c43ull
#define SPLIT_OUTPUT_BYTES (33423360ull * 3u)
#define SPLIT_OUTPUT_INDEX 3u

struct output_buffer { uint64_t data, reserved[3]; };
struct split_present {
    uint64_t magic, forward, flip_forward, stable;
    volatile uint32_t state, error, visits, busy;
    uint32_t mode, handle, physical_owned, mapped;
    uint32_t registered, cleanup_attempts, compose_result, register_result;
    uint32_t unregister_result, presented, flip_index, padding;
    uint64_t capture[2], output_physical, output_address, output_bytes;
    uint64_t register_function, unregister_function, status_function;
    uint64_t video_start, video_end, expected_register;
    uint64_t scene_frame, compose_ticks, flip_result;
    uint64_t attribute[10];
    struct output_buffer buffers[3];
    uint32_t workspace[COOP_SPLIT_WIDTH / 4u];
    uint64_t submitted_tail, observed_count;
    uint32_t submission_recorded, retirement_checks, tracking_error, owned_scanout_seen;
};
_Static_assert(offsetof(struct split_present,state)==32,"present state");
_Static_assert(offsetof(struct split_present,capture)==96,"present captures");
_Static_assert(offsetof(struct split_present,output_physical)==112,"present output");
_Static_assert(offsetof(struct split_present,register_function)==136,"present functions");
_Static_assert(offsetof(struct split_present,scene_frame)==184,"present frame");
_Static_assert(offsetof(struct split_present,attribute)==208,"present attribute");
_Static_assert(offsetof(struct split_present,buffers)==288,"present buffers");
_Static_assert(offsetof(struct split_present,workspace)==384,"present workspace");
_Static_assert(sizeof(struct split_present)==4256,"present mailbox");

#ifndef COOP_PRESENT_TEST
#define SECTION(name) __attribute__((section(name),used,visibility("hidden")))
#else
#define SECTION(name)
static int platform_status(uint64_t,int,void*);
static int platform_allocate(uint64_t*);
static int platform_map(uint64_t,uint64_t*);
static int platform_register(int,struct output_buffer*,void*);
static int platform_unregister(uint64_t,int);
static int platform_unmap(uint64_t);
static int platform_release(uint64_t);
static void platform_attribute(void*);
static uint64_t platform_flip(void*,uint32_t,int32_t,uint32_t,int64_t);
static void platform_forward(void*);
static int output_network(void);
#endif

SECTION(".split.data") volatile struct split_present split_present;

#ifndef COOP_PRESENT_TEST
static int output_network(void) { return *(volatile unsigned char*)0x5abefb8ull; }
static int platform_status(uint64_t function,int handle,void *status) {
    return ((int(*)(int,void*))function)(handle,status);
}
static int platform_allocate(uint64_t *physical) {
    return ((int(*)(uint64_t,uint64_t,int,uint64_t*))0x37ba870ull)(SPLIT_OUTPUT_BYTES,0x20000,3,physical);
}
static int platform_map(uint64_t physical,uint64_t *address) {
    return ((int(*)(uint64_t*,uint64_t,int,int,uint64_t,uint64_t))0x37ba7e0ull)
        (address,SPLIT_OUTPUT_BYTES,0x33,0,physical,0x20000);
}
static int platform_register(int handle,struct output_buffer *buffers,void *attribute) {
    return ((int(*)(int,int,int,void*,int,void*,int,void*))0x37ba8a0ull)
        (handle,1,3,buffers,3,attribute,0,0);
}
static int platform_unregister(uint64_t function,int handle) {
    return ((int(*)(int,int))function)(handle,1);
}
static int platform_unmap(uint64_t address) {
    return ((int(*)(void*,uint64_t))0x37ba7b0ull)((void*)address,SPLIT_OUTPUT_BYTES);
}
static int platform_release(uint64_t physical) {
    return ((int(*)(uint64_t,uint64_t))0x37ba7f0ull)(physical,SPLIT_OUTPUT_BYTES);
}
static void platform_attribute(void *attribute) {
    ((void(*)(void*,uint64_t,uint32_t,uint32_t,uint32_t,uint64_t,uint32_t,uint64_t))0x37ba890ull)
        (attribute,0x8100070422000000ull,0,3840,2160,0,0,0);
}
static uint64_t platform_flip(void *command,uint32_t handle,int32_t index,uint32_t mode,int64_t argument) {
    return ((uint64_t(*)(void*,uint32_t,int32_t,uint32_t,int64_t))split_present.flip_forward)
        (command,handle,index,mode,argument);
}
static void platform_forward(void *context) {
    if (split_present.forward) ((void(*)(void*))split_present.forward)(context);
}
#endif

static uint64_t pointer(const volatile void *p) {
    uint64_t result=(uint64_t)p;__asm__ volatile("" : "+r"(result));return result;
}
static int in_video(uint64_t p) {
    return p>=split_present.video_start && p<split_present.video_end;
}
static int capture_valid(uint64_t p) {
    return p && !(p&15u) && p<=UINT64_MAX-COOP_SPLIT_BYTES;
}
static void cleanup_output(void) {
    volatile struct split_present *m=&split_present;
    m->cleanup_attempts++;
    if (m->registered) {
        m->unregister_result=platform_unregister(m->unregister_function,(int)m->handle);
        if (m->unregister_result) { m->error=10;return; }
        m->registered=0;
    }
    if (m->mapped) {
        int result=platform_unmap(m->output_address);
        if (result) { m->error=11;return; }
        m->mapped=0;
    }
    if (m->physical_owned) {
        int result=platform_release(m->output_physical);
        if (result) { m->error=12;return; }
        m->physical_owned=0;
    }
}
static void setup_output(void) {
    volatile struct split_present *m=&split_present;
    if (output_network() || !m->flip_forward || !m->handle ||
        !m->register_function || !m->unregister_function || !m->status_function ||
        m->video_start>=m->video_end || !in_video(m->register_function) ||
        !in_video(m->unregister_function) || !in_video(m->status_function) ||
        m->register_function!=m->expected_register ||
        m->flip_index!=SPLIT_OUTPUT_INDEX ||
        !capture_valid(m->capture[0]) || !capture_valid(m->capture[1]) ||
        m->capture[0]==m->capture[1]) {
        m->error=1;return;
    }
    uint64_t physical=UINT64_MAX,address=0;
    m->output_bytes=SPLIT_OUTPUT_BYTES;
    int result=platform_allocate(&physical);
    if (result) { m->error=2;return; }
    m->output_physical=physical;m->physical_owned=1;
    result=platform_map(physical,&address);
    if (result) { m->error=3;cleanup_output();return; }
    m->output_address=address;m->mapped=1;
    if (!address || (address&0x1ffffu) || address>UINT64_MAX-SPLIT_OUTPUT_BYTES ||
        address==m->capture[0] || address==m->capture[1] ||
        (address<m->capture[0]+COOP_SPLIT_BYTES && m->capture[0]<address+SPLIT_OUTPUT_BYTES) ||
        (address<m->capture[1]+COOP_SPLIT_BYTES && m->capture[1]<address+SPLIT_OUTPUT_BYTES)) {
        m->error=4;cleanup_output();return;
    }
    for (unsigned i=0;i<3;i++) {
        m->buffers[i].data=address+i*33423360ull;
        m->buffers[i].reserved[0]=m->buffers[i].reserved[1]=m->buffers[i].reserved[2]=0;
    }
    platform_attribute((void*)pointer(m->attribute));
    m->register_result=platform_register((int)m->handle,
        (struct output_buffer*)pointer(m->buffers),(void*)pointer(m->attribute));
    if (!m->register_result) m->registered=1;
    else { m->error=5;cleanup_output();}
}
#ifdef COOP_PRESENT_TEST
static uint64_t ticks(void) { static uint64_t value;return ++value; }
#define WRITE_BARRIER() __asm__ volatile("" ::: "memory")
#else
static uint64_t ticks(void) {
    uint32_t lo,hi;
    __asm__ volatile("lfence; rdtsc" : "=a"(lo),"=d"(hi) :: "memory");
    return ((uint64_t)hi<<32)|lo;
}
#define WRITE_BARRIER() __asm__ volatile("sfence" ::: "memory")
#endif

SECTION(".entry") void _start(void *context) {
    volatile struct split_present *m=&split_present;
    m->visits++;
    if (m->magic==SPLIT_PRESENT_MAGIC) {
        if (m->mode==1u && m->state==0u) {
            uint32_t expected=0;
            if (__atomic_compare_exchange_n(&m->state,&expected,1,0,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED)) {
                m->busy=1;
                setup_output();
                m->busy=0;
                __asm__ volatile("" ::: "memory");
                m->state=m->error?6u:2u;
            }
        } else if (m->mode==2u && (m->state==2u||m->state==4u||m->state==6u)) {
            uint32_t expected=m->state;
            if (__atomic_compare_exchange_n(&m->state,&expected,5,0,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED)) {
                m->busy=1;
                m->error=0;
                uint64_t status[16]={0};
                int result=platform_status(m->status_function,(int)m->handle,(void*)pointer(status));
                uint32_t current=*(uint32_t*)((unsigned char*)status+56);
                uint64_t count=status[0];uint32_t pending=*(uint32_t*)((unsigned char*)status+52);
                m->retirement_checks++;m->observed_count=count;
                if(!result&&current==m->flip_index)m->owned_scanout_seen=1;
                /* A pending owned flip may still sit behind an ordinary
                 * current buffer. Require completion past its queue tail,
                 * then exclude it from current scanout. An ambiguous submit
                 * additionally requires the whole queue to drain. */
                if (result || current>=3u || (m->submission_recorded&&
                    (count<=m->submitted_tail || (m->tracking_error&&pending)))) m->error=13;
                else cleanup_output();
                m->busy=0;
                __asm__ volatile("" ::: "memory");
                m->state=m->error?6u:5u;
            }
        }
    }
    platform_forward(context);
}

SECTION(".flipentry")
__attribute__((noinline))
uint64_t flip_entry(void *command,uint32_t handle,int32_t index,uint32_t mode,int64_t argument) {
    volatile struct split_present *m=&split_present;
    if (m->magic!=SPLIT_PRESENT_MAGIC || m->state!=2u || !m->flip_forward ||
        handle!=m->handle || index<0 || index>2 || !m->output_address)
        return platform_flip(command,handle,index,mode,argument);
    uint32_t idle=0;
    if (!__atomic_compare_exchange_n(&m->busy,&idle,1,0,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED))
        return platform_flip(command,handle,index,mode,argument);
    uint32_t ready=2;
    if (!__atomic_compare_exchange_n(&m->state,&ready,3,0,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED)) {
        __atomic_store_n(&m->busy,0,__ATOMIC_RELEASE);
        return platform_flip(command,handle,index,mode,argument);
    }
    uint64_t result;
    uint32_t frame=(uint32_t)m->scene_frame;
    if (!frame || (uint32_t)m->capture[0]==(uint32_t)m->capture[1]) {
        m->error=20;
        result=platform_flip(command,handle,index,mode,argument);
    } else {
        struct coop_split_capture captures[2]={
            {{(uint32_t*)(uintptr_t)m->capture[0],COOP_SPLIT_BYTES,
              COOP_SPLIT_WIDTH,COOP_SPLIT_HEIGHT,COOP_SPLIT_STORAGE_ROWS,
              COOP_SPLIT_FORMAT_RGB10A2},1,frame,1,
             COOP_CAPTURE_OWNED|COOP_CAPTURE_COMPLETE},
            {{(uint32_t*)(uintptr_t)m->capture[1],COOP_SPLIT_BYTES,
              COOP_SPLIT_WIDTH,COOP_SPLIT_HEIGHT,COOP_SPLIT_STORAGE_ROWS,
              COOP_SPLIT_FORMAT_RGB10A2},2,frame-1u,2,
             COOP_CAPTURE_OWNED|COOP_CAPTURE_COMPLETE}
        };
        struct coop_split_surface destination={(uint32_t*)(uintptr_t)m->output_address,
            COOP_SPLIT_BYTES,COOP_SPLIT_WIDTH,COOP_SPLIT_HEIGHT,
            COOP_SPLIT_STORAGE_ROWS,COOP_SPLIT_FORMAT_RGB10A2};
        uint64_t start=ticks();
        m->compose_result=(uint32_t)coop_split_compose_workspace(
            captures,&destination,frame,(uint32_t*)pointer(m->workspace));
        m->compose_ticks=ticks()-start;
        WRITE_BARRIER();
        if (m->compose_result) {
            m->error=21u+m->compose_result;
            result=platform_flip(command,handle,index,mode,argument);
        } else {
            uint64_t before[16]={0},after[16]={0};
            int checked=platform_status(m->status_function,(int)handle,(void*)pointer(before));
            uint32_t pending=*(uint32_t*)((unsigned char*)before+52);
            if(checked||pending>16u||before[0]>UINT64_MAX-(uint64_t)pending-1u){
                m->error=30;result=platform_flip(command,handle,index,mode,argument);
            }else{
                uint64_t tail=before[0]+pending;
                m->submitted_tail=tail+1u;m->submission_recorded=1;
                result=platform_flip(command,handle,m->flip_index,mode,argument);
                m->flip_result=result;m->presented=1;
                checked=platform_status(m->status_function,(int)handle,(void*)pointer(after));
                pending=*(uint32_t*)((unsigned char*)after+52);
                if(checked||pending>16u||after[0]>UINT64_MAX-pending||after[0]+pending!=tail+1u){
                    m->tracking_error=1;m->error=31;
                    if(!checked&&pending<=16u&&after[0]<=UINT64_MAX-pending&&after[0]+pending>m->submitted_tail)
                        m->submitted_tail=after[0]+pending;
                }
            }
        }
    }
    __asm__ volatile("" ::: "memory");
    m->state=m->error?6u:4u;
    __atomic_store_n(&m->busy,0,__ATOMIC_RELEASE);
    return result;
}
