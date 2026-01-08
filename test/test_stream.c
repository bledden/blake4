/*
 * BLAKE4-STREAM Test Suite
 *
 * Tests verified streaming functionality including tree encoding,
 * verification, and slice operations.
 */

#include "../include/blake4.h"
#include "../include/blake4_stream.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_PASS 0
#define TEST_FAIL 1

static int tests_run = 0;
static int tests_passed = 0;

#define RUN_TEST(test_func) do { \
    tests_run++; \
    printf("  %-50s ", #test_func); \
    fflush(stdout); \
    if (test_func() == TEST_PASS) { \
        tests_passed++; \
        printf("[PASS]\n"); \
    } else { \
        printf("[FAIL]\n"); \
    } \
} while(0)

/* Hex printing for debug */
static void print_hex(const char *label, const uint8_t *data, size_t len) {
    printf("%s: ", label);
    for (size_t i = 0; i < len && i < 32; i++) {
        printf("%02x", data[i]);
    }
    if (len > 32) printf("...");
    printf("\n");
}

/* ============== Utility Tests ============== */

static int test_num_chunks(void) {
    /* 0 bytes = 1 chunk (empty chunk) */
    if (blake4_stream_num_chunks(0) != 1) return TEST_FAIL;

    /* 1 byte = 1 chunk */
    if (blake4_stream_num_chunks(1) != 1) return TEST_FAIL;

    /* 2048 bytes = 1 chunk (BLAKE4 chunk size) */
    if (blake4_stream_num_chunks(2048) != 1) return TEST_FAIL;

    /* 2049 bytes = 2 chunks */
    if (blake4_stream_num_chunks(2049) != 2) return TEST_FAIL;

    /* 4096 bytes = 2 chunks */
    if (blake4_stream_num_chunks(4096) != 2) return TEST_FAIL;

    /* 4097 bytes = 3 chunks */
    if (blake4_stream_num_chunks(4097) != 3) return TEST_FAIL;

    /* 1 MB = 512 chunks (1MB / 2048) */
    if (blake4_stream_num_chunks(1024 * 1024) != 512) return TEST_FAIL;

    return TEST_PASS;
}

static int test_tree_depth(void) {
    /* 1 chunk = depth 0 */
    if (blake4_stream_tree_depth(1) != 0) return TEST_FAIL;

    /* 2 chunks = depth 1 */
    if (blake4_stream_tree_depth(2) != 1) return TEST_FAIL;

    /* 3-4 chunks = depth 2 */
    if (blake4_stream_tree_depth(3) != 2) return TEST_FAIL;
    if (blake4_stream_tree_depth(4) != 2) return TEST_FAIL;

    /* 5-8 chunks = depth 3 */
    if (blake4_stream_tree_depth(5) != 3) return TEST_FAIL;
    if (blake4_stream_tree_depth(8) != 3) return TEST_FAIL;

    /* 512 chunks = depth 9 */
    if (blake4_stream_tree_depth(512) != 9) return TEST_FAIL;

    return TEST_PASS;
}

static int test_tree_data_size(void) {
    /* Single chunk = 0 internal nodes */
    if (blake4_stream_tree_data_size(2048) != 0) return TEST_FAIL;

    /* 2 chunks = 1 internal node = 64 bytes (BLAKE4 hash size) */
    if (blake4_stream_tree_data_size(4096) != 64) return TEST_FAIL;

    /* 4 chunks = 3 internal nodes = 192 bytes */
    if (blake4_stream_tree_data_size(8192) != 192) return TEST_FAIL;

    return TEST_PASS;
}

/* ============== Encoder Tests ============== */

static int test_encoder_empty(void) {
    blake4_stream_encoder *enc = blake4_stream_encoder_new();
    if (!enc) return TEST_FAIL;

    uint8_t *tree = NULL;
    size_t tree_len = 0;
    uint8_t root_hash[BLAKE4_STREAM_HASH_LEN];

    int result = blake4_stream_encoder_finalize(enc, &tree, &tree_len, root_hash);
    blake4_stream_encoder_free(enc);

    if (result != BLAKE4_STREAM_OK) {
        free(tree);
        return TEST_FAIL;
    }

    /* Empty input should have tree with just header */
    if (tree_len != BLAKE4_STREAM_TREE_HEADER_LEN) {
        printf("\n    Expected tree_len=%d, got %zu\n",
               BLAKE4_STREAM_TREE_HEADER_LEN, tree_len);
        free(tree);
        return TEST_FAIL;
    }

    /* Verify magic */
    if (memcmp(tree, "BK4T", 4) != 0) {
        free(tree);
        return TEST_FAIL;
    }

    free(tree);
    return TEST_PASS;
}

