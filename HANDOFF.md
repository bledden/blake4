# BLAKE4 Project Handoff Document

**Date:** January 8, 2026
**Status:** Post-quantum ready with HBS APIs, security analysis, and language bindings

---

## Project Overview

BLAKE4 is a cryptographic hash function with a **512-bit internal state**, extending the BLAKE family with larger security margins compared to BLAKE3's 256-bit state. This version includes **post-quantum hash-based signature (HBS) APIs** and formal security analysis.

### Key Parameters

| Parameter | BLAKE3 | BLAKE4 |
|-----------|--------|--------|
| Word size | 32-bit | 64-bit |
| Internal state | 256-bit | 512-bit |
| Block size | 64 bytes | 128 bytes |
| Chunk size | 1024 bytes | 2048 bytes |
| Rounds | 7 | 10 |
| Default output | 32 bytes | 64 bytes |
| Key size | 32 bytes | 64 bytes |

### Security Levels

| Mode | Output | Classical Collision | Quantum Collision (BHT) |
|------|--------|--------------------|-----------------------|
| BLAKE4-256 | 32 bytes | 128-bit | ~85-bit |
| BLAKE4-384 | 48 bytes | 192-bit | ~128-bit |
| BLAKE4-512 | 64 bytes | 256-bit | ~170-bit |

---

## Repository Structure

```
blake4-exploration/
├── include/
│   ├── blake4.h              # Core hash API (512-bit) + HBS APIs
│   ├── blake4_simd.h         # SIMD optimizations header
│   └── blake4_stream.h       # Verified streaming API
├── src/
│   ├── blake4.c              # Main implementation + HBS functions
│   ├── blake4_simd.c         # SIMD implementations (AVX2/AVX-512/NEON)
│   └── blake4_stream.c       # Merkle tree streaming
├── test/
│   ├── test_blake4.c         # Core tests (35 tests)
│   └── test_stream.c         # Stream tests (21 tests)
├── bindings/
│   ├── python/
│   │   └── blake4.py         # Python ctypes bindings
│   └── rust/
│       ├── Cargo.toml
│       ├── build.rs
│       └── src/lib.rs        # Rust FFI bindings
├── tools/
│   └── gen_vectors.c         # Test vector generator
├── cmake/
│   ├── blake4-config.cmake.in
│   └── blake4.pc.in
├── BLAKE4_SPEC.md            # Full specification with test vectors
├── SECURITY_ANALYSIS.md      # Formal cryptographic security analysis
├── QUANTUM_ANALYSIS.md       # Questions for quantum computing analysis
├── README.md                 # Project documentation
└── CMakeLists.txt            # Build configuration (v2.0.0)
```

---

## Current State

### What's Done

1. **Core BLAKE4 Implementation** (`src/blake4.c`)
   - 64-bit word operations
   - SHA-512 IV constants (fractional parts of sqrt of first 8 primes)
   - BLAKE2b-style message schedule (10 permutations)
   - 10 rounds with rotation constants R1=32, R2=24, R3=16, R4=63
   - G function (quarter-round mixing)
   - Merkle tree hashing with correct merge logic
   - All modes: hash, keyed hash, key derivation, XOF
   - State serialization/deserialization

2. **Hash-Based Signature APIs** (`src/blake4.c`) - **NEW**
   - `blake4_hbs_prf`: PRF(SK.PRF, ADRS) for SPHINCS+/XMSS key generation
   - `blake4_hbs_prf_msg`: PRFmsg for message randomization
   - `blake4_hbs_f`: Chain function for WOTS+
   - `blake4_hbs_h`: Tree hash for Merkle internal nodes
   - `blake4_hbs_t`: Tweakable hash for WOTS+ public key compression
   - `blake4_hbs_h_msg`: Message hash for SPHINCS+

3. **Configurable Output Modes** - **NEW**
   - `blake4_256_hash`: 256-bit output (NIST Level 1)
   - `blake4_384_hash`: 384-bit output (NIST Level 3)
   - `blake4_512_hash`: 512-bit output (NIST Level 5)

4. **BLAKE4-STREAM** (`src/blake4_stream.c`)
   - Updated for 2048-byte chunks
   - 64-byte hash outputs
   - Tree header: 80 bytes
   - Proof header: 96 bytes
   - Full Merkle path verification for slice proofs

5. **SIMD Optimizations** (`src/blake4_simd.c`)
   - AVX2 support for x86-64
   - AVX-512 support for x86-64
   - NEON support for ARM64
   - Runtime detection via `blake4_simd_name()` and `blake4_simd_available()`

6. **Language Bindings**
   - **Python** (`bindings/python/blake4.py`): ctypes-based bindings
   - **Rust** (`bindings/rust/`): Native FFI bindings with idiomatic API

7. **Security Documentation** - **NEW**
   - `SECURITY_ANALYSIS.md`: Formal security analysis with:
     - Precise security claims and bounds
     - Differential cryptanalysis estimates
     - Round number justification
     - Quantum attack analysis (Grover, BHT)
     - QROM considerations
     - Comparison with SHA-3-512 and BLAKE3
   - `QUANTUM_ANALYSIS.md`: Questions for quantum computing analysis

8. **Test Suites**
   - `test_blake4.c`: 35 tests covering all functionality
   - `test_stream.c`: 21 tests for streaming/Merkle operations
   - All 56 tests passing

### Test Vectors (Canonical)

