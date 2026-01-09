/*
 * BLAKE4 NEON Assembly Registration
 *
 * This file registers the hand-optimized ARM64 NEON assembly implementation
 * with the runtime dispatcher.
 *
 * The actual compression function is in blake4_neon_arm64.S
 *
 * This is free and unencumbered software released into the public domain.
 */

#include "blake4_dispatch.h"

#if defined(__aarch64__) || defined(_M_ARM64)

/* External assembly function */
extern void blake4_compress_neon_asm(const uint64_t cv[8],
                                     const uint8_t block[128],
                                     uint32_t block_len,
                                     uint64_t counter,
                                     uint32_t flags,
                                     uint64_t out[16]);

/*
 * Constructor to register the NEON assembly implementation on library load.
 * This runs after blake4_neon.c's constructor, so the assembly version
 * will be preferred if both are available.
 */
__attribute__((constructor(102)))  /* Priority 102, after intrinsics at 101 */
static void blake4_neon_asm_init(void) {
    blake4_register_neon_asm(blake4_compress_neon_asm);
}

#endif /* __aarch64__ || _M_ARM64 */
