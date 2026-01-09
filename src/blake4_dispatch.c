/*
 * BLAKE4 Runtime Dispatch
 *
 * Provides runtime CPU feature detection and dynamic dispatch to the
 * best available SIMD implementation of the compression function.
 *
 * Dispatch hierarchy (best to worst):
 *   x86-64: AVX-512 assembly -> AVX-512 intrinsics -> AVX2 assembly ->
 *           AVX2 intrinsics -> SSE4.1 -> Portable
 *   ARM64:  NEON assembly -> NEON intrinsics -> Portable
 *
 * This is free and unencumbered software released into the public domain.
 */

#include "blake4_dispatch.h"
#include <string.h>
#include <stdatomic.h>

/* Forward declaration removed - prototype is in blake4_dispatch.h */

/* ============== CPU Feature Detection ============== */

/* Cached feature flags */
static _Atomic int g_features_detected = 0;
static _Atomic uint32_t g_cpu_features = 0;

/* Feature flag bits */
#define BLAKE4_CPU_SSE41     (1 << 0)
#define BLAKE4_CPU_AVX       (1 << 1)
#define BLAKE4_CPU_AVX2      (1 << 2)
#define BLAKE4_CPU_AVX512F   (1 << 3)
#define BLAKE4_CPU_AVX512VL  (1 << 4)
#define BLAKE4_CPU_NEON      (1 << 5)

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)

/* x86/x86-64 CPUID implementation */
#if defined(_MSC_VER)
    #include <intrin.h>
    #define CPUID(info, leaf) __cpuid((int*)(info), (leaf))
    #define CPUIDEX(info, leaf, subleaf) __cpuidex((int*)(info), (leaf), (subleaf))
    #define XGETBV(xcr) _xgetbv(xcr)
#elif defined(__GNUC__) || defined(__clang__)
    #include <cpuid.h>
    static inline void CPUID(uint32_t info[4], uint32_t leaf) {
        __cpuid(leaf, info[0], info[1], info[2], info[3]);
    }
    static inline void CPUIDEX(uint32_t info[4], uint32_t leaf, uint32_t subleaf) {
        __cpuid_count(leaf, subleaf, info[0], info[1], info[2], info[3]);
    }
    static inline uint64_t XGETBV(uint32_t xcr) {
        uint32_t eax, edx;
        __asm__ volatile("xgetbv" : "=a"(eax), "=d"(edx) : "c"(xcr));
        return ((uint64_t)edx << 32) | eax;
    }
#endif

static uint32_t detect_cpu_features_x86(void) {
    uint32_t features = 0;
    uint32_t info[4];

    /* Check CPUID is available and get max leaf */
    CPUID(info, 0);
    uint32_t max_leaf = info[0];
    if (max_leaf < 1) {
        return 0;
    }

    /* Leaf 1: SSE4.1, OSXSAVE, AVX */
    CPUID(info, 1);
    int has_sse41 = (info[2] >> 19) & 1;
    int has_osxsave = (info[2] >> 27) & 1;
    int has_avx = (info[2] >> 28) & 1;

    if (has_sse41) {
        features |= BLAKE4_CPU_SSE41;
    }

    /* Check OS support for AVX (XMM and YMM state) */
    int os_avx = 0;
    if (has_osxsave && has_avx) {
        uint64_t xcr0 = XGETBV(0);
        os_avx = ((xcr0 & 0x6) == 0x6);  /* XMM and YMM state enabled */
        if (os_avx) {
            features |= BLAKE4_CPU_AVX;
        }
    }

    /* Leaf 7: AVX2, AVX-512 */
    if (max_leaf >= 7 && os_avx) {
        CPUIDEX(info, 7, 0);
        int has_avx2 = (info[1] >> 5) & 1;
        int has_avx512f = (info[1] >> 16) & 1;
        int has_avx512vl = (info[1] >> 31) & 1;

        if (has_avx2) {
            features |= BLAKE4_CPU_AVX2;
        }

        /* Check OS support for AVX-512 (ZMM state) */
        uint64_t xcr0 = XGETBV(0);
        int os_avx512 = ((xcr0 & 0xE6) == 0xE6);  /* XMM, YMM, ZMM, and opmask */

        if (os_avx512 && has_avx512f) {
            features |= BLAKE4_CPU_AVX512F;
            if (has_avx512vl) {
                features |= BLAKE4_CPU_AVX512VL;
            }
        }
    }

    return features;
}

