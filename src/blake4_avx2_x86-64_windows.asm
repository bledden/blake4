;
; BLAKE4 AVX2 Assembly Implementation (Windows MASM)
;
; Hand-optimized compression function for x86-64 with AVX2.
; Uses 256-bit YMM registers to process 4 G functions in parallel.
; 64-bit rotates are emulated using shift pairs (AVX2 lacks native 64-bit rotate).
;
; Calling convention (Windows x64):
;   rcx = cv pointer (const uint64_t cv[8])
;   rdx = block pointer (const uint8_t block[128])
;   r8d = block_len (uint32_t)
;   r9  = counter (uint64_t)
;   [rsp+40] = flags (uint32_t)
;   [rsp+48] = out pointer (uint64_t out[16])
;
; Register allocation:
;   ymm0  = state row 0 (words 0-3)
;   ymm1  = state row 1 (words 4-7)
;   ymm2  = state row 2 (words 8-11)
;   ymm3  = state row 3 (words 12-15)
;   ymm4-5 = message words mx, my
;   ymm6-15 = temporaries
;
; This is free and unencumbered software released into the public domain.
;

.code

; IV constants
ALIGN 64
BLAKE4_IV_AVX2_WIN:
    DQ 06A09E667F3BCC908h  ; IV[0]
    DQ 0BB67AE8584CAA73Bh  ; IV[1]
    DQ 03C6EF372FE94F82Bh  ; IV[2]
    DQ 0A54FF53A5F1D36F1h  ; IV[3]
    DQ 0510E527FADE682D1h  ; IV[4]
    DQ 09B05688C2B3E6C1Fh  ; IV[5]
    DQ 01F83D9ABFB41BD6Bh  ; IV[6]
    DQ 05BE0CD19137E2179h  ; IV[7]

; SIGMA table
ALIGN 16
BLAKE4_SIGMA_AVX2_WIN:
    ; Round 0
    DB  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15
    ; Round 1
    DB 14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3
    ; Round 2
    DB 11,  8, 12,  0,  5,  2, 15, 13, 10, 14,  3,  6,  7,  1,  9,  4
    ; Round 3
    DB  7,  9,  3,  1, 13, 12, 11, 14,  2,  6,  5, 10,  4,  0, 15,  8
    ; Round 4
    DB  9,  0,  5,  7,  2,  4, 10, 15, 14,  1, 11, 12,  6,  8,  3, 13
    ; Round 5
    DB  2, 12,  6, 10,  0, 11,  8,  3,  4, 13,  7,  5, 15, 14,  1,  9
    ; Round 6
    DB 12,  5,  1, 15, 14, 13,  4, 10,  0,  7,  6,  3,  9,  2,  8, 11
    ; Round 7
    DB 13, 11,  7, 14, 12,  1,  3,  9,  5,  0, 15,  4,  8,  6,  2, 10
    ; Round 8
    DB  6, 15, 14,  9, 11,  3,  0,  8, 12,  2, 13,  7,  1,  4, 10,  5
    ; Round 9
    DB 10,  2,  8,  4,  7,  6,  1,  5, 15, 11,  9, 14,  3, 12, 13,  0

; Masks for building row 3
ALIGN 32
MASK_IDX2_AVX2_WIN:
    DQ 0
    DQ 0
    DQ 0FFFFFFFFFFFFFFFFh
    DQ 0

MASK_IDX3_AVX2_WIN:
    DQ 0
    DQ 0
    DQ 0
    DQ 0FFFFFFFFFFFFFFFFh

