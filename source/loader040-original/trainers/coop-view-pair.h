/* Capture ownership for a continuous two-camera compositor. This module does
 * not render/select cameras or prove GPU completion. A producer must provide
 * those actual witnesses before publishing. No frame-parity camera guesses. */
#ifndef GTA_COOP_VIEW_PAIR_H
#define GTA_COOP_VIEW_PAIR_H
#include <stdint.h>
#include <stdatomic.h>
#define COOP_PAIR_SLOTS 4u
#define COOP_PAIR_FRAME_BYTES 33423360ull
#define COOP_PAIR_CAMERA_VERIFIED 1u
#define COOP_PAIR_GPU_COMPLETE 2u
#define COOP_PAIR_STABLE_COPY 4u
#define COOP_PAIR_WITNESSES 7u
/* Owned copy covers the declared 512..1663 tile rows, not a full-frame export. */
#define COOP_PAIR_CENTRAL_COPY 8u
struct coop_pair_frame {
    uint64_t camera, gpu_token;
    uint32_t epoch, player, frame, serial, witnesses;
};
struct coop_pair_slot {
    _Atomic uint32_t state, generation;
    uint64_t address;
    struct coop_pair_frame frame;
};
struct coop_view_pair {
    _Atomic uint32_t sequence;
    _Atomic uint32_t epoch;
    _Atomic uint64_t cameras[2];
    _Atomic uint32_t cursor[2];
    struct coop_pair_slot slots[COOP_PAIR_SLOTS];
};
struct coop_pair_lease {uint32_t slot,generation,epoch,sequence;};
struct coop_pair {struct coop_pair_lease lease[2];uint64_t address[2];struct coop_pair_frame frame[2];};
enum coop_pair_result {COOP_PAIR_OK,COOP_PAIR_WAIT,COOP_PAIR_BAD_INPUT,COOP_PAIR_BUSY,COOP_PAIR_STALE_LEASE};
enum coop_pair_slot_state {COOP_PAIR_FREE,COOP_PAIR_WRITING,COOP_PAIR_READY,COOP_PAIR_HELD};
int coop_pair_configure(struct coop_view_pair*,uint32_t,const uint64_t[2],const uint64_t[4]);
int coop_pair_begin_write(struct coop_view_pair*,uint32_t,struct coop_pair_lease*);
int coop_pair_publish(struct coop_view_pair*,const struct coop_pair_lease*,const struct coop_pair_frame*);
int coop_pair_abort_write(struct coop_view_pair*,const struct coop_pair_lease*);
int coop_pair_acquire(struct coop_view_pair*,uint32_t,uint32_t,uint32_t,struct coop_pair*);
int coop_pair_release(struct coop_view_pair*,const struct coop_pair*);
#endif
