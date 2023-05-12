// Called by the target instrumentation
#[no_mangle]
pub extern "C" fn __aflgo_trace_bb_target(_target_id: u32) {}
