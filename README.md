# High-Performance C++ Memory Allocator for Trading Systems

This repository contains a C++ project demonstrating the design and implementation of a high-performance, thread-safe memory management system for low-latency applications. The final version features a generic, templated pool allocator and a circular buffer, which together provide a **~26x performance increase** over the standard system allocator in relevant benchmarks.

## Project Versions

This repository contains two major versions, demonstrating the evolution of the design:

1.  **`v1-specific-allocator.cpp`**: A complete, thread-safe implementation where the allocator and buffer are specifically designed for a `Trade` struct. This version uses `operator new/delete` overloading.
2.  **`v2-generic-allocator.cpp`**: The final, professional-grade version. Both the `PoolAllocator` and `CircularBuffer` have been converted into reusable templates. This version is more robust, flexible, and showcases advanced C++ design patterns.

## Key Features (Final Version)

* **Generic Pool Allocator:** A `PoolAllocator<T, N>` template that provides fixed-size block allocation for any data type, avoiding expensive system calls and eliminating memory fragmentation.
* **Configurable Thread Safety:** The allocator uses template specialization (`std::conditional`) to select between `std::mutex` for thread-safe operation and a zero-overhead `DummyMutex` for single-threaded performance benchmarks, ensuring a fair and accurate comparison.
* **High-Throughput Circular Buffer:** A generic, thread-safe `CircularBuffer<T, N>` that owns its own `PoolAllocator` instance, providing a fully encapsulated, high-performance container for data streams.
* **Low-Latency Indexing:** The circular buffer uses a power-of-2 capacity to enable ultra-fast bitwise AND (`&`) operations for index wrap-around instead of slower modulo arithmetic.
* **Modern C++ Design:** Demonstrates clean, modern C++ practices including RAII (`std::lock_guard`), `const` correctness (`mutable` mutex), and compile-time configuration with non-type template parameters.

## Performance Analysis & Results

To validate its effectiveness, the custom allocator was benchmarked against the standard system allocator (`macOS default`).

*(Benchmarks were compiled with g++ -O3 on an Apple M-series CPU)*

### 1. Component Micro-Benchmark

This test measures the raw throughput of 5 million realistic allocation/deallocation cycles, isolating the performance of the allocator's core logic. The non-thread-safe version of the pool allocator was used for a fair, apples-to-apples comparison against the mostly lock-free standard allocator in a single-threaded context.

| Allocator               | Time (5M ops)   | Speedup      |
| ----------------------- | --------------- | ------------ |
| Standard `new`/`delete` | 124.67 ms       | 1.0x         |
| **Custom Pool Allocator** | **4.76 ms** | **~26.2x** |

The custom pool allocator is **over 26 times faster** than the default system allocator, proving its efficiency for the fixed-size allocation pattern common in low-latency applications.

### 2. Realistic System Simulation

This test measures the allocator's impact on the complete, multi-threaded application.

| Allocator               | Total Simulation Time | System-Level Improvement |
| ----------------------- | --------------------- | ------------------------ |
| Standard `new`/`delete` | 863.6 ms              | -                        |
| **Custom Pool Allocator** | **774.3 ms** | **~10.3% faster** |

This demonstrates that the massive component-level speedup translates into a significant, practical performance gain in a realistic, concurrent application, where other factors like mutex contention and thread scheduling contribute to the total runtime.

## How to Build and Run

The project is contained in a single file. The final version is `v2-generic-allocator.cpp`.

### Build Command

```bash
# The -O3 flag enables optimizations, which is important for benchmarks
# The -lpthread flag is required to link the std::thread library
g++ -std=c++17 -O3 -o allocator_project v2-generic-allocator.cpp -lpthread
```

### Run Command

```bash
./allocator_project
```
The program will first run the multi-threaded simulation to test for correctness and thread safety, and then run the micro-benchmark to output the performance results.

## System Design Deep Dive

### The Problem

In low-latency applications, the standard C++ `new` and `delete` operators are a major performance bottleneck. They can be slow and unpredictable (non-deterministic) due to system call overhead, heap contention in multi-threaded scenarios, and memory fragmentation.

### The Solution: Pool Allocator + Circular Buffer

This project solves the problem with a two-part architecture where the `CircularBuffer` class owns its own `PoolAllocator` instance.

1.  **`PoolAllocator`:** This class manages a large, contiguous block of memory pre-allocated at startup. It uses a "free list"—a simple linked list of available memory chunks—to handle `allocate()` and `deallocate()` requests in O(1) time with simple pointer manipulation. This completely avoids expensive system calls during the main program loop.

2.  **`CircularBuffer`:** This class acts as the high-level data structure. By owning its allocator, it becomes a fully self-contained system. This design is highly robust and flexible, allowing multiple, independent circular buffers of different types and sizes to coexist without interfering with each other. The buffer enforces a "first-in, first-out" policy by overwriting the oldest data once its capacity is reached, making it ideal for processing real-time data streams.

---
*This project was developed as a practical exploration of high-performance C++ techniques relevant to the quantitative finance industry.*
