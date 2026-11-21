#include "coop-engine-input.h"

static void engine_copy(void *destination,const void *source,size_t length) {
    uint8_t *out=destination;const uint8_t *in=source;
    for(size_t i=0;i<length;i++)out[i]=in[i];
}
static uint32_t engine_u32(const uint8_t *p) {
    return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}
static uint64_t engine_u64(const uint8_t *p) {
    return (uint64_t)engine_u32(p)|(uint64_t)engine_u32(p+4)<<32;
}
static int engine_overlap(uintptr_t a,size_t an,uintptr_t b,size_t bn) {
    return a<b+bn && b<a+an;
}

int coop_engine_input_clone(CoopEngineInput *out,size_t out_bytes,
    const void *source,size_t source_bytes,uint64_t source_address,
    int device,CoopEngineSource neutral,uint32_t now) {
    const uint8_t *in=source;
    if(!out||!in||out_bytes<sizeof(*out)||source_bytes!=COOP_ENGINE_SOURCE_BYTES||
       (uintptr_t)out%_Alignof(CoopEngineInput)||
       (uintptr_t)out>UINTPTR_MAX-sizeof(*out)||
       (uintptr_t)in>UINTPTR_MAX-source_bytes||
       source_address>UINT64_MAX-source_bytes||neutral.kind!=-1||
       neutral.code!=UINT32_MAX||neutral.device!=-3)return COOP_ENGINE_INPUT_ARGUMENT;
    if(device<0||device>3)return COOP_ENGINE_INPUT_DEVICE;
    if(engine_overlap((uintptr_t)out,sizeof(*out),(uintptr_t)in,source_bytes)||
       engine_overlap((uintptr_t)out,sizeof(*out),(uintptr_t)source_address,source_bytes))
        return COOP_ENGINE_INPUT_ALIAS;
    uint64_t first_action=source_address+COOP_ENGINE_ACTION_OFFSET;
    /* Complete validation precedes any destination mutation. This version
     * accepts inherited gamepad-device bindings only; other source encodings
     * need their own routing audit, instead of silently consuming P1 input.
     */
    for(unsigned g=0;g<COOP_ENGINE_GROUPS;g++){
        const uint8_t *group=in+COOP_ENGINE_GROUP_OFFSET+g*0x9c8;
        int32_t count=(int32_t)engine_u32(group),original_device=(int32_t)engine_u32(group+4);
        if(count<0||count>COOP_ENGINE_BINDINGS)return COOP_ENGINE_INPUT_BINDING;
        if(original_device<0||original_device>3||original_device==device)
            return COOP_ENGINE_INPUT_DEVICE;
        for(int32_t n=0;n<count;n++){
            uint32_t kind=engine_u32(group+8+n*4),packed=engine_u32(group+0x278+n*4);
            uint64_t target=engine_u64(group+0x4e8+n*8);
            if(kind>24||(int32_t)packed>>24 != -1||target<first_action||
               target-first_action>=COOP_ENGINE_ACTIONS*72u||(target-first_action)%72u)
                return COOP_ENGINE_INPUT_BINDING;
        }
    }
    for(unsigned n=0;n<COOP_ENGINE_ACTIONS;n++){
        const uint8_t *original=in+COOP_ENGINE_ACTION_OFFSET+n*72;
        CoopEngineAction *action=&out->actions[n];
        engine_copy(action,original,sizeof(*action));
        action->value=action->previous=action->pending=0;
        action->source=action->previous_source=action->pending_source=neutral;
        action->locked_kind=-1;action->generation=0;action->history_index=0;
        action->flags=(action->flags&0xf8u)|1u;action->disabled_until=0;
        action->history=engine_u64(original+0x40)?(uint64_t)(uintptr_t)out->histories[n]:0;
        for(unsigned h=0;h<COOP_ENGINE_HISTORY;h++)
            out->histories[n][h]=(CoopEngineHistory){.value=0,.time=now,.source=neutral};
    }
    for(unsigned g=0;g<COOP_ENGINE_GROUPS;g++){
        const uint8_t *original=in+COOP_ENGINE_GROUP_OFFSET+g*0x9c8;
        CoopEngineBindingGroup *group=&out->groups[g];
        engine_copy(group,original,sizeof(*group));group->device=device;
        for(int32_t n=0;n<group->count;n++){
            uint64_t index=(engine_u64(original+0x4e8+n*8)-first_action)/72u;
            group->destinations[n]=(uint64_t)(uintptr_t)&out->actions[index];
        }
        /* Unused capacity cannot retain pointers back into the original. */
        for(unsigned n=(unsigned)group->count;n<COOP_ENGINE_BINDINGS;n++){
            group->kinds[n]=0;group->codes[n]=0;group->destinations[n]=0;
        }
    }
    out->generation=0;
    return COOP_ENGINE_INPUT_OK;
}
