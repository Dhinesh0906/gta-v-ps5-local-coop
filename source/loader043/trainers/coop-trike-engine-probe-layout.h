/* Independent bounded trike_engine experiment; not a replacement co-op owner.
 * The actual player/vehicle handles are only read for exclusion. (AI-assisted) */
#pragma once
#include <stdint.h>
#define TRIKE_ENGINE_TABLE_BYTES 0x690u /* Complete CBike table plus ABI prefix. */
#define TRIKE_ENGINE_COPY_BASE 0x63000000ULL
#define TRIKE_ENGINE_PROBE_MAGIC 0x31524250454b5254ULL
#define TRIKE_ENGINE_HELPER_MAGIC 0x3154504e454b5254ULL
typedef struct {
 uint64_t magic,forward,owner,helper;
 uint32_t stop,busy,state,error,request,completed,model,vehicle;
 uint32_t pilot,pilot_model,model_owned,mode,started,last_frame,started_frame,steps;
 uint32_t created,deleted,pilots_created,pilots_deleted,restored,axis,frames,full_rotor;
 float value,original[4],values[4],rotation_start[3],rotation_end[3],velocity_start[3],velocity_end[3];
 uint64_t entity;
} TrikeEngineProbe;
_Static_assert(sizeof(TrikeEngineProbe)==224,"TrikeEngine probe ABI");
typedef struct {uint64_t magic,probe,entity,shadow;uint32_t stopped,busy,calls,applied,rejected,reason;float before[4],after[4];} TrikeEngineInput;
_Static_assert(sizeof(TrikeEngineInput)==88,"TrikeEngine input ABI");

/* Three distinct sites: pre-physics (0), engine entry (1), and the final
 * engine-component call (2). Readback only at both engine sites. */
typedef struct {uint32_t head,reserved[3],records[64][12];} TrikeEngineTrace;
_Static_assert(sizeof(TrikeEngineTrace)<=4096,"Bike engine trace ABI");
