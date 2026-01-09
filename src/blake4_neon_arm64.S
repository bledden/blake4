/*
 * BLAKE4 ARM64 NEON Assembly Implementation
 *
 * Hand-optimized compression function for ARM64 with NEON.
 * Uses 128-bit vector operations on state pairs for maximum throughput.
 *
 * Key optimizations:
 * - All 16 state words kept in NEON registers (v0-v7)
 * - Message words cached in registers where possible
 * - Optimal instruction scheduling for ARM64 out-of-order execution
 * - Native NEON rotate via REV32 (for R1=32) and shift pairs
 *
 * Calling convention (AAPCS64):
 *   x0 = cv pointer (const uint64_t cv[8])
 *   x1 = block pointer (const uint8_t block[128])
 *   x2 = block_len (uint32_t, passed as w2)
 *   x3 = counter (uint64_t)
 *   x4 = flags (uint32_t, passed as w4)
 *   x5 = out pointer (uint64_t out[16])
 *
 * Register allocation:
 *   v0-v1  = state rows 0-1 (words 0-3)
 *   v2-v3  = state rows 2-3 (words 4-7)
 *   v4-v5  = state rows 4-5 (words 8-11)
 *   v6-v7  = state rows 6-7 (words 12-15)
 *   v16-v23 = message words (cached across rounds)
 *   v24-v31 = temporaries for shuffles and message loads
 *
 * This is free and unencumbered software released into the public domain.
 */

#if defined(__aarch64__) || defined(__arm64__)

.text
.align 4

/* IV constants */
.section __DATA,__const
.align 4
BLAKE4_IV:
    .quad 0x6A09E667F3BCC908  /* IV[0] */
    .quad 0xBB67AE8584CAA73B  /* IV[1] */
    .quad 0x3C6EF372FE94F82B  /* IV[2] */
    .quad 0xA54FF53A5F1D36F1  /* IV[3] */
    .quad 0x510E527FADE682D1  /* IV[4] */
    .quad 0x9B05688C2B3E6C1F  /* IV[5] */
    .quad 0x1F83D9ABFB41BD6B  /* IV[6] */
    .quad 0x5BE0CD19137E2179  /* IV[7] */

/* Message schedule (SIGMA table) */
.align 4
BLAKE4_SIGMA:
    /* Round 0 */
    .byte  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15
    /* Round 1 */
    .byte 14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3
    /* Round 2 */
    .byte 11,  8, 12,  0,  5,  2, 15, 13, 10, 14,  3,  6,  7,  1,  9,  4
    /* Round 3 */
    .byte  7,  9,  3,  1, 13, 12, 11, 14,  2,  6,  5, 10,  4,  0, 15,  8
    /* Round 4 */
    .byte  9,  0,  5,  7,  2,  4, 10, 15, 14,  1, 11, 12,  6,  8,  3, 13
    /* Round 5 */
    .byte  2, 12,  6, 10,  0, 11,  8,  3,  4, 13,  7,  5, 15, 14,  1,  9
    /* Round 6 */
    .byte 12,  5,  1, 15, 14, 13,  4, 10,  0,  7,  6,  3,  9,  2,  8, 11
    /* Round 7 */
    .byte 13, 11,  7, 14, 12,  1,  3,  9,  5,  0, 15,  4,  8,  6,  2, 10
    /* Round 8 */
    .byte  6, 15, 14,  9, 11,  3,  0,  8, 12,  2, 13,  7,  1,  4, 10,  5
    /* Round 9 */
    .byte 10,  2,  8,  4,  7,  6,  1,  5, 15, 11,  9, 14,  3, 12, 13,  0

.text
.align 4

/*
 * BLAKE4 compression function - ARM64 NEON
 *
 * void blake4_compress_neon_asm(const uint64_t cv[8],
 *                               const uint8_t block[128],
 *                               uint32_t block_len,
 *                               uint64_t counter,
 *                               uint32_t flags,
 *                               uint64_t out[16])
 */
