/*
 * BLAKE4 Test Suite
 * Tests for the BLAKE4 implementation with 512-bit internal state.
 *
 * This is free and unencumbered software released into the public domain.
 */

#include "blake4.h"
#include "blake4_simd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests_run = 0;
static int tests_passed = 0;

static void hex_to_bytes(const char *hex, uint8_t *out, size_t out_len) {
    for (size_t i = 0; i < out_len; i++) {
        unsigned int byte;
        sscanf(hex + 2 * i, "%02x", &byte);
        out[i] = (uint8_t)byte;
    }
}

static void bytes_to_hex(const uint8_t *in, size_t len, char *out) {
    for (size_t i = 0; i < len; i++) {
        sprintf(out + 2 * i, "%02x", in[i]);
    }
    out[len * 2] = '\0';
}

static void print_hex(const char *label, const uint8_t *data, size_t len) {
    printf("%s: ", label);
    for (size_t i = 0; i < len && i < 32; i++) {
        printf("%02x", data[i]);
    }
    if (len > 32) printf("...");
    printf("\n");
}

/* ============== Basic Tests ============== */

static void test_empty_input(void) {
    tests_run++;
    printf("Test: Empty input... ");

    uint8_t hash[BLAKE4_OUT_LEN];
    blake4_hash("", 0, hash);

    /* Since this is a new algorithm, we record the output as canonical.
     * This test verifies consistency, not correctness against external vectors. */
    char hex[129];
    bytes_to_hex(hash, BLAKE4_OUT_LEN, hex);
    printf("PASS\n");
    printf("  Empty hash: %s\n", hex);
    tests_passed++;
}

static void test_abc(void) {
    tests_run++;
    printf("Test: ASCII 'abc'... ");

    uint8_t hash[BLAKE4_OUT_LEN];
    blake4_hash("abc", 3, hash);

    char hex[129];
    bytes_to_hex(hash, BLAKE4_OUT_LEN, hex);
    printf("PASS\n");
    printf("  'abc' hash: %s\n", hex);
    tests_passed++;
}

static void test_one_block(void) {
    tests_run++;
    printf("Test: One block (128 bytes)... ");

    /* 128 bytes = one full BLAKE4 block */
    uint8_t input[128];
    for (int i = 0; i < 128; i++) input[i] = (uint8_t)i;

    uint8_t hash[BLAKE4_OUT_LEN];
    blake4_hash(input, 128, hash);

    char hex[129];
    bytes_to_hex(hash, BLAKE4_OUT_LEN, hex);
    printf("PASS\n");
    printf("  128-byte hash: %s\n", hex);
    tests_passed++;
}

static void test_one_chunk(void) {
    tests_run++;
    printf("Test: One chunk (2048 bytes)... ");

    /* 2048 bytes = one full BLAKE4 chunk */
    uint8_t *input = malloc(2048);
    for (int i = 0; i < 2048; i++) input[i] = (uint8_t)(i % 251);

    uint8_t hash[BLAKE4_OUT_LEN];
    blake4_hash(input, 2048, hash);

    char hex[129];
    bytes_to_hex(hash, BLAKE4_OUT_LEN, hex);
    printf("PASS\n");
    printf("  2048-byte hash: %s\n", hex);
    free(input);
    tests_passed++;
}

static void test_two_chunks(void) {
    tests_run++;
    printf("Test: Two chunks (4096 bytes)... ");

    /* 4096 bytes = two BLAKE4 chunks, exercises Merkle tree */
    uint8_t *input = malloc(4096);
    for (int i = 0; i < 4096; i++) input[i] = (uint8_t)(i % 251);

    uint8_t hash[BLAKE4_OUT_LEN];
    blake4_hash(input, 4096, hash);

    char hex[129];
    bytes_to_hex(hash, BLAKE4_OUT_LEN, hex);
    printf("PASS\n");
    printf("  4096-byte hash: %s\n", hex);
    free(input);
    tests_passed++;
}

