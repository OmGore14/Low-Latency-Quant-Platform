# Low-Latency Quant Platform

**Author:** Om Gore

A C++ quantitative trading engine engineered for extreme low latency. This project implements a deterministic Z-Score mean-reversion strategy while aggressively minimizing CPU cache misses, branch mispredictions, and memory allocations on the hot path.

## Hardware Setup

* **Machine:** ASUS TUF Gaming F15
* **CPU:** Intel Core i5-12500H (Alder Lake Hybrid Architecture: P-Cores & E-Cores)
* **RAM:** 16 GB DDR4
* **OS / Kernel:** Ubuntu Linux
* **Compiler:** GCC / Clang (`-O2 -march=native`)

## Headline Performance

* **p50 Latency:** <= 128 ns
* **p90 Latency:** <= 128 ns
* **p99 Latency:** <= 256 ns
* **p999 Latency:** <= 512 ns

## Build & Run Instructions

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/quant_runner build/spec_strategy.so data/synthetic_large.csv

```

## Microbenchmarks

```bash
./build/bench_math

```

## Performance Profiling (`perf stat`)

```text
 Performance counter stats for './build/quant_runner build/spec_strategy.so data/synthetic_large.csv':

    19,179,571,124      cpu_core/cpu-cycles/                                          (99.64%)
    11,040,557,524      cpu_atom/cpu-cycles/                                          (0.36%)
    26,062,094,876      cpu_atom/instructions/                                        (0.36%)
    61,505,982,884      cpu_core/instructions/                                        (99.64%)
        21,381,205      cpu_atom/branch-misses/                                       (0.36%)
        40,340,982      cpu_core/branch-misses/                                       (99.64%)
   <not supported>      cpu_atom/L1-dcache-load-misses/                               
        17,669,654      cpu_core/L1-dcache-load-misses/                               (99.64%)

      10.716420511 seconds time elapsed

      10.018980000 seconds user
       0.695929000 seconds sys

```

**Analysis Note:** By explicitly profiling the hybrid architecture, we can see the workload was successfully pinned primarily to the Performance Cores (`cpu_core`). The P-cores achieved a staggering Instructions Per Cycle (IPC) ratio of 3.2 (61.5B instructions / 19.1B cycles). L1 data cache misses and branch mispredictions were crushed to fractions of a percent, validating the contiguous memory layout and branchless fast-path design.

## What Surprised Me

The most surprising takeaway from this build was realizing that a mathematically superior algorithm (O(1) time complexity) can actually be significantly slower in production than an O(N) algorithm due to hardware realities. I initially implemented an O(1) rolling sum to calculate the strategy's variance. However, the accumulation of microscopic floating-point rounding errors caused the math to drift, failing the strict bitwise correctness checks of the judge. Attempting to fix that float drift required inserting safety bounds and `if` statements, which utterly destroyed the CPU's branch predictor and caused massive tail latency spikes. I learned that executing a pure 8-Way unrolled O(N) loop on contiguous, cache-aligned memory is far faster. It eliminates branch mispredictions entirely and allows the CPU to fully leverage its superscalar execution ports and AVX registers.