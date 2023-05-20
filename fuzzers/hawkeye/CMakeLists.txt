corrosion_import_crate(MANIFEST_PATH Cargo.toml CRATE_TYPES staticlib CRATES
                       ${FUZZER})

set(EXTEND_CALLGRAPH
    "TRUE"
    PARENT_SCOPE)
set(USE_HAWKEYE_DISTANCE
    "TRUE"
    PARENT_SCOPE)
set(TRACE_FUNCTION_DISTANCE
    "TRUE"
    PARENT_SCOPE)
