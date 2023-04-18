set(PRINTER_PLUGIN_PATH $<TARGET_FILE:${PRINTER_PLUGIN_NAME}>)
set(AFLGO_PLUGIN_PATH $<TARGET_FILE:${AFLGO_PLUGIN_NAME}>)
set(TEST_CC_WRAPPER_PATH ${${PROJECT_NAME}_CC_WRAPPER_PATH})
set(TEST_CXX_WRAPPER_PATH ${${PROJECT_NAME}_CXX_WRAPPER_PATH})

set(TEST_DEPS ${AFLGO_PLUGIN_NAME} ${PRINTER_PLUGIN_NAME} ${TEST_WRAPPER_PATH})

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
