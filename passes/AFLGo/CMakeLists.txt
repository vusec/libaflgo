add_llvm_library(${AFLGO_PLUGIN_NAME} MODULE AFLGo.cpp)
target_compile_definitions(${AFLGO_PLUGIN_NAME} PRIVATE ${LLVM_DEFINITIONS})
target_include_directories(${AFLGO_PLUGIN_NAME} PRIVATE ${LLVM_INCLUDE_DIRS})
target_link_libraries(${AFLGO_PLUGIN_NAME} PRIVATE Analysis)