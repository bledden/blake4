/*
 * BLAKE4 API Contract Tests
 *
 * Tests edge cases, boundary conditions, and API contracts.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "blake4.h"
#include "blake4_simd.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static int test_##name(void)
#define RUN_TEST(name) do { \
    printf("  %-50s ", #name); \
    if (test_##name()) { \
        printf("[PASS]\n"); \
        tests_passed++; \
    } else { \
        printf("[FAIL]\n"); \
        tests_failed++; \
    } \
} while(0)

/* ============ Edge Case Tests ============ */

TEST(empty_input) {
    uint8_t hash[BLAKE4_OUT_LEN];
    blake4_hash(NULL, 0, hash);  /* NULL with 0 length should work */
    blake4_hash("", 0, hash);    /* Empty string should work */
    return 1;
}

TEST(null_pointer_with_zero_length) {
    uint8_t hash[BLAKE4_OUT_LEN];
    blake4_hasher h;

    blake4_hasher_init(&h);
    blake4_hasher_update(&h, NULL, 0);  /* Should not crash */
    blake4_hasher_finalize(&h, hash, BLAKE4_OUT_LEN);
    return 1;
}

TEST(single_byte_inputs) {
    uint8_t hash1[BLAKE4_OUT_LEN], hash2[BLAKE4_OUT_LEN];

    for (int i = 0; i < 256; i++) {
        uint8_t byte = (uint8_t)i;
        blake4_hash(&byte, 1, hash1);
        blake4_hash(&byte, 1, hash2);

        /* Same input should give same output */
        if (memcmp(hash1, hash2, BLAKE4_OUT_LEN) != 0) {
            return 0;
        }
    }
    return 1;
}

TEST(different_single_bytes_differ) {
    uint8_t hash1[BLAKE4_OUT_LEN], hash2[BLAKE4_OUT_LEN];
    uint8_t b1 = 0, b2 = 1;

    blake4_hash(&b1, 1, hash1);
    blake4_hash(&b2, 1, hash2);

    return memcmp(hash1, hash2, BLAKE4_OUT_LEN) != 0;
}

/* ============ Boundary Tests ============ */

TEST(block_boundary_127) {
    uint8_t data[127], hash1[BLAKE4_OUT_LEN], hash2[BLAKE4_OUT_LEN];
    memset(data, 0x42, 127);

    blake4_hash(data, 127, hash1);

    blake4_hasher h;
    blake4_hasher_init(&h);
    blake4_hasher_update(&h, data, 127);
    blake4_hasher_finalize(&h, hash2, BLAKE4_OUT_LEN);

    return memcmp(hash1, hash2, BLAKE4_OUT_LEN) == 0;
}

TEST(block_boundary_128) {
    uint8_t data[128], hash1[BLAKE4_OUT_LEN], hash2[BLAKE4_OUT_LEN];
    memset(data, 0x42, 128);

    blake4_hash(data, 128, hash1);

    blake4_hasher h;
    blake4_hasher_init(&h);
    blake4_hasher_update(&h, data, 128);
    blake4_hasher_finalize(&h, hash2, BLAKE4_OUT_LEN);

    return memcmp(hash1, hash2, BLAKE4_OUT_LEN) == 0;
}

TEST(block_boundary_129) {
    uint8_t data[129], hash1[BLAKE4_OUT_LEN], hash2[BLAKE4_OUT_LEN];
    memset(data, 0x42, 129);

    blake4_hash(data, 129, hash1);

    blake4_hasher h;
    blake4_hasher_init(&h);
    blake4_hasher_update(&h, data, 129);
    blake4_hasher_finalize(&h, hash2, BLAKE4_OUT_LEN);

    return memcmp(hash1, hash2, BLAKE4_OUT_LEN) == 0;
}

TEST(chunk_boundary_2047) {
    uint8_t *data = malloc(2047);
    uint8_t hash1[BLAKE4_OUT_LEN], hash2[BLAKE4_OUT_LEN];
    if (!data) return 0;
    memset(data, 0x42, 2047);

    blake4_hash(data, 2047, hash1);

    blake4_hasher h;
    blake4_hasher_init(&h);
    blake4_hasher_update(&h, data, 2047);
    blake4_hasher_finalize(&h, hash2, BLAKE4_OUT_LEN);

    free(data);
    return memcmp(hash1, hash2, BLAKE4_OUT_LEN) == 0;
}

