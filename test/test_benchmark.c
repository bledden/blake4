/*
 * BLAKE4 Performance Benchmark
 *
 * Measures throughput for various input sizes and compares
 * different hashing modes.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include "blake4.h"
#include "blake4_simd.h"

#define WARMUP_ITERS 100
#define MIN_TIME_NS 500000000  /* 500ms minimum per benchmark */

/* Get current time in nanoseconds */
static uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

/* Benchmark one-shot hashing */
static void bench_oneshot(size_t size, int show_header) {
    uint8_t *data = malloc(size);
    uint8_t hash[BLAKE4_OUT_LEN];

    if (!data) {
        fprintf(stderr, "Failed to allocate %zu bytes\n", size);
        return;
    }

    /* Fill with pattern */
    for (size_t i = 0; i < size; i++) {
        data[i] = (uint8_t)(i & 0xFF);
    }

    /* Warmup */
    for (int i = 0; i < WARMUP_ITERS; i++) {
        blake4_hash(data, size, hash);
    }

    /* Benchmark */
    uint64_t start = get_time_ns();
    uint64_t iterations = 0;

    while (get_time_ns() - start < MIN_TIME_NS) {
        for (int i = 0; i < 100; i++) {
            blake4_hash(data, size, hash);
            iterations++;
        }
    }

    uint64_t elapsed = get_time_ns() - start;
    double seconds = elapsed / 1e9;
    double bytes_per_sec = (iterations * size) / seconds;
    double mb_per_sec = bytes_per_sec / (1024 * 1024);
    double gb_per_sec = bytes_per_sec / (1024 * 1024 * 1024);
    double ns_per_hash = (double)elapsed / iterations;

    if (show_header) {
        printf("%-12s %12s %12s %12s %12s\n",
               "Size", "Iterations", "Time (s)", "MB/s", "ns/hash");
        printf("%-12s %12s %12s %12s %12s\n",
               "----", "----------", "--------", "----", "-------");
    }

    if (size >= 1024 * 1024) {
        printf("%-12s %12llu %12.3f %12.2f %12.1f\n",
               size >= 1024*1024 ? (size == 1024*1024 ? "1MB" : "10MB") :
               (size >= 1024 ? (size == 1024 ? "1KB" :
                size == 4096 ? "4KB" :
                size == 16384 ? "16KB" :
                size == 65536 ? "64KB" : "??KB") : "??"),
               (unsigned long long)iterations, seconds, mb_per_sec, ns_per_hash);
    } else {
        char size_str[32];
        if (size >= 1024) {
            snprintf(size_str, sizeof(size_str), "%zuKB", size / 1024);
        } else {
            snprintf(size_str, sizeof(size_str), "%zuB", size);
        }
        printf("%-12s %12llu %12.3f %12.2f %12.1f\n",
               size_str, (unsigned long long)iterations, seconds, mb_per_sec, ns_per_hash);
    }

    free(data);
}

/* Benchmark incremental hashing */
static void bench_incremental(size_t total_size, size_t chunk_size) {
    uint8_t *data = malloc(total_size);
    uint8_t hash[BLAKE4_OUT_LEN];

    if (!data) {
        fprintf(stderr, "Failed to allocate %zu bytes\n", total_size);
        return;
    }

    for (size_t i = 0; i < total_size; i++) {
        data[i] = (uint8_t)(i & 0xFF);
    }

    /* Warmup */
    for (int i = 0; i < WARMUP_ITERS; i++) {
        blake4_hasher h;
        blake4_hasher_init(&h);
        for (size_t off = 0; off < total_size; off += chunk_size) {
            size_t len = (off + chunk_size > total_size) ? total_size - off : chunk_size;
            blake4_hasher_update(&h, data + off, len);
        }
        blake4_hasher_finalize(&h, hash, BLAKE4_OUT_LEN);
    }

    /* Benchmark */
    uint64_t start = get_time_ns();
    uint64_t iterations = 0;

    while (get_time_ns() - start < MIN_TIME_NS) {
        for (int i = 0; i < 100; i++) {
            blake4_hasher h;
            blake4_hasher_init(&h);
            for (size_t off = 0; off < total_size; off += chunk_size) {
                size_t len = (off + chunk_size > total_size) ? total_size - off : chunk_size;
                blake4_hasher_update(&h, data + off, len);
            }
            blake4_hasher_finalize(&h, hash, BLAKE4_OUT_LEN);
            iterations++;
        }
    }

    uint64_t elapsed = get_time_ns() - start;
    double seconds = elapsed / 1e9;
    double bytes_per_sec = (iterations * total_size) / seconds;
    double mb_per_sec = bytes_per_sec / (1024 * 1024);

    printf("  %zu byte chunks: %.2f MB/s\n", chunk_size, mb_per_sec);

    free(data);
}

