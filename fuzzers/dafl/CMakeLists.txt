corrosion_import_crate(MANIFEST_PATH Cargo.toml CRATE_TYPES staticlib CRATES
                       ${FUZZER})

set(DAFL_MODE
    "TRUE"
    PARENT_SCOPE)
