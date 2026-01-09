/*
 * BLAKE4 NEON Implementation
 *
 * Optimized compression function using ARM NEON intrinsics.
 * This provides vectorized operations for ARM64 (AArch64) processors.
 *
 * Key optimizations:
 * - 128-bit vector operations on state pairs
 * - Parallel G function operations
 * - Optimized for Apple Silicon and ARM servers
 *
 * This is free and unencumbered software released into the public domain.
 */

#include "blake4_dispatch.h"

#if defined(__aarch64__) || defined(_M_ARM64)
#if defined(__ARM_NEON) || defined(__ARM_NEON__)

#include <arm_neon.h>
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

/* ============== NEON Helper Functions ============== */

/*
 * Rotate right for 64-bit values in NEON.
 * NEON doesn't have native 64-bit rotate, so we use shift pairs.
 */
static inline uint64x2_t vrorq_n_u64_32(uint64x2_t x) {
    /* R1 = 32: special case, can use rev32 */
    return vreinterpretq_u64_u32(vrev64q_u32(vreinterpretq_u32_u64(x)));
}

static inline uint64x2_t vrorq_n_u64_24(uint64x2_t x) {
    /* R2 = 24: use shift pair */
    return vorrq_u64(vshrq_n_u64(x, 24), vshlq_n_u64(x, 40));
}

static inline uint64x2_t vrorq_n_u64_16(uint64x2_t x) {
    /* R3 = 16: use shift pair */
    return vorrq_u64(vshrq_n_u64(x, 16), vshlq_n_u64(x, 48));
}

static inline uint64x2_t vrorq_n_u64_63(uint64x2_t x) {
    /* R4 = 63: use shift pair (or ror by 63 = rol by 1) */
    return vorrq_u64(vshrq_n_u64(x, 63), vshlq_n_u64(x, 1));
}

/* ============== NEON G Function ============== */

/*
 * Vectorized G function operating on two pairs of state values.
 * This processes two G operations in parallel.
 *
 * State layout in vectors:
 *   row0 = [state[0], state[1]]
 *   row1 = [state[2], state[3]]
 *   row2 = [state[4], state[5]]
 *   row3 = [state[6], state[7]]
 *   ... etc for the 16-word state
 */

/*
 * G function for column/diagonal operations.
 * Operates on state elements: a, b, c, d with message words mx, my.
 */
static inline void G_NEON(uint64x2_t *a, uint64x2_t *b, uint64x2_t *c, uint64x2_t *d,
                          uint64x2_t mx, uint64x2_t my) {
    /* a = a + b + mx */
    *a = vaddq_u64(vaddq_u64(*a, *b), mx);
    /* d = rotr(d ^ a, R1) */
    *d = vrorq_n_u64_32(veorq_u64(*d, *a));
    /* c = c + d */
    *c = vaddq_u64(*c, *d);
    /* b = rotr(b ^ c, R2) */
    *b = vrorq_n_u64_24(veorq_u64(*b, *c));
    /* a = a + b + my */
    *a = vaddq_u64(vaddq_u64(*a, *b), my);
    /* d = rotr(d ^ a, R3) */
    *d = vrorq_n_u64_16(veorq_u64(*d, *a));
    /* c = c + d */
    *c = vaddq_u64(*c, *d);
    /* b = rotr(b ^ c, R4) */
    *b = vrorq_n_u64_63(veorq_u64(*b, *c));
}

/* ============== NEON Compression Function ============== */

/*
 * Compression function using NEON intrinsics.
 * Processes the 16-word state using vectorized operations.
 */
