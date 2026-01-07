# BLAKE4-PORTABLE Specification

> Normative specification for BLAKE4-PORTABLE (Draft 0.1)

---

## 1. Overview

BLAKE4-PORTABLE is a cryptographic hash function derived from BLAKE3 with:
- Optimized short-input performance
- Stable C API/ABI
- State serialization support
- Clean build system integration

BLAKE4-PORTABLE produces outputs identical to BLAKE3 for inputs larger than one chunk (1024 bytes) when using 256-bit output mode.

---

## 2. Parameters

| Parameter | Value | Description |
|-----------|-------|-------------|
| BLOCK_LEN | 64 | Block size in bytes |
| CHUNK_LEN | 1024 | Chunk size in bytes |
| KEY_LEN | 32 | Key size in bytes |
| OUT_LEN | 32 | Default output size in bytes |
| MAX_DEPTH | 54 | Maximum tree depth |

### 2.1 Initialization Vector

```
IV[0] = 0x6A09E667
IV[1] = 0xB7E15163
IV[2] = 0x3C6EF372
IV[3] = 0xA54FF53A
IV[4] = 0x510E527F
IV[5] = 0x9B05688C
IV[6] = 0x1F83D9AB
IV[7] = 0x5BE0CD19
```

(Same as BLAKE3, derived from SHA-256 IV)

### 2.2 Flags

```
CHUNK_START         = 1 << 0
CHUNK_END           = 1 << 1
PARENT              = 1 << 2
ROOT                = 1 << 3
KEYED_HASH          = 1 << 4
DERIVE_KEY_CONTEXT  = 1 << 5
DERIVE_KEY_MATERIAL = 1 << 6
```

---

## 3. Compression Function

The compression function is identical to BLAKE3.

### 3.1 State Initialization

```
state[0..7]  = chaining_value[0..7]
state[8..11] = IV[0..3]
state[12]    = counter (low 32 bits)
state[13]    = counter (high 32 bits)
state[14]    = block_len
state[15]    = flags
```

### 3.2 Message Schedule

```
SIGMA[0]  = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15]
SIGMA[1]  = [2, 6, 3, 10, 7, 0, 4, 13, 1, 11, 12, 5, 9, 14, 15, 8]
SIGMA[2]  = [3, 4, 10, 12, 13, 2, 7, 14, 6, 5, 9, 0, 11, 15, 8, 1]
SIGMA[3]  = [10, 7, 12, 9, 14, 3, 13, 15, 4, 0, 11, 2, 5, 8, 1, 6]
SIGMA[4]  = [12, 13, 9, 11, 15, 10, 14, 8, 7, 2, 5, 3, 0, 1, 6, 4]
SIGMA[5]  = [9, 14, 11, 5, 8, 12, 15, 1, 13, 3, 0, 10, 2, 6, 4, 7]
SIGMA[6]  = [11, 15, 5, 0, 1, 9, 8, 6, 14, 10, 2, 12, 3, 4, 7, 13]
```

### 3.3 G Function

```
G(state, a, b, c, d, mx, my):
    state[a] = state[a] + state[b] + mx
    state[d] = rotr32(state[d] ^ state[a], 16)
    state[c] = state[c] + state[d]
    state[b] = rotr32(state[b] ^ state[c], 12)
    state[a] = state[a] + state[b] + my
    state[d] = rotr32(state[d] ^ state[a], 8)
    state[c] = state[c] + state[d]
    state[b] = rotr32(state[b] ^ state[c], 7)
```

### 3.4 Round Function

```
round(state, m, sigma):
    // Column step
    G(state, 0, 4,  8, 12, m[sigma[0]],  m[sigma[1]])
    G(state, 1, 5,  9, 13, m[sigma[2]],  m[sigma[3]])
    G(state, 2, 6, 10, 14, m[sigma[4]],  m[sigma[5]])
    G(state, 3, 7, 11, 15, m[sigma[6]],  m[sigma[7]])
    // Diagonal step
    G(state, 0, 5, 10, 15, m[sigma[8]],  m[sigma[9]])
    G(state, 1, 6, 11, 12, m[sigma[10]], m[sigma[11]])
    G(state, 2, 7,  8, 13, m[sigma[12]], m[sigma[13]])
    G(state, 3, 4,  9, 14, m[sigma[14]], m[sigma[15]])
```

### 3.5 Compression

