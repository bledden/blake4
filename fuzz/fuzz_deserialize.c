/*
 * BLAKE4 Fuzzer - State Deserialization
 *
 * Tests the state deserialization with arbitrary input.
 * This is important for security as deserialization parses untrusted data.
 *
 * Build with:
 *   clang -fsanitize=fuzzer,address -I../include -L../build fuzz_deserialize.c -lblake4 -o fuzz_deserialize
 *
 * This is free and unencumbered software released into the public domain.
 */

#include "blake4.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    blake4_hasher h;

    /* Attempt to deserialize fuzzer input as hasher state */
    int result = blake4_hasher_deserialize(&h, data, size);

    if (result) {
        /* If deserialization succeeded, verify we can use the hasher */
        uint8_t hash[BLAKE4_OUT_LEN];

        /* Update with some data */
        blake4_hasher_update(&h, "test", 4);

        /* Finalize should work */
        blake4_hasher_finalize(&h, hash, BLAKE4_OUT_LEN);

        /* Serialize the state back */
        uint8_t buf[BLAKE4_SERIALIZED_SIZE];
        size_t written = blake4_hasher_serialize(&h, buf, sizeof(buf));
        if (written == 0) {
            __builtin_trap(); /* Serialization should work after deserialization */
        }

        /* Re-deserialize and verify */
        blake4_hasher h2;
        if (!blake4_hasher_deserialize(&h2, buf, written)) {
            __builtin_trap(); /* Re-deserialization should work */
        }
    }

    return 0;
}
