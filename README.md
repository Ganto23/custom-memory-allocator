# High-Frequency Trading (HFT) Bare-Metal Memory Allocator

An enterprise-grade, ultra-low latency memory allocator designed for High-Frequency Trading (HFT) engines. Built entirely in modern C++, this allocator systematically bypasses the Linux kernel, mitigates cross-core cache contention, and operates at the absolute physical limits of the CPU silicon.

## Table of Contents
* [Performance Snapshot](#-performance-snapshot)
* [The Iterative Engineering Journey](#-the-iterative-engineering-journey)
  * [V1: The Standard Heap (Baseline)](#v1-the-standard-heap-baseline)
  * [V2: The Mutex Pool (OS Bottleneck)](#v2-the-mutex-pool-os-bottleneck)
  * [V3: The Lock-Free Pool (Hardware Bottleneck)](#v3-the-lock-free-pool-hardware-bottleneck)
  * [V4: Thread-Local Magazines (Zero Contention)](#v4-thread-local-magazines-zero-contention)
* [STL Container Integration](#-stl-container-integration)
* [Hardware & System Optimizations](#-hardware--system-optimizations)
* [The Mathematical Limit (Hitting the Silicon Wall)](#-the-mathematical-limit-hitting-the-silicon-wall)

---

## 🚀 Performance Snapshot

**Hardware:** Cortex-A76 (Raspberry Pi 5 @ 2.4 GHz)
**Environment:** Isolated CPU cores, HugePages, `schedutil` bypassed

| Architecture | Latency (ns) | CPU Cycles | Notes |
| :--- | :--- | :--- | :--- |
| `BM_V1_Standard_Heap` | 7.55 ns | ~18 | Standard `malloc`/`free` fallback |
| `BM_V2_Mutex_Pool` | 207 ns | ~497 | Thread-safe, heavy OS context switching |
| `BM_V3_LockFree_Pool` | 112 ns | ~269 | Lock-free atomics, heavy cache-line contention |
| `BM_V4_Thread_Local_Pool` | **3.33 ns** | **8** | **Zero-contention L1 cache magazines** |

---

## 🧠 The Iterative Engineering Journey

This project was built by systematically destroying latency bottlenecks layer by layer—moving from the operating system down to the bare metal.

### V1: The Standard Heap (Baseline)
Relying on the default `std::allocator`, the system must traverse the glibc heap, look for free memory blocks, and occasionally ask the Linux kernel for more RAM via `brk()` or `mmap()`. This administrative overhead is unacceptable for HFT.

### V2: The Mutex Pool (OS Bottleneck)
The first optimization was pre-allocating memory into a fixed-size array. To make it thread-safe, a `std::mutex` was used. 
*   **The Problem:** Cross-core latency spiked to 207ns. The Linux kernel was actively putting waiting threads to sleep and waking them up, completely destroying CPU pipeline efficiency and polluting the instruction cache.

### V3: The Lock-Free Pool (Hardware Bottleneck)
Locks were stripped out entirely in favor of a `std::atomic` Compare-And-Swap (CAS) loop.
*   **The Problem:** While this bypassed the OS kernel, it introduced a hardware bottleneck. Multiple cores were repeatedly hammering the exact same memory address (the global head pointer) to update it. This caused severe cache-line contention, forcing the MESI protocol to constantly sync L1 caches across the motherboard. Latency hovered at 112ns.

### V4: Thread-Local Magazines (Zero Contention)
To achieve true zero-contention, the architecture was shifted to a Thread-Local Magazine model. 
1.  **Isolated Caches:** Each thread maintains its own isolated `Magazine` (a local array of indices).
2.  **Batch Refilling:** Threads only access the global atomic pool when their local magazine is empty, grabbing a batch of 64 slots at once.
3.  **Result:** 98% of allocations occur entirely within the CPU's private L1 cache without ever triggering cross-core invalidations. Latency dropped to an ultra-stable **3.33ns**.

```cpp
// The Thread-Local Magazine Structure
struct alignas(64) Magazine {
    static constexpr size_t BatchSize = 64;
    uint32_t indices[BatchSize];
    size_t count = 0;
};

// thread_local instantiation prevents false-sharing
thread_local Magazine local_cache;