/*
 * BLAKE4 Avalanche Test
 *
 * Tests the avalanche effect: flipping a single input bit should
 * change approximately 50% of output bits.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include "blake4.h"

#define NUM_TESTS 10000
#define INPUT_SIZE 64

/* Count the number of differing bits between two byte arrays */
static int count_bit_diff(const uint8_t *a, const uint8_t *b, size_t len) {
    int diff = 0;
    for (size_t i = 0; i < len; i++) {
        uint8_t xor = a[i] ^ b[i];
        while (xor) {
            diff += xor & 1;
            xor >>= 1;
        }
    }
    return diff;
}

/* Flip a specific bit in a byte array */
static void flip_bit(uint8_t *data, size_t bit_index) {
    size_t byte_index = bit_index / 8;
    size_t bit_offset = bit_index % 8;
    data[byte_index] ^= (1 << bit_offset);
}

/* Test avalanche effect for a given input size and output size */
static int test_avalanche(size_t input_len, size_t output_len, int verbose) {
    uint8_t input[INPUT_SIZE];
    uint8_t input_flipped[INPUT_SIZE];
    uint8_t hash_original[BLAKE4_OUT_LEN];
    uint8_t hash_flipped[BLAKE4_OUT_LEN];

    int total_bits = output_len * 8;
    double sum_diff = 0;
    int min_diff = total_bits;
    int max_diff = 0;

    /* Run multiple tests with random inputs */
    for (int t = 0; t < NUM_TESTS; t++) {
        /* Generate pseudo-random input based on test number */
        for (size_t i = 0; i < input_len; i++) {
            input[i] = (uint8_t)((t * 31 + i * 17) & 0xFF);
        }

        /* Hash original input */
        blake4_hasher h;
        blake4_hasher_init(&h);
        blake4_hasher_update(&h, input, input_len);
        blake4_hasher_finalize(&h, hash_original, output_len);

        /* Test flipping each bit of the first byte */
        for (int bit = 0; bit < 8; bit++) {
            memcpy(input_flipped, input, input_len);
            flip_bit(input_flipped, bit);

            blake4_hasher_init(&h);
            blake4_hasher_update(&h, input_flipped, input_len);
            blake4_hasher_finalize(&h, hash_flipped, output_len);

            int diff = count_bit_diff(hash_original, hash_flipped, output_len);
            sum_diff += diff;
            if (diff < min_diff) min_diff = diff;
            if (diff > max_diff) max_diff = diff;
        }
    }

    double avg_diff = sum_diff / (NUM_TESTS * 8);
    double expected = total_bits / 2.0;
    double pct_actual = (avg_diff / total_bits) * 100;
    double pct_expected = 50.0;
    double deviation = fabs(pct_actual - pct_expected);

    printf("  Input: %zu bytes, Output: %zu bytes (%d bits)\n",
           input_len, output_len, total_bits);
    printf("    Average bits changed: %.2f / %d (%.2f%%)\n",
           avg_diff, total_bits, pct_actual);
    printf("    Expected: %.2f / %d (50.00%%)\n", expected, total_bits);
    printf("    Min/Max bits changed: %d / %d\n", min_diff, max_diff);
    printf("    Deviation from ideal: %.2f%%\n", deviation);

    /* Pass if within 5% of ideal 50% */
    int pass = deviation < 5.0;
    printf("    Result: %s\n\n", pass ? "PASS" : "FAIL");

    return pass;
}

/* Test strict avalanche criterion (SAC) */
static int test_sac(size_t input_len, size_t output_len) {
    uint8_t input[INPUT_SIZE];
    uint8_t input_flipped[INPUT_SIZE];
    uint8_t hash_original[BLAKE4_OUT_LEN];
    uint8_t hash_flipped[BLAKE4_OUT_LEN];

    int output_bits = output_len * 8;
    int input_bits = input_len * 8;

    /* Count how many times each output bit flips */
    int *flip_counts = calloc(output_bits, sizeof(int));
    if (!flip_counts) return 0;

    int total_tests = 0;

    for (int t = 0; t < 1000; t++) {
        /* Generate input */
        for (size_t i = 0; i < input_len; i++) {
            input[i] = (uint8_t)((t * 37 + i * 13) & 0xFF);
        }

        blake4_hasher h;
        blake4_hasher_init(&h);
        blake4_hasher_update(&h, input, input_len);
        blake4_hasher_finalize(&h, hash_original, output_len);

        /* Flip each input bit and check output changes */
        for (int in_bit = 0; in_bit < input_bits && in_bit < 64; in_bit++) {
            memcpy(input_flipped, input, input_len);
            flip_bit(input_flipped, in_bit);

            blake4_hasher_init(&h);
            blake4_hasher_update(&h, input_flipped, input_len);
            blake4_hasher_finalize(&h, hash_flipped, output_len);

            /* Count which output bits flipped */
            for (int out_byte = 0; out_byte < (int)output_len; out_byte++) {
                uint8_t diff = hash_original[out_byte] ^ hash_flipped[out_byte];
                for (int bit = 0; bit < 8; bit++) {
                    if (diff & (1 << bit)) {
                        flip_counts[out_byte * 8 + bit]++;
                    }
                }
            }
            total_tests++;
        }
    }

    /* Check that each output bit flips approximately 50% of the time */
    double expected = total_tests / 2.0;
    double max_deviation = 0;
    int worst_bit = 0;

    for (int i = 0; i < output_bits; i++) {
        double pct = (double)flip_counts[i] / total_tests * 100;
        double dev = fabs(pct - 50.0);
        if (dev > max_deviation) {
            max_deviation = dev;
            worst_bit = i;
        }
    }

    free(flip_counts);

    printf("  SAC Test (input: %zu bytes, output: %zu bytes)\n", input_len, output_len);
    printf("    Tests per output bit: %d\n", total_tests);
    printf("    Worst bit deviation: bit %d at %.2f%% from ideal\n",
           worst_bit, max_deviation);

    int pass = max_deviation < 10.0;
    printf("    Result: %s\n\n", pass ? "PASS" : "FAIL");

    return pass;
}

int main(void) {
    printf("=== BLAKE4 Avalanche Test Suite ===\n\n");

    int all_pass = 1;

    printf("1. Basic Avalanche Tests\n");
    printf("   (Single bit flip should change ~50%% of output bits)\n\n");

    all_pass &= test_avalanche(8, 64, 0);    /* Small input, full output */
    all_pass &= test_avalanche(64, 64, 0);   /* Medium input, full output */
    all_pass &= test_avalanche(64, 32, 0);   /* Medium input, 256-bit output */
    all_pass &= test_avalanche(64, 48, 0);   /* Medium input, 384-bit output */

    printf("2. Strict Avalanche Criterion (SAC) Tests\n");
    printf("   (Each output bit should flip ~50%% when any input bit flips)\n\n");

    all_pass &= test_sac(8, 64);
    all_pass &= test_sac(32, 64);

    printf("=== Summary ===\n");
    printf("Overall: %s\n", all_pass ? "ALL TESTS PASSED" : "SOME TESTS FAILED");

    return all_pass ? 0 : 1;
}
