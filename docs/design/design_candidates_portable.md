# BLAKE4-PORTABLE Design Candidates

> Phase 2: Concrete design options for the PORTABLE flagship.

---

## Design Goals Recap

1. Stable, versioned C API/ABI
2. First-class NO_ASM build path
3. Improved short-input performance
4. pkg-config and CMake support
5. Consistent cross-compiler performance

---

## Candidate A: BLAKE3 Core + Packaging Improvements Only

**Philosophy:** Minimal changes to algorithm. Focus entirely on build/API improvements.

### Algorithm
- Identical to BLAKE3 (7 rounds, 1024-byte chunks, same compression)
- Same output for same inputs as BLAKE3

### API Changes
```c
// Version info
#define BLAKE4_VERSION_MAJOR 1
#define BLAKE4_VERSION_MINOR 0
#define BLAKE4_VERSION_PATCH 0
const char* blake4_version_string(void);

// Standard API (same shape as BLAKE3)
void blake4_hasher_init(blake4_hasher *self);
void blake4_hasher_update(blake4_hasher *self, const void *input, size_t input_len);
void blake4_hasher_finalize(const blake4_hasher *self, uint8_t *out, size_t out_len);

// New: State serialization
size_t blake4_hasher_serialize(const blake4_hasher *self, uint8_t *buf, size_t buf_len);
int blake4_hasher_deserialize(blake4_hasher *self, const uint8_t *buf, size_t buf_len);
```

### Build System
```cmake
# CMakeLists.txt
option(BLAKE4_NO_ASM "Disable assembly implementations" OFF)
option(BLAKE4_NO_SIMD "Disable all SIMD (pure portable C)" OFF)
option(BLAKE4_SHARED "Build shared library" ON)
option(BLAKE4_STATIC "Build static library" ON)

# Generates blake4.pc for pkg-config
# Exports blake4::blake4 for find_package
```

### Pros
- Zero cryptographic risk (identical to BLAKE3)
- Easiest migration path
- Can ship quickly

### Cons
- Doesn't address short-input performance
- "Why not just improve BLAKE3?" criticism
- No real differentiation

---

## Candidate B: Short-Input Optimization

**Philosophy:** Same algorithm, but optimize the common case of small inputs.

### Algorithm Changes
- For inputs ≤ 64 bytes (single block): bypass tree construction entirely
- Direct single-block compression to output
- Larger inputs: same as BLAKE3

### Implementation
```c
void blake4_hasher_finalize(const blake4_hasher *self, uint8_t *out, size_t out_len) {
    if (self->total_len <= BLAKE4_BLOCK_LEN && self->chunk_count == 0) {
        // Fast path: single block, no tree overhead
        blake4_compress_single(self->cv, self->buf, self->buf_len,
                               BLAKE4_CHUNK_START | BLAKE4_CHUNK_END | BLAKE4_ROOT,
                               out, out_len);
    } else {
        // Standard tree finalization
        blake4_finalize_tree(self, out, out_len);
    }
}
```

### Performance Target
- Within 1.5x of BLAKE2s for inputs < 64 bytes
- Match or exceed BLAKE3 for inputs > 1KB

### Pros
- Addresses documented weakness
- Backward compatible for large inputs
- Measurable improvement

### Cons
- Two code paths = more complexity
- Still uses same round count (no security margin change)
- Minor output difference for edge cases at block boundary

---

## Candidate C: Reduced Initialization Overhead

**Philosophy:** Optimize the init/update/finalize cycle for typical use patterns.

