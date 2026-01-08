/*
 * BLAKE4-STREAM Verified Streaming
 * Public API Header
 *
 * Provides Merkle tree-based verified streaming and random access
 * verification of large files, based on Bao concepts with a normative
 * specification.
 *
 * This is free and unencumbered software released into the public domain.
 */

#ifndef BLAKE4_STREAM_H
#define BLAKE4_STREAM_H

#include "blake4.h"
#include <stddef.h>
#include <stdint.h>

/* Define ssize_t for platforms that don't have it */
#ifdef _WIN32
typedef ptrdiff_t ssize_t;
#else
#include <sys/types.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ============== Constants ============== */

/* Chunk size for tree hashing (2 KiB, matches BLAKE4 chunk size) */
#define BLAKE4_STREAM_CHUNK_LEN 2048

/* Hash output length (64 bytes / 512 bits) */
#define BLAKE4_STREAM_HASH_LEN 64

/* Magic bytes for file formats */
#define BLAKE4_STREAM_TREE_MAGIC  "BK4T"
#define BLAKE4_STREAM_PROOF_MAGIC "BK4P"

/* Format version (v2 for 512-bit BLAKE4) */
#define BLAKE4_STREAM_VERSION 2

/* Maximum tree depth (supports files up to 2^54 * 2048 bytes) */
#define BLAKE4_STREAM_MAX_DEPTH 54

/* Error codes */
#define BLAKE4_STREAM_OK              0
#define BLAKE4_STREAM_ERR_INVALID    -1  /* Invalid parameters */
#define BLAKE4_STREAM_ERR_FORMAT     -2  /* Invalid format */
#define BLAKE4_STREAM_ERR_VERSION    -3  /* Unsupported version */
#define BLAKE4_STREAM_ERR_VERIFY     -4  /* Verification failed */
#define BLAKE4_STREAM_ERR_RANGE      -5  /* Invalid range */
#define BLAKE4_STREAM_ERR_BUFFER     -6  /* Buffer too small */

/* ============== Types ============== */

/* Opaque encoder state */
typedef struct blake4_stream_encoder blake4_stream_encoder;

/* Opaque decoder state */
typedef struct blake4_stream_decoder blake4_stream_decoder;

/*
 * Tree file header (80 bytes fixed)
 * Format:
 *   [4 bytes: magic "BK4T"]
 *   [1 byte: version]
 *   [1 byte: flags]
 *   [2 bytes: reserved]
 *   [8 bytes: data length]
 *   [64 bytes: root hash]
 *
 * Followed by internal nodes in pre-order traversal.
 */
#define BLAKE4_STREAM_TREE_HEADER_LEN 80

/*
 * Proof header (96 bytes fixed)
 * Format:
 *   [4 bytes: magic "BK4P"]
 *   [1 byte: version]
 *   [1 byte: flags]
 *   [2 bytes: reserved]
 *   [64 bytes: root hash]
 *   [8 bytes: total data length]
 *   [8 bytes: slice start offset]
 *   [8 bytes: slice end offset (exclusive)]
 *
 * Followed by:
 *   [8 bytes: chunk index]
 *   [1 byte: proof depth]
 *   [depth * 64 bytes: sibling hashes from leaf to root]
 */
#define BLAKE4_STREAM_PROOF_HEADER_LEN 96

/* ============== Tree Encoding (Build Merkle Tree) ============== */

/**
 * Create a new encoder.
 * Returns NULL on allocation failure.
 */
blake4_stream_encoder* blake4_stream_encoder_new(void);

/**
 * Free an encoder.
 */
void blake4_stream_encoder_free(blake4_stream_encoder *enc);

/**
 * Feed data to the encoder. Can be called multiple times.
 */
void blake4_stream_encoder_update(blake4_stream_encoder *enc,
                                   const uint8_t *data, size_t len);

/**
 * Finalize encoding and get the tree data and root hash.
 *
 * tree_out: Receives pointer to allocated tree data (caller must free)
 * tree_len: Receives length of tree data
 * root_hash: Receives 64-byte root hash
 *
 * Returns BLAKE4_STREAM_OK on success.
 */
int blake4_stream_encoder_finalize(blake4_stream_encoder *enc,
                                    uint8_t **tree_out, size_t *tree_len,
                                    uint8_t root_hash[BLAKE4_STREAM_HASH_LEN]);

/**
 * Get the root hash without allocating tree storage.
 * Useful when you only need the hash, not the tree.
 */
void blake4_stream_encoder_root_hash(const blake4_stream_encoder *enc,
                                      uint8_t root_hash[BLAKE4_STREAM_HASH_LEN]);

/* ============== Full Decoding (Verify Entire File) ============== */

/**
 * Create a decoder with expected root hash and tree data.
 *
 * expected_root: 64-byte expected root hash
 * tree: Tree data from encoder (can be NULL for hash-only mode)
 * tree_len: Length of tree data
 *
 * Returns NULL on allocation failure or invalid tree format.
 */
blake4_stream_decoder* blake4_stream_decoder_new(
    const uint8_t expected_root[BLAKE4_STREAM_HASH_LEN],
    const uint8_t *tree, size_t tree_len);

/**
 * Free a decoder.
 */
void blake4_stream_decoder_free(blake4_stream_decoder *dec);