#elif defined(__aarch64__) || defined(_M_ARM64)

/* ARM64 feature detection */
#if defined(__linux__)
    #include <sys/auxv.h>
    #ifndef HWCAP_ASIMD
        #define HWCAP_ASIMD (1 << 1)
    #endif
#elif defined(__APPLE__)
    #include <sys/sysctl.h>
#endif

static uint32_t detect_cpu_features_arm64(void) {
    uint32_t features = 0;

    /* NEON is mandatory on AArch64, but verify anyway */
#if defined(__linux__)
    unsigned long hwcap = getauxval(AT_HWCAP);
    if (hwcap & HWCAP_ASIMD) {
        features |= BLAKE4_CPU_NEON;
    }
#elif defined(__APPLE__)
    /* NEON is always available on Apple Silicon */
    features |= BLAKE4_CPU_NEON;
#else
    /* Assume NEON available on ARM64 */
    features |= BLAKE4_CPU_NEON;
#endif

    return features;
}

#else

/* Unknown architecture */
static uint32_t detect_cpu_features_unknown(void) {
    return 0;
}

#endif /* Architecture detection */

/*
 * Detect CPU features (cached).
 * Thread-safe: uses atomic operations.
 */
static uint32_t get_cpu_features(void) {
    if (atomic_load(&g_features_detected)) {
        return atomic_load(&g_cpu_features);
    }

    uint32_t features = 0;

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    features = detect_cpu_features_x86();
#elif defined(__aarch64__) || defined(_M_ARM64)
    features = detect_cpu_features_arm64();
#else
    features = detect_cpu_features_unknown();
#endif

    atomic_store(&g_cpu_features, features);
    atomic_store(&g_features_detected, 1);

    return features;
}

/* ============== Implementation Registry ============== */

/* Implementation IDs */
typedef enum {
    BLAKE4_IMPL_PORTABLE = 0,
    BLAKE4_IMPL_SSE41,
    BLAKE4_IMPL_AVX2_INTRINSICS,
    BLAKE4_IMPL_AVX2_ASM,
    BLAKE4_IMPL_AVX512_INTRINSICS,
    BLAKE4_IMPL_AVX512_ASM,
    BLAKE4_IMPL_NEON_INTRINSICS,
    BLAKE4_IMPL_NEON_ASM,
    BLAKE4_IMPL_COUNT
} blake4_impl_id;

/* Implementation info */
typedef struct {
    blake4_impl_id id;
    const char *name;
    blake4_compress_fn compress;
    uint32_t required_features;  /* CPU features required */
    int priority;                /* Higher = preferred */
} blake4_impl_t;

/* Cached best implementation */
static _Atomic int g_impl_selected = 0;
static blake4_compress_fn g_compress_fn = NULL;
static const char *g_impl_name = "Uninitialized";

/* ============== Portable Implementation ============== */

/* IV: Same as SHA-512 */
static const uint64_t IV[8] = {
    0x6A09E667F3BCC908ULL,
    0xBB67AE8584CAA73BULL,
    0x3C6EF372FE94F82BULL,
    0xA54FF53A5F1D36F1ULL,
    0x510E527FADE682D1ULL,
    0x9B05688C2B3E6C1FULL,
    0x1F83D9ABFB41BD6BULL,
    0x5BE0CD19137E2179ULL
};

/* Message schedule permutations */
static const uint8_t SIGMA[10][16] = {
    { 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15},
    {14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3},
    {11,  8, 12,  0,  5,  2, 15, 13, 10, 14,  3,  6,  7,  1,  9,  4},
    { 7,  9,  3,  1, 13, 12, 11, 14,  2,  6,  5, 10,  4,  0, 15,  8},
    { 9,  0,  5,  7,  2,  4, 10, 15, 14,  1, 11, 12,  6,  8,  3, 13},
    { 2, 12,  6, 10,  0, 11,  8,  3,  4, 13,  7,  5, 15, 14,  1,  9},
    {12,  5,  1, 15, 14, 13,  4, 10,  0,  7,  6,  3,  9,  2,  8, 11},
    {13, 11,  7, 14, 12,  1,  3,  9,  5,  0, 15,  4,  8,  6,  2, 10},
    { 6, 15, 14,  9, 11,  3,  0,  8, 12,  2, 13,  7,  1,  4, 10,  5},
    {10,  2,  8,  4,  7,  6,  1,  5, 15, 11,  9, 14,  3, 12, 13,  0},
};