static int test_encoder_single_chunk(void) {
    blake4_stream_encoder *enc = blake4_stream_encoder_new();
    if (!enc) return TEST_FAIL;

    uint8_t data[512];
    memset(data, 0x42, sizeof(data));

    blake4_stream_encoder_update(enc, data, sizeof(data));

    uint8_t *tree = NULL;
    size_t tree_len = 0;
    uint8_t root_hash[BLAKE4_STREAM_HASH_LEN];

    int result = blake4_stream_encoder_finalize(enc, &tree, &tree_len, root_hash);
    blake4_stream_encoder_free(enc);

    if (result != BLAKE4_STREAM_OK) {
        free(tree);
        return TEST_FAIL;
    }

    /* Single chunk = header only */
    if (tree_len != BLAKE4_STREAM_TREE_HEADER_LEN) {
        printf("\n    Expected tree_len=%d, got %zu\n",
               BLAKE4_STREAM_TREE_HEADER_LEN, tree_len);
        free(tree);
        return TEST_FAIL;
    }

    free(tree);
    return TEST_PASS;
}

static int test_encoder_two_chunks(void) {
    blake4_stream_encoder *enc = blake4_stream_encoder_new();
    if (!enc) return TEST_FAIL;

    /* 4096 bytes = 2 chunks (2048 bytes per chunk) */
    uint8_t data[4096];
    memset(data, 0x42, sizeof(data));

    blake4_stream_encoder_update(enc, data, sizeof(data));

    uint8_t *tree = NULL;
    size_t tree_len = 0;
    uint8_t root_hash[BLAKE4_STREAM_HASH_LEN];

    int result = blake4_stream_encoder_finalize(enc, &tree, &tree_len, root_hash);
    blake4_stream_encoder_free(enc);

    if (result != BLAKE4_STREAM_OK) {
        free(tree);
        return TEST_FAIL;
    }

    /* 2 chunks = 1 internal node = header + 64 bytes */
    size_t expected_len = BLAKE4_STREAM_TREE_HEADER_LEN + BLAKE4_STREAM_HASH_LEN;
    if (tree_len != expected_len) {
        printf("\n    Expected tree_len=%zu, got %zu\n", expected_len, tree_len);
        free(tree);
        return TEST_FAIL;
    }

    free(tree);
    return TEST_PASS;
}

static int test_encoder_incremental(void) {
    /* Test that incremental updates produce same result as one-shot */
    uint8_t data[3000];
    for (size_t i = 0; i < sizeof(data); i++) {
        data[i] = (uint8_t)i;
    }

    /* One-shot encode */
    uint8_t *tree1 = NULL;
    size_t tree_len1 = 0;
    uint8_t root1[BLAKE4_STREAM_HASH_LEN];

    blake4_stream_encoder *enc1 = blake4_stream_encoder_new();
    blake4_stream_encoder_update(enc1, data, sizeof(data));
    blake4_stream_encoder_finalize(enc1, &tree1, &tree_len1, root1);
    blake4_stream_encoder_free(enc1);

    /* Incremental encode (small chunks) */
    uint8_t *tree2 = NULL;
    size_t tree_len2 = 0;
    uint8_t root2[BLAKE4_STREAM_HASH_LEN];

    blake4_stream_encoder *enc2 = blake4_stream_encoder_new();
    size_t offset = 0;
    while (offset < sizeof(data)) {
        size_t chunk = 17;  /* Odd chunk size to stress test */
        if (offset + chunk > sizeof(data)) {
            chunk = sizeof(data) - offset;
        }
        blake4_stream_encoder_update(enc2, data + offset, chunk);
        offset += chunk;
    }
    blake4_stream_encoder_finalize(enc2, &tree2, &tree_len2, root2);
    blake4_stream_encoder_free(enc2);

    /* Compare results */
    int result = TEST_PASS;
    if (memcmp(root1, root2, BLAKE4_STREAM_HASH_LEN) != 0) {
        printf("\n    Root hashes differ\n");
        result = TEST_FAIL;
    }
    if (tree_len1 != tree_len2) {
        printf("\n    Tree lengths differ: %zu vs %zu\n", tree_len1, tree_len2);
        result = TEST_FAIL;
    }

    free(tree1);
    free(tree2);
    return result;
}

