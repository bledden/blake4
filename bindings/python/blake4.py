"""
BLAKE4 Python Bindings

Python bindings for the BLAKE4 cryptographic hash function using ctypes.
Requires the BLAKE4 shared library (libblake4.dylib, libblake4.so, or blake4.dll).

This is free and unencumbered software released into the public domain.
"""

import ctypes
import ctypes.util
import os
import sys
from typing import Optional, Union

# ============== Library Loading ==============

def _find_library():
    """Find the BLAKE4 shared library."""
    # Try common locations
    search_paths = [
        # Relative to this file (development)
        os.path.join(os.path.dirname(__file__), '..', '..', 'build'),
        os.path.join(os.path.dirname(__file__), '..', '..', 'build', 'Release'),
        os.path.join(os.path.dirname(__file__), '..', '..', 'build', 'Debug'),
        # System paths
        '/usr/local/lib',
        '/usr/lib',
    ]

    # Platform-specific library name
    if sys.platform == 'darwin':
        lib_name = 'libblake4.dylib'
    elif sys.platform == 'win32':
        lib_name = 'blake4.dll'
    else:
        lib_name = 'libblake4.so'

    for path in search_paths:
        lib_path = os.path.join(path, lib_name)
        if os.path.exists(lib_path):
            return lib_path

    # Try system library finder
    found = ctypes.util.find_library('blake4')
    if found:
        return found

    raise OSError(f"Could not find BLAKE4 library. Searched: {search_paths}")


_lib = ctypes.CDLL(_find_library())

# ============== Constants ==============

BLAKE4_OUT_LEN = 64
BLAKE4_KEY_LEN = 64
BLAKE4_BLOCK_LEN = 128
BLAKE4_CHUNK_LEN = 2048
BLAKE4_MAX_DEPTH = 54

# ============== Type Definitions ==============

class Blake4Hasher(ctypes.Structure):
    """BLAKE4 hasher state structure."""
    _fields_ = [
        ('cv', ctypes.c_uint64 * 8),
        ('key_words', ctypes.c_uint64 * 8),
        ('chunk_counter', ctypes.c_uint64),
        ('buf', ctypes.c_uint8 * BLAKE4_BLOCK_LEN),
        ('buf_len', ctypes.c_uint8),
        ('chunk_buf', ctypes.c_uint8 * BLAKE4_CHUNK_LEN),
        ('chunk_buf_len', ctypes.c_uint16),
        ('cv_stack', (ctypes.c_uint8 * 64) * BLAKE4_MAX_DEPTH),
        ('cv_stack_len', ctypes.c_uint8),
        ('flags', ctypes.c_uint8),
    ]


# ============== Function Bindings ==============

# blake4_hash
_lib.blake4_hash.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p]
_lib.blake4_hash.restype = None

# blake4_hash_xof
_lib.blake4_hash_xof.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p, ctypes.c_size_t]
_lib.blake4_hash_xof.restype = None

# blake4_hash_keyed
_lib.blake4_hash_keyed.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p]
_lib.blake4_hash_keyed.restype = None

# blake4_derive_key
_lib.blake4_derive_key.argtypes = [ctypes.c_char_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p, ctypes.c_size_t]
_lib.blake4_derive_key.restype = None

# Hasher API
_lib.blake4_hasher_init.argtypes = [ctypes.POINTER(Blake4Hasher)]
_lib.blake4_hasher_init.restype = None

_lib.blake4_hasher_init_keyed.argtypes = [ctypes.POINTER(Blake4Hasher), ctypes.c_void_p]
_lib.blake4_hasher_init_keyed.restype = None

_lib.blake4_hasher_init_derive_key.argtypes = [ctypes.POINTER(Blake4Hasher), ctypes.c_char_p]
_lib.blake4_hasher_init_derive_key.restype = None

_lib.blake4_hasher_update.argtypes = [ctypes.POINTER(Blake4Hasher), ctypes.c_void_p, ctypes.c_size_t]
_lib.blake4_hasher_update.restype = None

_lib.blake4_hasher_finalize.argtypes = [ctypes.POINTER(Blake4Hasher), ctypes.c_void_p, ctypes.c_size_t]
_lib.blake4_hasher_finalize.restype = None

_lib.blake4_hasher_reset.argtypes = [ctypes.POINTER(Blake4Hasher)]
_lib.blake4_hasher_reset.restype = None

# Version API
_lib.blake4_version_string.argtypes = []
_lib.blake4_version_string.restype = ctypes.c_char_p

_lib.blake4_version_major.argtypes = []
_lib.blake4_version_major.restype = ctypes.c_int

_lib.blake4_version_minor.argtypes = []
_lib.blake4_version_minor.restype = ctypes.c_int

_lib.blake4_version_patch.argtypes = []
_lib.blake4_version_patch.restype = ctypes.c_int


# ============== Python API ==============

def hash(data: Union[bytes, bytearray, memoryview]) -> bytes:
    """
    Compute BLAKE4 hash of data.

    Args:
        data: Input data to hash

    Returns:
        64-byte hash digest
    """
    if isinstance(data, memoryview):
        data = bytes(data)
    out = (ctypes.c_uint8 * BLAKE4_OUT_LEN)()
    _lib.blake4_hash(data, len(data), out)
    return bytes(out)