/* Rotation constants */
#define R1 32
#define R2 24
#define R3 16
#define R4 63

static inline uint64_t rotr64(uint64_t x, int n) {
    return (x >> n) | (x << (64 - n));
}

static inline uint64_t load64_le(const uint8_t *p) {
    return ((uint64_t)p[0]) |
           ((uint64_t)p[1] << 8) |
           ((uint64_t)p[2] << 16) |
           ((uint64_t)p[3] << 24) |
           ((uint64_t)p[4] << 32) |
           ((uint64_t)p[5] << 40) |
           ((uint64_t)p[6] << 48) |
           ((uint64_t)p[7] << 56);
}

static inline void g_portable(uint64_t *state, int a, int b, int c, int d,
                              uint64_t mx, uint64_t my) {
    state[a] = state[a] + state[b] + mx;
    state[d] = rotr64(state[d] ^ state[a], R1);
    state[c] = state[c] + state[d];
    state[b] = rotr64(state[b] ^ state[c], R2);
    state[a] = state[a] + state[b] + my;
    state[d] = rotr64(state[d] ^ state[a], R3);
    state[c] = state[c] + state[d];
    state[b] = rotr64(state[b] ^ state[c], R4);
}

void blake4_compress_portable(const uint64_t cv[8],
                              const uint8_t block[128],
                              uint32_t block_len,
                              uint64_t counter,
                              uint32_t flags,
                              uint64_t out[16]) {
    uint64_t state[16];
    uint64_t m[16];

    /* Initialize state */
    for (int i = 0; i < 8; i++) state[i] = cv[i];
    state[8]  = IV[0];
    state[9]  = IV[1];
    state[10] = IV[2];
    state[11] = IV[3];
    state[12] = IV[4] ^ counter;
    state[13] = IV[5];
    state[14] = IV[6] ^ block_len;
    state[15] = IV[7] ^ flags;

    /* Load message */
    for (int i = 0; i < 16; i++) {
        m[i] = load64_le(block + i * 8);
    }

    /* 10 rounds */
    for (int r = 0; r < 10; r++) {
        const uint8_t *s = SIGMA[r];

        /* Column step */
        g_portable(state, 0, 4,  8, 12, m[s[ 0]], m[s[ 1]]);
        g_portable(state, 1, 5,  9, 13, m[s[ 2]], m[s[ 3]]);
        g_portable(state, 2, 6, 10, 14, m[s[ 4]], m[s[ 5]]);
        g_portable(state, 3, 7, 11, 15, m[s[ 6]], m[s[ 7]]);

        /* Diagonal step */
        g_portable(state, 0, 5, 10, 15, m[s[ 8]], m[s[ 9]]);
        g_portable(state, 1, 6, 11, 12, m[s[10]], m[s[11]]);
        g_portable(state, 2, 7,  8, 13, m[s[12]], m[s[13]]);
        g_portable(state, 3, 4,  9, 14, m[s[14]], m[s[15]]);
    }

    /* Output: XOR halves */
    for (int i = 0; i < 8; i++) {
        out[i] = state[i] ^ state[i + 8];
    }
    for (int i = 8; i < 16; i++) {
        out[i] = state[i] ^ cv[i - 8];
    }
}

/* ============== External Implementation Declarations ============== */

/*
 * These are conditionally defined based on what's actually compiled in.
 * CMake sets these defines when the corresponding source files are included.
 *
 * IMPORTANT: Static library linking may drop object files that aren't directly
 * referenced. To ensure SIMD implementations are always linked, we declare
 * external initialization functions and call them explicitly.
 */

/* External SIMD initialization functions - defined in respective source files */
#if defined(__aarch64__) || defined(_M_ARM64)
extern void blake4_init_neon(void);
extern void blake4_init_neon_asm(void);
#elif defined(__x86_64__) || defined(_M_X64)
extern void blake4_init_avx2(void);
extern void blake4_init_avx2_asm(void);
extern void blake4_init_avx512(void);
extern void blake4_init_avx512_asm(void);
#endif

/* Implementation function pointers - NULL if not available */
static blake4_compress_fn g_avx2_fn = NULL;
static blake4_compress_fn g_avx2_asm_fn = NULL;
static blake4_compress_fn g_avx512_fn = NULL;
static blake4_compress_fn g_avx512_asm_fn = NULL;
static blake4_compress_fn g_neon_fn = NULL;
static blake4_compress_fn g_neon_asm_fn = NULL;

