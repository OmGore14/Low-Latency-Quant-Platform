# Low-Latency Quantitative Trading Platform

A high-performance, C++20-based quantitative trading platform and systems-engineering testbed. This project focuses strictly on micro-architectural optimization, hardware-aware data structures, and nanosecond-level latency reduction techniques used in high-frequency trading (HFT) environments.

## 🚀 Track Progress & Architecture Highlights

### **Week 1: High-Frequency Matching Engine & Strategy**
Engineered a highly localized, zero-allocation matching engine and strategy execution environment using Data-Oriented Design (DoD). Leveraged Linux `perf` for micro-architectural profiling, achieving order-of-magnitude latency reductions by transitioning from pointer-based Array-of-Structures (AoS) layouts to contiguous, cache-friendly Struct-of-Arrays (SoA) memory models.

### **Week 2: Sub-Millisecond L1/L2 Cache Hierarchy Simulator**
Built a mathematically perfect, two-level true-LRU cache simulator processing ~40+ Million accesses/second over massive memory traces. Stripped the hot-path of all heap allocations and CPU branch mispredictions by utilizing `constexpr` template geometry for compile-time loop unrolling, branchless bitwise hit-masking (`__builtin_ctz`), and explicit hardware prefetching (`__builtin_prefetch`).

### **Week 3: Multi-Threading & Concurrency [In Progress]**
*(Preparing for lock-free data structures, `alignas(64)` false-sharing avoidance, CPU core pinning, and multi-threaded market data ingestion.)*

---
**Tech Stack & Tools:** C++20, CMake, Linux `perf`, CPU Hardware Counters, GCC Optimization (`-O3`, `-march=x86-64-v2`), WSL2/Ubuntu.