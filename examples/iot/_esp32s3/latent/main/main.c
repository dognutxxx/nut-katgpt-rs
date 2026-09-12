#include <inttypes.h>
#include <stdio.h>

#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "katgpt_latent.h"

void app_main(void) {
    static katgpt_runtime_t runtime;
    int8_t query[KATGPT_LATENT_DIMS] = {0};
    uint32_t checksum = 0;
    uint32_t prior_uses = 0;
    const uint32_t iterations = 100000;

    katgpt_init_demo_bank(&runtime);
    const int64_t started_us = esp_timer_get_time();
    for (uint32_t i = 0; i < iterations; ++i) {
        query[i % KATGPT_LATENT_DIMS] = (int8_t)((int32_t)(i & 31U) - 16);
        const katgpt_retrieval_t result = katgpt_nearest_prior(&runtime, query, (uint8_t)(i & 31U));
        checksum += result.state;
        prior_uses += result.prior_used;
    }
    const int64_t elapsed_us = esp_timer_get_time() - started_us;

    printf("KATGPT_EVIDENCE revision=v1.08 classification=PROTOTYPE\n");
    printf("pipeline_us_avg=%.6f iterations=%" PRIu32 " checksum=%" PRIu32 " prior_uses=%" PRIu32 "\n",
           (double)elapsed_us / (double)iterations, iterations, checksum, prior_uses);
    printf("runtime_bytes=%u free_internal_sram=%u free_psram=%u\n",
           (unsigned)sizeof(runtime),
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    printf("BOARD_PASS=NO reason=log_requires_capture_and_review\n");
}
