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

/* ============== Parallel Hashing API ============== */
/*
 * Multi-threaded parallel hashing for large inputs.
 * Uses chunk-based work distribution with independent CVs per worker.
 * Parallelization threshold: inputs > 1MB will benefit from multi-threading.
 */

/**
 * Parallel hasher state (opaque).
 * Created with blake4_parallel_hasher_new, destroyed with blake4_parallel_hasher_free.
 */
typedef struct blake4_parallel_hasher blake4_parallel_hasher;

/**
 * Create a new parallel hasher.
 *
 * @param num_threads Number of worker threads (0 = auto-detect CPU count)
 * @return New hasher, or NULL on allocation failure
 */
blake4_parallel_hasher* blake4_parallel_hasher_new(unsigned num_threads);

/**
 * Free a parallel hasher and its resources.
 */
void blake4_parallel_hasher_free(blake4_parallel_hasher *self);

/**
 * Initialize parallel hasher for standard hashing.
 */
void blake4_parallel_hasher_init(blake4_parallel_hasher *self);

/**
 * Initialize parallel hasher for keyed hashing (MAC).
 */
void blake4_parallel_hasher_init_keyed(blake4_parallel_hasher *self,
                                        const uint8_t key[BLAKE4_KEY_LEN]);

/**
 * Add input data to the parallel hasher.
 * For small inputs (< 1MB), uses single-threaded path.
 * For large inputs, distributes chunks across worker threads.
 */
void blake4_parallel_hasher_update(blake4_parallel_hasher *self,
                                    const void *input, size_t input_len);

/**
 * Finalize and produce output hash.
 * Coordinates worker threads and performs tree merge.
 */
void blake4_parallel_hasher_finalize(blake4_parallel_hasher *self,
                                      uint8_t *out, size_t out_len);

/**
 * Reset parallel hasher to initial state.
 */
void blake4_parallel_hasher_reset(blake4_parallel_hasher *self);

/**
 * Hash data in one call using multiple threads.
 * For inputs < 1MB, automatically falls back to single-threaded.
 *
 * @param input Input data
 * @param input_len Length of input
 * @param out Output buffer (BLAKE4_OUT_LEN bytes)
 * @param num_threads Number of threads (0 = auto-detect)
 */
void blake4_parallel_hash(const void *input, size_t input_len,
                          uint8_t out[BLAKE4_OUT_LEN],
                          unsigned num_threads);

/**
 * Parallel hash with arbitrary output length (XOF mode).
 */
void blake4_parallel_hash_xof(const void *input, size_t input_len,
                               uint8_t *out, size_t out_len,
                               unsigned num_threads);

/**
 * Get the number of threads used by a parallel hasher.
 */
unsigned blake4_parallel_hasher_num_threads(const blake4_parallel_hasher *self);

/**
 * Query if parallel hashing is available on this platform.
 * Returns 1 if threading support is compiled in, 0 otherwise.
 */
int blake4_parallel_available(void);

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

/* ============== Constant-Time Utilities ============== */
/*
 * These functions provide constant-time operations critical for
 * cryptographic security. They prevent timing side-channel attacks
 * by ensuring execution time is independent of secret data values.
 */

/**
 * Constant-time memory comparison.
 * Use this instead of memcmp() when comparing MACs, keys, or secrets.
 *
 * Returns 0 if equal, non-zero if different.
 * Execution time is independent of where arrays differ.
 */
int blake4_ct_memcmp(const void *a, const void *b, size_t n);

/**
 * Secure memory zeroing.
 * Use this to clear keys, intermediate state, and other secrets.
 * Guaranteed not to be optimized away by the compiler.
 */
void blake4_ct_memzero(void *ptr, size_t n);

/**
 * Constant-time 64-bit conditional selection.
 * Returns a if condition is 0, b if condition is non-zero.
 */
uint64_t blake4_ct_select64(uint64_t condition, uint64_t a, uint64_t b);

/**
 * Constant-time 8-bit conditional selection.
 */
uint8_t blake4_ct_select8(uint8_t condition, uint8_t a, uint8_t b);

/**
 * Constant-time conditional memory copy.
 * If condition is non-zero, copies src to dst.
 * If condition is zero, dst is unchanged (but still accessed).
 */
void blake4_ct_memcpy_if(void *dst, const void *src, size_t n, int condition);

/**
 * Constant-time 64-bit equality check.
 * Returns 1 if a == b, 0 otherwise.
 */
int blake4_ct_eq64(uint64_t a, uint64_t b);

/**
 * Constant-time 64-bit less-than comparison.
 * Returns 1 if a < b, 0 otherwise.
 */
int blake4_ct_lt64(uint64_t a, uint64_t b);

/* ============== Version Info ============== */

const char* blake4_version_string(void);
int blake4_version_major(void);
int blake4_version_minor(void);
int blake4_version_patch(void);

#ifdef __cplusplus
}
#endif

#endif /* BLAKE4_H */