```
Empty input:
741c4ffdf535f7a77843b052bc106f109f1ae51895173c10ae9eb0f1d3e7b147
1a1791b0ac27f0d63ba391ed31734691b8311b8502c0eddf7cc60e0b89f8e11e

"abc":
0585c5c0e59ba7f064bb89065e60a2a41a96cd8ff5490a4b2f0a02ee8a4d71b6
2ab9283c03114dfee37b63fd0622a41f889d9ac8845317080cb7c95a75673377

128 bytes (0x00-0x7F):
21484e45ee40be624ac835126b6ecf1d5606890e49ddd1e9de3d41964bfddcd1
507fceea9bf3343a090b5d3678f22040fcbca8d00d2191942af88a48e3c790de
```

---

## Build & Test

```bash
cd /Users/bledden/Documents/blake4-exploration
rm -rf build && mkdir build && cd build
cmake ..
make
ctest --output-on-failure
```

Expected output:
```
100% tests passed, 0 tests failed out of 2
- blake4: 35/35 tests passed
- stream: 21/21 tests passed
```

---

## Post-Quantum Features

### Hash-Based Signature Support

BLAKE4 includes dedicated APIs for post-quantum hash-based signature schemes:

```c
// PRF for key generation (SPHINCS+, XMSS)
uint8_t out[64];
blake4_hbs_prf(secret_key, address, out, 64);

// Chain function for WOTS+
blake4_hbs_f(pub_seed, addr, input, output);

// Tree hash for Merkle trees
blake4_hbs_h(pub_seed, addr, left_child, right_child, output);

// Message hash for SPHINCS+
blake4_hbs_h_msg(randomizer, pub_seed, pub_root, message, msg_len, output, 64);
```

### Security Margins

| Attack | Classical | Quantum |
|--------|-----------|---------|
| Preimage | 2^512 | 2^256 (Grover) |
| Collision | 2^256 | 2^170 (BHT) |

The 512-bit state provides **256-bit post-quantum preimage security**, matching AES-256's quantum security level.

---

## API Quick Reference

### Basic Hashing
```c
uint8_t hash[64];
blake4_hash("data", 4, hash);  // 64-byte output

// Or with specific output size
uint8_t hash256[32], hash384[48];
blake4_256_hash("data", 4, hash256);  // 32-byte output
blake4_384_hash("data", 4, hash384);  // 48-byte output
```

### Keyed Hashing
```c
uint8_t key[64], mac[64];
blake4_hash_keyed(key, "data", 4, mac);
```

### Key Derivation
```c
uint8_t derived[64];
blake4_derive_key("context", material, len, derived, 64);
```

### XOF Mode
```c
uint8_t output[256];
blake4_hash_xof("data", 4, output, 256);
```

### Incremental
```c
blake4_hasher h;
blake4_hasher_init(&h);
blake4_hasher_update(&h, data1, len1);
blake4_hasher_update(&h, data2, len2);
blake4_hasher_finalize(&h, hash, 64);
```

### State Serialization
```c
blake4_hasher h;
blake4_hasher_init(&h);
blake4_hasher_update(&h, partial_data, len);

// Save state
uint8_t state_buf[BLAKE4_SERIALIZED_SIZE];
blake4_hasher_serialize(&h, state_buf, sizeof(state_buf));

// Later: restore state
blake4_hasher h2;
blake4_hasher_deserialize(&h2, state_buf, sizeof(state_buf));
blake4_hasher_update(&h2, more_data, more_len);
blake4_hasher_finalize(&h2, hash, 64);
```

### Python Bindings
```python
import blake4

# Simple hash
h = blake4.hash(b"hello world")
print(h.hex())

# Incremental
hasher = blake4.Hasher()
hasher.update(b"hello ")
hasher.update(b"world")
h = hasher.finalize()
```

### Rust Bindings
```rust
// Simple hash
let hash = blake4::hash(b"hello world");

// Incremental
let mut hasher = blake4::Hasher::new();
hasher.update(b"hello ");
hasher.update(b"world");
let hash = hasher.finalize();
```

---

## Remaining Work

### For Formal Submission

1. **Quantum Computing Analysis** - Use `QUANTUM_ANALYSIS.md` questions with quantum computing resources

2. **External Security Audit** - Independent cryptanalysis by domain experts

3. **Formal Verification** - Prove implementation correctness

### Additional Optimizations

1. Pure assembly implementations for highest performance
2. Parallel chunk processing (multi-threaded)
3. SIMD integration into main hasher path

### Additional Language Bindings

- Go
- JavaScript/WASM
- Others

---

## Key Files

| File | Purpose |
|------|---------|
| `src/blake4.c` | Core implementation + HBS APIs |
| `include/blake4.h` | Public API header |
| `SECURITY_ANALYSIS.md` | Formal security claims and analysis |
| `QUANTUM_ANALYSIS.md` | Questions for quantum analysis |
| `BLAKE4_SPEC.md` | Full specification with test vectors |

---

## References

- **Specification:** [BLAKE4_SPEC.md](BLAKE4_SPEC.md)
- **Security Analysis:** [SECURITY_ANALYSIS.md](SECURITY_ANALYSIS.md)
- **Quantum Analysis:** [QUANTUM_ANALYSIS.md](QUANTUM_ANALYSIS.md)
- **BLAKE2 RFC:** RFC 7693
- **BLAKE3:** https://github.com/BLAKE3-team/BLAKE3
- **SPHINCS+:** https://sphincs.org/
- **SHA-512 IV:** FIPS 180-4

---

*Last updated: January 8, 2026*
