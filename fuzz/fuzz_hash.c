/*
 * BLAKE4 Fuzzer - One-shot Hashing
 *
 * Tests the blake4_hash() function with arbitrary inputs.
 *
 * Build with:
 *   clang -fsanitize=fuzzer,address -I../include -L../build fuzz_hash.c -lblake4 -o fuzz_hash
 *
 * Run with:
 *   ./fuzz_hash corpus_hash/
 *
 * This is free and unencumbered software released into the public domain.
 */

#include "blake4.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    uint8_t hash[BLAKE4_OUT_LEN];

    /* Test one-shot hashing */
    blake4_hash(data, size, hash);

    /* Test XOF mode with various output sizes */
    uint8_t xof_out[256];
    blake4_hash_xof(data, size, xof_out, 32);
    blake4_hash_xof(data, size, xof_out, 64);
    blake4_hash_xof(data, size, xof_out, 128);
    blake4_hash_xof(data, size, xof_out, 256);

    /* Verify XOF prefix consistency */
    uint8_t xof_64[64], xof_128[128];
    blake4_hash_xof(data, size, xof_64, 64);
    blake4_hash_xof(data, size, xof_128, 128);
    if (memcmp(xof_64, xof_128, 64) != 0) {
        __builtin_trap(); /* XOF prefix mismatch */
    }

    /* Test convenience functions */
    uint8_t out256[32], out384[48], out512[64];
    blake4_256_hash(data, size, out256);
    blake4_384_hash(data, size, out384);
    blake4_512_hash(data, size, out512);

    /* Verify 256/384 are prefixes of 512 */
    if (memcmp(out256, out512, 32) != 0) {
        __builtin_trap();
    }
    if (memcmp(out384, out512, 48) != 0) {
        __builtin_trap();
    }

    return 0;
}
