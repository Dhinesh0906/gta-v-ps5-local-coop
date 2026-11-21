/* Shared fixture-only observer ABI. (AI-assisted) */
#pragma once
#include <stdint.h>
#ifndef PLANE_LATE_BASE
#define PLANE_LATE_BASE 0x61a00000ULL
#endif
#define PLANE_LATE_MAGIC 0x3154504e494c5050ULL
#define PLANE_LATE_SHADOW (PLANE_LATE_BASE+0x10010ULL)
typedef struct {
 uint64_t magic,engine,owner,entity,shadow,original;
 uint32_t mode,stop,busy,remaining,vehicle,pilot,calls,applied,rejected,reserved,reason,frame;
 uint32_t before[4],desired[4],after[4],phase,phases[3];
} PlaneLateInput;
_Static_assert(sizeof(PlaneLateInput)==160,"Late plane mailbox ABI");

/* A lease for the normal pedal fields that otherwise retain NPC braking.
 * Cleared only after the private callback is stopped/drained. (AI-assisted) */
typedef struct {uint64_t entity;uint32_t gas,brake,last_gas,last_brake;} PlanePedalLease;
_Static_assert(sizeof(PlanePedalLease)==24,"Plane pedal lease ABI");
#define PLANE_PEDAL_LEASE (PLANE_LATE_BASE+0x1100ULL)
