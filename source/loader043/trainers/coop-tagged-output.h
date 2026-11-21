/* Ownership follows an actual displayed user tag plus the matching submitted
 * GPU fence. Recording a DcbSetFlip command alone is never completion proof. */
#ifndef COOP_TAGGED_OUTPUT_H
#define COOP_TAGGED_OUTPUT_H
#include <stdint.h>
#define COOP_TAGGED_SLOTS 5u
struct coop_tagged_slot {uint32_t ticket,associated;uint64_t token;};
static inline uint64_t coop_output_tag(uint32_t cookie,uint32_t ticket){return ((uint64_t)cookie<<32)|ticket;}
static inline int coop_tagged_retired(const struct coop_tagged_slot *s,uint32_t cookie,
    uint64_t displayed_tag,uint32_t current,unsigned slot,int fence_complete){
 if(slot>=COOP_TAGGED_SLOTS||current==slot+3u)return 0;
 if(!s->ticket)return 1;
 return s->associated==s->ticket&&s->token&&fence_complete&&
    (uint32_t)(displayed_tag>>32)==cookie&&(uint32_t)displayed_tag>s->ticket;
}
static inline int coop_tagged_can_release(uint32_t cookie,uint32_t last_owned,uint32_t associated,
    uint64_t token,uint64_t displayed_tag,uint32_t current,int fence_complete,uint32_t pending){
 if(pending)return 0;
 if(!last_owned)return current<3;
 return current<3&&associated==last_owned&&token&&fence_complete&&
    (uint32_t)(displayed_tag>>32)==cookie&&(uint32_t)displayed_tag>last_owned;
}
#endif
