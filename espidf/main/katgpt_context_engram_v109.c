#include "katgpt_context_engram_v109.h"

#include <string.h>

static uint8_t clamp_dwell(uint8_t dwell) {
    return dwell < KATGPT_V109_DWELL_BUCKETS ? dwell : (KATGPT_V109_DWELL_BUCKETS - 1U);
}

void katgpt_context_engram_v109_init(katgpt_context_engram_v109_t *memory) {
    memset(memory, 0, sizeof(*memory));
}

void katgpt_context_engram_v109_observe(
    katgpt_context_engram_v109_t *memory,
    uint8_t previous_state,
    uint8_t dwell_bucket,
    uint8_t observed_state) {
    if (previous_state >= KATGPT_V109_STATES || observed_state >= KATGPT_V109_STATES) return;
    const uint8_t d = clamp_dwell(dwell_bucket);
    uint8_t *candidate = &memory->candidate[previous_state][d];
    uint8_t *confidence = &memory->confidence[previous_state][d];

    if (*confidence == 0U) {
        *candidate = observed_state;
        *confidence = 1U;
    } else if (*candidate == observed_state) {
        if (*confidence != UINT8_MAX) ++(*confidence);
    } else {
        --(*confidence);
        if (*confidence == 0U) {
            *candidate = observed_state;
            *confidence = 1U;
        }
    }
}

katgpt_context_decision_v109_t katgpt_context_engram_v109_decide(
    const katgpt_context_engram_v109_t *memory,
    uint8_t previous_state,
    uint8_t dwell_bucket,
    uint8_t raw_state,
    const int32_t distances[KATGPT_V109_STATES]) {
    katgpt_context_decision_v109_t out = {raw_state, 0U};
    if (previous_state >= KATGPT_V109_STATES || raw_state >= KATGPT_V109_STATES) return out;

    const uint8_t d = clamp_dwell(dwell_bucket);
    const uint8_t confidence = memory->confidence[previous_state][d];
    const uint8_t candidate = memory->candidate[previous_state][d];
    if (confidence < KATGPT_V109_CONF_MIN || candidate >= KATGPT_V109_STATES) return out;

    const int32_t delta = distances[candidate] - distances[raw_state];
    if (delta < 0 || delta > KATGPT_V109_MARGIN) return out;

    out.state = candidate;
    out.used = candidate != raw_state;
    return out;
}
