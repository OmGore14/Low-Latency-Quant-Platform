# Week 2: Hands-on Experiments

This directory contains the theoretical and architectural proofs completed before building the final cache simulator. The goal of these experiments is to verify the mechanics of the L1/L2 cache hierarchy and physically measure the performance impact of data-oriented design.

## 1. Trace Analysis & Validation (`tiny.trace`)
**Command:** `python3 data/gen_trace.py --dump data/tiny.trace`

A manual, pen-and-paper validation of the first 14 memory accesses to prove the exact eviction and writeback mechanics of a 64-set L1 and 512-set L2 cache (both 8-way associative).

**Key Findings:**
* **Access 0-2 (Block 0):** Mapped to L1 Set 0. Compulsory misses followed by hits. Access 2 (Write) correctly marks Block 0 as **DIRTY**.
* **Access 3-9:** Reads to Blocks 64, 128, 192, 256, 320, 384, 448. All map to L1 Set 0 (`block % 64 == 0`). Set 0 reaches maximum capacity (8 ways full).
* **Access 10 (Block 512):** Maps to L1 Set 0. Triggers an **EVICTION** of the LRU Block 0. Because Block 0 was dirty, this triggers a **Writeback** to L2.
* **Access 11 (Block 0):** CPU requests Block 0 again. It is an L1 Miss, but successfully registers as an **L2 Hit** due to the writeback from Access 10.

---

## 2. Instruction Set Verification (Mask vs. Modulo)
**Goal:** Verify the assembly differences between modulo arithmetic (`% 64`) and bitwise masking (`& 63`) for set indexing.

**Result:**
At `-O3`, the GCC compiler recognizes `64` as a compile-time constant power of two. It automatically optimizes `address % 64` into a 1-cycle `and eax, 63` instruction. This proves that keeping cache geometry as `constexpr` completely eliminates the hardware division penalty, rendering manual bitwise operators unnecessary for constant divisors.

---

## 3. The Layout Benchmark (AoS vs. SoA)
**Goal:** Measure the wall-clock and throughput differences between a node-based architecture and a flat, contiguous memory layout on a large data set.
**Command:** `python3 data/gen_trace.py --accesses 20000000 --seed 42 --out data/large.trace`

| Architecture | Throughput (M acc/s) |
| :--- | :--- |
| **AoS (Unordered Map / List)** | 29.60 |
| **SoA (Flat Vector / Array)** | 41.68 |

**Notes:** By flattening the memory layout (Struct of Arrays) and removing heap allocations (`malloc`/`new`) inside the hot path, simulation time dropped from ~675ms to ~479ms. This ~1.4x architectural speedup confirms that pointer-chasing and hidden allocator overhead are the primary bottlenecks in simulation loops.