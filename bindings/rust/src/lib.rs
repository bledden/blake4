//! BLAKE4 Cryptographic Hash Function
//!
//! This crate provides Rust bindings for the BLAKE4 hash function,
//! a 512-bit internal state hash function extending the BLAKE family.
//!
//! # Examples
//!
//! ## Simple hashing
//! ```
//! let hash = blake4::hash(b"hello world");
//! assert_eq!(hash.as_bytes().len(), 64);
//! ```
//!
//! ## Incremental hashing
//! ```
//! let mut hasher = blake4::Hasher::new();
//! hasher.update(b"hello ");
//! hasher.update(b"world");
//! let hash = hasher.finalize();
//! ```
//!
//! ## Keyed hashing (MAC)
//! ```
//! let key = [0u8; 64];
//! let mac = blake4::hash_keyed(&key, b"message");
//! ```
//!
//! ## Key derivation
//! ```
//! let derived = blake4::derive_key("my-app context", b"secret key material");
//! ```

#![forbid(unsafe_op_in_unsafe_fn)]

use std::mem::MaybeUninit;

// ============== Constants ==============

/// Default output length (64 bytes / 512 bits)
pub const OUT_LEN: usize = 64;

/// Key length (64 bytes)
pub const KEY_LEN: usize = 64;

/// Block size (128 bytes)
pub const BLOCK_LEN: usize = 128;

/// Chunk size (2048 bytes)
pub const CHUNK_LEN: usize = 2048;

/// Maximum tree depth
pub const MAX_DEPTH: usize = 54;

// ============== FFI Bindings ==============

mod ffi {
    use std::os::raw::{c_char, c_int, c_void};

    #[repr(C)]
    pub struct Blake4Hasher {
        cv: [u64; 8],
        key_words: [u64; 8],
        chunk_counter: u64,
        buf: [u8; super::BLOCK_LEN],
        buf_len: u8,
        chunk_buf: [u8; super::CHUNK_LEN],
        chunk_buf_len: u16,
        cv_stack: [[u8; 64]; super::MAX_DEPTH],
        cv_stack_len: u8,
        flags: u8,
    }

    extern "C" {
        pub fn blake4_hash(input: *const c_void, input_len: usize, out: *mut u8);
        pub fn blake4_hash_xof(input: *const c_void, input_len: usize, out: *mut u8, out_len: usize);
        pub fn blake4_hash_keyed(key: *const u8, input: *const c_void, input_len: usize, out: *mut u8);
        pub fn blake4_derive_key(
            context: *const c_char,
            key_material: *const c_void,
            key_material_len: usize,
            out: *mut u8,
            out_len: usize,
        );

        pub fn blake4_hasher_init(hasher: *mut Blake4Hasher);
        pub fn blake4_hasher_init_keyed(hasher: *mut Blake4Hasher, key: *const u8);
        pub fn blake4_hasher_init_derive_key(hasher: *mut Blake4Hasher, context: *const c_char);
        pub fn blake4_hasher_update(hasher: *mut Blake4Hasher, input: *const c_void, input_len: usize);
        pub fn blake4_hasher_finalize(hasher: *const Blake4Hasher, out: *mut u8, out_len: usize);
        pub fn blake4_hasher_reset(hasher: *mut Blake4Hasher);

        pub fn blake4_version_string() -> *const c_char;
        pub fn blake4_version_major() -> c_int;
        pub fn blake4_version_minor() -> c_int;
        pub fn blake4_version_patch() -> c_int;
    }
}

// ============== Hash Type ==============

/// A BLAKE4 hash digest.
///
/// This is a fixed-size 64-byte array containing the hash output.
#[derive(Clone, Copy, PartialEq, Eq, Hash)]
pub struct Hash([u8; OUT_LEN]);

impl Hash {
    /// Create a Hash from a byte array.
    pub fn from_bytes(bytes: [u8; OUT_LEN]) -> Self {
        Hash(bytes)
    }

    /// Get the hash as a byte slice.
    pub fn as_bytes(&self) -> &[u8] {
        &self.0
    }

    /// Get the hash as a byte array.
    pub fn to_bytes(&self) -> [u8; OUT_LEN] {
        self.0
    }

    /// Convert to lowercase hex string.
    pub fn to_hex(&self) -> String {
        self.0.iter().map(|b| format!("{:02x}", b)).collect()
    }
}

impl AsRef<[u8]> for Hash {
    fn as_ref(&self) -> &[u8] {
        &self.0
    }
}

impl std::fmt::Debug for Hash {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "Hash({})", self.to_hex())
    }
}

impl std::fmt::Display for Hash {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.to_hex())
    }
}

// ============== Simple API ==============

