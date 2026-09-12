//! Host golden model for the ESP32-S3 integer latent retrieval primitive.

pub const LATENT_DIMS: usize = 16;
pub const LATENT_STATES: usize = 32;
pub const PRIOR_LAMBDA: i32 = 24;
pub const PRIOR_MARGIN: i32 = 16;
pub const PRIOR_CONF_MIN: u8 = 8;
pub const INVALID_STATE: u8 = 0xff;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct Transition {
    pub candidate: u8,
    pub confidence: u8,
}

#[repr(C)]
#[derive(Clone)]
pub struct Runtime {
    pub bank: [[i8; LATENT_DIMS]; LATENT_STATES],
    pub transition: [Transition; LATENT_STATES],
    pub dwell: [u8; LATENT_STATES],
    pub metadata: [u8; 5],
}

impl Default for Runtime {
    fn default() -> Self {
        Self {
            bank: [[0; LATENT_DIMS]; LATENT_STATES],
            transition: [Transition::default(); LATENT_STATES],
            dwell: [0; LATENT_STATES],
            metadata: [0; 5],
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Retrieval {
    pub state: u8,
    pub distance: i32,
    pub raw_state: u8,
    pub prior_used: bool,
}

pub fn l1(a: &[i8; LATENT_DIMS], b: &[i8; LATENT_DIMS]) -> i32 {
    a.iter()
        .zip(b)
        .map(|(&x, &y)| (i32::from(x) - i32::from(y)).abs())
        .sum()
}

pub fn nearest_raw(
    bank: &[[i8; LATENT_DIMS]; LATENT_STATES],
    query: &[i8; LATENT_DIMS],
) -> (u8, i32) {
    let mut best_state = 0u8;
    let mut best_distance = i32::MAX;
    for (state, latent) in bank.iter().enumerate() {
        let distance = l1(latent, query);
        if distance < best_distance {
            best_state = state as u8;
            best_distance = distance;
        }
    }
    (best_state, best_distance)
}

pub fn nearest_with_prior(
    runtime: &Runtime,
    query: &[i8; LATENT_DIMS],
    previous_state: u8,
) -> Retrieval {
    let (raw_state, raw_distance) = nearest_raw(&runtime.bank, query);
    let mut result = Retrieval {
        state: raw_state,
        distance: raw_distance,
        raw_state,
        prior_used: false,
    };

    if usize::from(previous_state) >= LATENT_STATES {
        return result;
    }
    let prior = runtime.transition[usize::from(previous_state)];
    if prior.confidence < PRIOR_CONF_MIN || usize::from(prior.candidate) >= LATENT_STATES {
        return result;
    }

    let prior_distance = l1(&runtime.bank[usize::from(prior.candidate)], query);
    let delta = prior_distance - raw_distance;
    if !(0..=PRIOR_MARGIN).contains(&delta) {
        return result;
    }

    let raw_score = raw_distance * 256;
    let prior_score = prior_distance * 256 - PRIOR_LAMBDA * i32::from(prior.confidence);
    if prior_score < raw_score {
        result.state = prior.candidate;
        result.distance = prior_distance;
        result.prior_used = prior.candidate != raw_state;
    }
    result
}

pub fn observe_transition(slot: &mut Transition, next_state: u8) {
    if usize::from(next_state) >= LATENT_STATES {
        return;
    }
    if slot.confidence == 0 {
        slot.candidate = next_state;
        slot.confidence = 1;
    } else if slot.candidate == next_state {
        slot.confidence = slot.confidence.saturating_add(1);
    } else {
        slot.confidence -= 1;
        if slot.confidence == 0 {
            slot.candidate = next_state;
            slot.confidence = 1;
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn runtime_is_exactly_613_bytes() {
        assert_eq!(std::mem::size_of::<Runtime>(), 613);
    }

    #[test]
    fn raw_retrieval_is_exact_on_bank_vectors() {
        let mut runtime = Runtime::default();
        for state in 0..LATENT_STATES {
            runtime.bank[state] = [state as i8; LATENT_DIMS];
        }
        for state in 0..LATENT_STATES {
            assert_eq!(
                nearest_raw(&runtime.bank, &runtime.bank[state]).0,
                state as u8
            );
        }
    }

    #[test]
    fn weak_or_far_prior_cannot_hijack_raw_winner() {
        let mut runtime = Runtime::default();
        runtime.bank[0] = [0; LATENT_DIMS];
        runtime.bank[1] = [20; LATENT_DIMS];
        runtime.transition[0] = Transition {
            candidate: 1,
            confidence: PRIOR_CONF_MIN - 1,
        };
        assert_eq!(nearest_with_prior(&runtime, &[0; LATENT_DIMS], 0).state, 0);

        runtime.transition[0].confidence = 255;
        assert_eq!(nearest_with_prior(&runtime, &[0; LATENT_DIMS], 0).state, 0);
    }

    #[test]
    fn qualified_near_prior_can_correct_boundary_case() {
        let mut runtime = Runtime::default();
        runtime.bank[0] = [0; LATENT_DIMS];
        runtime.bank[1] = [1; LATENT_DIMS];
        runtime.transition[2] = Transition {
            candidate: 1,
            confidence: 255,
        };
        let result = nearest_with_prior(&runtime, &[0; LATENT_DIMS], 2);
        assert_eq!(result.raw_state, 0);
        assert_eq!(result.state, 1);
        assert!(result.prior_used);
    }

    #[test]
    fn online_transition_update_saturates() {
        let mut slot = Transition::default();
        for _ in 0..300 {
            observe_transition(&mut slot, 7);
        }
        assert_eq!(slot.candidate, 7);
        assert_eq!(slot.confidence, 255);
    }

    #[test]
    fn invalid_transition_observation_is_ignored() {
        let mut slot = Transition {
            candidate: 7,
            confidence: 42,
        };
        observe_transition(&mut slot, INVALID_STATE);
        assert_eq!(slot.candidate, 7);
        assert_eq!(slot.confidence, 42);
    }
}
