# BLAKE4: Comprehensive Handoff Plan (Working Document)

> **Purpose:** This document is a structured handoff for another model or team to continue exploring (and potentially designing) a hypothetical **BLAKE4** hash function or family.  
> It consolidates **possible design directions**, **themes from community discussions**, and a **step-by-step roadmap** covering research, design, implementation, evaluation, and standardization.

---

## 0) Executive Summary

BLAKE3 (released ~2020) is already a highly optimized general-purpose hash framework (hash/MAC/KDF/XOF) with a Merkle-tree construction for parallelism. A “BLAKE4” only makes sense if it:

1) **Solves a problem BLAKE3 does not fully solve**, or  
2) **Delivers materially better security margin / compliance fit**, or  
3) **Becomes materially easier to adopt, verify, and deploy** across languages/hardware/certification contexts, or  
4) **Targets a distinct domain** (ZK proofs, streaming verifiability format standardization, embedded/energy constraints).

Most credible directions cluster into **profiles/variants** rather than a single monolithic successor. A realistic “BLAKE4 program” would therefore:

- Define **requirements** from real-world constraints (standards, hardware, portability, side-channels, packaging)
- Select **1–2 flagship variants** to avoid fragmentation
- Provide **reference implementations**, **test vectors**, **benchmarks**, and **security analysis plan**
- Consider **standardization** from day one (spec clarity, stable ABI, interoperability, metadata and domain separation rules)

---

## 1) Context & Non-Goals

### Context
- **BLAKE** → SHA-3 finalist lineage  
- **BLAKE2** → widely used, fast, simple  
- **BLAKE3** → major redesign: tree hash, parallel, XOF, unified modes

### Non-goals (avoid scope creep)
- Creating “BLAKE4” *only* as a rename of BLAKE3
- Shipping a new primitive without **clear threat model**
- Publishing a design without **test vectors + reference code**
- Offering too many parameters/modes such that interoperability is undermined
- Making claims like “post-quantum safe” without carefully defining what is meant

---

## 2) Requirements Backlog (From Community Themes)

This is the **starting backlog**—the “why” behind BLAKE4.

### A) Higher security margin / “PQ-ish” positioning
- Desire for **512-bit outputs** and/or **256-bit collision security** (comfort margin and compliance posture)
- Potentially stronger internal capacity/state or more conservative round counts

**Risks:** performance, bandwidth/storage overhead, “unnecessary” for many users.

### B) Standards and certification reality (adoption constraint)
- Many environments require **standardized / certified** algorithms (e.g., FIPS-like).
- Even if technically superior, an algorithm may be blocked without a clear standardization path.

**Implication:** A BLAKE4 effort should decide whether it aims for:
- “best practical hash” (non-certified) **or**
- “standardization-first” (more conservative, more process heavy)

### C) Side-channel and hardening expectations
- Some practitioners prefer permutations/ciphers that are easier to harden (or perceive ARX as harder).
- BLAKE4 may need:
  - explicit constant-time guidance
  - hardened reference paths
  - optional masking-friendly variant (if hardware/embedded focus)

### D) Portability & build friction
- Demand for:
  - **no-assembly build option** (`NO_ASM`)
  - predictable performance across compilers (MSVC/GCC/Clang)
  - clean CPU-feature dispatch
  - small-footprint portable implementation

### E) Streaming verification and interoperable Merkle proofs
- BLAKE3 enables tree hashing, but **interchange formats** for verified streaming are not universal.
- Desire for standardized “proof-carrying” hash formats (chunk verification, random access)

### F) Stateful/resumable hashing interchange
- Demand for canonical, safe **state serialization** (to resume hashing across processes / store partial hash state)
- This is footgun-prone without strict rules.

### G) Ecosystem adoption basics
- Stable C API/ABI
- Easy bindings
- Standard-library inclusion targets
- Tooling (benchmarks, test harness, fuzzing)

---

## 3) Candidate Design Directions (Options + Pros/Cons)

> **Important:** Treat these as **profiles** unless you have a compelling reason to unify.

### Direction 1: BLAKE4-512 (High-Security / Large-Digest Profile)
**Core idea:** Keep BLAKE3 philosophy but scale parameters for a stronger margin (e.g., bigger state/capacity, 512-bit default digest).

**Pros**
- Simple story for “extra margin”
- Familiar API shape (hash/MAC/KDF/XOF still possible)
- Likely easiest “incremental successor”