def hash_xof(data: Union[bytes, bytearray, memoryview], output_len: int) -> bytes:
    """
    Compute BLAKE4 hash with arbitrary output length (XOF mode).

    Args:
        data: Input data to hash
        output_len: Desired output length in bytes

    Returns:
        Hash digest of specified length
    """
    if isinstance(data, memoryview):
        data = bytes(data)
    out = (ctypes.c_uint8 * output_len)()
    _lib.blake4_hash_xof(data, len(data), out, output_len)
    return bytes(out)


def hash_keyed(key: bytes, data: Union[bytes, bytearray, memoryview]) -> bytes:
    """
    Compute keyed BLAKE4 hash (MAC).

    Args:
        key: 64-byte key
        data: Input data to hash

    Returns:
        64-byte keyed hash digest
    """
    if len(key) != BLAKE4_KEY_LEN:
        raise ValueError(f"Key must be {BLAKE4_KEY_LEN} bytes")
    if isinstance(data, memoryview):
        data = bytes(data)
    out = (ctypes.c_uint8 * BLAKE4_OUT_LEN)()
    _lib.blake4_hash_keyed(key, data, len(data), out)
    return bytes(out)


def derive_key(context: str, key_material: bytes, output_len: int = BLAKE4_OUT_LEN) -> bytes:
    """
    Derive a key using BLAKE4.

    Args:
        context: Application-specific context string
        key_material: Input key material
        output_len: Desired output length (default: 64 bytes)

    Returns:
        Derived key
    """
    out = (ctypes.c_uint8 * output_len)()
    _lib.blake4_derive_key(
        context.encode('utf-8'),
        key_material,
        len(key_material),
        out,
        output_len
    )
    return bytes(out)


class Hasher:
    """
    Incremental BLAKE4 hasher.

    Example:
        h = blake4.Hasher()
        h.update(b"hello ")
        h.update(b"world")
        digest = h.finalize()
    """

    def __init__(self, key: Optional[bytes] = None, derive_key_context: Optional[str] = None):
        """
        Initialize a hasher.

        Args:
            key: Optional 64-byte key for keyed hashing
            derive_key_context: Optional context for key derivation mode
        """
        self._state = Blake4Hasher()
        if key is not None and derive_key_context is not None:
            raise ValueError("Cannot specify both key and derive_key_context")
        if key is not None:
            if len(key) != BLAKE4_KEY_LEN:
                raise ValueError(f"Key must be {BLAKE4_KEY_LEN} bytes")
            _lib.blake4_hasher_init_keyed(ctypes.byref(self._state), key)
        elif derive_key_context is not None:
            _lib.blake4_hasher_init_derive_key(
                ctypes.byref(self._state),
                derive_key_context.encode('utf-8')
            )
        else:
            _lib.blake4_hasher_init(ctypes.byref(self._state))

    def update(self, data: Union[bytes, bytearray, memoryview]) -> 'Hasher':
        """
        Add data to the hasher.

        Args:
            data: Data to hash

        Returns:
            self (for chaining)
        """
        if isinstance(data, memoryview):
            data = bytes(data)
        _lib.blake4_hasher_update(ctypes.byref(self._state), data, len(data))
        return self

    def finalize(self, output_len: int = BLAKE4_OUT_LEN) -> bytes:
        """
        Finalize and return the hash.

        Args:
            output_len: Desired output length (default: 64 bytes)

        Returns:
            Hash digest
        """
        out = (ctypes.c_uint8 * output_len)()
        _lib.blake4_hasher_finalize(ctypes.byref(self._state), out, output_len)
        return bytes(out)

    def reset(self) -> 'Hasher':
        """
        Reset the hasher to its initial state (preserving key if keyed).

        Returns:
            self (for chaining)
        """
        _lib.blake4_hasher_reset(ctypes.byref(self._state))
        return self

    def copy(self) -> 'Hasher':
        """
        Create a copy of this hasher.

        Returns:
            New Hasher with same state
        """
        new = Hasher.__new__(Hasher)
        new._state = Blake4Hasher()
        ctypes.memmove(ctypes.byref(new._state), ctypes.byref(self._state), ctypes.sizeof(Blake4Hasher))
        return new


def version() -> str:
    """Get BLAKE4 library version string."""
    return _lib.blake4_version_string().decode('utf-8')


def version_tuple() -> tuple:
    """Get BLAKE4 library version as (major, minor, patch) tuple."""
    return (
        _lib.blake4_version_major(),
        _lib.blake4_version_minor(),
        _lib.blake4_version_patch()
    )


# ============== Module Info ==============

__version__ = version()
__all__ = [
    'hash', 'hash_xof', 'hash_keyed', 'derive_key',
    'Hasher', 'version', 'version_tuple',
    'BLAKE4_OUT_LEN', 'BLAKE4_KEY_LEN', 'BLAKE4_BLOCK_LEN', 'BLAKE4_CHUNK_LEN',
]


if __name__ == '__main__':
    # Simple self-test
    print(f"BLAKE4 Python Bindings v{__version__}")
    print()

    # Test basic hash
    h = hash(b"hello world")
    print(f"hash('hello world'): {h.hex()[:64]}...")

    # Test incremental
    hasher = Hasher()
    hasher.update(b"hello ")
    hasher.update(b"world")
    h2 = hasher.finalize()
    assert h == h2, "Incremental hash mismatch!"
    print("Incremental hash: OK")

    # Test keyed hash
    key = bytes(BLAKE4_KEY_LEN)
    h3 = hash_keyed(key, b"test")
    print(f"keyed hash: {h3.hex()[:64]}...")

    # Test key derivation
    derived = derive_key("my-app context", b"secret key material")
    print(f"derived key: {derived.hex()[:64]}...")

    print()
    print("All tests passed!")
