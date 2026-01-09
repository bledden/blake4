/*
 * BLAKE4 Cryptographic Benchmark Suite
 *
 * Measures performance metrics that matter to cryptographers:
 * - Cycles per byte (cpb) at various message lengths
 * - Latency for small messages
 * - Long message throughput
 * - Comparison-ready output format
 *
 * Build:
 *   cc -O3 -o crypto_bench crypto_bench.c -I../include -L../build -lblake4
 *
 * Or via CMake:
 *   cmake --build build --target crypto_bench
 *
 * This is free and unencumbered software released into the public domain.
 */

#include "blake4.h"
#include "blake4_dispatch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ============== Cycle Counter ============== */

#if defined(__x86_64__) || defined(_M_X64)
static inline uint64_t rdtsc(void) {
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}
#define HAVE_RDTSC 1
#elif defined(__aarch64__)
static inline uint64_t rdtsc(void) {
    uint64_t val;
    __asm__ volatile ("mrs %0, cntvct_el0" : "=r"(val));
    return val;
}
#define HAVE_RDTSC 1
#else
#define HAVE_RDTSC 0
static inline uint64_t rdtsc(void) { return 0; }
#endif

/* High-resolution timer fallback */
#if defined(_WIN32)
    #include <windows.h>
    static double get_time_ns(void) {
        LARGE_INTEGER freq, count;
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&count);
        return (double)count.QuadPart * 1e9 / (double)freq.QuadPart;
    }
#else
    #include <time.h>
    static double get_time_ns(void) {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
    }
#endif

/* ============== Benchmark Utilities ============== */

#define WARMUP_ITERATIONS 100
#define MIN_ITERATIONS 1000
#define MIN_TIME_NS 50000000.0  /* 50ms minimum measurement time */

/* Prevent dead code elimination */
static volatile uint8_t sink;

static void consume(const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        sink ^= data[i];
    }
}

/* ============== Core Benchmarks ============== */

typedef struct {
    double cycles_per_byte;
    double ns_per_call;
    double throughput_mbps;
    uint64_t iterations;
} bench_result;

static bench_result bench_hash(size_t msg_len) {
    bench_result result = {0};
    uint8_t *msg = malloc(msg_len > 0 ? msg_len : 1);
    uint8_t hash[BLAKE4_OUT_LEN];

    if (!msg) {
        fprintf(stderr, "Failed to allocate %zu bytes\n", msg_len);
        return result;
    }

    /* Fill with pattern */
    for (size_t i = 0; i < msg_len; i++) {
        msg[i] = (uint8_t)(i & 0xFF);
    }

    /* Warmup */
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        blake4_hash(msg, msg_len, hash);
    }
    consume(hash, BLAKE4_OUT_LEN);

    /* Determine iteration count */
    uint64_t iterations = MIN_ITERATIONS;
    double start_ns = get_time_ns();
    for (uint64_t i = 0; i < iterations; i++) {
        blake4_hash(msg, msg_len, hash);
    }
    double elapsed_ns = get_time_ns() - start_ns;

    /* Scale iterations for accurate measurement */
    if (elapsed_ns < MIN_TIME_NS) {
        iterations = (uint64_t)(iterations * MIN_TIME_NS / elapsed_ns) + 1;
    }

    /* Actual measurement */
    uint64_t start_cycles = rdtsc();
    start_ns = get_time_ns();

    for (uint64_t i = 0; i < iterations; i++) {
        blake4_hash(msg, msg_len, hash);
    }

    uint64_t end_cycles = rdtsc();
    double end_ns = get_time_ns();
    consume(hash, BLAKE4_OUT_LEN);

    elapsed_ns = end_ns - start_ns;
    uint64_t elapsed_cycles = end_cycles - start_cycles;

    /* Calculate results */
    result.iterations = iterations;
    result.ns_per_call = elapsed_ns / iterations;
    result.throughput_mbps = (msg_len * iterations / (1024.0 * 1024.0)) / (elapsed_ns / 1e9);

    if (HAVE_RDTSC && msg_len > 0) {
        result.cycles_per_byte = (double)elapsed_cycles / (iterations * msg_len);
    }

    free(msg);
    return result;
}

