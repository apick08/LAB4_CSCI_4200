/*
 * CS:APP Chapter 5 Alignment: Code Optimization & ILP
 * Lab 4: Matrix Row Sum Optimization (Student Template)
 *
 * Compilation Instructions:
 *   gcc -O1 -Wall lab4_matrix.c -o lab4_matrix -lm
 *
 * Usage:
 *   ./lab4_matrix
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>    // For fabs() validation

// Odd N dimension guarantees the cleanup loops run!
#define N 8193       // Matrix dimension (N x N) — 512 MB Matrix
#define RUNS 5       // Number of benchmark runs for averaging
#define EPSILON 1e-4 // Floating-point comparison tolerance

/* =========================================================================
 * BASELINE: Unoptimized Matrix Row Sums
 *
 * Bottleneck: Using 'volatile double *row_sums' models the strict memory
 * aliasing penalty by forcing the compiler to issue a load and store on
 * EVERY inner-loop iteration (Read-Modify-Write cycle).
 * ========================================================================= */
void matrix_row_sum1(double *matrix, volatile double *row_sums, long size) {
    long i, j;

    for (i = 0; i < size; i++) {
        row_sums[i] = 0.0;
        for (j = 0; j < size; j++) {
            // Forced Memory Read-Modify-Write bottleneck (N^2 memory writes)
            row_sums[i] += matrix[i * size + j];
        }
    }
}

/* =========================================================================
 * TODO 1: Register Caching & Pointer Arithmetic
 *
 * Goal: Eliminate memory aliasing writes inside the inner loop.
 * Instructions:
 *   1. Create a local scalar variable 'sum' to accumulate the row sum.
 *      Local scalars are cached in CPU registers (e.g., %xmm0).
 *   2. Use a pointer 'row_ptr' to cache the starting address of row 'i'
 *      to avoid re-evaluating (i * size) on every iteration.
 *   3. Write 'sum' back to row_sums[i] ONCE at the end of each row (1 write per row).
 * ========================================================================= */
void matrix_row_sum2(double *matrix, double *row_sums, long size) {
    // TODO: Implement Step 1 (Register Caching & Pointer Arithmetic)

}

/* =========================================================================
 * TODO 2: 2x1 Loop Unrolling
 *
 * Goal: Reduce loop branch pressure and counter increments.
 * Instructions:
 *   1. Maintain register caching and row pointer from matrix_row_sum2.
 *   2. Unroll the inner loop to process 2 elements per iteration (j += 2).
 *   3. Add a cleanup loop after the unrolled loop to handle any remaining
 *      elements when 'size' is odd (like N = 8193).
 * ========================================================================= */
void matrix_row_sum3(double *matrix, double *row_sums, long size) {
    // TODO: Implement Step 2 (2x1 Loop Unrolling + Cleanup Loop)

}

/* =========================================================================
 * TODO 3: 2x2 Unrolling with Parallel Accumulators
 *
 * Goal: Break sequential data dependencies to exploit Instruction-Level
 *       Parallelism (ILP).
 * Instructions:
 *   1. Use TWO independent local scalar accumulators (e.g., 'sum0' and 'sum1').
 *   2. Accumulate EVEN columns into 'sum0' and ODD columns into 'sum1'.
 *   3. Combine 'sum0' and 'sum1' after the inner loop finishes.
 *   4. Include the cleanup loop for odd matrix sizes.
 * ========================================================================= */
void matrix_row_sum4(double *matrix, double *row_sums, long size) {
    // TODO: Implement Step 3 (2x2 Parallel Accumulators + Cleanup Loop)

}

/* =========================================================================
 * HELPER: High-Resolution Monotonic Timer
 * ========================================================================= */
double get_time_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + (ts.tv_nsec * 1e-9);
}

/* =========================================================================
 * HELPER: Matrix Initialization
 * ========================================================================= */
void init_matrix(double *matrix, long size) {
    for (long i = 0; i < size * size; i++) {
        matrix[i] = (double)(rand() % 100) / 10.0;
    }
}

/* =========================================================================
 * VALIDATION ENGINE: Correctness Checker
 * ========================================================================= */
int verify_correctness(const char *name, double *expected, double *actual, long size) {
    for (long i = 0; i < size; i++) {
        if (fabs(expected[i] - actual[i]) > EPSILON) {
            printf("[FAIL] %-34s | Mismatch at row %ld! (Expected: %.4f, Got: %.4f)\n", 
                   name, i, expected[i], actual[i]);
            return 0; // Validation failed
        }
    }
    printf("[PASS] %-34s | Output matches baseline perfectly.\n", name);
    return 1; // Validation passed
}

