cmake_minimum_required(VERSION 3.24)

project(AFLGoNew)

include(GNUInstallDirs)

include(FetchContent)

FetchContent_Declare(
  Corrosion
  GIT_REPOSITORY https://github.com/corrosion-rs/corrosion.git
  GIT_TAG v0.3.5)
FetchContent_Declare(
  Z3
  URL https://github.com/Z3Prover/z3/releases/download/z3-4.8.12/z3-4.8.12-x64-glibc-2.31.zip
  FIND_PACKAGE_ARGS 4.8)
FetchContent_Declare(
  SVF
  GIT_REPOSITORY https://github.com/SVF-tools/SVF.git
  GIT_TAG d047e7edd605d93c06f29f68d344df0622e9cbc6 # June 4th, 2023
)
FetchContent_MakeAvailable(Corrosion Z3)

# SVF picks up Z3_LIBRARIES from LLVM, so import Z3 before LLVM.
find_package(LLVM 15 REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

set(Z3_DIR "${z3_SOURCE_DIR}") # SVF does not use find_package for Z3
set(SVF_ENABLE_ASSERTIONS ON CACHE BOOL "Enable SVF assertions")
FetchContent_GetProperties(SVF)
if(NOT svf_POPULATED)
  FetchContent_Populate(SVF)
  # Avoid installing SVF
  add_subdirectory(${svf_SOURCE_DIR} ${svf_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

# Fix SVF CMake targets
target_include_directories(SvfCore PUBLIC "${svf_SOURCE_DIR}/svf/include"
                                          "${svf_BINARY_DIR}/include")
target_link_libraries(SvfLLVM PUBLIC SvfCore)
set_target_properties(SvfCoreObj SvfCore SvfLLVM
                      PROPERTIES POSITION_INDEPENDENT_CODE ON)

find_package(Python3 REQUIRED)

set(PRINTER_PLUGIN_NAME AFLGoAnalysisPrinter)
set(AFLGO_COMPILER_PLUGIN_NAME AFLGoCompilerPlugin)
set(AFLGO_LINKER_PLUGIN_NAME AFLGoLTOPlugin)

add_subdirectory(passes)
add_subdirectory(fuzzers)

if(BUILD_TESTING)
  find_program(LIT_PROGRAM lit REQUIRED)
  add_subdirectory(test)
endif()