/* Benchmark keyed hashing */
static void bench_keyed(size_t size) {
    uint8_t *data = malloc(size);
    uint8_t key[BLAKE4_KEY_LEN];
    uint8_t hash[BLAKE4_OUT_LEN];

    if (!data) return;

    memset(key, 0x42, BLAKE4_KEY_LEN);
    for (size_t i = 0; i < size; i++) {
        data[i] = (uint8_t)(i & 0xFF);
    }

    /* Warmup */
    for (int i = 0; i < WARMUP_ITERS; i++) {
        blake4_hash_keyed(key, data, size, hash);
    }

    /* Benchmark */
    uint64_t start = get_time_ns();
    uint64_t iterations = 0;

    while (get_time_ns() - start < MIN_TIME_NS) {
        for (int i = 0; i < 100; i++) {
            blake4_hash_keyed(key, data, size, hash);
            iterations++;
        }
    }

    uint64_t elapsed = get_time_ns() - start;
    double seconds = elapsed / 1e9;
    double bytes_per_sec = (iterations * size) / seconds;
    double mb_per_sec = bytes_per_sec / (1024 * 1024);

    printf("  %zu bytes: %.2f MB/s\n", size, mb_per_sec);

    free(data);
}

/* Benchmark different output sizes */
static void bench_output_sizes(size_t input_size) {
    uint8_t *data = malloc(input_size);
    uint8_t hash[BLAKE4_OUT_LEN];

    if (!data) return;

    for (size_t i = 0; i < input_size; i++) {
        data[i] = (uint8_t)(i & 0xFF);
    }

    size_t output_sizes[] = {32, 48, 64};
    const char *names[] = {"BLAKE4-256", "BLAKE4-384", "BLAKE4-512"};

    for (int s = 0; s < 3; s++) {
        /* Warmup */
        for (int i = 0; i < WARMUP_ITERS; i++) {
            blake4_hasher h;
            blake4_hasher_init(&h);
            blake4_hasher_update(&h, data, input_size);
            blake4_hasher_finalize(&h, hash, output_sizes[s]);
        }

        /* Benchmark */
        uint64_t start = get_time_ns();
        uint64_t iterations = 0;

        while (get_time_ns() - start < MIN_TIME_NS) {
            for (int i = 0; i < 100; i++) {
                blake4_hasher h;
                blake4_hasher_init(&h);
                blake4_hasher_update(&h, data, input_size);
                blake4_hasher_finalize(&h, hash, output_sizes[s]);
                iterations++;
            }
        }

        uint64_t elapsed = get_time_ns() - start;
        double seconds = elapsed / 1e9;
        double bytes_per_sec = (iterations * input_size) / seconds;
        double mb_per_sec = bytes_per_sec / (1024 * 1024);

        printf("  %s: %.2f MB/s\n", names[s], mb_per_sec);
    }

    free(data);
}

/* Benchmark small message latency */
static void bench_latency(void) {
    uint8_t data[64];
    uint8_t hash[BLAKE4_OUT_LEN];

    memset(data, 0x42, sizeof(data));

    printf("\nSmall Message Latency:\n");
    printf("%-12s %12s\n", "Size", "Latency (ns)");
    printf("%-12s %12s\n", "----", "------------");

    size_t sizes[] = {1, 8, 16, 32, 64};
    for (int s = 0; s < 5; s++) {
        /* Warmup */
        for (int i = 0; i < 10000; i++) {
            blake4_hash(data, sizes[s], hash);
        }

        /* Benchmark with many iterations for accuracy */
        uint64_t start = get_time_ns();
        int iterations = 1000000;

        for (int i = 0; i < iterations; i++) {
            blake4_hash(data, sizes[s], hash);
        }

        uint64_t elapsed = get_time_ns() - start;
        double ns_per_hash = (double)elapsed / iterations;

        printf("%-12zu %12.1f\n", sizes[s], ns_per_hash);
    }
}

int main(void) {
    printf("=== BLAKE4 Performance Benchmark ===\n");
    printf("SIMD: %s\n\n", blake4_simd_name());

    printf("1. One-shot Hashing Throughput\n");
    bench_oneshot(64, 1);
    bench_oneshot(256, 0);
    bench_oneshot(1024, 0);
    bench_oneshot(4096, 0);
    bench_oneshot(16384, 0);
    bench_oneshot(65536, 0);
    bench_oneshot(1024 * 1024, 0);

    printf("\n2. Incremental Hashing (1MB total)\n");
    bench_incremental(1024 * 1024, 64);
    bench_incremental(1024 * 1024, 256);
    bench_incremental(1024 * 1024, 1024);
    bench_incremental(1024 * 1024, 4096);
    bench_incremental(1024 * 1024, 16384);

    printf("\n3. Keyed Hashing (MAC) Throughput\n");
    bench_keyed(64);
    bench_keyed(1024);
    bench_keyed(16384);

    printf("\n4. Output Size Comparison (64KB input)\n");
    bench_output_sizes(65536);

    bench_latency();

    printf("\n=== Benchmark Complete ===\n");

    return 0;
}
