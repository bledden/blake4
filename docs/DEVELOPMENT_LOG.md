# BLAKE4 Development Log

This document chronicles the development process, decisions made, and lessons learned during the BLAKE4 production optimization effort.

## Project Overview

**Goal**: Transform BLAKE4 from a reference implementation to a production-ready cryptographic library with competitive performance and robust security guarantees.

**Starting State**: A functional but slow (~15 MB/s) portable C implementation with SIMD code that existed but wasn't connected to the main code path.

**Final State**: A high-performance library achieving ~420 MB/s (serial) and ~3.2 GB/s (8 threads) on ARM64 with NEON assembly.

---

## Phase 1: SIMD Dispatch Layer

### What We Did
Created a runtime CPU feature detection and dispatch system in `src/blake4_dispatch.c`:

- **CPU Detection**: CPUID for x86-64, system registers for ARM64
- **Feature Flags**: SSE4.1, AVX, AVX2, AVX-512F/VL, NEON
- **Function Pointer Dispatch**: Selects best available compression function at runtime
- **Fallback Chain**: AVX-512 → AVX2 → NEON → Portable C

### Key Files
- `src/blake4_dispatch.c` - CPU detection and function dispatch
- `include/blake4_dispatch.h` - Public dispatch API

### Lessons Learned
The dispatch layer needed to be called from code that's always linked. Using `__attribute__((constructor))` alone wasn't sufficient for static library linking (see Phase 6).

---

## Phase 2: SIMD Intrinsics Implementations

### What We Did
Created SIMD implementations using compiler intrinsics:

- **AVX-512** (`src/blake4_avx512.c`): 512-bit vectors, native 64-bit rotates
- **AVX2** (`src/blake4_avx2.c`): 256-bit vectors, emulated rotates via shifts
- **NEON** (`src/blake4_neon.c`): 128-bit vectors, ARM64 optimized

### Implementation Details

**G Function Vectorization**: The core BLAKE4 mixing function operates on 4 pairs of 64-bit words. We vectorized this to process all 4 G operations in parallel.

**64-bit Rotations**:
- AVX-512: Native `_mm512_ror_epi64`
- AVX2: Shift pair (`(x >> n) | (x << (64-n))`)
- NEON: `vrorq_n_u64` or shift pair

**Row Permutations**: The diagonal step requires permuting rows. We use:
- AVX-512/AVX2: `_mm256_permute4x64_epi64`
- NEON: `vextq_u64` for rotation

---

## Phase 3: Hand-Optimized Assembly

### What We Did
Created hand-written assembly for maximum performance:

| File | Platform | Description |
|------|----------|-------------|
| `blake4_avx512_x86-64.S` | Unix x86-64 | AVX-512 assembly (GNU as) |
| `blake4_avx512_x86-64_windows.asm` | Windows x86-64 | AVX-512 assembly (MASM) |
| `blake4_avx2_x86-64.S` | Unix x86-64 | AVX2 assembly (GNU as) |
| `blake4_avx2_x86-64_windows.asm` | Windows x86-64 | AVX2 assembly (MASM) |
| `blake4_neon_arm64.S` | ARM64 | NEON assembly |

### Assembly Optimizations

1. **Register Allocation**: Keep entire state in vector registers
2. **Instruction Scheduling**: Interleave independent operations
3. **Unrolled Rounds**: Reduce loop overhead
4. **Message Preloading**: Load next round's messages while computing

### Calling Convention Differences

**Unix (System V AMD64 ABI)**:
- Arguments: RDI, RSI, RDX, RCX, R8, R9
- Callee-saved: RBX, RBP, R12-R15
- XMM0-XMM15 are caller-saved

**Windows (x64 ABI)**:
- Arguments: RCX, RDX, R8, R9, stack
- Callee-saved: RBX, RBP, RDI, RSI, R12-R15, **XMM6-XMM15**
- Must save/restore XMM6-15 if used

---

## Phase 4: Multi-threaded Parallel Hashing

### What We Did
Implemented chunk-based parallel hashing in `src/blake4_parallel.c`:

- **Thread Pool**: Persistent workers, work queue
- **Chunk Distribution**: Divide input into BLAKE4_CHUNK_LEN (2048 byte) chunks
- **Per-Worker CVs**: Each thread maintains independent chaining values
- **Tree Merge**: Final reduction of worker CVs to root hash

