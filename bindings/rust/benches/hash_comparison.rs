//! Criterion benchmarks comparing BLAKE4 against industry-standard hash functions.
//!
//! Run with: cargo bench
//!
//! This benchmark suite compares:
//! - BLAKE4 (this implementation)
//! - BLAKE3 (reference implementation)
//! - SHA-256 (sha2 crate)
//! - SHA-512 (sha2 crate)

use criterion::{black_box, criterion_group, criterion_main, BenchmarkId, Criterion, Throughput};
use sha2::{Digest, Sha256, Sha512};

/// Input sizes to benchmark (in bytes)
const SIZES: &[usize] = &[
    64,           // Tiny
    256,          // Small
    1024,         // 1 KB
    4096,         // 4 KB
    16384,        // 16 KB
    65536,        // 64 KB
    262144,       // 256 KB
    1048576,      // 1 MB
    4194304,      // 4 MB
    16777216,     // 16 MB
];

fn bench_blake4(c: &mut Criterion) {
    let mut group = c.benchmark_group("BLAKE4");

    for &size in SIZES {
        let data = vec![0u8; size];

        group.throughput(Throughput::Bytes(size as u64));
        group.bench_with_input(BenchmarkId::from_parameter(format_size(size)), &data, |b, data| {
            b.iter(|| {
                let hash = blake4::hash(black_box(data));
                black_box(hash)
            })
        });
    }

    group.finish();
}

fn bench_blake3(c: &mut Criterion) {
    let mut group = c.benchmark_group("BLAKE3");

    for &size in SIZES {
        let data = vec![0u8; size];

        group.throughput(Throughput::Bytes(size as u64));
        group.bench_with_input(BenchmarkId::from_parameter(format_size(size)), &data, |b, data| {
            b.iter(|| {
                let hash = blake3::hash(black_box(data));
                black_box(hash)
            })
        });
    }

    group.finish();
}

fn bench_sha256(c: &mut Criterion) {
    let mut group = c.benchmark_group("SHA-256");

    for &size in SIZES {
        let data = vec![0u8; size];

        group.throughput(Throughput::Bytes(size as u64));
        group.bench_with_input(BenchmarkId::from_parameter(format_size(size)), &data, |b, data| {
            b.iter(|| {
                let mut hasher = Sha256::new();
                hasher.update(black_box(data));
                let hash = hasher.finalize();
                black_box(hash)
            })
        });
    }

    group.finish();
}

fn bench_sha512(c: &mut Criterion) {
    let mut group = c.benchmark_group("SHA-512");

    for &size in SIZES {
        let data = vec![0u8; size];

        group.throughput(Throughput::Bytes(size as u64));
        group.bench_with_input(BenchmarkId::from_parameter(format_size(size)), &data, |b, data| {
            b.iter(|| {
                let mut hasher = Sha512::new();
                hasher.update(black_box(data));
                let hash = hasher.finalize();
                black_box(hash)
            })
        });
    }

    group.finish();
}

fn bench_incremental(c: &mut Criterion) {
    let mut group = c.benchmark_group("Incremental (1MB in 4KB chunks)");

    let data = vec![0u8; 1048576]; // 1 MB
    let chunk_size = 4096;

    group.throughput(Throughput::Bytes(data.len() as u64));

    group.bench_function("BLAKE4", |b| {
        b.iter(|| {
            let mut hasher = blake4::Hasher::new();
            for chunk in data.chunks(chunk_size) {
                hasher.update(black_box(chunk));
            }
            let hash = hasher.finalize();
            black_box(hash)
        })
    });

    group.bench_function("BLAKE3", |b| {
        b.iter(|| {
            let mut hasher = blake3::Hasher::new();
            for chunk in data.chunks(chunk_size) {
                hasher.update(black_box(chunk));
            }
            let hash = hasher.finalize();
            black_box(hash)
        })
    });

    group.bench_function("SHA-256", |b| {
        b.iter(|| {
            let mut hasher = Sha256::new();
            for chunk in data.chunks(chunk_size) {
                hasher.update(black_box(chunk));
            }
            let hash = hasher.finalize();
            black_box(hash)
        })
    });

    group.bench_function("SHA-512", |b| {
        b.iter(|| {
            let mut hasher = Sha512::new();
            for chunk in data.chunks(chunk_size) {
                hasher.update(black_box(chunk));
            }
            let hash = hasher.finalize();
            black_box(hash)
        })
    });

    group.finish();
}

/// Format size for display
fn format_size(bytes: usize) -> String {
    if bytes >= 1048576 {
        format!("{}MB", bytes / 1048576)
    } else if bytes >= 1024 {
        format!("{}KB", bytes / 1024)
    } else {
        format!("{}B", bytes)
    }
}

criterion_group!(
    benches,
    bench_blake4,
    bench_blake3,
    bench_sha256,
    bench_sha512,
    bench_incremental,
);
criterion_main!(benches);
