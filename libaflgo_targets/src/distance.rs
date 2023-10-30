use std::sync::atomic::{AtomicU64, Ordering};

use libafl::prelude::{ExitKind, Named, Observer, OwnedRef, UsesInput};
use serde::{Deserialize, Serialize};

use libaflgo::DistanceObserver;

// XXX: this should be kept in sync with passes/AFLGoLinker/DistanceInstrumentation.cpp
const DISTANCE_RESOLUTION: f64 = 1e3;

#[derive(Debug, Serialize, Deserialize)]
pub struct DistanceStats {
    bb_distance_sum: AtomicU64,
    bb_distance_count: AtomicU64,
}

impl DistanceStats {
    pub const fn new() -> Self {
        Self {
            bb_distance_sum: AtomicU64::new(0),
            bb_distance_count: AtomicU64::new(0),
        }
    }

    pub fn add_bb_distance(&self, bb_distance: u64) {
        self.bb_distance_sum
            .fetch_add(bb_distance, Ordering::Relaxed);
        self.bb_distance_count.fetch_add(1, Ordering::Relaxed);
    }

    pub fn compute_test_case_distance(&self) -> f64 {
        let distance_restored =
            self.bb_distance_sum.load(Ordering::Relaxed) as f64 / DISTANCE_RESOLUTION;
        distance_restored / self.bb_distance_count.load(Ordering::Relaxed) as f64
    }

    pub fn reset(&self) {
        self.bb_distance_sum.store(0, Ordering::Relaxed);
        self.bb_distance_count.store(0, Ordering::Relaxed);
    }
}

static STATS: DistanceStats = DistanceStats::new();

// Called by the distance instrumentation
#[no_mangle]
pub extern "C" fn __aflgo_trace_bb_distance(bb_distance: u64) {
    STATS.add_bb_distance(bb_distance);
}

#[derive(Debug, Serialize, Deserialize)]
pub struct InProcessDistanceObserver<'a> {
    name: String,
    distance: Option<f64>,
    stats: OwnedRef<'a, DistanceStats>,
}

impl<'a> InProcessDistanceObserver<'a> {
    pub fn new(name: String) -> Self {
        Self::with_stats_ref(name, &STATS)
    }

    #[must_use]
    pub fn with_stats_ref(name: String, stats: &'a DistanceStats) -> Self {
        Self {
            name,
            distance: None,
            stats: OwnedRef::Ref(stats),
        }
    }
}
impl<S: UsesInput> DistanceObserver<S> for InProcessDistanceObserver<'_> {
    fn distance(&self) -> f64 {
        self.distance.expect("distance not set")
    }
}

impl<'a> Named for InProcessDistanceObserver<'a> {
    fn name(&self) -> &str {
        &self.name
    }
}

impl<'a, S: UsesInput> Observer<S> for InProcessDistanceObserver<'a> {
    fn pre_exec(&mut self, _state: &mut S, _input: &S::Input) -> Result<(), libafl::Error> {
        self.stats.as_ref().reset();
        self.distance = None;
        Ok(())
    }

    fn post_exec(
        &mut self,
        _state: &mut S,
        _input: &S::Input,
        _exit_kind: &ExitKind,
    ) -> Result<(), libafl::Error> {
        self.distance = Some(self.stats.as_ref().compute_test_case_distance());
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_distance_calculation() {
        let stats = DistanceStats::new();
        stats.reset();

        stats.add_bb_distance(1 * DISTANCE_RESOLUTION as u64);
        stats.add_bb_distance(2 * DISTANCE_RESOLUTION as u64);

        let test_case_distance = stats.compute_test_case_distance();
        assert_eq!(test_case_distance, 1.5);

        stats.reset();

        let test_case_distance = stats.compute_test_case_distance();
        assert!(test_case_distance.is_nan());
    }
}