TEST(chunk_boundary_2048) {
    uint8_t *data = malloc(2048);
    uint8_t hash1[BLAKE4_OUT_LEN], hash2[BLAKE4_OUT_LEN];
    if (!data) return 0;
    memset(data, 0x42, 2048);

    blake4_hash(data, 2048, hash1);

    blake4_hasher h;
    blake4_hasher_init(&h);
    blake4_hasher_update(&h, data, 2048);
    blake4_hasher_finalize(&h, hash2, BLAKE4_OUT_LEN);

    free(data);
    return memcmp(hash1, hash2, BLAKE4_OUT_LEN) == 0;
}

TEST(chunk_boundary_2049) {
    uint8_t *data = malloc(2049);
    uint8_t hash1[BLAKE4_OUT_LEN], hash2[BLAKE4_OUT_LEN];
    if (!data) return 0;
    memset(data, 0x42, 2049);

    blake4_hash(data, 2049, hash1);

    blake4_hasher h;
    blake4_hasher_init(&h);
    blake4_hasher_update(&h, data, 2049);
    blake4_hasher_finalize(&h, hash2, BLAKE4_OUT_LEN);

    free(data);
    return memcmp(hash1, hash2, BLAKE4_OUT_LEN) == 0;
}

/* ============ Incremental Hashing Tests ============ */

TEST(incremental_byte_by_byte) {
    uint8_t data[256], hash1[BLAKE4_OUT_LEN], hash2[BLAKE4_OUT_LEN];
    for (int i = 0; i < 256; i++) data[i] = (uint8_t)i;

    blake4_hash(data, 256, hash1);

    blake4_hasher h;
    blake4_hasher_init(&h);
    for (int i = 0; i < 256; i++) {
        blake4_hasher_update(&h, &data[i], 1);
    }
    blake4_hasher_finalize(&h, hash2, BLAKE4_OUT_LEN);

    return memcmp(hash1, hash2, BLAKE4_OUT_LEN) == 0;
}

TEST(incremental_various_chunk_sizes) {
    uint8_t data[1024], hash1[BLAKE4_OUT_LEN], hash2[BLAKE4_OUT_LEN];
    for (int i = 0; i < 1024; i++) data[i] = (uint8_t)i;

    blake4_hash(data, 1024, hash1);

    /* Update with varying chunk sizes */
    blake4_hasher h;
    blake4_hasher_init(&h);
    blake4_hasher_update(&h, data, 7);
    blake4_hasher_update(&h, data + 7, 128);
    blake4_hasher_update(&h, data + 135, 1);
    blake4_hasher_update(&h, data + 136, 500);
    blake4_hasher_update(&h, data + 636, 388);
    blake4_hasher_finalize(&h, hash2, BLAKE4_OUT_LEN);

    return memcmp(hash1, hash2, BLAKE4_OUT_LEN) == 0;
}

TEST(multiple_empty_updates) {
    uint8_t data[] = "test";
    uint8_t hash1[BLAKE4_OUT_LEN], hash2[BLAKE4_OUT_LEN];

    blake4_hash(data, 4, hash1);

    blake4_hasher h;
    blake4_hasher_init(&h);
    blake4_hasher_update(&h, NULL, 0);
    blake4_hasher_update(&h, data, 2);
    blake4_hasher_update(&h, NULL, 0);
    blake4_hasher_update(&h, data + 2, 2);
    blake4_hasher_update(&h, NULL, 0);
    blake4_hasher_finalize(&h, hash2, BLAKE4_OUT_LEN);

    return memcmp(hash1, hash2, BLAKE4_OUT_LEN) == 0;
}

/* ============ Output Size Tests ============ */

TEST(output_size_1) {
    uint8_t hash[1];
    blake4_hasher h;
    blake4_hasher_init(&h);
    blake4_hasher_update(&h, "test", 4);
    blake4_hasher_finalize(&h, hash, 1);
    return 1;  /* Should not crash */
}

TEST(output_size_32) {
    uint8_t hash[32];
    blake4_256_hash("test", 4, hash);
    return 1;
}

TEST(output_size_48) {
    uint8_t hash[48];
    blake4_384_hash("test", 4, hash);
    return 1;
}

TEST(output_size_64) {
    uint8_t hash[64];
    blake4_512_hash("test", 4, hash);
    return 1;
}

TEST(output_size_large_xof) {
    uint8_t hash[1024];
    blake4_hash_xof("test", 4, hash, 1024);
    return 1;
}

TEST(output_sizes_differ) {
    uint8_t hash32[32], hash48[48], hash64[64];

    blake4_256_hash("test", 4, hash32);
    blake4_384_hash("test", 4, hash48);
    blake4_512_hash("test", 4, hash64);

    /* First 32 bytes should match between all */
    if (memcmp(hash32, hash48, 32) != 0) return 0;
    if (memcmp(hash32, hash64, 32) != 0) return 0;
    if (memcmp(hash48, hash64, 48) != 0) return 0;

    return 1;
}

