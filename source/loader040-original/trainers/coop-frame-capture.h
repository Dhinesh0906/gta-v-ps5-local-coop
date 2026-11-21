/* A completed-frame lease derived from GTA's existing presentation queue.
 * These helpers do not render, call the GPU, or change a camera.
 */
#ifndef GTA_COOP_FRAME_CAPTURE_H
#define GTA_COOP_FRAME_CAPTURE_H
#include <stdint.h>
#include <stddef.h>
#define COOP_FRAME_BYTES 33423360ull
#define COOP_FENCE_MASK 0x00ffffffffffffffull
#define COOP_CAPTURE_MAX_FRAMES 4u
/* Raw float bits avoid touching the renderer's floating-point/SIMD state.
 * This is an engine view-pose record, not a native camera handle/player ID.
 */
struct coop_render_view {
    uint32_t selector, valid, pose[16];
};
static inline int coop_render_view_valid(const struct coop_render_view *v) {
    if (v->selector>1u || !v->valid) return 0;
    for (unsigned i=0;i<16;i++)
        if ((v->pose[i]&0x7f800000u)==0x7f800000u) return 0;
    /* An inverse affine view matrix has a homogeneous final column. */
    return !(v->pose[3]&0x7fffffffu) && !(v->pose[7]&0x7fffffffu) &&
        !(v->pose[11]&0x7fffffffu) && v->pose[15]==0x3f800000u;
}
struct coop_capture_frame {
    struct coop_render_view view;
    uint64_t token, copy_ticks;
    uint32_t index, serial, frame, copies_equal;
};
static inline int coop_capture_sequence_next(const struct coop_capture_frame *captures,
                                             uint32_t count,uint32_t target,
                                             uint32_t serial,uint32_t frame) {
    if (target<1u || target>COOP_CAPTURE_MAX_FRAMES || count>=target) return 0;
    return !count || ((uint32_t)(serial-captures[count-1u].serial)==1u &&
                     (uint32_t)(frame-captures[count-1u].frame)==1u);
}
struct coop_present_snapshot {
    uint32_t frame, next_buffer, rotation, read_serial, write_serial, padding;
    uint64_t tokens[2], completed[8];
};
struct coop_present_record {
    uint64_t token;
    uint32_t serial, frame, valid, padding;
};
static inline int coop_present_record_frame(const struct coop_present_snapshot *s,
                                            struct coop_present_record records[3]) {
    if (s->rotation!=3 || s->next_buffer>=3 ||
        (uint32_t)(s->write_serial-s->read_serial)!=1u) return 0;
    unsigned just_submitted=(s->next_buffer+2u)%3u;
    uint64_t token=s->tokens[(s->write_serial-1u)&1u];
    if (token==UINT64_MAX || !(token&COOP_FENCE_MASK) || (token>>56)>=8) return 0;
    records[just_submitted]=(struct coop_present_record){token,s->write_serial,s->frame-1u,1,0};
    return 1;
}
static inline int coop_present_completed_source(const struct coop_present_snapshot *s,
                                                const struct coop_present_record records[3]) {
    if (s->rotation!=3 || s->next_buffer>=3 ||
        (uint32_t)(s->write_serial-s->read_serial)!=1u) return -1;
    unsigned index=(s->next_buffer+1u)%3u;
    const struct coop_present_record *r=&records[index];
    if (!r->valid || r->serial!=s->read_serial || r->token==UINT64_MAX ||
        r->token!=s->tokens[(s->read_serial-1u)&1u]) return -1;
    unsigned context=(unsigned)(r->token>>56);
    if (context>=8 || !(r->token&COOP_FENCE_MASK) ||
        (s->completed[context]&COOP_FENCE_MASK)<(r->token&COOP_FENCE_MASK)) return -1;
    return (int)index;
}
struct coop_frame_capture_mailbox {
    uint64_t magic, forward;
    volatile uint32_t state, error, enabled, cancel;
    uint64_t calls, tls, render_context;
    uint32_t tls_flags, thread_changes;
    struct coop_present_snapshot snapshot;
    struct coop_present_record records[3];
    uint64_t data, data_bytes, expected_device, expected_labels, sources[3];
    uint64_t copy_ticks, source_token, source_address;
    uint32_t source_index, source_serial, source_frame, frame_after, index_after;
    uint32_t copies_equal, rejected_snapshots, cancel_ack, observer_busy, contended;
    struct coop_render_view views[3];
    uint32_t capture_target, capture_count;
    struct coop_capture_frame captures[COOP_CAPTURE_MAX_FRAMES];
};
_Static_assert(offsetof(struct coop_frame_capture_mailbox,forward)==8,"forward slot");
_Static_assert(offsetof(struct coop_frame_capture_mailbox,snapshot)==64,"snapshot offset");
_Static_assert(offsetof(struct coop_frame_capture_mailbox,records)==168,"record offset");
_Static_assert(offsetof(struct coop_frame_capture_mailbox,data)==240,"owned-buffer offset");
_Static_assert(offsetof(struct coop_frame_capture_mailbox,copy_ticks)==296,"copy result offset");
_Static_assert(offsetof(struct coop_frame_capture_mailbox,views)==360,"view history offset");
_Static_assert(offsetof(struct coop_frame_capture_mailbox,capture_target)==576,"capture target offset");
_Static_assert(offsetof(struct coop_frame_capture_mailbox,captures)==584,"capture metadata offset");
_Static_assert(sizeof(struct coop_capture_frame)==104,"capture metadata stride");
_Static_assert(sizeof(struct coop_frame_capture_mailbox)==1000,"capture mailbox size");
#endif
