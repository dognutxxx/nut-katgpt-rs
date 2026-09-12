# Latent runtime

The v1.08 runtime is a fixed-size state estimator rather than a neural network:

```text
observation evidence (INT8 L1 distance)
                  +
trajectory evidence (one learned successor + confidence)
                  |
                  v
       gated fixed-point re-score
                  |
                  v
           retrieved state
```

The prior is deliberately weak. It is ignored when confidence is below 8 or when its candidate is more than 16 L1 units behind the raw nearest state. Scores use integer arithmetic only:

```text
raw_score   = raw_distance * 256
prior_score = prior_distance * 256 - 24 * confidence
```

The device state is 613 bytes: 512-byte latent bank, 64-byte transition memory, 32-byte dwell state, and five metadata bytes. No heap or PSRAM is required by the core primitive.

v1.08 does not alter the v1.07 algorithm or tune its parameters. It adds held-out test distributions, reproducible host evidence, and a board-evidence protocol.