/// Compute the BLAKE4 hash of input data.
///
/// Returns a 64-byte hash.
///
/// # Example
/// ```
/// let hash = blake4::hash(b"hello world");
/// assert_eq!(hash.as_bytes().len(), 64);
/// ```
pub fn hash(input: &[u8]) -> Hash {
    let mut out = [0u8; OUT_LEN];
    unsafe {
        ffi::blake4_hash(input.as_ptr() as *const _, input.len(), out.as_mut_ptr());
    }
    Hash(out)
}

/// Compute BLAKE4 hash with arbitrary output length (XOF mode).
///
/// # Example
/// ```
/// let hash = blake4::hash_xof(b"data", 128);
/// assert_eq!(hash.len(), 128);
/// ```
pub fn hash_xof(input: &[u8], output_len: usize) -> Vec<u8> {
    let mut out = vec![0u8; output_len];
    unsafe {
        ffi::blake4_hash_xof(
            input.as_ptr() as *const _,
            input.len(),
            out.as_mut_ptr(),
            output_len,
        );
    }
    out
}

/// Compute a keyed BLAKE4 hash (MAC).
///
/// The key must be exactly 64 bytes.
///
/// # Panics
/// Panics if the key is not exactly 64 bytes.
///
/// # Example
/// ```
/// let key = [0u8; 64];
/// let mac = blake4::hash_keyed(&key, b"message");
/// ```
pub fn hash_keyed(key: &[u8; KEY_LEN], input: &[u8]) -> Hash {
    let mut out = [0u8; OUT_LEN];
    unsafe {
        ffi::blake4_hash_keyed(
            key.as_ptr(),
            input.as_ptr() as *const _,
            input.len(),
            out.as_mut_ptr(),
        );
    }
    Hash(out)
}

/// Derive a key using BLAKE4.
///
/// # Example
/// ```
/// let derived = blake4::derive_key("my-app context", b"secret key material");
/// assert_eq!(derived.as_bytes().len(), 64);
/// ```
pub fn derive_key(context: &str, key_material: &[u8]) -> Hash {
    derive_key_with_len(context, key_material, OUT_LEN)
        .try_into()
        .map(Hash)
        .unwrap()
}

/// Derive a key with specified output length.
///
/// # Example
/// ```
/// let derived = blake4::derive_key_with_len("context", b"material", 32);
/// assert_eq!(derived.len(), 32);
/// ```
pub fn derive_key_with_len(context: &str, key_material: &[u8], output_len: usize) -> Vec<u8> {
    let mut out = vec![0u8; output_len];
    let context_cstr = std::ffi::CString::new(context).expect("context contains null byte");
    unsafe {
        ffi::blake4_derive_key(
            context_cstr.as_ptr(),
            key_material.as_ptr() as *const _,
            key_material.len(),
            out.as_mut_ptr(),
            output_len,
        );
    }
    out
}

// ============== Incremental Hasher ==============

/// Incremental BLAKE4 hasher.
///
/// Use this for streaming data or when you need to hash data in chunks.
///
/// # Example
/// ```
/// let mut hasher = blake4::Hasher::new();
/// hasher.update(b"hello ");
/// hasher.update(b"world");
/// let hash = hasher.finalize();
/// ```
pub struct Hasher {
    state: ffi::Blake4Hasher,
}

impl Hasher {
    /// Create a new hasher for standard hashing.
    pub fn new() -> Self {
        let mut state = MaybeUninit::<ffi::Blake4Hasher>::uninit();
        unsafe {
            ffi::blake4_hasher_init(state.as_mut_ptr());
            Hasher {
                state: state.assume_init(),
            }
        }
    }

    /// Create a new hasher for keyed hashing (MAC).
    ///
    /// # Panics
    /// Panics if the key is not exactly 64 bytes.
    pub fn new_keyed(key: &[u8; KEY_LEN]) -> Self {
        let mut state = MaybeUninit::<ffi::Blake4Hasher>::uninit();
        unsafe {
            ffi::blake4_hasher_init_keyed(state.as_mut_ptr(), key.as_ptr());
            Hasher {
                state: state.assume_init(),
            }
        }
    }

    /// Create a new hasher for key derivation.
    pub fn new_derive_key(context: &str) -> Self {
        let context_cstr = std::ffi::CString::new(context).expect("context contains null byte");
        let mut state = MaybeUninit::<ffi::Blake4Hasher>::uninit();
        unsafe {
            ffi::blake4_hasher_init_derive_key(state.as_mut_ptr(), context_cstr.as_ptr());
            Hasher {
                state: state.assume_init(),
            }
        }
    }

    /// Add data to the hasher.
    ///
    /// This can be called multiple times to process data incrementally.
    pub fn update(&mut self, input: &[u8]) -> &mut Self {
        unsafe {
            ffi::blake4_hasher_update(
                &mut self.state,
                input.as_ptr() as *const _,
                input.len(),
            );
        }
        self
    }

