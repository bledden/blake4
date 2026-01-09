/*
 * BLAKE4 Constant-Time Utilities
 *
 * These functions provide constant-time operations critical for
 * cryptographic security. They prevent timing side-channel attacks
 * by ensuring execution time is independent of secret data values.
 *
 * Usage:
 *   - blake4_ct_memcmp: Use for comparing MACs, keys, or other secrets
 *   - blake4_ct_memzero: Use for clearing sensitive data from memory
 *   - blake4_ct_select: Use for constant-time conditional selection
 *
 * Important: These functions MUST be used in keyed hashing modes
 * to prevent timing attacks on MAC verification.
 *
 * This is free and unencumbered software released into the public domain.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/*
 * Constant-time memory comparison.
 *
 * Compares two byte arrays in constant time regardless of where
 * they differ. This prevents timing attacks that could reveal
 * information about secret values (like MAC tags).
 *
 * Returns 0 if equal, non-zero if different.
 *
 * Time complexity: O(n), always fully iterates.
 */
int blake4_ct_memcmp(const void *a, const void *b, size_t n) {
    const volatile uint8_t *x = (const volatile uint8_t *)a;
    const volatile uint8_t *y = (const volatile uint8_t *)b;
    volatile uint8_t diff = 0;

    for (size_t i = 0; i < n; i++) {
        diff |= x[i] ^ y[i];
    }

    /* Convert to 0 or 1 to avoid leaking bit patterns */
    return (int)((diff | (diff >> 1) | (diff >> 2) | (diff >> 3) |
                  (diff >> 4) | (diff >> 5) | (diff >> 6) | (diff >> 7)) & 1);
}

/*
 * Secure memory zeroing.
 *
 * Zeros memory in a way that won't be optimized away by the compiler.
 * Essential for clearing keys, intermediate state, and other secrets.
 *
 * This function uses volatile to prevent compiler optimization and
 * memory barriers where available to ensure the write completes.
 */
void blake4_ct_memzero(void *ptr, size_t n) {
    volatile uint8_t *p = (volatile uint8_t *)ptr;

    for (size_t i = 0; i < n; i++) {
        p[i] = 0;
    }

    /*
     * Memory barrier to ensure the writes are not reordered or eliminated.
     * Different compilers/platforms need different approaches.
     */
#if defined(__GNUC__) || defined(__clang__)
    __asm__ __volatile__("" : : "r"(ptr) : "memory");
#elif defined(_MSC_VER)
    /* MSVC: MemoryBarrier() or _ReadWriteBarrier() */
    _ReadWriteBarrier();
#endif
}

/*
 * Constant-time conditional selection.
 *
 * Returns a if condition is 0, b if condition is non-zero.
 * Runs in constant time regardless of condition value.
 *
 * This is useful for implementing constant-time algorithms
 * that need to select between values without branching.
 */
uint64_t blake4_ct_select64(uint64_t condition, uint64_t a, uint64_t b) {
    /*
     * Convert condition to a mask:
     * - If condition == 0, mask = 0x0000000000000000
     * - If condition != 0, mask = 0xFFFFFFFFFFFFFFFF
     */
    uint64_t mask = (uint64_t)(-(int64_t)(condition != 0));
    return (a & ~mask) | (b & mask);
}

/*
 * Constant-time byte selection.
 */
uint8_t blake4_ct_select8(uint8_t condition, uint8_t a, uint8_t b) {
    uint8_t mask = (uint8_t)(-(int8_t)(condition != 0));
    return (a & ~mask) | (b & mask);
}

/*
 * Constant-time conditional memory copy.
 *
 * If condition is non-zero, copies src to dst.
 * If condition is zero, dst is unchanged.
 * Always reads both src and performs all operations.
 */
void blake4_ct_memcpy_if(void *dst, const void *src, size_t n, int condition) {
    volatile uint8_t *d = (volatile uint8_t *)dst;
    const volatile uint8_t *s = (const volatile uint8_t *)src;
    uint8_t mask = (uint8_t)(-(int8_t)(condition != 0));

    for (size_t i = 0; i < n; i++) {
        d[i] = (d[i] & ~mask) | (s[i] & mask);
    }
}

/*
 * Constant-time equality check.
 *
 * Returns 1 if a == b, 0 otherwise.
 * Constant time with respect to the value of a and b.
 */
