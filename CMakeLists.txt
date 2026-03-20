cmake_minimum_required(VERSION 3.14)
project(CustomMemoryAllocator VERSION 1.0 LANGUAGES CXX)

# Enforce C++20 (Crucial for Concepts and updated atomics)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# HFT Grade Compiler Flags (Optimized for your Pi 5's architecture)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -mcpu=native -Wall -Wextra -pthread")

# Automatically download and link GoogleTest
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
FetchContent_MakeAvailable(googletest)

# Automatically download and link Google Benchmark
FetchContent_Declare(
  googlebenchmark
  URL https://github.com/google/benchmark/archive/refs/tags/v1.8.3.zip
)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "Disable benchmark tests" FORCE)
FetchContent_MakeAvailable(googlebenchmark)

# Make the include directory available to all subdirectories
include_directories(include)

# Add the test and benchmark executables
add_subdirectory(tests)
add_subdirectory(benchmarks)