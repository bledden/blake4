# BLAKE4 Requirements Document

> Generated from Phase 0 discovery scan of community discussions, GitHub issues, research papers, and forum threads.

---

## A) Security Margin / Post-Quantum Positioning

### Requirement: 512-bit output with 256-bit collision security

**Source:** [BLAKE3 Issue #535 - BLAKE4 Wishlist](https://github.com/BLAKE3-team/BLAKE3/issues/535)

> "Regulatory compliance requirements increasingly demand larger hash outputs to meet post-quantum security standards. Current BLAKE3 deployment becomes difficult when organizations must satisfy standards and governmental agencies mandating 512-bit hashes to target 256-bit of post-quantum security."

**Why it matters:**
- Grover's algorithm provides quadratic speedup against hash functions
- 256-bit hash provides ~128 bits of quantum collision resistance
- 512-bit output provides ~256 bits of quantum collision resistance
- Compliance-driven organizations need this margin now, even if attacks are theoretical

**Risks:**
- Larger hashes increase bandwidth/storage overhead
- Some argue BLAKE3's XOF output can already produce 512 bits
- Performance impact from larger internal state

---

## B) Standards and Certification Reality

### Requirement: FIPS 140-3 pathway or explicit non-certification positioning

**Source:** [vLLM Issue #18334](https://github.com/vllm-project/vllm/issues/18334)

> "BLAKE3 is not listed as a secure hashing algorithm in NIST SP 800-140Cr2 and therefore not FIPS 140 compliant."

**Source:** [IETF Datatracker - draft-aumasson-blake3-00](https://datatracker.ietf.org/doc/draft-aumasson-blake3/)

The BLAKE3 IETF Internet-Draft expired January 2025 without progressing to RFC status.

**Why it matters:**
- Government and enterprise environments require FIPS compliance
- Many organizations cannot adopt non-standardized algorithms regardless of technical merit
- Without standardization, adoption is capped at "best effort" scenarios

**Decision point:** A BLAKE4 effort must decide early whether to pursue:
- "Best practical hash" (non-certified, fast iteration)
- "Standardization-first" (conservative design, more process)

---

## C) Performance on Short Inputs

### Requirement: Competitive performance across all input sizes

**Source:** [jszym.com - Maybe don't use Blake3 on Short Inputs](https://jszym.com/blog/short_input_hash/)

> "BLAKE3 remained slow even at 1 kilobyte of input. Half of the tested cryptographic functions beat BLAKE3's performance. Only at larger data sizes (megabytes+) did BLAKE3 demonstrate its expected speed advantage."

**Source:** [Hacker News Discussion](https://news.ycombinator.com/item?id=22021769)

> "On its own, BLAKE3 is slower than SHA2. It can achieve these performance numbers via clever tricks, which are not always available."

**Why it matters:**
- Many real-world use cases involve short inputs (UUIDs, keys, tokens, messages)
- BLAKE2s outperforms BLAKE3 on short inputs
- Tree hashing overhead is wasted on small data

**Potential solutions:**
- Bypass tree construction for small inputs (already done, but overhead remains)
- Optimize single-block path
- Consider a "BLAKE4-LITE" variant optimized for short inputs

---

## D) Portability and Build Friction

### Requirement: Clean NO_ASM build option with predictable performance

**Source:** [BLAKE3 Issue #537](https://github.com/BLAKE3-team/BLAKE3/issues/537) - NO_ASM flag request

**Source:** [BLAKE3 Issue #182](https://github.com/BLAKE3-team/BLAKE3/issues/182) - FreeBSD compilation

> "The BLAKE3 sources, as distributed, don't build a C library directly. For a straight C++ build with no asm, some fixing is needed."

**Source:** [BLAKE3 Issue #534](https://github.com/BLAKE3-team/BLAKE3/issues/534) - MSVC vs GCC performance differences

**Why it matters:**
- Assembly adds platform-specific complexity
- Different compilers produce wildly different performance from intrinsics
- Some environments (sandboxes, WASM, unusual architectures) cannot use assembly
- Audited environments may require pure C for review

**Requirements:**
- Single flag to disable all assembly (`-DBLAKE4_NO_ASM`)
- Portable intrinsics implementation with reasonable performance
- Consistent behavior across GCC/Clang/MSVC
- Clean CPU feature dispatch without runtime surprises

---

## E) Stable C API/ABI

### Requirement: Documented, stable ABI with versioning

**Source:** [ccache Discussion #503](https://github.com/ccache/ccache/discussions/503)

> "There is no official C-API, so the sources would have to be included directly in ccache."

**Source:** [BLAKE3 C README](https://github.com/BLAKE3-team/BLAKE3/blob/master/c/README.md)

The current approach expects callers to compile BLAKE3 sources directly into their projects.

**Why it matters:**
- Library inclusion in system packages requires stable ABI
- Dynamic linking requires symbol stability
- Language bindings need predictable function signatures
- Package managers need versioned releases

**Requirements:**
- Stable header with versioned symbols
- Explicit ABI compatibility promises
- Clean separation between public API and internal implementation
- pkg-config / CMake find_package support

---

## F) State Serialization for Resumable Hashing

### Requirement: Safe state serialization/deserialization API

**Source:** [BLAKE3 Issue #523](https://github.com/BLAKE3-team/BLAKE3/issues/523)

> "The user requests the ability to serialize and deserialize a BLAKE3 Hasher object's state before finalization, allowing the state to be loaded at a later time."
>
> Use case: "Proof of Data Possession/Proof of Retrievability system" - hash large files, store intermediate state, later append nonce and finalize.

**Why it matters:**
- Large file hashing across process restarts
- Distributed hashing where state must be transferred
- Proof-of-storage and content-addressed storage systems
- Checkpoint/resume for long-running operations

**Risks:**
- State serialization is footgun-prone
- Security implications of exposed internal state
- Versioning challenges if internal format changes

**Requirements:**
- Explicit, documented serialization format
- Version tag in serialized state
- Clear security guidance on when this is safe to use

---

## G) Verified Streaming / Proof-Carrying Hash Format

### Requirement: Standardized Merkle proof interchange format

**Source:** [Bao Repository](https://github.com/oconnor663/bao)

> "Bao enables verification of file portions without rehashing entire contents by storing file bytes alongside hash tree nodes in an encoded format."

**Why it matters:**
- Streaming verification for large files (video, backups)
- Random-access verification for distributed storage
- BitTorrent-like systems need standard proof format
- Content-addressed storage (IPFS, etc.) needs interoperability

**Current status:**
- Bao exists but is "beta cryptography software" without formal audit
- No universal standard for BLAKE3 tree proofs
- Different implementations may use incompatible formats

**Requirements:**
- Canonical chunk size specification (or explicit negotiation)
- Standard proof encoding format
- Clear domain separation for proof contexts
- Interoperability test vectors

---

## H) Hardware Implementation Considerations

### Requirement: Design amenable to efficient hardware implementation

**Source:** [BLAKE3 Issue #207](https://github.com/BLAKE3-team/BLAKE3/issues/207) - Hardware comparison

**Source:** [Springer - BLAKE3 FPGA Power Analysis](https://link.springer.com/chapter/10.1007/978-3-031-37720-4_27)

> "BLAKE cryptographic hash functions can be efficiently coded in software but their hardware realizations are not as fast and power-effective as those of alternative algorithms."

**Source:** [Springer - Dedicated FPGA Resources for BLAKE3](https://link.springer.com/chapter/10.1007/978-3-031-61857-4_28)

> "One method of decreasing power consumption in BLAKE3 FPGA implementations involves application of dedicated DSP resources for binary summations."

**Why it matters:**
- Hardware acceleration increasingly important (cloud, embedded, ASICs)
- SHA-3/Keccak sponge construction claimed to be more hardware-efficient
- Power consumption critical for embedded/IoT devices

**Considerations:**
- ARX construction has different hardware tradeoffs than sponge
- Tree hashing enables parallelism but adds overhead for small inputs
- Consider hardware-friendly variants for embedded profile

---

## I) ChaCha Core Reuse

### Requirement: Consider using standard ChaCha permutation

**Source:** [BLAKE3 Issue #535](https://github.com/BLAKE3-team/BLAKE3/issues/535)

> "Leveraging the standard ChaCha cryptographic primitive for BLAKE4. The rationale is enabling code reuse across a complete cryptosystem—encompassing hashing, key derivation, message authentication, and encryption—all built around a single core that is widely analyzed, deployed and easy to implement."

**Why it matters:**
- ChaCha is extremely well-analyzed
- Single primitive simplifies implementation and audit
- Already deployed in TLS (ChaCha20-Poly1305)
- Reduces attack surface by reusing vetted component

**Tradeoffs:**
- BLAKE3 already uses a ChaCha-derived permutation
- Strict ChaCha compatibility may constrain optimization
- May limit hardware-specific optimizations

---

## J) ZK-Friendly Variant (Separate Direction)

### Requirement: If pursuing ZK, accept it's a different algorithm

**Source:** Background knowledge on Poseidon/Rescue hashes

Traditional ARX constructions like BLAKE are expensive in ZK circuits:
- XOR and rotations require many constraints in arithmetic circuits
- ZK-friendly hashes (Poseidon, Rescue, Griffin) use algebraic operations
- Could be 100-1000x more efficient in constraint count

**Recommendation:**
- ZK-friendly hash should be a separate project, not "BLAKE4"
- Different security model, different analysis, different community
- Branding as "BLAKE" would be misleading if internals change fundamentally

---

## K) Side-Channel Hardening

### Requirement: Explicit constant-time guidance and hardened implementations

**Why it matters:**
- Keyed modes (MAC, KDF) involve secret data
- Embedded/hardware implementations face timing, cache, power attacks
- ARX is generally easier to harden than table-based ciphers

**Requirements:**
- Reference implementation must be constant-time
- Document expected timing behavior
- Provide guidance for hardware implementations
- Optional: masking-friendly variant for high-security embedded

---

## L) Round Count Concerns

### Requirement: Address the 7-round security margin question

**Source:** [Hacker News Discussion](https://news.ycombinator.com/item?id=38250398)

> "The biggest concern for many is BLAKE3 using only seven rounds, down from 12 in BLAKE2 and other hashes. This could mean BLAKE3 is less secure against future, currently unknown attacks."

**Why it matters:**
- BLAKE3's 7 rounds is aggressive compared to BLAKE2's 10-12
- Conservative users prefer larger security margins
- Future cryptanalysis may reduce effective security

**Options for BLAKE4:**
- Keep 7 rounds (proven sufficient, best performance)
- Offer configurable round count (flexibility, complexity)
- Increase to 10+ rounds for "conservative" profile

---

## Summary: Priority Ranking

| Priority | Requirement | Rationale |
|----------|-------------|-----------|
| High | 512-bit / post-quantum margin | Compliance driver, clear differentiation |
| High | Stable C API/ABI | Adoption blocker for many projects |
| High | Portable NO_ASM build | Build friction is real pain point |
| Medium | State serialization | Niche but vocal demand |
| Medium | Verified streaming format | Differentiator, clear new capability |
| Medium | Short input performance | Addresses known weakness |
| Low | FIPS certification path | Long process, may not be worth pursuing |
| Low | Hardware optimization | Important but design-constrained |
| Separate | ZK-friendly variant | Different algorithm, different project |

---

## Sources

- [BLAKE3 GitHub Repository](https://github.com/BLAKE3-team/BLAKE3)
- [BLAKE3 Issue #535 - BLAKE4 Wishlist](https://github.com/BLAKE3-team/BLAKE3/issues/535)
- [BLAKE3 Issue #523 - State Serialization](https://github.com/BLAKE3-team/BLAKE3/issues/523)
- [BLAKE3 Issue #537 - NO_ASM Flag](https://github.com/BLAKE3-team/BLAKE3/issues/537)
- [BLAKE3 IETF Draft](https://datatracker.ietf.org/doc/draft-aumasson-blake3/)
- [Bao Verified Streaming](https://github.com/oconnor663/bao)
- [BLAKE3 Short Input Performance](https://jszym.com/blog/short_input_hash/)
- [ccache BLAKE3 Discussion](https://github.com/ccache/ccache/discussions/503)
- [BLAKE3 FPGA Research](https://link.springer.com/chapter/10.1007/978-3-031-37720-4_27)
