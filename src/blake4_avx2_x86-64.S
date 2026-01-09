/*
 * BLAKE4 AVX2 Assembly Implementation (Unix/GNU as)
 *
 * Hand-optimized compression function for x86-64 with AVX2.
 * Uses 256-bit YMM registers to process 4 G functions in parallel.
 *
 * Key optimizations:
 * - Emulated 64-bit rotates using shift pairs (no vprorq in AVX2)
 * - Full state in YMM registers (ymm0-ymm3)
 * - Shuffle-based 32-bit rotate optimization
 * - Optimal instruction scheduling for modern x86-64
 *
 * Calling convention (System V AMD64 ABI):
 *   rdi = cv pointer (const uint64_t cv[8])
 *   rsi = block pointer (const uint8_t block[128])
 *   rdx = block_len (uint32_t, zero-extended in edx)
 *   rcx = counter (uint64_t)
 *   r8d = flags (uint32_t)
 *   r9  = out pointer (uint64_t out[16])
 *
 * Register allocation:
 *   ymm0  = state row 0 (words 0-3)
 *   ymm1  = state row 1 (words 4-7)
 *   ymm2  = state row 2 (words 8-11)
 *   ymm3  = state row 3 (words 12-15)
 *   ymm4-5 = message words mx, my
 *   ymm6-15 = temporaries
 *
 * This is free and unencumbered software released into the public domain.
 */

#if defined(__x86_64__) || defined(_M_X64)

.intel_syntax noprefix

/* Section for read-only data */
.section .rodata
.align 64

/* IV constants */
BLAKE4_IV_AVX2:
    .quad 0x6A09E667F3BCC908  /* IV[0] */
    .quad 0xBB67AE8584CAA73B  /* IV[1] */
    .quad 0x3C6EF372FE94F82B  /* IV[2] */
    .quad 0xA54FF53A5F1D36F1  /* IV[3] */
    .quad 0x510E527FADE682D1  /* IV[4] */
    .quad 0x9B05688C2B3E6C1F  /* IV[5] */
    .quad 0x1F83D9ABFB41BD6B  /* IV[6] */
    .quad 0x5BE0CD19137E2179  /* IV[7] */

/* Message schedule (SIGMA table) */
BLAKE4_SIGMA_AVX2:
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
.align 16

/*
 * BLAKE4 compression function - AVX2
 *
 * void blake4_compress_avx2_asm(const uint64_t cv[8],
 *                                const uint8_t block[128],
 *                                uint32_t block_len,
 *                                uint64_t counter,
 *                                uint32_t flags,
 *                                uint64_t out[16])
 */
.globl blake4_compress_avx2_asm
.type blake4_compress_avx2_asm, @function
blake4_compress_avx2_asm:
    /* Save callee-saved registers */
    push    rbx
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    mov     rbp, rsp
    and     rsp, -32                /* Align stack to 32 bytes */
    sub     rsp, 192                /* Space for message words + saved regs */

    /* Save input arguments */
    mov     r12, rdi                /* cv */
    mov     r13, rsi                /* block */
    mov     r14d, edx               /* block_len */
    mov     r15, rcx                /* counter */
    mov     ebx, r8d                /* flags */
    mov     [rsp+160], r9           /* out */

    /* Load IV address */
    lea     rax, [rip + BLAKE4_IV_AVX2]

    /* Initialize state:
     * ymm0 = [cv[0], cv[1], cv[2], cv[3]]
     * ymm1 = [cv[4], cv[5], cv[6], cv[7]]
     * ymm2 = [IV[0], IV[1], IV[2], IV[3]]
     * ymm3 = [IV[4]^counter, IV[5], IV[6]^block_len, IV[7]^flags]
     */

    /* Load chaining value */
    vmovdqu ymm0, [r12]              /* cv[0..3] */
    vmovdqu ymm1, [r12 + 32]         /* cv[4..7] */

    /* Load IV */
    vmovdqa ymm2, [rax]              /* IV[0..3] */

    /* Build row 3: IV[4..7] with XOR tweaks */
    vmovdqa ymm3, [rax + 32]         /* IV[4..7] */

    /* XOR counter into IV[4] (first element) */
    vmovq   xmm4, r15                /* counter in xmm4[0] */
    vpxor   xmm5, xmm3, xmm4         /* XOR counter into first qword */
    vpblendd ymm3, ymm3, ymm5, 0x03  /* Replace first qword */

    /* XOR block_len into IV[6] (third element, index 2) */
    mov     eax, r14d
    vmovq   xmm4, rax
    vpermq  ymm4, ymm4, 0x00         /* Broadcast to all lanes */
    vpand   ymm4, ymm4, [rip + .Lmask_idx2_avx2]  /* Mask to only index 2 */
    vpxor   ymm3, ymm3, ymm4

    /* XOR flags into IV[7] (fourth element, index 3) */
    mov     eax, ebx
    vmovq   xmm4, rax
    vpermq  ymm4, ymm4, 0x00         /* Broadcast to all lanes */
    vpand   ymm4, ymm4, [rip + .Lmask_idx3_avx2]  /* Mask to only index 3 */
    vpxor   ymm3, ymm3, ymm4

    /* Load message block into memory for indexed access */
    vmovdqu ymm8, [r13]
    vmovdqu ymm9, [r13 + 32]
    vmovdqu ymm10, [r13 + 64]
    vmovdqu ymm11, [r13 + 96]
    vmovdqu [rsp], ymm8              /* m[0..3] */
    vmovdqu [rsp + 32], ymm9         /* m[4..7] */
    vmovdqu [rsp + 64], ymm10        /* m[8..11] */
    vmovdqu [rsp + 96], ymm11        /* m[12..15] */

    /* Get SIGMA table address */
    lea     r10, [rip + BLAKE4_SIGMA_AVX2]

    /* Round loop counter */
    mov     ecx, 10

