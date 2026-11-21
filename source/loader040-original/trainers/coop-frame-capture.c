/* CPU snapshot after the one original Present call. This helper is compiled
 * with general registers only; the wrapper preserves the original function's
 * post-call registers/flags and never touches SIMD state.
 */
#include "coop-frame-capture.h"
#include <coop-frame-capture-profile.h>
__attribute__((section(".capture.data"),used,visibility("hidden")))
volatile struct coop_frame_capture_mailbox capture_mailbox;

static uint64_t loadq(uintptr_t address) { return *(volatile uint64_t *)address; }
static uint32_t loadl(uintptr_t address) { return *(volatile uint32_t *)address; }
static uint64_t ticks(void) {
    uint32_t lo,hi;
    __asm__ volatile("mfence; lfence; rdtsc" : "=a"(lo),"=d"(hi) :: "memory");
    return ((uint64_t)hi<<32)|lo;
}
static void copy_frame(uint64_t to,uint64_t from) {
    size_t count=COOP_FRAME_BYTES/8u;
    __asm__ volatile("cld; rep movsq" : "+D"(to),"+S"(from),"+c"(count) :: "memory","cc");
}
static int same_frames(uint64_t first,uint64_t second) {
    size_t count=COOP_FRAME_BYTES/8u;unsigned char equal;
    __asm__ volatile("cld; repe cmpsq; sete %0" : "=qm"(equal),"+D"(first),"+S"(second),"+c"(count) :: "memory","cc");
    return equal;
}
__attribute__((used,visibility("hidden")))
void capture_after_present(void) {
    volatile struct coop_frame_capture_mailbox *m=&capture_mailbox;
    if (m->magic!=0x315041434d524643ull || !m->enabled) return;
    if (__atomic_exchange_n(&m->observer_busy,1,__ATOMIC_ACQUIRE)) {
        __atomic_fetch_add(&m->contended,1,__ATOMIC_RELAXED); return;
    }
    m->calls++;
    uint64_t tls;
    __asm__ volatile("movq %%fs:0,%0" : "=r"(tls));
    if (m->tls && m->tls!=tls) m->thread_changes++;
    m->tls=tls;
    if (tls) { m->tls_flags=loadl(tls-0x40); m->render_context=loadq(tls-0x38); }
    if (m->cancel) {
        uint32_t state=m->state;
        if (state==0 || state==1)
            __atomic_compare_exchange_n(&m->state,&state,5,0,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED);
        if (m->state!=2) m->cancel_ack=1;
        goto done;
    }
    struct coop_present_snapshot s;
    s.frame=loadl(CAPTURE_FRAME); s.next_buffer=loadl(CAPTURE_CURRENT_BUFFER);
    s.rotation=loadl(CAPTURE_ROTATION); s.read_serial=loadl(CAPTURE_READ_SERIAL);
    s.write_serial=loadl(CAPTURE_WRITE_SERIAL); s.padding=0;
    s.tokens[0]=loadq(CAPTURE_TOKENS); s.tokens[1]=loadq(CAPTURE_TOKENS+8);
    uint64_t labels=loadq(CAPTURE_LABELS);
    if (labels!=m->expected_labels) {
        m->rejected_snapshots++; goto done;
    }
    for (unsigned i=0;i<8;i++) s.completed[i]=loadq(labels+i*32u);
    /* Volatile member assignment avoids any assumption that host reads of
     * the whole telemetry block are atomic. State=3 is the publication point.
     */
    m->snapshot=s;
    struct coop_present_record records[3];
    for (unsigned i=0;i<3;i++) records[i]=m->records[i];
    if (!coop_present_record_frame(&s,records)) { m->rejected_snapshots++; goto done; }
    for (unsigned i=0;i<3;i++) m->records[i]=records[i];
#ifdef COOP_COMPACT_PIPELINE
    compact_observe_submission(&s);
#endif
    struct coop_render_view view={0};
    view.selector=loadl(CAPTURE_RENDER_SELECTOR);
    if (view.selector<=1u) {
        uintptr_t pose=CAPTURE_VIEW_ARRAY+(uintptr_t)view.selector*0x460u+0xc0u;
        for (unsigned i=0;i<16;i++) view.pose[i]=loadl(pose+i*4u);
        view.valid=1;
        if (!coop_render_view_valid(&view) || loadl(CAPTURE_RENDER_SELECTOR)!=view.selector)
            view.valid=0;
    }
    m->views[(s.next_buffer+2u)%3u]=view;
    int source=coop_present_completed_source(&s,records);
    if (source<0) { m->rejected_snapshots++; goto done; }
#ifdef COOP_COMPACT_PIPELINE
    compact_capture(m,&s,records,(unsigned)source);
    goto done;
#endif
#ifdef COOP_LIVE_PIPELINE
    pipeline_capture(m,&s,records,(unsigned)source);
    goto done;
#endif
    uint32_t armed=1;
    if (!__atomic_compare_exchange_n(&m->state,&armed,2,0,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED)) goto done;
    uint32_t error=0;
    uint64_t data=m->data,bytes=m->data_bytes;
    uint32_t target=m->capture_target,count=m->capture_count;
    struct coop_render_view source_view=m->views[source];
    if (*(volatile unsigned char *)(uintptr_t)CAPTURE_NETWORK ||
        loadq(CAPTURE_DEVICE)!=m->expected_device || m->thread_changes || !m->render_context)
        error=1;
    else if (target<1u || target>COOP_CAPTURE_MAX_FRAMES || count>=target ||
             bytes!=((uint64_t)target+1u)*COOP_FRAME_BYTES || !data || (data&63u) || data>UINT64_MAX-bytes)
        error=2;
    else if (loadl(CAPTURE_WIDTH)!=3840 || loadl(CAPTURE_HEIGHT)!=2160 ||
             loadl(CAPTURE_BUFFER_COUNT)!=3 ||
             *(volatile unsigned char *)(uintptr_t)CAPTURE_FORMAT!=24) error=3;
    for (unsigned i=0;i<3 && !error;i++) {
        uint64_t p=loadq(CAPTURE_BUFFERS+i*32u);
        if (!p || p!=m->sources[i] || loadq(CAPTURE_BUFFERS+i*32u+8) ||
            loadq(CAPTURE_BUFFERS+i*32u+16) || loadq(CAPTURE_BUFFERS+i*32u+24) ||
            p>UINT64_MAX-COOP_FRAME_BYTES ||
            (data<p+COOP_FRAME_BYTES && p<data+bytes)) error=4;
    }
    if (!error && (loadl(CAPTURE_FRAME)!=s.frame || loadl(CAPTURE_CURRENT_BUFFER)!=s.next_buffer)) error=5;
    if (!error && !coop_render_view_valid(&source_view)) error=8;
    struct coop_capture_frame captures[COOP_CAPTURE_MAX_FRAMES];
    for (unsigned i=0;i<COOP_CAPTURE_MAX_FRAMES;i++) captures[i]=m->captures[i];
    if (!error && !coop_capture_sequence_next(captures,count,target,records[source].serial,records[source].frame)) error=9;
    if (!error) {
        m->source_index=(uint32_t)source; m->source_token=records[source].token;
        m->source_serial=records[source].serial; m->source_frame=records[source].frame;
        m->source_address=m->sources[source];
        uint64_t start=ticks();
        uint64_t destination=data+(uint64_t)count*COOP_FRAME_BYTES;
        uint64_t scratch=data+(uint64_t)target*COOP_FRAME_BYTES;
        copy_frame(destination,m->sources[source]);
        copy_frame(scratch,m->sources[source]);
        m->copies_equal=(uint32_t)same_frames(destination,scratch);
        m->copy_ticks=ticks()-start;
        m->frame_after=loadl(CAPTURE_FRAME); m->index_after=loadl(CAPTURE_CURRENT_BUFFER);
        if (!m->copies_equal) error=6;
        else if (m->frame_after!=s.frame || m->index_after!=s.next_buffer) error=7;
        if (!error) {
            struct coop_capture_frame capture={source_view,records[source].token,m->copy_ticks,
                (uint32_t)source,records[source].serial,records[source].frame,m->copies_equal};
            m->captures[count]=capture;
            m->capture_count=count+1u;
        }
    }
    m->error=error;
    __asm__ volatile("" ::: "memory");
    /* A bounded sequence keeps owning each destination until the host has
     * removed the hook. No slot is recycled or read live over the network.
     */
    m->state=error?4:(m->capture_count==target?3:1);
done:
#ifdef COOP_COMPACT_PIPELINE
    compact_pace();
#endif
    __atomic_store_n(&m->observer_busy,0,__ATOMIC_RELEASE);
}
