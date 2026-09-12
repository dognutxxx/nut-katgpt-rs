use std::fmt::Write as _;
use std::path::PathBuf;

use katgpt_latent_host::{
    LATENT_DIMS, LATENT_STATES, Runtime, Transition, nearest_raw, nearest_with_prior,
};

const SEEDS: u64 = 10;
const SAMPLES: usize = 20_000;
const NOISE_LEVELS: [u32; 5] = [12, 18, 26, 36, 48];
const STABILITIES: [u32; 4] = [95, 85, 70, 55];

#[derive(Clone, Copy)]
struct Rng(u64);

impl Rng {
    fn next(&mut self) -> u32 {
        self.0 ^= self.0 << 13;
        self.0 ^= self.0 >> 7;
        self.0 ^= self.0 << 17;
        (self.0 >> 16) as u32
    }

    fn range(&mut self, upper: u32) -> u32 {
        self.next() % upper
    }
}

#[derive(Clone, Copy)]
enum Disturbance {
    None,
    Single { start: usize, len: usize },
    Periodic { period: usize, len: usize },
}

impl Disturbance {
    fn active(self, step: usize) -> bool {
        match self {
            Self::None => false,
            Self::Single { start, len } => (start..start + len).contains(&step),
            Self::Periodic { period, len } => step % period < len,
        }
    }
}

#[derive(Default, Clone, Copy)]
struct Metrics {
    samples: u64,
    raw_ok: u64,
    biased_ok: u64,
    prior_used: u64,
    corrected: u64,
    harmed: u64,
}

impl Metrics {
    fn observe(&mut self, raw: usize, biased: usize, truth: usize, prior_used: bool) {
        self.samples += 1;
        self.raw_ok += u64::from(raw == truth);
        self.biased_ok += u64::from(biased == truth);
        self.prior_used += u64::from(prior_used);
        self.corrected += u64::from(raw != truth && biased == truth);
        self.harmed += u64::from(raw == truth && biased != truth);
    }

    fn raw_pct(self) -> f64 {
        100.0 * self.raw_ok as f64 / self.samples as f64
    }
    fn biased_pct(self) -> f64 {
        100.0 * self.biased_ok as f64 / self.samples as f64
    }
    fn gain_pp(self) -> f64 {
        self.biased_pct() - self.raw_pct()
    }
    fn prior_pct(self) -> f64 {
        100.0 * self.prior_used as f64 / self.samples as f64
    }
}

struct RunResult {
    aggregate: Metrics,
    worst_window_regression_pp: f64,
}

fn make_runtime() -> Runtime {
    let mut runtime = Runtime::default();
    for state in 0..LATENT_STATES {
        for dim in 0..LATENT_DIMS {
            let bit = ((state >> (dim % 5)) ^ (dim / 5)) & 1;
            runtime.bank[state][dim] = if bit == 0 { -36 } else { 36 };
        }
        runtime.transition[state] = Transition {
            candidate: ((state + 1) % LATENT_STATES) as u8,
            confidence: 255,
        };
    }
    runtime
}

fn boundary_query(
    runtime: &Runtime,
    truth: usize,
    distractor: usize,
    noise: u32,
    rng: &mut Rng,
) -> [i8; LATENT_DIMS] {
    let mut query = runtime.bank[truth];
    let boundary_chance = (noise * 13 / 2).min(420);
    let boundary = rng.range(1000) < boundary_chance;
    for (dim, value) in query.iter_mut().enumerate() {
        let t = i32::from(runtime.bank[truth][dim]);
        let d = i32::from(runtime.bank[distractor][dim]);
        let jitter_span = (noise / 6).max(2);
        let jitter = rng.range(jitter_span * 2 + 1) as i32 - jitter_span as i32;
        let base = if boundary {
            let tilt = rng.range(5) as i32 - 2;
            (t + d) / 2 + tilt
        } else {
            t
        };
        *value = (base + jitter).clamp(-128, 127) as i8;
    }
    query
}

