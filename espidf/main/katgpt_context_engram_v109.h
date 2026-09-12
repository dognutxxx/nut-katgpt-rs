#ifndef KATGPT_CONTEXT_ENGRAM_V109_H
#define KATGPT_CONTEXT_ENGRAM_V109_H

#include <stdint.h>

#define KATGPT_V109_STATES 32
#define KATGPT_V109_DWELL_BUCKETS 5
#define KATGPT_V109_CONF_MIN 32
#define KATGPT_V109_MARGIN 16

typedef struct {
    uint8_t candidate[KATGPT_V109_STATES][KATGPT_V109_DWELL_BUCKETS];
    uint8_t confidence[KATGPT_V109_STATES][KATGPT_V109_DWELL_BUCKETS];
} katgpt_context_engram_v109_t;

typedef struct {
    uint8_t state;
    uint8_t used;
} katgpt_context_decision_v109_t;

void katgpt_context_engram_v109_init(katgpt_context_engram_v109_t *memory);
void katgpt_context_engram_v109_observe(
    katgpt_context_engram_v109_t *memory,
    uint8_t previous_state,
    uint8_t dwell_bucket,
    uint8_t observed_state);
katgpt_context_decision_v109_t katgpt_context_engram_v109_decide(
    const katgpt_context_engram_v109_t *memory,
    uint8_t previous_state,
    uint8_t dwell_bucket,
    uint8_t raw_state,
    const int32_t distances[KATGPT_V109_STATES]);

#endif
