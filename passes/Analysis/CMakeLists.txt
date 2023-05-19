add_library(Analysis TargetDetection.cpp FunctionDistance.cpp
                     BasicBlockDistance.cpp ExtendedCallGraphAnalysis.cpp)
set_property(TARGET Analysis PROPERTY POSITION_INDEPENDENT_CODE TRUE)
target_compile_definitions(Analysis PRIVATE ${LLVM_DEFINITIONS})
target_include_directories(
  Analysis
  PUBLIC ${CMAKE_SOURCE_DIR}/include
  PRIVATE ${LLVM_INCLUDE_DIRS})
target_link_libraries(Analysis PRIVATE SvfLLVM)
