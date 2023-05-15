use libafl::prelude::StdMapObserver;

const TARGETS_MAP_SIZE: usize = 256;
static mut TARGETS_MAP: [u8; TARGETS_MAP_SIZE] = [0; TARGETS_MAP_SIZE];

// Called by the target instrumentation
#[no_mangle]
pub unsafe extern "C" fn __aflgo_trace_bb_target(target_id: u32) {
    TARGETS_MAP[target_id as usize] += 1;
}

pub unsafe fn get_targets_map_observer<'a, S>(name: S) -> StdMapObserver<'a, u8, false>
where
    S: Into<String>,
{
    StdMapObserver::new(name, &mut TARGETS_MAP)
}