/* ============== One-shot Encode Tests ============== */

static int test_encode_empty(void) {
    uint8_t *tree = NULL;
    size_t tree_len = 0;
    uint8_t root_hash[BLAKE4_STREAM_HASH_LEN];

    int result = blake4_stream_encode(NULL, 0, &tree, &tree_len, root_hash);
    if (result != BLAKE4_STREAM_OK) {
        free(tree);
        return TEST_FAIL;
    }

    free(tree);
    return TEST_PASS;
}

static int test_encode_large(void) {
    /* 64 KB = 32 chunks (2048 bytes per chunk) */
    size_t data_len = 64 * 1024;
    uint8_t *data = (uint8_t *)malloc(data_len);
    if (!data) return TEST_FAIL;

    for (size_t i = 0; i < data_len; i++) {
        data[i] = (uint8_t)(i * 7 + 13);
    }

    uint8_t *tree = NULL;
    size_t tree_len = 0;
    uint8_t root_hash[BLAKE4_STREAM_HASH_LEN];

    int result = blake4_stream_encode(data, data_len, &tree, &tree_len, root_hash);
    free(data);

    if (result != BLAKE4_STREAM_OK) {
        free(tree);
        return TEST_FAIL;
    }

    /* 32 chunks = 31 internal nodes = header + 31*64 bytes */
    size_t expected_len = BLAKE4_STREAM_TREE_HEADER_LEN + 31 * BLAKE4_STREAM_HASH_LEN;
    if (tree_len != expected_len) {
        printf("\n    Expected tree_len=%zu, got %zu\n", expected_len, tree_len);
        free(tree);
        return TEST_FAIL;
    }

    free(tree);
    return TEST_PASS;
}

/* ============== Verification Tests ============== */

static int test_verify_empty(void) {
    uint8_t *tree = NULL;
    size_t tree_len = 0;
    uint8_t root_hash[BLAKE4_STREAM_HASH_LEN];

    blake4_stream_encode(NULL, 0, &tree, &tree_len, root_hash);

    int result = blake4_stream_verify(root_hash, tree, tree_len, NULL, 0);
    free(tree);

    return (result == BLAKE4_STREAM_OK) ? TEST_PASS : TEST_FAIL;
}

static int test_verify_single_chunk(void) {
    uint8_t data[512];
    memset(data, 0xAB, sizeof(data));

    uint8_t *tree = NULL;
    size_t tree_len = 0;
    uint8_t root_hash[BLAKE4_STREAM_HASH_LEN];

    blake4_stream_encode(data, sizeof(data), &tree, &tree_len, root_hash);

    int result = blake4_stream_verify(root_hash, tree, tree_len, data, sizeof(data));
    free(tree);

    return (result == BLAKE4_STREAM_OK) ? TEST_PASS : TEST_FAIL;
}

static int test_verify_multi_chunk(void) {
    /* 5000 bytes = 5 chunks */
    uint8_t data[5000];
    for (size_t i = 0; i < sizeof(data); i++) {
        data[i] = (uint8_t)(i ^ (i >> 8));
    }

    uint8_t *tree = NULL;
    size_t tree_len = 0;
    uint8_t root_hash[BLAKE4_STREAM_HASH_LEN];

    blake4_stream_encode(data, sizeof(data), &tree, &tree_len, root_hash);

    int result = blake4_stream_verify(root_hash, tree, tree_len, data, sizeof(data));
    free(tree);

    return (result == BLAKE4_STREAM_OK) ? TEST_PASS : TEST_FAIL;
}

static int test_verify_tampered_data(void) {
    uint8_t data[2048];
    memset(data, 0x42, sizeof(data));

    uint8_t *tree = NULL;
    size_t tree_len = 0;
    uint8_t root_hash[BLAKE4_STREAM_HASH_LEN];

    blake4_stream_encode(data, sizeof(data), &tree, &tree_len, root_hash);

    /* Tamper with data */
    data[1000] ^= 0xFF;

    int result = blake4_stream_verify(root_hash, tree, tree_len, data, sizeof(data));
    free(tree);

    /* Should fail verification */
    return (result == BLAKE4_STREAM_ERR_VERIFY) ? TEST_PASS : TEST_FAIL;
}

