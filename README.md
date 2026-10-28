# Lab 4: Matrix Row Sum & Instruction-Level Parallelism (ILP)

**Focus:** Memory Aliasing, Register Promotion, Loop Unrolling, and Data Dependency Chains

---

## 1. Overview & Objectives

In modern computer architectures, writing high-performance C code requires understanding how the compiler interacts with CPU registers, caches, and hardware execution pipelines.

In this lab, you will optimize a simple linear algebra operation: computing the row sums of an N × N matrix of double-precision floating-point numbers.

By completing this lab, you will learn to:
* Identify and eliminate **Memory Aliasing** bottlenecks using **Register Promotion**.
* Measure execution performance using **Cycles Per Element (CPE)**.
* Apply **Loop Unrolling** to reduce branch overhead and loop control costs.
* Exploit Instruction-Level Parallelism (**ILP**) using multiple parallel accumulators to break sequential latency bounds.
* Correctly handle odd matrix dimensions using **Cleanup Loops**.

---

## 2. Theoretical Background

### A. Memory Aliasing & The Pointer Problem
Memory Aliasing occurs when two or more pointers refer to overlapping regions of memory. Consider the naive baseline implementation for computing matrix row sums:

```c
void matrix_row_sum1(double *matrix, volatile double *row_sums, long size) {
    long i, j;
    for (i = 0; i < size; i++) {
        row_sums[i] = 0.0;
        for (j = 0; j < size; j++) {
            row_sums[i] += matrix[i * size + j]; // Read-Modify-Write Bottleneck!
        }
    }
}
```

If the compiler cannot mathematically prove that `row_sums` and `matrix` point to completely separate memory regions, it must assume the worst: writing to `row_sums[i]` might alter the values inside `matrix`.

To guarantee program correctness, an optimizing compiler playing it safe cannot keep the running total in a fast CPU register. Instead, it generates a heavy **Read-Modify-Write (RMW)** sequence on every single iteration of `j`:
1. Load `row_sums[i]` from memory into a floating-point register (`%xmm0`).
2. Add `matrix[i * size + j]` to `%xmm0`.
3. Store `%xmm0` back to memory at `row_sums[i]`.

For an 8193 × 8193 matrix, this results in over 67 million memory store instructions.

### B. The Role of `volatile` in the Baseline
In modern microprocessors, ultra-fast L1 Store Buffers can sometimes hide memory store latencies when arrays fit in cache, making memory-bound code appear deceptively fast.

To model the true architectural penalty of severe memory aliasing and forced memory-sink operations, our baseline parameter marks `row_sums` as `volatile double *`.

The `volatile` keyword informs the compiler that the memory location may be modified externally or has side effects. It explicitly forbids the compiler from caching the value in a register across loop iterations, forcing a genuine memory read and write on every element.

### C. Register Promotion & Local Scalars
To eliminate memory aliasing bottlenecks, you will perform **Register Promotion**.

By declaring a local scalar variable inside the row loop (`double sum = 0.0;`), the C language guarantees that no external pointer can alias `sum`. The compiler can safely promote `sum` into a dedicated CPU register (such as `%xmm0`).

```c
// Register Promotion Pattern
for (i = 0; i < size; i++) {
    double sum = 0.0; // Cached in a CPU register (%xmm0)
    for (j = 0; j < size; j++) {
        sum += ...;   // FAST: Register-to-register addition!
    }
    row_sums[i] = sum; // Single memory write per row!
}
```

Instead of N² memory writes, this reduces total stores down to N (8193 writes total)—yielding an immediate 4× to 5× speedup.

---

## 3. Lab Instructions & Tasks

You are provided with `lab4_matrix.c`. Compile the template using GCC with `-O1` optimization enabled:

```bash
gcc -O1 -Wall lab4_matrix.c -o lab4_matrix -lm
```

> [!NOTE]
> **Dimension N = 8193:** The benchmark sets N = 8193 (an odd number). Your unrolled loops must include a cleanup loop to correctly process the final remaining column, or correctness verification will fail!

### Task 1: Register Caching & Pointer Arithmetic (`matrix_row_sum2`)
* **Goal:** Eliminate the N² memory writes and redundant matrix index calculations.
* **Instructions:**
  1. Declare a local scalar `double sum = 0.0;` inside the row loop to accumulate the row total in a CPU register.
  2. Compute a pointer `double *row_ptr = matrix + (i * size);` prior to entering the inner loop to avoid re-evaluating `i * size` multiplication on every element.
  3. Accumulate `row_ptr[j]` into `sum`.
  4. Write `sum` back to `row_sums[i]` once at the end of each row.

