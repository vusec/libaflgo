cmake_minimum_required(VERSION 3.20.0)

project(AFLGoNew)

include(GNUInstallDirs)

if(NOT LLVM_FOUND)
  find_package(LLVM REQUIRED CONFIG)
  message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
  message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
  list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
  include(AddLLVM)
endif()

find_package(Python3 REQUIRED)

set(PRINTER_PLUGIN_NAME AFLGoAnalysisPrinter)

add_subdirectory(passes)

if(BUILD_TESTING)
  find_program(LIT_PROGRAM lit REQUIRED)
  add_subdirectory(test)
endif()