int blake4_ct_eq64(uint64_t a, uint64_t b) {
    uint64_t diff = a ^ b;
    /* Collapse diff to a single bit */
    diff |= diff >> 32;
    diff |= diff >> 16;
    diff |= diff >> 8;
    diff |= diff >> 4;
    diff |= diff >> 2;
    diff |= diff >> 1;
    return (int)(1 - (diff & 1));
}

/*
 * Constant-time less-than comparison.
 *
 * Returns 1 if a < b, 0 otherwise.
 * Uses the sign bit of (a - b) in a way that's constant-time.
 */
int blake4_ct_lt64(uint64_t a, uint64_t b) {
    /*
     * We want to compute a < b without branching.
     * The basic idea: a < b iff (a - b) has its high bit set,
     * but we need to handle overflow carefully.
     *
     * For unsigned: a < b iff ((a ^ ((a ^ b) | ((a - b) ^ b))) >> 63) & 1
     */
    uint64_t diff = a ^ b;
    uint64_t sub = a - b;
    uint64_t borrow = (a ^ (diff | (sub ^ b)));
    return (int)((borrow >> 63) & 1);
}

/*
 * Verification documentation:
 *
 * CONSTANT-TIME GUARANTEES:
 *
 * The following functions in BLAKE4 are guaranteed constant-time
 * with respect to secret data:
 *
 * 1. blake4_ct_memcmp() - Safe for comparing MACs, keys
 * 2. blake4_ct_memzero() - Safe for clearing sensitive data
 * 3. blake4_ct_select*() - Safe for conditional operations on secrets
 *
 * The following core hash functions are data-independent:
 *
 * 1. blake4_hasher_update() - No secret-dependent branches
 * 2. blake4_hasher_finalize() - No secret-dependent branches
 * 3. compress() - No secret-dependent branches or memory access
 *
 * IMPORTANT: The portable implementation uses only:
 * - Fixed loop counts
 * - Fixed memory access patterns (no secret-dependent indexing)
 * - Bitwise operations, addition (no secret-dependent branches)
 *
 * SIMD implementations inherit these properties as they use:
 * - Lane-parallel operations with fixed shuffle patterns
 * - Table lookups only on public indices (SIGMA table)
 *
 * VERIFICATION:
 *
 * To verify constant-time properties:
 *
 * 1. Static analysis: Use tools like ct-verif or ctgrind
 *    Example: valgrind --tool=memcheck --track-origins=yes ./test_blake4
 *
 * 2. Dynamic analysis: Use dudect or similar statistical timing tests
 *    Example: Run identical operations with different secret values,
 *    verify no statistically significant timing difference.
 *
 * 3. Code review: Inspect all code paths for:
 *    - No secret-dependent branches (if/else, ternary on secrets)
 *    - No secret-dependent array indexing
 *    - No early-exit conditions based on secrets
 *    - No variable-time instructions on secrets (division, etc.)
 *
 * KEYED MODE CONSIDERATIONS:
 *
 * When using BLAKE4 in keyed mode (blake4_hasher_init_keyed):
 *
 * 1. ALWAYS use blake4_ct_memcmp() to verify MACs
 * 2. ALWAYS use blake4_ct_memzero() to clear key material
 * 3. The key is processed through the same data-independent path
 *    as regular input, so no timing leakage occurs
 *
 * NON-CONSTANT-TIME OPERATIONS (intentional):
 *
 * 1. Input length checks - length is public metadata
 * 2. Output buffer allocation - sizes are public
 * 3. Thread scheduling in parallel mode - timing is not security-relevant
 *
 * PLATFORM NOTES:
 *
 * - x86/x64: All operations used are constant-time on modern Intel/AMD
 * - ARM: NEON operations are constant-time on Cortex-A53+
 * - Compiler: Use -O2 or -O3; do not use -ffast-math or similar
 *
 * REFERENCES:
 *
 * - "Timing Attacks on Implementations of Diffie-Hellman, RSA, DSS"
 *   Paul Kocher, 1996
 * - "Remote Timing Attacks are Practical"
 *   David Brumley, Dan Boneh, 2003
 * - "Cache-timing attacks on AES"
 *   Daniel J. Bernstein, 2005
 */
