set(FUZZERS aflgo hawkeye)

foreach(FUZZER ${FUZZERS})
  # Reset feature flags
  unset(EXTEND_CALLGRAPH)
  unset(USE_HAWKEYE_DISTANCE)
  unset(TRACE_FUNCTION_DISTANCE)

  add_subdirectory(${FUZZER})

  # Corrosion does not currently support installing libraries through CMake, so
  # it is necessary to install this one manually. See:
  # https://github.com/AndrewGaspar/corrosion/issues/64
  install(FILES $<TARGET_FILE:${FUZZER}-static> TYPE LIB)

  set(PLUGIN_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}/lib${LLVM_LIBDIR_SUFFIX}")
  set(LIBAFLGO_COMPILER_PLUGIN_PATH
      "${PLUGIN_INSTALL_PREFIX}/$<TARGET_FILE_NAME:${AFLGO_COMPILER_PLUGIN_NAME}>"
  )
  set(LIBAFLGO_LINKER_PLUGIN_PATH
      "${PLUGIN_INSTALL_PREFIX}/$<TARGET_FILE_NAME:${AFLGO_LINKER_PLUGIN_NAME}>"
  )

  set(FUZZER_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
  set(FUZZER_PATH
      "${FUZZER_INSTALL_PREFIX}/$<TARGET_FILE_NAME:${FUZZER}-static>")

  configure_file("${PROJECT_SOURCE_DIR}/wrapper/libaflgo_cc.in"
                 "libaflgo_${FUZZER}_cc.gen" @ONLY)
  file(
    GENERATE
    OUTPUT "libaflgo_${FUZZER}_cc"
    INPUT "${CMAKE_CURRENT_BINARY_DIR}/libaflgo_${FUZZER}_cc.gen")
  install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/libaflgo_${FUZZER}_cc" TYPE BIN)
  install(
    PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/libaflgo_${FUZZER}_cc"
    TYPE BIN
    RENAME "libaflgo_${FUZZER}_cxx")

  if(BUILD_TESTING)
    # If testing is enabled, build a version of the wrapper that refers to the
    # artifacts in the build directory. This version of the wrapper can be used
    # directly in tests, without requiring to install the project.

    set(LIBAFLGO_COMPILER_PLUGIN_PATH
        $<TARGET_FILE:${AFLGO_COMPILER_PLUGIN_NAME}>)
    set(LIBAFLGO_LINKER_PLUGIN_PATH $<TARGET_FILE:${AFLGO_LINKER_PLUGIN_NAME}>)

    # The `-static` suffix is needed because this is how Corruption defines the
    # target.
    set(FUZZER_PATH "$<TARGET_FILE:${FUZZER}-static>")

    configure_file("${PROJECT_SOURCE_DIR}/wrapper/libaflgo_cc.in"
                   "libaflgo_${FUZZER}_cc_test.gen" @ONLY)
    file(
      GENERATE
      OUTPUT "libaflgo_${FUZZER}_cc_test"
      INPUT "${CMAKE_CURRENT_BINARY_DIR}/libaflgo_${FUZZER}_cc_test.gen")

    add_custom_target(
      "build_libaflgo_${FUZZER}_cxx_test" ALL
      DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/libaflgo_${FUZZER}_cxx_test")
    add_custom_command(
      OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/libaflgo_${FUZZER}_cxx_test"
      COMMAND
        ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_BINARY_DIR}/libaflgo_${FUZZER}_cc_test"
        "${CMAKE_CURRENT_BINARY_DIR}/libaflgo_${FUZZER}_cxx_test"
      MAIN_DEPENDENCY "${CMAKE_CURRENT_BINARY_DIR}/libaflgo_${FUZZER}_cc_test"
      DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/libaflgo_${FUZZER}_cc_test.gen")

    list(APPEND TEST_CC_WRAPPER_PATHS
         "${CMAKE_CURRENT_BINARY_DIR}/libaflgo_${FUZZER}_cc_test")
    list(APPEND TEST_CXX_WRAPPER_PATHS
         "${CMAKE_CURRENT_BINARY_DIR}/libaflgo_${FUZZER}_cxx_test")
  endif()
endforeach()

set(FUZZERS
    ${FUZZERS}
    PARENT_SCOPE)
set(TEST_CC_WRAPPER_PATHS
    ${TEST_CC_WRAPPER_PATHS}
    PARENT_SCOPE)
set(TEST_CXX_WRAPPER_PATHS
    ${TEST_CXX_WRAPPER_PATHS}
    PARENT_SCOPE)
