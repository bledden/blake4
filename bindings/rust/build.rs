//! Build script for BLAKE4 Rust bindings.
//!
//! Compiles the BLAKE4 C library from source.

fn main() {
    // Path to the C source files (relative to bindings/rust)
    let blake4_src = "../../src/blake4.c";
    let blake4_simd_src = "../../src/blake4_simd.c";
    let include_dir = "../../include";

    // Build the BLAKE4 library
    let mut build = cc::Build::new();

    build
        .file(blake4_src)
        .file(blake4_simd_src)
        .include(include_dir)
        .opt_level(3);

    // Enable SIMD on supported platforms
    #[cfg(target_arch = "x86_64")]
    {
        build.flag_if_supported("-mavx2");
    }

    build.compile("blake4");

    // Tell cargo to invalidate the built crate whenever the C sources change
    println!("cargo:rerun-if-changed={}", blake4_src);
    println!("cargo:rerun-if-changed={}", blake4_simd_src);
    println!("cargo:rerun-if-changed={}/blake4.h", include_dir);
    println!("cargo:rerun-if-changed={}/blake4_simd.h", include_dir);
}