/* ============ Serialization Tests ============ */

TEST(serialize_roundtrip_empty) {
    uint8_t hash1[BLAKE4_OUT_LEN], hash2[BLAKE4_OUT_LEN];
    uint8_t state[BLAKE4_SERIALIZED_SIZE];

    blake4_hasher h1, h2;
    blake4_hasher_init(&h1);

    size_t written = blake4_hasher_serialize(&h1, state, sizeof(state));
    if (written != BLAKE4_SERIALIZED_SIZE) return 0;

    if (blake4_hasher_deserialize(&h2, state, sizeof(state)) != 1) return 0;

    blake4_hasher_update(&h1, "test", 4);
    blake4_hasher_update(&h2, "test", 4);

    blake4_hasher_finalize(&h1, hash1, BLAKE4_OUT_LEN);
    blake4_hasher_finalize(&h2, hash2, BLAKE4_OUT_LEN);

    return memcmp(hash1, hash2, BLAKE4_OUT_LEN) == 0;
}

TEST(serialize_roundtrip_partial) {
    uint8_t hash1[BLAKE4_OUT_LEN], hash2[BLAKE4_OUT_LEN];
    uint8_t state[BLAKE4_SERIALIZED_SIZE];

    blake4_hasher h1, h2;
    blake4_hasher_init(&h1);
    blake4_hasher_update(&h1, "hello ", 6);

    blake4_hasher_serialize(&h1, state, sizeof(state));
    blake4_hasher_deserialize(&h2, state, sizeof(state));

    blake4_hasher_update(&h1, "world", 5);
    blake4_hasher_update(&h2, "world", 5);

    blake4_hasher_finalize(&h1, hash1, BLAKE4_OUT_LEN);
    blake4_hasher_finalize(&h2, hash2, BLAKE4_OUT_LEN);

    return memcmp(hash1, hash2, BLAKE4_OUT_LEN) == 0;
}

TEST(serialize_buffer_too_small) {
    uint8_t state[BLAKE4_SERIALIZED_SIZE - 1];
    blake4_hasher h;
    blake4_hasher_init(&h);

    size_t written = blake4_hasher_serialize(&h, state, sizeof(state));
    return written == 0;  /* Should fail */
}

TEST(deserialize_invalid_magic) {
    uint8_t state[BLAKE4_SERIALIZED_SIZE];
    memset(state, 0xFF, sizeof(state));  /* Invalid magic */

    blake4_hasher h;
    int result = blake4_hasher_deserialize(&h, state, sizeof(state));
    return result == 0;  /* Should fail - deserialize returns 0 on failure */
}

/* ============ Keyed Hashing Tests ============ */

TEST(keyed_differs_from_unkeyed) {
    uint8_t key[BLAKE4_KEY_LEN];
    uint8_t hash1[BLAKE4_OUT_LEN], hash2[BLAKE4_OUT_LEN];

    memset(key, 0x42, BLAKE4_KEY_LEN);
    blake4_hash("test", 4, hash1);
    blake4_hash_keyed(key, "test", 4, hash2);

    return memcmp(hash1, hash2, BLAKE4_OUT_LEN) != 0;
}

TEST(different_keys_differ) {
    uint8_t key1[BLAKE4_KEY_LEN], key2[BLAKE4_KEY_LEN];
    uint8_t hash1[BLAKE4_OUT_LEN], hash2[BLAKE4_OUT_LEN];

    memset(key1, 0x00, BLAKE4_KEY_LEN);
    memset(key2, 0xFF, BLAKE4_KEY_LEN);

    blake4_hash_keyed(key1, "test", 4, hash1);
    blake4_hash_keyed(key2, "test", 4, hash2);

    return memcmp(hash1, hash2, BLAKE4_OUT_LEN) != 0;
}

/* ============ Key Derivation Tests ============ */

TEST(derive_key_different_contexts) {
    uint8_t derived1[64], derived2[64];

    blake4_derive_key("context1", "material", 8, derived1, 64);
    blake4_derive_key("context2", "material", 8, derived2, 64);

    return memcmp(derived1, derived2, 64) != 0;
}

TEST(derive_key_different_materials) {
    uint8_t derived1[64], derived2[64];

    blake4_derive_key("context", "material1", 9, derived1, 64);
    blake4_derive_key("context", "material2", 9, derived2, 64);

    return memcmp(derived1, derived2, 64) != 0;
}

/* ============ HBS Function Tests ============ */

