/*
 * BLAKE4 Cryptographic Hash Function
 * 512-bit internal state implementation
 *
 * This is free and unencumbered software released into the public domain.
 */

#ifndef BLAKE4_H
#define BLAKE4_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============== Constants ============== */

#define BLAKE4_VERSION_MAJOR 2
#define BLAKE4_VERSION_MINOR 0
#define BLAKE4_VERSION_PATCH 0
#define BLAKE4_VERSION_STRING "2.0.0"

#define BLAKE4_OUT_LEN      64    /* Default output: 64 bytes (512 bits) */
#define BLAKE4_KEY_LEN      64    /* Key size: 64 bytes */
#define BLAKE4_BLOCK_LEN    128   /* Block size: 128 bytes */
#define BLAKE4_CHUNK_LEN    2048  /* Chunk size: 2048 bytes (16 blocks) */
#define BLAKE4_MAX_DEPTH    54    /* Max tree depth for 2^54 chunks */

/* ============== Hasher State ============== */

typedef struct {
    uint64_t cv[8];                          /* Current chaining value */
    uint64_t key_words[8];                   /* Key (or IV for unkeyed) */
    uint64_t chunk_counter;                  /* Chunks processed */
    uint8_t buf[BLAKE4_BLOCK_LEN];           /* Block buffer */
    uint8_t buf_len;                         /* Bytes in block buffer */
    uint8_t chunk_buf[BLAKE4_CHUNK_LEN];     /* Chunk buffer */
    uint16_t chunk_buf_len;                  /* Bytes in chunk buffer */
    uint8_t cv_stack[BLAKE4_MAX_DEPTH][64];  /* CV stack (64 bytes each) */
    uint8_t cv_stack_len;                    /* CVs on stack */
    uint8_t flags;                           /* Mode flags */
} blake4_hasher;

/* ============== Core API ============== */

/**
 * Initialize a hasher for standard hashing.
 */
void blake4_hasher_init(blake4_hasher *self);

/**
 * Initialize a hasher for keyed hashing (MAC).
 * Key must be exactly BLAKE4_KEY_LEN (64) bytes.
 */
void blake4_hasher_init_keyed(blake4_hasher *self,
                              const uint8_t key[BLAKE4_KEY_LEN]);

/**
 * Initialize a hasher for key derivation.
 * Context is an application-specific string.
 */
void blake4_hasher_init_derive_key(blake4_hasher *self, const char *context);

/**
 * Initialize for key derivation with explicit length.
 */
void blake4_hasher_init_derive_key_raw(blake4_hasher *self,
                                       const void *context,
                                       size_t context_len);

/**
 * Add input data to the hasher.
 */
void blake4_hasher_update(blake4_hasher *self, const void *input,
                          size_t input_len);

/**
 * Finalize and produce output hash.
 */
void blake4_hasher_finalize(const blake4_hasher *self, uint8_t *out,
                            size_t out_len);

/**
 * Finalize with seek (for XOF mode).
 */
void blake4_hasher_finalize_seek(const blake4_hasher *self, uint64_t seek,
                                 uint8_t *out, size_t out_len);

/**
 * Reset hasher to initial state (preserving key if keyed).
 */
void blake4_hasher_reset(blake4_hasher *self);

/* ============== State Serialization ============== */

/**
 * Size needed for serialized hasher state.
 * This is a fixed size regardless of hasher contents.
 */
#define BLAKE4_SERIALIZED_SIZE (8 + 64 + 64 + 8 + 128 + 1 + 2048 + 2 + (54 * 64) + 1 + 1)

/**
 * Serialize hasher state to a byte buffer.
 * Allows saving and restoring hasher mid-stream.
 *
 * buf: Output buffer (must be at least BLAKE4_SERIALIZED_SIZE bytes)
 * buf_len: Size of buffer
 *
 * Returns number of bytes written, or 0 on error.
 */
size_t blake4_hasher_serialize(const blake4_hasher *self, uint8_t *buf,
                               size_t buf_len);

/**
 * Deserialize hasher state from a byte buffer.
 *
 * buf: Input buffer containing serialized state
 * buf_len: Size of buffer
 *
 * Returns 1 on success, 0 on error (invalid data or buffer too small).
 */
int blake4_hasher_deserialize(blake4_hasher *self, const uint8_t *buf,
                              size_t buf_len);

/* ============== Convenience Functions ============== */

/**
 * Hash data in one call (64-byte output).
 */
void blake4_hash(const void *input, size_t input_len,
                 uint8_t out[BLAKE4_OUT_LEN]);

/**
 * Hash with arbitrary output length (XOF mode).
 */
void blake4_hash_xof(const void *input, size_t input_len, uint8_t *out,
                     size_t out_len);

/**
 * Keyed hash (MAC) in one call.
 */
void blake4_hash_keyed(const uint8_t key[BLAKE4_KEY_LEN], const void *input,
                       size_t input_len, uint8_t out[BLAKE4_OUT_LEN]);

/**
 * Key derivation in one call.
 */
void blake4_derive_key(const char *context, const void *key_material,
                       size_t key_material_len, uint8_t *out, size_t out_len);