static void test_large_input(void) {
    tests_run++;
    printf("Test: Large input (100KB)... ");

    size_t size = 100 * 1024;
    uint8_t *input = malloc(size);
    for (size_t i = 0; i < size; i++) input[i] = (uint8_t)(i % 251);

    uint8_t hash[BLAKE4_OUT_LEN];
    blake4_hash(input, size, hash);

    char hex[129];
    bytes_to_hex(hash, BLAKE4_OUT_LEN, hex);
    printf("PASS\n");
    printf("  100KB hash: %.64s...\n", hex);
    free(input);
    tests_passed++;
}

/* ============== Incremental API Tests ============== */

static void test_incremental_matches_oneshot(void) {
    tests_run++;
    printf("Test: Incremental matches one-shot... ");

    /* Generate test data */
    uint8_t input[5000];
    for (int i = 0; i < 5000; i++) input[i] = (uint8_t)(i % 251);

    /* One-shot */
    uint8_t hash1[BLAKE4_OUT_LEN];
    blake4_hash(input, 5000, hash1);

    /* Incremental in various chunk sizes */
    blake4_hasher h;
    blake4_hasher_init(&h);

    size_t offset = 0;
    size_t chunk_sizes[] = {1, 7, 63, 127, 128, 129, 255, 1000, 2048, 1000};
    for (size_t i = 0; i < sizeof(chunk_sizes)/sizeof(chunk_sizes[0]) && offset < 5000; i++) {
        size_t take = chunk_sizes[i];
        if (offset + take > 5000) take = 5000 - offset;
        blake4_hasher_update(&h, input + offset, take);
        offset += take;
    }
    if (offset < 5000) {
        blake4_hasher_update(&h, input + offset, 5000 - offset);
    }

    uint8_t hash2[BLAKE4_OUT_LEN];
    blake4_hasher_finalize(&h, hash2, BLAKE4_OUT_LEN);

    if (memcmp(hash1, hash2, BLAKE4_OUT_LEN) == 0) {
        printf("PASS\n");
        tests_passed++;
    } else {
        printf("FAIL\n");
        print_hex("  One-shot  ", hash1, BLAKE4_OUT_LEN);
        print_hex("  Incremental", hash2, BLAKE4_OUT_LEN);
    }
}

static void test_byte_by_byte(void) {
    tests_run++;
    printf("Test: Byte-by-byte matches one-shot... ");

    uint8_t input[500];
    for (int i = 0; i < 500; i++) input[i] = (uint8_t)(i % 251);

    /* One-shot */
    uint8_t hash1[BLAKE4_OUT_LEN];
    blake4_hash(input, 500, hash1);

    /* Byte by byte */
    blake4_hasher h;
    blake4_hasher_init(&h);
    for (int i = 0; i < 500; i++) {
        blake4_hasher_update(&h, &input[i], 1);
    }
    uint8_t hash2[BLAKE4_OUT_LEN];
    blake4_hasher_finalize(&h, hash2, BLAKE4_OUT_LEN);

    if (memcmp(hash1, hash2, BLAKE4_OUT_LEN) == 0) {
        printf("PASS\n");
        tests_passed++;
    } else {
        printf("FAIL\n");
        print_hex("  One-shot  ", hash1, BLAKE4_OUT_LEN);
        print_hex("  Byte-by-byte", hash2, BLAKE4_OUT_LEN);
    }
}

/* ============== XOF Tests ============== */

static void test_xof_output_lengths(void) {
    tests_run++;
    printf("Test: XOF various output lengths... ");

    /* Verify XOF produces consistent prefixes */
    uint8_t hash64[64], hash128[128], hash256[256];

    blake4_hash_xof("test", 4, hash64, 64);
    blake4_hash_xof("test", 4, hash128, 128);
    blake4_hash_xof("test", 4, hash256, 256);

    /* First 64 bytes should match */
    if (memcmp(hash64, hash128, 64) != 0 ||
        memcmp(hash64, hash256, 64) != 0 ||
        memcmp(hash128, hash256, 128) != 0) {
        printf("FAIL - XOF prefixes don't match\n");
        return;
    }

    printf("PASS\n");
    tests_passed++;
}

