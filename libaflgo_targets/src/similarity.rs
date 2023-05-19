use std::sync::atomic::{AtomicU64, Ordering};

use libafl::prelude::{ExitKind, Named, Observer, OwnedRef, UsesInput};
use serde::{Deserialize, Serialize};

use libaflgo::SimilarityObserver;

const SIMILARITY_RESOLUTION: f64 = 1e3;

#[derive(Debug, Serialize, Deserialize)]
pub struct SimilarityStats {
    similarity_inc_sum: AtomicU64,
    similarity_inc_count: AtomicU64,
}

impl SimilarityStats {
    pub const fn new() -> Self {
        Self {
            similarity_inc_sum: AtomicU64::new(0),
            similarity_inc_count: AtomicU64::new(0),
        }
    }

    pub fn add_fun_distance(&self, fun_distance: f64) {
        let similarity_inc = 1_f64 / fun_distance;
        let similarity_inc_approx = (similarity_inc * SIMILARITY_RESOLUTION).trunc() as u64;

        self.similarity_inc_sum
            .fetch_add(similarity_inc_approx, Ordering::Relaxed);
        self.similarity_inc_count.fetch_add(1, Ordering::Relaxed);
    }

    pub fn compute_similarity(&self) -> f64 {
        let similarity_inc_sum_restored =
            self.similarity_inc_sum.load(Ordering::Relaxed) as f64 / SIMILARITY_RESOLUTION;
        similarity_inc_sum_restored / self.similarity_inc_count.load(Ordering::Relaxed) as f64
    }

    pub fn reset(&self) {
        self.similarity_inc_sum.store(0, Ordering::Relaxed);
        self.similarity_inc_count.store(0, Ordering::Relaxed);
    }
}

static STATS: SimilarityStats = SimilarityStats::new();

// Called by the function distance instrumentation
#[no_mangle]
pub extern "C" fn __aflgo_trace_fun_distance(fun_distance: f64) {
    STATS.add_fun_distance(fun_distance)
}

#[derive(Debug, Serialize, Deserialize)]
pub struct InProcessSimilarityObserver<'a> {
    name: String,
    similarity: Option<f64>,
    stats: OwnedRef<'a, SimilarityStats>,
}

impl<'a> InProcessSimilarityObserver<'a> {
    pub fn new(name: String) -> Self {
        Self::with_stats_ref(name, &STATS)
    }

    #[must_use]
    pub fn with_stats_ref(name: String, stats: &'a SimilarityStats) -> Self {
        Self {
            name,
            similarity: None,
            stats: OwnedRef::Ref(stats),
        }
    }
}
impl<S: UsesInput> SimilarityObserver<S> for InProcessSimilarityObserver<'_> {
    fn similarity(&self) -> f64 {
        self.similarity.expect("distance not set")
    }
}

impl<'a> Named for InProcessSimilarityObserver<'a> {
    fn name(&self) -> &str {
        &self.name
    }
}

impl<'a, S: UsesInput> Observer<S> for InProcessSimilarityObserver<'a> {
    fn pre_exec(&mut self, _state: &mut S, _input: &S::Input) -> Result<(), libafl::Error> {
        self.stats.as_ref().reset();
        self.similarity = None;
        Ok(())
    }

    fn post_exec(
        &mut self,
        _state: &mut S,
        _input: &S::Input,
        _exit_kind: &ExitKind,
    ) -> Result<(), libafl::Error> {
        self.similarity = Some(self.stats.as_ref().compute_similarity());
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_similarity_calculation() {
        let stats = SimilarityStats::new();
        stats.reset();

        stats.add_fun_distance(1.0);
        stats.add_fun_distance(2.0);

        let test_case_similarity = stats.compute_similarity();
        assert_eq!(test_case_similarity, 0.75);

        stats.reset();

        let test_case_similarity = stats.compute_similarity();
        assert!(test_case_similarity.is_nan());
    }
}
