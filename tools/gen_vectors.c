/*
 * BLAKE4 Test Vector Generator
 * Generates test vectors for interoperability testing
 */

#include "../include/blake4.h"
#include "../include/blake4_stream.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void print_hex(const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        printf("%02x", data[i]);
    }
}

static void print_vector(const char *name, const void *input, size_t len,
                         const uint8_t *hash) {
    printf("{\n");
    printf("  \"name\": \"%s\",\n", name);
    printf("  \"input_len\": %zu,\n", len);
    printf("  \"input_hex\": \"");
    print_hex((const uint8_t *)input, len);
    printf("\",\n");
    printf("  \"output_hex\": \"");
    print_hex(hash, BLAKE4_OUT_LEN);
    printf("\"\n");
    printf("}");
}

int main(void) {
    uint8_t hash[BLAKE4_OUT_LEN];

    printf("{\n");
    printf("  \"version\": \"2.0.0\",\n");
    printf("  \"generator\": \"BLAKE4 reference implementation\",\n\n");

    /* ========== Portable Hash Vectors ========== */
    printf("  \"portable\": [\n");

    /* Empty input */
    blake4_hash("", 0, hash);
    printf("    ");
    print_vector("empty", "", 0, hash);
    printf(",\n");

    /* Single byte 0x00 */
    uint8_t zero = 0;
    blake4_hash(&zero, 1, hash);
    printf("    ");
    print_vector("single_zero", &zero, 1, hash);
    printf(",\n");

    /* "abc" */
    blake4_hash("abc", 3, hash);
    printf("    ");
    print_vector("abc", "abc", 3, hash);
    printf(",\n");

    /* 64 bytes (one block) */
    uint8_t block64[64];
    for (int i = 0; i < 64; i++) block64[i] = (uint8_t)i;
    blake4_hash(block64, 64, hash);
    printf("    ");
    print_vector("one_block_64", block64, 64, hash);
    printf(",\n");

    /* 1024 bytes (one chunk) */
    uint8_t chunk1024[1024];
    for (int i = 0; i < 1024; i++) chunk1024[i] = (uint8_t)(i % 256);
    blake4_hash(chunk1024, 1024, hash);
    printf("    ");
    print_vector("one_chunk_1024", chunk1024, 1024, hash);
    printf(",\n");

    /* 2048 bytes (two chunks) */
    uint8_t *data2048 = (uint8_t *)malloc(2048);
    for (int i = 0; i < 2048; i++) data2048[i] = (uint8_t)(i % 256);
    blake4_hash(data2048, 2048, hash);
    printf("    ");
    print_vector("two_chunks_2048", data2048, 2048, hash);
    free(data2048);

    printf("\n  ],\n\n");

    /* ========== Keyed Mode Vectors ========== */
    printf("  \"keyed\": [\n");

    /* Zero key, empty input */
    uint8_t zero_key[32] = {0};
    blake4_hash_keyed(zero_key, "", 0, hash);
    printf("    {\n");
    printf("      \"name\": \"zero_key_empty\",\n");
    printf("      \"key_hex\": \"");
    print_hex(zero_key, 32);
    printf("\",\n");
    printf("      \"input_hex\": \"\",\n");
    printf("      \"output_hex\": \"");
    print_hex(hash, BLAKE4_OUT_LEN);
    printf("\"\n");
    printf("    },\n");

    /* Sequential key, "abc" input */
    uint8_t seq_key[32];
    for (int i = 0; i < 32; i++) seq_key[i] = (uint8_t)i;
    blake4_hash_keyed(seq_key, "abc", 3, hash);
    printf("    {\n");
    printf("      \"name\": \"seq_key_abc\",\n");
    printf("      \"key_hex\": \"");
    print_hex(seq_key, 32);
    printf("\",\n");
    printf("      \"input_hex\": \"616263\",\n");
    printf("      \"output_hex\": \"");
    print_hex(hash, BLAKE4_OUT_LEN);
    printf("\"\n");
    printf("    }\n");

    printf("  ],\n\n");

    /* ========== Key Derivation Vectors ========== */
    printf("  \"derive_key\": [\n");

    uint8_t derived[32];
    blake4_derive_key("BLAKE4 test context", "key material", 12, derived, 32);
    printf("    {\n");
    printf("      \"name\": \"test_context\",\n");
    printf("      \"context\": \"BLAKE4 test context\",\n");
    printf("      \"key_material_hex\": \"6b6579206d6174657269616c\",\n");
    printf("      \"output_len\": 32,\n");
    printf("      \"output_hex\": \"");
    print_hex(derived, 32);
    printf("\"\n");
    printf("    }\n");

    printf("  ],\n\n");

    /* ========== XOF Vectors ========== */
    printf("  \"xof\": [\n");

    uint8_t xof64[64];
    blake4_hash_xof("abc", 3, xof64, 64);
    printf("    {\n");
    printf("      \"name\": \"abc_64_bytes\",\n");
    printf("      \"input_hex\": \"616263\",\n");
    printf("      \"output_len\": 64,\n");
    printf("      \"output_hex\": \"");
    print_hex(xof64, 64);
    printf("\"\n");
    printf("    }\n");

    printf("  ],\n\n");

    /* ========== Stream Vectors ========== */
    printf("  \"stream\": [\n");

    /* Empty tree */
    uint8_t *tree;
    size_t tree_len;
    uint8_t root[32];

    blake4_stream_encode(NULL, 0, &tree, &tree_len, root);
    printf("    {\n");
    printf("      \"name\": \"empty\",\n");
    printf("      \"input_len\": 0,\n");
    printf("      \"tree_len\": %zu,\n", tree_len);
    printf("      \"root_hash_hex\": \"");
    print_hex(root, 32);
    printf("\"\n");
    printf("    },\n");
    free(tree);

    /* Single chunk */
    uint8_t single_chunk[512];
    memset(single_chunk, 0x42, sizeof(single_chunk));
    blake4_stream_encode(single_chunk, sizeof(single_chunk), &tree, &tree_len, root);
    printf("    {\n");
    printf("      \"name\": \"single_chunk_512\",\n");
    printf("      \"input_len\": 512,\n");
    printf("      \"tree_len\": %zu,\n", tree_len);
    printf("      \"root_hash_hex\": \"");
    print_hex(root, 32);
    printf("\"\n");
    printf("    },\n");
    free(tree);

    /* Two chunks */
    uint8_t *two_chunks = (uint8_t *)malloc(2048);
    for (int i = 0; i < 2048; i++) two_chunks[i] = (uint8_t)(i % 256);
    blake4_stream_encode(two_chunks, 2048, &tree, &tree_len, root);
    printf("    {\n");
    printf("      \"name\": \"two_chunks_2048\",\n");
    printf("      \"input_len\": 2048,\n");
    printf("      \"tree_len\": %zu,\n", tree_len);
    printf("      \"root_hash_hex\": \"");
    print_hex(root, 32);
    printf("\"\n");
    printf("    }\n");
    free(tree);
    free(two_chunks);

    printf("  ]\n");

    printf("}\n");

    return 0;
}
