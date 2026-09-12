#ifndef KATGPT_ENGRAM_V108_H
#define KATGPT_ENGRAM_V108_H

#include <stdint.h>

#define KATGPT_LATENT_D 16
#define KATGPT_STATE_N 32
#define KATGPT_ENGRAM_CAP 32

typedef struct {
    int8_t key[KATGPT_LATENT_D];
    uint8_t successor;
    uint8_t confidence;
    uint8_t age;
    uint8_t used;
} katgpt_engram_slot_t;

typedef struct {
    katgpt_engram_slot_t slots[KATGPT_ENGRAM_CAP];
    uint8_t previous_state;
    uint8_t has_previous;
} katgpt_engram_t;

void katgpt_engram_init(katgpt_engram_t *memory);
int katgpt_engram_lookup(const katgpt_engram_t *memory, const int8_t *key,
                         uint8_t *successor, uint8_t *confidence);
void katgpt_engram_update(katgpt_engram_t *memory, const int8_t *key,
                          uint8_t successor);
int katgpt_nearest32x16_engram(const int8_t bank[KATGPT_STATE_N][KATGPT_LATENT_D],
                               const int8_t *query, katgpt_engram_t *memory,
                               uint8_t *engram_used);

#endif