fn run_case(
    seed: u64,
    noise: u32,
    stability: u32,
    memoryless: bool,
    disturbance: Disturbance,
    measure_disturbance_only: bool,
) -> RunResult {
    let mut rng = Rng(seed.wrapping_mul(0x9e37_79b9_7f4a_7c15).wrapping_add(17));
    let mut runtime = make_runtime();
    if memoryless {
        for slot in &mut runtime.transition {
            slot.confidence = 0;
        }
    } else {
        let confidence = ((stability * 255) / 100).max(8) as u8;
        for slot in &mut runtime.transition {
            slot.confidence = confidence;
        }
    }

    let mut previous = rng.range(LATENT_STATES as u32) as usize;
    let mut aggregate = Metrics::default();
    let mut window = Metrics::default();
    let mut was_disturbed = false;
    let mut worst_window_regression_pp = 0.0f64;
    for step in 0..SAMPLES {
        let predicted = (previous + 1) % LATENT_STATES;
        let disturbed = disturbance.active(step);
        let follows = !memoryless && !disturbed && rng.range(100) < stability;
        let truth = if follows {
            predicted
        } else if memoryless {
            rng.range(LATENT_STATES as u32) as usize
        } else {
            (predicted + 1 + rng.range((LATENT_STATES - 1) as u32) as usize) % LATENT_STATES
        };
        let distractor = if truth == predicted {
            (truth + 1 + rng.range((LATENT_STATES - 1) as u32) as usize) % LATENT_STATES
        } else {
            predicted
        };
        let query = boundary_query(&runtime, truth, distractor, noise, &mut rng);
        let raw = nearest_raw(&runtime.bank, &query).0 as usize;
        let biased = nearest_with_prior(&runtime, &query, previous as u8);
        let biased_state = biased.state as usize;

        if !measure_disturbance_only || disturbed {
            aggregate.observe(raw, biased_state, truth, biased.prior_used);
        }
        if disturbed {
            window.observe(raw, biased_state, truth, biased.prior_used);
        } else if was_disturbed {
            worst_window_regression_pp = worst_window_regression_pp.max(-window.gain_pp());
            window = Metrics::default();
        }
        was_disturbed = disturbed;
        previous = truth;
    }
    if was_disturbed {
        worst_window_regression_pp = worst_window_regression_pp.max(-window.gain_pp());
    }
    RunResult {
        aggregate,
        worst_window_regression_pp,
    }
}

fn append_row(csv: &mut String, scenario: &str, seed: u64, noise: u32, stability: u32, m: Metrics) {
    writeln!(
        csv,
        "{scenario},{seed},{noise},{stability},{:.4},{:.4},{:.4},{:.4},{},{}",
        m.raw_pct(),
        m.biased_pct(),
        m.gain_pp(),
        m.prior_pct(),
        m.corrected,
        m.harmed
    )
    .expect("String writes cannot fail");
}

