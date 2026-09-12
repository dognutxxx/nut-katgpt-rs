#include "katgpt_latent.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

static int32_t l1(const int8_t *a, const int8_t *b) {
    int32_t distance = 0;
    for (size_t dim = 0; dim < KATGPT_LATENT_DIMS; ++dim) {
        int32_t delta = (int32_t)a[dim] - (int32_t)b[dim];
        distance += delta < 0 ? -delta : delta;
    }
    return distance;
}

katgpt_retrieval_t katgpt_nearest_prior(
    const katgpt_runtime_t *runtime,
    const int8_t query[KATGPT_LATENT_DIMS],
    uint8_t previous_state) {
    katgpt_retrieval_t result = {0, INT32_MAX, 0, 0};
    for (uint8_t state = 0; state < KATGPT_LATENT_STATES; ++state) {
        const int32_t distance = l1(runtime->bank[state], query);
        if (distance < result.distance) {
            result.state = state;
            result.raw_state = state;
            result.distance = distance;
        }
    }

    if (previous_state >= KATGPT_LATENT_STATES) return result;
    const katgpt_transition_t prior = runtime->transition[previous_state];
    if (prior.confidence < KATGPT_PRIOR_CONF_MIN || prior.candidate >= KATGPT_LATENT_STATES) return result;

    const int32_t prior_distance = l1(runtime->bank[prior.candidate], query);
    const int32_t delta = prior_distance - result.distance;
    if (delta < 0 || delta > KATGPT_PRIOR_MARGIN) return result;

    const int32_t raw_score = result.distance * 256;
    const int32_t prior_score = prior_distance * 256 - KATGPT_PRIOR_LAMBDA * (int32_t)prior.confidence;
    if (prior_score < raw_score) {
        result.state = prior.candidate;
        result.distance = prior_distance;
        result.prior_used = prior.candidate != result.raw_state;
    }
    return result;
}

void katgpt_observe_transition(katgpt_transition_t *slot, uint8_t next_state) {
    if (next_state >= KATGPT_LATENT_STATES) return;
    if (slot->confidence == 0) {
        slot->candidate = next_state;
        slot->confidence = 1;
    } else if (slot->candidate == next_state) {
        if (slot->confidence != UINT8_MAX) ++slot->confidence;
    } else if (--slot->confidence == 0) {
        slot->candidate = next_state;
        slot->confidence = 1;
    }
}

void katgpt_init_demo_bank(katgpt_runtime_t *runtime) {
    memset(runtime, 0, sizeof(*runtime));
    for (uint8_t state = 0; state < KATGPT_LATENT_STATES; ++state) {
        for (uint8_t dim = 0; dim < KATGPT_LATENT_DIMS; ++dim) {
            const uint8_t bit = (uint8_t)(((state >> (dim % 5U)) ^ (dim / 5U)) & 1U);
            runtime->bank[state][dim] = bit == 0 ? -36 : 36;
        }
        runtime->transition[state].candidate = (uint8_t)((state + 1U) % KATGPT_LATENT_STATES);
        runtime->transition[state].confidence = UINT8_MAX;
    }
}