/* ============== Hash-Based Signature Support ============== */
/*
 * These functions are optimized for post-quantum hash-based signature schemes
 * such as SPHINCS+, XMSS, and LMS. They provide the specific primitives needed:
 * - F: Random function for leaf computation
 * - H: Tree hashing function
 * - PRF: Pseudorandom function for key generation
 * - PRFmsg: PRF for message randomization
 */

/**
 * PRF function for hash-based signatures.
 * Computes PRF(key, addr) where addr is a 32-byte address.
 * Used in WOTS+, XMSS, SPHINCS+ for pseudorandom key generation.
 *
 * @param key 64-byte secret key (SK.PRF in SPHINCS+)
 * @param addr 32-byte address (ADRS structure)
 * @param out Output buffer (n bytes, where n is security parameter)
 * @param out_len Output length (typically 32 or 64 bytes)
 */
void blake4_hbs_prf(const uint8_t key[BLAKE4_KEY_LEN],
                    const uint8_t addr[32],
                    uint8_t *out, size_t out_len);

/**
 * PRF for message randomization (PRFmsg in SPHINCS+).
 * Computes PRFmsg(SK.PRF, OptRand, M) for randomized message hashing.
 *
 * @param key 64-byte secret key (SK.PRF)
 * @param opt_rand 64-byte optional randomness
 * @param message Message to include in PRF
 * @param message_len Length of message
 * @param out Output buffer
 * @param out_len Output length
 */
void blake4_hbs_prf_msg(const uint8_t key[BLAKE4_KEY_LEN],
                        const uint8_t opt_rand[BLAKE4_OUT_LEN],
                        const void *message, size_t message_len,
                        uint8_t *out, size_t out_len);

/**
 * Tree hash function (H in SPHINCS+).
 * Computes H(PK.seed, ADRS, M1 || M2) for Merkle tree internal nodes.
 *
 * @param pub_seed 64-byte public seed (PK.seed)
 * @param addr 32-byte address
 * @param left 64-byte left child hash
 * @param right 64-byte right child hash
 * @param out 64-byte output
 */
void blake4_hbs_h(const uint8_t pub_seed[BLAKE4_OUT_LEN],
                  const uint8_t addr[32],
                  const uint8_t left[BLAKE4_OUT_LEN],
                  const uint8_t right[BLAKE4_OUT_LEN],
                  uint8_t out[BLAKE4_OUT_LEN]);

/**
 * Chaining function (F in SPHINCS+, WOTS+).
 * Computes F(PK.seed, ADRS, M) for WOTS+ chain steps.
 *
 * @param pub_seed 64-byte public seed
 * @param addr 32-byte address
 * @param input 64-byte chain input
 * @param out 64-byte output
 */
void blake4_hbs_f(const uint8_t pub_seed[BLAKE4_OUT_LEN],
                  const uint8_t addr[32],
                  const uint8_t input[BLAKE4_OUT_LEN],
                  uint8_t out[BLAKE4_OUT_LEN]);

/**
 * Tweakable hash (T_l in SPHINCS+).
 * Computes T_l(PK.seed, ADRS, M) for l-block messages.
 * Used for WOTS+ public key compression and FORS tree roots.
 *
 * @param pub_seed 64-byte public seed
 * @param addr 32-byte address
 * @param input Input blocks (each BLAKE4_OUT_LEN bytes)
 * @param num_blocks Number of input blocks
 * @param out 64-byte output
 */
void blake4_hbs_t(const uint8_t pub_seed[BLAKE4_OUT_LEN],
                  const uint8_t addr[32],
                  const uint8_t *input, size_t num_blocks,
                  uint8_t out[BLAKE4_OUT_LEN]);

/**
 * Message hash (H_msg in SPHINCS+).
 * Computes H_msg(R, PK.seed, PK.root, M) for message digest.
 *
 * @param r 64-byte randomizer
 * @param pub_seed 64-byte public seed
 * @param pub_root 64-byte public root
 * @param message Message to hash
 * @param message_len Message length
 * @param out Output buffer
 * @param out_len Output length (typically enough for tree indices + FORS indices)
 */
void blake4_hbs_h_msg(const uint8_t r[BLAKE4_OUT_LEN],
                      const uint8_t pub_seed[BLAKE4_OUT_LEN],
                      const uint8_t pub_root[BLAKE4_OUT_LEN],
                      const void *message, size_t message_len,
                      uint8_t *out, size_t out_len);

/* ============== Configurable Output Modes ============== */

/**
 * BLAKE4-256: Truncated 256-bit output mode.
 * Provides 128-bit classical / 85-bit quantum collision resistance.
 */
void blake4_256_hash(const void *input, size_t input_len, uint8_t out[32]);

/**
 * BLAKE4-384: Truncated 384-bit output mode.
 * Provides 192-bit classical / 128-bit quantum collision resistance.
 */
void blake4_384_hash(const void *input, size_t input_len, uint8_t out[48]);

/**
 * BLAKE4-512: Full 512-bit output (same as blake4_hash).
 * Provides 256-bit classical / 170-bit quantum collision resistance.
 */
void blake4_512_hash(const void *input, size_t input_len, uint8_t out[64]);

/* ============== Version Info ============== */

const char* blake4_version_string(void);
int blake4_version_major(void);
int blake4_version_minor(void);
int blake4_version_patch(void);

#ifdef __cplusplus
}
#endif

#endif /* BLAKE4_H */
