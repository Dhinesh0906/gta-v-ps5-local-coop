/* Fixed, default-off ownership of P2's original/Mk II flight adapter. (AI-assisted) */
#pragma once
#include <stdint.h>
#define OPPRESSOR_RUNTIME_MAGIC 0x31544e555232504fULL
#define OPPRESSOR_PRODUCTION_MAGIC 0x315450525032504fULL
#define OPPRESSOR_PRODUCTION_BASE 0x62c00000ULL
typedef struct {
 uint64_t magic,owner,entity;
 uint32_t enabled,vehicle,pilot,frame,status,error;
 float values[4];
} OppressorFlightRuntime;
_Static_assert(sizeof(OppressorFlightRuntime)==64,"Mk II ownership ABI");
