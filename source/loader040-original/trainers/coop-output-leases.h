/* CPU compositor/scanout ownership for a single observed presentation queue.
 * Observations must be made after the previous DcbSetFlip was submitted.
 * No API calls, game memory access, or claims of GPU completion occur here.
 */
#ifndef GTA_COOP_OUTPUT_LEASES_H
#define GTA_COOP_OUTPUT_LEASES_H
#include <stdint.h>
#define COOP_OUTPUT_SLOTS 4u
enum coop_output_state { COOP_OUTPUT_FREE,COOP_OUTPUT_WRITING,COOP_OUTPUT_READY,COOP_OUTPUT_QUEUED };
struct coop_flip_observation { uint64_t count;uint32_t pending;int32_t current; };
struct coop_output_leases {
    uint64_t observed_count,submitted_total,queued_at[COOP_OUTPUT_SLOTS];
    uint32_t initialized,fault,state[COOP_OUTPUT_SLOTS];
    int32_t current;
};
static inline int coop_output_observe(struct coop_output_leases *l,struct coop_flip_observation s) {
    if(!l || l->fault)return 0;
    if(s.pending>16u || s.current<0 || s.current>6 || s.count>UINT64_MAX-s.pending){l->fault=1;return 0;}
    uint64_t total=s.count+s.pending;
    if(l->initialized && (s.count<l->observed_count || total!=l->submitted_total)){
        l->fault=2;return 0; /* another submit, a missing submit, reset, or stale observation */
    }
    l->initialized=1;l->observed_count=s.count;l->submitted_total=total;l->current=s.current;
    for(unsigned i=0;i<COOP_OUTPUT_SLOTS;i++)if(l->state[i]==COOP_OUTPUT_QUEUED &&
        l->queued_at[i]<=s.count && s.current!=(int32_t)i+3)l->state[i]=COOP_OUTPUT_FREE;
    return 1;
}
static inline int coop_output_begin_write(struct coop_output_leases *l) {
    if(!l || !l->initialized || l->fault)return -1;
    for(unsigned i=0;i<COOP_OUTPUT_SLOTS;i++)if(l->state[i]==COOP_OUTPUT_FREE && l->current!=(int32_t)i+3){
        l->state[i]=COOP_OUTPUT_WRITING;return (int)i;
    }
    return -1;
}
static inline int coop_output_publish(struct coop_output_leases *l,unsigned slot) {
    if(!l || l->fault || slot>=COOP_OUTPUT_SLOTS || l->state[slot]!=COOP_OUTPUT_WRITING)return 0;
    /* The adapter must fence its actual pixel writes before publishing READY. */
    l->state[slot]=COOP_OUTPUT_READY;return 1;
}
static inline int coop_output_submit(struct coop_output_leases *l,int32_t index) {
    if(!l || !l->initialized || l->fault || index<0 || index>6 || l->submitted_total==UINT64_MAX)return 0;
    if(index>=3 && l->state[index-3]!=COOP_OUTPUT_READY)return 0;
    l->submitted_total++;
    if(index>=3){unsigned slot=(unsigned)index-3u;l->state[slot]=COOP_OUTPUT_QUEUED;l->queued_at[slot]=l->submitted_total;}
    return 1;
}
static inline int coop_output_can_unregister(const struct coop_output_leases *l) {
    if(!l || !l->initialized || l->fault || l->current>=3)return 0;
    for(unsigned i=0;i<COOP_OUTPUT_SLOTS;i++)if(l->queued_at[i]>l->observed_count || l->state[i]==COOP_OUTPUT_WRITING)return 0;
    return 1;
}
#endif
