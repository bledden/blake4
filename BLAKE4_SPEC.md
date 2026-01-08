# BLAKE4 Specification

**Version:** Draft 0.1
**Status:** Experimental

## Overview

BLAKE4 is a cryptographic hash function that extends the BLAKE family with a 512-bit internal state, providing a larger security margin compared to BLAKE3's 256-bit state.

## Design Rationale

| Parameter | BLAKE3 | BLAKE4 | Rationale |
|-----------|--------|--------|-----------|
| Word size | 32-bit | 64-bit | Efficient on modern 64-bit CPUs |
| State words | 8 | 8 | Same structure, larger words |
| Internal state | 256-bit | 512-bit | Doubled security margin |
| Block size | 64 bytes | 128 bytes | Doubled to match state |
| Chunk size | 1024 bytes | 2048 bytes | Doubled to maintain ratio |
| Rounds | 7 | 10 | Increased for larger state security |
| Output (default) | 32 bytes | 64 bytes | Matches internal state |

## Constants

### Initialization Vector (IV)

Derived from the fractional parts of the square roots of the first 8 primes, as 64-bit words:

```c
static const uint64_t IV[8] = {
    0x6A09E667F3BCC908,  // sqrt(2)
    0xBB67AE8584CAA73B,  // sqrt(3)
    0x3C6EF372FE94F82B,  // sqrt(5)
    0xA54FF53A5F1D36F1,  // sqrt(7)
    0x510E527FADE682D1,  // sqrt(11)
    0x9B05688C2B3E6C1F,  // sqrt(13)
    0x1F83D9ABFB41BD6B,  // sqrt(17)
    0x5BE0CD19137E2179   // sqrt(19)
};
```

These are the same as SHA-512's IV (FIPS 180-4), providing well-analyzed constants.

### Message Schedule

10 rounds with the following permutation pattern (same structure as BLAKE2b):

```c
static const uint8_t SIGMA[10][16] = {
    { 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15},
    {14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3},
    {11,  8, 12,  0,  5,  2, 15, 13, 10, 14,  3,  6,  7,  1,  9,  4},
    { 7,  9,  3,  1, 13, 12, 11, 14,  2,  6,  5, 10,  4,  0, 15,  8},
    { 9,  0,  5,  7,  2,  4, 10, 15, 14,  1, 11, 12,  6,  8,  3, 13},
    { 2, 12,  6, 10,  0, 11,  8,  3,  4, 13,  7,  5, 15, 14,  1,  9},
    {12,  5,  1, 15, 14, 13,  4, 10,  0,  7,  6,  3,  9,  2,  8, 11},
    {13, 11,  7, 14, 12,  1,  3,  9,  5,  0, 15,  4,  8,  6,  2, 10},
    { 6, 15, 14,  9, 11,  3,  0,  8, 12,  2, 13,  7,  1,  4, 10,  5},
    {10,  2,  8,  4,  7,  6,  1,  5, 15, 11,  9, 14,  3, 12, 13,  0},
};
```

### Rotation Constants

For 64-bit words (same as BLAKE2b):

```c
#define R1 32
#define R2 24
#define R3 16
#define R4 63
```

## Compression Function

### State Initialization

The compression function state is 16 × 64-bit words:

```
     v[0]   v[1]   v[2]   v[3]
     v[4]   v[5]   v[6]   v[7]
     v[8]   v[9]  v[10]  v[11]
    v[12]  v[13]  v[14]  v[15]
```

Initialized as:
- `v[0..7]` = chaining value (CV) or IV for first block
- `v[8..11]` = IV[0..3]
- `v[12]` = IV[4] XOR counter_low
- `v[13]` = IV[5] XOR counter_high
- `v[14]` = IV[6] XOR block_len
- `v[15]` = IV[7] XOR flags

### G Function

The quarter-round mixing function:

```c
void G(uint64_t *v, int a, int b, int c, int d, uint64_t mx, uint64_t my) {
    v[a] = v[a] + v[b] + mx;
    v[d] = ROTR64(v[d] ^ v[a], R1);  // 32
    v[c] = v[c] + v[d];
    v[b] = ROTR64(v[b] ^ v[c], R2);  // 24
    v[a] = v[a] + v[b] + my;
    v[d] = ROTR64(v[d] ^ v[a], R3);  // 16
    v[c] = v[c] + v[d];
    v[b] = ROTR64(v[b] ^ v[c], R4);  // 63
}
```

### Round Function

Each round applies 8 G functions in a specific pattern:

```c
void round(uint64_t *v, uint64_t *m, int r) {
    // Column step
    G(v, 0, 4,  8, 12, m[SIGMA[r][ 0]], m[SIGMA[r][ 1]]);
    G(v, 1, 5,  9, 13, m[SIGMA[r][ 2]], m[SIGMA[r][ 3]]);
    G(v, 2, 6, 10, 14, m[SIGMA[r][ 4]], m[SIGMA[r][ 5]]);
    G(v, 3, 7, 11, 15, m[SIGMA[r][ 6]], m[SIGMA[r][ 7]]);

    // Diagonal step
    G(v, 0, 5, 10, 15, m[SIGMA[r][ 8]], m[SIGMA[r][ 9]]);
    G(v, 1, 6, 11, 12, m[SIGMA[r][10]], m[SIGMA[r][11]]);
    G(v, 2, 7,  8, 13, m[SIGMA[r][12]], m[SIGMA[r][13]]);
    G(v, 3, 4,  9, 14, m[SIGMA[r][14]], m[SIGMA[r][15]]);
}
```

