use std::sync::atomic::{AtomicU64, Ordering};

use libafl::prelude::{ExitKind, Named, Observer, OwnedRef, UsesInput};
use serde::{Deserialize, Serialize};

use libaflgo::DAFLObserver;

#[derive(Debug, Serialize, Deserialize)]
pub struct DAFLStats(AtomicU64);

impl DAFLStats {
    pub const fn new() -> Self {
        Self(AtomicU64::new(0))
    }

    pub fn add_bb_relevance(&self, bb_relevance: u64) {
        self.0.fetch_add(bb_relevance, Ordering::Relaxed);
    }

    pub fn compute_test_case_relevance(&self) -> u64 {
        self.0.load(Ordering::Relaxed)
    }

    pub fn reset(&self) {
        self.0.store(0, Ordering::Relaxed);
    }
}

static STATS: DAFLStats = DAFLStats::new();

#[no_mangle]
pub extern "C" fn __aflgo_trace_bb_dafl(bb_relevance: u64) {
    STATS.add_bb_relevance(bb_relevance);
}

#[derive(Debug, Serialize, Deserialize)]
pub struct InProcessDAFLObserver<'a> {
    name: String,
    relevance: Option<u64>,
    stats: OwnedRef<'a, DAFLStats>,
}

impl<'a> InProcessDAFLObserver<'a> {
    pub fn new(name: String) -> Self {
        Self::with_stats_ref(name, &STATS)
    }

    #[must_use]
    pub fn with_stats_ref(name: String, stats: &'a DAFLStats) -> Self {
        Self {
            name,
            relevance: None,
            stats: OwnedRef::Ref(stats),
        }
    }
}

impl<S: UsesInput> DAFLObserver<S> for InProcessDAFLObserver<'_> {
    fn relevance(&self) -> u64 {
        self.relevance.expect("relevance not set")
    }
}

impl<'a> Named for InProcessDAFLObserver<'a> {
    fn name(&self) -> &str {
        &self.name
    }
}

impl<'a, S: UsesInput> Observer<S> for InProcessDAFLObserver<'a> {
    fn pre_exec(&mut self, _state: &mut S, _input: &S::Input) -> Result<(), libafl::Error> {
        self.stats.as_ref().reset();
        self.relevance = None;
        Ok(())
    }

    fn post_exec(
        &mut self,
        _state: &mut S,
        _input: &S::Input,
        _exit_kind: &ExitKind,
    ) -> Result<(), libafl::Error> {
        self.relevance = Some(self.stats.as_ref().compute_test_case_relevance());
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_distance_calculation() {
        let stats = DAFLStats::new();
        stats.reset();

        stats.add_bb_relevance(1);
        stats.add_bb_relevance(2);

        let test_case_relevance = stats.compute_test_case_relevance();
        assert_eq!(test_case_relevance, 3);

        stats.reset();

        let test_case_relevance = stats.compute_test_case_relevance();
        assert_eq!(test_case_relevance, 0);
    }
}