static bench_result bench_incremental(size_t msg_len, size_t chunk_size) {
    bench_result result = {0};
    uint8_t *msg = malloc(msg_len > 0 ? msg_len : 1);
    uint8_t hash[BLAKE4_OUT_LEN];
    blake4_hasher h;

    if (!msg) return result;

    for (size_t i = 0; i < msg_len; i++) {
        msg[i] = (uint8_t)(i & 0xFF);
    }

    /* Warmup */
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        blake4_hasher_init(&h);
        for (size_t off = 0; off < msg_len; off += chunk_size) {
            size_t len = (off + chunk_size <= msg_len) ? chunk_size : (msg_len - off);
            blake4_hasher_update(&h, msg + off, len);
        }
        blake4_hasher_finalize(&h, hash, BLAKE4_OUT_LEN);
    }
    consume(hash, BLAKE4_OUT_LEN);

    uint64_t iterations = MIN_ITERATIONS / 10;  /* Incremental is slower */

    uint64_t start_cycles = rdtsc();
    double start_ns = get_time_ns();

    for (uint64_t i = 0; i < iterations; i++) {
        blake4_hasher_init(&h);
        for (size_t off = 0; off < msg_len; off += chunk_size) {
            size_t len = (off + chunk_size <= msg_len) ? chunk_size : (msg_len - off);
            blake4_hasher_update(&h, msg + off, len);
        }
        blake4_hasher_finalize(&h, hash, BLAKE4_OUT_LEN);
    }

    uint64_t end_cycles = rdtsc();
    double end_ns = get_time_ns();
    consume(hash, BLAKE4_OUT_LEN);

    double elapsed_ns = end_ns - start_ns;
    uint64_t elapsed_cycles = end_cycles - start_cycles;

    result.iterations = iterations;
    result.ns_per_call = elapsed_ns / iterations;
    result.throughput_mbps = (msg_len * iterations / (1024.0 * 1024.0)) / (elapsed_ns / 1e9);

    if (HAVE_RDTSC && msg_len > 0) {
        result.cycles_per_byte = (double)elapsed_cycles / (iterations * msg_len);
    }

    free(msg);
    return result;
}

/* ============== Output Formatting ============== */

static void print_header(void) {
    printf("╔══════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                     BLAKE4 Cryptographic Benchmark Suite                     ║\n");
    printf("╚══════════════════════════════════════════════════════════════════════════════╝\n\n");

    printf("Implementation: %s\n", blake4_dispatch_impl_name());
    printf("Output size:    %d bytes (%d bits)\n", BLAKE4_OUT_LEN, BLAKE4_OUT_LEN * 8);
    printf("Block size:     %d bytes\n", BLAKE4_BLOCK_LEN);
    printf("Chunk size:     %d bytes\n", BLAKE4_CHUNK_LEN);

#if HAVE_RDTSC
    printf("Cycle counter:  Available\n");
#else
    printf("Cycle counter:  Not available (using wall clock)\n");
#endif
    printf("\n");
}

static void print_cpb_table(void) {
    printf("┌─────────────────────────────────────────────────────────────────────────────┐\n");
    printf("│                          Cycles Per Byte (cpb)                              │\n");
    printf("├──────────────┬───────────┬────────────────┬──────────────────┬──────────────┤\n");
    printf("│ Message Size │    cpb    │  Latency (ns)  │ Throughput MB/s  │  Iterations  │\n");
    printf("├──────────────┼───────────┼────────────────┼──────────────────┼──────────────┤\n");

    /* Test various message sizes - these are standard crypto benchmark sizes */
    size_t sizes[] = {
        0,      /* Empty message */
        1,      /* Single byte */
        8,      /* Tiny */
        16,     /* Very small */
        32,     /* Small */
        64,     /* One block (64 bytes is typical) */
        128,    /* BLAKE4 block size */
        256,    /* */
        512,    /* */
        1024,   /* 1 KB */
        2048,   /* BLAKE4 chunk size */
        4096,   /* 4 KB - typical page size */
        8192,   /* 8 KB */
        16384,  /* 16 KB */
        65536,  /* 64 KB */
        1048576 /* 1 MB */
    };

    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        bench_result r = bench_hash(sizes[i]);

        char size_str[16];
        if (sizes[i] == 0) {
            snprintf(size_str, sizeof(size_str), "empty");
        } else if (sizes[i] >= 1048576) {
            snprintf(size_str, sizeof(size_str), "%zu MB", sizes[i] / 1048576);
        } else if (sizes[i] >= 1024) {
            snprintf(size_str, sizeof(size_str), "%zu KB", sizes[i] / 1024);
        } else {
            snprintf(size_str, sizeof(size_str), "%zu B", sizes[i]);
        }

        if (HAVE_RDTSC && sizes[i] > 0) {
            printf("│ %12s │ %9.2f │ %14.1f │ %16.2f │ %12llu │\n",
                   size_str, r.cycles_per_byte, r.ns_per_call,
                   r.throughput_mbps, (unsigned long long)r.iterations);
        } else {
            printf("│ %12s │ %9s │ %14.1f │ %16.2f │ %12llu │\n",
                   size_str, "N/A", r.ns_per_call,
                   sizes[i] > 0 ? r.throughput_mbps : 0.0,
                   (unsigned long long)r.iterations);
        }
    }

    printf("└──────────────┴───────────┴────────────────┴──────────────────┴──────────────┘\n\n");
}