;
; void blake4_compress_avx2_asm_win(const uint64_t cv[8],
;                                   const uint8_t block[128],
;                                   uint32_t block_len,
;                                   uint64_t counter,
;                                   uint32_t flags,
;                                   uint64_t out[16])
;
blake4_compress_avx2_asm_win PROC
    ; Save non-volatile registers (Windows x64 ABI)
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    mov     rbp, rsp
    and     rsp, -32                ; Align stack to 32 bytes
    sub     rsp, 224                ; Space for locals + shadow space

    ; Save XMM6-15 (non-volatile on Windows)
    vmovdqa [rsp+32], xmm6
    vmovdqa [rsp+48], xmm7
    vmovdqa [rsp+64], xmm8
    vmovdqa [rsp+80], xmm9
    vmovdqa [rsp+96], xmm10
    vmovdqa [rsp+112], xmm11
    vmovdqa [rsp+128], xmm12
    vmovdqa [rsp+144], xmm13
    vmovdqa [rsp+160], xmm14
    vmovdqa [rsp+176], xmm15

    ; Windows x64 calling convention:
    ; rcx = cv, rdx = block, r8d = block_len, r9 = counter
    ; [rbp+48+64] = flags, [rbp+56+64] = out (accounting for pushes)
    mov     r12, rcx                ; cv
    mov     r13, rdx                ; block
    mov     r14d, r8d               ; block_len
    mov     r15, r9                 ; counter
    mov     ebx, DWORD PTR [rbp+112] ; flags (48 + 64 bytes of pushes)
    mov     rsi, QWORD PTR [rbp+120] ; out

    ; Load IV address
    lea     rax, BLAKE4_IV_AVX2_WIN

    ; Initialize state
    vmovdqu ymm0, YMMWORD PTR [r12]      ; cv[0..3]
    vmovdqu ymm1, YMMWORD PTR [r12+32]   ; cv[4..7]
    vmovdqa ymm2, YMMWORD PTR [rax]      ; IV[0..3]
    vmovdqa ymm3, YMMWORD PTR [rax+32]   ; IV[4..7]

    ; XOR counter into IV[4]
    vmovq   xmm4, r15
    vpxor   xmm5, xmm3, xmm4
    vpblendd ymm3, ymm3, ymm5, 03h

    ; XOR block_len into IV[6]
    mov     eax, r14d
    vmovq   xmm4, rax
    vpermq  ymm4, ymm4, 0
    vpand   ymm4, ymm4, YMMWORD PTR [MASK_IDX2_AVX2_WIN]
    vpxor   ymm3, ymm3, ymm4

    ; XOR flags into IV[7]
    mov     eax, ebx
    vmovq   xmm4, rax
    vpermq  ymm4, ymm4, 0
    vpand   ymm4, ymm4, YMMWORD PTR [MASK_IDX3_AVX2_WIN]
    vpxor   ymm3, ymm3, ymm4

    ; Store message pointer
    mov     rdi, r13                ; message pointer

    ; Get SIGMA table address
    lea     r10, BLAKE4_SIGMA_AVX2_WIN

    ; Round loop
    mov     ecx, 10

