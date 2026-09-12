# nut-katgpt-rs

Reproducible R&D for a small, deterministic latent-space sensor runtime targeting ESP32-S3.

The current research branch contains **v1.08**, a held-out robustness gate for the v1.07 trajectory-biased fixed-point retrieval primitive, plus an Engram-style sparse successor-memory experiment. Host evidence is separated from physical-board evidence.

```text
sensor -> INT8 latent -> exact L1 retrieval
                         + gated trajectory prior
                         + sparse successor memory (experiment)
                         -> latent state
```

## Status vocabulary

- `HOST-PASS`: the Rust golden model compiled and its host tests/benchmark passed.
- `ESP-IDF BUILD-PASS`: `idf.py set-target esp32s3 && idf.py build` completed for this revision.
- `BOARD-PASS`: the same revision was flashed and produced captured serial evidence from a physical ESP32-S3.

The robustness promotion gate is **FAIL** for stale wrong-prior bursts; the Engram experiment is also **NO-GO** after coarse-key collisions reduced its deterministic reference accuracy (`77.5280%` raw → `77.1920%` aided). No board result is inferred from host results.

## Host checks

```sh
cargo test
cargo run --release --bin robustness_v108
python benchmarks/v108_reference.py
```

## ESP32-S3 build and measurement

```sh
cd examples/iot/_esp32s3/latent
idf.py set-target esp32s3
idf.py build
idf.py size
idf.py size-components
idf.py -p COM4 flash monitor
```

Capture board logs under `benchmarks/esp32s3/v1.08/board/`. Until those logs exist, board metrics remain `N/A` and `BOARD-PASS=NO`.

## Research layout

- `host/` — Rust golden model and held-out robustness gate
- `examples/iot/_esp32s3/latent/` — ESP-IDF component example
- `include/`, `espidf/`, `src/` — v1.08 Engram experiment API and host mirror
- `benchmarks/esp32s3/v1.08/` — generated host evidence and benchmark notes
- `.research/breeding/` — research hypotheses, provenance, and decisions
