# BLAKE4 Threat Model

> Defines the adversaries, attack vectors, and security goals that BLAKE4 must address.

---

## 1. Adversary Classes

### 1.1 Classical Adversary

A computationally bounded adversary with access to:
- Arbitrary chosen inputs
- Multiple hash outputs
- Timing information (if implementation is not constant-time)
- Cache behavior (if running on shared hardware)

**Capabilities:**
- Up to 2^128 computational operations (practical limit)
- Parallel computation via GPU/ASIC farms
- Known cryptanalytic techniques (differential, linear, rotational)

### 1.2 Quantum Adversary (Future)

An adversary with access to a large-scale fault-tolerant quantum computer.

**Capabilities:**
- Grover's algorithm: Reduces brute-force from O(2^n) to O(2^(n/2))
- For collision attacks: Reduces from O(2^(n/2)) to O(2^(n/3)) (BHT algorithm)
- No known quantum speedup for differential/linear cryptanalysis

**Implications:**
- 256-bit hash provides ~128 bits of quantum preimage security
- 256-bit hash provides ~85 bits of quantum collision security
- 512-bit hash provides ~256 bits of quantum preimage security
- 512-bit hash provides ~170 bits of quantum collision security

### 1.3 Side-Channel Adversary

An adversary who can observe:
- Execution timing
- Cache access patterns
- Power consumption
- Electromagnetic emissions

**Relevant for:**
- Keyed modes (MAC, PRF, KDF) where key is secret
- Hardware/embedded implementations
- Shared cloud environments

### 1.4 Malicious Input Adversary

An adversary who controls input to the hash function and wants to:
- Cause denial of service (algorithmic complexity attacks)
- Exploit length extension
- Create ambiguous inputs across modes

---

## 2. Security Goals

### 2.1 Collision Resistance

**Definition:** Computationally infeasible to find two distinct inputs m1 ≠ m2 such that H(m1) = H(m2).

**Target strength:**
- Classical: 2^(n/2) operations for n-bit output
- BLAKE4-256: 2^128 classical collision resistance
- BLAKE4-512: 2^256 classical collision resistance

**Quantum consideration:**
- BLAKE4-512 preferred for long-term collision resistance

### 2.2 Preimage Resistance

**Definition:** Given a hash output h, computationally infeasible to find any input m such that H(m) = h.

**Target strength:**
- Classical: 2^n operations for n-bit output
- BLAKE4-256: 2^256 classical preimage resistance
- BLAKE4-512: 2^512 classical preimage resistance

### 2.3 Second Preimage Resistance

**Definition:** Given an input m1, computationally infeasible to find m2 ≠ m1 such that H(m1) = H(m2).

**Target strength:** Same as preimage resistance for ideal hash function.

### 2.4 PRF Security (Keyed Mode)

**Definition:** For a random key k, BLAKE4(k, m) is computationally indistinguishable from a random function.

**Requirements:**
- No key recovery faster than brute force
- No distinguisher with advantage better than birthday bound
- Constant-time implementation required

### 2.5 XOF Indistinguishability

**Definition:** The extendable output should be indistinguishable from a random oracle.

**Requirements:**
- No correlation between output segments
- No distinguisher from true random stream

### 2.6 Domain Separation Correctness

**Definition:** Different modes (hash, MAC, KDF, XOF) produce independent outputs.

**Requirements:**
- BLAKE4_hash(m) ≠ BLAKE4_mac(k, m) for any k, m
- BLAKE4_kdf(context1, m) ≠ BLAKE4_kdf(context2, m) for context1 ≠ context2
- No cross-mode attacks possible

---

## 3. Attack Vectors

### 3.1 Cryptanalytic Attacks

| Attack | Applicability | Mitigation |
|--------|---------------|------------|
| Differential cryptanalysis | Core permutation | Sufficient rounds, proven bounds |
| Linear cryptanalysis | Core permutation | Sufficient rounds, proven bounds |
| Rotational cryptanalysis | ARX constructions | Asymmetric rotations, constants |
| Rebound attacks | Compression function | Sufficient rounds in middle |
| Length extension | Merkle-Damgård variants | Tree hashing prevents this |

**Note:** BLAKE3's 7-round compression is analyzed against these. BLAKE4 should either:
- Retain 7 rounds with equivalent analysis
- Increase rounds for additional margin

### 3.2 Implementation Attacks

