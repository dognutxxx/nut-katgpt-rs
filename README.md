# nut-katgpt-rs

KatGPT-rs is a small, modelless latent-runtime research project for ESP32-S3 sensor workloads.

## v1.08 focus

`research/esp32s3-v1.08` adds an Engram-style sparse successor memory to the v1.07 fixed-point trajectory prior:

```text
sensor -> 16-D INT8 latent -> 32-state L1 retrieval
       -> trajectory prior + sparse successor memory -> decision
```

The implementation uses fixed-size storage, integer-only hot paths, bounded probing, and no heap allocation.

The first deterministic host run is intentionally recorded as a failed quality gate (`77.5280%` raw vs `77.1920%` aided) because coarse key bucketing causes collisions. This branch is a reproducible research checkpoint, not a deployment claim.

## Evidence policy

Host benchmarks and C syntax checks are reported separately from physical-board evidence. This branch is **not BOARD-PASS**: no ESP32-S3 build/flash/run/serial log is included yet.

## Layout

- `src/engram_v108.cpp` — host reference implementation and benchmark
- `include/katgpt_engram_v108.h` — ESP-IDF-compatible C API
- `espidf/main/katgpt_engram_v108.c` — MCU implementation
- `benchmarks/v108_reference.py` — deterministic host benchmark
- `.research/breeding/v1.08-engram-trajectory.md` — research note and provenance

## Run host benchmark

```text
python benchmarks/v108_reference.py
```
