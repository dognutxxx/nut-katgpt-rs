#include "katgpt_engram_v108.h"

static uint8_t hash_key(const int8_t *key) {
    uint32_t h = 2166136261u;
    for (int i = 0; i < KATGPT_LATENT_D; ++i) {
        h ^= (uint8_t)key[i];
        h *= 16777619u;
    }
    return (uint8_t)(h % KATGPT_ENGRAM_CAP);
}

/* Bucket noisy INT8 observations so repeated sensor contexts share a key. */
static void quantize_key(const int8_t *input, int8_t *output) {
    for (int i = 0; i < KATGPT_LATENT_D; ++i) output[i] = (int8_t)(input[i] / 16);
}

static int same_key(const int8_t *a, const int8_t *b) {
    uint8_t diff = 0;
    for (int i = 0; i < KATGPT_LATENT_D; ++i) diff |= (uint8_t)(a[i] ^ b[i]);
    return diff == 0;
}

void katgpt_engram_init(katgpt_engram_t *memory) {
    for (int i = 0; i < KATGPT_ENGRAM_CAP; ++i) memory->slots[i].used = 0;
    memory->previous_state = 0;
    memory->has_previous = 0;
}

int katgpt_engram_lookup(const katgpt_engram_t *memory, const int8_t *key,
                         uint8_t *successor, uint8_t *confidence) {
    uint8_t start = hash_key(key);
    for (uint8_t probe = 0; probe < 4; ++probe) {
        const katgpt_engram_slot_t *slot = &memory->slots[(start + probe) % KATGPT_ENGRAM_CAP];
        if (!slot->used) return 0;
        if (same_key(slot->key, key)) {
            *successor = slot->successor;
            *confidence = slot->confidence;
            return 1;
        }
    }
    return 0;
}

void katgpt_engram_update(katgpt_engram_t *memory, const int8_t *key,
                          uint8_t successor) {
    uint8_t start = hash_key(key);
    katgpt_engram_slot_t *victim = 0;
    for (uint8_t probe = 0; probe < 4; ++probe) {
        katgpt_engram_slot_t *slot = &memory->slots[(start + probe) % KATGPT_ENGRAM_CAP];
        if (!slot->used || same_key(slot->key, key)) { victim = slot; break; }
        if (!victim || slot->age > victim->age) victim = slot;
    }
    for (int i = 0; i < KATGPT_LATENT_D; ++i) victim->key[i] = key[i];
    if (victim->used && victim->successor == successor && victim->confidence < 255) victim->confidence++;
    else { victim->successor = successor; victim->confidence = 1; }
    victim->age = 0;
    victim->used = 1;
    for (int i = 0; i < KATGPT_ENGRAM_CAP; ++i) if (memory->slots[i].used && memory->slots[i].age < 255) memory->slots[i].age++;
}

int katgpt_nearest32x16_engram(const int8_t bank[KATGPT_STATE_N][KATGPT_LATENT_D],
                               const int8_t *query, katgpt_engram_t *memory,
                               uint8_t *engram_used) {
    int best = 0, best_distance = 0x7fffffff;
    for (int state = 0; state < KATGPT_STATE_N; ++state) {
        int distance = 0;
        for (int dim = 0; dim < KATGPT_LATENT_D; ++dim) {
            int delta = (int)query[dim] - (int)bank[state][dim];
            distance += delta < 0 ? -delta : delta;
        }
        if (distance < best_distance) { best_distance = distance; best = state; }
    }
    int8_t key[KATGPT_LATENT_D];
    quantize_key(query, key);
    *engram_used = 0;
    if (memory->has_previous) {
        uint8_t successor = 0, confidence = 0;
        if (katgpt_engram_lookup(memory, key, &successor, &confidence) && confidence >= 2) {
            if (successor < KATGPT_STATE_N) { best = successor; *engram_used = 1; }
        }
    }
    if (memory->has_previous) katgpt_engram_update(memory, key, (uint8_t)best);
    memory->previous_state = (uint8_t)best;
    memory->has_previous = 1;
    return best;
}
