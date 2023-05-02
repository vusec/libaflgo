set(PLUGIN_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}/lib${LLVM_LIBDIR_SUFFIX}")
set(AFLGO_PLUGIN_PATH
    "${PLUGIN_INSTALL_PREFIX}/$<TARGET_FILE_NAME:${AFLGO_PLUGIN_NAME}>")

set(FUZZER_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
set(FUZZER_PATH
    "${FUZZER_INSTALL_PREFIX}/$<TARGET_FILE_NAME:${FUZZER_NAME}-static>")

configure_file(libaflgo_cc.in libaflgo_cc.gen @ONLY)
file(
  GENERATE
  OUTPUT libaflgo_cc
  INPUT "${CMAKE_CURRENT_BINARY_DIR}/libaflgo_cc.gen")
install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/libaflgo_cc" TYPE BIN)
install(
  PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/libaflgo_cc"
  TYPE BIN
  RENAME libaflgo_cxx)

if(BUILD_TESTING)
  # If testing is enabled, build a version of the wrapper that refers to the
  # artifacts in the build directory. This version of the wrapper can be used
  # directly in tests, without requiring to install the project.

  set(AFLGO_PLUGIN_PATH $<TARGET_FILE:${AFLGO_PLUGIN_NAME}>)

  # The `-static` suffix is needed because this is how Corruption defines the
  # target.
  set(FUZZER_PATH "$<TARGET_FILE:${FUZZER_NAME}-static>")

  configure_file(libaflgo_cc.in libaflgo_cc_test.gen @ONLY)
  file(
    GENERATE
    OUTPUT libaflgo_cc_test
    INPUT "${CMAKE_CURRENT_BINARY_DIR}/libaflgo_cc_test.gen")
  add_custom_target(build_libaflgo_cxx_test ALL
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/libaflgo_cxx_test")
  add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/libaflgo_cxx_test"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_BINARY_DIR}/libaflgo_cc_test"
            "${CMAKE_CURRENT_BINARY_DIR}/libaflgo_cxx_test")

  set(${PROJECT_NAME}_CC_WRAPPER_PATH
      "${CMAKE_CURRENT_BINARY_DIR}/libaflgo_cc_test"
      CACHE INTERNAL "")
  set(${PROJECT_NAME}_CXX_WRAPPER_PATH
      "${CMAKE_CURRENT_BINARY_DIR}/libaflgo_cxx_test"
      CACHE INTERNAL "")
endif()
