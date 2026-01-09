/*
 * BLAKE4 Fuzzer - Incremental API
 *
 * Tests the incremental hashing API with various update patterns.
 *
 * Build with:
 *   clang -fsanitize=fuzzer,address -I../include -L../build fuzz_incremental.c -lblake4 -o fuzz_incremental
 *
 * This is free and unencumbered software released into the public domain.
 */

#include "blake4.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size == 0) return 0;

    uint8_t hash_oneshot[BLAKE4_OUT_LEN];
    uint8_t hash_incremental[BLAKE4_OUT_LEN];
    uint8_t hash_bytebybyte[BLAKE4_OUT_LEN];

    /* One-shot reference */
    blake4_hash(data, size, hash_oneshot);

    /* Incremental with various chunk sizes based on fuzzer input */
    blake4_hasher h;
    blake4_hasher_init(&h);

    size_t offset = 0;
    size_t chunk_idx = 0;
    while (offset < size) {
        /* Use bytes from input to determine chunk size */
        size_t chunk_size = 1;
        if (chunk_idx < size) {
            chunk_size = (data[chunk_idx] % 64) + 1;
        }
        if (offset + chunk_size > size) {
            chunk_size = size - offset;
        }
        blake4_hasher_update(&h, data + offset, chunk_size);
        offset += chunk_size;
        chunk_idx++;
    }
    blake4_hasher_finalize(&h, hash_incremental, BLAKE4_OUT_LEN);

    /* Verify incremental matches one-shot */
    if (memcmp(hash_oneshot, hash_incremental, BLAKE4_OUT_LEN) != 0) {
        __builtin_trap();
    }

    /* Byte-by-byte (limited to small inputs for performance) */
    if (size <= 1024) {
        blake4_hasher_init(&h);
        for (size_t i = 0; i < size; i++) {
            blake4_hasher_update(&h, &data[i], 1);
        }
        blake4_hasher_finalize(&h, hash_bytebybyte, BLAKE4_OUT_LEN);

        if (memcmp(hash_oneshot, hash_bytebybyte, BLAKE4_OUT_LEN) != 0) {
            __builtin_trap();
        }
    }

    /* Test hasher reset */
    blake4_hasher_init(&h);
    blake4_hasher_update(&h, "garbage", 7);
    blake4_hasher_reset(&h);
    blake4_hasher_update(&h, data, size);
    uint8_t hash_reset[BLAKE4_OUT_LEN];
    blake4_hasher_finalize(&h, hash_reset, BLAKE4_OUT_LEN);

    if (memcmp(hash_oneshot, hash_reset, BLAKE4_OUT_LEN) != 0) {
        __builtin_trap();
    }

    return 0;
}
