#!/bin/bash
#
# BLAKE4 Industry Benchmark Comparison Script
#
# Compares BLAKE4 against SHA-256, SHA-512, and BLAKE3
# Reports throughput in MB/s for various input sizes
#
# Usage: ./compare_hashes.sh [build_dir]
#

set -e

BUILD_DIR="${1:-../build}"
TEMP_DIR=$(mktemp -d)
ITERATIONS=100

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

cleanup() {
    rm -rf "$TEMP_DIR"
}
trap cleanup EXIT

echo "=============================================="
echo "BLAKE4 Industry Benchmark Comparison"
echo "=============================================="
echo ""

# Check for required tools
check_tool() {
    if ! command -v "$1" &> /dev/null; then
        echo -e "${RED}Warning: $1 not found, skipping those benchmarks${NC}"
        return 1
    fi
    return 0
}

# Create test files
echo "Creating test files..."
SIZES=(1024 65536 1048576 16777216)  # 1KB, 64KB, 1MB, 16MB
SIZE_NAMES=("1KB" "64KB" "1MB" "16MB")

for i in "${!SIZES[@]}"; do
    dd if=/dev/zero of="$TEMP_DIR/test_${SIZE_NAMES[$i]}" bs="${SIZES[$i]}" count=1 2>/dev/null
done
echo ""

# Function to benchmark and calculate throughput
benchmark() {
    local name="$1"
    local cmd="$2"
    local size="$3"
    local iterations="$4"

    # Time the operation
    start=$(python3 -c "import time; print(time.time())")
    for ((i=0; i<iterations; i++)); do
        eval "$cmd" > /dev/null 2>&1
    done
    end=$(python3 -c "import time; print(time.time())")

    # Calculate throughput
    elapsed=$(python3 -c "print(${end} - ${start})")
    total_bytes=$(python3 -c "print(${size} * ${iterations})")
    throughput=$(python3 -c "print(f'{(${total_bytes} / ${elapsed}) / (1024*1024):.2f}')")

    echo "$throughput"
}

echo "Running benchmarks (this may take a few minutes)..."
echo ""

# BLAKE4 benchmarks
if [ -f "$BUILD_DIR/benchmark" ]; then
    echo -e "${BLUE}=== BLAKE4 ===${NC}"

    # Run the built-in benchmark and extract results
    "$BUILD_DIR/benchmark" 50 2>&1 | grep -E "Serial \(one-shot\)" | head -4 | while read line; do
        echo "  $line"
    done

    # Get implementation name
    impl=$("$BUILD_DIR/benchmark" 1 2>&1 | grep "Implementation:" | cut -d: -f2 | xargs)
    echo "  Implementation: $impl"
    echo ""
else
    echo -e "${RED}BLAKE4 benchmark not found at $BUILD_DIR/benchmark${NC}"
fi

# OpenSSL SHA benchmarks
if check_tool openssl; then
    echo -e "${BLUE}=== OpenSSL SHA-256 ===${NC}"
    openssl speed -seconds 3 sha256 2>&1 | grep -E "^sha256|^Doing" | tail -1
    echo ""

    echo -e "${BLUE}=== OpenSSL SHA-512 ===${NC}"
    openssl speed -seconds 3 sha512 2>&1 | grep -E "^sha512|^Doing" | tail -1
    echo ""
fi

# BLAKE3 benchmarks
if check_tool b3sum; then
    echo -e "${BLUE}=== BLAKE3 (b3sum) ===${NC}"

    for i in "${!SIZES[@]}"; do
        size_name="${SIZE_NAMES[$i]}"
        file="$TEMP_DIR/test_${size_name}"

        # Adjust iterations based on size
        if [ "${SIZES[$i]}" -ge 1048576 ]; then
            iters=50
        else
            iters=500
        fi

        start=$(python3 -c "import time; print(time.time())")
        for ((j=0; j<iters; j++)); do
            b3sum "$file" > /dev/null 2>&1
        done
        end=$(python3 -c "import time; print(time.time())")

        elapsed=$(python3 -c "print(${end} - ${start})")
        throughput=$(python3 -c "print(f'{(${SIZES[$i]} * ${iters} / ${elapsed}) / (1024*1024):.2f}')")

        echo "  ${size_name}: ${throughput} MB/s"
    done
    echo ""
fi

# Summary comparison table
echo "=============================================="
echo "Summary Comparison"
echo "=============================================="
echo ""
echo "Note: BLAKE4 results from built-in benchmark tool."
echo "      SHA/BLAKE3 results from system tools."
echo ""
echo "For accurate comparison, all algorithms should be"
echo "tested with the same methodology. The built-in"
echo "BLAKE4 benchmark measures pure hashing throughput"
echo "without CLI overhead."
echo ""

# Run BLAKE4's comprehensive benchmark
if [ -f "$BUILD_DIR/benchmark" ]; then
    echo "=============================================="
    echo "Detailed BLAKE4 Results"
    echo "=============================================="
    echo ""
    "$BUILD_DIR/benchmark" 50 2>&1
fi