TEST(hbs_prf_deterministic) {
    uint8_t key[BLAKE4_KEY_LEN];
    uint8_t addr[32];
    uint8_t out1[64], out2[64];

    memset(key, 0x42, BLAKE4_KEY_LEN);
    memset(addr, 0x13, 32);

    blake4_hbs_prf(key, addr, out1, 64);
    blake4_hbs_prf(key, addr, out2, 64);

    return memcmp(out1, out2, 64) == 0;
}

TEST(hbs_functions_differ) {
    uint8_t seed[BLAKE4_OUT_LEN];
    uint8_t addr[32];
    uint8_t input[BLAKE4_OUT_LEN];
    uint8_t out_f[BLAKE4_OUT_LEN];
    uint8_t out_h[BLAKE4_OUT_LEN];

    memset(seed, 0x42, BLAKE4_OUT_LEN);
    memset(addr, 0x13, 32);
    memset(input, 0x37, BLAKE4_OUT_LEN);

    blake4_hbs_f(seed, addr, input, out_f);
    blake4_hbs_h(seed, addr, input, input, out_h);

    return memcmp(out_f, out_h, BLAKE4_OUT_LEN) != 0;
}

/* ============ Reset Tests ============ */

TEST(reset_clears_state) {
    uint8_t hash1[BLAKE4_OUT_LEN], hash2[BLAKE4_OUT_LEN];

    blake4_hasher h;
    blake4_hasher_init(&h);
    blake4_hasher_update(&h, "garbage", 7);
    blake4_hasher_reset(&h);
    blake4_hasher_update(&h, "test", 4);
    blake4_hasher_finalize(&h, hash1, BLAKE4_OUT_LEN);

    blake4_hash("test", 4, hash2);

    return memcmp(hash1, hash2, BLAKE4_OUT_LEN) == 0;
}

/* ============ Version API Tests ============ */

TEST(version_api) {
    const char *version = blake4_version_string();
    if (!version) return 0;
    if (strlen(version) == 0) return 0;

    int major = blake4_version_major();
    int minor = blake4_version_minor();
    int patch = blake4_version_patch();

    if (major < 0 || minor < 0 || patch < 0) return 0;

    return 1;
}

TEST(simd_info) {
    const char *simd = blake4_simd_name();
    if (!simd) return 0;
    /* blake4_simd_available() should return 0 or 1 */
    int avail = blake4_simd_available();
    if (avail != 0 && avail != 1) return 0;
    return 1;
}

int main(void) {
    printf("=== BLAKE4 API Contract Tests ===\n\n");

    printf("Edge Cases:\n");
    RUN_TEST(empty_input);
    RUN_TEST(null_pointer_with_zero_length);
    RUN_TEST(single_byte_inputs);
    RUN_TEST(different_single_bytes_differ);

    printf("\nBoundary Conditions:\n");
    RUN_TEST(block_boundary_127);
    RUN_TEST(block_boundary_128);
    RUN_TEST(block_boundary_129);
    RUN_TEST(chunk_boundary_2047);
    RUN_TEST(chunk_boundary_2048);
    RUN_TEST(chunk_boundary_2049);

    printf("\nIncremental Hashing:\n");
    RUN_TEST(incremental_byte_by_byte);
    RUN_TEST(incremental_various_chunk_sizes);
    RUN_TEST(multiple_empty_updates);

    printf("\nOutput Sizes:\n");
    RUN_TEST(output_size_1);
    RUN_TEST(output_size_32);
    RUN_TEST(output_size_48);
    RUN_TEST(output_size_64);
    RUN_TEST(output_size_large_xof);
    RUN_TEST(output_sizes_differ);

    printf("\nSerialization:\n");
    RUN_TEST(serialize_roundtrip_empty);
    RUN_TEST(serialize_roundtrip_partial);
    RUN_TEST(serialize_buffer_too_small);
    RUN_TEST(deserialize_invalid_magic);

    printf("\nKeyed Hashing:\n");
    RUN_TEST(keyed_differs_from_unkeyed);
    RUN_TEST(different_keys_differ);

    printf("\nKey Derivation:\n");
    RUN_TEST(derive_key_different_contexts);
    RUN_TEST(derive_key_different_materials);

    printf("\nHBS Functions:\n");
    RUN_TEST(hbs_prf_deterministic);
    RUN_TEST(hbs_functions_differ);

    printf("\nState Management:\n");
    RUN_TEST(reset_clears_state);

    printf("\nVersion/Info API:\n");
    RUN_TEST(version_api);
    RUN_TEST(simd_info);

    printf("\n=== Summary ===\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    printf("Total:  %d\n", tests_passed + tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