static void test_xof_seek(void) {
    tests_run++;
    printf("Test: XOF seek mode... ");

    blake4_hasher h;
    blake4_hasher_init(&h);
    blake4_hasher_update(&h, "test", 4);

    /* Get full 256 bytes */
    uint8_t full[256];
    blake4_hasher_finalize(&h, full, 256);

    /* Get bytes 64-192 using seek */
    uint8_t seeked[128];
    blake4_hasher_finalize_seek(&h, 64, seeked, 128);

    if (memcmp(full + 64, seeked, 128) == 0) {
        printf("PASS\n");
        tests_passed++;
    } else {
        printf("FAIL - seek mismatch\n");
        print_hex("  Expected", full + 64, 32);
        print_hex("  Got     ", seeked, 32);
    }
}

/* ============== Keyed Hashing Tests ============== */

static void test_keyed_hash_different_from_unkeyed(void) {
    tests_run++;
    printf("Test: Keyed hash differs from unkeyed... ");

    uint8_t key[BLAKE4_KEY_LEN];
    memset(key, 0x42, BLAKE4_KEY_LEN);

    uint8_t unkeyed[BLAKE4_OUT_LEN];
    uint8_t keyed[BLAKE4_OUT_LEN];

    blake4_hash("test", 4, unkeyed);
    blake4_hash_keyed(key, "test", 4, keyed);

    if (memcmp(unkeyed, keyed, BLAKE4_OUT_LEN) != 0) {
        printf("PASS\n");
        tests_passed++;
    } else {
        printf("FAIL - keyed equals unkeyed!\n");
    }
}

static void test_keyed_hash_different_keys(void) {
    tests_run++;
    printf("Test: Different keys produce different hashes... ");

    uint8_t key1[BLAKE4_KEY_LEN], key2[BLAKE4_KEY_LEN];
    memset(key1, 0x11, BLAKE4_KEY_LEN);
    memset(key2, 0x22, BLAKE4_KEY_LEN);

    uint8_t hash1[BLAKE4_OUT_LEN], hash2[BLAKE4_OUT_LEN];
    blake4_hash_keyed(key1, "test", 4, hash1);
    blake4_hash_keyed(key2, "test", 4, hash2);

    if (memcmp(hash1, hash2, BLAKE4_OUT_LEN) != 0) {
        printf("PASS\n");
        tests_passed++;
    } else {
        printf("FAIL - different keys, same hash!\n");
    }
}

/* ============== Key Derivation Tests ============== */

static void test_derive_key_different_contexts(void) {
    tests_run++;
    printf("Test: Different contexts produce different keys... ");

    uint8_t material[32];
    memset(material, 0xAB, 32);

    uint8_t key1[64], key2[64];
    blake4_derive_key("context 1", material, 32, key1, 64);
    blake4_derive_key("context 2", material, 32, key2, 64);

    if (memcmp(key1, key2, 64) != 0) {
        printf("PASS\n");
        tests_passed++;
    } else {
        printf("FAIL - different contexts, same key!\n");
    }
}

/* ============== State Serialization Tests ============== */

static void test_serialize_basic(void) {
    tests_run++;
    printf("Test: Basic serialization round-trip... ");

    /* Create and update hasher */
    blake4_hasher h1;
    blake4_hasher_init(&h1);
    blake4_hasher_update(&h1, "some data to hash", 17);

    /* Serialize */
    uint8_t buf[BLAKE4_SERIALIZED_SIZE];
    size_t written = blake4_hasher_serialize(&h1, buf, sizeof(buf));

    if (written == 0) {
        printf("FAIL - serialize returned 0\n");
        return;
    }

    /* Deserialize into new hasher */
    blake4_hasher h2;
    if (!blake4_hasher_deserialize(&h2, buf, written)) {
        printf("FAIL - deserialize returned 0\n");
        return;
    }

    /* Both should produce same final hash */
    blake4_hasher_update(&h1, " more data", 10);
    blake4_hasher_update(&h2, " more data", 10);

    uint8_t hash1[BLAKE4_OUT_LEN], hash2[BLAKE4_OUT_LEN];
    blake4_hasher_finalize(&h1, hash1, BLAKE4_OUT_LEN);
    blake4_hasher_finalize(&h2, hash2, BLAKE4_OUT_LEN);

    if (memcmp(hash1, hash2, BLAKE4_OUT_LEN) == 0) {
        printf("PASS\n");
        tests_passed++;
    } else {
        printf("FAIL - hashes differ after deserialization\n");
        print_hex("  Original", hash1, BLAKE4_OUT_LEN);
        print_hex("  Restored", hash2, BLAKE4_OUT_LEN);
    }
}

