/*
 * BLAKE4 Random Data Generator
 *
 * Generates random data using BLAKE4 in counter mode for
 * statistical testing with dieharder/ent/practrand.
 *
 * Usage:
 *   ./gen_random | dieharder -a -g 200
 *   ./gen_random | ent
 *   ./gen_random 1000000 | practrand stdin
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "blake4.h"

int main(int argc, char **argv) {
    uint64_t bytes_to_generate = 0;  /* 0 = infinite */

    if (argc > 1) {
        bytes_to_generate = strtoull(argv[1], NULL, 10);
    }

    /* Use a fixed seed for reproducibility */
    uint8_t seed[BLAKE4_KEY_LEN];
    memset(seed, 0x42, BLAKE4_KEY_LEN);

    uint64_t counter = 0;
    uint8_t output[BLAKE4_OUT_LEN];
    uint64_t total_bytes = 0;

    /* Generate random data using BLAKE4 in counter mode */
    while (bytes_to_generate == 0 || total_bytes < bytes_to_generate) {
        /* Hash: seed || counter */
        blake4_hasher h;
        blake4_hasher_init_keyed(&h, seed);
        blake4_hasher_update(&h, &counter, sizeof(counter));
        blake4_hasher_finalize(&h, output, BLAKE4_OUT_LEN);

        size_t to_write = BLAKE4_OUT_LEN;
        if (bytes_to_generate > 0 && total_bytes + to_write > bytes_to_generate) {
            to_write = bytes_to_generate - total_bytes;
        }

        if (fwrite(output, 1, to_write, stdout) != to_write) {
            break;  /* Pipe closed or error */
        }

        total_bytes += to_write;
        counter++;
    }

    return 0;
}