/*
 * Register an implementation at runtime.
 * This is called by the SIMD source files during static initialization.
 */
void blake4_register_avx2(blake4_compress_fn fn) { g_avx2_fn = fn; }
void blake4_register_avx2_asm(blake4_compress_fn fn) { g_avx2_asm_fn = fn; }
void blake4_register_avx512(blake4_compress_fn fn) { g_avx512_fn = fn; }
void blake4_register_avx512_asm(blake4_compress_fn fn) { g_avx512_asm_fn = fn; }
void blake4_register_neon(blake4_compress_fn fn) { g_neon_fn = fn; }
void blake4_register_neon_asm(blake4_compress_fn fn) { g_neon_asm_fn = fn; }

/* ============== Implementation Selection ============== */

/*
 * Initialize SIMD implementations.
 * This must be called before select_implementation() to ensure all
 * implementations are registered even when linking statically.
 */
static _Atomic int g_simd_initialized = 0;

static void init_simd_implementations(void) {
    if (atomic_load(&g_simd_initialized)) {
        return;
    }

#if defined(__aarch64__) || defined(_M_ARM64)
    blake4_init_neon();
    blake4_init_neon_asm();
#elif defined(__x86_64__) || defined(_M_X64)
    blake4_init_avx512();
    blake4_init_avx512_asm();
    blake4_init_avx2();
    blake4_init_avx2_asm();
#endif

    atomic_store(&g_simd_initialized, 1);
}

/*
 * Select the best available implementation based on CPU features.
 * Called automatically on first use.
 */
static void select_implementation(void) {
    if (atomic_load(&g_impl_selected)) {
        return;
    }

    /* Ensure SIMD implementations are registered */
    init_simd_implementations();

    uint32_t features = get_cpu_features();

    /* Try implementations in order of preference (highest priority first) */

    /* AVX-512 assembly (highest priority for x86-64) */
    if ((features & (BLAKE4_CPU_AVX512F | BLAKE4_CPU_AVX512VL)) ==
        (BLAKE4_CPU_AVX512F | BLAKE4_CPU_AVX512VL)) {
        if (g_avx512_asm_fn) {
            g_compress_fn = g_avx512_asm_fn;
            g_impl_name = "AVX-512 (asm)";
            atomic_store(&g_impl_selected, 1);
            return;
        }
        if (g_avx512_fn) {
            g_compress_fn = g_avx512_fn;
            g_impl_name = "AVX-512 (intrinsics)";
            atomic_store(&g_impl_selected, 1);
            return;
        }
    }

    /* AVX2 assembly */
    if (features & BLAKE4_CPU_AVX2) {
        if (g_avx2_asm_fn) {
            g_compress_fn = g_avx2_asm_fn;
            g_impl_name = "AVX2 (asm)";
            atomic_store(&g_impl_selected, 1);
            return;
        }
        if (g_avx2_fn) {
            g_compress_fn = g_avx2_fn;
            g_impl_name = "AVX2 (intrinsics)";
            atomic_store(&g_impl_selected, 1);
            return;
        }
    }

    /* NEON assembly (ARM64) */
    if (features & BLAKE4_CPU_NEON) {
        if (g_neon_asm_fn) {
            g_compress_fn = g_neon_asm_fn;
            g_impl_name = "NEON (asm)";
            atomic_store(&g_impl_selected, 1);
            return;
        }
        if (g_neon_fn) {
            g_compress_fn = g_neon_fn;
            g_impl_name = "NEON (intrinsics)";
            atomic_store(&g_impl_selected, 1);
            return;
        }
    }

    /* Fallback to portable */
    g_compress_fn = blake4_compress_portable;
    g_impl_name = "Portable C";
    atomic_store(&g_impl_selected, 1);
}

/* ============== Public API ============== */

/*
 * Get the compression function to use.
 * This is the main entry point for the hasher.
 */
blake4_compress_fn blake4_dispatch_get_compress(void) {
    if (!atomic_load(&g_impl_selected)) {
        select_implementation();
    }
    return g_compress_fn;
}

/*
 * Compress a single block using the best available implementation.
 */
void blake4_dispatch_compress(const uint64_t cv[8],
                              const uint8_t block[128],
                              uint32_t block_len,
                              uint64_t counter,
                              uint32_t flags,
                              uint64_t out[16]) {
    if (!atomic_load(&g_impl_selected)) {
        select_implementation();
    }
    g_compress_fn(cv, block, block_len, counter, flags, out);
}

