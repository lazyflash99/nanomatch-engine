cmake_minimum_required(VERSION 3.20)
project(nanomatch LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -----------------------------------------------------------------------------
# Optimization Flags for Ultra-Low Latency
# -----------------------------------------------------------------------------
# -O3: Maximum optimization
# -march=native: Enable all instruction sets supported by the host CPU (AVX2, AVX512, etc.)
# -flto: Link Time Optimization to enable cross-module inlining
# -fno-plt: Reduce overhead of calling functions in shared libraries
# -fomit-frame-pointer: Free up a register for general use
add_compile_options(-O3 -march=native -flto -fno-plt -fomit-frame-pointer -Wall -Wextra -Werror)
add_link_options(-flto -fno-plt)

include_directories(include)

# Main Executable
add_executable(nanomatch_engine src/main.cpp)

# Unit Testing
add_executable(nanomatch_test tests/unit_tests.cpp)
target_link_libraries(nanomatch_test benchmark::benchmark)
target_compile_options(nanomatch_test PRIVATE -UNDEBUG)

# Benchmarking
# Note: Requires google-benchmark installed (e.g., sudo apt install libbenchmark-dev)
find_package(benchmark REQUIRED)
add_executable(nanomatch_bench tests/benchmark_main.cpp)
target_link_libraries(nanomatch_bench benchmark::benchmark)
