/*
 * BLAKE4 Basic Tests
 * Validates correctness against known BLAKE3 test vectors
 */

#include "blake4.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void print_hex(const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        printf("%02x", data[i]);
    }
}

static int hex_to_bytes(const char *hex, uint8_t *out, size_t out_len) {
    size_t hex_len = strlen(hex);
    if (hex_len != out_len * 2) {
        return -1;
    }
    for (size_t i = 0; i < out_len; i++) {
        unsigned int byte;
        if (sscanf(hex + i * 2, "%2x", &byte) != 1) {
            return -1;
        }
        out[i] = (uint8_t)byte;
    }
    return 0;
}

static int test_vector(const char *name, const void *input, size_t input_len,
                       const char *expected_hex) {
    uint8_t out[BLAKE4_OUT_LEN];
    uint8_t expected[BLAKE4_OUT_LEN];

    if (hex_to_bytes(expected_hex, expected, BLAKE4_OUT_LEN) != 0) {
        printf("FAIL: %s - invalid expected hex\n", name);
        return 1;
    }

    blake4_hash(input, input_len, out);

    if (memcmp(out, expected, BLAKE4_OUT_LEN) != 0) {
        printf("FAIL: %s\n", name);
        printf("  Expected: %s\n", expected_hex);
        printf("  Got:      ");
        print_hex(out, BLAKE4_OUT_LEN);
        printf("\n");
        return 1;
    }

    printf("PASS: %s\n", name);
    return 0;
}

static int test_incremental(const char *name, const void *input, size_t input_len,
                            size_t chunk_size) {
    uint8_t out_oneshot[BLAKE4_OUT_LEN];
    uint8_t out_incremental[BLAKE4_OUT_LEN];
    const uint8_t *in = (const uint8_t *)input;

    /* One-shot */
    blake4_hash(input, input_len, out_oneshot);

    /* Incremental with specified chunk size */
    blake4_hasher hasher;
    blake4_hasher_init(&hasher);
    size_t remaining = input_len;
    while (remaining > 0) {
        size_t take = (remaining < chunk_size) ? remaining : chunk_size;
        blake4_hasher_update(&hasher, in, take);
        in += take;
        remaining -= take;
    }
    blake4_hasher_finalize(&hasher, out_incremental, BLAKE4_OUT_LEN);

    if (memcmp(out_oneshot, out_incremental, BLAKE4_OUT_LEN) != 0) {
        printf("FAIL: %s (chunk_size=%zu)\n", name, chunk_size);
        printf("  One-shot:     ");
        print_hex(out_oneshot, BLAKE4_OUT_LEN);
        printf("\n");
        printf("  Incremental:  ");
        print_hex(out_incremental, BLAKE4_OUT_LEN);
        printf("\n");
        return 1;
    }

    printf("PASS: %s (chunk_size=%zu)\n", name, chunk_size);
    return 0;
}