/*
 * Get the name of the current implementation.
 */
const char *blake4_dispatch_impl_name(void) {
    if (!atomic_load(&g_impl_selected)) {
        select_implementation();
    }
    return g_impl_name;
}

/*
 * Force selection of a specific implementation.
 * Useful for testing or benchmarking.
 * Returns 1 on success, 0 if implementation is not available.
 */
int blake4_dispatch_set_impl(blake4_impl_select impl) {
    uint32_t features = get_cpu_features();

    switch (impl) {
        case BLAKE4_SELECT_PORTABLE:
            g_compress_fn = blake4_compress_portable;
            g_impl_name = "Portable C (forced)";
            break;

        case BLAKE4_SELECT_AVX2:
            if (!(features & BLAKE4_CPU_AVX2)) return 0;
            if (g_avx2_fn) {
                g_compress_fn = g_avx2_fn;
                g_impl_name = "AVX2 (intrinsics, forced)";
            } else if (g_avx2_asm_fn) {
                g_compress_fn = g_avx2_asm_fn;
                g_impl_name = "AVX2 (asm, forced)";
            } else {
                return 0;
            }
            break;

        case BLAKE4_SELECT_AVX2_ASM:
            if (!(features & BLAKE4_CPU_AVX2)) return 0;
            if (!g_avx2_asm_fn) return 0;
            g_compress_fn = g_avx2_asm_fn;
            g_impl_name = "AVX2 (asm, forced)";
            break;

        case BLAKE4_SELECT_AVX512:
            if (!((features & BLAKE4_CPU_AVX512F) &&
                  (features & BLAKE4_CPU_AVX512VL))) return 0;
            if (g_avx512_fn) {
                g_compress_fn = g_avx512_fn;
                g_impl_name = "AVX-512 (intrinsics, forced)";
            } else if (g_avx512_asm_fn) {
                g_compress_fn = g_avx512_asm_fn;
                g_impl_name = "AVX-512 (asm, forced)";
            } else {
                return 0;
            }
            break;

        case BLAKE4_SELECT_AVX512_ASM:
            if (!((features & BLAKE4_CPU_AVX512F) &&
                  (features & BLAKE4_CPU_AVX512VL))) return 0;
            if (!g_avx512_asm_fn) return 0;
            g_compress_fn = g_avx512_asm_fn;
            g_impl_name = "AVX-512 (asm, forced)";
            break;

        case BLAKE4_SELECT_NEON:
            if (!(features & BLAKE4_CPU_NEON)) return 0;
            if (g_neon_fn) {
                g_compress_fn = g_neon_fn;
                g_impl_name = "NEON (intrinsics, forced)";
            } else if (g_neon_asm_fn) {
                g_compress_fn = g_neon_asm_fn;
                g_impl_name = "NEON (asm, forced)";
            } else {
                return 0;
            }
            break;

        case BLAKE4_SELECT_NEON_ASM:
            if (!(features & BLAKE4_CPU_NEON)) return 0;
            if (!g_neon_asm_fn) return 0;
            g_compress_fn = g_neon_asm_fn;
            g_impl_name = "NEON (asm, forced)";
            break;

        case BLAKE4_SELECT_AUTO:
        default:
            atomic_store(&g_impl_selected, 0);
            select_implementation();
            return 1;
    }

    atomic_store(&g_impl_selected, 1);
    return 1;
}

/*
 * Get CPU feature flags.
 */
uint32_t blake4_dispatch_cpu_features(void) {
    return get_cpu_features();
}

/*
 * Check if a specific feature is supported.
 */
int blake4_dispatch_has_feature(blake4_cpu_feature feature) {
    uint32_t features = get_cpu_features();

    switch (feature) {
        case BLAKE4_FEATURE_SSE41:
            return (features & BLAKE4_CPU_SSE41) != 0;
        case BLAKE4_FEATURE_AVX:
            return (features & BLAKE4_CPU_AVX) != 0;
        case BLAKE4_FEATURE_AVX2:
            return (features & BLAKE4_CPU_AVX2) != 0;
        case BLAKE4_FEATURE_AVX512F:
            return (features & BLAKE4_CPU_AVX512F) != 0;
        case BLAKE4_FEATURE_AVX512VL:
            return (features & BLAKE4_CPU_AVX512VL) != 0;
        case BLAKE4_FEATURE_NEON:
            return (features & BLAKE4_CPU_NEON) != 0;
        default:
            return 0;
    }
}
