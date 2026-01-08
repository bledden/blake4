# BLAKE4-STREAM Specification

> Normative specification for verified streaming with Merkle proofs.

**Version:** 1.0.0-draft
**Status:** Draft
**Based on:** BLAKE4-PORTABLE, Bao concepts

---

## 1. Overview

BLAKE4-STREAM extends BLAKE4-PORTABLE to provide:

1. **Verified streaming**: Verify data integrity as it's received, before the entire file is downloaded
2. **Random access verification**: Verify any byte range without processing the entire file
3. **Compact proofs**: Minimal overhead for proof transmission
4. **Interoperability**: Standardized formats for cross-implementation compatibility

### 1.1 Design Philosophy

BLAKE4-STREAM uses an **outboard tree** approach:

- Original data remains unchanged
- Merkle tree is stored separately in a "tree file"
- Proofs can be extracted for arbitrary byte ranges
- Compatible with content-addressed storage systems

---

## 2. Constants

```c
BLAKE4_STREAM_CHUNK_LEN  = 1024      // Chunk size in bytes
BLAKE4_STREAM_HASH_LEN   = 32        // Hash output length
BLAKE4_STREAM_MAX_DEPTH  = 54        // Maximum tree depth
BLAKE4_STREAM_VERSION    = 1         // Format version
```

### 2.1 Magic Bytes

| Format | Magic | Description |
|--------|-------|-------------|
| Tree file | `BK4T` | BLAKE4 Tree |
| Proof | `BK4P` | BLAKE4 Proof |

---

## 3. Tree Construction

### 3.1 Chunking

Input data is divided into chunks of `BLAKE4_STREAM_CHUNK_LEN` (1024) bytes.

- Last chunk may be smaller than 1024 bytes
- Empty input (0 bytes) is treated as a single empty chunk
- Number of chunks: `ceil(data_len / 1024)` or `1` if `data_len == 0`

### 3.2 Chunk Hashing

Each chunk is hashed using BLAKE4:

```
chunk_hash[i] = BLAKE4(chunk_data[i])
```

The chunk index and position flags are handled internally by BLAKE4's tree construction (CHUNK_START, CHUNK_END flags).

### 3.3 Tree Structure

The tree is a binary Merkle tree:

- **Leaves**: Chunk hashes
- **Internal nodes**: Hash of concatenated child hashes
- **Root**: Single 32-byte hash identifying the content

Parent node computation:

```
parent_hash = BLAKE4(left_child || right_child)
```

Where `||` denotes concatenation.

### 3.4 Unbalanced Trees

When the number of chunks is not a power of two:

- Odd nodes at any level are promoted to the next level unchanged
- This creates an unbalanced tree that leans left

Example for 5 chunks:

```
        Root
       /    \
      P1     P2
     /  \   /  \
    P3  C3 C4  C5
   /  \
  C1  C2
```

Where `Pn` are parent nodes and `Cn` are chunk hashes.

---

## 4. File Formats

### 4.1 Tree File Format

The tree file stores all internal nodes for efficient proof extraction.

```
Offset  Size    Field
------  ----    -----
0       4       Magic "BK4T"
4       1       Version (1)
5       1       Flags (reserved, set to 0)
6       2       Reserved (set to 0)
8       8       Data length (little-endian uint64)
16      32      Root hash
48      N*32    Internal nodes (pre-order traversal)
```

**Total header size**: 48 bytes

**Number of internal nodes**: `num_chunks - 1` for a complete tree

**Tree data size**: `(num_chunks - 1) * 32` bytes

### 4.2 Proof Format

A proof enables verification of a specific byte range.

```
Offset  Size       Field
------  ----       -----
0       4          Magic "BK4P"
4       1          Version (1)
5       1          Flags (reserved, set to 0)
6       2          Reserved (set to 0)
8       32         Root hash
40      8          Total data length (little-endian uint64)
48      8          Slice start offset (little-endian uint64)
56      8          Slice end offset (exclusive, little-endian uint64)
64      1          Proof depth
65      depth*32   Sibling hashes (from leaf to root)
```