    /// Finalize and return the hash.
    ///
    /// This does not consume the hasher; you can continue to add data
    /// and call finalize again to get an updated hash.
    pub fn finalize(&self) -> Hash {
        let mut out = [0u8; OUT_LEN];
        unsafe {
            ffi::blake4_hasher_finalize(&self.state, out.as_mut_ptr(), OUT_LEN);
        }
        Hash(out)
    }

    /// Finalize with arbitrary output length (XOF mode).
    pub fn finalize_xof(&self, output_len: usize) -> Vec<u8> {
        let mut out = vec![0u8; output_len];
        unsafe {
            ffi::blake4_hasher_finalize(&self.state, out.as_mut_ptr(), output_len);
        }
        out
    }

    /// Reset the hasher to its initial state.
    ///
    /// If the hasher was created with a key, the key is preserved.
    pub fn reset(&mut self) -> &mut Self {
        unsafe {
            ffi::blake4_hasher_reset(&mut self.state);
        }
        self
    }
}

impl Default for Hasher {
    fn default() -> Self {
        Self::new()
    }
}

impl Clone for Hasher {
    fn clone(&self) -> Self {
        // Safe because Blake4Hasher is POD
        unsafe {
            let mut new_state = MaybeUninit::<ffi::Blake4Hasher>::uninit();
            std::ptr::copy_nonoverlapping(&self.state, new_state.as_mut_ptr(), 1);
            Hasher {
                state: new_state.assume_init(),
            }
        }
    }
}

impl std::io::Write for Hasher {
    fn write(&mut self, buf: &[u8]) -> std::io::Result<usize> {
        self.update(buf);
        Ok(buf.len())
    }

    fn flush(&mut self) -> std::io::Result<()> {
        Ok(())
    }
}

// ============== Version Info ==============

/// Get the BLAKE4 library version string.
pub fn version() -> &'static str {
    unsafe {
        let ptr = ffi::blake4_version_string();
        std::ffi::CStr::from_ptr(ptr)
            .to_str()
            .expect("invalid UTF-8 in version string")
    }
}

/// Get the BLAKE4 library version as (major, minor, patch).
pub fn version_tuple() -> (i32, i32, i32) {
    unsafe {
        (
            ffi::blake4_version_major(),
            ffi::blake4_version_minor(),
            ffi::blake4_version_patch(),
        )
    }
}

// ============== Tests ==============

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_hash_empty() {
        let hash = hash(b"");
        assert_eq!(hash.as_bytes().len(), OUT_LEN);
    }

    #[test]
    fn test_hash_abc() {
        let hash = hash(b"abc");
        assert_eq!(hash.as_bytes().len(), OUT_LEN);
    }

    #[test]
    fn test_incremental_matches_oneshot() {
        let input = b"hello world, this is a test";

        let hash1 = hash(input);

        let mut hasher = Hasher::new();
        hasher.update(b"hello ");
        hasher.update(b"world, ");
        hasher.update(b"this is a test");
        let hash2 = hasher.finalize();

        assert_eq!(hash1, hash2);
    }

    #[test]
    fn test_keyed_hash() {
        let key = [0u8; KEY_LEN];
        let mac1 = hash_keyed(&key, b"message");
        let mac2 = hash_keyed(&key, b"message");
        assert_eq!(mac1, mac2);

        let mac3 = hash_keyed(&key, b"different");
        assert_ne!(mac1, mac3);
    }

    #[test]
    fn test_key_derivation() {
        let derived1 = derive_key("context 1", b"material");
        let derived2 = derive_key("context 2", b"material");
        assert_ne!(derived1, derived2);
    }

    #[test]
    fn test_xof() {
        let short = hash_xof(b"data", 32);
        let long = hash_xof(b"data", 128);

        assert_eq!(short.len(), 32);
        assert_eq!(long.len(), 128);
        assert_eq!(&short[..], &long[..32]);
    }

    #[test]
    fn test_hasher_reset() {
        let mut hasher = Hasher::new();
        hasher.update(b"garbage");
        hasher.reset();
        hasher.update(b"test");
        let hash1 = hasher.finalize();

        let hash2 = hash(b"test");
        assert_eq!(hash1, hash2);
    }

    #[test]
    fn test_hasher_clone() {
        let mut hasher1 = Hasher::new();
        hasher1.update(b"hello ");

        let mut hasher2 = hasher1.clone();

        hasher1.update(b"world");
        hasher2.update(b"world");

        assert_eq!(hasher1.finalize(), hasher2.finalize());
    }

    #[test]
    fn test_version() {
        let ver = version();
        assert!(!ver.is_empty());
        assert!(ver.contains('.'));

        let (major, minor, patch) = version_tuple();
        assert!(major >= 0);
        assert!(minor >= 0);
        assert!(patch >= 0);
    }

    #[test]
    fn test_hash_display() {
        let h = hash(b"test");
        let hex = h.to_string();
        assert_eq!(hex.len(), 128); // 64 bytes * 2 hex chars
    }
}
