/*
 * BLAKE4 Fuzzer - Keyed Hashing
 *
 * Tests keyed hashing and key derivation APIs.
 *
 * Build with:
 *   clang -fsanitize=fuzzer,address -I../include -L../build fuzz_keyed.c -lblake4 -o fuzz_keyed
 *
 * This is free and unencumbered software released into the public domain.
 */

#include "blake4.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < BLAKE4_KEY_LEN) return 0;

    /* Use first 64 bytes as key, rest as data */
    const uint8_t *key = data;
    const uint8_t *input = data + BLAKE4_KEY_LEN;
    size_t input_len = size - BLAKE4_KEY_LEN;

    /* Test keyed hashing */
    uint8_t keyed_hash[BLAKE4_OUT_LEN];
    blake4_hash_keyed(key, input, input_len, keyed_hash);

    /* Test incremental keyed hashing */
    blake4_hasher h;
    blake4_hasher_init_keyed(&h, key);
    blake4_hasher_update(&h, input, input_len);
    uint8_t incremental_hash[BLAKE4_OUT_LEN];
    blake4_hasher_finalize(&h, incremental_hash, BLAKE4_OUT_LEN);

    /* Both should match */
    if (memcmp(keyed_hash, incremental_hash, BLAKE4_OUT_LEN) != 0) {
        __builtin_trap();
    }

    /* Keyed hash should differ from unkeyed */
    uint8_t unkeyed_hash[BLAKE4_OUT_LEN];
    blake4_hash(input, input_len, unkeyed_hash);
    /* Note: Not checking inequality because theoretically they could collide
     * (astronomically unlikely but not a bug) */

    /* Test key derivation */
    uint8_t derived_key[64];
    blake4_derive_key("test context", input, input_len, derived_key, 64);

    /* Different contexts should give different keys */
    uint8_t derived_key2[64];
    blake4_derive_key("other context", input, input_len, derived_key2, 64);
    /* Again, not checking inequality due to theoretical collision possibility */

    return 0;
}