.Lround_loop_avx2:
    /* ===== Column step ===== */
    /* Load SIGMA indices and build message vectors */
    movzx   eax, byte ptr [r10]      /* s[0] */
    movzx   r8d, byte ptr [r10 + 2]  /* s[2] */
    movzx   r9d, byte ptr [r10 + 4]  /* s[4] */
    movzx   r11d, byte ptr [r10 + 6] /* s[6] */
    shl     rax, 3
    shl     r8, 3
    shl     r9, 3
    shl     r11, 3
    vmovq   xmm4, [rsp + rax]
    vmovq   xmm5, [rsp + r8]
    vpunpcklqdq xmm4, xmm4, xmm5
    vmovq   xmm6, [rsp + r9]
    vmovq   xmm7, [rsp + r11]
    vpunpcklqdq xmm6, xmm6, xmm7
    vinserti128 ymm4, ymm4, xmm6, 1  /* ymm4 = mx */

    movzx   eax, byte ptr [r10 + 1]  /* s[1] */
    movzx   r8d, byte ptr [r10 + 3]  /* s[3] */
    movzx   r9d, byte ptr [r10 + 5]  /* s[5] */
    movzx   r11d, byte ptr [r10 + 7] /* s[7] */
    shl     rax, 3
    shl     r8, 3
    shl     r9, 3
    shl     r11, 3
    vmovq   xmm5, [rsp + rax]
    vmovq   xmm6, [rsp + r8]
    vpunpcklqdq xmm5, xmm5, xmm6
    vmovq   xmm6, [rsp + r9]
    vmovq   xmm7, [rsp + r11]
    vpunpcklqdq xmm6, xmm6, xmm7
    vinserti128 ymm5, ymm5, xmm6, 1  /* ymm5 = my */

    /* G function: a=ymm0, b=ymm1, c=ymm2, d=ymm3, mx=ymm4, my=ymm5 */
    /* a = a + b + mx */
    vpaddq  ymm0, ymm0, ymm1
    vpaddq  ymm0, ymm0, ymm4
    /* d = rotr(d ^ a, 32) - use shuffle (swap 32-bit halves within each 64-bit) */
    vpxor   ymm3, ymm3, ymm0
    vpshufd ymm3, ymm3, 0xB1         /* _MM_SHUFFLE(2,3,0,1) swaps 32-bit pairs */
    /* c = c + d */
    vpaddq  ymm2, ymm2, ymm3
    /* b = rotr(b ^ c, 24) - shift pair */
    vpxor   ymm1, ymm1, ymm2
    vpsrlq  ymm6, ymm1, 24
    vpsllq  ymm1, ymm1, 40
    vpor    ymm1, ymm1, ymm6
    /* a = a + b + my */
    vpaddq  ymm0, ymm0, ymm1
    vpaddq  ymm0, ymm0, ymm5
    /* d = rotr(d ^ a, 16) - shift pair */
    vpxor   ymm3, ymm3, ymm0
    vpsrlq  ymm6, ymm3, 16
    vpsllq  ymm3, ymm3, 48
    vpor    ymm3, ymm3, ymm6
    /* c = c + d */
    vpaddq  ymm2, ymm2, ymm3
    /* b = rotr(b ^ c, 63) - shift pair */
    vpxor   ymm1, ymm1, ymm2
    vpsrlq  ymm6, ymm1, 63
    vpsllq  ymm1, ymm1, 1
    vpor    ymm1, ymm1, ymm6

    /* ===== Diagonal step ===== */
    /* Shuffle rows for diagonal */
    vpermq  ymm1, ymm1, 0x39         /* [4,5,6,7] -> [5,6,7,4] */
    vpermq  ymm2, ymm2, 0x4E         /* [8,9,10,11] -> [10,11,8,9] */
    vpermq  ymm3, ymm3, 0x93         /* [12,13,14,15] -> [15,12,13,14] */

    /* Load message words for diagonal step */
    movzx   eax, byte ptr [r10 + 8]
    movzx   r8d, byte ptr [r10 + 10]
    movzx   r9d, byte ptr [r10 + 12]
    movzx   r11d, byte ptr [r10 + 14]
    shl     rax, 3
    shl     r8, 3
    shl     r9, 3
    shl     r11, 3
    vmovq   xmm4, [rsp + rax]
    vmovq   xmm5, [rsp + r8]
    vpunpcklqdq xmm4, xmm4, xmm5
    vmovq   xmm6, [rsp + r9]
    vmovq   xmm7, [rsp + r11]
    vpunpcklqdq xmm6, xmm6, xmm7
    vinserti128 ymm4, ymm4, xmm6, 1

    movzx   eax, byte ptr [r10 + 9]
    movzx   r8d, byte ptr [r10 + 11]
    movzx   r9d, byte ptr [r10 + 13]
    movzx   r11d, byte ptr [r10 + 15]
    shl     rax, 3
    shl     r8, 3
    shl     r9, 3
    shl     r11, 3
    vmovq   xmm5, [rsp + rax]
    vmovq   xmm6, [rsp + r8]
    vpunpcklqdq xmm5, xmm5, xmm6
    vmovq   xmm6, [rsp + r9]
    vmovq   xmm7, [rsp + r11]
    vpunpcklqdq xmm6, xmm6, xmm7
    vinserti128 ymm5, ymm5, xmm6, 1

    /* G function for diagonal */
    vpaddq  ymm0, ymm0, ymm1
    vpaddq  ymm0, ymm0, ymm4
    vpxor   ymm3, ymm3, ymm0
    vpshufd ymm3, ymm3, 0xB1
    vpaddq  ymm2, ymm2, ymm3
    vpxor   ymm1, ymm1, ymm2
    vpsrlq  ymm6, ymm1, 24
    vpsllq  ymm1, ymm1, 40
    vpor    ymm1, ymm1, ymm6
    vpaddq  ymm0, ymm0, ymm1
    vpaddq  ymm0, ymm0, ymm5
    vpxor   ymm3, ymm3, ymm0
    vpsrlq  ymm6, ymm3, 16
    vpsllq  ymm3, ymm3, 48
    vpor    ymm3, ymm3, ymm6
    vpaddq  ymm2, ymm2, ymm3
    vpxor   ymm1, ymm1, ymm2
    vpsrlq  ymm6, ymm1, 63
    vpsllq  ymm1, ymm1, 1
    vpor    ymm1, ymm1, ymm6

    /* Unshuffle rows back */
    vpermq  ymm1, ymm1, 0x93         /* [5,6,7,4] -> [4,5,6,7] */
    vpermq  ymm2, ymm2, 0x4E         /* [10,11,8,9] -> [8,9,10,11] */
    vpermq  ymm3, ymm3, 0x39         /* [15,12,13,14] -> [12,13,14,15] */

    /* Advance SIGMA pointer and loop */
    add     r10, 16
    dec     ecx
    jnz     .Lround_loop_avx2

    /* ===== Finalization ===== */
    vpxor   ymm4, ymm0, ymm2         /* out[0..3] */
    vpxor   ymm5, ymm1, ymm3         /* out[4..7] */

    vmovdqu ymm6, [r12]
    vmovdqu ymm7, [r12 + 32]
    vpxor   ymm6, ymm2, ymm6         /* out[8..11] */
    vpxor   ymm7, ymm3, ymm7         /* out[12..15] */

    /* Store output */
    mov     rax, [rsp + 160]
    vmovdqu [rax], ymm4
    vmovdqu [rax + 32], ymm5
    vmovdqu [rax + 64], ymm6
    vmovdqu [rax + 96], ymm7

    /* Clear sensitive data from YMM registers */
    vzeroupper

    /* Restore stack and callee-saved registers */
    mov     rsp, rbp
    pop     rbp
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    ret

.size blake4_compress_avx2_asm, .-blake4_compress_avx2_asm

/* Mask constants */
.section .rodata
.align 32
.Lmask_idx2_avx2:
    .quad 0
    .quad 0
    .quad 0xFFFFFFFFFFFFFFFF
    .quad 0

.Lmask_idx3_avx2:
    .quad 0
    .quad 0
    .quad 0
    .quad 0xFFFFFFFFFFFFFFFF

#endif /* __x86_64__ || _M_X64 */