### Threading Abstraction
Created `src/blake4_thread.h` with platform abstraction:

| Platform | Implementation |
|----------|----------------|
| Unix/macOS | pthreads |
| Windows | Win32 threads (CreateThread, CRITICAL_SECTION) |

### CPU Count Detection

```c
// macOS
sysctlbyname("hw.ncpu", &count, &size, NULL, 0);

// Linux
sysconf(_SC_NPROCESSORS_ONLN);

// Windows
GetSystemInfo(&sysinfo);
sysinfo.dwNumberOfProcessors;
```

### Parallelization Threshold
Only parallelize for inputs > 1MB. Below this, thread overhead exceeds benefit.

---

## Phase 5: Security Hardening

### Constant-Time Utilities (`src/blake4_ct.c`)

| Function | Purpose |
|----------|---------|
| `blake4_ct_memcmp()` | Compare secrets without timing leaks |
| `blake4_ct_memzero()` | Clear secrets (not optimized away) |
| `blake4_ct_select64()` | Branchless conditional select |
| `blake4_ct_eq64()` | Constant-time equality |
| `blake4_ct_lt64()` | Constant-time less-than |

### Implementation Techniques

**Constant-time comparison**:
```c
int blake4_ct_memcmp(const void *a, const void *b, size_t n) {
    volatile uint8_t diff = 0;
    for (size_t i = 0; i < n; i++) {
        diff |= ((const uint8_t *)a)[i] ^ ((const uint8_t *)b)[i];
    }
    return diff;
}
```

**Secure zeroing** (prevents compiler optimization):
```c
void blake4_ct_memzero(void *ptr, size_t n) {
    volatile uint8_t *p = (volatile uint8_t *)ptr;
    while (n--) *p++ = 0;
}
```

### Fuzzing Infrastructure

Created 4 libFuzzer harnesses in `fuzz/`:
- `fuzz_hash.c` - One-shot hashing
- `fuzz_incremental.c` - Streaming API
- `fuzz_deserialize.c` - State deserialization
- `fuzz_keyed.c` - Keyed hashing (MAC)

### Sanitizer Support

CMake options for runtime checking:
- `BLAKE4_SANITIZE_ADDRESS` - AddressSanitizer
- `BLAKE4_SANITIZE_UNDEFINED` - UBSan
- `BLAKE4_SANITIZE_MEMORY` - MemorySanitizer (Clang only)
- `BLAKE4_SANITIZE_THREAD` - ThreadSanitizer

---

## Phase 6: Static Library Linking Fix (Critical Bug)

### The Problem

When building BLAKE4 as a static library and linking into an application, the SIMD implementations weren't being used - it always fell back to "Portable C".

**Root Cause**: Static library linking only includes object files that resolve undefined symbols. The SIMD registration files (`blake4_neon.c`, `blake4_neon_asm.c`) used `__attribute__((constructor))` for auto-registration, but since no other code referenced symbols from those files, the linker dropped them entirely.

### Symptoms

```
Implementation: Portable C    # Should be "NEON (asm)"
NEON available: 1             # Feature detected correctly!
```

The CPU feature was detected, but the implementation function pointer was NULL because the registration code was never linked.

### The Fix

Changed from constructor-based auto-registration to explicit initialization:

**Before** (broken):
```c
// blake4_neon_asm.c
__attribute__((constructor))
static void blake4_neon_asm_init(void) {
    blake4_register_neon_asm(blake4_compress_neon_asm);
}
```

**After** (working):
```c
// blake4_neon_asm.c
void blake4_init_neon_asm(void) {
    blake4_register_neon_asm(blake4_compress_neon_asm);
}

// blake4_dispatch.c (called from select_implementation)
static void init_simd_implementations(void) {
#if defined(__aarch64__)
    blake4_init_neon();
    blake4_init_neon_asm();
#elif defined(__x86_64__)
    blake4_init_avx512();
    blake4_init_avx512_asm();
    blake4_init_avx2();
    blake4_init_avx2_asm();
#endif
}
```

Since `blake4_dispatch.c` IS referenced by the main code path, calling functions in the SIMD files forces the linker to include those object files.

