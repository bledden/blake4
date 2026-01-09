/*
 * BLAKE4 AVX-512 Assembly Registration
 *
 * This file registers the hand-optimized x86-64 AVX-512 assembly implementation
 * with the runtime dispatcher.
 *
 * The actual compression function is in blake4_avx512_x86-64.S (Unix)
 * or blake4_avx512_x86-64_windows.asm (Windows).
 *
 * This is free and unencumbered software released into the public domain.
 */

#include "blake4_dispatch.h"

#if defined(__x86_64__) || defined(_M_X64)

/* Check for AVX-512 support at compile time */
#if defined(__AVX512F__) && defined(__AVX512VL__)

/* External assembly function */
extern void blake4_compress_avx512_asm(const uint64_t cv[8],
                                        const uint8_t block[128],
                                        uint32_t block_len,
                                        uint64_t counter,
                                        uint32_t flags,
                                        uint64_t out[16]);

/*
 * Constructor to register the AVX-512 assembly implementation on library load.
 * This runs after blake4_avx512.c's constructor, so the assembly version
 * will be preferred if both are available.
 */
__attribute__((constructor(102)))  /* Priority 102, after intrinsics at 101 */
static void blake4_avx512_asm_init(void) {
    blake4_register_avx512_asm(blake4_compress_avx512_asm);
}

#endif /* __AVX512F__ && __AVX512VL__ */
#endif /* __x86_64__ || _M_X64 */
