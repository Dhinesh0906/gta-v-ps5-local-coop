/* One-shot private input-mapper fixture. No entity tasks or player writes. */
#include "coop-engine-input.c"
typedef struct {
    uint64_t magic,forward,data,data_bytes,mapper;
    volatile uint32_t state,error,busy,visits;
    uint32_t clone_result,bindings,passes,reserved;
    int32_t pressed[6],released[6];
} EngineInputTrial;
_Static_assert(sizeof(EngineInputTrial)==120,"input trial mailbox");
#ifndef COOP_ENGINE_SECTION
#define COOP_ENGINE_SECTION(s) __attribute__((section(s),used,visibility("hidden")))
#endif
COOP_ENGINE_SECTION(".probe.data") volatile EngineInputTrial input_trial;
#ifndef COOP_ENGINE_EXECUTE
#define COOP_ENGINE_EXECUTE(mapper,group,now) ((void(*)(void*,uint32_t))(uintptr_t)(mapper))(group,now)
#endif
static void trial_values(CoopEngineInput *input,volatile int32_t *values){
    const unsigned actions[6]={21,22,23,24,71,72};
    for(unsigned n=0;n<6;n++)values[n]=input->actions[actions[n]].value;
}
COOP_ENGINE_SECTION(".entry") void _start(void *context){
    input_trial.visits++;
    if(input_trial.magic==0x3154504e49474e45ull&&input_trial.state==1){
        input_trial.state=2;input_trial.busy=1;
        if(!input_trial.data||(input_trial.data&63)||input_trial.data_bytes!=0xa0000||
           input_trial.data>UINTPTR_MAX-input_trial.data_bytes||!input_trial.mapper)
            input_trial.error=1;
        else{
            CoopEngineInput *input=(void*)(uintptr_t)input_trial.data;
            uint8_t *source=(void*)(uintptr_t)(input_trial.data+0x90000);
            uint8_t *mapper=(void*)(uintptr_t)input_trial.mapper;
            const CoopEngineSource neutral={-1,UINT32_MAX,-3};
            input_trial.clone_result=coop_engine_input_clone(input,sizeof(*input),source,
                COOP_ENGINE_SOURCE_BYTES,0x57c1610,1,neutral,10000);
            if(input_trial.clone_result)input_trial.error=2;
            else{
                uint64_t *vtable=(void*)(mapper+0x2800),*pointers=(void*)(mapper+0x2020);
                for(unsigned n=0;n<4;n++){
                    uint8_t *pad=mapper+0x2100+n*0x98;
                    uint64_t vt=(uint64_t)(uintptr_t)vtable;engine_copy(pad,&vt,8);
                    pad[0x14]=pad[0x15]=pad[0x16]=pad[0x17]=128;
                    pointers[n]=(uint64_t)(uintptr_t)pad;
                    vtable[(0x60+n*8)/8]=(uint64_t)(uintptr_t)(mapper+0x1200+n*0x40);
                }
                for(unsigned g=0;g<7;g++)input_trial.bindings+=(uint32_t)input->groups[g].count;
                uint8_t *secondary=mapper+0x2100+0x98;
                /* Engine-normalized Cross and full R2, not actual user input. */
                secondary[0xc]=64;secondary[0x19]=255;
                for(unsigned g=0;g<7;g++)COOP_ENGINE_EXECUTE(input_trial.mapper,&input->groups[g],10001);
                input_trial.passes++;trial_values(input,input_trial.pressed);
                secondary[0xc]=0;secondary[0x19]=0;
                for(unsigned g=0;g<7;g++)COOP_ENGINE_EXECUTE(input_trial.mapper,&input->groups[g],10002);
                input_trial.passes++;trial_values(input,input_trial.released);
                if(input_trial.pressed[0]!=255||input_trial.pressed[1]||input_trial.pressed[2]||
                   input_trial.pressed[3]!=255||input_trial.pressed[4]!=255||input_trial.pressed[5])
                    input_trial.error=3;
                for(unsigned n=0;n<6;n++)if(input_trial.released[n])input_trial.error=4;
            }
        }
        __asm__ volatile("" ::: "memory");input_trial.busy=0;input_trial.state=3;
    }
    if(input_trial.forward)((void(*)(void*))(uintptr_t)input_trial.forward)(context);
}
