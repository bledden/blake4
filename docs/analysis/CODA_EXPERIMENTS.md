# BLAKE4 Quantum Experiments for Coda

**Date**: January 8, 2026
**Purpose**: Experimental verification of BLAKE4 quantum security claims
**Reference**: See QUANTUM_ANALYSIS.md for full theoretical analysis

---

## Summary

All theoretical quantum analysis is complete. The following experiments require quantum circuit simulation to verify our theoretical estimates.

**Key theoretical findings to verify:**
- ~4,000 T-gates per G function
- ~200,000-400,000 T-gates per compression function
- Standard Grover O(√N) speedup applies (no shortcuts)
- No exploitable algebraic structure in ARX operations

---

## Experiment 1: G Function Quantum Circuit

### Task
Implement the BLAKE4 G function as a reversible quantum circuit.

### G Function Definition
```
G(a, b, c, d, mx, my):
    a = a + b + mx      // 64-bit modular addition
    d = (d ^ a) >>> 32  // XOR then rotate right 32
    c = c + d           // 64-bit modular addition
    b = (b ^ c) >>> 24  // XOR then rotate right 24
    a = a + b + my      // 64-bit modular addition
    d = (d ^ a) >>> 16  // XOR then rotate right 16
    c = c + d           // 64-bit modular addition
    b = (b ^ c) >>> 63  // XOR then rotate right 63
```

### Measurements Required
1. **T-gate count**: How many T-gates for one G function call?
   - Theoretical estimate: ~4,000
   - Need: Actual count from circuit implementation

2. **Circuit depth**: What is the critical path length?

3. **Ancilla qubits**: How many ancilla qubits needed for reversibility?

### Expected Output
```
G function circuit:
- T-gates: [actual count]
- Total gates: [count]
- Depth: [count]
- Ancilla qubits: [count]
- Comparison to estimate: [% difference from 4,000]
```

---

## Experiment 2: Full Compression Function Oracle

### Task
Implement BLAKE4's full compression function as a Grover oracle.

### Compression Function Structure
- 10 rounds
- Each round: 8 G function calls (4 column + 4 diagonal)
- Input: 512-bit state + 1024-bit message block + 128-bit counter/flags
- Output: 512-bit state

### Measurements Required
1. **Total T-gates**: For complete compression function
   - Theoretical estimate: 200,000-400,000
   - Components: 80 G calls × ~4,000 + overhead

2. **Qubit requirements**:
   - Working qubits for state
   - Ancilla for reversible additions
   - Total count

3. **Oracle overhead**: Additional gates for Grover oracle construction

### Expected Output
```
Compression function oracle:
- T-gates (forward): [count]
- T-gates (with uncomputation): [count]
- Total qubits: [count]
- Circuit depth: [count]
- Comparison to estimate: [% difference from 300k]
```

---

## Experiment 3: Reduced-Round Analysis

### Task
Analyze quantum advantage on reduced-round BLAKE4.

### Approach
1. Implement 1, 2, 3, 4-round versions
2. For each, measure:
   - Grover iteration cost
   - Actual vs theoretical speedup
   - Point where quantum advantage becomes negligible

### Key Question
At what round count does the quantum circuit overhead make Grover's speedup negligible compared to classical parallel search?

### Expected Output
```
Round analysis:
| Rounds | T-gates | Grover speedup | Practical advantage |
|--------|---------|----------------|---------------------|
| 1      | [count] | [√N]           | [yes/no]            |
| 2      | [count] | [√N]           | [yes/no]            |
| 3      | [count] | [√N]           | [yes/no]            |
| 4      | [count] | [√N]           | [yes/no]            |
| 10     | [count] | [√N]           | [yes/no]            |

Threshold: Quantum advantage becomes negligible at [X] rounds
```

---

## Experiment 4: Collision Search Simulation (BHT)

### Task
Simulate BHT collision finding on 4-round BLAKE4.

### Approach
1. Implement 4-round BLAKE4 as quantum oracle
2. Set up BHT algorithm structure
3. Measure actual vs theoretical O(2^(n/3)) complexity

### Measurements Required
1. **Query complexity**: Does it match O(2^(n/3))?
2. **Memory requirements**: Classical + quantum memory needs
3. **Scaling**: Extrapolation to 10 rounds

### Expected Output
```
4-round BHT simulation:
- Queries executed: [count]
- Theoretical (O(2^(n/3))): [count]
- Match: [yes/no, % difference]
- Memory used: [classical] + [quantum]
- Extrapolated 10-round cost: [estimate]
```

---

## Experiment 5: Structure Detection

### Task
Test if BLAKE4 has any distinguishable structure from a random permutation using quantum queries.

### Approach
1. Implement quantum distinguisher
2. Compare BLAKE4 compression vs random permutation
3. Measure distinguishing advantage

### Key Question
Can a quantum adversary distinguish BLAKE4 from a random oracle with non-negligible advantage?

### Expected Output
```
Structure detection:
- Query count: [number]
- Distinguishing advantage: [probability]
- Conclusion: [Indistinguishable / Distinguishable with advantage X]
```

---

## Experiment 6: Grover Speedup Verification

### Task
Verify that Grover's algorithm achieves expected O(√N) speedup on BLAKE4.

### Approach
1. Choose small target space (e.g., find input with specific 8-bit prefix)
2. Run Grover search
3. Compare iterations to √N theoretical

### Measurements Required
1. **Iterations to find target**: Actual count
2. **Theoretical iterations**: π/4 × √N
3. **Match**: Within expected variance?

### Expected Output
```
Grover verification (8-bit prefix search):
- Target space size: 2^8 = 256
- Theoretical iterations: π/4 × 16 ≈ 13
- Actual iterations: [count]
- Match: [yes/no]
- Notes: [any anomalies]
```

---

## Priority Order

1. **Experiment 1**: G function circuit (foundational)
2. **Experiment 2**: Full compression oracle (validates main estimate)
3. **Experiment 6**: Grover verification (confirms no speedup anomalies)
4. **Experiment 3**: Reduced-round analysis (security margin validation)
5. **Experiment 4**: BHT simulation (collision security validation)
6. **Experiment 5**: Structure detection (QROM security)

---

## Reporting Format

For each experiment, please provide:

1. **Circuit implementation details**
   - Gate counts by type (T, Clifford, etc.)
   - Qubit counts (working + ancilla)
   - Circuit depth

2. **Comparison to theoretical estimates**
   - Our estimate vs actual measurement
   - Explanation of any significant differences

3. **Security implications**
   - Does this change any security claims?
   - Any unexpected findings?

4. **Raw data/circuits** (if possible)
   - Circuit descriptions
   - Measurement logs

---

## Contact

Results should be integrated into:
- `QUANTUM_ANALYSIS.md` - Update pending experimental section
- `SECURITY_ANALYSIS.md` - Update Appendix E

Repository: https://github.com/bledden/blake4

---

*Prepared for quantum circuit verification of BLAKE4*
*January 8, 2026*