### Task 2: 2x1 Loop Unrolling (`matrix_row_sum3`)
* **Goal:** Reduce loop branch overhead and loop counter update instructions.
* **Instructions:**
  1. Retain register caching and row pointer calculations from Task 1.
  2. Unroll the inner loop by a factor of 2 (i.e., `j += 2`), processing two elements (`row_ptr[j]` and `row_ptr[j + 1]`) into `sum` per iteration.
  3. Define a boundary limit `long limit = size - 1;` so the main unrolled loop stops safely before overflowing array bounds.
  4. Implement a cleanup loop immediately after the unrolled loop:
     ```c
     for (; j < size; j++) {
         sum += row_ptr[j];
     }
     ```

### Task 3: 2x2 Unrolling with Parallel Accumulators (`matrix_row_sum4`)
* **Goal:** Exploit Instruction-Level Parallelism (ILP) by breaking the sequential dependency chain of floating-point additions.
* **Background:** Floating-point addition on modern x86-64 CPUs typically has a latency of 3 to 5 clock cycles. In Task 2, `sum += ...` forces each addition to wait for the previous addition to complete.
* **Instructions:**
  1. Declare two independent local accumulators: `double sum0 = 0.0;` and `double sum1 = 0.0;`.
  2. In the unrolled loop (`j += 2`), accumulate even columns into `sum0` and odd columns into `sum1`:
     ```c
     sum0 += row_ptr[j];
     sum1 += row_ptr[j + 1];
     ```
  3. Include the cleanup loop for odd matrix sizes (accumulating any remaining element into `sum0`).
  4. At the end of the row, combine both accumulators and write to memory:
     ```c
     row_sums[i] = sum0 + sum1;
     ```

---

## 4. Verification & Benchmarking

When you run `./lab4_matrix`, the harness automatically validates your code against the baseline before printing benchmark metrics:

```text
=======================================================================
CS:APP Lab 4 — Matrix Row Sum Optimization Benchmark (-O1)
Matrix Dimension: 8193 x 8193 (67125249 elements)
=======================================================================
Computing baseline reference values...
-----------------------------------------------------------------------
CORRECTNESS VERIFICATION (Testing odd N = 8193 for cleanup loop correctness):
-----------------------------------------------------------------------
[PASS] 2. Register Caching (TODO 1)        | Output matches baseline perfectly.
[PASS] 3. 2x1 Loop Unrolling (TODO 2)      | Output matches baseline perfectly.
[PASS] 4. 2x2 Accumulators (TODO 3)        | Output matches baseline perfectly.
-----------------------------------------------------------------------
PERFORMANCE BENCHMARKS (-O1 Optimization):
-----------------------------------------------------------------------
Implementation                     |   Time (s) |   Est. CPE |  Speedup
-----------------------------------------------------------------------
1. Baseline (Memory Aliasing)      |    0.2850 s |      12.74 CPE |    1.00x
2. Register Caching (TODO 1)       |    0.0610 s |       2.73 CPE |    4.67x
3. 2x1 Loop Unrolling (TODO 2)     |    0.0480 s |       2.15 CPE |    5.94x
4. 2x2 Accumulators (TODO 3)       |    0.0250 s |       1.12 CPE |   11.40x
=======================================================================
```

---

## 5. Deliverables & Questions for Analysis

Submit your completed `lab4_matrix.c` file along with a short lab report answering the following analysis questions:

1. **Memory Bottlenecks:** Explain why changing `row_sums[i] += ...` to a local variable `double sum` drastically reduces clock cycles per element (CPE). How many memory writes take place in Step 1 vs. Baseline for $N = 8193$?
2. **Loop Control Overhead:** What physical processor bottlenecks does loop unrolling (Task 2) help relieve?
3. **Data Dependencies & ILP:** Why does using two accumulators (`sum0`, `sum1` in Task 3) yield a significant speedup over single-accumulator unrolling (Task 2), even though both execute the exact same number of floating-point additions?
4. **Edge Cases:** What would happen if $N = 8193$ and you omitted the cleanup loop from matrix_row_sum4? Explain the effect on program execution and verification.
