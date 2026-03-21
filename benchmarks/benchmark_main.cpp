#include <benchmark/benchmark.h>
#include "v1_standard_allocator.hpp"
#include "v2_mutex_pool.hpp"
#include "v3_lockfree_pool.hpp"
#include "v4_thread_local_pool.hpp"
#include <vector>
#include <array>
#include <atomic>

struct DummyTrade {
    int64_t order_id;
    double price;
    int32_t quantity;
    std::array<char, 8> symbol;
};

// Global sync primitives for Producer/Consumer tests
static std::atomic<DummyTrade*> g_transfer_slot{nullptr};
static std::atomic<bool> g_producer_done{false};

// --- Best Case Benchmarks (Allocate & Deallocate on same thread) ---

static void BM_V1_Standard_Heap(benchmark::State& state) {
    HeapAllocator<DummyTrade> allocator;
    for (auto _ : state) {
        auto* ptr = allocator.allocate(12345, 100.50, 10, std::array<char, 8>{{'A', 'A', 'P', 'L'}});
        benchmark::DoNotOptimize(ptr);
        allocator.deallocate(ptr);
    }
}
BENCHMARK(BM_V1_Standard_Heap)->ThreadRange(1, 8);

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

static void BM_V4_Thread_Local_Pool(benchmark::State& state) {
    static ThreadLocalPool<DummyTrade, 1000000> pool;
    for (auto _ : state) {
        auto* ptr = pool.allocate(12345, 100.50, 10, std::array<char, 8>{{'A', 'A', 'P', 'L'}});
        if (ptr) [[likely]] {
            benchmark::DoNotOptimize(ptr);
            pool.deallocate(ptr);
        }
    }
}
BENCHMARK(BM_V4_Thread_Local_Pool)->ThreadRange(1, 8);

// --- Difficult Benchmarks (Producer/Consumer Cross-Thread) ---

template <typename Allocator>
void ProducerConsumerTest(benchmark::State& state, Allocator& pool) {
    if (state.thread_index() == 0) {
        g_transfer_slot.store(nullptr, std::memory_order_release);
    }

    for (auto _ : state) {
        if (state.thread_index() == 0) { // PRODUCER
            auto* ptr = pool.allocate(12345, 100.50, 10, std::array<char, 8>{{'A', 'A', 'P', 'L'}});
            
            // The FIX: Only put 'ptr' in if the slot is currently 'nullptr'
            DummyTrade* expected = nullptr;
            while (!g_transfer_slot.compare_exchange_weak(expected, ptr, std::memory_order_acq_rel)) {
                expected = nullptr; // Reset expected because compare_exchange modifies it on failure
                asm volatile("yield" ::: "memory");
            }
        } 
        else { // CONSUMER
            DummyTrade* ptr = nullptr;
            // The FIX: Only put 'nullptr' in if the slot currently has a valid pointer
            while (true) {
                ptr = g_transfer_slot.load(std::memory_order_acquire);
                if (ptr != nullptr && g_transfer_slot.compare_exchange_weak(ptr, nullptr, std::memory_order_acq_rel)) {
                    break; // Successfully grabbed the pointer
                }
                asm volatile("yield" ::: "memory");
            }
            pool.deallocate(ptr);
        }
    }
}

// V1 - Likely still prone to glibc aborts due to cross-thread free()
static void BM_V1_PC(benchmark::State& state) {
    static HeapAllocator<DummyTrade> pool;
    ProducerConsumerTest(state, pool);
}
//BENCHMARK(BM_V1_PC)->Threads(2);

static void BM_V2_PC(benchmark::State& state) {
    static PoolAllocator<DummyTrade, 1000000> pool;
    ProducerConsumerTest(state, pool);
}
BENCHMARK(BM_V2_PC)->Threads(2);

static void BM_V3_PC(benchmark::State& state) {
    static LockFreePool<DummyTrade, 1000000> pool;
    ProducerConsumerTest(state, pool);
}
BENCHMARK(BM_V3_PC)->Threads(2);

static void BM_V4_PC(benchmark::State& state) {
    static ThreadLocalPool<DummyTrade, 1000000> pool;
    ProducerConsumerTest(state, pool);
}
BENCHMARK(BM_V4_PC)->Threads(2);

BENCHMARK_MAIN();