```
compress(chaining_value, block, block_len, counter, flags):
    state = initialize_state(chaining_value, counter, block_len, flags)
    m = words_from_bytes(block)  // 16 x 32-bit words

    for i in 0..7:
        round(state, m, SIGMA[i % 7])

    // XOR the two halves
    for i in 0..8:
        state[i] ^= state[i + 8]

    return state[0..7]
```

---

## 4. Tree Hashing

### 4.1 Chunk Processing

Each chunk (up to 1024 bytes) is processed as a sequence of blocks:

```
chunk_chaining_value(chunk, chunk_counter, key_words, flags):
    cv = key_words  // or IV for non-keyed mode
    chunk_flags = flags

    num_blocks = ceil(len(chunk) / BLOCK_LEN)
    for i in 0..num_blocks:
        block = chunk[i * BLOCK_LEN : (i+1) * BLOCK_LEN]
        block_flags = chunk_flags
        if i == 0:
            block_flags |= CHUNK_START
        if i == num_blocks - 1:
            block_flags |= CHUNK_END
        cv = compress(cv, block, len(block), chunk_counter, block_flags)

    return cv
```

### 4.2 Parent Node

```
parent_chaining_value(left_cv, right_cv, key_words, flags):
    block = left_cv || right_cv  // 64 bytes
    return compress(key_words, block, 64, 0, flags | PARENT)
```

### 4.3 Tree Construction

The tree is built bottom-up:
1. Hash input chunks to get leaf chaining values
2. Combine pairs of chaining values into parent nodes
3. Continue until single root remains
4. Finalize root with ROOT flag

---

## 5. Short-Input Optimization (BLAKE4-specific)

For inputs that fit in a single chunk (≤ 1024 bytes), BLAKE4 uses a simplified path that avoids tree state management.

### 5.1 Single-Chunk Path

```
blake4_hash_short(input, input_len, out, out_len):
    assert input_len <= CHUNK_LEN

    cv = IV
    num_blocks = max(1, ceil(input_len / BLOCK_LEN))

    for i in 0..num_blocks:
        block = input[i * BLOCK_LEN : (i+1) * BLOCK_LEN]
        flags = 0
        if i == 0:
            flags |= CHUNK_START
        if i == num_blocks - 1:
            flags |= CHUNK_END | ROOT
        cv = compress(cv, block, len(block), 0, flags)

    output_from_cv(cv, out, out_len)
```

### 5.2 Performance Benefit

This path eliminates:
- Tree state allocation
- CV stack management
- Extra memory copies

For single-block inputs (≤ 64 bytes), this reduces to a single compression call.

---

## 6. Modes

### 6.1 Hash Mode (Default)

```
blake4_hash(input):
    return blake4_process(IV, input, 0)
```

### 6.2 Keyed Hash Mode (MAC)

```
blake4_keyed_hash(key, input):
    key_words = words_from_bytes(key)
    return blake4_process(key_words, input, KEYED_HASH)
```

### 6.3 Key Derivation Mode

```
blake4_derive_key(context, key_material):
    // First, derive context key
    context_hasher = blake4_init_derive_key_context()
    context_hasher.update(context)
    context_key = context_hasher.finalize()

    // Then derive output from key material
    key_words = words_from_bytes(context_key)
    return blake4_process(key_words, key_material, DERIVE_KEY_MATERIAL)
```

### 6.4 XOF Mode

The hasher can produce arbitrary-length output by incrementing a seek counter:

```
blake4_xof(input, seek, out_len):
    root_cv = blake4_compute_root(input)

    for block_index in (seek / 64)..(seek + out_len) / 64:
        block_out = compress(root_cv, [0; 64], 64, block_index, ROOT)
        copy relevant bytes to output
```

---

## 7. State Serialization

### 7.1 Format

```
BLAKE4_STATE_MAGIC = 0x424B3453  // "BK4S"
BLAKE4_STATE_VERSION = 1

serialized_state:
    magic: u32          // BLAKE4_STATE_MAGIC
    version: u8         // BLAKE4_STATE_VERSION
    mode: u8            // 0=hash, 1=keyed, 2=derive_context, 3=derive_material
    flags: u8           // Current flags
    _reserved: u8
    total_len: u64      // Total bytes processed
    cv: [u32; 8]        // Current chaining value
    key_words: [u32; 8] // Key words (if keyed mode)
    buf_len: u16        // Bytes in buffer
    buf: [u8; 64]       // Block buffer
    chunk_len: u16      // Bytes in current chunk
    chunk_buf: [u8; 1024] // Chunk buffer (only if chunk_len > 0)
    cv_stack_len: u8    // Number of CVs on stack
    cv_stack: [[u32; 8]; cv_stack_len]  // CV stack
    checksum: u32       // CRC32 of preceding bytes
```