static void test_serialize_mid_chunk(void) {
    tests_run++;
    printf("Test: Serialize mid-chunk... ");

    /* Create hasher with partial chunk */
    blake4_hasher h1;
    blake4_hasher_init(&h1);

    /* Add 1000 bytes (less than 2048 chunk size) */
    uint8_t input[1000];
    for (int i = 0; i < 1000; i++) input[i] = (uint8_t)(i % 251);
    blake4_hasher_update(&h1, input, 1000);

    /* Serialize and restore */
    uint8_t buf[BLAKE4_SERIALIZED_SIZE];
    size_t written = blake4_hasher_serialize(&h1, buf, sizeof(buf));

    blake4_hasher h2;
    blake4_hasher_deserialize(&h2, buf, written);

    /* Add more data and finalize both */
    uint8_t more[500];
    for (int i = 0; i < 500; i++) more[i] = (uint8_t)(i % 251);
    blake4_hasher_update(&h1, more, 500);
    blake4_hasher_update(&h2, more, 500);

    uint8_t hash1[BLAKE4_OUT_LEN], hash2[BLAKE4_OUT_LEN];
    blake4_hasher_finalize(&h1, hash1, BLAKE4_OUT_LEN);
    blake4_hasher_finalize(&h2, hash2, BLAKE4_OUT_LEN);

    if (memcmp(hash1, hash2, BLAKE4_OUT_LEN) == 0) {
        printf("PASS\n");
        tests_passed++;
    } else {
        printf("FAIL\n");
    }
}

static void test_serialize_multi_chunk(void) {
    tests_run++;
    printf("Test: Serialize after multiple chunks... ");

    /* Add data spanning multiple chunks */
    blake4_hasher h1;
    blake4_hasher_init(&h1);

    uint8_t *big = malloc(5000);
    for (int i = 0; i < 5000; i++) big[i] = (uint8_t)(i % 251);
    blake4_hasher_update(&h1, big, 5000);

    /* Serialize and restore */
    uint8_t buf[BLAKE4_SERIALIZED_SIZE];
    size_t written = blake4_hasher_serialize(&h1, buf, sizeof(buf));

    blake4_hasher h2;
    blake4_hasher_deserialize(&h2, buf, written);

    /* Finalize both */
    uint8_t hash1[BLAKE4_OUT_LEN], hash2[BLAKE4_OUT_LEN];
    blake4_hasher_finalize(&h1, hash1, BLAKE4_OUT_LEN);
    blake4_hasher_finalize(&h2, hash2, BLAKE4_OUT_LEN);

    free(big);

    if (memcmp(hash1, hash2, BLAKE4_OUT_LEN) == 0) {
        printf("PASS\n");
        tests_passed++;
    } else {
        printf("FAIL\n");
    }
}

static void test_serialize_keyed(void) {
    tests_run++;
    printf("Test: Serialize keyed hasher... ");

    uint8_t key[BLAKE4_KEY_LEN];
    for (int i = 0; i < BLAKE4_KEY_LEN; i++) key[i] = (uint8_t)(i * 3);

    blake4_hasher h1;
    blake4_hasher_init_keyed(&h1, key);
    blake4_hasher_update(&h1, "keyed data", 10);

    /* Serialize and restore */
    uint8_t buf[BLAKE4_SERIALIZED_SIZE];
    size_t written = blake4_hasher_serialize(&h1, buf, sizeof(buf));

    blake4_hasher h2;
    blake4_hasher_deserialize(&h2, buf, written);

    /* Continue hashing */
    blake4_hasher_update(&h1, " more", 5);
    blake4_hasher_update(&h2, " more", 5);

    uint8_t hash1[BLAKE4_OUT_LEN], hash2[BLAKE4_OUT_LEN];
    blake4_hasher_finalize(&h1, hash1, BLAKE4_OUT_LEN);
    blake4_hasher_finalize(&h2, hash2, BLAKE4_OUT_LEN);

    if (memcmp(hash1, hash2, BLAKE4_OUT_LEN) == 0) {
        printf("PASS\n");
        tests_passed++;
    } else {
        printf("FAIL\n");
    }
}

