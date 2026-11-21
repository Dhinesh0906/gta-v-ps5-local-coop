/* Bounded output-surface lifecycle check. No flips, game-pixel writes,
 * camera changes or persistent hooks. Mode 0 resolves/observes only;
 * mode 1 also allocates and registers an owned, never-presented buffer set.
 */
#include <stdint.h>
#include <stddef.h>
#ifndef OUTPUT_WIDTH
#define OUTPUT_WIDTH 3840u
#define OUTPUT_HEIGHT 2160u
#define OUTPUT_STRIDE 33423360ull
#endif
#define OUTPUT_BYTES (OUTPUT_STRIDE*3u)
_Static_assert(OUTPUT_BYTES%0x20000u==0,"direct memory alignment");
struct output_buffer { uint64_t data, reserved[3]; };
struct output_probe {
    uint64_t magic,forward;
    volatile uint32_t state,error,visits,busy;
    uint32_t mode,handle,module_count,module_handle;
    uint64_t list_function,resolve_function,video_start,video_end,expected_register;
    uint64_t unregister_function,status_function;
    int32_t list_result,resolve_result,allocate_result,map_result;
    int32_t register_result,unregister_result,unmap_result,release_result;
    int32_t status_results[2];
    uint32_t physical_owned,mapped,registered,cleanup_attempts;
    uint64_t physical,address,bytes;
    struct output_buffer buffers[3];
    uint64_t attribute[10],status_before[16],status_after[16];
};
_Static_assert(offsetof(struct output_probe,mode)==32,"output mode");
_Static_assert(offsetof(struct output_probe,list_function)==48,"output functions");
_Static_assert(offsetof(struct output_probe,physical)==160,"output memory");
_Static_assert(offsetof(struct output_probe,buffers)==184,"output buffers");
_Static_assert(offsetof(struct output_probe,status_before)==360,"output status");
_Static_assert(sizeof(struct output_probe)==616,"output mailbox");
#ifdef COOP_OUTPUT_TEST
#define SECTION(name)
#else
#define SECTION(name) __attribute__((section(name),used,visibility("hidden")))
#endif
SECTION(".probe.data") volatile struct output_probe output;
#ifdef COOP_OUTPUT_TEST
static int output_network(void);
static int platform_status(uint64_t,int,void*);
static int platform_allocate(uint64_t*);
static int platform_map(uint64_t,uint64_t*);
static int platform_register(int,struct output_buffer*,void*);
static int platform_unregister(uint64_t,int);
static int platform_unmap(uint64_t);
static int platform_release(uint64_t);
static void platform_attribute(void*);
static void platform_forward(void*);
#else
static int output_network(void) { return *(volatile unsigned char*)0x5abefb8ull; }
static int platform_status(uint64_t function,int handle,void *status) {
    return ((int(*)(int,void*))function)(handle,status);
}
static int platform_allocate(uint64_t *physical) {
    return ((int(*)(uint64_t,uint64_t,int,uint64_t*))0x37ba870ull)(OUTPUT_BYTES,0x20000,3,physical);
}
static int platform_map(uint64_t physical,uint64_t *address) {
    return ((int(*)(uint64_t*,uint64_t,int,int,uint64_t,uint64_t))0x37ba7e0ull)
        (address,OUTPUT_BYTES,0x33,0,physical,0x20000);
}
static int platform_register(int handle,struct output_buffer *buffers,void *attribute) {
    return ((int(*)(int,int,int,void*,int,void*,int,void*))0x37ba8a0ull)
        (handle,1,3,buffers,3,attribute,0,0);
}
static int platform_unregister(uint64_t function,int handle) {
    return ((int(*)(int,int))function)(handle,1);
}
static int platform_unmap(uint64_t address) {
    return ((int(*)(void*,uint64_t))0x37ba7b0ull)((void*)address,OUTPUT_BYTES);
}
static int platform_release(uint64_t physical) {
    return ((int(*)(uint64_t,uint64_t))0x37ba7f0ull)(physical,OUTPUT_BYTES);
}
static void platform_attribute(void *attribute) {
    ((void(*)(void*,uint64_t,uint32_t,uint32_t,uint32_t,uint64_t,uint32_t,uint64_t))0x37ba890ull)
        (attribute,0x8100070422000000ull,0,OUTPUT_WIDTH,OUTPUT_HEIGHT,0,0,0);
}
static void platform_forward(void *context) {
    if (output.forward) ((void(*)(void*))output.forward)(context);
}
#endif
static uint64_t pointer(const volatile void *p) {
    uint64_t result=(uint64_t)p;__asm__ volatile("" : "+r"(result));return result;
}
static int in_video(uint64_t p) { return p>=output.video_start && p<output.video_end; }
static void cleanup_output(void) {
    output.cleanup_attempts++;
    if (output.registered) {
        output.unregister_result=platform_unregister(output.unregister_function,(int)output.handle);
        if (output.unregister_result) { output.error=10;return; }
        output.registered=0;
    }
    if (output.mapped) {
        output.unmap_result=platform_unmap(output.address);
        if (output.unmap_result) { output.error=11;return; }
        output.mapped=0;
    }
    if (output.physical_owned) {
        output.release_result=platform_release(output.physical);
        if (output.release_result) { output.error=12;return; }
        output.physical_owned=0;
    }
}
static void check_output(void) {
    if (output_network() || output.mode>2u) { output.error=1;return; }
    if (output.mode==2u) { cleanup_output();return; }
    if (output.physical_owned || output.mapped || output.registered ||
        output.video_start>=output.video_end || !in_video(output.expected_register) ||
        !in_video(output.unregister_function) || !in_video(output.status_function)) {
        output.error=2;return;
    }
    uint64_t status=output.status_function;
    output.status_results[0]=platform_status(status,(int)output.handle,(void*)pointer(output.status_before));
    if (output.status_results[0] || ((uint32_t*)pointer(output.status_before))[14]>2u) {
        output.error=7;return;
    }
    if (!output.mode) return;
    uint64_t physical=UINT64_MAX,address=0;
    output.bytes=OUTPUT_BYTES;
    output.allocate_result=platform_allocate(&physical);
    if (output.allocate_result) { output.error=8;return; }
    output.physical=physical;output.physical_owned=1;
    output.map_result=platform_map(physical,&address);
    if (output.map_result) { output.error=9;cleanup_output();return; }
    output.address=address;output.mapped=1;
    if (!address || (address&0x1ffffu) || address>UINT64_MAX-OUTPUT_BYTES) {
        output.error=13;cleanup_output();return;
    }
    for (unsigned i=0;i<3;i++) output.buffers[i].data=address+i*OUTPUT_STRIDE;
    platform_attribute((void*)pointer(output.attribute));
    output.register_result=platform_register((int)output.handle,
        (struct output_buffer*)pointer(output.buffers),(void*)pointer(output.attribute));
    if (!output.register_result) output.registered=1;
    else output.error=14;
    /* These surfaces are never flipped. Unregister the owned set before
     * releasing mappings; a cleanup failure retains the remaining resources.
     */
    cleanup_output();
    output.status_results[1]=platform_status(status,(int)output.handle,(void*)pointer(output.status_after));
    if (output.status_results[1] || ((uint32_t*)pointer(output.status_after))[14]>2u) output.error=15;
}
SECTION(".entry") void _start(void *context) {
    output.visits++;
    uint32_t armed=1;
    if (output.magic==0x3154554f504f4f43ull &&
        __atomic_compare_exchange_n(&output.state,&armed,2,0,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED)) {
        output.busy=1;check_output();output.busy=0;
        __atomic_store_n(&output.state,3,__ATOMIC_RELEASE);
    }
    platform_forward(context);
}