### 7.2 Deserialization Validation

1. Check magic number
2. Check version compatibility
3. Validate lengths are within bounds
4. Verify checksum
5. Return error if any check fails

---

## 8. C API

```c
// === Constants ===
#define BLAKE4_VERSION_MAJOR 1
#define BLAKE4_VERSION_MINOR 0
#define BLAKE4_VERSION_PATCH 0
#define BLAKE4_OUT_LEN 32
#define BLAKE4_KEY_LEN 32
#define BLAKE4_BLOCK_LEN 64
#define BLAKE4_CHUNK_LEN 1024
#define BLAKE4_MAX_STATE_SIZE 2048

// === Types ===
typedef struct {
    // Opaque - size is BLAKE4_HASHER_SIZE
    uint8_t opaque[1920];
} blake4_hasher;

// === Version ===
const char* blake4_version_string(void);

// === Initialization ===
void blake4_hasher_init(blake4_hasher *self);
void blake4_hasher_init_keyed(blake4_hasher *self, const uint8_t key[BLAKE4_KEY_LEN]);
void blake4_hasher_init_derive_key(blake4_hasher *self, const char *context);
void blake4_hasher_init_derive_key_raw(blake4_hasher *self, const void *context, size_t context_len);

// === Processing ===
void blake4_hasher_update(blake4_hasher *self, const void *input, size_t input_len);
void blake4_hasher_finalize(const blake4_hasher *self, uint8_t *out, size_t out_len);
void blake4_hasher_finalize_seek(const blake4_hasher *self, uint64_t seek, uint8_t *out, size_t out_len);
void blake4_hasher_reset(blake4_hasher *self);

// === Serialization ===
size_t blake4_hasher_state_size(const blake4_hasher *self);
int blake4_hasher_serialize(const blake4_hasher *self, uint8_t *buf, size_t buf_len);
int blake4_hasher_deserialize(blake4_hasher *self, const uint8_t *buf, size_t buf_len);

// === Convenience ===
void blake4_hash(const void *input, size_t input_len, uint8_t out[BLAKE4_OUT_LEN]);
void blake4_hash_keyed(const uint8_t key[BLAKE4_KEY_LEN], const void *input, size_t input_len, uint8_t out[BLAKE4_OUT_LEN]);
```

---

## 9. Security Considerations

### 9.1 Constant-Time Operations

All operations on secret data (keyed modes) MUST be constant-time:
- No secret-dependent branches
- No secret-dependent memory access
- No secret-dependent timing

### 9.2 State Serialization Warnings

Serialized hasher state exposes internal values. Users MUST:
- Not expose serialized state of keyed hashers
- Protect serialized state with appropriate access controls
- Validate deserialized state before use

### 9.3 Domain Separation

Different modes produce different outputs for the same input due to flag differences. This is intentional and provides domain separation.

---

## 10. Test Vectors

### 10.1 Empty Input

```
blake4_hash("") =
  af1349b9f5f9a1a6a0404dea36dcc9499bcb25c9adc112b7cc9a93cae41f3262
```
(Same as BLAKE3)

### 10.2 Short Inputs

```
blake4_hash("a") =
  17762fddd969a453925d65717ac3eea21320b66b54342fde15128d6caf21215f

blake4_hash("abc") =
  6437b3ac38465133ffb63b75273a8db548c558465d79db03fd359c6cd5bd9d85

blake4_hash(64 bytes of 0x00) =
  [TBD - generate with reference implementation]
```

### 10.3 Chunk Boundary

```
blake4_hash(1024 bytes of 0x00) = [TBD]
blake4_hash(1025 bytes of 0x00) = [TBD]
```

### 10.4 Keyed Mode

```
key = 32 bytes of 0x00
blake4_keyed_hash(key, "abc") = [TBD]
```

---

## 11. Differences from BLAKE3

| Aspect | BLAKE3 | BLAKE4-PORTABLE |
|--------|--------|-----------------|
| Algorithm | - | Identical for inputs > 1 chunk |
| Short-input path | Generic | Optimized |
| State serialization | Not specified | Specified format |
| C ABI | Not stable | Versioned, stable |
| Build system | Basic Makefile | CMake + pkg-config |
| Default output | 32 bytes | 32 bytes |

For inputs larger than 1024 bytes with 256-bit output, BLAKE3 and BLAKE4-PORTABLE produce identical hashes.
