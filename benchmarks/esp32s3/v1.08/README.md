# v1.08 benchmark evidence

This file is updated from an actual run of `cargo run --release --bin robustness_v108` after implementation. It must not contain estimated board metrics.

## Host result

Executed on 2026-09-12 with Rust 1.95.0 in release mode. The matrix contains 200 cyclic sequential cases (10 seeds x 5 noise levels x 4 transition stabilities), 50 memoryless random cases, 10 sudden-drift cases, and 100 isolated wrong-prior burst windows.

| Gate | Result | Status |
|---|---:|---|
| mean sequential gain > +2.0 pp | **+5.6752 pp** | PASS |
| worst memoryless regression <= 0.25 pp | **0.0000 pp** | PASS |
| wrong-prior degradation <= 0.5 pp | **15.0000 pp** | **FAIL** |
| worst sudden-drift regression (diagnostic) | **3.6000 pp** | FAIL |
| persistent runtime <= 613 B | **613 B** | PASS |
| host unit tests | **6/6 passed** | PASS |

The checked-in `host-results.csv` and `summary.json` are generated directly by the benchmark's `--emit-dir` option. The worst-burst gate is computed per 20-sample window before taking the maximum, so a severe window cannot be hidden by averaging it with other bursts.

## Decision

`v1.08` is a reproducible negative robustness result. It validates the v1.07 gain across held-out seeds/noise/stability, but it does **not** promote trajectory-biased retrieval to a stable default because stale high-confidence transitions can still harm burst changes.

The next experiment should add a temporal trust signal or fast confidence decay after observation/transition disagreement, while retaining the same distance margin and fixed-size memory discipline.

## ESP32-S3 result

| Evidence | Status |
|---|---|
| `idf.py set-target esp32s3` | not run in this workspace |
| ESP-IDF build | not verified |
| flash | not verified |
| serial output | not captured |
| flash/SRAM/PSRAM measurements | `N/A` |
| latency/current/energy per query | `N/A` |
| `BOARD-PASS` | **NO** |

Place raw serial logs and an evidence manifest in `board/`; do not replace `N/A` with host-derived estimates.