static void blake4_compress_neon_impl(const uint64_t cv[8],
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
     * We use pairs of 64-bit values in each 128-bit vector.
     */

    /* Load chaining value and IV into vectors */
    uint64x2_t row0_lo = vld1q_u64(cv + 0);      /* [cv[0], cv[1]] */
    uint64x2_t row0_hi = vld1q_u64(cv + 2);      /* [cv[2], cv[3]] */
    uint64x2_t row1_lo = vld1q_u64(cv + 4);      /* [cv[4], cv[5]] */
    uint64x2_t row1_hi = vld1q_u64(cv + 6);      /* [cv[6], cv[7]] */

    uint64x2_t row2_lo = vld1q_u64(IV + 0);      /* [IV[0], IV[1]] */
    uint64x2_t row2_hi = vld1q_u64(IV + 2);      /* [IV[2], IV[3]] */

    /* Row 3: IV[4..7] with counter, block_len, and flags XOR'd in */
    uint64_t row3_vals[4] = {
        IV[4] ^ counter,
        IV[5],
        IV[6] ^ block_len,
        IV[7] ^ flags
    };
    uint64x2_t row3_lo = vld1q_u64(row3_vals + 0);
    uint64x2_t row3_hi = vld1q_u64(row3_vals + 2);

    /* Load message block (16 x 64-bit words) */
    uint64_t m[16];
    for (int i = 0; i < 16; i++) {
        const uint8_t *p = block + i * 8;
        m[i] = ((uint64_t)p[0]) |
               ((uint64_t)p[1] << 8) |
               ((uint64_t)p[2] << 16) |
               ((uint64_t)p[3] << 24) |
               ((uint64_t)p[4] << 32) |
               ((uint64_t)p[5] << 40) |
               ((uint64_t)p[6] << 48) |
               ((uint64_t)p[7] << 56);
    }

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

        /* Column G(0,4,8,12) and G(1,5,9,13) in parallel */
        {
            uint64x2_t mx = {m[s[0]], m[s[2]]};
            uint64x2_t my = {m[s[1]], m[s[3]]};
            G_NEON(&row0_lo, &row1_lo, &row2_lo, &row3_lo, mx, my);
        }

        /* Column G(2,6,10,14) and G(3,7,11,15) in parallel */
        {
            uint64x2_t mx = {m[s[4]], m[s[6]]};
            uint64x2_t my = {m[s[5]], m[s[7]]};
            G_NEON(&row0_hi, &row1_hi, &row2_hi, &row3_hi, mx, my);
        }

        /*
         * Diagonal step: G operations on diagonals
         * G(0, 5, 10, 15) with m[s[8]], m[s[9]]
         * G(1, 6, 11, 12) with m[s[10]], m[s[11]]
         * G(2, 7,  8, 13) with m[s[12]], m[s[13]]
         * G(3, 4,  9, 14) with m[s[14]], m[s[15]]
         *
         * For diagonals, we need to shuffle the rows to align the elements.
         * After column step:
         *   row0 = [0, 1, 2, 3]
         *   row1 = [4, 5, 6, 7]
         *   row2 = [8, 9, 10, 11]
         *   row3 = [12, 13, 14, 15]
         *
         * For diagonal, we need:
         *   (0, 5, 10, 15), (1, 6, 11, 12), (2, 7, 8, 13), (3, 4, 9, 14)
         *
         * Rotate row1 left by 1: [5, 6, 7, 4]
         * Rotate row2 left by 2: [10, 11, 8, 9]
         * Rotate row3 left by 3: [15, 12, 13, 14]
         */

        /* Shuffle for diagonal: rotate rows */
        /* Row 1: [4, 5] [6, 7] -> [5, 6] [7, 4] */
        uint64_t t1_lo_0 = vgetq_lane_u64(row1_lo, 1);  /* 5 */
        uint64_t t1_lo_1 = vgetq_lane_u64(row1_hi, 0);  /* 6 */
        uint64_t t1_hi_0 = vgetq_lane_u64(row1_hi, 1);  /* 7 */
        uint64_t t1_hi_1 = vgetq_lane_u64(row1_lo, 0);  /* 4 */
        uint64x2_t diag_row1_lo = vcombine_u64(vcreate_u64(t1_lo_0), vcreate_u64(t1_lo_1));
        uint64x2_t diag_row1_hi = vcombine_u64(vcreate_u64(t1_hi_0), vcreate_u64(t1_hi_1));

        /* Row 2: [8, 9] [10, 11] -> [10, 11] [8, 9] (swap halves) */
        uint64x2_t diag_row2_lo = row2_hi;  /* [10, 11] */
        uint64x2_t diag_row2_hi = row2_lo;  /* [8, 9] */

        /* Row 3: [12, 13] [14, 15] -> [15, 12] [13, 14] */
        uint64_t t3_lo_0 = vgetq_lane_u64(row3_hi, 1);  /* 15 */
        uint64_t t3_lo_1 = vgetq_lane_u64(row3_lo, 0);  /* 12 */
        uint64_t t3_hi_0 = vgetq_lane_u64(row3_lo, 1);  /* 13 */
        uint64_t t3_hi_1 = vgetq_lane_u64(row3_hi, 0);  /* 14 */
        uint64x2_t diag_row3_lo = vcombine_u64(vcreate_u64(t3_lo_0), vcreate_u64(t3_lo_1));
        uint64x2_t diag_row3_hi = vcombine_u64(vcreate_u64(t3_hi_0), vcreate_u64(t3_hi_1));

        /* Diagonal G(0,5,10,15) and G(1,6,11,12) in parallel */
        {
            uint64x2_t mx = {m[s[8]], m[s[10]]};
            uint64x2_t my = {m[s[9]], m[s[11]]};
            G_NEON(&row0_lo, &diag_row1_lo, &diag_row2_lo, &diag_row3_lo, mx, my);
        }

        /* Diagonal G(2,7,8,13) and G(3,4,9,14) in parallel */
        {
            uint64x2_t mx = {m[s[12]], m[s[14]]};
            uint64x2_t my = {m[s[13]], m[s[15]]};
            G_NEON(&row0_hi, &diag_row1_hi, &diag_row2_hi, &diag_row3_hi, mx, my);
        }

        /* Unshuffle: rotate back */
        /* Row 1: [5, 6] [7, 4] -> [4, 5] [6, 7] */
        t1_lo_0 = vgetq_lane_u64(diag_row1_hi, 1);  /* 4 */
        t1_lo_1 = vgetq_lane_u64(diag_row1_lo, 0);  /* 5 */
        t1_hi_0 = vgetq_lane_u64(diag_row1_lo, 1);  /* 6 */
        t1_hi_1 = vgetq_lane_u64(diag_row1_hi, 0);  /* 7 */
        row1_lo = vcombine_u64(vcreate_u64(t1_lo_0), vcreate_u64(t1_lo_1));
        row1_hi = vcombine_u64(vcreate_u64(t1_hi_0), vcreate_u64(t1_hi_1));

        /* Row 2: [10, 11] [8, 9] -> [8, 9] [10, 11] */
        row2_lo = diag_row2_hi;
        row2_hi = diag_row2_lo;

        /* Row 3: [15, 12] [13, 14] -> [12, 13] [14, 15] */
        t3_lo_0 = vgetq_lane_u64(diag_row3_lo, 1);  /* 12 */
        t3_lo_1 = vgetq_lane_u64(diag_row3_hi, 0);  /* 13 */
        t3_hi_0 = vgetq_lane_u64(diag_row3_hi, 1);  /* 14 */
        t3_hi_1 = vgetq_lane_u64(diag_row3_lo, 0);  /* 15 */
        row3_lo = vcombine_u64(vcreate_u64(t3_lo_0), vcreate_u64(t3_lo_1));
        row3_hi = vcombine_u64(vcreate_u64(t3_hi_0), vcreate_u64(t3_hi_1));
    }

    /* Finalize: XOR state halves */
    uint64x2_t out0 = veorq_u64(row0_lo, row2_lo);  /* [0, 1] ^ [8, 9] */
    uint64x2_t out1 = veorq_u64(row0_hi, row2_hi);  /* [2, 3] ^ [10, 11] */
    uint64x2_t out2 = veorq_u64(row1_lo, row3_lo);  /* [4, 5] ^ [12, 13] */
    uint64x2_t out3 = veorq_u64(row1_hi, row3_hi);  /* [6, 7] ^ [14, 15] */

    /* Second half: state[8..15] ^ cv[0..7] */
    uint64x2_t cv0 = vld1q_u64(cv + 0);
    uint64x2_t cv1 = vld1q_u64(cv + 2);
    uint64x2_t cv2 = vld1q_u64(cv + 4);
    uint64x2_t cv3 = vld1q_u64(cv + 6);

    uint64x2_t out4 = veorq_u64(row2_lo, cv0);
    uint64x2_t out5 = veorq_u64(row2_hi, cv1);
    uint64x2_t out6 = veorq_u64(row3_lo, cv2);
    uint64x2_t out7 = veorq_u64(row3_hi, cv3);

    /* Store output */
    vst1q_u64(out + 0, out0);
    vst1q_u64(out + 2, out1);
    vst1q_u64(out + 4, out2);
    vst1q_u64(out + 6, out3);
    vst1q_u64(out + 8, out4);
    vst1q_u64(out + 10, out5);
    vst1q_u64(out + 12, out6);
    vst1q_u64(out + 14, out7);
}

/* ============== Registration ============== */

/*
 * Constructor to register the NEON implementation on library load.
 */
__attribute__((constructor))
static void blake4_neon_init(void) {
    blake4_register_neon(blake4_compress_neon_impl);
}

#endif /* __ARM_NEON || __ARM_NEON__ */
#endif /* __aarch64__ || _M_ARM64 */