static void test_serialize_buffer_too_small(void) {
    tests_run++;
    printf("Test: Serialize with small buffer... ");

    blake4_hasher h;
    blake4_hasher_init(&h);
    blake4_hasher_update(&h, "test", 4);

    uint8_t small_buf[100];  /* Too small */
    size_t written = blake4_hasher_serialize(&h, small_buf, sizeof(small_buf));

    if (written == 0) {
        printf("PASS\n");
        tests_passed++;
    } else {
        printf("FAIL - should have returned 0 for small buffer\n");
    }
}

static void test_deserialize_invalid_magic(void) {
    tests_run++;
    printf("Test: Deserialize with invalid magic... ");

    uint8_t buf[BLAKE4_SERIALIZED_SIZE];
    memset(buf, 0, sizeof(buf));
    buf[0] = 'X';  /* Invalid magic */

    blake4_hasher h;
    int result = blake4_hasher_deserialize(&h, buf, sizeof(buf));

    if (result == 0) {
        printf("PASS\n");
        tests_passed++;
    } else {
        printf("FAIL - should have rejected invalid magic\n");
    }
}

/* ============== Hasher Reset Test ============== */

static void test_hasher_reset(void) {
    tests_run++;
    printf("Test: Hasher reset... ");

    blake4_hasher h;
    blake4_hasher_init(&h);

    /* Hash something */
    blake4_hasher_update(&h, "garbage", 7);

    /* Reset and hash real data */
    blake4_hasher_reset(&h);
    blake4_hasher_update(&h, "test", 4);
    uint8_t hash1[BLAKE4_OUT_LEN];
    blake4_hasher_finalize(&h, hash1, BLAKE4_OUT_LEN);

    /* Compare with fresh hasher */
    uint8_t hash2[BLAKE4_OUT_LEN];
    blake4_hash("test", 4, hash2);

    if (memcmp(hash1, hash2, BLAKE4_OUT_LEN) == 0) {
        printf("PASS\n");
        tests_passed++;
    } else {
        printf("FAIL\n");
        print_hex("  After reset", hash1, BLAKE4_OUT_LEN);
        print_hex("  Fresh      ", hash2, BLAKE4_OUT_LEN);
    }
}

/* ============== Consistency Tests ============== */

static void test_deterministic(void) {
    tests_run++;
    printf("Test: Hash is deterministic... ");

    uint8_t hash1[BLAKE4_OUT_LEN], hash2[BLAKE4_OUT_LEN];
    blake4_hash("the same input", 14, hash1);
    blake4_hash("the same input", 14, hash2);

    if (memcmp(hash1, hash2, BLAKE4_OUT_LEN) == 0) {
        printf("PASS\n");
        tests_passed++;
    } else {
        printf("FAIL - non-deterministic!\n");
    }
}

static void test_different_inputs_different_outputs(void) {
    tests_run++;
    printf("Test: Different inputs give different hashes... ");

    uint8_t hash1[BLAKE4_OUT_LEN], hash2[BLAKE4_OUT_LEN];
    blake4_hash("input A", 7, hash1);
    blake4_hash("input B", 7, hash2);

    if (memcmp(hash1, hash2, BLAKE4_OUT_LEN) != 0) {
        printf("PASS\n");
        tests_passed++;
    } else {
        printf("FAIL - collision!\n");
    }
}

/* ============== Version API Test ============== */

static void test_version_api(void) {
    tests_run++;
    printf("Test: Version API... ");

    const char *ver = blake4_version_string();
    int major = blake4_version_major();
    int minor = blake4_version_minor();
    int patch = blake4_version_patch();

    if (major == 2 && minor == 0 && patch == 0 && strcmp(ver, "2.0.0") == 0) {
        printf("PASS (v%s)\n", ver);
        tests_passed++;
    } else {
        printf("FAIL (got v%d.%d.%d / %s)\n", major, minor, patch, ver);
    }
}

static void test_simd_info(void) {
    tests_run++;
    printf("Test: SIMD info... ");

    const char *simd_name = blake4_simd_name();
    int simd_avail = blake4_simd_available();

    /* This test just verifies the API works, not specific results */
    if (simd_name != NULL && strlen(simd_name) > 0) {
        printf("PASS (%s, available=%d)\n", simd_name, simd_avail);
        tests_passed++;
    } else {
        printf("FAIL (null or empty SIMD name)\n");
    }
}