| Attack | Scenario | Mitigation |
|--------|----------|------------|
| Timing attacks | Keyed modes | Constant-time implementation |
| Cache timing | Shared hardware | No secret-dependent memory access |
| Power analysis | Embedded/hardware | Optional masking, DPA resistance |
| Fault injection | High-security hardware | Redundant computation, detection |

### 3.3 Protocol-Level Attacks

| Attack | Scenario | Mitigation |
|--------|----------|------------|
| Length extension | Raw hash misuse | Tree structure prevents |
| Related-key attacks | KDF with related contexts | Strong domain separation |
| Collision-based forgery | MAC with weak keys | PRF security assumption |
| Multi-target attacks | Many hashes of same structure | Standard security reduction |

### 3.4 Tree Hashing Specific Attacks

| Attack | Scenario | Mitigation |
|--------|----------|------------|
| Chunk boundary manipulation | Malicious encoder | Canonical chunking rules |
| Proof forgery | Verified streaming | Proper tree commitment |
| Ambiguous tree structure | Different chunk sizes | Explicit metadata encoding |

---

## 4. Threat Scenarios by Profile

### 4.1 BLAKE4-PORTABLE (General Purpose)

**Primary threats:**
- Cryptanalytic attacks on compression function
- Implementation bugs leading to incorrect output
- Build/compilation issues causing silent failures

**Not primary concerns:**
- Side-channel attacks (no secrets in basic hashing)
- Quantum attacks (256-bit sufficient for most uses)

### 4.2 BLAKE4-512 (High Security)

**Primary threats:**
- Long-term cryptanalysis advances
- Quantum computer development
- Compliance/regulatory requirements

**Design response:**
- Larger state/capacity
- Possibly more rounds
- Conservative parameter choices

### 4.3 BLAKE4-STREAM (Verified Streaming)

**Primary threats:**
- Proof forgery attacks
- Chunk boundary ambiguity
- Malicious data injection during streaming
- Incomplete proof verification

**Design response:**
- Canonical proof encoding
- Explicit domain separation
- Clear verification failure semantics

### 4.4 BLAKE4-MAC (Keyed Mode)

**Primary threats:**
- Key recovery via side channels
- Timing-based distinguishers
- Related-key attacks

**Design response:**
- Mandatory constant-time implementation
- Strong key scheduling
- Clear key size requirements (256-bit minimum)

---

## 5. Non-Threats (Explicit Exclusions)

### 5.1 Password Hashing

BLAKE4 is **not** designed for password hashing.

**Reason:** Fast hashes are unsuitable for low-entropy inputs. Use Argon2, bcrypt, or scrypt instead.

### 5.2 Post-Quantum Signatures

BLAKE4 does not provide post-quantum signature security.

**Reason:** Hash-based signatures (SPHINCS+, etc.) have separate requirements.

### 5.3 Encryption

BLAKE4 is not an encryption primitive.

**Reason:** Use ChaCha20-Poly1305, AES-GCM, or similar AEAD constructions.

---

## 6. Security Assumptions

### 6.1 Compression Function

The BLAKE4 compression function (derived from ChaCha permutation) is assumed to be:
- A secure pseudorandom function when keyed
- Collision-resistant when used in Merkle-Damgård or tree mode
- Free of structural weaknesses after N rounds (N TBD)

### 6.2 Tree Construction

The Merkle tree construction is assumed to:
- Preserve collision resistance of underlying compression
- Provide secure domain separation between internal and leaf nodes
- Not introduce vulnerabilities beyond the compression function

### 6.3 Implementation

Correct implementations are assumed to:
- Follow the specification exactly
- Use constant-time operations for keyed modes
- Not introduce side channels through optimization

---

## 7. Open Questions for Design Phase

1. **Round count:** Is 7 sufficient, or should BLAKE4 increase to 10+ for margin?

2. **State size:** Is 512-bit internal state sufficient for 512-bit output security?

3. **Tree structure:** Should BLAKE4-STREAM use a different tree structure than BLAKE3?

4. **Key schedule:** Does the keyed mode need stronger key derivation?

5. **Side-channel profile:** Should constant-time be mandatory for all modes, or only keyed?

---

## 8. Versioning and Agility

BLAKE4 should include:
- Version identifier in domain separation
- Clear deprecation path if vulnerabilities found
- Explicit algorithm identifier for protocol negotiation

This enables:
- Smooth migration from BLAKE3
- Future updates without breaking existing deployments
- Clear signaling of security level