static void print_latency_analysis(void) {
    printf("┌─────────────────────────────────────────────────────────────────────────────┐\n");
    printf("│                         Small Message Latency                               │\n");
    printf("├──────────────┬───────────────┬───────────────┬──────────────────────────────┤\n");
    printf("│ Message Size │ Latency (ns)  │ Latency (μs)  │           Notes              │\n");
    printf("├──────────────┼───────────────┼───────────────┼──────────────────────────────┤\n");

    size_t sizes[] = {0, 1, 16, 32, 64, 128};
    const char *notes[] = {
        "Empty message (IV only)",
        "Single byte",
        "Typical short string",
        "32-byte input",
        "64-byte input",
        "One BLAKE4 block"
    };

    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        bench_result r = bench_hash(sizes[i]);

        char size_str[16];
        if (sizes[i] == 0) {
            snprintf(size_str, sizeof(size_str), "empty");
        } else {
            snprintf(size_str, sizeof(size_str), "%zu B", sizes[i]);
        }

        printf("│ %12s │ %13.1f │ %13.3f │ %-28s │\n",
               size_str, r.ns_per_call, r.ns_per_call / 1000.0, notes[i]);
    }

    printf("└──────────────┴───────────────┴───────────────┴──────────────────────────────┘\n\n");
}

static void print_throughput_analysis(void) {
    printf("┌─────────────────────────────────────────────────────────────────────────────┐\n");
    printf("│                      Long Message Throughput                                │\n");
    printf("├──────────────┬──────────────────┬──────────────────┬────────────────────────┤\n");
    printf("│ Message Size │ Throughput MB/s  │ Throughput GB/s  │     cpb (if avail)     │\n");
    printf("├──────────────┼──────────────────┼──────────────────┼────────────────────────┤\n");

    size_t sizes[] = {16384, 65536, 262144, 1048576, 4194304, 16777216};

    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        bench_result r = bench_hash(sizes[i]);

        char size_str[16];
        if (sizes[i] >= 1048576) {
            snprintf(size_str, sizeof(size_str), "%zu MB", sizes[i] / 1048576);
        } else {
            snprintf(size_str, sizeof(size_str), "%zu KB", sizes[i] / 1024);
        }

        if (HAVE_RDTSC) {
            printf("│ %12s │ %16.2f │ %16.3f │ %22.2f │\n",
                   size_str, r.throughput_mbps, r.throughput_mbps / 1024.0,
                   r.cycles_per_byte);
        } else {
            printf("│ %12s │ %16.2f │ %16.3f │ %22s │\n",
                   size_str, r.throughput_mbps, r.throughput_mbps / 1024.0, "N/A");
        }
    }

    printf("└──────────────┴──────────────────┴──────────────────┴────────────────────────┘\n\n");
}

static void print_incremental_analysis(void) {
    printf("┌─────────────────────────────────────────────────────────────────────────────┐\n");
    printf("│                    Incremental Hashing (Streaming)                          │\n");
    printf("├──────────────┬──────────────┬──────────────────┬────────────────────────────┤\n");
    printf("│  Total Size  │  Chunk Size  │ Throughput MB/s  │           Notes            │\n");
    printf("├──────────────┼──────────────┼──────────────────┼────────────────────────────┤\n");

    struct {
        size_t total;
        size_t chunk;
        const char *note;
    } tests[] = {
        {1048576, 64, "Small chunks (64B)"},
        {1048576, 256, "256B chunks"},
        {1048576, 1024, "1KB chunks"},
        {1048576, 4096, "4KB chunks (page size)"},
        {1048576, 16384, "16KB chunks"},
        {1048576, 65536, "64KB chunks"},
    };

    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        bench_result r = bench_incremental(tests[i].total, tests[i].chunk);

        printf("│ %12s │ %12s │ %16.2f │ %-26s │\n",
               "1 MB",
               tests[i].chunk >= 1024 ?
                   (tests[i].chunk >= 65536 ? "64 KB" :
                    tests[i].chunk >= 16384 ? "16 KB" :
                    tests[i].chunk >= 4096 ? "4 KB" : "1 KB") :
                   (tests[i].chunk >= 256 ? "256 B" : "64 B"),
               r.throughput_mbps,
               tests[i].note);
    }

    printf("└──────────────┴──────────────┴──────────────────┴────────────────────────────┘\n\n");
}

