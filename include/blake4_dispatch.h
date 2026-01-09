/*
 * BLAKE4 Runtime Dispatch Header
 *
 * Provides runtime CPU feature detection and dynamic dispatch to the
 * best available SIMD implementation of the compression function.
 *
 * This is free and unencumbered software released into the public domain.
 */

#ifndef BLAKE4_DISPATCH_H
#define BLAKE4_DISPATCH_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============== Compression Function Type ============== */

/**
 * Function pointer type for BLAKE4 compression function.
 *
 * @param cv        8-word (512-bit) chaining value
 * @param block     128-byte block
 * @param block_len Actual bytes in block
 * @param counter   Block counter within chunk
 * @param flags     Domain separation flags
 * @param out       16-word output (first 8 for chaining, all 16 for root)
 */
typedef void (*blake4_compress_fn)(const uint64_t cv[8],
                                   const uint8_t block[128],
                                   uint32_t block_len,
                                   uint64_t counter,
                                   uint32_t flags,
                                   uint64_t out[16]);

/* ============== CPU Feature Detection ============== */

/**
 * CPU feature flags for querying support.
 */
typedef enum {
    BLAKE4_FEATURE_SSE41,
    BLAKE4_FEATURE_AVX,
    BLAKE4_FEATURE_AVX2,
    BLAKE4_FEATURE_AVX512F,
    BLAKE4_FEATURE_AVX512VL,
    BLAKE4_FEATURE_NEON
} blake4_cpu_feature;

/**
 * Implementation selection options.
 */
typedef enum {
    BLAKE4_SELECT_AUTO = 0,      /* Automatic (best available) */
    BLAKE4_SELECT_PORTABLE,      /* Portable C */
    BLAKE4_SELECT_AVX2,          /* AVX2 (prefer intrinsics) */
    BLAKE4_SELECT_AVX2_ASM,      /* AVX2 assembly only */
    BLAKE4_SELECT_AVX512,        /* AVX-512 (prefer intrinsics) */
    BLAKE4_SELECT_AVX512_ASM,    /* AVX-512 assembly only */
    BLAKE4_SELECT_NEON,          /* NEON (prefer intrinsics) */
    BLAKE4_SELECT_NEON_ASM       /* NEON assembly only */
} blake4_impl_select;

/* ============== Dispatch API ============== */

/**
 * Get the compression function to use.
 * Automatically selects the best available implementation on first call.
 *
 * @return Pointer to the compression function
 */
blake4_compress_fn blake4_dispatch_get_compress(void);

/**
 * Compress a single block using the best available implementation.
 * This is a convenience function that handles dispatch internally.
 *
 * @param cv        8-word (512-bit) chaining value
 * @param block     128-byte block
 * @param block_len Actual bytes in block
 * @param counter   Block counter within chunk
 * @param flags     Domain separation flags
 * @param out       16-word output
 */
void blake4_dispatch_compress(const uint64_t cv[8],
                              const uint8_t block[128],
                              uint32_t block_len,
                              uint64_t counter,
                              uint32_t flags,
                              uint64_t out[16]);

/**
 * Get the name of the currently selected implementation.
 *
 * @return Human-readable name (e.g., "AVX-512 (asm)", "Portable C")
 */
const char *blake4_dispatch_impl_name(void);

/**
 * Force selection of a specific implementation.
 * Useful for testing, benchmarking, or working around CPU bugs.
 *
 * @param impl Implementation to use
 * @return 1 on success, 0 if implementation is not available
 */
int blake4_dispatch_set_impl(blake4_impl_select impl);

/**
 * Get raw CPU feature flags.
 *
 * @return Bitmask of detected CPU features
 */
uint32_t blake4_dispatch_cpu_features(void);

/**
 * Check if a specific CPU feature is supported.
 *
 * @param feature Feature to check
 * @return 1 if supported, 0 otherwise
 */
int blake4_dispatch_has_feature(blake4_cpu_feature feature);

/* ============== Implementation Registration ============== */

/**
 * Register SIMD implementations with the dispatcher.
 * These are called by the SIMD source files during initialization.
 * Not typically called by user code.
 */
void blake4_register_avx2(blake4_compress_fn fn);
void blake4_register_avx2_asm(blake4_compress_fn fn);
void blake4_register_avx512(blake4_compress_fn fn);
void blake4_register_avx512_asm(blake4_compress_fn fn);
void blake4_register_neon(blake4_compress_fn fn);
void blake4_register_neon_asm(blake4_compress_fn fn);

/* ============== Portable Implementation ============== */

/**
 * Portable C reference implementation.
 * Always available and used as fallback.
 */
void blake4_compress_portable(const uint64_t cv[8],
                              const uint8_t block[128],
                              uint32_t block_len,
                              uint64_t counter,
                              uint32_t flags,
                              uint64_t out[16]);

#ifdef __cplusplus
}
#endif

#endif /* BLAKE4_DISPATCH_H */
