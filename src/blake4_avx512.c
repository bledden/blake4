/*
 * BLAKE4 AVX-512 Implementation
 *
 * Optimized compression function using AVX-512 intrinsics.
 * This provides maximum performance on modern x86-64 processors with AVX-512.
 *
 * Key optimizations:
 * - Full 512-bit state held in ZMM registers
 * - Native 64-bit rotates using vprorq
 * - Parallel G function operations within vectors
 *
 * This is free and unencumbered software released into the public domain.
 */

#include "blake4_dispatch.h"

#if defined(__x86_64__) || defined(_M_X64)
#if defined(__AVX512F__) && defined(__AVX512VL__)

#include <immintrin.h>
#include <string.h>

/* ============== Constants ============== */

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

/* ============== AVX-512 Helper Macros ============== */

/*
 * AVX-512 provides native 64-bit rotate via vprorq.
 * We use 256-bit (4x64) and 512-bit (8x64) vectors.
 */

/* Rotate right by constant for 256-bit vector (4 x 64-bit) */
#define ROR256_32(x) _mm256_ror_epi64(x, 32)
#define ROR256_24(x) _mm256_ror_epi64(x, 24)
#define ROR256_16(x) _mm256_ror_epi64(x, 16)
#define ROR256_63(x) _mm256_ror_epi64(x, 63)

/* ============== AVX-512 G Function ============== */

/*
 * BLAKE4 G function operating on 4 parallel operations.
 * Uses 256-bit AVX-512 vectors (4 x 64-bit).
 *
 * State layout in vectors:
 *   a = [state[0], state[1], state[2], state[3]]
 *   b = [state[4], state[5], state[6], state[7]]
 *   c = [state[8], state[9], state[10], state[11]]
 *   d = [state[12], state[13], state[14], state[15]]
 *
 * This allows processing all 4 column or diagonal G operations in parallel.
 */
static inline void G_AVX512(__m256i *a, __m256i *b, __m256i *c, __m256i *d,
                             __m256i mx, __m256i my) {
    /* a = a + b + mx */
    *a = _mm256_add_epi64(_mm256_add_epi64(*a, *b), mx);
    /* d = rotr(d ^ a, R1) */
    *d = ROR256_32(_mm256_xor_si256(*d, *a));
    /* c = c + d */
    *c = _mm256_add_epi64(*c, *d);
    /* b = rotr(b ^ c, R2) */
    *b = ROR256_24(_mm256_xor_si256(*b, *c));
    /* a = a + b + my */
    *a = _mm256_add_epi64(_mm256_add_epi64(*a, *b), my);
    /* d = rotr(d ^ a, R3) */
    *d = ROR256_16(_mm256_xor_si256(*d, *a));
    /* c = c + d */
    *c = _mm256_add_epi64(*c, *d);
    /* b = rotr(b ^ c, R4) */
    *b = ROR256_63(_mm256_xor_si256(*b, *c));
}

/* ============== AVX-512 Compression Function ============== */

/*
 * Compression function using AVX-512 intrinsics.
 * Uses 256-bit operations to process 4 G functions in parallel.
 */