static int test_verify_wrong_root(void) {
    uint8_t data[1024];
    memset(data, 0x42, sizeof(data));

    uint8_t *tree = NULL;
    size_t tree_len = 0;
    uint8_t root_hash[BLAKE4_STREAM_HASH_LEN];

    blake4_stream_encode(data, sizeof(data), &tree, &tree_len, root_hash);

    /* Use wrong root hash */
    uint8_t wrong_root[BLAKE4_STREAM_HASH_LEN];
    memset(wrong_root, 0xFF, sizeof(wrong_root));

    int result = blake4_stream_verify(wrong_root, tree, tree_len, data, sizeof(data));
    free(tree);

    /* Should fail verification */
    return (result == BLAKE4_STREAM_ERR_VERIFY) ? TEST_PASS : TEST_FAIL;
}

/* ============== Hash Function Tests ============== */

static int test_hash_deterministic(void) {
    uint8_t data[3000];
    for (size_t i = 0; i < sizeof(data); i++) {
        data[i] = (uint8_t)i;
    }

    uint8_t hash1[BLAKE4_STREAM_HASH_LEN], hash2[BLAKE4_STREAM_HASH_LEN];
    blake4_stream_hash(data, sizeof(data), hash1);
    blake4_stream_hash(data, sizeof(data), hash2);

    return (memcmp(hash1, hash2, BLAKE4_STREAM_HASH_LEN) == 0) ? TEST_PASS : TEST_FAIL;
}

static int test_hash_different_data(void) {
    uint8_t data1[1024], data2[1024];
    memset(data1, 0x00, sizeof(data1));
    memset(data2, 0xFF, sizeof(data2));

    uint8_t hash1[BLAKE4_STREAM_HASH_LEN], hash2[BLAKE4_STREAM_HASH_LEN];
    blake4_stream_hash(data1, sizeof(data1), hash1);
    blake4_stream_hash(data2, sizeof(data2), hash2);

    return (memcmp(hash1, hash2, BLAKE4_STREAM_HASH_LEN) != 0) ? TEST_PASS : TEST_FAIL;
}

/* ============== Decoder Tests ============== */

static int test_decoder_creation(void) {
    uint8_t data[2048];
    memset(data, 0x42, sizeof(data));

    uint8_t *tree = NULL;
    size_t tree_len = 0;
    uint8_t root_hash[BLAKE4_STREAM_HASH_LEN];

    blake4_stream_encode(data, sizeof(data), &tree, &tree_len, root_hash);

    blake4_stream_decoder *dec = blake4_stream_decoder_new(root_hash, tree, tree_len);
    free(tree);

    if (!dec) return TEST_FAIL;

    blake4_stream_decoder_free(dec);
    return TEST_PASS;
}

static int test_decoder_streaming(void) {
    uint8_t data[4096];
    for (size_t i = 0; i < sizeof(data); i++) {
        data[i] = (uint8_t)(i * 3);
    }

    uint8_t *tree = NULL;
    size_t tree_len = 0;
    uint8_t root_hash[BLAKE4_STREAM_HASH_LEN];

    blake4_stream_encode(data, sizeof(data), &tree, &tree_len, root_hash);

    blake4_stream_decoder *dec = blake4_stream_decoder_new(root_hash, tree, tree_len);
    free(tree);

    if (!dec) return TEST_FAIL;

    /* Stream data through decoder */
    uint8_t output[4096];
    size_t total_verified = 0;
    size_t offset = 0;

    while (offset < sizeof(data)) {
        size_t chunk = 100;  /* Read in 100-byte chunks */
        if (offset + chunk > sizeof(data)) {
            chunk = sizeof(data) - offset;
        }

        ssize_t verified = blake4_stream_decoder_update(
            dec, data + offset, chunk,
            output + total_verified, sizeof(output) - total_verified);

        if (verified < 0) {
            blake4_stream_decoder_free(dec);
            return TEST_FAIL;
        }

        total_verified += (size_t)verified;
        offset += chunk;
    }

    int result = blake4_stream_decoder_finalize(dec);
    blake4_stream_decoder_free(dec);

    if (result != BLAKE4_STREAM_OK) return TEST_FAIL;

    /* Verify output matches input */
    if (memcmp(data, output, total_verified) != 0) return TEST_FAIL;

    return TEST_PASS;
}

