#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <limits.h>

#define N 4

// Simulate the secret permutation μ: maps logical t to permuted row μ(t)
uint32_t mu_tilde[N] = {1, 3, 0, 2}; 

// Simulated pivot column info
uint8_t is_pivot_column[N] = {1, 1, 1, 1};  // All columns are pivot columns

// Simple timer in nanoseconds
uint64_t timestamp_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

int main() {
    uint64_t timings[N] = {0};         // Store time taken per col_idx
    uint32_t pi_tilde[N] = {0};        // The output permutation
    uint32_t piv_idx = 0;
    usleep(5000);
    for (uint32_t col_idx = 0; col_idx < N; col_idx++) {
        uint32_t row_idx = 0;
        volatile uint32_t dummy = 0;
        // --- Start timing ---
        uint64_t t_start = timestamp_ns();

        // Search for μ⁻¹(col_idx)
        for (int repeat = 0; repeat < 100000; repeat++) {
            for (uint32_t t = 0; t < N; t++) {
                if (mu_tilde[t] == col_idx) {
                    dummy += t;
                    row_idx = t;
                    break;
                }
            }
        }
        // --- End timing ---
        uint64_t t_end = timestamp_ns();
        timings[col_idx] = t_end - t_start;

        // If it's a pivot column, record in π̃
        if (is_pivot_column[col_idx]) {
            pi_tilde[piv_idx++] = row_idx;
        }

        printf("col_idx %u → number of comparisons %u (timing = %lu ns)\n", col_idx, row_idx, timings[col_idx]);
    }

    // Inference phase: reconstruct π̃ by ranking timings (lower time → earlier match → smaller t)
    // printf("\nInferred π̃[col_idx] (from timings):\n");
    uint32_t inferred_pi[N] = {UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX};
    uint8_t used[N] = {0};

    for (uint32_t rank = 0; rank < N; rank++) {
        uint64_t best_time = ULLONG_MAX;
        uint32_t best_col = UINT_MAX;
        for (uint32_t col = 0; col < N; col++) {
            if (!used[col] && timings[col] < best_time) {
                best_time = timings[col];
                best_col = col;
            }
        }
        inferred_pi[best_col] = rank;
        used[best_col] = 1;
    }

    // for (uint32_t col = 0; col < N; col++) {
    //     printf("  col %u → inferred π̃ = %u\n", col, inferred_pi[col]);
    // }
    uint32_t mu_tilde_pi[N] = {0};
    for(int i=0;i<N;i++){
        mu_tilde_pi[inferred_pi[i]] = i;
    }
    printf("original permutation part: \n");
    for (int i = 0; i < N; i++)
    {
        printf("%d ", mu_tilde[i]);
    }
    printf("\nfinal mu_tilde obtained through timing: \n");
    for(int i=0;i<N;i++){
        printf("%d ", mu_tilde_pi[i]);
    }
    return 0;
}
