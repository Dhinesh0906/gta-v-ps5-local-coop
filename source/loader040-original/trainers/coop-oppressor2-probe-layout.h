/* Independent bounded oppressor2 experiment; not a replacement co-op owner.
 * The actual player/vehicle handles are only read for exclusion. (AI-assisted) */
#pragma once
#include <stdint.h>
#define OPPRESSOR_TABLE_BYTES 0x680u /* Complete CBike table plus ABI prefix. */
#define OPPRESSOR_COPY_BASE 0x61c00000ULL
#define OPPRESSOR_PROBE_MAGIC 0x315242505032504fULL
#define OPPRESSOR_HELPER_MAGIC 0x3154504e4932504fULL
typedef struct {
 uint64_t magic,forward,owner,helper;
 uint32_t stop,busy,state,error,request,completed,model,vehicle;
 uint32_t pilot,pilot_model,model_owned,mode,started,last_frame,started_frame,steps;
 uint32_t created,deleted,pilots_created,pilots_deleted,restored,axis,frames,full_rotor;
 float value,original[4],values[4],rotation_start[3],rotation_end[3],velocity_start[3],velocity_end[3];
 uint64_t entity;
} OppressorProbe;
_Static_assert(sizeof(OppressorProbe)==224,"Oppressor probe ABI");
typedef struct {uint64_t magic,probe,entity,shadow;uint32_t stopped,busy,calls,applied,rejected,reason;float before[4],after[4];} OppressorInput;
_Static_assert(sizeof(OppressorInput)==88,"Oppressor input ABI");