/* ============== Slice Tests ============== */

static int test_proof_size(void) {
    /* For 8KB file (4 chunks), proof should be header + chunk_index + depth_byte + depth*64 */
    /* 4 chunks = depth 2, so proof = 96 + 8 + 1 + 2*64 = 233 bytes */
    size_t size = blake4_stream_proof_size(8192, 0, 2048);
    if (size != 233) {
        printf("\n    Expected proof size 233, got %zu\n", size);
        return TEST_FAIL;
    }

    /* Invalid range should return 0 */
    if (blake4_stream_proof_size(8192, 4000, 2000) != 0) return TEST_FAIL;
    if (blake4_stream_proof_size(8192, 0, 10000) != 0) return TEST_FAIL;

    return TEST_PASS;
}

static int test_slice_extract(void) {
    uint8_t data[8192];
    for (size_t i = 0; i < sizeof(data); i++) {
        data[i] = (uint8_t)i;
    }

    uint8_t *tree = NULL;
    size_t tree_len = 0;
    uint8_t root_hash[BLAKE4_STREAM_HASH_LEN];

    blake4_stream_encode(data, sizeof(data), &tree, &tree_len, root_hash);

    /* Extract first chunk */
    uint8_t proof[512];
    size_t proof_len = sizeof(proof);

    int result = blake4_stream_extract_slice(
        tree, tree_len, data, sizeof(data),
        0, 2048, proof, &proof_len);

    free(tree);

    if (result != BLAKE4_STREAM_OK) {
        printf("\n    Extract failed with error %d\n", result);
        return TEST_FAIL;
    }

    /* Verify proof format */
    if (memcmp(proof, "BK4P", 4) != 0) {
        printf("\n    Invalid proof magic\n");
        return TEST_FAIL;
    }

    return TEST_PASS;
}

static int test_slice_verify(void) {
    uint8_t data[8192];
    for (size_t i = 0; i < sizeof(data); i++) {
        data[i] = (uint8_t)i;
    }

    uint8_t *tree = NULL;
    size_t tree_len = 0;
    uint8_t root_hash[BLAKE4_STREAM_HASH_LEN];

    blake4_stream_encode(data, sizeof(data), &tree, &tree_len, root_hash);

    /* Extract first chunk */
    uint8_t proof[512];
    size_t proof_len = sizeof(proof);

    blake4_stream_extract_slice(
        tree, tree_len, data, sizeof(data),
        0, 2048, proof, &proof_len);

    free(tree);

    /* Verify the slice */
    int result = blake4_stream_verify_slice(
        root_hash, 0, 2048,
        data, 2048,
        proof, proof_len);

    return (result == BLAKE4_STREAM_OK) ? TEST_PASS : TEST_FAIL;
}

/* ============== Main ============== */

int main(void) {
    printf("BLAKE4-STREAM Test Suite\n");
    printf("========================\n\n");

    printf("Utility Functions:\n");
    RUN_TEST(test_num_chunks);
    RUN_TEST(test_tree_depth);
    RUN_TEST(test_tree_data_size);

    printf("\nEncoder Tests:\n");
    RUN_TEST(test_encoder_empty);
    RUN_TEST(test_encoder_single_chunk);
    RUN_TEST(test_encoder_two_chunks);
    RUN_TEST(test_encoder_incremental);

    printf("\nOne-shot Encode Tests:\n");
    RUN_TEST(test_encode_empty);
    RUN_TEST(test_encode_large);

    printf("\nVerification Tests:\n");
    RUN_TEST(test_verify_empty);
    RUN_TEST(test_verify_single_chunk);
    RUN_TEST(test_verify_multi_chunk);
    RUN_TEST(test_verify_tampered_data);
    RUN_TEST(test_verify_wrong_root);

    printf("\nHash Function Tests:\n");
    RUN_TEST(test_hash_deterministic);
    RUN_TEST(test_hash_different_data);

    printf("\nDecoder Tests:\n");
    RUN_TEST(test_decoder_creation);
    RUN_TEST(test_decoder_streaming);

    printf("\nSlice Tests:\n");
    RUN_TEST(test_proof_size);
    RUN_TEST(test_slice_extract);
    RUN_TEST(test_slice_verify);

    printf("\n========================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? 0 : 1;
}