static void blake4_compress_avx512_impl(const uint64_t cv[8],
                                        const uint8_t block[128],
                                        uint32_t block_len,
                                        uint64_t counter,
                                        uint32_t flags,
                                        uint64_t out[16]) {
    /*
     * State layout (16 x 64-bit words):
     *   [ 0  1  2  3 ]   <- row 0 (cv[0..3])
     *   [ 4  5  6  7 ]   <- row 1 (cv[4..7])
     *   [ 8  9 10 11 ]   <- row 2 (IV[0..3])
     *   [12 13 14 15 ]   <- row 3 (IV[4..7] with tweaks)
     *
     * Reorganize into columns for parallel G operations:
     *   a = [0, 1, 2, 3]
     *   b = [4, 5, 6, 7]
     *   c = [8, 9, 10, 11]
     *   d = [12, 13, 14, 15]
     */

    /* Load chaining value */
    __m256i row0 = _mm256_loadu_si256((const __m256i *)cv);       /* cv[0..3] */
    __m256i row1 = _mm256_loadu_si256((const __m256i *)(cv + 4)); /* cv[4..7] */

    /* Load IV */
    __m256i row2 = _mm256_loadu_si256((const __m256i *)IV);       /* IV[0..3] */

    /* Row 3: IV[4..7] with counter, block_len, and flags XOR'd in */
    __m256i row3 = _mm256_set_epi64x(
        IV[7] ^ flags,
        IV[6] ^ block_len,
        IV[5],
        IV[4] ^ counter
    );

    /* Load message block (16 x 64-bit words) */
    __m256i m0 = _mm256_loadu_si256((const __m256i *)(block + 0));
    __m256i m1 = _mm256_loadu_si256((const __m256i *)(block + 32));
    __m256i m2 = _mm256_loadu_si256((const __m256i *)(block + 64));
    __m256i m3 = _mm256_loadu_si256((const __m256i *)(block + 96));

    /* Extract individual message words for permutation */
    uint64_t m[16];
    _mm256_storeu_si256((__m256i *)m, m0);
    _mm256_storeu_si256((__m256i *)(m + 4), m1);
    _mm256_storeu_si256((__m256i *)(m + 8), m2);
    _mm256_storeu_si256((__m256i *)(m + 12), m3);

    /* 10 rounds */
    for (int r = 0; r < 10; r++) {
        const uint8_t *s = SIGMA[r];

        /*
         * Column step: G operations on columns
         * G(0, 4,  8, 12) with m[s[0]], m[s[1]]
         * G(1, 5,  9, 13) with m[s[2]], m[s[3]]
         * G(2, 6, 10, 14) with m[s[4]], m[s[5]]
         * G(3, 7, 11, 15) with m[s[6]], m[s[7]]
         */
        __m256i mx = _mm256_set_epi64x(m[s[6]], m[s[4]], m[s[2]], m[s[0]]);
        __m256i my = _mm256_set_epi64x(m[s[7]], m[s[5]], m[s[3]], m[s[1]]);
        G_AVX512(&row0, &row1, &row2, &row3, mx, my);

        /*
         * Diagonal step: G operations on diagonals
         * G(0, 5, 10, 15) with m[s[8]], m[s[9]]
         * G(1, 6, 11, 12) with m[s[10]], m[s[11]]
         * G(2, 7,  8, 13) with m[s[12]], m[s[13]]
         * G(3, 4,  9, 14) with m[s[14]], m[s[15]]
         *
         * For diagonal, we need to permute rows:
         * Row 1: [4, 5, 6, 7] -> [5, 6, 7, 4] (rotate left by 1)
         * Row 2: [8, 9, 10, 11] -> [10, 11, 8, 9] (rotate left by 2)
         * Row 3: [12, 13, 14, 15] -> [15, 12, 13, 14] (rotate left by 3)
         */

        /* Shuffle rows for diagonal step */
        row1 = _mm256_permute4x64_epi64(row1, _MM_SHUFFLE(0, 3, 2, 1));  /* [4,5,6,7] -> [5,6,7,4] */
        row2 = _mm256_permute4x64_epi64(row2, _MM_SHUFFLE(1, 0, 3, 2));  /* [8,9,10,11] -> [10,11,8,9] */
        row3 = _mm256_permute4x64_epi64(row3, _MM_SHUFFLE(2, 1, 0, 3));  /* [12,13,14,15] -> [15,12,13,14] */

        /* Diagonal G operations */
        mx = _mm256_set_epi64x(m[s[14]], m[s[12]], m[s[10]], m[s[8]]);
        my = _mm256_set_epi64x(m[s[15]], m[s[13]], m[s[11]], m[s[9]]);
        G_AVX512(&row0, &row1, &row2, &row3, mx, my);

        /* Unshuffle rows back to column order */
        row1 = _mm256_permute4x64_epi64(row1, _MM_SHUFFLE(2, 1, 0, 3));  /* [5,6,7,4] -> [4,5,6,7] */
        row2 = _mm256_permute4x64_epi64(row2, _MM_SHUFFLE(1, 0, 3, 2));  /* [10,11,8,9] -> [8,9,10,11] */
        row3 = _mm256_permute4x64_epi64(row3, _MM_SHUFFLE(0, 3, 2, 1));  /* [15,12,13,14] -> [12,13,14,15] */
    }

    /* Finalize: XOR state halves */
    __m256i out0 = _mm256_xor_si256(row0, row2);  /* [0..3] ^ [8..11] */
    __m256i out1 = _mm256_xor_si256(row1, row3);  /* [4..7] ^ [12..15] */

    /* Second half: state[8..15] ^ cv[0..7] */
    __m256i cv0 = _mm256_loadu_si256((const __m256i *)cv);
    __m256i cv1 = _mm256_loadu_si256((const __m256i *)(cv + 4));

    __m256i out2 = _mm256_xor_si256(row2, cv0);  /* [8..11] ^ cv[0..3] */
    __m256i out3 = _mm256_xor_si256(row3, cv1);  /* [12..15] ^ cv[4..7] */

    /* Store output */
    _mm256_storeu_si256((__m256i *)out, out0);
    _mm256_storeu_si256((__m256i *)(out + 4), out1);
    _mm256_storeu_si256((__m256i *)(out + 8), out2);
    _mm256_storeu_si256((__m256i *)(out + 12), out3);
}

/* ============== Registration ============== */

/*
 * Initialize and register the AVX-512 intrinsics implementation.
 * Called by blake4_dispatch.c to ensure registration even with static linking.
 */
void blake4_init_avx512(void) {
    blake4_register_avx512(blake4_compress_avx512_impl);
}

#endif /* __AVX512F__ && __AVX512VL__ */
#endif /* __x86_64__ || _M_X64 */
