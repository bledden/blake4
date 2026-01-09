/*
 * BLAKE4 Benchmark Tool
 *
 * Measures throughput of BLAKE4 hashing at various input sizes.
 * Reports MB/s and compares serial vs parallel performance.
 *
 * Build:
 *   cmake --build build --target benchmark
 *
 * Usage:
 *   ./benchmark [iterations]
 *
 * This is free and unencumbered software released into the public domain.
 */

#include "blake4.h"
#include "blake4_dispatch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
    #include <windows.h>
    static double get_time_sec(void) {
        LARGE_INTEGER freq, count;
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&count);
        return (double)count.QuadPart / (double)freq.QuadPart;
    }
#else
    #include <sys/time.h>
    static double get_time_sec(void) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return (double)tv.tv_sec + (double)tv.tv_usec * 1e-6;
    }
#endif

/* Benchmark configuration */
typedef struct {
    const char *name;
    size_t size;
    int iterations;
} bench_config;

/* Run a single benchmark */
static double bench_serial(const uint8_t *data, size_t size, int iterations) {
    uint8_t hash[BLAKE4_OUT_LEN];
    double start = get_time_sec();

    for (int i = 0; i < iterations; i++) {
        blake4_hash(data, size, hash);
    }

    double end = get_time_sec();
    return (end - start);
}

static double bench_parallel(const uint8_t *data, size_t size, int iterations,
                            unsigned threads) {
    uint8_t hash[BLAKE4_OUT_LEN];
    double start = get_time_sec();

    for (int i = 0; i < iterations; i++) {
        blake4_parallel_hash(data, size, hash, threads);
    }

    double end = get_time_sec();
    return (end - start);
}

static double bench_incremental(const uint8_t *data, size_t size, int iterations) {
    uint8_t hash[BLAKE4_OUT_LEN];
    blake4_hasher h;
    double start = get_time_sec();

    for (int i = 0; i < iterations; i++) {
        blake4_hasher_init(&h);
        blake4_hasher_update(&h, data, size);
        blake4_hasher_finalize(&h, hash, BLAKE4_OUT_LEN);
    }

    double end = get_time_sec();
    return (end - start);
}

/* Print throughput in MB/s */
static void print_throughput(const char *label, size_t size, int iterations,
                            double elapsed) {
    double total_mb = ((double)size * iterations) / (1024.0 * 1024.0);
    double mbps = total_mb / elapsed;
    printf("  %-25s %8.2f MB/s  (%.3f s)\n", label, mbps, elapsed);
}

/* Size formatting */
static void format_size(size_t size, char *buf, size_t buf_len) {
    if (size >= 1024 * 1024) {
        snprintf(buf, buf_len, "%zu MB", size / (1024 * 1024));
    } else if (size >= 1024) {
        snprintf(buf, buf_len, "%zu KB", size / 1024);
    } else {
        snprintf(buf, buf_len, "%zu B", size);
    }
}

int main(int argc, char **argv) {
    int base_iterations = 100;

    if (argc > 1) {
        base_iterations = atoi(argv[1]);
        if (base_iterations < 1) base_iterations = 100;
    }

    printf("=== BLAKE4 Benchmark ===\n\n");
    printf("Implementation: %s\n", blake4_dispatch_impl_name());
    printf("Parallel support: %s\n", blake4_parallel_available() ? "yes" : "no");
    printf("Base iterations: %d\n\n", base_iterations);

    /* Benchmark configurations */
    bench_config configs[] = {
        {"64 bytes",    64,                 base_iterations * 10000},
        {"256 bytes",   256,                base_iterations * 5000},
        {"1 KB",        1024,               base_iterations * 2000},
        {"4 KB",        4096,               base_iterations * 500},
        {"16 KB",       16384,              base_iterations * 200},
        {"64 KB",       65536,              base_iterations * 50},
        {"256 KB",      262144,             base_iterations * 20},
        {"1 MB",        1024 * 1024,        base_iterations * 5},
        {"4 MB",        4 * 1024 * 1024,    base_iterations * 2},
        {"16 MB",       16 * 1024 * 1024,   base_iterations},
        {NULL, 0, 0}
    };

    /* Allocate maximum needed buffer */
    size_t max_size = 16 * 1024 * 1024;
    uint8_t *data = malloc(max_size);
    if (!data) {
        fprintf(stderr, "Failed to allocate %zu bytes\n", max_size);
        return 1;
    }

    /* Fill with pattern */
    for (size_t i = 0; i < max_size; i++) {
        data[i] = (uint8_t)(i % 256);
    }

    /* Warmup */
    printf("Warming up...\n");
    uint8_t warmup_hash[BLAKE4_OUT_LEN];
    for (int i = 0; i < 1000; i++) {
        blake4_hash(data, 4096, warmup_hash);
    }

    printf("\n--- Throughput Benchmarks ---\n\n");

    for (int c = 0; configs[c].name != NULL; c++) {
        bench_config *cfg = &configs[c];
        char size_str[32];
        format_size(cfg->size, size_str, sizeof(size_str));

        printf("%s (%d iterations):\n", cfg->name, cfg->iterations);

        /* Serial one-shot */
        double serial_time = bench_serial(data, cfg->size, cfg->iterations);
        print_throughput("Serial (one-shot)", cfg->size, cfg->iterations, serial_time);

        /* Incremental */
        double incr_time = bench_incremental(data, cfg->size, cfg->iterations);
        print_throughput("Incremental", cfg->size, cfg->iterations, incr_time);

        /* Parallel (only for larger inputs) */
        if (blake4_parallel_available() && cfg->size >= 1024 * 1024) {
            double par2_time = bench_parallel(data, cfg->size, cfg->iterations, 2);
            print_throughput("Parallel (2 threads)", cfg->size, cfg->iterations, par2_time);

            double par4_time = bench_parallel(data, cfg->size, cfg->iterations, 4);
            print_throughput("Parallel (4 threads)", cfg->size, cfg->iterations, par4_time);

            double par8_time = bench_parallel(data, cfg->size, cfg->iterations, 8);
            print_throughput("Parallel (8 threads)", cfg->size, cfg->iterations, par8_time);

            double par_auto_time = bench_parallel(data, cfg->size, cfg->iterations, 0);
            print_throughput("Parallel (auto)", cfg->size, cfg->iterations, par_auto_time);
        }

        printf("\n");
    }

    /* Summary statistics */
    printf("--- Summary ---\n\n");

    /* 1KB reference */
    double ref_time = bench_serial(data, 1024, 10000);
    double ref_mbps = (1024.0 * 10000 / (1024.0 * 1024.0)) / ref_time;
    printf("Reference (1KB, 10K iterations): %.2f MB/s\n", ref_mbps);

    /* Large block reference */
    double large_time = bench_serial(data, 1024 * 1024, 100);
    double large_mbps = (1024.0 * 1024.0 * 100 / (1024.0 * 1024.0)) / large_time;
    printf("Large block (1MB, 100 iterations): %.2f MB/s\n", large_mbps);

    if (blake4_parallel_available()) {
        double par_time = bench_parallel(data, 16 * 1024 * 1024, 10, 0);
        double par_mbps = (16.0 * 1024.0 * 1024.0 * 10 / (1024.0 * 1024.0)) / par_time;
        printf("Parallel peak (16MB, auto threads): %.2f MB/s\n", par_mbps);
    }

    free(data);

    printf("\nBenchmark complete.\n");
    return 0;
}