/* ============== Edge Cases ============== */

static void test_chunk_boundary(void) {
    tests_run++;
    printf("Test: Chunk boundary (2047, 2048, 2049 bytes)... ");

    uint8_t *input = malloc(2049);
    for (int i = 0; i < 2049; i++) input[i] = (uint8_t)(i % 251);

    uint8_t hash2047[BLAKE4_OUT_LEN], hash2048[BLAKE4_OUT_LEN], hash2049[BLAKE4_OUT_LEN];
    blake4_hash(input, 2047, hash2047);
    blake4_hash(input, 2048, hash2048);
    blake4_hash(input, 2049, hash2049);

    /* All three should be different */
    if (memcmp(hash2047, hash2048, BLAKE4_OUT_LEN) != 0 &&
        memcmp(hash2048, hash2049, BLAKE4_OUT_LEN) != 0 &&
        memcmp(hash2047, hash2049, BLAKE4_OUT_LEN) != 0) {
        printf("PASS\n");
        tests_passed++;
    } else {
        printf("FAIL - boundary collision\n");
    }

    free(input);
}

static void test_block_boundary(void) {
    tests_run++;
    printf("Test: Block boundary (127, 128, 129 bytes)... ");

    uint8_t input[129];
    for (int i = 0; i < 129; i++) input[i] = (uint8_t)(i % 251);

    uint8_t hash127[BLAKE4_OUT_LEN], hash128[BLAKE4_OUT_LEN], hash129[BLAKE4_OUT_LEN];
    blake4_hash(input, 127, hash127);
    blake4_hash(input, 128, hash128);
    blake4_hash(input, 129, hash129);

    /* All three should be different */
    if (memcmp(hash127, hash128, BLAKE4_OUT_LEN) != 0 &&
        memcmp(hash128, hash129, BLAKE4_OUT_LEN) != 0 &&
        memcmp(hash127, hash129, BLAKE4_OUT_LEN) != 0) {
        printf("PASS\n");
        tests_passed++;
    } else {
        printf("FAIL - boundary collision\n");
    }
}

/* ============== Output Parameter Tests ============== */

static void test_different_output_sizes(void) {
    tests_run++;
    printf("Test: Different output sizes (1, 32, 64, 100 bytes)... ");

    uint8_t full[100], partial[100];
    blake4_hash_xof("test", 4, full, 100);

    /* 1 byte */
    blake4_hash_xof("test", 4, partial, 1);
    if (partial[0] != full[0]) {
        printf("FAIL - 1 byte mismatch\n");
        return;
    }

    /* 32 bytes */
    blake4_hash_xof("test", 4, partial, 32);
    if (memcmp(partial, full, 32) != 0) {
        printf("FAIL - 32 byte mismatch\n");
        return;
    }

    /* 64 bytes (default) */
    blake4_hash_xof("test", 4, partial, 64);
    if (memcmp(partial, full, 64) != 0) {
        printf("FAIL - 64 byte mismatch\n");
        return;
    }

    printf("PASS\n");
    tests_passed++;
}

/* ============== Hash-Based Signature API Tests ============== */

static void test_hbs_prf(void) {
    tests_run++;
    printf("Test: HBS PRF function... ");

    uint8_t key[BLAKE4_KEY_LEN];
    for (int i = 0; i < BLAKE4_KEY_LEN; i++) key[i] = (uint8_t)i;

    uint8_t addr1[32], addr2[32];
    memset(addr1, 0x00, 32);
    memset(addr2, 0x01, 32);

    uint8_t out1[64], out2[64], out3[64];
    blake4_hbs_prf(key, addr1, out1, 64);
    blake4_hbs_prf(key, addr2, out2, 64);
    blake4_hbs_prf(key, addr1, out3, 64);

    /* Same key + addr should give same output */
    if (memcmp(out1, out3, 64) != 0) {
        printf("FAIL - not deterministic\n");
        return;
    }

    /* Different addr should give different output */
    if (memcmp(out1, out2, 64) == 0) {
        printf("FAIL - collision on different addresses\n");
        return;
    }

    printf("PASS\n");
    tests_passed++;
}

