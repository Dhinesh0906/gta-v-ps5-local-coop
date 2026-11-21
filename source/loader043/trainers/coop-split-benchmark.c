/* One-shot CPU benchmark, not a renderer hook. Writes only the mailbox and
 * three host-allocated fixture surfaces, then forwards the resident menu.
 */
#include "coop-split-compositor.c"
struct split_benchmark {
    uint64_t magic, forward;
    volatile uint32_t state, error, visits, busy;
    uint64_t data, data_bytes, tsc_frequency, fill_cycles;
    uint64_t compose_cycles[3];
    uint32_t results[3], checked_pixels;
};
_Static_assert(sizeof(struct split_benchmark)==104,"benchmark mailbox size");
#ifndef COOP_BENCH_SECTION
#define COOP_BENCH_SECTION(name) __attribute__((section(name),used,visibility("hidden")))
#endif
COOP_BENCH_SECTION(".probe.data")
volatile struct split_benchmark benchmark;

#ifndef COOP_BENCH_TICKS
#define COOP_BENCH_TICKS() ((uint64_t (*)(void))0x37b9990ull)()
#define COOP_BENCH_FREQUENCY() ((uint64_t (*)(void))0x37ba560ull)()
#define COOP_BENCH_NETWORK() (*(volatile unsigned char *)0x5abefb8ull)
#endif

static uint64_t ticks(void) {
    return COOP_BENCH_TICKS();
}

COOP_BENCH_SECTION(".entry")
void _start(void *context) {
    benchmark.visits++;
    if (benchmark.magic==0x3148434e45424353ull && benchmark.state==1) {
        benchmark.state=2;
        benchmark.busy=1;
        uint64_t data=benchmark.data;
        if (COOP_BENCH_NETWORK()) benchmark.error=1;
        else if (!data || (data&63u) || benchmark.data_bytes != 3u*COOP_SPLIT_BYTES ||
                 data>UINTPTR_MAX-benchmark.data_bytes) benchmark.error=2;
        else {
            benchmark.tsc_frequency=COOP_BENCH_FREQUENCY();
            if (benchmark.tsc_frequency<1000000ull || benchmark.tsc_frequency>10000000000ull)
                benchmark.error=3;
            else {
                uint32_t *a=(uint32_t *)(uintptr_t)data;
                uint32_t *b=(uint32_t *)(uintptr_t)(data+COOP_SPLIT_BYTES);
                uint32_t *out=(uint32_t *)(uintptr_t)(data+2u*COOP_SPLIT_BYTES);
                uint64_t start=ticks();
                for (size_t i=0;i<COOP_SPLIT_BYTES/4u;i++) {
                    a[i]=(uint32_t)(i*2654435761u)^0xc0a71abdu;
                    b[i]=(uint32_t)(i*2246822519u)^0x3f1248e9u;
                    out[i]=0x87654321u;
                }
                benchmark.fill_cycles=ticks()-start;
                struct coop_split_capture captures[2]={
                    {{a,COOP_SPLIT_BYTES,3840,2160,2176,24},101,200,1,3},
                    {{b,COOP_SPLIT_BYTES,3840,2160,2176,24},202,199,2,3}
                };
                struct coop_split_surface destination={out,COOP_SPLIT_BYTES,3840,2160,2176,24};
                for (unsigned n=0;n<3;n++) {
                    start=ticks();
                    benchmark.results[n]=coop_split_compose(captures,&destination,200);
                    benchmark.compose_cycles[n]=ticks()-start;
                    if (benchmark.results[n]) benchmark.error=4;
                }
                /* Fixed externally computed tile-address sentinels. Full
                 * pixel correctness is covered by the independent host test.
                 * source y=540 -> upper y=0, lower y=1080.
                 */
                if (out[0]!=a[1967312] || out[3933632]!=b[1967312]) benchmark.error=5;
                else benchmark.checked_pixels=2;
            }
        }
        __asm__ volatile("" ::: "memory");
        benchmark.busy=0;
        benchmark.state=3;
    }
    if (benchmark.forward) ((void (*)(void *))(uintptr_t)benchmark.forward)(context);
}