**Cons**
- Larger hashes increase bandwidth/storage
- If it’s just “BLAKE3 but bigger,” some will ask why XOF output from BLAKE3 isn’t enough
- Careful analysis needed if internal structure changes

**Best for:** long-term integrity, compliance-driven orgs, conservative security posture.

---

### Direction 2: BLAKE4-PORTABLE (Portable-First, Fewer ASM Targets)
**Core idea:** Prioritize predictable performance and deployability over peak speed. Reduce assembly complexity; emphasize portable intrinsics and stable ABI.

**Pros**
- Big adoption win (toolchains, multi-arch, “no asm” builds)
- Better for audited environments
- Fewer platform-specific pitfalls

**Cons**
- Might not beat BLAKE3 on x86 with hand-optimized asm
- Hard to make “sexy” claims; mostly engineering

**Best for:** build systems, package managers, language runtimes, general library inclusion.

---

### Direction 3: BLAKE4-STREAM (Standard Midstream Verification / Proof-Carrying Hash)
**Core idea:** Standardize tree-hash chunk verification: define canonical chunking, node hashing, and proof encoding.

**Pros**
- Huge for distributed systems, content-addressed storage, P2P
- Enables verify-as-you-download, random access verification
- Differentiates clearly from “just another hash”

**Cons**
- Needs careful spec to avoid misuse
- Increases API/format complexity
- Proof overhead (usually modest, but nonzero)

**Best for:** content distribution, storage, replication, verifiable data pipelines.

---

### Direction 4: BLAKE4-ZK (Proof-System-Friendly Hash Variant)
**Core idea:** A ZK-friendly “BLAKE4” variant that is cheap inside SNARK/STARK circuits (arithmetization-friendly).

**Pros**
- Clear niche with massive demand (blockchains, ZK apps)
- Real performance wins in proof generation

**Cons**
- Likely not ARX / not “BLAKE-like” internally
- Won’t be a general-purpose replacement
- Security analysis is specialized; standardization harder

**Best for:** ZK applications, rollups, privacy systems.

---

### Direction 5: BLAKE4-LITE (Energy/Embedded Profile)
**Core idea:** Minimize energy per byte and memory footprint; predictable constant-time code; microcontroller-friendly.

**Pros**
- Strong security for IoT without resorting to weak checksums
- Can be a “BLAKE2s-for-the-2020s” concept but energy-driven

**Cons**
- Gains may require reducing state/rounds → careful security review
- Embedded designs must also handle side-channel realities

**Best for:** IoT, firmware verification, battery-powered secure logging.

---

### Direction 6: BLAKE4-AGILE (Framework + Negotiable Modes)
**Core idea:** A “hash framework” with explicit versioning, mode negotiation, and standardized domain separation conventions.

**Pros**
- Future-proofing; easier algorithm rollovers
- Consolidates hash/MAC/KDF/XOF under one umbrella
- Encourages correct usage patterns

**Cons**
- Risk of interoperability fragmentation
- Spec complexity; more room for mistakes
- Standards bodies may resist “too flexible”

**Best for:** protocols, large orgs, long-lived systems.

---

## 4) Recommended Strategy: Pick 1–2 Flagship Targets

Avoid building “six BLAKE4s.” Choose a **flagship** and treat others as optional.

### Suggested flagship candidates
- **Flagship A:** BLAKE4-PORTABLE (adoption and engineering wins)  
- **Flagship B:** BLAKE4-STREAM (new capabilities)  
- Optional extension: BLAKE4-512 (if you want stronger margin as a clear selling point)

ZK and Embedded variants are valuable but likely deserve **distinct branding** to avoid confusing general users.

---

## 5) Compatibility & Migration Plan (If Desired)

Compatibility means multiple things; define which you want:

### A) API compatibility
- Provide a BLAKE3-like interface (init/update/finalize)
- Keep modes: hash, keyed hash/MAC, derive key/KDF, XOF
- Preserve “one library does it all” philosophy

### B) Output compatibility (rarely feasible)
- Generally impossible if the algorithm changes meaningfully.
- Instead: offer a **BLAKE3 compatibility mode** in the same library.

### C) Behavioral compatibility
- Preserve incremental hashing semantics
- Preserve concurrency patterns
- Preserve deterministic output given same inputs + parameters

### D) Wire-format compatibility (for STREAM profile)
- Provide a standard encoded format for proofs/chunks and document versioning.

---

## 6) Security Goals & Threat Model Checklist

The next model/team should fill this in explicitly before any design choices.

