/*
 * BLAKE4 vs BLAKE3 Performance Comparison
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include "blake4.h"
#include <blake3.h>

static uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static double benchmark_blake4(const uint8_t *data, size_t len, int iterations) {
    uint8_t hash[64];
    uint64_t start = get_time_ns();

    for (int i = 0; i < iterations; i++) {
        blake4_hash(data, len, hash);
    }

    uint64_t elapsed = get_time_ns() - start;
    double total_bytes = (double)len * iterations;
    double seconds = elapsed / 1e9;
    return total_bytes / seconds / 1e6;  /* MB/s */
}

static double benchmark_blake3(const uint8_t *data, size_t len, int iterations) {
    uint8_t hash[32];
    uint64_t start = get_time_ns();

    for (int i = 0; i < iterations; i++) {
        blake3_hasher hasher;
        blake3_hasher_init(&hasher);
        blake3_hasher_update(&hasher, data, len);
        blake3_hasher_finalize(&hasher, hash, 32);
    }

    uint64_t elapsed = get_time_ns() - start;
    double total_bytes = (double)len * iterations;
    double seconds = elapsed / 1e9;
    return total_bytes / seconds / 1e6;  /* MB/s */
}

int main(void) {
    printf("=== BLAKE4 vs BLAKE3 Performance Comparison ===\n\n");
    printf("BLAKE4: 512-bit state, 10 rounds, 64-byte output\n");
    printf("BLAKE3: 256-bit state, 7 rounds, 32-byte output\n\n");

    /* Test sizes */
    size_t sizes[] = {64, 256, 1024, 4096, 16384, 65536, 262144, 1048576};
    const char *labels[] = {"64B", "256B", "1KB", "4KB", "16KB", "64KB", "256KB", "1MB"};
    int num_sizes = sizeof(sizes) / sizeof(sizes[0]);

    /* Allocate largest buffer */
    uint8_t *data = malloc(sizes[num_sizes - 1]);
    if (!data) {
        fprintf(stderr, "Failed to allocate memory\n");
        return 1;
    }

    /* Fill with pseudo-random data */
    for (size_t i = 0; i < sizes[num_sizes - 1]; i++) {
        data[i] = (uint8_t)(i * 17 + 31);
    }

    printf("%-10s %12s %12s %10s\n", "Size", "BLAKE4 MB/s", "BLAKE3 MB/s", "Ratio");
    printf("%-10s %12s %12s %10s\n", "----", "-----------", "-----------", "-----");

    for (int i = 0; i < num_sizes; i++) {
        size_t len = sizes[i];

        /* Calculate iterations for ~500ms of testing */
        int iterations = (int)(500000000.0 / (len * 100));
        if (iterations < 10) iterations = 10;
        if (iterations > 100000) iterations = 100000;

        /* Warmup */
        uint8_t warmup[64];
        for (int w = 0; w < 100; w++) {
            blake4_hash(data, len, warmup);
        }

        double blake4_speed = benchmark_blake4(data, len, iterations);
        double blake3_speed = benchmark_blake3(data, len, iterations);
        double ratio = blake3_speed / blake4_speed;

        printf("%-10s %12.2f %12.2f %9.2fx\n", labels[i], blake4_speed, blake3_speed, ratio);
    }

    printf("\n");
    printf("Note: BLAKE3 is expected to be faster due to:\n");
    printf("  - Smaller state (256-bit vs 512-bit)\n");
    printf("  - Fewer rounds (7 vs 10)\n");
    printf("  - 32-bit words vs 64-bit words\n");
    printf("  - Highly optimized reference implementation\n");
    printf("\n");
    printf("BLAKE4 provides higher security margins:\n");
    printf("  - 256-bit post-quantum preimage vs 128-bit\n");
    printf("  - ~170-bit quantum collision vs ~85-bit\n");

    free(data);
    return 0;
}