/**
 * Feed data to verify. Returns the number of verified bytes output.
 *
 * data: Input data to verify
 * len: Length of input data
 * verified_out: Buffer to receive verified data (can be same as data)
 * out_len: Length of output buffer
 *
 * Returns number of verified bytes written, or negative on error.
 * May return less than input length if waiting for more data at chunk boundary.
 */
ssize_t blake4_stream_decoder_update(blake4_stream_decoder *dec,
                                      const uint8_t *data, size_t len,
                                      uint8_t *verified_out, size_t out_len);

/**
 * Finalize decoding and check verification.
 * Returns BLAKE4_STREAM_OK if all data verified successfully.
 */
int blake4_stream_decoder_finalize(blake4_stream_decoder *dec);

/* ============== Slice Operations ============== */

/**
 * Calculate the proof size needed for a slice.
 *
 * data_len: Total file length
 * start: Slice start offset
 * end: Slice end offset (exclusive)
 *
 * Returns proof size in bytes, or 0 on invalid range.
 */
size_t blake4_stream_proof_size(uint64_t data_len, uint64_t start, uint64_t end);

/**
 * Extract a slice with its proof.
 *
 * tree: Tree data from encoder
 * tree_len: Length of tree data
 * data: Original file data
 * data_len: Length of file data
 * start: Slice start offset
 * end: Slice end offset (exclusive)
 * proof_out: Buffer to receive proof
 * proof_len: In: buffer size, Out: actual proof size
 *
 * Returns BLAKE4_STREAM_OK on success.
 */
int blake4_stream_extract_slice(const uint8_t *tree, size_t tree_len,
                                 const uint8_t *data, size_t data_len,
                                 uint64_t start, uint64_t end,
                                 uint8_t *proof_out, size_t *proof_len);

/**
 * Verify a slice against its proof.
 *
 * expected_root: 64-byte expected root hash
 * start: Slice start offset
 * end: Slice end offset (exclusive)
 * slice_data: The slice data to verify
 * slice_len: Length of slice data
 * proof: The proof data
 * proof_len: Length of proof data
 *
 * Returns BLAKE4_STREAM_OK if verification succeeds.
 */
int blake4_stream_verify_slice(const uint8_t expected_root[BLAKE4_STREAM_HASH_LEN],
                                uint64_t start, uint64_t end,
                                const uint8_t *slice_data, size_t slice_len,
                                const uint8_t *proof, size_t proof_len);

/* ============== Convenience Functions ============== */

/**
 * One-shot encode: hash data and build tree.
 *
 * data: Input data
 * data_len: Length of data
 * tree_out: Receives pointer to allocated tree data (caller must free)
 * tree_len: Receives length of tree data
 * root_hash: Receives 64-byte root hash
 *
 * Returns BLAKE4_STREAM_OK on success.
 */
int blake4_stream_encode(const uint8_t *data, size_t data_len,
                          uint8_t **tree_out, size_t *tree_len,
                          uint8_t root_hash[BLAKE4_STREAM_HASH_LEN]);

/**
 * One-shot verify: verify entire file against tree and root hash.
 *
 * expected_root: 64-byte expected root hash
 * tree: Tree data
 * tree_len: Length of tree data
 * data: File data to verify
 * data_len: Length of file data
 *
 * Returns BLAKE4_STREAM_OK if verification succeeds.
 */
int blake4_stream_verify(const uint8_t expected_root[BLAKE4_STREAM_HASH_LEN],
                          const uint8_t *tree, size_t tree_len,
                          const uint8_t *data, size_t data_len);

/**
 * Compute root hash only (no tree storage).
 * This is equivalent to blake4_hash for the tree structure.
 */
void blake4_stream_hash(const uint8_t *data, size_t data_len,
                         uint8_t root_hash[BLAKE4_STREAM_HASH_LEN]);

/* ============== Tree Utilities ============== */

/**
 * Calculate the number of chunks for a given data length.
 */
static inline uint64_t blake4_stream_num_chunks(uint64_t data_len) {
    if (data_len == 0) return 1;  /* Empty input is one chunk */
    return (data_len + BLAKE4_STREAM_CHUNK_LEN - 1) / BLAKE4_STREAM_CHUNK_LEN;
}

/**
 * Calculate tree depth for a given number of chunks.
 */
static inline uint8_t blake4_stream_tree_depth(uint64_t num_chunks) {
    if (num_chunks <= 1) return 0;
    uint8_t depth = 0;
    uint64_t n = num_chunks - 1;
    while (n > 0) {
        depth++;
        n >>= 1;
    }
    return depth;
}

/**
 * Calculate the size of the tree data (excluding header) for a given data length.
 */
static inline size_t blake4_stream_tree_data_size(uint64_t data_len) {
    uint64_t num_chunks = blake4_stream_num_chunks(data_len);
    if (num_chunks <= 1) return 0;  /* No internal nodes for single chunk */
    /* Number of internal nodes = num_chunks - 1 for a complete tree */
    /* But we need to account for unbalanced trees */
    return (size_t)(num_chunks - 1) * BLAKE4_STREAM_HASH_LEN;
}

/**
 * Calculate the total tree file size for a given data length.
 */
static inline size_t blake4_stream_tree_file_size(uint64_t data_len) {
    return BLAKE4_STREAM_TREE_HEADER_LEN + blake4_stream_tree_data_size(data_len);
}

#ifdef __cplusplus
}
#endif

#endif /* BLAKE4_STREAM_H */
