cmake_minimum_required(VERSION 3.16)

set(WINSIZE_VERSION "0.1.0")
project(winsize VERSION ${WINSIZE_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Project-local CMake modules live under cmake/.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# Test suite.
option(BUILD_TESTING "Build and register test targets" OFF)

# Normalize architecture name for output folder layout.
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(PLATFORM_NAME x64)
else()
    set(PLATFORM_NAME x86)
endif()

# Keep all binary artifacts under out/<arch>-<Config>.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/out/${PLATFORM_NAME}-$<CONFIG>)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/out/${PLATFORM_NAME}-$<CONFIG>)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/out/${PLATFORM_NAME}-$<CONFIG>)

# Apply compiler warning policy across all targets.
include(CompilerOptions)
winsize_apply_compiler_options()

# Build third-party, libraries, then applications.
add_subdirectory(external)
add_subdirectory(libs)
add_subdirectory(apps)

# Tests are registered in tests/CMakeLists.txt.
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
