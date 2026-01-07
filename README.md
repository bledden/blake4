# BLAKE4 Exploration

Exploratory research and design work for a hypothetical BLAKE4 hash function.

## Status

**Phase 1 Complete** - Requirements gathered, threat model defined, flagship profiles selected.

## Selected Direction

Based on community feedback and gap analysis:

- **Primary Flagship:** BLAKE4-PORTABLE - Stable ABI, clean builds, ecosystem adoption
- **Secondary Flagship:** BLAKE4-STREAM - Standardized verified streaming with Merkle proofs
- **Optional Extension:** 512-bit output mode for compliance/post-quantum margin

## Documents

| Document | Description |
|----------|-------------|
| [requirements.md](requirements.md) | Community-sourced requirements with citations |
| [threat_model.md](threat_model.md) | Adversary classes, security goals, attack vectors |
| [goals_non_goals.md](goals_non_goals.md) | What BLAKE4 aims to achieve and explicitly avoids |
| [decision_matrix.md](decision_matrix.md) | Evaluation of candidate profiles, flagship selection |
| [HANDOFF_PLAN.md](HANDOFF_PLAN.md) | Original handoff document with full roadmap |

## Key Findings

### Why BLAKE4?

BLAKE3 is excellent but has gaps:
1. No stable C ABI (sources compiled directly into projects)
2. Build friction with assembly/intrinsics
3. No standardized verified streaming format
4. State serialization not officially supported
5. Slower than BLAKE2s on short inputs
6. No 512-bit native output for compliance needs

### What BLAKE4 Is Not

- Not a ZK-friendly hash (fundamentally different design)
- Not a password hashing function (use Argon2)
- Not pursuing FIPS certification initially
- Not replacing BLAKE3 for all use cases

## Next Steps (Phase 2)

1. Design candidates for PORTABLE (C API, build system, short-input optimization)
2. Design candidates for STREAM (chunk size, proof format, verification semantics)
3. Prototype reference implementation
4. Generate test vectors
5. Security analysis documentation

## Sources

Key references from discovery scan:
- [BLAKE3 GitHub Issues](https://github.com/BLAKE3-team/BLAKE3/issues)
- [BLAKE3 Issue #535 - BLAKE4 Wishlist](https://github.com/BLAKE3-team/BLAKE3/issues/535)
- [Bao Verified Streaming](https://github.com/oconnor663/bao)
- [BLAKE3 IETF Draft](https://datatracker.ietf.org/doc/draft-aumasson-blake3/)
- [Short Input Performance Analysis](https://jszym.com/blog/short_input_hash/)