### Full Compression

```c
void compress(uint64_t cv[8], uint8_t block[128], uint64_t counter,
              uint32_t block_len, uint32_t flags, uint64_t out[16]) {
    uint64_t v[16], m[16];

    // Initialize state
    for (int i = 0; i < 8; i++) v[i] = cv[i];
    v[8]  = IV[0];
    v[9]  = IV[1];
    v[10] = IV[2];
    v[11] = IV[3];
    v[12] = IV[4] ^ counter;
    v[13] = IV[5] ^ (counter >> 64);  // High bits if needed
    v[14] = IV[6] ^ block_len;
    v[15] = IV[7] ^ flags;

    // Load message
    for (int i = 0; i < 16; i++) {
        m[i] = load64_le(block + i * 8);
    }

    // 10 rounds
    for (int r = 0; r < 10; r++) {
        round(v, m, r);
    }

    // Finalize
    for (int i = 0; i < 8; i++) {
        out[i] = v[i] ^ v[i + 8];
    }
    for (int i = 8; i < 16; i++) {
        out[i] = v[i] ^ cv[i - 8];
    }
}
```

## Domain Separation Flags

Same flags as BLAKE3, in the low bits of the flags word:

| Flag | Value | Meaning |
|------|-------|---------|
| CHUNK_START | 1 << 0 | First block of a chunk |
| CHUNK_END | 1 << 1 | Last block of a chunk |
| PARENT | 1 << 2 | Parent node in tree |
| ROOT | 1 << 3 | Root node (final output) |
| KEYED_HASH | 1 << 4 | Keyed mode |
| DERIVE_KEY_CONTEXT | 1 << 5 | Key derivation context phase |
| DERIVE_KEY_MATERIAL | 1 << 6 | Key derivation material phase |

## Tree Structure

Same Merkle tree structure as BLAKE3:
- Chunk size: 2048 bytes (16 blocks of 128 bytes)
- Binary tree with left-to-right bottom-up construction
- Counter increments per chunk, not per block

## Output Modes

### Fixed Output (Default: 64 bytes)
Return the first N bytes of the root output.

### XOF (Extendable Output)
Counter mode on the root compression output, producing unlimited bytes.

## Differences from BLAKE3

| Aspect | BLAKE3 | BLAKE4 |
|--------|--------|--------|
| Word size | 32-bit | 64-bit |
| State size | 256-bit | 512-bit |
| Block size | 64 bytes | 128 bytes |
| Chunk size | 1024 bytes | 2048 bytes |
| Rounds | 7 | 10 |
| Default output | 32 bytes | 64 bytes |
| Key size | 32 bytes | 64 bytes |
| Rotation constants | 16, 12, 8, 7 | 32, 24, 16, 63 |

## Security Claims

- Collision resistance: 256 bits (birthday bound of 512-bit state)
- Preimage resistance: 512 bits
- Second preimage resistance: 512 bits
- PRF security: 512 bits (in keyed mode)

## Test Vectors

All hashes are 64 bytes (512 bits), shown in hexadecimal.

### Empty Input
```
Input:  (empty)
Hash:   741c4ffdf535f7a77843b052bc106f109f1ae51895173c10ae9eb0f1d3e7b147
        1a1791b0ac27f0d63ba391ed31734691b8311b8502c0eddf7cc60e0b89f8e11e
```

### ASCII "abc"
```
Input:  0x616263
Hash:   0585c5c0e59ba7f064bb89065e60a2a41a96cd8ff5490a4b2f0a02ee8a4d71b6
        2ab9283c03114dfee37b63fd0622a41f889d9ac8845317080cb7c95a75673377
```

### One Block (128 bytes)
```
Input:  0x00 0x01 0x02 ... 0x7F
Hash:   21484e45ee40be624ac835126b6ecf1d5606890e49ddd1e9de3d41964bfddcd1
        507fceea9bf3343a090b5d3678f22040fcbca8d00d2191942af88a48e3c790de
```

### One Chunk (2048 bytes)
```
Input:  Sequential bytes 0x00..0xFA repeated (mod 251)
Hash:   651051cd629098b4a480748d90f0b56151d38f864c26fb50bb01a251a0746692
        c24e987283b5c9ebacd558757bbaf138f51c1b7faa9629f58294c6f4ae2cf5b5
```

### Two Chunks (4096 bytes)
```
Input:  Sequential bytes 0x00..0xFA repeated (mod 251)
Hash:   e3778cbd58a832ab8b922e90a24eed39e223c112d8f19c92b5a5e8d3590c1e7f
        abf5d3385b6b06773aafafec5e149f66d62b08cae8b88f6c15f1263a3c6d3f42
```

## Implementation Notes

1. **Endianness**: All multi-byte values are little-endian
2. **Alignment**: Implementations should handle unaligned input
3. **Parallelism**: Tree structure enables parallel chunk processing
4. **Streaming**: Same streaming properties as BLAKE3

## References

- BLAKE (SHA-3 competition): Aumasson et al.
- BLAKE2: RFC 7693
- BLAKE3: https://github.com/BLAKE3-team/BLAKE3-specs
- SHA-512 IV: FIPS 180-4
