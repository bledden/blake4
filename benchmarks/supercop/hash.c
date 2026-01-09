/*
 * SUPERCOP hash.c wrapper for BLAKE4
 *
 * This file provides the crypto_hash() interface required by SUPERCOP.
 *
 * SUPERCOP expects:
 *   int crypto_hash(unsigned char *out, const unsigned char *in,
 *                   unsigned long long inlen);
 *
 * Returns 0 on success.
 */

#include "blake4.h"

int crypto_hash(unsigned char *out, const unsigned char *in,
                unsigned long long inlen) {
    blake4_hash(in, (size_t)inlen, out);
    return 0;
}
