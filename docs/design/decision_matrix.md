# BLAKE4 Decision Matrix

> Evaluates candidate design directions and selects flagship variants.

---

## Evaluation Dimensions

Each dimension scored 1-5:
- **1** = Poor / Major concern
- **3** = Adequate / Neutral
- **5** = Excellent / Strong advantage

---

## Candidate Profiles

### Profile A: BLAKE4-PORTABLE

**Focus:** Deployability, stable ABI, clean builds, ecosystem adoption

**Changes from BLAKE3:**
- Stable versioned C API/ABI
- First-class NO_ASM build path
- Improved short-input performance
- pkg-config and CMake find_package support
- Documented intrinsics strategy for consistent cross-compiler performance

### Profile B: BLAKE4-STREAM

**Focus:** Standardized verified streaming with Merkle proofs

**Changes from BLAKE3:**
- Canonical chunk size specification
- Standard proof encoding format
- Domain separation for proof contexts
- Interoperability test vectors
- Clear verification failure semantics

### Profile C: BLAKE4-512

**Focus:** Higher security margin for compliance/post-quantum

**Changes from BLAKE3:**
- 512-bit output as default
- Larger internal state (possibly 1024-bit)
- Possibly increased round count (10+)
- Conservative parameter choices

### Profile D: BLAKE4-LITE

**Focus:** Embedded/IoT, energy efficiency, small inputs

**Changes from BLAKE3:**
- Optimized for small inputs (<1KB)
- Minimal memory footprint
- Energy-efficient design
- 32-bit friendly

### Profile E: BLAKE4-AGILE

**Focus:** Framework with negotiable modes and versioning

**Changes from BLAKE3:**
- Explicit version negotiation
- Algorithm agility built-in
- Standardized mode identifiers

---

## Scoring Matrix

| Dimension | PORTABLE | STREAM | 512 | LITE | AGILE |
|-----------|----------|--------|-----|------|-------|
| Security margin | 3 | 3 | 5 | 2 | 3 |
| Performance (large inputs) | 5 | 4 | 3 | 3 | 4 |
| Performance (small inputs) | 4 | 3 | 3 | 5 | 3 |
| Performance (SIMD/parallel) | 5 | 5 | 4 | 2 | 4 |
| Portability | 5 | 4 | 4 | 4 | 3 |
| Implementation complexity | 4 | 3 | 3 | 4 | 2 |
| Side-channel hardening | 3 | 3 | 3 | 3 | 3 |
| Ecosystem adoption likelihood | 5 | 4 | 3 | 3 | 2 |
| Differentiation from BLAKE3 | 3 | 5 | 4 | 3 | 3 |
| Standardization feasibility | 4 | 4 | 4 | 3 | 2 |
| **TOTAL** | **41** | **38** | **36** | **32** | **29** |

---

## Analysis by Profile

### BLAKE4-PORTABLE (Score: 41)

**Strengths:**
- Addresses real pain points (build friction, ABI stability)
- Highest adoption likelihood
- Lowest implementation risk
- Builds on proven BLAKE3 design

**Weaknesses:**
- Least differentiated from BLAKE3
- May be seen as "just BLAKE3 with better packaging"
- Doesn't add new capabilities

**Verdict:** Strong flagship candidate. Solves practical problems.

### BLAKE4-STREAM (Score: 38)

**Strengths:**
- Clear new capability (standardized proofs)
- Growing demand (distributed storage, P2P, content-addressed)
- Builds on Bao's proven concept
- Strong differentiation

**Weaknesses:**
- More complex specification
- Niche audience (not everyone needs streaming verification)
- Requires careful security analysis of proof format

**Verdict:** Strong flagship candidate. Provides new value.

### BLAKE4-512 (Score: 36)

**Strengths:**
- Clear security story
- Compliance-driven demand
- Simple to explain

**Weaknesses:**
- Narrow audience
- Performance penalty
- Could be an option within PORTABLE, not separate profile

**Verdict:** Better as optional parameter within PORTABLE, not standalone flagship.

### BLAKE4-LITE (Score: 32)

**Strengths:**
- Addresses short-input weakness
- IoT/embedded demand exists

**Weaknesses:**
- Different optimization targets than main BLAKE4
- May require different internal design
- Limited audience

**Verdict:** Deprioritize. Consider as future extension if demand materializes.

### BLAKE4-AGILE (Score: 29)

**Strengths:**
- Future-proofing
- Algorithm negotiation is valuable for protocols

**Weaknesses:**
- Complexity increases risk
- Interoperability harder
- Standards bodies resist excessive flexibility

**Verdict:** Out of scope. Agility should be in protocols, not in the hash itself.

---

## Flagship Selection

### Primary Flagship: BLAKE4-PORTABLE

**Rationale:**
- Highest overall score
- Addresses most frequently cited pain points
- Lowest risk, highest adoption likelihood
- Foundation for other profiles

### Secondary Flagship: BLAKE4-STREAM

**Rationale:**
- Clear differentiation from BLAKE3
- New capability, not just polish
- Growing market demand
- Can be built on PORTABLE foundation

### Optional Extension: 512-bit Mode

**Rationale:**
- Include as parameter option within PORTABLE
- Not a separate profile to avoid fragmentation
- Satisfies compliance needs without complexity

---

## Recommended Architecture

```
BLAKE4
├── BLAKE4-PORTABLE (flagship)
│   ├── Default: 256-bit output
│   ├── Option: 512-bit output
│   ├── Modes: hash, mac, kdf, xof
│   └── Features: stable ABI, NO_ASM, state serialization
│
└── BLAKE4-STREAM (flagship)
    ├── Built on PORTABLE core
    ├── Adds: proof encoding, chunk verification
    └── Features: canonical format, test vectors
```

---

## Implementation Order

1. **Phase 1:** PORTABLE core with 256-bit default
2. **Phase 2:** Add 512-bit option to PORTABLE
3. **Phase 3:** STREAM profile on top of PORTABLE
4. **Phase 4:** State serialization API
5. **Future:** LITE profile if demand emerges

---

## Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| BLAKE3 team doesn't endorse | Medium | High | Design independently, seek collaboration later |
| No adoption beyond niche | Medium | High | Focus PORTABLE on real pain points |
| Security issue discovered | Low | Critical | Conservative design, invite cryptanalysis |
| STREAM format incompatibility | Medium | Medium | Extensive test vectors, reference implementations |
| Build complexity despite goals | Medium | Medium | CI on multiple platforms from day one |

---

## Next Steps

1. **Design candidates for PORTABLE:** Spec out C API, build system, short-input optimization
2. **Design candidates for STREAM:** Define chunk size, proof format, verification semantics
3. **Prototype:** Reference implementation in C + Rust
4. **Test vectors:** Generate comprehensive test suite
5. **Security review:** Document assumptions, invite analysis
