set(PRINTER_PLUGIN_PATH $<TARGET_FILE:${PRINTER_PLUGIN_NAME}>)
set(AFLGO_COMPILER_PLUGIN_PATH $<TARGET_FILE:${AFLGO_COMPILER_PLUGIN_NAME}>)
set(AFLGO_LINKER_PLUGIN_PATH $<TARGET_FILE:${AFLGO_LINKER_PLUGIN_NAME}>)

set(TEST_DEPS
    ${AFLGO_COMPILER_PLUGIN_NAME} ${AFLGO_LINKER_PLUGIN_NAME}
    ${PRINTER_PLUGIN_NAME})
foreach(FUZZER ${FUZZERS})
  list(APPEND TEST_DEPS ${FUZZER}-static)
endforeach()
foreach(TEST_CC_WRAPPER ${TEST_CC_WRAPPERS})
  list(APPEND TEST_DEPS ${TEST_CC_WRAPPER})
endforeach()
foreach(TEST_CXX_WRAPPER ${TEST_CXX_WRAPPERS})
  list(APPEND TEST_DEPS ${TEST_CXX_WRAPPER})
endforeach()

configure_file(lit.cfg.in lit.cfg.gen @ONLY)
file(
  GENERATE
  OUTPUT lit.cfg
  INPUT "${CMAKE_CURRENT_BINARY_DIR}/lit.cfg.gen")

add_custom_target(
  ${PROJECT_NAME}-check
  COMMAND ${LIT_PROGRAM} ${CMAKE_CURRENT_BINARY_DIR} -sv
  DEPENDS ${TEST_DEPS})

if(NOT TARGET check)
  # This target may have been created by the parent project
  add_custom_target(check)
endif()
add_dependencies(check ${PROJECT_NAME}-check)

add_custom_command(
  TARGET check
  PRE_BUILD
  COMMAND cargo test
  USES_TERMINAL
  COMMENT "Running Rust tests")
