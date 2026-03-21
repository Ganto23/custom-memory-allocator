#include <benchmark/benchmark.h>
#include "v1_standard_allocator.hpp"
#include "v2_mutex_pool.hpp"
#include "v3_lockfree_pool.hpp"
#include <vector>
#include <array>

// High-frequency "Dummy" object
struct DummyTrade {
    int64_t order_id;
    double price;
    int32_t quantity;
    std::array<char, 8> symbol;
};

// --- V1: Standard Heap Benchmark ---
static void BM_V1_Standard_Heap(benchmark::State& state) {
    HeapAllocator<DummyTrade> allocator;
    for (auto _ : state) {
        auto* ptr = allocator.allocate(12345, 100.50, 10, std::array<char, 8>{{'A', 'A', 'P', 'L'}});
        benchmark::DoNotOptimize(ptr);
        allocator.deallocate(ptr);
    }
}
BENCHMARK(BM_V1_Standard_Heap)->ThreadRange(1, 8);

// --- V2: Mutex Pool Benchmark ---
// We use a large capacity to ensure we don't run out during the test
static void BM_V2_Mutex_Pool(benchmark::State& state) {
    static PoolAllocator<DummyTrade, 1000000> pool;
    for (auto _ : state) {
        auto* ptr = pool.allocate(12345, 100.50, 10, std::array<char, 8>{{'A', 'A', 'P', 'L'}});
        if (ptr) {
            benchmark::DoNotOptimize(ptr);
            pool.deallocate(ptr);
        }
    }
}
BENCHMARK(BM_V2_Mutex_Pool)->ThreadRange(1, 8);

static void BM_V3_LockFree_Pool(benchmark::State& state) {
    static LockFreePool<DummyTrade, 1000000> pool;
    
    for (auto _ : state) {
        auto* ptr = pool.allocate(12345, 100.50, 10, std::array<char, 8>{{'A', 'A', 'P', 'L'}});
        if (ptr) [[likely]] {
            benchmark::DoNotOptimize(ptr);
            pool.deallocate(ptr);
        }
    }
}
BENCHMARK(BM_V3_LockFree_Pool)->ThreadRange(1, 8);

BENCHMARK_MAIN();