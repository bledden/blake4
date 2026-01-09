# BLAKE4 Test Vectors

> Comprehensive test vectors for BLAKE4-PORTABLE and BLAKE4-STREAM implementations.

**Version:** 1.0.0
**Format:** All hashes are hex-encoded, little-endian byte order

---

## 1. BLAKE4-PORTABLE Test Vectors

### 1.1 Basic Hashing (BLAKE3-Compatible)

These vectors are compatible with BLAKE3 and can be used for cross-validation.

#### Empty Input

```
Input:  (empty string, 0 bytes)
Output: af1349b9f5f9a1a6a0404dea36dcc9499bcb25c9adc112b7cc9a93cae41f3262
```

#### Single Byte (0x00)

```
Input:  00
Output: 2d3adedff11b61f14c886e35afa036736dcd87a74d27b5c1510225d0f592e213
```

#### ASCII String "abc"

```
Input:  616263 (ASCII: "abc")
Output: 6437b3ac38465133ffb63b75273a8db548c558465d79db03fd359c6cd5bd9d85
```

#### 64 Bytes (One Block)

```
Input:  000102030405060708090a0b0c0d0e0f...3f (bytes 0x00 through 0x3f)
Output: 4eed7141ea4a5cd4b788606bd23f46e212af9cacebacdc7d1f4c6dc7f2511b98
```

### 1.2 Chunk Boundary Tests

#### One Chunk (1024 bytes, pattern i % 256)

```
Input:  000102...ff000102...ff... (4 repetitions of 0-255)
Chunks: 1
Output: 882179b8dbccd285cda241d968cfcccb3156c5edac2fa3761bb6eda7ff8cb172
```

#### Two Chunks (2048 bytes, pattern i % 256)

```
Input:  000102...ff000102...ff... (8 repetitions of 0-255)
Chunks: 2
Output: cd06da8aa321e2da17047a7e94dfab89baef6f10340e8b72e2610faaca4fe31e
```

### 1.3 XOF (Extended Output Function) Tests

#### 64-byte Output

```
Input:  616263 (ASCII: "abc")
Output (64 bytes):
6437b3ac38465133ffb63b75273a8db548c558465d79db03fd359c6cd5bd9d85
1fb250ae7393f5d02813b65d521a0d492d9ba09cf7ce7f4cffd900f23374bf0b
```

Note: First 32 bytes match the standard hash of "abc".

#### Seek Test

```
Input:   616263 (ASCII: "abc")
Seek:    32
Length:  32
Output:  1fb250ae7393f5d02813b65d521a0d492d9ba09cf7ce7f4cffd900f23374bf0b
```

### 1.4 Keyed Mode Tests

#### Keyed Hash with Zero Key

```
Key:    0000000000000000000000000000000000000000000000000000000000000000
Input:  (empty)
Output: a7f91ced0533c12cd59706f2dc38c2a8c39c007ae89ab6492698778c8684c483
```

#### Keyed Hash with Sequential Key

```
Key:    000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f
Input:  616263 (ASCII: "abc")
Output: 6da54495d8152f2bcba87bd7282df70901cdb66b4448ed5f4c7bd2852b8b5532
```

### 1.5 Key Derivation Mode Tests

#### Context: "BLAKE4 test context"

```
Context:      "BLAKE4 test context"
Key Material: "key material" (12 bytes)
Output (32):  7a560576cb9d77bec024544629bf9696025b1312d49cda397fff0432ffb11398
```

---

## 2. BLAKE4-STREAM Test Vectors

### 2.1 Tree Format Tests

#### Empty File

```
Input Data:     (empty, 0 bytes)
Chunks:         1
Tree File Size: 48 bytes (header only)
Root Hash:      af1349b9f5f9a1a6a0404dea36dcc9499bcb25c9adc112b7cc9a93cae41f3262
```

Tree File Structure:
```
Offset  Hex                                       Description
------  ----------------------------------------  -----------
0-3     424b3454                                  Magic "BK4T"
4       01                                        Version 1
5       00                                        Flags
6-7     0000                                      Reserved
8-15    0000000000000000                          Data length (0)
16-47   af1349b9f5f9a1a6...                       Root hash
```

#### Single Chunk (512 bytes of 0x42)

```
Input Data:     0x42 repeated 512 times
Chunks:         1
Tree File Size: 48 bytes (header only)
Root Hash:      51aff54ef792166739c497610f624422a0995f0f35705cf3142c9c0177576ef1
```

#### Two Chunks (2048 bytes)

```
Input Data:     Pattern: byte[i] = i % 256
Data Length:    2048 bytes
Chunks:         2
Internal Nodes: 1
Tree File Size: 48 + 32 = 80 bytes
Root Hash:      9b12fe139c872130d9915468577a069c0519567c234d2db3718bca7ab5893ac7
```

### 2.2 Proof Format Tests

#### Proof for First Chunk of 4KB File

```
File Size:     4096 bytes
Slice Start:   0
Slice End:     1024
Proof Depth:   2

Proof Structure:
Offset  Size   Description
------  ----   -----------
0-3     4      Magic "BK4P"
4       1      Version (1)
5       1      Flags (0)
6-7     2      Reserved
8-39    32     Root hash
40-47   8      Total data length (4096)
48-55   8      Slice start (0)
56-63   8      Slice end (1024)
64      1      Depth (2)
65-96   32     Sibling hash 0 (sibling of chunk 0)
97-128  32     Sibling hash 1 (parent's sibling)
```

### 2.3 Verification Tests

#### Tampered Data Detection

```
Original Data:  2048 bytes, pattern i % 256
Original Hash:  (compute with reference implementation)

Tampered Data:  Same but byte[1000] ^= 0xFF
Expected:       Verification FAILS
```