round_loop_avx2_win:
    ; Load SIGMA indices for column step
    movzx   eax, BYTE PTR [r10]
    movzx   r8d, BYTE PTR [r10+2]
    movzx   r9d, BYTE PTR [r10+4]
    movzx   r11d, BYTE PTR [r10+6]

    ; Build mx (column step)
    shl     rax, 3
    shl     r8, 3
    shl     r9, 3
    shl     r11, 3
    vmovq   xmm4, QWORD PTR [rdi+rax]
    vmovq   xmm5, QWORD PTR [rdi+r8]
    vpunpcklqdq xmm4, xmm4, xmm5
    vmovq   xmm6, QWORD PTR [rdi+r9]
    vmovq   xmm7, QWORD PTR [rdi+r11]
    vpunpcklqdq xmm6, xmm6, xmm7
    vinserti128 ymm4, ymm4, xmm6, 1

    ; Build my (column step)
    movzx   eax, BYTE PTR [r10+1]
    movzx   r8d, BYTE PTR [r10+3]
    movzx   r9d, BYTE PTR [r10+5]
    movzx   r11d, BYTE PTR [r10+7]
    shl     rax, 3
    shl     r8, 3
    shl     r9, 3
    shl     r11, 3
    vmovq   xmm5, QWORD PTR [rdi+rax]
    vmovq   xmm6, QWORD PTR [rdi+r8]
    vpunpcklqdq xmm5, xmm5, xmm6
    vmovq   xmm6, QWORD PTR [rdi+r9]
    vmovq   xmm7, QWORD PTR [rdi+r11]
    vpunpcklqdq xmm6, xmm6, xmm7
    vinserti128 ymm5, ymm5, xmm6, 1

    ; G function (column step)
    ; a = a + b + mx
    vpaddq  ymm0, ymm0, ymm1
    vpaddq  ymm0, ymm0, ymm4
    ; d = rotr(d ^ a, 32) - use vpshufd for 32-bit rotate
    vpxor   ymm3, ymm3, ymm0
    vpshufd ymm3, ymm3, 0B1h        ; Swap 32-bit halves = rotate by 32

    ; c = c + d
    vpaddq  ymm2, ymm2, ymm3
    ; b = rotr(b ^ c, 24) - use shift pairs
    vpxor   ymm1, ymm1, ymm2
    vpsrlq  ymm6, ymm1, 24
    vpsllq  ymm7, ymm1, 40
    vpor    ymm1, ymm6, ymm7

    ; a = a + b + my
    vpaddq  ymm0, ymm0, ymm1
    vpaddq  ymm0, ymm0, ymm5
    ; d = rotr(d ^ a, 16) - use shift pairs
    vpxor   ymm3, ymm3, ymm0
    vpsrlq  ymm6, ymm3, 16
    vpsllq  ymm7, ymm3, 48
    vpor    ymm3, ymm6, ymm7

    ; c = c + d
    vpaddq  ymm2, ymm2, ymm3
    ; b = rotr(b ^ c, 63) - use shift pairs
    vpxor   ymm1, ymm1, ymm2
    vpsrlq  ymm6, ymm1, 63
    vpsllq  ymm7, ymm1, 1
    vpor    ymm1, ymm6, ymm7

    ; Shuffle for diagonal step
    vpermq  ymm1, ymm1, 39h         ; 0, 3, 2, 1 -> rotate left by 1
    vpermq  ymm2, ymm2, 4Eh         ; 1, 0, 3, 2 -> rotate left by 2
    vpermq  ymm3, ymm3, 93h         ; 2, 1, 0, 3 -> rotate left by 3

    ; Load diagonal message words
    movzx   eax, BYTE PTR [r10+8]
    movzx   r8d, BYTE PTR [r10+10]
    movzx   r9d, BYTE PTR [r10+12]
    movzx   r11d, BYTE PTR [r10+14]
    shl     rax, 3
    shl     r8, 3
    shl     r9, 3
    shl     r11, 3
    vmovq   xmm4, QWORD PTR [rdi+rax]
    vmovq   xmm5, QWORD PTR [rdi+r8]
    vpunpcklqdq xmm4, xmm4, xmm5
    vmovq   xmm6, QWORD PTR [rdi+r9]
    vmovq   xmm7, QWORD PTR [rdi+r11]
    vpunpcklqdq xmm6, xmm6, xmm7
    vinserti128 ymm4, ymm4, xmm6, 1

    movzx   eax, BYTE PTR [r10+9]
    movzx   r8d, BYTE PTR [r10+11]
    movzx   r9d, BYTE PTR [r10+13]
    movzx   r11d, BYTE PTR [r10+15]
    shl     rax, 3
    shl     r8, 3
    shl     r9, 3
    shl     r11, 3
    vmovq   xmm5, QWORD PTR [rdi+rax]
    vmovq   xmm6, QWORD PTR [rdi+r8]
    vpunpcklqdq xmm5, xmm5, xmm6
    vmovq   xmm6, QWORD PTR [rdi+r9]
    vmovq   xmm7, QWORD PTR [rdi+r11]
    vpunpcklqdq xmm6, xmm6, xmm7
    vinserti128 ymm5, ymm5, xmm6, 1

    ; G function (diagonal step)
    ; a = a + b + mx
    vpaddq  ymm0, ymm0, ymm1
    vpaddq  ymm0, ymm0, ymm4
    ; d = rotr(d ^ a, 32)
    vpxor   ymm3, ymm3, ymm0
    vpshufd ymm3, ymm3, 0B1h

    ; c = c + d
    vpaddq  ymm2, ymm2, ymm3
    ; b = rotr(b ^ c, 24)
    vpxor   ymm1, ymm1, ymm2
    vpsrlq  ymm6, ymm1, 24
    vpsllq  ymm7, ymm1, 40
    vpor    ymm1, ymm6, ymm7

    ; a = a + b + my
    vpaddq  ymm0, ymm0, ymm1
    vpaddq  ymm0, ymm0, ymm5
    ; d = rotr(d ^ a, 16)
    vpxor   ymm3, ymm3, ymm0
    vpsrlq  ymm6, ymm3, 16
    vpsllq  ymm7, ymm3, 48
    vpor    ymm3, ymm6, ymm7

    ; c = c + d
    vpaddq  ymm2, ymm2, ymm3
    ; b = rotr(b ^ c, 63)
    vpxor   ymm1, ymm1, ymm2
    vpsrlq  ymm6, ymm1, 63
    vpsllq  ymm7, ymm1, 1
    vpor    ymm1, ymm6, ymm7

    ; Unshuffle
    vpermq  ymm1, ymm1, 93h         ; rotate right by 1
    vpermq  ymm2, ymm2, 4Eh         ; rotate right by 2
    vpermq  ymm3, ymm3, 39h         ; rotate right by 3

    ; Loop
    add     r10, 16
    dec     ecx
    jnz     round_loop_avx2_win

    ; Finalization
    vpxor   ymm4, ymm0, ymm2
    vpxor   ymm5, ymm1, ymm3

    vmovdqu ymm6, YMMWORD PTR [r12]
    vmovdqu ymm7, YMMWORD PTR [r12+32]
    vpxor   ymm6, ymm2, ymm6
    vpxor   ymm7, ymm3, ymm7

    ; Store output
    vmovdqu YMMWORD PTR [rsi], ymm4
    vmovdqu YMMWORD PTR [rsi+32], ymm5
    vmovdqu YMMWORD PTR [rsi+64], ymm6
    vmovdqu YMMWORD PTR [rsi+96], ymm7

    vzeroupper

    ; Restore XMM registers
    vmovdqa xmm6, [rsp+32]
    vmovdqa xmm7, [rsp+48]
    vmovdqa xmm8, [rsp+64]
    vmovdqa xmm9, [rsp+80]
    vmovdqa xmm10, [rsp+96]
    vmovdqa xmm11, [rsp+112]
    vmovdqa xmm12, [rsp+128]
    vmovdqa xmm13, [rsp+144]
    vmovdqa xmm14, [rsp+160]
    vmovdqa xmm15, [rsp+176]

    ; Restore stack and registers
    mov     rsp, rbp
    pop     rbp
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

blake4_compress_avx2_asm_win ENDP

END
