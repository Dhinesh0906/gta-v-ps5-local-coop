/* Independent bounded glider experiment; not a replacement co-op owner.
 * The actual player/vehicle handles are only read for exclusion. (AI-assisted) */
#pragma once
#include <stdint.h>
#define GLIDER_TABLE_BYTES 0x680u /* Complete CBike table plus ABI prefix. */
#define GLIDER_COPY_BASE 0x62200000ULL
#define GLIDER_PROBE_MAGIC 0x3152425044494c47ULL
#define GLIDER_HELPER_MAGIC 0x3154504e494c4743ULL
typedef struct {
 uint64_t magic,forward,owner,helper;
 uint32_t stop,busy,state,error,request,completed,model,vehicle;
 uint32_t pilot,pilot_model,model_owned,mode,started,last_frame,started_frame,steps;
 uint32_t created,deleted,pilots_created,pilots_deleted,restored,axis,frames,full_rotor;
 float value,original[4],values[4],rotation_start[3],rotation_end[3],velocity_start[3],velocity_end[3];
 uint64_t entity;
} GliderProbe;
_Static_assert(sizeof(GliderProbe)==224,"Glider probe ABI");
typedef struct {uint64_t magic,probe,entity,shadow;uint32_t stopped,busy,calls,applied,rejected,reason;float before[4],after[4];} GliderInput;
_Static_assert(sizeof(GliderInput)==88,"Glider input ABI");