static void print_comparison_reference(void) {
    printf("┌─────────────────────────────────────────────────────────────────────────────┐\n");
    printf("│                    Reference: Other Hash Functions                          │\n");
    printf("├─────────────────────────┬───────────┬───────────────────────────────────────┤\n");
    printf("│ Algorithm               │ cpb (typ) │ Notes                                 │\n");
    printf("├─────────────────────────┼───────────┼───────────────────────────────────────┤\n");
    printf("│ BLAKE3 (AVX-512)        │ 0.3-0.5   │ Tree-based, 256-bit state             │\n");
    printf("│ BLAKE3 (AVX2)           │ 0.5-0.8   │ Tree-based, 256-bit state             │\n");
    printf("│ BLAKE3 (NEON)           │ 0.8-1.2   │ Tree-based, 256-bit state             │\n");
    printf("│ BLAKE2b (AVX2)          │ 2.5-3.5   │ 512-bit state, serial                 │\n");
    printf("│ BLAKE2s (AVX2)          │ 3.5-4.5   │ 256-bit state, serial                 │\n");
    printf("│ SHA-256 (SHA-NI)        │ 1.5-2.0   │ Hardware instruction (x86)            │\n");
    printf("│ SHA-256 (ARM CE)        │ 1.0-1.5   │ Hardware instruction (ARM)            │\n");
    printf("│ SHA-512 (software)      │ 5.0-7.0   │ No hardware acceleration              │\n");
    printf("│ SHA3-256 (AVX2)         │ 8.0-12.0  │ Keccak sponge                         │\n");
    printf("├─────────────────────────┴───────────┴───────────────────────────────────────┤\n");
    printf("│ BLAKE4: 512-bit state for post-quantum security (256-bit vs Grover)        │\n");
    printf("│ Target: Competitive with BLAKE2b while providing larger security margin    │\n");
    printf("└─────────────────────────────────────────────────────────────────────────────┘\n\n");
}

static void print_machine_readable(void) {
    printf("# Machine-readable output (CSV format)\n");
    printf("# algorithm,implementation,msg_bytes,cpb,latency_ns,throughput_mbps\n");

    size_t sizes[] = {0, 1, 16, 64, 128, 256, 512, 1024, 4096, 16384, 65536, 1048576};

    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        bench_result r = bench_hash(sizes[i]);
        printf("BLAKE4,%s,%zu,%.2f,%.1f,%.2f\n",
               blake4_dispatch_impl_name(),
               sizes[i],
               HAVE_RDTSC && sizes[i] > 0 ? r.cycles_per_byte : -1.0,
               r.ns_per_call,
               sizes[i] > 0 ? r.throughput_mbps : 0.0);
    }
}

/* ============== Main ============== */

int main(int argc, char **argv) {
    int machine_readable = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--csv") == 0 || strcmp(argv[i], "-m") == 0) {
            machine_readable = 1;
        }
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: %s [options]\n", argv[0]);
            printf("Options:\n");
            printf("  --csv, -m    Output machine-readable CSV format\n");
            printf("  --help, -h   Show this help\n");
            return 0;
        }
    }

    if (machine_readable) {
        print_machine_readable();
        return 0;
    }

    print_header();
    print_cpb_table();
    print_latency_analysis();
    print_throughput_analysis();
    print_incremental_analysis();
    print_comparison_reference();

    printf("Notes:\n");
    printf("  - cpb = cycles per byte (lower is better)\n");
    printf("  - Latency = time for single hash operation\n");
    printf("  - Throughput = sustained rate for large messages\n");
    printf("  - Results may vary based on CPU frequency scaling\n");
    printf("  - For best results, disable turbo boost and run on idle system\n");
    printf("\n");

    printf("Run with --csv for machine-readable output.\n");

    return 0;
}