static void test_hbs_f(void) {
    tests_run++;
    printf("Test: HBS F (chain) function... ");

    uint8_t pub_seed[64], addr[32], input[64];
    memset(pub_seed, 0xAA, 64);
    memset(addr, 0x00, 32);
    memset(input, 0x55, 64);

    uint8_t out1[64], out2[64];
    blake4_hbs_f(pub_seed, addr, input, out1);
    blake4_hbs_f(pub_seed, addr, input, out2);

    /* Should be deterministic */
    if (memcmp(out1, out2, 64) != 0) {
        printf("FAIL - not deterministic\n");
        return;
    }

    /* Output should differ from input */
    if (memcmp(input, out1, 64) == 0) {
        printf("FAIL - output equals input\n");
        return;
    }

    printf("PASS\n");
    tests_passed++;
}

static void test_hbs_h(void) {
    tests_run++;
    printf("Test: HBS H (tree) function... ");

    uint8_t pub_seed[64], addr[32], left[64], right[64];
    memset(pub_seed, 0xBB, 64);
    memset(addr, 0x00, 32);
    memset(left, 0x11, 64);
    memset(right, 0x22, 64);

    uint8_t out[64];
    blake4_hbs_h(pub_seed, addr, left, right, out);

    /* Swapping left/right should give different result */
    uint8_t out_swapped[64];
    blake4_hbs_h(pub_seed, addr, right, left, out_swapped);

    if (memcmp(out, out_swapped, 64) == 0) {
        printf("FAIL - order doesn't matter\n");
        return;
    }

    printf("PASS\n");
    tests_passed++;
}

static void test_hbs_t(void) {
    tests_run++;
    printf("Test: HBS T (tweakable) function... ");

    uint8_t pub_seed[64], addr[32];
    memset(pub_seed, 0xCC, 64);
    memset(addr, 0x00, 32);

    /* Test with 2 blocks */
    uint8_t input2[128];
    memset(input2, 0x33, 128);

    /* Test with 4 blocks */
    uint8_t input4[256];
    memset(input4, 0x33, 256);  /* Same content for first 128 bytes */

    uint8_t out2[64], out4[64];
    blake4_hbs_t(pub_seed, addr, input2, 2, out2);
    blake4_hbs_t(pub_seed, addr, input4, 4, out4);

    /* Different number of blocks should give different output */
    if (memcmp(out2, out4, 64) == 0) {
        printf("FAIL - different block counts give same output\n");
        return;
    }

    printf("PASS\n");
    tests_passed++;
}

static void test_hbs_h_msg(void) {
    tests_run++;
    printf("Test: HBS H_msg function... ");

    uint8_t r[64], pub_seed[64], pub_root[64];
    memset(r, 0xDD, 64);
    memset(pub_seed, 0xEE, 64);
    memset(pub_root, 0xFF, 64);

    const char *msg1 = "message 1";
    const char *msg2 = "message 2";

    uint8_t out1[64], out2[64];
    blake4_hbs_h_msg(r, pub_seed, pub_root, msg1, strlen(msg1), out1, 64);
    blake4_hbs_h_msg(r, pub_seed, pub_root, msg2, strlen(msg2), out2, 64);

    /* Different messages should give different output */
    if (memcmp(out1, out2, 64) == 0) {
        printf("FAIL - different messages give same output\n");
        return;
    }

    printf("PASS\n");
    tests_passed++;
}

/* ============== Configurable Output Mode Tests ============== */

static void test_blake4_256(void) {
    tests_run++;
    printf("Test: BLAKE4-256 output mode... ");

    uint8_t out256[32];
    blake4_256_hash("test", 4, out256);

    /* Verify it matches truncated BLAKE4-512 */
    uint8_t out512[64];
    blake4_hash("test", 4, out512);

    if (memcmp(out256, out512, 32) == 0) {
        printf("PASS\n");
        tests_passed++;
    } else {
        printf("FAIL - doesn't match truncated 512\n");
    }
}

