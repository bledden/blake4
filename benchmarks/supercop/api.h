/*
 * SUPERCOP API header for BLAKE4
 *
 * SUPERCOP (System for Unified Performance Evaluation Related to
 * Cryptographic Operations and Primitives) compatibility layer.
 *
 * To use with SUPERCOP:
 * 1. Create directory: crypto_hash/blake4/ref/
 * 2. Copy this file and hash.c to that directory
 * 3. Copy blake4.c and blake4.h from the main source
 * 4. Run: ./do-part crypto_hash blake4
 */

#ifndef API_H
#define API_H

/* BLAKE4 produces 64-byte (512-bit) output by default */
#define CRYPTO_BYTES 64

/* Algorithm identifier */
#define CRYPTO_ALGNAME "blake4"

#endif /* API_H */
