/* Exact PPSA04264 01.011.001 input layout, decoded from 0x2f6a5b0.
 * This prepares private input storage. It does not install player tasks or
 * route any engine code to it. The destination must stay at the same address.
 */
#pragma once
#include <stdint.h>
#include <stddef.h>

enum {
    COOP_ENGINE_ACTIONS = 402, COOP_ENGINE_GROUPS = 7,
    COOP_ENGINE_BINDINGS = 156, COOP_ENGINE_HISTORY = 64,
    COOP_ENGINE_SOURCE_BYTES = 0xd750, COOP_ENGINE_ACTION_OFFSET = 0x1e68,
    COOP_ENGINE_GROUP_OFFSET = 0x9118
};
typedef struct { int32_t kind; uint32_t code; int32_t device; } CoopEngineSource;
typedef struct { int32_t value; uint32_t time; CoopEngineSource source; } CoopEngineHistory;
typedef struct {
    int32_t multiplier, value, previous, pending;
    CoopEngineSource source, previous_source, pending_source;
    int32_t locked_kind;
    uint8_t generation, history_index, flags, reserved;
    uint32_t disabled_until;
    uint64_t history;
} CoopEngineAction;
typedef struct {
    int32_t count, device;
    uint32_t kinds[COOP_ENGINE_BINDINGS], codes[COOP_ENGINE_BINDINGS];
    uint64_t destinations[COOP_ENGINE_BINDINGS];
} CoopEngineBindingGroup;
typedef struct {
    CoopEngineAction actions[COOP_ENGINE_ACTIONS];
    CoopEngineBindingGroup groups[COOP_ENGINE_GROUPS];
    CoopEngineHistory histories[COOP_ENGINE_ACTIONS][COOP_ENGINE_HISTORY];
    uint8_t generation;
} CoopEngineInput;
_Static_assert(sizeof(CoopEngineSource)==12, "Engine source ABI");
_Static_assert(sizeof(CoopEngineHistory)==20, "Engine history ABI");
_Static_assert(sizeof(CoopEngineAction)==72, "Engine action ABI");
_Static_assert(offsetof(CoopEngineAction,history)==0x40, "Engine history pointer");
_Static_assert(sizeof(CoopEngineBindingGroup)==0x9c8, "Engine binding-group ABI");

enum { COOP_ENGINE_INPUT_OK, COOP_ENGINE_INPUT_ARGUMENT, COOP_ENGINE_INPUT_ALIAS,
       COOP_ENGINE_INPUT_BINDING, COOP_ENGINE_INPUT_DEVICE };
int coop_engine_input_clone(CoopEngineInput *destination, size_t destination_bytes,
    const void *source, size_t source_bytes, uint64_t source_address,
    int secondary_device, CoopEngineSource neutral, uint32_t now);