### Changes
- Pre-computed IV in static memory (no runtime init)
- Lazy chunk allocation (don't allocate tree state until needed)
- Stack-friendly hasher size for small inputs

### Implementation
```c
typedef struct {
    uint32_t cv[8];           // Current chaining value
    uint8_t buf[64];          // Block buffer
    uint8_t buf_len;          // Bytes in buffer
    uint8_t flags;            // Mode flags
    uint16_t _reserved;
    // Tree state only allocated on heap if input > 1 chunk
    blake4_tree_state *tree;  // NULL for small inputs
} blake4_hasher;  // 80 bytes vs BLAKE3's 1912 bytes for small inputs
```

### Pros
- Dramatically smaller hasher for common case
- Cache-friendly
- Faster init/finalize cycle

### Cons
- More complex memory management
- Heap allocation for large inputs
- API difference (may need blake4_hasher_destroy for cleanup)

---

## Candidate D: Increased Security Margin

**Philosophy:** More conservative parameters for users who want extra margin.

### Changes
- 10 rounds instead of 7 (matches BLAKE2)
- Optional 512-bit output with larger internal state
- Same tree structure

### Performance Impact
- ~30% slower than BLAKE3 (10/7 ≈ 1.43x more work)
- Still faster than SHA-256, SHA-3

### API
```c
// Default: 256-bit, 10 rounds
void blake4_hasher_init(blake4_hasher *self);

// Explicit 512-bit mode
void blake4_hasher_init_512(blake4_hasher *self);
```

### Pros
- Clear security story
- Addresses round-count criticism
- Conservative choice for compliance-minded users

### Cons
- Performance regression vs BLAKE3
- May not satisfy those who think 7 is already sufficient
- Internal state changes complicate implementation

---

## Candidate E: Hybrid (Recommended)

**Philosophy:** Combine the best of B, C, and packaging from A.

### Algorithm
- 7 rounds (proven sufficient, maintains speed)
- Short-input fast path (Candidate B)
- Reduced hasher size for small inputs (Candidate C concept, but simpler)

### Concrete Design
```c
// Compact hasher - always stack-allocated, no heap
typedef struct {
    uint32_t cv[8];              // 32 bytes
    uint64_t counter;            // 8 bytes
    uint8_t buf[64];             // 64 bytes (one block)
    uint8_t buf_len;             // 1 byte
    uint8_t flags;               // 1 byte
    uint8_t chunk_buf[1024];     // Current chunk
    uint16_t chunk_len;          // 2 bytes
    // Tree state embedded, not heap-allocated
    uint32_t cv_stack[8 * 54];   // 1728 bytes (max tree depth)
    uint8_t cv_stack_len;        // 1 byte
} blake4_hasher;  // ~1900 bytes total, all on stack
```

### Short-Input Path
```c
// Inputs ≤ 1024 bytes: single chunk, no tree
// Inputs ≤ 64 bytes: single block, minimal overhead
```

### API (Full)
```c
// === Version ===
#define BLAKE4_VERSION_MAJOR 1
#define BLAKE4_VERSION_MINOR 0
#define BLAKE4_VERSION_PATCH 0
#define BLAKE4_OUT_LEN 32
#define BLAKE4_KEY_LEN 32

const char* blake4_version_string(void);
int blake4_version_major(void);
int blake4_version_minor(void);
int blake4_version_patch(void);

// === Core Hasher ===
typedef struct blake4_hasher blake4_hasher;

void blake4_hasher_init(blake4_hasher *self);
void blake4_hasher_init_keyed(blake4_hasher *self, const uint8_t key[BLAKE4_KEY_LEN]);
void blake4_hasher_init_derive_key(blake4_hasher *self, const char *context);
void blake4_hasher_init_derive_key_raw(blake4_hasher *self, const void *context, size_t context_len);

void blake4_hasher_update(blake4_hasher *self, const void *input, size_t input_len);
void blake4_hasher_finalize(const blake4_hasher *self, uint8_t *out, size_t out_len);
void blake4_hasher_finalize_seek(const blake4_hasher *self, uint64_t seek, uint8_t *out, size_t out_len);
void blake4_hasher_reset(blake4_hasher *self);

// === State Serialization (NEW) ===
#define BLAKE4_STATE_SIZE 2048  // Max serialized state size

size_t blake4_hasher_state_size(const blake4_hasher *self);
int blake4_hasher_serialize(const blake4_hasher *self, uint8_t *buf, size_t buf_len);
int blake4_hasher_deserialize(blake4_hasher *self, const uint8_t *buf, size_t buf_len);

// === Convenience ===
void blake4_hash(const void *input, size_t input_len, uint8_t *out, size_t out_len);
void blake4_hash_keyed(const uint8_t key[BLAKE4_KEY_LEN], const void *input, size_t input_len, uint8_t *out, size_t out_len);

// === 512-bit Mode (Optional Extension) ===
void blake4_hasher_init_512(blake4_hasher *self);
void blake4_hash_512(const void *input, size_t input_len, uint8_t out[64]);
```

### Build System
```cmake
cmake_minimum_required(VERSION 3.14)
project(blake4 VERSION 1.0.0 LANGUAGES C)

option(BLAKE4_NO_ASM "Disable assembly, use C intrinsics only" OFF)
option(BLAKE4_NO_SIMD "Disable all SIMD, pure portable C" OFF)
option(BLAKE4_BUILD_SHARED "Build shared library" ON)
option(BLAKE4_BUILD_STATIC "Build static library" ON)
option(BLAKE4_BUILD_TESTS "Build test suite" ON)
option(BLAKE4_BUILD_BENCH "Build benchmarks" OFF)

# Auto-detect architecture and select implementations
include(cmake/DetectArch.cmake)
include(cmake/DetectSIMD.cmake)

# Library targets
add_library(blake4 ...)
add_library(blake4::blake4 ALIAS blake4)

# Install with pkg-config and CMake config
include(GNUInstallDirs)
install(TARGETS blake4 EXPORT blake4-targets ...)
install(EXPORT blake4-targets NAMESPACE blake4:: ...)

configure_file(blake4.pc.in blake4.pc @ONLY)
install(FILES ${CMAKE_BINARY_DIR}/blake4.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
```

### Pros
- Addresses all documented pain points
- No security regression (same rounds as BLAKE3)
- Short-input optimization
- Clean API with state serialization
- Modern build system

### Cons
- More implementation work than Candidate A
- Slight divergence from BLAKE3 (short-input path)
- Need thorough testing of edge cases

---

## Recommendation

**Candidate E (Hybrid)** - It addresses the real problems without introducing unnecessary risk.

### Implementation Order
1. Core compression function (portable C)
2. Hasher state management
3. Short-input fast path
4. State serialization
5. SIMD implementations (SSE2, SSE4.1, AVX2, AVX-512, NEON)
6. Build system (CMake + pkg-config)
7. Test suite
8. Benchmarks

---

## Validation Plan

### Correctness
- Generate test vectors for all input sizes 0-10KB
- Compare against BLAKE3 for inputs > 1024 bytes (should match for 256-bit output)
- Fuzz with AFL/libFuzzer
- Differential testing against BLAKE3 reference

### Performance
- Benchmark suite: 1B, 64B, 256B, 1KB, 4KB, 64KB, 1MB, 1GB
- Compare against BLAKE3, BLAKE2s, BLAKE2b, SHA-256
- Test on: x86_64 (Intel/AMD), ARM64 (Apple Silicon, AWS Graviton)
- Compiler matrix: GCC 11+, Clang 14+, MSVC 2022

### Build System
- CI for Linux (Ubuntu), macOS (Intel + ARM), Windows
- Test NO_ASM and NO_SIMD builds
- Verify pkg-config and find_package work correctly
- Test static and shared library builds

---

## Next Steps

1. [ ] Write `spec_portable.md` with normative algorithm description
2. [ ] Implement reference C code (portable, no SIMD)
3. [ ] Generate initial test vectors
4. [ ] Benchmark short-input performance vs BLAKE2s/BLAKE3
5. [ ] Implement state serialization format
6. [ ] Add SIMD implementations
7. [ ] Build system integration
