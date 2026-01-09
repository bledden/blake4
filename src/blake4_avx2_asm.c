/*
 * BLAKE4 AVX2 Assembly Registration
 *
 * This file registers the hand-optimized x86-64 AVX2 assembly implementation
 * with the runtime dispatcher.
 *
 * The actual compression function is in blake4_avx2_x86-64.S (Unix)
 * or blake4_avx2_x86-64_windows.asm (Windows).
 *
 * This is free and unencumbered software released into the public domain.
 */

#include "blake4_dispatch.h"

#if defined(__x86_64__) || defined(_M_X64)

/* Check for AVX2 support at compile time */
#if defined(__AVX2__)

/* External assembly function */
extern void blake4_compress_avx2_asm(const uint64_t cv[8],
                                      const uint8_t block[128],
                                      uint32_t block_len,
                                      uint64_t counter,
                                      uint32_t flags,
                                      uint64_t out[16]);

/*
 * Constructor to register the AVX2 assembly implementation on library load.
 * This runs after blake4_avx2.c's constructor, so the assembly version
 * will be preferred if both are available.
 */
__attribute__((constructor(102)))  /* Priority 102, after intrinsics at 101 */
static void blake4_avx2_asm_init(void) {
    blake4_register_avx2_asm(blake4_compress_avx2_asm);
}

#endif /* __AVX2__ */
#endif /* __x86_64__ || _M_X64 */
