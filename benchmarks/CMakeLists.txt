# Rename the target to avoid colliding with Google Benchmark's internal targets
add_executable(pool_benchmarks benchmark_main.cpp)

# Link against Google Benchmark and POSIX threads. 
# Use PRIVATE to explicitly define the linkage scope.
target_link_libraries(pool_benchmarks PRIVATE benchmark::benchmark pthread)