**Header size**: 64 bytes
**Total proof size**: `65 + depth * 32` bytes

---

## 5. Operations

### 5.1 Encoding (Tree Building)

**Input**: Data bytes
**Output**: Tree file, root hash

Algorithm:

1. Divide data into 1024-byte chunks
2. Hash each chunk to produce leaf hashes
3. Build tree bottom-up:
   - Pair adjacent hashes
   - Compute parent hash for each pair
   - Promote odd hash to next level
   - Repeat until single root remains
4. Store internal nodes in pre-order traversal
5. Write tree file with header

### 5.2 Verification (Full File)

**Input**: Root hash, tree file, data
**Output**: Success/failure

Algorithm:

1. Parse tree file header
2. Verify magic, version, data length
3. Verify tree file root hash matches expected
4. Rebuild tree from data
5. Compare computed root hash to expected
6. Return success if match

### 5.3 Slice Extraction

**Input**: Tree file, data, byte range [start, end)
**Output**: Proof

Algorithm:

1. Determine which chunks cover the byte range
2. Walk tree from target leaf to root
3. Collect sibling hashes at each level
4. Package into proof format

### 5.4 Slice Verification

**Input**: Root hash, slice data, proof
**Output**: Success/failure

Algorithm:

1. Parse proof header
2. Verify magic, version
3. Verify root hash matches expected
4. Verify byte range matches
5. Compute chunk hash from slice data
6. Walk proof path from leaf to root:
   - At each level, combine with sibling hash
   - Compute parent hash
7. Compare final hash to root hash
8. Return success if match

---

## 6. Security Considerations

### 6.1 Collision Resistance

The tree inherits BLAKE4's collision resistance:

- Finding two different files with the same root hash requires breaking BLAKE4
- Modifying any byte changes the root hash

### 6.2 Second Preimage Resistance

An attacker cannot find alternative data that produces the same root hash without breaking BLAKE4's second preimage resistance.

### 6.3 Proof Soundness

A valid proof cryptographically binds the slice data to the root hash:

- Cannot forge a proof for data not in the original file
- Cannot substitute different data with same proof
- Truncated or modified proofs fail verification

### 6.4 Domain Separation

BLAKE4's internal domain separation (PARENT flag) prevents:

- Leaf/internal node confusion
- Length extension attacks
- Cross-protocol attacks

---

## 7. API Reference

### 7.1 Types

```c
typedef struct blake4_stream_encoder blake4_stream_encoder;
typedef struct blake4_stream_decoder blake4_stream_decoder;
```

### 7.2 Encoder Functions

```c
// Create encoder
blake4_stream_encoder* blake4_stream_encoder_new(void);

// Free encoder
void blake4_stream_encoder_free(blake4_stream_encoder *enc);

// Add data (can be called multiple times)
void blake4_stream_encoder_update(blake4_stream_encoder *enc,
                                   const uint8_t *data, size_t len);

// Finalize and get tree + root hash
int blake4_stream_encoder_finalize(blake4_stream_encoder *enc,
                                    uint8_t **tree_out, size_t *tree_len,
                                    uint8_t root_hash[32]);

// Get root hash only (no tree allocation)
void blake4_stream_encoder_root_hash(const blake4_stream_encoder *enc,
                                      uint8_t root_hash[32]);
```

### 7.3 Decoder Functions

```c
// Create decoder with expected root and tree
blake4_stream_decoder* blake4_stream_decoder_new(
    const uint8_t expected_root[32],
    const uint8_t *tree, size_t tree_len);

// Free decoder
void blake4_stream_decoder_free(blake4_stream_decoder *dec);

// Feed data and get verified output
ssize_t blake4_stream_decoder_update(blake4_stream_decoder *dec,
                                      const uint8_t *data, size_t len,
                                      uint8_t *verified_out, size_t out_len);

// Check final verification status
int blake4_stream_decoder_finalize(blake4_stream_decoder *dec);
```