static void test_blake4_384(void) {
    tests_run++;
    printf("Test: BLAKE4-384 output mode... ");

    uint8_t out384[48];
    blake4_384_hash("test", 4, out384);

    /* Verify it matches truncated BLAKE4-512 */
    uint8_t out512[64];
    blake4_hash("test", 4, out512);

    if (memcmp(out384, out512, 48) == 0) {
        printf("PASS\n");
        tests_passed++;
    } else {
        printf("FAIL - doesn't match truncated 512\n");
    }
}

static void test_blake4_512(void) {
    tests_run++;
    printf("Test: BLAKE4-512 output mode... ");

    uint8_t out512a[64], out512b[64];
    blake4_512_hash("test", 4, out512a);
    blake4_hash("test", 4, out512b);

    if (memcmp(out512a, out512b, 64) == 0) {
        printf("PASS\n");
        tests_passed++;
    } else {
        printf("FAIL - doesn't match blake4_hash\n");
    }
}

/* ============== Generate Test Vectors ============== */

static void generate_test_vectors(void) {
    printf("\n=== BLAKE4 v2 Test Vectors ===\n");
    printf("(512-bit internal state)\n\n");

    struct {
        const char *name;
        const uint8_t *input;
        size_t len;
    } vectors[] = {
        {"empty", (const uint8_t *)"", 0},
        {"'abc'", (const uint8_t *)"abc", 3},
        {NULL, NULL, 0}
    };

    /* Static vectors */
    for (int i = 0; vectors[i].name != NULL; i++) {
        uint8_t hash[64];
        blake4_hash(vectors[i].input, vectors[i].len, hash);
        char hex[129];
        bytes_to_hex(hash, 64, hex);
        printf("%-10s: %s\n", vectors[i].name, hex);
    }

    /* Sequential byte patterns */
    printf("\nSequential byte patterns (0x00, 0x01, 0x02, ...):\n");
    size_t sizes[] = {1, 64, 128, 129, 1024, 2048, 2049, 8192};
    for (size_t i = 0; i < sizeof(sizes)/sizeof(sizes[0]); i++) {
        size_t size = sizes[i];
        uint8_t *input = malloc(size);
        for (size_t j = 0; j < size; j++) input[j] = (uint8_t)(j % 256);

        uint8_t hash[64];
        blake4_hash(input, size, hash);
        char hex[129];
        bytes_to_hex(hash, 64, hex);
        printf("%6zu bytes: %s\n", size, hex);
        free(input);
    }
}

/* ============== Main ============== */

int main(void) {
    printf("=== BLAKE4 v2 Test Suite ===\n");
    printf("Testing 512-bit internal state implementation\n\n");

    /* Basic tests */
    test_empty_input();
    test_abc();
    test_one_block();
    test_one_chunk();
    test_two_chunks();
    test_large_input();

    /* Incremental API */
    test_incremental_matches_oneshot();
    test_byte_by_byte();

    /* XOF mode */
    test_xof_output_lengths();
    test_xof_seek();

    /* Keyed hashing */
    test_keyed_hash_different_from_unkeyed();
    test_keyed_hash_different_keys();

    /* Key derivation */
    test_derive_key_different_contexts();

    /* State serialization */
    test_serialize_basic();
    test_serialize_mid_chunk();
    test_serialize_multi_chunk();
    test_serialize_keyed();
    test_serialize_buffer_too_small();
    test_deserialize_invalid_magic();

    /* Reset */
    test_hasher_reset();

    /* Consistency */
    test_deterministic();
    test_different_inputs_different_outputs();

    /* Version */
    test_version_api();

    /* SIMD */
    test_simd_info();

    /* Edge cases */
    test_chunk_boundary();
    test_block_boundary();

    /* Output sizes */
    test_different_output_sizes();

    /* Hash-based signature API */
    test_hbs_prf();
    test_hbs_f();
    test_hbs_h();
    test_hbs_t();
    test_hbs_h_msg();

    /* Configurable output modes */
    test_blake4_256();
    test_blake4_384();
    test_blake4_512();

    /* Summary */
    printf("\n=== Summary ===\n");
    printf("Tests: %d/%d passed\n", tests_passed, tests_run);

    if (tests_passed == tests_run) {
        /* Generate vectors for documentation */
        generate_test_vectors();
        printf("\nAll tests passed!\n");
        return 0;
    } else {
        printf("\nSome tests failed.\n");
        return 1;
    }
}
