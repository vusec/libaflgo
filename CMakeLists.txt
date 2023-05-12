cmake_minimum_required(VERSION 3.21)

project(AFLGoNew)

include(GNUInstallDirs)

if(NOT LLVM_FOUND)
  find_package(LLVM 16 REQUIRED CONFIG)
  message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
  message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
  list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
  include(AddLLVM)
endif()

include(FetchContent)

FetchContent_Declare(
    Corrosion
    GIT_REPOSITORY https://github.com/corrosion-rs/corrosion.git
    GIT_TAG v0.3.5
)
FetchContent_MakeAvailable(Corrosion)

find_package(Python3 REQUIRED)

set(PRINTER_PLUGIN_NAME AFLGoAnalysisPrinter)
set(AFLGO_COMPILER_PLUGIN_NAME AFLGoCompilerPlugin)
set(AFLGO_LINKER_PLUGIN_NAME AFLGoLTOPlugin)
set(FUZZER_NAME fuzzer_aflgo)

add_subdirectory(passes)
add_subdirectory(fuzzer)
add_subdirectory(wrapper)

if(BUILD_TESTING)
  find_program(LIT_PROGRAM lit REQUIRED)
  add_subdirectory(test)
endif()
