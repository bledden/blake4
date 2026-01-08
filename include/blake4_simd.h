/*
 * BLAKE4 SIMD Optimizations Header
 *
 * Provides SIMD-optimized implementations of BLAKE4 compression
 * for improved performance on supported platforms.
 *
 * This is free and unencumbered software released into the public domain.
 */

#ifndef BLAKE4_SIMD_H
#define BLAKE4_SIMD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============== Compression Function Type ============== */

/**
 * Function pointer type for BLAKE4 compression function.
 *
 * cv: 8-word (512-bit) chaining value
 * block: 128-byte block
 * block_len: actual bytes in block
 * counter: block counter within chunk
 * flags: domain separation flags
 * out: 16-word output (first 8 for chaining, all 16 for root)
 */
typedef void (*blake4_compress_fn)(const uint64_t cv[8],
                                   const uint8_t block[128],
                                   uint32_t block_len,
                                   uint64_t counter,
                                   uint32_t flags,
                                   uint64_t out[16]);

/* ============== Platform Detection ============== */

/**
 * Check if SIMD optimizations are available on this platform.
 * Returns 1 if SIMD is available, 0 otherwise.
 */
int blake4_simd_available(void);

/**
 * Get the name of the SIMD implementation in use.
 * Returns one of: "AVX-512", "AVX2", "NEON", or "Portable C"
 */
const char* blake4_simd_name(void);

/**
 * Get the best available compression function for this platform.
 * Returns NULL if only portable implementation is available.
 */
blake4_compress_fn blake4_get_compress_impl(void);

/* ============== SIMD Compression Functions ============== */

#if defined(__AVX512F__) && defined(__AVX512VL__)
/**
 * AVX-512 optimized compression function.
 */
void blake4_compress_avx512(const uint64_t cv[8], const uint8_t block[128],
                            uint32_t block_len, uint64_t counter,
                            uint32_t flags, uint64_t out[16]);
#endif

#if defined(__AVX2__)
/**
 * AVX2 optimized compression function.
 */
void blake4_compress_avx2(const uint64_t cv[8], const uint8_t block[128],
                          uint32_t block_len, uint64_t counter,
                          uint32_t flags, uint64_t out[16]);
#endif

#if defined(__ARM_NEON)
/**
 * ARM NEON optimized compression function.
 */
void blake4_compress_neon(const uint64_t cv[8], const uint8_t block[128],
                          uint32_t block_len, uint64_t counter,
                          uint32_t flags, uint64_t out[16]);
#endif

#ifdef __cplusplus
}
#endif

#endif /* BLAKE4_SIMD_H */