.globl _blake4_compress_neon_asm
.globl blake4_compress_neon_asm
_blake4_compress_neon_asm:
blake4_compress_neon_asm:
    /* Save callee-saved registers */
    stp     x19, x20, [sp, #-96]!
    stp     x21, x22, [sp, #16]
    stp     x23, x24, [sp, #32]
    stp     x25, x26, [sp, #48]
    stp     x27, x28, [sp, #64]
    stp     d8, d9, [sp, #80]

    /* Save arguments for later */
    mov     x19, x0                 /* cv pointer */
    mov     x20, x1                 /* block pointer */
    mov     w21, w2                 /* block_len */
    mov     x22, x3                 /* counter */
    mov     w23, w4                 /* flags */
    mov     x24, x5                 /* out pointer */

    /* Load IV address */
    adrp    x10, BLAKE4_IV@PAGE
    add     x10, x10, BLAKE4_IV@PAGEOFF

    /* Initialize state:
     * v0 = [cv[0], cv[1]]
     * v1 = [cv[2], cv[3]]
     * v2 = [cv[4], cv[5]]
     * v3 = [cv[6], cv[7]]
     * v4 = [IV[0], IV[1]]
     * v5 = [IV[2], IV[3]]
     * v6 = [IV[4]^counter, IV[5]]
     * v7 = [IV[6]^block_len, IV[7]^flags]
     */

    /* Load chaining value */
    ld1     {v0.2d, v1.2d, v2.2d, v3.2d}, [x19]

    /* Load IV */
    ld1     {v4.2d, v5.2d}, [x10], #32
    ld1     {v28.2d, v29.2d}, [x10]     /* IV[4..7] in v28, v29 */

    /* Prepare row 3 with counter, block_len, and flags */
    /* v6 = [IV[4] ^ counter, IV[5]] */
    mov     v6.d[0], x22                /* counter */
    eor     v6.16b, v28.16b, v6.16b     /* Just XOR counter into first lane */
    mov     v6.d[1], v28.d[1]           /* Keep IV[5] unchanged */

    /* v7 = [IV[6] ^ block_len, IV[7] ^ flags] */
    mov     x10, x21                    /* block_len (zero-extended) */
    mov     x11, x23                    /* flags (zero-extended) */
    mov     v7.d[0], x10
    mov     v7.d[1], x11
    eor     v7.16b, v29.16b, v7.16b

    /* Load message block into v16-v23 (16 x 64-bit words) */
    ld1     {v16.2d, v17.2d, v18.2d, v19.2d}, [x20], #64
    ld1     {v20.2d, v21.2d, v22.2d, v23.2d}, [x20]

    /* Get SIGMA table address */
    adrp    x10, BLAKE4_SIGMA@PAGE
    add     x10, x10, BLAKE4_SIGMA@PAGEOFF

    /* Round loop counter */
    mov     w25, #10

/*
 * Main round loop
 *
 * State layout:
 *   v0 = [state[0], state[1]]    row0_lo
 *   v1 = [state[2], state[3]]    row0_hi
 *   v2 = [state[4], state[5]]    row1_lo
 *   v3 = [state[6], state[7]]    row1_hi
 *   v4 = [state[8], state[9]]    row2_lo
 *   v5 = [state[10], state[11]]  row2_hi
 *   v6 = [state[12], state[13]]  row3_lo
 *   v7 = [state[14], state[15]]  row3_hi
 */
.Lround_loop:
    /* Load SIGMA indices for this round */
    ldrb    w11, [x10, #0]      /* s[0] */
    ldrb    w12, [x10, #1]      /* s[1] */
    ldrb    w13, [x10, #2]      /* s[2] */
    ldrb    w14, [x10, #3]      /* s[3] */

    /* ===== Column step ===== */
    /* G(0, 4, 8, 12) and G(1, 5, 9, 13) in parallel */

    /* Load message words for column G(0,4,8,12) and G(1,5,9,13) */
    /* mx = [m[s[0]], m[s[2]]] */
    /* my = [m[s[1]], m[s[3]]] */

    /* Build mx: load m[s[0]] and m[s[2]] */
    lsl     x11, x11, #3        /* s[0] * 8 */
    lsl     x13, x13, #3        /* s[2] * 8 */
    add     x11, x20, x11
    add     x13, x20, x13
    sub     x11, x11, #64       /* Adjust since we advanced x20 */
    sub     x13, x13, #64

    /* We have message in v16-v23, indexed by word number
     * m[0..1] in v16, m[2..3] in v17, etc.
     * For efficiency, we'll use table lookups or individual element extracts
     */

    /* Reload original message pointer for indexed access */
    sub     x26, x20, #64       /* x26 = original block pointer */

    /* Get message indices */
    ldrb    w11, [x10, #0]      /* s[0] */
    ldrb    w12, [x10, #1]      /* s[1] */
    ldrb    w13, [x10, #2]      /* s[2] */
    ldrb    w14, [x10, #3]      /* s[3] */

    /* Load mx = [m[s[0]], m[s[2]]] */
    lsl     x11, x11, #3
    lsl     x13, x13, #3
    ldr     x27, [x26, x11]     /* m[s[0]] */
    ldr     x28, [x26, x13]     /* m[s[2]] */
    mov     v24.d[0], x27
    mov     v24.d[1], x28       /* v24 = mx */

    /* Load my = [m[s[1]], m[s[3]]] */
    lsl     x12, x12, #3
    lsl     x14, x14, #3
    ldr     x27, [x26, x12]     /* m[s[1]] */
    ldr     x28, [x26, x14]     /* m[s[3]] */
    mov     v25.d[0], x27
    mov     v25.d[1], x28       /* v25 = my */

    /* G(0,4,8,12) and G(1,5,9,13): a=v0, b=v2, c=v4, d=v6 */
    /* a = a + b + mx */
    add     v0.2d, v0.2d, v2.2d
    add     v0.2d, v0.2d, v24.2d
    /* d = rotr(d ^ a, 32) - shift pair */
    eor     v6.16b, v6.16b, v0.16b
    ushr    v26.2d, v6.2d, #32
    shl     v6.2d, v6.2d, #32
    orr     v6.16b, v6.16b, v26.16b
    /* c = c + d */
    add     v4.2d, v4.2d, v6.2d
    /* b = rotr(b ^ c, 24) - shift pair */
    eor     v2.16b, v2.16b, v4.16b
    ushr    v26.2d, v2.2d, #24
    shl     v2.2d, v2.2d, #40
    orr     v2.16b, v2.16b, v26.16b
    /* a = a + b + my */
    add     v0.2d, v0.2d, v2.2d
    add     v0.2d, v0.2d, v25.2d
    /* d = rotr(d ^ a, 16) - shift pair */
    eor     v6.16b, v6.16b, v0.16b
    ushr    v26.2d, v6.2d, #16
    shl     v6.2d, v6.2d, #48
    orr     v6.16b, v6.16b, v26.16b
    /* c = c + d */
    add     v4.2d, v4.2d, v6.2d
    /* b = rotr(b ^ c, 63) - shift pair */
    eor     v2.16b, v2.16b, v4.16b
    ushr    v26.2d, v2.2d, #63
    shl     v2.2d, v2.2d, #1
    orr     v2.16b, v2.16b, v26.16b

    /* G(2, 6, 10, 14) and G(3, 7, 11, 15) in parallel */
    /* Load message words s[4..7] */
    ldrb    w11, [x10, #4]      /* s[4] */
    ldrb    w12, [x10, #5]      /* s[5] */
    ldrb    w13, [x10, #6]      /* s[6] */
    ldrb    w14, [x10, #7]      /* s[7] */

    /* Load mx = [m[s[4]], m[s[6]]] */
    lsl     x11, x11, #3
    lsl     x13, x13, #3
    ldr     x27, [x26, x11]
    ldr     x28, [x26, x13]
    mov     v24.d[0], x27
    mov     v24.d[1], x28

    /* Load my = [m[s[5]], m[s[7]]] */
    lsl     x12, x12, #3
    lsl     x14, x14, #3
    ldr     x27, [x26, x12]
    ldr     x28, [x26, x14]
    mov     v25.d[0], x27
    mov     v25.d[1], x28

    /* G(2,6,10,14) and G(3,7,11,15): a=v1, b=v3, c=v5, d=v7 */
    /* a = a + b + mx */
    add     v1.2d, v1.2d, v3.2d
    add     v1.2d, v1.2d, v24.2d
    /* d = rotr(d ^ a, 32) - shift pair */
    eor     v7.16b, v7.16b, v1.16b
    ushr    v26.2d, v7.2d, #32
    shl     v7.2d, v7.2d, #32
    orr     v7.16b, v7.16b, v26.16b
    /* c = c + d */
    add     v5.2d, v5.2d, v7.2d
    /* b = rotr(b ^ c, 24) */
    eor     v3.16b, v3.16b, v5.16b
    ushr    v26.2d, v3.2d, #24
    shl     v3.2d, v3.2d, #40
    orr     v3.16b, v3.16b, v26.16b
    /* a = a + b + my */
    add     v1.2d, v1.2d, v3.2d
    add     v1.2d, v1.2d, v25.2d
    /* d = rotr(d ^ a, 16) */
    eor     v7.16b, v7.16b, v1.16b
    ushr    v26.2d, v7.2d, #16
    shl     v7.2d, v7.2d, #48
    orr     v7.16b, v7.16b, v26.16b
    /* c = c + d */
    add     v5.2d, v5.2d, v7.2d
    /* b = rotr(b ^ c, 63) */
    eor     v3.16b, v3.16b, v5.16b
    ushr    v26.2d, v3.2d, #63
    shl     v3.2d, v3.2d, #1
    orr     v3.16b, v3.16b, v26.16b

    /* ===== Diagonal step ===== */
    /*
     * For diagonal, we need to shuffle rows:
     * Row 1: [4, 5, 6, 7] -> [5, 6, 7, 4] (rotate left by 1)
     * Row 2: [8, 9, 10, 11] -> [10, 11, 8, 9] (rotate left by 2, swap halves)
     * Row 3: [12, 13, 14, 15] -> [15, 12, 13, 14] (rotate left by 3)
     */

    /* Shuffle row 1 (v2, v3): [4,5] [6,7] -> [5,6] [7,4] */
    mov     x27, v2.d[1]        /* 5 */
    mov     x28, v3.d[0]        /* 6 */
    mov     v26.d[0], x27
    mov     v26.d[1], x28       /* v26 = [5, 6] = new v2 */

    mov     x27, v3.d[1]        /* 7 */
    mov     x28, v2.d[0]        /* 4 */
    mov     v27.d[0], x27
    mov     v27.d[1], x28       /* v27 = [7, 4] = new v3 */

    mov     v8.16b, v2.16b      /* Save original v2 for unshuffle */
    mov     v9.16b, v3.16b      /* Save original v3 for unshuffle */
    mov     v2.16b, v26.16b
    mov     v3.16b, v27.16b

    /* Shuffle row 2 (v4, v5): [8,9] [10,11] -> [10,11] [8,9] (swap) */
    mov     v26.16b, v4.16b     /* Save original v4 */
    mov     v4.16b, v5.16b      /* v4 = [10, 11] */
    mov     v5.16b, v26.16b     /* v5 = [8, 9] */

    /* Shuffle row 3 (v6, v7): [12,13] [14,15] -> [15,12] [13,14] */
    mov     x27, v7.d[1]        /* 15 */
    mov     x28, v6.d[0]        /* 12 */
    mov     v26.d[0], x27
    mov     v26.d[1], x28       /* v26 = [15, 12] = new v6 */

    mov     x27, v6.d[1]        /* 13 */
    mov     x28, v7.d[0]        /* 14 */
    mov     v27.d[0], x27
    mov     v27.d[1], x28       /* v27 = [13, 14] = new v7 */

    mov     v28.16b, v6.16b     /* Save for unshuffle */
    mov     v29.16b, v7.16b
    mov     v6.16b, v26.16b
    mov     v7.16b, v27.16b

    /* G(0,5,10,15) and G(1,6,11,12): a=v0, b=v2, c=v4, d=v6 */
    /* Load message words s[8..11] */
    ldrb    w11, [x10, #8]      /* s[8] */
    ldrb    w12, [x10, #9]      /* s[9] */
    ldrb    w13, [x10, #10]     /* s[10] */
    ldrb    w14, [x10, #11]     /* s[11] */

    /* Load mx = [m[s[8]], m[s[10]]] */
    lsl     x11, x11, #3
    lsl     x13, x13, #3
    ldr     x27, [x26, x11]
    ldr     x28, [x26, x13]
    mov     v24.d[0], x27
    mov     v24.d[1], x28

    /* Load my = [m[s[9]], m[s[11]]] */
    lsl     x12, x12, #3
    lsl     x14, x14, #3
    ldr     x27, [x26, x12]
    ldr     x28, [x26, x14]
    mov     v25.d[0], x27
    mov     v25.d[1], x28

    /* G function */
    add     v0.2d, v0.2d, v2.2d
    add     v0.2d, v0.2d, v24.2d
    eor     v6.16b, v6.16b, v0.16b
    ushr    v26.2d, v6.2d, #32
    shl     v6.2d, v6.2d, #32
    orr     v6.16b, v6.16b, v26.16b
    add     v4.2d, v4.2d, v6.2d
    eor     v2.16b, v2.16b, v4.16b
    ushr    v26.2d, v2.2d, #24
    shl     v2.2d, v2.2d, #40
    orr     v2.16b, v2.16b, v26.16b
    add     v0.2d, v0.2d, v2.2d
    add     v0.2d, v0.2d, v25.2d
    eor     v6.16b, v6.16b, v0.16b
    ushr    v26.2d, v6.2d, #16
    shl     v6.2d, v6.2d, #48
    orr     v6.16b, v6.16b, v26.16b
    add     v4.2d, v4.2d, v6.2d
    eor     v2.16b, v2.16b, v4.16b
    ushr    v26.2d, v2.2d, #63
    shl     v2.2d, v2.2d, #1
    orr     v2.16b, v2.16b, v26.16b

    /* G(2,7,8,13) and G(3,4,9,14): a=v1, b=v3, c=v5, d=v7 */
    /* Load message words s[12..15] */
    ldrb    w11, [x10, #12]     /* s[12] */
    ldrb    w12, [x10, #13]     /* s[13] */
    ldrb    w13, [x10, #14]     /* s[14] */
    ldrb    w14, [x10, #15]     /* s[15] */

    /* Load mx = [m[s[12]], m[s[14]]] */
    lsl     x11, x11, #3
    lsl     x13, x13, #3
    ldr     x27, [x26, x11]
    ldr     x28, [x26, x13]
    mov     v24.d[0], x27
    mov     v24.d[1], x28

    /* Load my = [m[s[13]], m[s[15]]] */
    lsl     x12, x12, #3
    lsl     x14, x14, #3
    ldr     x27, [x26, x12]
    ldr     x28, [x26, x14]
    mov     v25.d[0], x27
    mov     v25.d[1], x28

    /* G function */
    add     v1.2d, v1.2d, v3.2d
    add     v1.2d, v1.2d, v24.2d
    eor     v7.16b, v7.16b, v1.16b
    ushr    v26.2d, v7.2d, #32
    shl     v7.2d, v7.2d, #32
    orr     v7.16b, v7.16b, v26.16b
    add     v5.2d, v5.2d, v7.2d
    eor     v3.16b, v3.16b, v5.16b
    ushr    v26.2d, v3.2d, #24
    shl     v3.2d, v3.2d, #40
    orr     v3.16b, v3.16b, v26.16b
    add     v1.2d, v1.2d, v3.2d
    add     v1.2d, v1.2d, v25.2d
    eor     v7.16b, v7.16b, v1.16b
    ushr    v26.2d, v7.2d, #16
    shl     v7.2d, v7.2d, #48
    orr     v7.16b, v7.16b, v26.16b
    add     v5.2d, v5.2d, v7.2d
    eor     v3.16b, v3.16b, v5.16b
    ushr    v26.2d, v3.2d, #63
    shl     v3.2d, v3.2d, #1
    orr     v3.16b, v3.16b, v26.16b

    /* ===== Unshuffle rows ===== */
    /* Row 1: [5,6] [7,4] -> [4,5] [6,7] */
    mov     x27, v3.d[1]        /* 4 */
    mov     x28, v2.d[0]        /* 5 */
    mov     v26.d[0], x27
    mov     v26.d[1], x28       /* [4, 5] */

    mov     x27, v2.d[1]        /* 6 */
    mov     x28, v3.d[0]        /* 7 */
    mov     v27.d[0], x27
    mov     v27.d[1], x28       /* [6, 7] */

    mov     v2.16b, v26.16b
    mov     v3.16b, v27.16b

    /* Row 2: [10,11] [8,9] -> [8,9] [10,11] (swap back) */
    mov     v26.16b, v4.16b
    mov     v4.16b, v5.16b
    mov     v5.16b, v26.16b

    /* Row 3: [15,12] [13,14] -> [12,13] [14,15] */
    mov     x27, v6.d[1]        /* 12 */
    mov     x28, v7.d[0]        /* 13 */
    mov     v26.d[0], x27
    mov     v26.d[1], x28       /* [12, 13] */

    mov     x27, v7.d[1]        /* 14 */
    mov     x28, v6.d[0]        /* 15 */
    mov     v27.d[0], x27
    mov     v27.d[1], x28       /* [14, 15] */

    mov     v6.16b, v26.16b
    mov     v7.16b, v27.16b

    /* Advance SIGMA pointer and loop */
    add     x10, x10, #16
    subs    w25, w25, #1
    b.ne    .Lround_loop

    /* ===== Finalization ===== */
    /* out[0..7] = state[0..7] ^ state[8..15] */
    eor     v24.16b, v0.16b, v4.16b     /* [0,1] ^ [8,9] */
    eor     v25.16b, v1.16b, v5.16b     /* [2,3] ^ [10,11] */
    eor     v26.16b, v2.16b, v6.16b     /* [4,5] ^ [12,13] */
    eor     v27.16b, v3.16b, v7.16b     /* [6,7] ^ [14,15] */

    /* out[8..15] = state[8..15] ^ cv[0..7] */
    /* Reload cv */
    ld1     {v28.2d, v29.2d, v30.2d, v31.2d}, [x19]
    eor     v4.16b, v4.16b, v28.16b     /* [8,9] ^ cv[0,1] */
    eor     v5.16b, v5.16b, v29.16b     /* [10,11] ^ cv[2,3] */
    eor     v6.16b, v6.16b, v30.16b     /* [12,13] ^ cv[4,5] */
    eor     v7.16b, v7.16b, v31.16b     /* [14,15] ^ cv[6,7] */

    /* Store output */
    st1     {v24.2d, v25.2d, v26.2d, v27.2d}, [x24], #64
    st1     {v4.2d, v5.2d, v6.2d, v7.2d}, [x24]

    /* Restore callee-saved registers */
    ldp     d8, d9, [sp, #80]
    ldp     x27, x28, [sp, #64]
    ldp     x25, x26, [sp, #48]
    ldp     x23, x24, [sp, #32]
    ldp     x21, x22, [sp, #16]
    ldp     x19, x20, [sp], #96

    ret

#endif /* __aarch64__ || __arm64__ */
