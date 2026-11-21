/* Observe the existing DcbSetFlip call without changing any argument or
 * adding a GPU command. Ordinary five-argument integer/pointer platform ABI.
 */
#include <stdint.h>
#include <stddef.h>
struct flip_status { uint64_t count;uint32_t pending;int32_t current;uint32_t graphics;int32_t result; };
struct flip_sample {
    uint64_t sequence,tls,command,argument,result;
    uint32_t frame,handle,index,mode;
    struct flip_status before,after;
};
struct flip_observer {
    uint64_t magic,forward,status_function;
    uint32_t handle,enabled;
    volatile uint32_t busy,contended;
    uint64_t calls,foreign,published;
    struct flip_sample samples[64];
};
_Static_assert(sizeof(struct flip_sample)==104,"flip sample size");
_Static_assert(offsetof(struct flip_observer,samples)==64,"flip samples offset");
_Static_assert(sizeof(struct flip_observer)==6720,"flip observer size");
#ifdef COOP_FLIP_TEST
#define SECTION(name)
static int get_status(uint32_t,void*);
static uint64_t get_tls(void);
static uint32_t get_frame(void);
static uint64_t forward_flip(void*,uint32_t,int32_t,uint32_t,int64_t);
#else
#define SECTION(name) __attribute__((section(name),used,visibility("hidden")))
#endif
SECTION(".probe.data") volatile struct flip_observer flip_observer;
#ifndef COOP_FLIP_TEST
static int get_status(uint32_t handle,void *data) {
    return ((int(*)(int,void*))flip_observer.status_function)((int)handle,data);
}
static uint64_t get_tls(void) { uint64_t result;__asm__ volatile("movq %%fs:0,%0":"=r"(result));return result; }
static uint32_t get_frame(void) { return *(volatile uint32_t*)0x62ae210ull; }
static uint64_t forward_flip(void *command,uint32_t handle,int32_t index,uint32_t mode,int64_t argument) {
    return ((uint64_t(*)(void*,uint32_t,int32_t,uint32_t,int64_t))flip_observer.forward)(command,handle,index,mode,argument);
}
#endif
static struct flip_status status(uint32_t handle) {
    uint64_t raw[16]={0};int result=get_status(handle,raw);
    const unsigned char *bytes=(const unsigned char*)raw;
    struct flip_status s={raw[0],*(const uint32_t*)(bytes+52),*(const int32_t*)(bytes+56),*(const uint32_t*)(bytes+48),result};
    return s;
}
SECTION(".entry") uint64_t _start(void *command,uint32_t handle,int32_t index,uint32_t mode,int64_t argument) {
    volatile struct flip_observer *m=&flip_observer;
    if(m->magic!=0x3153424f50494c46ull||!m->enabled)return forward_flip(command,handle,index,mode,argument);
    if(handle!=m->handle){__atomic_fetch_add(&m->foreign,1,__ATOMIC_RELAXED);return forward_flip(command,handle,index,mode,argument);}
    if(__atomic_exchange_n(&m->busy,1,__ATOMIC_ACQUIRE)){
        __atomic_fetch_add(&m->contended,1,__ATOMIC_RELAXED);return forward_flip(command,handle,index,mode,argument);
    }
    uint64_t sequence=++m->calls;
    struct flip_sample sample={0};sample.sequence=sequence;sample.tls=get_tls();sample.command=(uint64_t)command;
    sample.frame=get_frame();sample.handle=handle;sample.index=(uint32_t)index;sample.mode=mode;sample.argument=(uint64_t)argument;
    sample.before=status(handle);
    sample.result=forward_flip(command,handle,index,mode,argument);
    sample.after=status(handle);
    m->samples[(sequence-1u)&63u]=sample;
    __atomic_store_n(&m->published,sequence,__ATOMIC_RELEASE);
    __atomic_store_n(&m->busy,0,__ATOMIC_RELEASE);
    return sample.result;
}
