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
- **High Performance**: ~1.4 GB/s serial, ~9.7 GB/s parallel (8 threads)
- **SIMD Optimized**: Hand-tuned assembly for AVX-512, AVX2, and ARM NEON
- **Multi-threaded**: Parallel hashing API with automatic thread scaling
- **Hash-Based Signature Support**: Dedicated APIs for SPHINCS+, XMSS, LMS
- **Configurable Output**: BLAKE4-256, BLAKE4-384, BLAKE4-512 modes
- **Verified Streaming**: Merkle tree construction with slice proofs
- **Security Hardened**: Constant-time utilities, sanitizer support, fuzzing infrastructure
- **Language Bindings**: Python and Rust

## Quick Start

```bash
# Build
mkdir build && cd build
cmake ..
make

# Run tests (44 tests)
ctest --output-on-failure

# Run benchmarks
./benchmark
```

## Performance

Benchmark results on ARM64 (Apple Silicon, portable C implementation):

| Input Size | Serial | Parallel (8 threads) |
|------------|--------|---------------------|
| 1 KB | 1,374 MB/s | - |
| 1 MB | 1,421 MB/s | 4,600 MB/s |
| 16 MB | 1,426 MB/s | 9,670 MB/s |

x86-64 systems with AVX-512/AVX2 will see additional gains from hand-tuned assembly.

## Project Structure

```
blake4/
├── include/
│   ├── blake4.h           # Core hash API + HBS + parallel + constant-time
│   ├── blake4_dispatch.h  # SIMD dispatch interface
│   └── blake4_stream.h    # Verified streaming API
├── src/
│   ├── blake4.c           # Core implementation
│   ├── blake4_dispatch.c  # CPU detection and dispatch
│   ├── blake4_parallel.c  # Multi-threaded hashing
│   ├── blake4_ct.c        # Constant-time utilities
│   ├── blake4_thread.h    # Cross-platform threading
│   ├── blake4_simd.c      # SIMD implementations
│   ├── blake4_avx512.c    # AVX-512 intrinsics
│   ├── blake4_avx2.c      # AVX2 intrinsics
│   ├── blake4_neon.c      # ARM NEON intrinsics
│   ├── blake4_avx512_x86-64.S      # AVX-512 assembly (Unix)
│   ├── blake4_avx512_x86-64_windows.asm  # AVX-512 assembly (Windows)
│   ├── blake4_avx2_x86-64.S        # AVX2 assembly (Unix)
│   ├── blake4_avx2_x86-64_windows.asm    # AVX2 assembly (Windows)
│   ├── blake4_neon_arm64.S         # NEON assembly (ARM64)
│   └── blake4_stream.c    # Merkle tree streaming
├── test/
│   ├── test_blake4.c      # Core tests (44 tests)
│   └── test_stream.c      # Stream tests (21 tests)
├── fuzz/
│   ├── fuzz_hash.c        # Fuzz one-shot hashing
│   ├── fuzz_incremental.c # Fuzz streaming API
│   ├── fuzz_deserialize.c # Fuzz state deserialization
│   └── fuzz_keyed.c       # Fuzz keyed hashing
├── tools/
│   ├── benchmark.c        # Performance benchmarking
│   └── gen_vectors.c      # Test vector generator
├── bindings/
│   ├── python/            # Python ctypes bindings
│   └── rust/              # Rust FFI bindings
├── docs/
│   └── analysis/          # Security analysis documents
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

### Parallel Hashing

```c
// One-shot parallel hash (auto-detect threads)
uint8_t hash[64];
blake4_parallel_hash(large_data, data_len, hash, 0);

// Explicit thread count
blake4_parallel_hash(large_data, data_len, hash, 8);

// Incremental parallel hashing
blake4_parallel_hasher *ph = blake4_parallel_hasher_new(4);
blake4_parallel_hasher_init(ph);
blake4_parallel_hasher_update(ph, data, len);
blake4_parallel_hasher_finalize(ph, hash, 64);
blake4_parallel_hasher_free(ph);

// Check availability
if (blake4_parallel_available()) {
    // Threading support is available
}
```

### Constant-Time Utilities

```c
// Constant-time MAC comparison (prevents timing attacks)
if (blake4_ct_memcmp(computed_mac, expected_mac, 64) == 0) {
    // MACs match
}

// Secure memory zeroing (won't be optimized away)
blake4_ct_memzero(secret_key, 64);

// Constant-time conditional selection
uint64_t result = blake4_ct_select64(condition, value_if_false, value_if_true);
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

### Constant-Time Guarantees

The following operations are constant-time with respect to secret data:
- `blake4_ct_memcmp()` - Safe for comparing MACs
- `blake4_ct_memzero()` - Safe for clearing secrets
- `blake4_hasher_update()` - No secret-dependent branches
- `compress()` - No secret-dependent memory access

## Building

### Requirements

- CMake 3.14+
- C11 compiler (GCC, Clang, or MSVC)

### Options

```bash
cmake -DBLAKE4_BUILD_TESTS=ON           # Build tests (default: ON)
cmake -DBLAKE4_BUILD_SHARED=ON          # Build shared library (default: ON)
cmake -DBLAKE4_BUILD_STATIC=ON          # Build static library (default: ON)
cmake -DBLAKE4_ENABLE_SIMD=ON           # Enable SIMD (default: ON)
cmake -DBLAKE4_SANITIZE_ADDRESS=ON      # Enable AddressSanitizer
cmake -DBLAKE4_SANITIZE_UNDEFINED=ON    # Enable UBSan
cmake -DBLAKE4_SANITIZE_MEMORY=ON       # Enable MemorySanitizer (Clang only)
cmake -DBLAKE4_SANITIZE_THREAD=ON       # Enable ThreadSanitizer
cmake -DBLAKE4_BUILD_FUZZ=ON            # Build fuzz harnesses (requires LLVM Clang)
```

### Building with Sanitizers

```bash
mkdir build_asan && cd build_asan
cmake .. -DBLAKE4_SANITIZE_ADDRESS=ON -DBLAKE4_SANITIZE_UNDEFINED=ON
make
./test_blake4  # Run with sanitizers
```

### Building Fuzz Targets

Requires LLVM Clang (not Apple Clang):

```bash
# Install LLVM on macOS
brew install llvm

# Build fuzz targets
mkdir build_fuzz && cd build_fuzz
cmake .. -DBLAKE4_BUILD_FUZZ=ON -DCMAKE_C_COMPILER=/opt/homebrew/opt/llvm/bin/clang
make

# Run fuzzer
./fuzz_hash corpus_hash/
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
- [x] Hand-tuned assembly (AVX-512, AVX2, NEON)
- [x] Multi-threaded parallel hashing
- [x] Constant-time utilities
- [x] Fuzzing infrastructure
- [x] Sanitizer support
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