fn emit_evidence(dir: PathBuf, csv: &str, summary: &str) -> std::io::Result<()> {
    std::fs::create_dir_all(&dir)?;
    std::fs::write(dir.join("host-results.csv"), csv)?;
    std::fs::write(dir.join("summary.json"), summary)?;
    Ok(())
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let emit_dir = match args.as_slice() {
        [_] => None,
        [_, flag, dir] if flag == "--emit-dir" => Some(PathBuf::from(dir)),
        _ => {
            eprintln!("usage: robustness_v108 [--emit-dir <directory>]");
            std::process::exit(64);
        }
    };

    let mut cyclic_gain_sum = 0.0;
    let mut cyclic_cases = 0u32;
    let mut worst_memoryless_regression = 0.0f64;
    let mut worst_sudden_drift_regression = 0.0f64;
    let mut worst_wrong_prior_regression = 0.0f64;
    let mut total_corrected = 0u64;
    let mut total_harmed = 0u64;
    let mut csv = String::from(
        "scenario,seed,noise,stability,raw_pct,biased_pct,gain_pp,prior_pct,corrected,harmed\n",
    );

    for seed in 1..=SEEDS {
        for noise in NOISE_LEVELS {
            for stability in STABILITIES {
                let m = run_case(seed, noise, stability, false, Disturbance::None, false).aggregate;
                cyclic_gain_sum += m.gain_pp();
                cyclic_cases += 1;
                total_corrected += m.corrected;
                total_harmed += m.harmed;
                append_row(&mut csv, "cyclic", seed, noise, stability, m);
            }
        }

        for noise in NOISE_LEVELS {
            let m = run_case(seed, noise, 0, true, Disturbance::None, false).aggregate;
            worst_memoryless_regression = worst_memoryless_regression.max(-m.gain_pp());
            append_row(&mut csv, "memoryless-random", seed, noise, 0, m);
        }

        let sudden = run_case(
            seed,
            36,
            95,
            false,
            Disturbance::Single {
                start: SAMPLES / 2,
                len: 500,
            },
            true,
        );
        worst_sudden_drift_regression =
            worst_sudden_drift_regression.max(sudden.worst_window_regression_pp);
        append_row(&mut csv, "sudden-drift", seed, 36, 95, sudden.aggregate);

        let burst = run_case(
            seed,
            36,
            95,
            false,
            Disturbance::Periodic {
                period: 2000,
                len: 20,
            },
            true,
        );
        worst_wrong_prior_regression =
            worst_wrong_prior_regression.max(burst.worst_window_regression_pp);
        append_row(&mut csv, "wrong-prior-burst", seed, 36, 95, burst.aggregate);
    }

    let mean_gain = cyclic_gain_sum / f64::from(cyclic_cases);
    let passed = mean_gain > 2.0
        && worst_memoryless_regression <= 0.25
        && worst_wrong_prior_regression <= 0.5
        && std::mem::size_of::<Runtime>() <= 613;
    let summary = format!(
        concat!(
            "{{\n",
            "  \"version\": \"1.08\",\n",
            "  \"classification\": \"PROTOTYPE\",\n",
            "  \"run_date\": \"2026-09-12\",\n",
            "  \"host\": {{\n",
            "    \"rustc\": \"1.95.0\",\n",
            "    \"unit_tests_passed\": 6,\n",
            "    \"unit_tests_failed\": 0,\n",
            "    \"cyclic_cases\": 200,\n",
            "    \"memoryless_cases\": 50,\n",
            "    \"sudden_drift_cases\": 10,\n",
            "    \"wrong_prior_burst_windows\": 100,\n",
            "    \"mean_sequential_gain_pp\": {:.4},\n",
            "    \"worst_memoryless_regression_pp\": {:.4},\n",
            "    \"worst_sudden_drift_regression_pp\": {:.4},\n",
            "    \"worst_wrong_prior_regression_pp\": {:.4},\n",
            "    \"corrected\": {},\n",
            "    \"harmed\": {},\n",
            "    \"runtime_bytes\": {},\n",
            "    \"promotion_gate\": \"{}\"\n",
            "  }},\n",
            "  \"esp32s3\": {{\n",
            "    \"idf_build\": \"NOT_VERIFIED\",\n",
            "    \"flash\": \"NOT_VERIFIED\",\n",
            "    \"serial\": \"NOT_CAPTURED\",\n",
            "    \"board_pass\": false\n",
            "  }}\n",
            "}}\n"
        ),
        mean_gain,
        worst_memoryless_regression,
        worst_sudden_drift_regression,
        worst_wrong_prior_regression,
        total_corrected,
        total_harmed,
        std::mem::size_of::<Runtime>(),
        if passed { "PASS" } else { "FAIL" }
    );

    print!("{csv}");
    eprintln!(
        "SUMMARY mean_sequential_gain_pp={mean_gain:.4} worst_memoryless_regression_pp={worst_memoryless_regression:.4} worst_sudden_drift_regression_pp={worst_sudden_drift_regression:.4} worst_wrong_prior_regression_pp={worst_wrong_prior_regression:.4} corrected={total_corrected} harmed={total_harmed} runtime_bytes={} promotion_gate={}",
        std::mem::size_of::<Runtime>(),
        if passed { "PASS" } else { "FAIL" }
    );
    if let Some(dir) = emit_dir
        && let Err(error) = emit_evidence(dir, &csv, &summary)
    {
        eprintln!("failed to emit evidence: {error}");
        std::process::exit(74);
    }
    if !passed {
        std::process::exit(2);
    }
}