### 7.4 Slice Functions

```c
// Calculate proof size for a range
size_t blake4_stream_proof_size(uint64_t data_len,
                                 uint64_t start, uint64_t end);

// Extract slice proof
int blake4_stream_extract_slice(const uint8_t *tree, size_t tree_len,
                                 const uint8_t *data, size_t data_len,
                                 uint64_t start, uint64_t end,
                                 uint8_t *proof_out, size_t *proof_len);

// Verify slice against proof
int blake4_stream_verify_slice(const uint8_t expected_root[32],
                                uint64_t start, uint64_t end,
                                const uint8_t *slice_data, size_t slice_len,
                                const uint8_t *proof, size_t proof_len);
```

### 7.5 Convenience Functions

```c
// One-shot encode
int blake4_stream_encode(const uint8_t *data, size_t data_len,
                          uint8_t **tree_out, size_t *tree_len,
                          uint8_t root_hash[32]);

// One-shot verify
int blake4_stream_verify(const uint8_t expected_root[32],
                          const uint8_t *tree, size_t tree_len,
                          const uint8_t *data, size_t data_len);

// Compute root hash only
void blake4_stream_hash(const uint8_t *data, size_t data_len,
                         uint8_t root_hash[32]);
```

### 7.6 Error Codes

```c
BLAKE4_STREAM_OK           =  0   // Success
BLAKE4_STREAM_ERR_INVALID  = -1   // Invalid parameters
BLAKE4_STREAM_ERR_FORMAT   = -2   // Invalid format
BLAKE4_STREAM_ERR_VERSION  = -3   // Unsupported version
BLAKE4_STREAM_ERR_VERIFY   = -4   // Verification failed
BLAKE4_STREAM_ERR_RANGE    = -5   // Invalid range
BLAKE4_STREAM_ERR_BUFFER   = -6   // Buffer too small
```

---

## 8. Test Vectors

### 8.1 Empty Input

```
Input: (empty)
Root hash: [computed by BLAKE4 of empty input]
Tree size: 48 bytes (header only)
```

### 8.2 Single Chunk (512 bytes of 0x42)

```
Input: 512 bytes, all 0x42
Chunks: 1
Tree size: 48 bytes (header only)
```

### 8.3 Two Chunks (2048 bytes)

```
Input: 2048 bytes, pattern i % 256
Chunks: 2
Internal nodes: 1
Tree size: 48 + 32 = 80 bytes
```

### 8.4 Multi-Chunk (5000 bytes)

```
Input: 5000 bytes
Chunks: 5
Internal nodes: 4
Tree size: 48 + 128 = 176 bytes
```

---

## 9. Comparison with Bao

| Feature | Bao | BLAKE4-STREAM |
|---------|-----|---------------|
| Chunk size | 1024 | 1024 |
| Tree structure | Binary Merkle | Binary Merkle |
| Embedded format | Yes | No |
| Outboard format | Yes | Yes |
| Specification | Informal | Normative |
| Test vectors | Limited | Comprehensive |
| Versioning | No | Yes |

BLAKE4-STREAM focuses on the outboard format and provides explicit versioning for forward compatibility.

---

## 10. Future Considerations

### 10.1 Embedded Format

A future version may add an embedded format where tree nodes are interleaved with data, enabling single-pass streaming without separate tree storage.

### 10.2 Parallel Encoding

The tree structure naturally supports parallel encoding - chunks can be hashed independently before tree construction.

### 10.3 Incremental Updates

For append-only use cases, the tree could support incremental updates without recomputing the entire tree.

---

## Appendix A: Reference Implementation Notes

The reference implementation in `src/blake4_stream.c`:

1. Buffers all input data before building tree (memory-intensive for large files)
2. Builds tree bottom-up with level-by-level merge
3. Stores internal nodes during construction
4. Streaming decoder currently does pass-through verification

Production implementations should:

1. Use streaming tree construction
2. Memory-map large files
3. Implement proper Merkle path verification in decoder
4. Support parallel chunk hashing