#### Wrong Root Hash Detection

```
Data:           1024 bytes of 0x00
Correct Root:   (compute)
Wrong Root:     All 0xFF
Expected:       Verification FAILS
```

---

## 3. Serialization Test Vectors

### 3.1 Serialization Format

#### Fresh Hasher (After Init)

```
Serialized Size: 1172 bytes (minimum, no CV stack)

Header:
Offset  Hex           Description
------  ------------  -----------
0-3     424b3453      Magic "BK4S"
4       01            Version 1
5       00            Flags (unkeyed mode)
6-7     0000          Reserved
8-39    6a09e667...   CV (IV values)
40-71   6a09e667...   Key words (IV values)
72-79   00000000...   Chunk counter (0)
80      00            Block buffer length (0)
81-144  00000000...   Block buffer (zeros)
145-146 0000          Chunk buffer length (0)
147-1170 00000000...  Chunk buffer (zeros)
1171    00            CV stack length (0)
```

### 3.2 Round-Trip Tests

#### Partial Chunk State

```
Initial State:  blake4_hasher_init()
Update:         "hello world" (11 bytes)
Serialize:      (save state)
Deserialize:    (restore state)
Continue:       " and more" (9 bytes)
Finalize:       (should match hashing "hello world and more" directly)
```

#### Multi-Chunk State

```
Initial State:  blake4_hasher_init()
Update:         3000 bytes of pattern data
Serialize:      (save state - includes CV stack)
Deserialize:    (restore state)
Continue:       1000 more bytes
Finalize:       (should match hashing all 4000 bytes directly)
```

---

## 4. Edge Cases

### 4.1 Boundary Conditions

| Test | Input Size | Chunks | Notes |
|------|------------|--------|-------|
| Empty | 0 | 1 | Single empty chunk |
| 1 byte | 1 | 1 | Minimal non-empty |
| 63 bytes | 63 | 1 | Just under block size |
| 64 bytes | 64 | 1 | Exactly one block |
| 65 bytes | 65 | 1 | Just over block size |
| 1023 bytes | 1023 | 1 | Just under chunk size |
| 1024 bytes | 1024 | 1 | Exactly one chunk |
| 1025 bytes | 1025 | 2 | Just over chunk size |
| 2048 bytes | 2048 | 2 | Exactly two chunks |
| 16384 bytes | 16384 | 16 | Power of two chunks |
| 16385 bytes | 16385 | 17 | Power of two + 1 |

### 4.2 Invalid Input Handling

| Test | Input | Expected |
|------|-------|----------|
| NULL hasher | blake4_hash(NULL, 0, out) | No crash |
| NULL output | blake4_hash(data, len, NULL) | No crash |
| Invalid magic | Deserialize with "BAD!" magic | Return -1 |
| Truncated state | Deserialize with 100 bytes | Return -1 |
| Future version | Deserialize with version 99 | Return -1 |

---

## 5. Generating Test Vectors

### Reference Implementation Usage

```c
#include "blake4.h"
#include <stdio.h>

void print_test_vector(const char *name, const void *input, size_t len) {
    uint8_t hash[32];
    blake4_hash(input, len, hash);

    printf("Test: %s\n", name);
    printf("Input length: %zu\n", len);
    printf("Output: ");
    for (int i = 0; i < 32; i++) {
        printf("%02x", hash[i]);
    }
    printf("\n\n");
}

int main(void) {
    print_test_vector("empty", "", 0);

    uint8_t zero = 0;
    print_test_vector("single zero", &zero, 1);

    print_test_vector("abc", "abc", 3);

    uint8_t seq[250];
    for (int i = 0; i < 250; i++) seq[i] = i;
    print_test_vector("0-249 sequence", seq, 250);

    return 0;
}
```

### Cross-Validation with BLAKE3

Since BLAKE4-PORTABLE is algorithm-compatible with BLAKE3, implementations can be validated against:

- Official BLAKE3 test vectors: https://github.com/BLAKE3-team/BLAKE3/tree/master/test_vectors
- BLAKE3 reference implementation: https://github.com/BLAKE3-team/BLAKE3

---

## 6. Test Vector JSON Format

For automated testing, vectors can be provided in JSON:

```json
{
  "version": "1.0.0",
  "portable": [
    {
      "name": "empty",
      "input_hex": "",
      "output_hex": "af1349b9f5f9a1a6a0404dea36dcc9499bcb25c9adc112b7cc9a93cae41f3262"
    },
    {
      "name": "single_zero",
      "input_hex": "00",
      "output_hex": "2d3adedff11b61f14c886e35afa036736dcd87a74d27b5c1510225d0f592e213"
    }
  ],
  "keyed": [
    {
      "name": "zero_key_empty",
      "key_hex": "0000000000000000000000000000000000000000000000000000000000000000",
      "input_hex": "",
      "output_hex": "..."
    }
  ],
  "derive_key": [
    {
      "name": "test_context",
      "context": "BLAKE4 test",
      "key_material_hex": "00010203",
      "output_len": 32,
      "output_hex": "..."
    }
  ],
  "stream": [
    {
      "name": "empty_tree",
      "input_hex": "",
      "root_hash_hex": "af1349b9f5f9a1a6a0404dea36dcc9499bcb25c9adc112b7cc9a93cae41f3262",
      "tree_size": 48
    }
  ]
}
```

---

## Appendix: Computing Vectors with Reference Implementation

To generate authoritative test vectors, build and run:

```bash
cd blake4-exploration
mkdir build && cd build
cmake ..
make
./test_basic  # Validates known vectors
```

The reference implementation in `src/blake4.c` and `src/blake4_stream.c` produces the canonical outputs for all test cases.
