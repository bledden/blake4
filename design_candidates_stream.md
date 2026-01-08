# BLAKE4-STREAM Design Candidates

> Phase 2: Design options for verified streaming with Merkle proofs.

---

## Background

BLAKE4-STREAM builds on BLAKE4-PORTABLE to provide:
- Streaming verification of large files
- Random access verification of arbitrary byte ranges
- Standardized proof format for interoperability
- Integration with content-addressed storage systems

This is based on concepts from [Bao](https://github.com/oconnor663/bao) but with a normative specification.

---

## Design Goals

1. **Verify-as-you-stream**: Start verifying before download completes
2. **Random access**: Verify any byte range without processing entire file
3. **Compact proofs**: Minimal overhead for proof transmission
4. **Interoperability**: Standard format for cross-implementation compatibility
5. **Security**: Cryptographic binding to root hash

---

## Key Design Decisions

### D1: Chunk Size

The chunk size determines granularity of verification.

| Option | Size | Proofs/GB | Pros | Cons |
|--------|------|-----------|------|------|
| A | 1 KiB | 1M | Fine granularity | Large tree, more overhead |
| B | 16 KiB | 64K | Balance | Reasonable for most uses |
| C | 64 KiB | 16K | Fewer nodes | Coarser verification |
| D | 1 MiB | 1K | Minimal overhead | Very coarse verification |

**Recommendation: 1 KiB (Option A)**

Rationale:
- Matches BLAKE3/Bao chunk size (1024 bytes)
- Enables fine-grained streaming verification
- Tree depth is log2(file_size / 1024), manageable even for huge files
- 1GB file = 20 levels deep, proof size ≈ 20 * 32 = 640 bytes

### D2: Tree Structure

**Binary Merkle tree** (same as BLAKE3):
- Each parent node hashes two 32-byte child CVs
- Left-to-right, bottom-up construction
- Unbalanced trees handled by promoting odd nodes

### D3: Proof Format

A proof consists of:
1. **Root hash**: 32 bytes, identifies the content
2. **Chunk index**: Which chunk is being proven
3. **Sibling hashes**: Path from chunk to root
4. **Chunk data**: The actual 1 KiB chunk

```
Proof := {
    root_hash: [u8; 32],
    chunk_index: u64,
    siblings: [[u8; 32]; depth],
    chunk_data: [u8; 0..1024],  // Last chunk may be smaller
}
```

### D4: Encoding Format

**Option A: Simple Binary**
```
[4 bytes: magic "BK4S"]
[1 byte: version]
[1 byte: flags]
[2 bytes: reserved]
[32 bytes: root hash]
[8 bytes: total length]
[8 bytes: chunk index]
[1 byte: proof depth]
[depth * 32 bytes: siblings]
[2 bytes: chunk length]
[chunk_length bytes: chunk data]
```

**Option B: CBOR-encoded**
More flexible, self-describing, but adds dependency.

**Option C: Protobuf**
Good tooling, but heavyweight for simple proofs.

**Recommendation: Option A (Simple Binary)**

Rationale:
- No external dependencies
- Minimal parsing overhead
- Easy to implement in any language
- Fixed structure is easy to validate

---

## Candidate Designs

### Candidate A: Embedded Proofs (Bao-style)

**Concept**: Interleave hash tree nodes with data in a single stream.

```
[Header]
[Root hash]
[Left subtree | Right subtree]  (recursive)
...
[Chunk data]
```

**Encoding process**:
1. Write header
2. Recursively write tree: for each parent, write left child, then right
3. Leaf nodes are followed by their chunk data

**Decoding process**:
1. Read header and root hash
2. Recursively verify: read expected hash, verify children
3. Output verified chunk data as available

**Pros**:
- Stream entire file with verification in single pass
- Decoder can output data as it verifies

**Cons**:
- File size increases by ~6% (hash overhead)
- Seeking requires parsing tree structure
- Complex encoder/decoder

### Candidate B: Separate Proofs (Slice-based)

**Concept**: Keep data separate, generate proofs on demand.

**API**:
```c
// Generate proof for a byte range
int blake4_stream_prove(const uint8_t *data, size_t data_len,
                        uint64_t start, uint64_t end,
                        uint8_t *proof_out, size_t *proof_len);

// Verify proof against root hash
int blake4_stream_verify(const uint8_t root_hash[32],
                         uint64_t start, uint64_t end,
                         const uint8_t *proof, size_t proof_len,
                         const uint8_t *data, size_t data_len);
```

**Pros**:
- Data stays unchanged
- Generate proofs for arbitrary ranges
- Simple verification

**Cons**:
- Requires full data to generate proofs
- Need to transmit proof + data separately

### Candidate C: Outboard Tree (Recommended)

**Concept**: Store tree separately from data, enabling both streaming and random access.

**Components**:
1. **Data file**: Original file, unchanged
2. **Tree file**: All internal tree nodes in pre-order
3. **Root hash**: 32-byte identifier

**Tree file format**:
```
[4 bytes: magic "BK4T"]
[1 byte: version]
[1 byte: flags]
[2 bytes: reserved]
[8 bytes: data length]
[32 bytes: root hash]
[N * 32 bytes: internal nodes in pre-order]
```

**Operations**:

```c
// Build tree from data, write to tree_file
int blake4_stream_encode(const uint8_t *data, size_t data_len,
                         uint8_t *tree_out, size_t *tree_len,
                         uint8_t root_hash[32]);

// Verify entire file using tree
int blake4_stream_decode_all(const uint8_t *data, size_t data_len,
                             const uint8_t *tree, size_t tree_len,
                             const uint8_t expected_root[32]);

// Extract and verify a slice
int blake4_stream_slice(const uint8_t *data, size_t data_len,
                        const uint8_t *tree, size_t tree_len,
                        uint64_t start, uint64_t end,
                        uint8_t *slice_out,
                        uint8_t *proof_out, size_t *proof_len);

// Verify a slice with its proof
int blake4_stream_verify_slice(const uint8_t expected_root[32],
                               uint64_t start, uint64_t end,
                               const uint8_t *slice_data, size_t slice_len,
                               const uint8_t *proof, size_t proof_len);
```

**Pros**:
- Original data unchanged
- Efficient random access with pre-computed tree
- Proof extraction is O(log n)
- Tree size is ~3% of data (vs 6% for embedded)

**Cons**:
- Two files to manage
- Need tree file for proof generation

---

## Recommended Design: Candidate C (Outboard Tree)

### Rationale

1. **Flexibility**: Supports both streaming and random access
2. **Efficiency**: Smaller overhead than embedded proofs
3. **Practicality**: Original files stay unchanged
4. **Compatibility**: Tree can be computed once, proofs extracted many times

### API Design

```c
/* ============== Types ============== */

#define BLAKE4_STREAM_CHUNK_LEN 1024
#define BLAKE4_STREAM_HASH_LEN 32

typedef struct blake4_stream_encoder blake4_stream_encoder;
typedef struct blake4_stream_decoder blake4_stream_decoder;

/* ============== Encoding (Build Tree) ============== */

// Create encoder
blake4_stream_encoder* blake4_stream_encoder_new(void);
void blake4_stream_encoder_free(blake4_stream_encoder *enc);

// Feed data (can be called multiple times)
void blake4_stream_encoder_update(blake4_stream_encoder *enc,
                                  const uint8_t *data, size_t len);

// Finalize and get tree + root hash
int blake4_stream_encoder_finalize(blake4_stream_encoder *enc,
                                   uint8_t **tree_out, size_t *tree_len,
                                   uint8_t root_hash[32]);

/* ============== Decoding (Verify) ============== */

// Create decoder with expected root
blake4_stream_decoder* blake4_stream_decoder_new(
    const uint8_t expected_root[32],
    const uint8_t *tree, size_t tree_len);
void blake4_stream_decoder_free(blake4_stream_decoder *dec);

// Feed data and get verified output
// Returns number of verified bytes (may be less than input if at chunk boundary)
ssize_t blake4_stream_decoder_update(blake4_stream_decoder *dec,
                                     const uint8_t *data, size_t len,
                                     uint8_t *verified_out, size_t out_len);

// Check if all data verified successfully
int blake4_stream_decoder_finalize(blake4_stream_decoder *dec);

/* ============== Slice Operations ============== */

// Extract a slice with proof
int blake4_stream_extract_slice(
    const uint8_t *tree, size_t tree_len,
    const uint8_t *data, size_t data_len,
    uint64_t start, uint64_t end,
    uint8_t *proof_out, size_t *proof_len);

// Verify a slice
int blake4_stream_verify_slice(
    const uint8_t expected_root[32],
    uint64_t start, uint64_t end,
    const uint8_t *slice_data, size_t slice_len,
    const uint8_t *proof, size_t proof_len);

/* ============== Convenience ============== */

// One-shot encode
int blake4_stream_encode(const uint8_t *data, size_t data_len,
                         uint8_t **tree_out, size_t *tree_len,
                         uint8_t root_hash[32]);

// One-shot verify
int blake4_stream_verify(const uint8_t expected_root[32],
                         const uint8_t *tree, size_t tree_len,
                         const uint8_t *data, size_t data_len);

// Compute root hash only (no tree storage)
void blake4_stream_hash(const uint8_t *data, size_t data_len,
                        uint8_t root_hash[32]);
```

### Proof Wire Format

```
BLAKE4_STREAM_PROOF := {
    magic: "BK4P" (4 bytes)
    version: u8
    flags: u8
    reserved: u16
    root_hash: [u8; 32]
    data_len: u64 (total file length)
    start: u64 (slice start offset)
    end: u64 (slice end offset, exclusive)
    proof_depth: u8
    siblings: [[u8; 32]; proof_depth] (from leaf to root)
}
```

Note: Slice data is transmitted separately (not in proof).

### Tree File Format

```
BLAKE4_STREAM_TREE := {
    magic: "BK4T" (4 bytes)
    version: u8
    flags: u8
    reserved: u16
    data_len: u64
    root_hash: [u8; 32]
    nodes: [[u8; 32]; num_internal_nodes] (pre-order traversal)
}
```

Number of internal nodes = num_chunks - 1 (for complete binary tree).

---

## Test Plan

### Correctness Tests

1. Empty file (0 bytes)
2. Single chunk (1-1024 bytes)
3. Two chunks (1025-2048 bytes)
4. Power-of-two chunks (1024, 2048, 4096, ... bytes)
5. Non-power-of-two chunks (1500, 3000, 5000 bytes)
6. Large file (1MB, 100MB)

### Slice Tests

1. Single chunk slice
2. Multi-chunk slice
3. Partial chunk at start
4. Partial chunk at end
5. Entire file as slice
6. Adjacent slices cover whole file

### Interoperability Tests

1. Verify tree/proof format matches specification
2. Cross-implementation verification (C vs Rust)
3. Bao compatibility mode (if desired)

### Security Tests

1. Tampered chunk data fails verification
2. Tampered sibling hash fails verification
3. Wrong root hash fails verification
4. Truncated proof fails verification
5. Out-of-bounds slice indices fail

---

## Implementation Order

1. [ ] Tree encoding (build tree from data)
2. [ ] Tree storage format
3. [ ] Full file verification
4. [ ] Slice extraction
5. [ ] Slice verification
6. [ ] Streaming encoder
7. [ ] Streaming decoder
8. [ ] Test suite
9. [ ] Benchmarks

---

## Comparison with Bao

| Feature | Bao | BLAKE4-STREAM |
|---------|-----|---------------|
| Chunk size | 1024 | 1024 |
| Tree structure | Binary Merkle | Binary Merkle |
| Embedded format | Yes | No (outboard only) |
| Outboard format | Yes | Yes |
| Streaming decode | Yes | Yes |
| Slice extraction | Yes | Yes |
| Specification | Informal | Normative |
| Test vectors | Limited | Comprehensive |

BLAKE4-STREAM focuses on the outboard format for simplicity and avoids the embedded format complexity.