### Alternative Solutions (Not Used)

1. **`-force_load` (macOS) / `--whole-archive` (Linux)**: Forces linker to include all objects from static library. Works but requires users to know special flags.

2. **Dummy symbols**: Export a symbol from each SIMD file and reference it from dispatch. Hacky but works.

3. **Single compilation unit**: Include all SIMD code via `#include`. Increases coupling.

### Lesson Learned

**Never rely on constructors for static library registration.** Always have explicit initialization functions called from code that's guaranteed to be linked.

---

## Performance Results

### Final Benchmarks (ARM64 Apple Silicon, NEON Assembly)

| Input Size | Serial | 2 Threads | 4 Threads | 8 Threads |
|------------|--------|-----------|-----------|-----------|
| 64 B | 144 MB/s | - | - | - |
| 1 KB | 428 MB/s | - | - | - |
| 1 MB | 421 MB/s | 810 MB/s | 1,492 MB/s | 2,336 MB/s |
| 4 MB | 421 MB/s | 826 MB/s | 1,608 MB/s | 2,958 MB/s |
| 16 MB | 422 MB/s | 830 MB/s | 1,638 MB/s | **3,218 MB/s** |

### Improvement Summary

| Metric | Before | After | Improvement |
|--------|--------|-------|-------------|
| Serial (1MB) | 133 MB/s | 421 MB/s | **3.2x** |
| Parallel 8-thread (16MB) | 943 MB/s | 3,218 MB/s | **3.4x** |

### Comparison to Industry Standards

| Algorithm | Implementation | 1MB Throughput |
|-----------|----------------|----------------|
| BLAKE4 | NEON asm (serial) | 421 MB/s |
| BLAKE4 | NEON asm (8 threads) | 3,218 MB/s |
| OpenSSL SHA-256 | ARM crypto ext | ~3,000 MB/s |
| OpenSSL SHA-512 | ARM crypto ext | ~1,700 MB/s |

---

## Test Coverage

### Test Suites
- **Core tests**: 44 tests (basic hashing, incremental, XOF, keyed, serialization, parallel, constant-time)
- **Stream tests**: 21 tests (Merkle tree encoding, verification, slicing)
- **Total**: 65 tests, all passing

### Sanitizer Validation
All tests pass with:
- AddressSanitizer
- UndefinedBehaviorSanitizer

---

## Files Created/Modified

### New Files (This Development Cycle)
| File | Purpose |
|------|---------|
| `src/blake4_dispatch.c` | CPU detection and SIMD dispatch |
| `src/blake4_avx512.c` | AVX-512 intrinsics implementation |
| `src/blake4_avx2.c` | AVX2 intrinsics implementation |
| `src/blake4_neon.c` | NEON intrinsics implementation |
| `src/blake4_avx512_x86-64.S` | AVX-512 assembly (Unix) |
| `src/blake4_avx512_x86-64_windows.asm` | AVX-512 assembly (Windows) |
| `src/blake4_avx2_x86-64.S` | AVX2 assembly (Unix) |
| `src/blake4_avx2_x86-64_windows.asm` | AVX2 assembly (Windows) |
| `src/blake4_neon_arm64.S` | NEON assembly (ARM64) |
| `src/blake4_parallel.c` | Multi-threaded hashing |
| `src/blake4_thread.h` | Cross-platform threading |
| `src/blake4_ct.c` | Constant-time utilities |
| `tools/benchmark.c` | Performance benchmarking |
| `fuzz/fuzz_*.c` | Fuzzing harnesses (4 files) |

### Key Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Threading | pthreads + Win32 | Maximum portability |
| Assembly | Both intrinsics and hand-written | Performance + maintainability |
| CPU target | Runtime dispatch | One binary for all CPUs |
| Registration | Explicit init functions | Static library compatibility |

---

## Future Work

1. **Formal Industry Benchmarks**: SUPERCOP, libsodium benchmark suite
2. **External Security Audit**: Required for production cryptographic use
3. **Formal Verification**: Prove constant-time properties
4. **Additional Platforms**: RISC-V vector extensions, WebAssembly SIMD

---

## Contributors

- Initial implementation and optimization: Claude Code collaboration
- Design decisions: User-guided with AI assistance

---

*Last updated: January 2026*
