/* Independent bounded blimp experiment; not a replacement co-op owner.
 * The actual player/vehicle handles are only read for exclusion. (AI-assisted) */
#pragma once
#include <stdint.h>
#define BLIMP_TABLE_BYTES 0x6b8u /* Prefix + all class-9 slots through 0x6a0. (AI-assisted) */
#define BLIMP_COPY_BASE 0x61600000ULL
#define BLIMP_PROBE_MAGIC 0x315242504d494c42ULL
#define BLIMP_HELPER_MAGIC 0x3154504d494c4243ULL
typedef struct {
 uint64_t magic,forward,owner,helper;
 uint32_t stop,busy,state,error,request,completed,model,vehicle;
 uint32_t pilot,pilot_model,model_owned,mode,started,last_frame,started_frame,steps;
 uint32_t created,deleted,pilots_created,pilots_deleted,restored,axis,frames,full_rotor;
 float value,original[4],values[4],rotation_start[3],rotation_end[3],velocity_start[3],velocity_end[3];
 uint64_t entity;
} BlimpProbe;
_Static_assert(sizeof(BlimpProbe)==224,"Blimp probe ABI");
typedef struct {uint64_t magic,probe,entity,shadow;uint32_t stopped,busy,calls,applied,rejected,reason;float before[4],after[4];} BlimpInput;
_Static_assert(sizeof(BlimpInput)==88,"Blimp input ABI");
