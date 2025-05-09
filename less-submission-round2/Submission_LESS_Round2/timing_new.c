#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>

#define N 4
#define N_REPS 21

// Secret permutation μ
uint32_t mu_tilde[N] = {1, 3, 0, 2};
uint8_t is_pivot_column[N] = {1, 1, 1, 1};

// Use inline assembly to get TSC (x86-only)
static __inline__ uint64_t rdtsc() {
    unsigned int lo, hi;
    __asm__ __volatile__ (
        "cpuid\n\t"            // Serialize before rdtsc
        "rdtsc\n\t"
        : "=a"(lo), "=d"(hi)
        : "a"(0)
        : "%ebx", "%ecx");
    return ((uint64_t)hi << 32) | lo;
}

// Comparator for qsort
int cmp_uint64(const void *a, const void *b) {
    uint64_t ua = *(const uint64_t *)a;
    uint64_t ub = *(const uint64_t *)b;
    return (ua > ub) - (ua < ub);
}

int main() {
    uint64_t timings[N] = {0};     // Median timings
    uint64_t reps[N_REPS];         // Per-col_idx repetition timings
    uint32_t pi_tilde[N] = {0};    // Actual match row_idx
    uint32_t piv_idx = 0;

    usleep(50000); // Settle noise

    for (uint32_t col_idx = 0; col_idx < N; col_idx++) {
        volatile uint32_t dummy = 0;
        uint32_t row_idx = UINT32_MAX;

        for (int r = 0; r < N_REPS; r++) {
            usleep(5000); // Reduce burst noise

            uint64_t t_start = rdtsc();

            for (int repeat = 0; repeat < 10000; repeat++) {
                for (uint32_t t = 0; t < N; t++) {
                    if (mu_tilde[t] == col_idx) {
                        dummy += t;
                        if (r == 0) row_idx = t;
                        break;
                    }
                }
            }

            uint64_t t_end = rdtsc();
            reps[r] = t_end - t_start;
        }

        qsort(reps, N_REPS, sizeof(uint64_t), cmp_uint64);
        timings[col_idx] = reps[N_REPS / 2];

        if (is_pivot_column[col_idx]) {
            pi_tilde[piv_idx++] = row_idx;
        }

        printf("col_idx %u → row_idx %u (median timing = %lu cycles)\n",
               col_idx, row_idx, timings[col_idx]);
    }

    // Rank col_idx by timing: smaller = faster = earlier match = lower t
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

    // Reconstruct mu_tilde[t] = col_idx
    uint32_t mu_tilde_pi[N];
    for (int col_idx = 0; col_idx < N; col_idx++) {
        mu_tilde_pi[inferred_pi[col_idx]] = col_idx;
    }

    printf("\noriginal permutation part:\n");
    for (int i = 0; i < N; i++) {
        printf("%d ", mu_tilde[i]);
    }

    printf("\nfinal mu_tilde obtained through timing:\n");
    for (int i = 0; i < N; i++) {
        printf("%d ", mu_tilde_pi[i]);
    }

    printf("\n");
    return 0;
}