/* =========================================================================
 * BENCHMARK RUNNER (Includes DCE Protection for -O1 Compilation)
 * ========================================================================= */
void run_benchmark(const char *name, 
                   void (*func)(double *, double *, long), 
                   double *matrix, 
                   double *row_sums, 
                   long size, 
                   double baseline_time) 
{
    double min_time = 1e9;
    volatile double dummy_checksum = 0.0; // Prevents Dead-Code Elimination (-O1)

    for (int r = 0; r < RUNS; r++) {
        // Clear row_sums array between runs
        for (long k = 0; k < size; k++) row_sums[k] = 0.0;

        double start = get_time_sec();
        func(matrix, row_sums, size);
        double elapsed = get_time_sec() - start;

        // Force compiler to consume function output so it won't optimize away execution
        dummy_checksum += row_sums[0] + row_sums[size - 1];

        if (elapsed < min_time) {
            min_time = elapsed;
        }
    }

    double total_elements = (double)size * (double)size;
    // Estimated CPE assuming nominal 3.0 GHz CPU clock
    double estimated_cpe = (min_time * 3.0e9) / total_elements; 
    double speedup = (baseline_time > 0) ? (baseline_time / min_time) : 1.0;

    printf("%-34s | %8.4f s | %8.2f CPE | %6.2fx\n", 
           name, min_time, estimated_cpe, speedup);
}

int main(void) {
    printf("=======================================================================\n");
    printf("CS:APP Lab 4 — Matrix Row Sum Optimization Benchmark (-O1)\n");
    printf("Matrix Dimension: %d x %d (%ld elements)\n", N, N, (long)N * N);
    printf("=======================================================================\n");

    double *matrix = (double *)malloc(sizeof(double) * N * N);
    double *baseline_sums = (double *)malloc(sizeof(double) * N);
    double *test_sums = (double *)malloc(sizeof(double) * N);

    if (!matrix || !baseline_sums || !test_sums) {
        fprintf(stderr, "Error: Memory allocation failed! Ensure N is reasonable.\n");
        return 1;
    }

    init_matrix(matrix, N);

    // 1. Compute Reference Baseline
    printf("Computing baseline reference values...\n");
    double start_base = get_time_sec();
    matrix_row_sum1(matrix, baseline_sums, N);
    double baseline_time = get_time_sec() - start_base;

    // 2. Validate Correctness First
    printf("-----------------------------------------------------------------------\n");
    printf("CORRECTNESS VERIFICATION (Testing odd N = %d for cleanup loop correctness):\n", N);
    printf("-----------------------------------------------------------------------\n");

    matrix_row_sum2(matrix, test_sums, N);
    int p2 = verify_correctness("2. Register Caching (TODO 1)", baseline_sums, test_sums, N);

    matrix_row_sum3(matrix, test_sums, N);
    int p3 = verify_correctness("3. 2x1 Loop Unrolling (TODO 2)", baseline_sums, test_sums, N);

    matrix_row_sum4(matrix, test_sums, N);
    int p4 = verify_correctness("4. 2x2 Accumulators (TODO 3)", baseline_sums, test_sums, N);

    if (!p2 || !p3 || !p4) {
        printf("\n[WARNING] Fix correctness errors before relying on benchmark numbers!\n");
    }

    // 3. Performance Benchmarks
    printf("-----------------------------------------------------------------------\n");
    printf("PERFORMANCE BENCHMARKS (-O1 Optimization):\n");
    printf("-----------------------------------------------------------------------\n");
    printf("%-34s | %10s | %10s | %8s\n", "Implementation", "Time (s)", "Est. CPE", "Speedup");
    printf("-----------------------------------------------------------------------\n");

    run_benchmark("1. Baseline (Memory Aliasing)", (void (*)(double*, double*, long))matrix_row_sum1, matrix, test_sums, N, baseline_time);
    run_benchmark("2. Register Caching (TODO 1)",  matrix_row_sum2, matrix, test_sums, N, baseline_time);
    run_benchmark("3. 2x1 Loop Unrolling (TODO 2)", matrix_row_sum3, matrix, test_sums, N, baseline_time);
    run_benchmark("4. 2x2 Accumulators (TODO 3)",   matrix_row_sum4, matrix, test_sums, N, baseline_time);

    printf("=======================================================================\n");

    free(matrix);
    free(baseline_sums);
    free(test_sums);
    return 0;
}
