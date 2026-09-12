#ifndef KATGPT_LATENT_H
#define KATGPT_LATENT_H

#include <stdint.h>

#define KATGPT_LATENT_DIMS 16
#define KATGPT_LATENT_STATES 32
#define KATGPT_PRIOR_LAMBDA 24
#define KATGPT_PRIOR_MARGIN 16
#define KATGPT_PRIOR_CONF_MIN 8

typedef struct {
    uint8_t candidate;
    uint8_t confidence;
} katgpt_transition_t;

typedef struct {
    int8_t bank[KATGPT_LATENT_STATES][KATGPT_LATENT_DIMS];
    katgpt_transition_t transition[KATGPT_LATENT_STATES];
    uint8_t dwell[KATGPT_LATENT_STATES];
    uint8_t metadata[5];
} katgpt_runtime_t;

typedef struct {
    uint8_t state;
    int32_t distance;
    uint8_t raw_state;
    uint8_t prior_used;
} katgpt_retrieval_t;

_Static_assert(sizeof(katgpt_runtime_t) == 613, "v1.08 persistent runtime must remain 613 bytes");

katgpt_retrieval_t katgpt_nearest_prior(
    const katgpt_runtime_t *runtime,
    const int8_t query[KATGPT_LATENT_DIMS],
    uint8_t previous_state);

void katgpt_observe_transition(katgpt_transition_t *slot, uint8_t next_state);
void katgpt_init_demo_bank(katgpt_runtime_t *runtime);

#endif

