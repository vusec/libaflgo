add_llvm_library(${PRINTER_PLUGIN_NAME} MODULE AnalysisPrinter.cpp BUILDTREE_ONLY)
target_compile_definitions(${PRINTER_PLUGIN_NAME} PRIVATE ${LLVM_DEFINITIONS})
target_include_directories(${PRINTER_PLUGIN_NAME} PRIVATE ${LLVM_INCLUDE_DIRS})
target_link_libraries(${PRINTER_PLUGIN_NAME} PRIVATE Analysis)
