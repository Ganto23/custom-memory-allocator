# High-Performance C++ Memory Allocator for Trading Systems

This project is a complete C++ implementation of a high-performance, thread-safe memory management system designed for low-latency applications like quantitative trading. It features a custom pool allocator and a circular buffer that together provide a significant performance increase over standard library equivalents.

## Key Features

* **Custom Pool Allocator:** A fixed-size block allocator that completely avoids expensive system calls (`malloc`) during runtime, drastically reducing latency and eliminating memory fragmentation.
* **High-Throughput Circular Buffer:** A thread-safe, fixed-size container for storing a stream of objects (e.g., `Trade` data) with O(1) complexity for push operations.
* **Low-Latency Indexing:** Uses a power-of-2 capacity to enable ultra-fast bitwise AND (`&`) operations for index wrap-around instead of slower modulo arithmetic.
* **Robust Thread Safety:** Correctly implements mutex locks (`std::lock_guard`) on all critical sections to ensure safe use in a concurrent environment with multiple reader/writer threads.
* **Modern C++ Design:** Demonstrates clean, modern C++ practices including RAII, `const` correctness (`mutable` mutex), move semantics, and smart use of the standard library.

## Performance Analysis & Results

To validate its effectiveness, the custom allocator was benchmarked against the standard system allocator (`macOS default`). The results clearly demonstrate the significant performance gains achieved.

### 1. Component Micro-Benchmark

This test measures the raw throughput of 5 million allocation and deallocation cycles, isolating the performance of the allocator itself.

| Allocator               | Time (5M ops) | Throughput               | Speedup |
| ----------------------- | ------------- | ------------------------ | ------- |
| Standard `new`/`delete` | 114.6 ms      | ~43.6 Million ops/sec    | 1.0x    |
| **Custom Pool Allocator** | **25.5 ms** | **~196.3 Million ops/sec** | **~4.5x** |

The custom pool allocator is **4.5 times faster** than the default system allocator for this workload, showcasing its efficiency for the fixed-size allocation pattern common in trading systems.

### 2. Realistic System Simulation

This test measures the allocator's impact on a complete, multi-threaded application with one writer and one reader thread.

| Allocator               | Total Simulation Time | System-Level Improvement |
| ----------------------- | --------------------- | ------------------------ |
| Standard `new`/`delete` | 863.6 ms              | -                        |
| **Custom Pool Allocator** | **774.3 ms** | **~10.3% faster** |

This demonstrates that the component-level speedup translates into a significant, practical performance gain in a realistic, concurrent application.

## How to Build and Run

This project is contained in a single file and can be compiled with a C++17 compliant compiler (like `g++`).

### Build Command

```bash
# The -O3 flag enables optimizations, which is important for benchmarks
# The -lpthread flag is required to link the std::thread library
g++ -std=c++17 -O3 -o allocator_project custom-memory-allocator.cpp -lpthread
```

### Run Command

```bash
./allocator_project
```

To test the performance of the standard allocator for comparison, comment out the `Trade::operator new` and `Trade::operator delete` functions and re-compile.

## System Design and Implementation

### The Problem

In low-latency applications, the standard C++ `new` and `delete` operators are a major performance bottleneck. They can be slow and unpredictable (non-deterministic) due to system call overhead, heap contention in multi-threaded scenarios, and memory fragmentation.

### The Solution: Pool Allocator + Circular Buffer

This project solves the problem with a two-part architecture:

1.  **`TradeAllocator`:** A custom pool allocator is implemented to manage a large, contiguous block of memory pre-allocated at startup. It uses a "free list" (a linked list of available memory chunks) to handle `allocate()` and `deallocate()` requests in O(1) time with simple pointer manipulation, completely avoiding system calls during the main program loop.

2.  **`CircularBuffer`:** This class acts as the high-level data structure. It uses the `TradeAllocator` to store pointers to `Trade` objects. This design decouples the lifetime of a `Trade` object from its presence in the buffer, providing the flexibility needed for complex systems where multiple components might need to access the same data for different durations. The buffer enforces a "first-in, first-out" policy by overwriting the oldest data once its capacity is reached, making it ideal for processing real-time data streams.

---
*This project was developed as a practical exploration of high-performance C++ techniques relevant to the quantitative finance industry.*
