use once_cell::sync::Lazy;
use std::sync::Mutex;

#[derive(Default)]
struct DistanceStats {
    bb_distance_sum: f64,
    bb_distance_count: u64,
}

impl DistanceStats {
    pub fn add_bb_distance(&mut self, bb_distance: f64) {
        self.bb_distance_sum += bb_distance;
        self.bb_distance_count += 1;
    }

    pub fn compute_test_case_distance(&self) -> f64 {
        self.bb_distance_sum / self.bb_distance_count as f64
    }

    pub fn reset(&mut self) {
        self.bb_distance_sum = 0.0;
        self.bb_distance_count = 0;
    }
}

static STATS: Lazy<Mutex<DistanceStats>> = Lazy::new(|| Mutex::new(DistanceStats::default()));

// Called by the AFLGo instrumentation
#[no_mangle]
pub extern "C" fn __aflgo_trace_bb_distance(bb_distance: f64) {
    let mut stats = STATS.lock().unwrap();
    stats.add_bb_distance(bb_distance);
}

pub fn compute_test_case_distance() -> f64 {
    let stats = STATS.lock().unwrap();
    stats.compute_test_case_distance()
}

pub fn reset_distance_stats() {
    let mut stats = STATS.lock().unwrap();
    stats.reset();
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
