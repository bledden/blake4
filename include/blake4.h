/*
 * BLAKE4 Cryptographic Hash Function
 * Public API Header
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

/* Version information */
#define BLAKE4_VERSION_MAJOR 1
#define BLAKE4_VERSION_MINOR 0
#define BLAKE4_VERSION_PATCH 0
#define BLAKE4_VERSION_STRING "1.0.0"

/* Constants */
#define BLAKE4_OUT_LEN 32
#define BLAKE4_KEY_LEN 32
#define BLAKE4_BLOCK_LEN 64
#define BLAKE4_CHUNK_LEN 1024
#define BLAKE4_MAX_DEPTH 54

/* Maximum serialized state size */
#define BLAKE4_MAX_STATE_SIZE 2048

/* Hasher state - opaque structure */
typedef struct {
    uint32_t cv[8];                     /* Current chaining value */
    uint32_t key_words[8];              /* Key words (for keyed modes) */
    uint64_t chunk_counter;             /* Number of complete chunks */
    uint8_t buf[BLAKE4_BLOCK_LEN];      /* Block buffer */
    uint8_t buf_len;                    /* Bytes in block buffer */
    uint8_t chunk_buf[BLAKE4_CHUNK_LEN];/* Chunk buffer */
    uint16_t chunk_buf_len;             /* Bytes in chunk buffer */
    uint8_t cv_stack[BLAKE4_MAX_DEPTH][32]; /* CV stack for tree */
    uint8_t cv_stack_len;               /* Number of CVs on stack */
    uint8_t flags;                      /* Mode flags */
} blake4_hasher;

/* ============== Version Functions ============== */

/**
 * Get version string (e.g., "1.0.0")
 */
const char* blake4_version_string(void);

/**
 * Get version components
 */
int blake4_version_major(void);
int blake4_version_minor(void);
int blake4_version_patch(void);

/* ============== Hasher Initialization ============== */

/**
 * Initialize a hasher for default hashing mode.
 */
void blake4_hasher_init(blake4_hasher *self);

/**
 * Initialize a hasher for keyed hashing (MAC) mode.
 * The key must be exactly BLAKE4_KEY_LEN (32) bytes.
 */
void blake4_hasher_init_keyed(blake4_hasher *self,
                              const uint8_t key[BLAKE4_KEY_LEN]);

/**
 * Initialize a hasher for key derivation mode.
 * The context should be a hardcoded, globally unique, application-specific
 * null-terminated string.
 */
void blake4_hasher_init_derive_key(blake4_hasher *self, const char *context);

/**
 * Initialize a hasher for key derivation mode with raw context bytes.
 * Intended for language bindings where C string conversion is inconvenient.
 */
void blake4_hasher_init_derive_key_raw(blake4_hasher *self,
                                       const void *context,
                                       size_t context_len);

/* ============== Hasher Operations ============== */

/**
 * Add input bytes to the hasher. This can be called any number of times.
 */
void blake4_hasher_update(blake4_hasher *self,
                          const void *input,
                          size_t input_len);

/**
 * Finalize the hasher and write output bytes.
 * This does not modify the hasher state - you can call finalize multiple
 * times or continue adding input and finalize again.
 *
 * out_len can be any length. The default is BLAKE4_OUT_LEN (32 bytes).
 */
void blake4_hasher_finalize(const blake4_hasher *self,
                            uint8_t *out,
                            size_t out_len);

/**
 * Finalize with a seek offset for XOF (extendable output function) mode.
 * This allows efficiently streaming a large output without allocating memory.
 */
void blake4_hasher_finalize_seek(const blake4_hasher *self,
                                 uint64_t seek,
                                 uint8_t *out,
                                 size_t out_len);

/**
 * Reset the hasher to its initial state (as after init).
 */
void blake4_hasher_reset(blake4_hasher *self);

/* ============== State Serialization ============== */

/**
 * Get the size needed to serialize the current hasher state.
 * Returns 0 on error.
 */
size_t blake4_hasher_state_size(const blake4_hasher *self);

/**
 * Serialize the hasher state to a buffer.
 * Returns the number of bytes written, or 0 on error.
 *
 * WARNING: For keyed modes, the serialized state contains key material.
 * Protect it accordingly.
 */
size_t blake4_hasher_serialize(const blake4_hasher *self,
                               uint8_t *buf,
                               size_t buf_len);

/**
 * Deserialize a hasher state from a buffer.
 * Returns 0 on success, -1 on error (invalid format, checksum mismatch, etc.)
 */
int blake4_hasher_deserialize(blake4_hasher *self,
                              const uint8_t *buf,
                              size_t buf_len);

/* ============== Convenience Functions ============== */

/**
 * Hash input bytes in one call (default 32-byte output).
 */
void blake4_hash(const void *input,
                 size_t input_len,
                 uint8_t out[BLAKE4_OUT_LEN]);

/**
 * Hash input bytes with a custom output length.
 */
void blake4_hash_xof(const void *input,
                     size_t input_len,
                     uint8_t *out,
                     size_t out_len);

/**
 * Keyed hash (MAC) in one call.
 */
void blake4_hash_keyed(const uint8_t key[BLAKE4_KEY_LEN],
                       const void *input,
                       size_t input_len,
                       uint8_t out[BLAKE4_OUT_LEN]);

/**
 * Derive a key in one call.
 */
void blake4_derive_key(const char *context,
                       const void *key_material,
                       size_t key_material_len,
                       uint8_t *out,
                       size_t out_len);

#ifdef __cplusplus
}
#endif

#endif /* BLAKE4_H */