int main(void) {
    int failures = 0;

    printf("=== BLAKE4 Basic Tests ===\n\n");

    /* Test version */
    printf("Version: %s\n", blake4_version_string());
    printf("Version: %d.%d.%d\n\n",
           blake4_version_major(), blake4_version_minor(), blake4_version_patch());

    /* BLAKE3 official test vectors */
    printf("--- Known Test Vectors (BLAKE3 compatible) ---\n");

    /* Empty input */
    failures += test_vector(
        "empty",
        "", 0,
        "af1349b9f5f9a1a6a0404dea36dcc9499bcb25c9adc112b7cc9a93cae41f3262"
    );

    /* Single byte inputs - these are BLAKE3 test vectors */
    uint8_t single_byte[1] = {0};
    failures += test_vector(
        "single zero byte",
        single_byte, 1,
        "2d3adedff11b61f14c886e35afa036736dcd87a74d27b5c1510225d0f592e213"
    );

    /* 1 byte = 0x00 repeated */
    /* From BLAKE3 test vectors: input of length 1 with all zeros */

    printf("\n--- Incremental Hashing Tests ---\n");

    /* Generate test data */
    uint8_t test_data[4096];
    for (size_t i = 0; i < sizeof(test_data); i++) {
        test_data[i] = (uint8_t)(i % 251);  /* Prime to vary data */
    }

    /* Test incremental with various chunk sizes */
    failures += test_incremental("64 bytes", test_data, 64, 1);
    failures += test_incremental("64 bytes", test_data, 64, 7);
    failures += test_incremental("64 bytes", test_data, 64, 64);

    failures += test_incremental("1024 bytes (one chunk)", test_data, 1024, 1);
    failures += test_incremental("1024 bytes (one chunk)", test_data, 1024, 63);
    failures += test_incremental("1024 bytes (one chunk)", test_data, 1024, 64);
    failures += test_incremental("1024 bytes (one chunk)", test_data, 1024, 100);

    failures += test_incremental("2048 bytes (two chunks)", test_data, 2048, 1);
    failures += test_incremental("2048 bytes (two chunks)", test_data, 2048, 64);
    failures += test_incremental("2048 bytes (two chunks)", test_data, 2048, 1000);
    failures += test_incremental("2048 bytes (two chunks)", test_data, 2048, 1024);

    failures += test_incremental("4096 bytes (four chunks)", test_data, 4096, 1);
    failures += test_incremental("4096 bytes (four chunks)", test_data, 4096, 64);
    failures += test_incremental("4096 bytes (four chunks)", test_data, 4096, 1024);

    printf("\n--- XOF Mode Tests ---\n");

    /* Test that different output lengths work */
    uint8_t xof_out_64[64];
    uint8_t xof_out_32[32];
    blake4_hash_xof("test", 4, xof_out_64, 64);
    blake4_hash("test", 4, xof_out_32);

    if (memcmp(xof_out_64, xof_out_32, 32) == 0) {
        printf("PASS: XOF 64-byte output starts with 32-byte output\n");
    } else {
        printf("FAIL: XOF output mismatch\n");
        failures++;
    }

    /* Test seek functionality */
    uint8_t seek_out[32];
    blake4_hasher hasher;
    blake4_hasher_init(&hasher);
    blake4_hasher_update(&hasher, "test", 4);
    blake4_hasher_finalize_seek(&hasher, 32, seek_out, 32);

    if (memcmp(seek_out, xof_out_64 + 32, 32) == 0) {
        printf("PASS: XOF seek works correctly\n");
    } else {
        printf("FAIL: XOF seek output mismatch\n");
        failures++;
    }

    printf("\n--- Keyed Mode Tests ---\n");

    uint8_t key[BLAKE4_KEY_LEN] = {0};
    uint8_t keyed_out[BLAKE4_OUT_LEN];
    uint8_t unkeyed_out[BLAKE4_OUT_LEN];

    blake4_hash_keyed(key, "test", 4, keyed_out);
    blake4_hash("test", 4, unkeyed_out);

    if (memcmp(keyed_out, unkeyed_out, BLAKE4_OUT_LEN) != 0) {
        printf("PASS: Keyed mode produces different output than unkeyed\n");
    } else {
        printf("FAIL: Keyed mode should differ from unkeyed\n");
        failures++;
    }

    printf("\n--- Key Derivation Tests ---\n");

    uint8_t derived_key[32];
    blake4_derive_key("BLAKE4 test context 2024", "key material", 12,
                      derived_key, 32);
    printf("PASS: Key derivation completed (output: ");
    print_hex(derived_key, 8);
    printf("...)\n");

    /* Test that different contexts produce different keys */
    uint8_t derived_key2[32];
    blake4_derive_key("Different context", "key material", 12,
                      derived_key2, 32);
    if (memcmp(derived_key, derived_key2, 32) != 0) {
        printf("PASS: Different contexts produce different keys\n");
    } else {
        printf("FAIL: Different contexts should produce different keys\n");
        failures++;
    }

    printf("\n--- Reset Test ---\n");

    blake4_hasher h1, h2;
    blake4_hasher_init(&h1);
    blake4_hasher_update(&h1, "hello", 5);
    blake4_hasher_reset(&h1);
    blake4_hasher_update(&h1, "world", 5);

    blake4_hasher_init(&h2);
    blake4_hasher_update(&h2, "world", 5);

    uint8_t out1[32], out2[32];
    blake4_hasher_finalize(&h1, out1, 32);
    blake4_hasher_finalize(&h2, out2, 32);

    if (memcmp(out1, out2, 32) == 0) {
        printf("PASS: Reset works correctly\n");
    } else {
        printf("FAIL: Reset didn't clear state properly\n");
        failures++;
    }

    printf("\n=== Summary ===\n");
    if (failures == 0) {
        printf("All tests passed!\n");
    } else {
        printf("%d test(s) FAILED\n", failures);
    }

    return failures;
}