### Baseline security goals
- Collision resistance (target strength X)
- Preimage resistance
- Second-preimage resistance
- PRF security for keyed mode (if supported)
- XOF indistinguishability (if supported)
- Domain separation correctness (no cross-mode attacks)

### Threat models to decide early
- Classical vs quantum adversary
- Side-channel attacker: timing, cache, power (embedded/hardware)
- Fault attacks (glitching) if embedded/hardware is a target
- Malicious input streams / length ambiguity / chunk boundary ambiguity

---

## 7) Evaluation & Decision Matrix

Define an explicit scoring rubric so design choices are defensible.

### Suggested dimensions (score 1–5)
1. Security margin / conservativeness  
2. Performance: scalar CPU  
3. Performance: SIMD / multithread scaling  
4. Performance: GPU friendliness (optional)  
5. Hardware efficiency (area/throughput/power)  
6. Portability (no-asm, compilers, arches)  
7. Implementation complexity  
8. Side-channel hardening feasibility  
9. Ecosystem adoption likelihood (APIs, libs, licensing)  
10. Standardization feasibility  

### Benchmark suite requirements
- Short messages, long streams, chunked hashing
- Keyed mode throughput
- XOF throughput
- Multi-thread scaling curves
- CPU families: x86_64, ARM64, RISCV (if possible)
- Compiler matrix: clang/gcc/msvc
- “No asm” baseline vs optimized

---

## 8) Work Plan & Milestones (Phase-Based)

> Replace dates with “Week 1/2/3…” if the next model prefers.

### Phase 0 — Discovery & Discussion Scan (Requirements Expansion)
**Goal:** Expand backlog from real-world sources; gather citations and links.

**Tasks**
- Search for discussions: BLAKE4, BLAKE3 future, “BLAKE3 hardware,” “Bao verified streaming,” “BLAKE3 no asm,” “BLAKE3 C API,” “BLAKE3 side channel,” “BLAKE3 MSVC perf”
- Scan:
  - BLAKE3 repo issues/discussions
  - cryptography forums / StackExchange / r/cryptography
  - NIST / IETF drafts and discussions about hashing and PQ migration
  - hardware implementation papers (FPGA/ASIC BLAKE3)
  - ZK hash literature (Poseidon/Rescue/Griffin/etc.)

**Deliverable**
- `requirements.md` with categorized wishlist + links + quotes (short snippets) + “why it matters”

---

### Phase 1 — Threat Model + Goals (Lock the “Why”)
**Goal:** Decide which flagship direction(s) to pursue and define success criteria.

**Deliverables**
- `threat_model.md`
- `goals_non_goals.md`
- `decision_matrix.md` (scoring rubric + chosen variants)

Decision gate: **Select 1–2 flagship profiles**.

---

### Phase 2 — Design Sketches (3–5 Candidates per Flagship)
**Goal:** Produce multiple design options and down-select.

**For BLAKE4-PORTABLE**
- Candidate A: BLAKE3-compatible core but new dispatch / intrinsics strategy
- Candidate B: fewer rounds vs more rounds tradeoff (if any)
- Candidate C: restructure compression for fewer 64-bit adds on some targets
- Candidate D: unify C ABI + stable symbols + minimal dependencies

**For BLAKE4-STREAM**
- Candidate A: standard proof format for fixed chunk size (e.g., 1 KiB / 16 KiB / 1 MiB)
- Candidate B: variable chunk size with explicit metadata (avoid ambiguity)
- Candidate C: define “stream root” + “chunk proof” encoding and canonicalization rules

**Deliverables**
- `design_candidates.md` (each candidate: spec sketch, rationale, expected performance, risks)

Decision gate: pick **one candidate per flagship** to prototype.

---

### Phase 3 — Prototype Implementations (Reference + Optimized)
**Goal:** Produce working code, test vectors, and minimal docs.

**Deliverables**
- Reference implementation (correctness-first)
- Optimized implementation (intrinsics, optional asm)
- Stable C API (headers + ABI guidelines)
- Rust crate (if relevant) with identical test vectors
- `test_vectors/` + generator scripts
- `README.md` usage and security notes
- Fuzz harness + differential tests vs reference

---

### Phase 4 — Cryptanalysis & Review Plan
**Goal:** Structure the security validation work and invite scrutiny.

**Tasks**
- Internal review: known ARX / permutation analyses, rotational/differential attacks
- For STREAM: tree-hash security reasoning, domain separation checks, ambiguity checks
- For AGILE: mode separation, misuse resistance analysis

**Deliverables**
- `security_notes.md` (claims, assumptions, known limits)
- `attack_surface_checklist.md`
- Public “call for cryptanalysis” document if open-sourcing

