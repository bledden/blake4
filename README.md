# BLAKE4

A cryptographic hash function with 512-bit internal state, designed for post-quantum security margins.

## Overview

BLAKE4 extends the BLAKE family with a larger state size, providing enhanced security margins for long-term cryptographic applications including post-quantum hash-based signatures.

| Parameter | Value | Post-Quantum Security |
|-----------|-------|----------------------|
| Internal state | 512-bit | 256-bit (Grover) |
| Default output | 64 bytes | 170-bit collision (BHT) |
| Block size | 128 bytes | - |
| Chunk size | 2048 bytes | - |
| Rounds | 10 | - |
| Key size | 64 bytes | - |

### Features

- **Post-Quantum Ready**: 512-bit state provides 256-bit security against Grover's algorithm
- **Hash-Based Signature Support**: Dedicated APIs for SPHINCS+, XMSS, LMS
- **Configurable Output**: BLAKE4-256, BLAKE4-384, BLAKE4-512 modes
- **Verified Streaming**: Merkle tree construction with slice proofs
- **SIMD Optimized**: AVX2, AVX-512, NEON support
- **Language Bindings**: Python and Rust

## Quick Start

```bash
# Build
mkdir build && cd build
cmake ..
make

# Run tests (56 tests)
ctest --output-on-failure
```

## Project Structure

```
blake4/
├── include/
│   ├── blake4.h           # Core hash API + HBS functions
│   ├── blake4_simd.h      # SIMD optimizations
│   └── blake4_stream.h    # Verified streaming API
├── src/
│   ├── blake4.c           # Core implementation
│   ├── blake4_simd.c      # SIMD implementations
│   └── blake4_stream.c    # Merkle tree streaming
├── test/
│   ├── test_blake4.c      # Core tests (35 tests)
│   └── test_stream.c      # Stream tests (21 tests)
├── bindings/
│   ├── python/            # Python ctypes bindings
│   └── rust/              # Rust FFI bindings
├── docs/
│   ├── analysis/          # Security analysis documents
│   │   ├── SECURITY_ANALYSIS.md
│   │   └── QUANTUM_ANALYSIS.md
│   └── design/            # Design documents
├── BLAKE4_SPEC.md         # Full specification
└── HANDOFF.md             # Development guide
```

## API Overview

### Basic Hashing

```c
#include "blake4.h"

// One-shot hash (64-byte output)
uint8_t hash[64];
blake4_hash("hello", 5, hash);

// Configurable output sizes
uint8_t hash256[32], hash384[48];
blake4_256_hash("hello", 5, hash256);  // 256-bit output
blake4_384_hash("hello", 5, hash384);  // 384-bit output

// Incremental hashing
blake4_hasher h;
blake4_hasher_init(&h);
blake4_hasher_update(&h, data1, len1);
blake4_hasher_update(&h, data2, len2);
blake4_hasher_finalize(&h, hash, 64);
```

### Hash-Based Signature Support

BLAKE4 includes optimized primitives for post-quantum signature schemes:

```c
// PRF for key generation (SPHINCS+, XMSS)
blake4_hbs_prf(secret_key, address, output, 64);

// Chain function for WOTS+
blake4_hbs_f(pub_seed, addr, input, output);

// Tree hash for Merkle trees
blake4_hbs_h(pub_seed, addr, left, right, output);

// Message hash for SPHINCS+
blake4_hbs_h_msg(randomizer, pub_seed, pub_root, message, msg_len, output, 64);
```

### Keyed Hashing (MAC)

```c
uint8_t key[64], mac[64];
blake4_hash_keyed(key, message, msg_len, mac);
```

### Key Derivation

```c
uint8_t derived[64];
blake4_derive_key("my-app context", key_material, len, derived, 64);
```

### XOF Mode

```c
uint8_t output[256];
blake4_hash_xof("input", 5, output, 256);
```

### State Serialization

```c
// Save hasher state mid-stream
uint8_t state[BLAKE4_SERIALIZED_SIZE];
blake4_hasher_serialize(&h, state, sizeof(state));

// Restore later
blake4_hasher h2;
blake4_hasher_deserialize(&h2, state, sizeof(state));
```

## Language Bindings

### Python

```python
import blake4

h = blake4.hash(b"hello world")
print(h.hex())

# Incremental
hasher = blake4.Hasher()
hasher.update(b"hello ")
hasher.update(b"world")
h = hasher.finalize()
```

### Rust

```rust
let hash = blake4::hash(b"hello world");

// Incremental
let mut hasher = blake4::Hasher::new();
hasher.update(b"hello ");
hasher.update(b"world");
let hash = hasher.finalize();
```

## Test Vectors

| Input | Output (first 64 chars) |
|-------|------------------------|
| (empty) | `741c4ffdf535f7a77843b052bc106f109f1ae51895173c10ae9eb0f1...` |
| `"abc"` | `0585c5c0e59ba7f064bb89065e60a2a41a96cd8ff5490a4b2f0a02ee...` |

See [BLAKE4_SPEC.md](BLAKE4_SPEC.md) for complete test vectors.

## Security Analysis

BLAKE4 includes formal security analysis documentation:

- **[Security Analysis](docs/analysis/SECURITY_ANALYSIS.md)**: Formal security claims, differential cryptanalysis bounds, quantum attack analysis
- **[Quantum Analysis](docs/analysis/QUANTUM_ANALYSIS.md)**: Questions for quantum computing verification

### Security Claims

| Property | Classical | Post-Quantum |
|----------|-----------|--------------|
| Preimage | 512 bits | 256 bits (Grover) |
| Collision | 256 bits | ~170 bits (BHT) |
| PRF Security | 512 bits | 256 bits |

## Building

### Requirements

- CMake 3.14+
- C11 compiler (GCC, Clang, or MSVC)

### Options

```bash
cmake -DBLAKE4_BUILD_TESTS=ON     # Build tests (default: ON)
cmake -DBLAKE4_BUILD_SHARED=ON    # Build shared library (default: ON)
cmake -DBLAKE4_BUILD_STATIC=ON    # Build static library (default: ON)
cmake -DBLAKE4_ENABLE_SIMD=ON     # Enable SIMD (default: ON)
```

### Installation

```bash
cmake --install build --prefix /usr/local
```

## Status

**Research/Exploration Project** - Not audited for production use.

- [x] BLAKE4 512-bit state implementation
- [x] Hash-based signature APIs
- [x] SIMD optimizations (AVX2/AVX-512/NEON)
- [x] Verified streaming (BLAKE4-STREAM)
- [x] Python and Rust bindings
- [x] Security analysis documentation
- [ ] External security audit
- [ ] Formal verification

## License

This is free and unencumbered software released into the public domain.

## References

- [BLAKE4_SPEC.md](BLAKE4_SPEC.md) - Full specification
- [HANDOFF.md](HANDOFF.md) - Development guide
- [Security Analysis](docs/analysis/SECURITY_ANALYSIS.md)
- [BLAKE3](https://github.com/BLAKE3-team/BLAKE3)
- [SPHINCS+](https://sphincs.org/)
