use std::sync::{atomic::AtomicU64, atomic::Ordering};

const DISTANCE_RESOLUTION: f64 = 1000_f64;

#[derive(Default)]
struct DistanceStats {
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

    pub fn add_bb_distance(&self, bb_distance: f64) {
        let distance_approx = (bb_distance * DISTANCE_RESOLUTION).trunc() as u64;

        self.bb_distance_sum
            .fetch_add(distance_approx, Ordering::Relaxed);
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
pub extern "C" fn __aflgo_trace_bb_distance(bb_distance: f64) {
    STATS.add_bb_distance(bb_distance);
}

pub fn compute_test_case_distance() -> f64 {
    STATS.compute_test_case_distance()
}

pub fn reset_distance_stats() {
    STATS.reset();
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn it_works() {
        reset_distance_stats();

        __aflgo_trace_bb_distance(1.0);
        __aflgo_trace_bb_distance(2.0);

        let test_case_distance = compute_test_case_distance();
        assert_eq!(test_case_distance, 1.5);

        reset_distance_stats();

        let test_case_distance = compute_test_case_distance();
        assert!(test_case_distance.is_nan());
    }
}