---

### Phase 5 — Benchmarking & Comparison Report
**Goal:** Produce reproducible performance numbers and tradeoff explanations.

**Deliverables**
- `benchmarks.md` with methodology and results
- scripts to reproduce
- comparisons:
  - BLAKE3
  - BLAKE2 variants
  - SHA-2 / SHA-3 / SHAKE
  - KangarooTwelve/TurboSHAKE (if relevant)
  - ZK-friendly hashes if pursuing ZK

---

### Phase 6 — Standardization & Adoption Strategy (If Pursued)
**Goal:** Choose a standardization path or explicitly opt out.

**Options**
- IETF Internet-Draft / RFC-style spec
- CFRG interest (if applicable)
- Document why not pursuing certification (if that’s the choice)

**Deliverables**
- `spec.md` (complete normative spec)
- `interop.md` (wire format, domain separation, versioning)
- `migration_guide.md` (from BLAKE3 and others)
- `compliance_notes.md` (clear positioning)

---

## 9) Specification Outline (Template)

A future `spec.md` should include:

1. Terminology and notations  
2. Parameters (digest sizes, chunk size, keyed mode, personalization)  
3. Core permutation / compression function definition  
4. Tree hashing rules (if applicable)  
5. Domain separation tags / flags (normative)  
6. Modes:
   - hash
   - MAC / keyed hash
   - KDF
   - XOF
   - STREAM proofs (if applicable)
7. Test vectors (normative)
8. Security considerations and known limits
9. Implementation notes (constant-time, memory, etc.)

---

## 10) Risks & Mitigations (Risk Register)

### Risk: “BLAKE4 is redundant”
- **Mitigation:** pick a flagship that BLAKE3 doesn’t fully solve (STREAM format, portability/certification path, 512-bit story)

### Risk: fragmentation from too many profiles
- **Mitigation:** ship 1 flagship + 1 optional profile; clearly brand others as separate variants

### Risk: misuse due to complex API (especially STREAM/state serialization)
- **Mitigation:** safe defaults, strong documentation, misuse-resistant interfaces, canonical encoding rules

### Risk: side-channel issues in embedded/hardware
- **Mitigation:** constant-time reference, optional hardened implementation, guidance for masking/hardware pipelines

### Risk: no adoption without standardization
- **Mitigation:** decide early whether standardization is a goal; invest accordingly

---

## 11) Concrete Next-Model Instructions (Copy/Paste Prompt)

> Use this prompt to hand off to another model:

**PROMPT**
You are continuing a “BLAKE4 exploration” project. Your tasks:

1) Perform a broad web scan for current discussions on: BLAKE4, BLAKE3 future, BLAKE3 hardware/FPGA/ASIC, BLAKE3 portability/no-asm, BLAKE3 C API/ABI requests, BLAKE3 side-channel concerns, Bao verified streaming format, post-quantum hash size guidance, and ZK-friendly hash comparisons (Poseidon/Rescue/etc.).
2) Expand `requirements.md` with citations/links and categorize requirements.
3) Propose 2 flagship directions for a useful BLAKE4 (e.g., PORTABLE and STREAM, optionally 512-bit), with a decision matrix and explicit threat model.
4) Draft a `design_candidates.md` with 3–5 candidates per flagship; include spec sketches, compatibility strategy, and a benchmark plan.
5) Output a minimal `spec.md` skeleton and a `test_vectors.md` plan.

Focus on pragmatic usefulness (deployability + a clear differentiator from BLAKE3). Avoid excessive parameterization. Explicitly document tradeoffs.

---

## 12) Appendix: Minimal Deliverables Checklist

- [ ] requirements.md (with links/citations)  
- [ ] threat_model.md  
- [ ] decision_matrix.md  
- [ ] design_candidates.md  
- [ ] spec.md (normative draft)  
- [ ] reference implementation (C or Rust)  
- [ ] stable C API + ABI notes  
- [ ] test vectors + generator  
- [ ] fuzzing + differential tests  
- [ ] benchmark suite + reproducible scripts  
- [ ] security_notes.md + call for review  

---

## 13) Notes on Naming & Messaging

- Treat **“BLAKE4” as a working name** until there’s a coherent spec and buy-in.
- Consider “B4” / “BLAKE-next” internally.
- Avoid grand claims; emphasize measured goals (portability, streaming verification format, larger security margin).
- If collaborating with existing BLAKE authors is possible, do it early (but design should still stand independently).

---

*End of document.*
