# BLAKE4 Goals and Non-Goals

> Defines what BLAKE4 aims to achieve and explicitly what it does not attempt.

---

## Goals

### G1. Solve Real Problems BLAKE3 Doesn't Fully Address

BLAKE4 only makes sense if it provides value beyond BLAKE3. The following are concrete gaps:

| Gap | BLAKE3 Status | BLAKE4 Goal |
|-----|---------------|-------------|
| 512-bit native output | XOF can produce 512 bits, but internal state is 256-bit | Native 512-bit with appropriate internal capacity |
| Stable C ABI | Sources meant to be compiled in | Versioned, stable shared library |
| NO_ASM portable build | Possible but underdocumented | First-class `-DBLAKE4_NO_ASM` flag |
| State serialization | Not officially supported | Documented, versioned serialization API |
| Verified streaming format | Bao exists but is beta, not standardized | Canonical proof format in spec |
| Short input performance | Slower than BLAKE2s for <1KB | Competitive across all input sizes |

### G2. Maintain BLAKE3's Core Strengths

BLAKE4 should preserve what makes BLAKE3 excellent:

- **Speed:** Remain competitive with or faster than BLAKE3 on large inputs
- **Parallelism:** Support tree hashing for multi-core and SIMD
- **Unified modes:** Single algorithm for hash, MAC, KDF, XOF
- **Simplicity:** No complex parameter negotiation
- **Security:** At least equivalent cryptanalytic resistance

### G3. Improve Deployability

Reduce friction for adoption:

- Clean CMake build with find_package support
- pkg-config file generation
- Stable header with semantic versioning
- Pre-built binaries for common platforms
- Clear licensing (public domain + Apache 2.0)

### G4. Provide Clear Security Positioning

Be explicit about security claims:

- Document classical and quantum security levels
- State round count justification
- Provide cryptanalysis summary
- Clear guidance on appropriate use cases

### G5. Enable Verified Streaming (BLAKE4-STREAM Profile)

Standardize what Bao pioneered:

- Canonical chunk size (or explicit size negotiation)
- Standard proof encoding format
- Interoperability test vectors
- Clear domain separation for proof context

### G6. Support Higher Security Margin (BLAKE4-512 Profile)

For compliance-driven and conservative users:

- 512-bit output as first-class option
- Appropriate internal state for security margin
- Possibly increased round count
- Clear post-quantum security claims

---

## Non-Goals

### NG1. Replace BLAKE3 for All Use Cases

BLAKE3 is already deployed and working. BLAKE4 should:
- Coexist, not replace
- Target specific gaps
- Provide migration path, not forced upgrade

### NG2. Create a ZK-Friendly Hash

ZK-friendly hashes require fundamentally different design:
- Algebraic operations instead of ARX
- Different security analysis
- Different community and use cases

A "BLAKE4-ZK" would not be BLAKE in any meaningful sense. This is a separate project.

### NG3. Password Hashing

BLAKE4 is fast by design. Fast hashes are unsuitable for passwords.
- Use Argon2, bcrypt, or scrypt for passwords
- BLAKE4's KDF mode is for key derivation, not password hashing

### NG4. FIPS 140 Certification (Initial Release)

FIPS certification is:
- Expensive and time-consuming
- Requires stable, frozen specification
- May constrain design choices

BLAKE4 should:
- Design with certification possibility in mind
- Not pursue certification in initial release
- Document compliance considerations for future

### NG5. Too Many Variants

Fragmentation killed adoption of earlier hash families.

BLAKE4 should have:
- Maximum 2 flagship variants (PORTABLE + STREAM recommended)
- Optional 512-bit profile as extension
- Clear "don't use" guidance for others

### NG6. Backwards Compatibility with BLAKE3 Outputs

Changing the algorithm means changing outputs. Accept this.
- Same API shape is valuable
- Same output is impossible if algorithm changes
- Library can include BLAKE3 for migration

### NG7. Novel Cryptographic Primitives

BLAKE4 should use well-analyzed components:
- ChaCha-derived permutation (proven)
- Standard tree hashing (proven)
- Known domain separation techniques

Avoid:
- New permutation designs without analysis
- Novel tree structures
- Experimental constructions

### NG8. Hardware-First Design

BLAKE4 prioritizes software efficiency:
- ARX is naturally software-friendly
- Hardware can still implement efficiently
- Not optimizing for ASIC/FPGA at expense of software

### NG9. Support for Legacy Platforms

Target modern systems:
- 64-bit architectures primarily
- 32-bit as secondary (BLAKE4-LITE could address)
- No support for platforms without 64-bit integers

---

## Success Criteria

BLAKE4 is successful if:

1. **Adoption:** At least 3 major projects adopt within 2 years of release
2. **Performance:** Matches or exceeds BLAKE3 on inputs >1KB
3. **Short inputs:** Within 2x of BLAKE2s on inputs <64 bytes
4. **Build simplicity:** Single-command build on Linux, macOS, Windows
5. **Interoperability:** Multiple independent implementations pass test vectors
6. **Security:** No attacks better than generic after 1 year of public scrutiny

---

## Decision Log

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Number of flagships | 2 (PORTABLE + STREAM) | Avoid fragmentation while addressing main gaps |
| 512-bit support | Optional profile, not default | Most users don't need it; keeps default simple |
| ZK variant | Explicitly out of scope | Too different to be "BLAKE" |
| FIPS path | Defer to future | Don't constrain initial design |
| API compatibility | Same shape as BLAKE3 | Ease migration |
| Output compatibility | Not attempted | Algorithm change makes this impossible |
