# BLAKE4 SUPERCOP Integration

This directory contains files needed to integrate BLAKE4 into SUPERCOP, the standard cryptographic benchmarking suite.

## What is SUPERCOP?

SUPERCOP (System for Unified Performance Evaluation Related to Cryptographic Operations and Primitives) is the standard benchmarking system used by the cryptographic community. It's maintained by Daniel J. Bernstein and others.

Website: https://bench.cr.yp.to/

## Installation

### Download SUPERCOP

```bash
wget https://bench.cr.yp.to/supercop/supercop-20240808.tar.xz
tar xf supercop-20240808.tar.xz
cd supercop-20240808
```

### Add BLAKE4 Implementation

```bash
# Create implementation directory
mkdir -p crypto_hash/blake4/ref

# Copy BLAKE4 source files
cp /path/to/blake4/src/blake4.c crypto_hash/blake4/ref/
cp /path/to/blake4/src/blake4_dispatch.c crypto_hash/blake4/ref/
cp /path/to/blake4/src/blake4_simd.c crypto_hash/blake4/ref/
cp /path/to/blake4/include/blake4.h crypto_hash/blake4/ref/
cp /path/to/blake4/include/blake4_dispatch.h crypto_hash/blake4/ref/

# Copy SUPERCOP wrapper files
cp /path/to/blake4/benchmarks/supercop/api.h crypto_hash/blake4/ref/
cp /path/to/blake4/benchmarks/supercop/hash.c crypto_hash/blake4/ref/
```

For optimized implementations, create additional directories:

```bash
# NEON implementation (ARM64)
mkdir -p crypto_hash/blake4/neon
# Copy files + NEON sources

# AVX2 implementation (x86-64)
mkdir -p crypto_hash/blake4/avx2
# Copy files + AVX2 sources

# AVX-512 implementation (x86-64)
mkdir -p crypto_hash/blake4/avx512
# Copy files + AVX-512 sources
```

### Run Benchmarks

```bash
# Initialize (first time only)
./do-part init

# Benchmark all hash functions
./do-part crypto_hash

# Benchmark only BLAKE4
./do-part crypto_hash blake4
```

## Results

Results are written to `bench/*/data`. Key files:

- `cycles` - CPU cycles per byte
- `xof` - Cycles for various message lengths
- `checksum` - Output verification

## Interpreting Results

SUPERCOP reports cycles per byte for various message lengths. Lower is better.

Example output:
```
blake4 ref cycles/byte for 1 byte: 1500
blake4 ref cycles/byte for 64 bytes: 40
blake4 ref cycles/byte for 1024 bytes: 8
blake4 ref cycles/byte for 16384 bytes: 5
```

## Comparison with Other Hash Functions

After running SUPERCOP, you can compare results:

```bash
# List all hash function results
ls bench/*/data/crypto_hash/*/cycles

# Compare specific implementations
cat bench/amd64-skylake/data/crypto_hash/blake4/ref/cycles
cat bench/amd64-skylake/data/crypto_hash/blake3/ref/cycles
cat bench/amd64-skylake/data/crypto_hash/sha256/openssl/cycles
```

## Files in This Directory

| File | Purpose |
|------|---------|
| `api.h` | SUPERCOP API header (defines CRYPTO_BYTES) |
| `hash.c` | Wrapper implementing crypto_hash() |
| `README.md` | This file |

## Notes

- SUPERCOP builds take several hours for full benchmark suite
- Use `./do-part crypto_hash blake4` to benchmark only BLAKE4
- Results vary by CPU; run on target platform for accurate numbers
- Disable CPU frequency scaling for consistent results
