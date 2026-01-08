# BLAKE4

A cryptographic hash function with 512-bit internal state, extending the BLAKE family.

## Overview

BLAKE4 is a hash function with a 512-bit internal state, providing larger security margins compared to 256-bit designs. Key parameters:

| Parameter | Value |
|-----------|-------|
| Internal state | 512-bit (64-bit words) |
| Block size | 128 bytes |
| Chunk size | 2048 bytes |
| Rounds | 10 |
| Default output | 64 bytes |
| Key size | 64 bytes |

Features:
- **Stable C ABI**: Versioned, stable shared library with semantic versioning
- **Portable builds**: First-class `NO_ASM` support for pure C builds
- **Verified streaming**: Standardized Merkle proof format (BLAKE4-STREAM)

## Quick Start

```bash
# Build
mkdir build && cd build
cmake ..
make

# Run tests
ctest --output-on-failure
```

## Project Structure

```
blake4-exploration/
├── include/
│   ├── blake4.h           # Core hash API
│   └── blake4_stream.h    # Verified streaming API
├── src/
│   ├── blake4.c           # Reference implementation
│   └── blake4_stream.c    # Merkle tree streaming
├── test/
│   ├── test_blake4.c      # Core functionality tests
│   └── test_stream.c      # Streaming tests
├── tools/
│   └── gen_vectors.c      # Test vector generator
├── BLAKE4_SPEC.md         # Full BLAKE4 specification
└── test_vectors.md        # Interoperability test vectors
```

## API Overview

### Basic Hashing

```c
#include "blake4.h"

// One-shot hash (64-byte output)
uint8_t hash[64];
blake4_hash("hello", 5, hash);

// Incremental hashing
blake4_hasher h;
blake4_hasher_init(&h);
blake4_hasher_update(&h, data1, len1);
blake4_hasher_update(&h, data2, len2);
blake4_hasher_finalize(&h, hash, 64);
```

### Keyed Hashing (MAC)

```c
uint8_t key[64] = {...};  // 64-byte key
uint8_t mac[64];
blake4_hash_keyed(key, message, msg_len, mac);
```

### Key Derivation

```c
uint8_t derived_key[64];
blake4_derive_key("my app v1", key_material, km_len, derived_key, 64);
```

### XOF Mode (Extendable Output)

```c
// Generate arbitrary-length output
uint8_t output[256];
blake4_hash_xof("input", 5, output, 256);
```

### Verified Streaming

```c
#include "blake4_stream.h"

// Encode file with Merkle tree
uint8_t *tree;
size_t tree_len;
uint8_t root_hash[64];
blake4_stream_encode(file_data, file_len, &tree, &tree_len, root_hash);

// Verify file against root hash
int result = blake4_stream_verify(root_hash, tree, tree_len, file_data, file_len);
if (result == BLAKE4_STREAM_OK) {
    // File verified
}

// Extract proof for a byte range
uint8_t proof[512];
size_t proof_len = sizeof(proof);
blake4_stream_extract_slice(tree, tree_len, file_data, file_len,
                            start, end, proof, &proof_len);
```

## Test Vectors

BLAKE4 outputs (64 bytes / 512 bits):

| Input | Output (hex, first 32 bytes shown) |
|-------|-------------------------------------|
| (empty) | `741c4ffdf535f7a77843b052bc106f10...` |
| `"abc"` | `0585c5c0e59ba7f064bb89065e60a2a4...` |

See [BLAKE4_SPEC.md](BLAKE4_SPEC.md) for complete test vectors.

## Building

### Requirements

- CMake 3.14+
- C11 compiler (GCC, Clang, or MSVC)

### Options

```bash
cmake -DBLAKE4_BUILD_TESTS=ON    # Build tests (default: ON)
cmake -DBLAKE4_BUILD_SHARED=ON   # Build shared library (default: ON)
cmake -DBLAKE4_BUILD_STATIC=ON   # Build static library (default: ON)
```

### Installation

```bash
cmake --install build --prefix /usr/local
```

This installs:
- Headers to `include/`
- Libraries to `lib/`
- CMake config for `find_package(blake4)`
- pkg-config file

## Status

This is an **exploration project** implementing ideas for a BLAKE4 specification. It is not production-ready and should not be used for security-critical applications.

Current status:
- [x] BLAKE4 512-bit state specification
- [x] Reference implementation
- [x] BLAKE4-STREAM verified streaming
- [x] Test vectors
- [ ] Security audit
- [ ] Performance optimization
- [ ] Additional language bindings

## License

This is free and unencumbered software released into the public domain.

## Acknowledgments

- BLAKE family designers (Aumasson et al.)
- Bao project for verified streaming concepts

## References

- [BLAKE4_SPEC.md](BLAKE4_SPEC.md) - Full BLAKE4 specification
- [Bao Verified Streaming](https://github.com/oconnor663/bao)
