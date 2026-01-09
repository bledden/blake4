# BLAKE4 Industry Benchmarks

This directory contains benchmark infrastructure for comparing BLAKE4 against industry-standard hash functions.

## Quick Benchmark

Run the built-in benchmark tool:

```bash
cd build
./benchmark 100   # 100 base iterations
```

## Benchmark Suites

### 1. Internal Benchmark (`tools/benchmark.c`)

Tests BLAKE4 at various input sizes with serial and parallel modes.

```bash
./benchmark
```

### 2. OpenSSL Comparison

Compare against OpenSSL's optimized SHA implementations:

```bash
# SHA-256 and SHA-512 benchmarks
openssl speed sha256 sha512

# Specific message sizes
openssl speed -evp sha256 -bytes 1024
openssl speed -evp sha512 -bytes 1048576
```

### 3. BLAKE3 Comparison

Using the b3sum tool:

```bash
# Install b3sum
cargo install b3sum
# or: brew install b3sum

# Benchmark with hyperfine
hyperfine --warmup 3 \
  'b3sum /path/to/1MB_file' \
  './blake4sum /path/to/1MB_file'
```

### 4. Criterion Microbenchmarks (Rust)

For the Rust bindings:

```bash
cd bindings/rust
cargo bench
```

## Running Comprehensive Benchmarks

### compare_hashes.sh

```bash
./benchmarks/compare_hashes.sh
```

This script:
1. Creates test files of various sizes
2. Benchmarks BLAKE4, SHA-256, SHA-512, BLAKE3
3. Reports throughput in MB/s
4. Generates comparison tables

## Benchmark Results Format

Results are output in both human-readable and machine-parseable formats:

```
Algorithm,Size,Throughput_MBps,Time_ms
BLAKE4,1KB,428.5,0.023
SHA256,1KB,2100.3,0.005
```

## SUPERCOP Integration

SUPERCOP (System for Unified Performance Evaluation Related to Cryptographic Operations and Primitives) is the standard benchmarking system for cryptographic software.

### Installing SUPERCOP

```bash
# Download SUPERCOP
wget https://bench.cr.yp.to/supercop/supercop-20240808.tar.xz
tar xf supercop-20240808.tar.xz
cd supercop-20240808

# Build (takes several hours)
./do-part init
./do-part crypto_hash
```

### Adding BLAKE4 to SUPERCOP

1. Create the implementation directory:
```bash
mkdir -p crypto_hash/blake4/ref
```

2. Copy BLAKE4 files:
```bash
cp /path/to/blake4/src/blake4.c crypto_hash/blake4/ref/
cp /path/to/blake4/include/blake4.h crypto_hash/blake4/ref/
```

3. Create the required `api.h`:
```c
#define CRYPTO_BYTES 64
```

4. Create `hash.c` wrapper:
```c
#include "blake4.h"

int crypto_hash(unsigned char *out, const unsigned char *in,
                unsigned long long inlen) {
    blake4_hash(in, inlen, out);
    return 0;
}
```

5. Run the benchmark:
```bash
./do-part crypto_hash blake4
```

## Performance Targets

Based on current implementation:

| Platform | Target (Serial) | Target (8 threads) |
|----------|-----------------|-------------------|
| ARM64 NEON | 400+ MB/s | 3000+ MB/s |
| x86-64 AVX2 | 500+ MB/s | 4000+ MB/s |
| x86-64 AVX-512 | 700+ MB/s | 5000+ MB/s |

## Notes

- Benchmarks should be run on an idle system
- Run multiple iterations to reduce variance
- Consider CPU frequency scaling (disable turbo for consistency)
- Memory bandwidth can be a limiting factor for large inputs
