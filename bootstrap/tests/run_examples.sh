#!/bin/sh
# Compile, link, and run each example, checking exit codes.

BOOTDIR="$(cd "$(dirname "$0")/.." && pwd)"
EXAMPLES_DIR="$BOOTDIR/../examples"
PASS=0
FAIL=0

run_test() {
    example="$1"
    expected="$2"
    src="$EXAMPLES_DIR/$example"
    
    if [ ! -f "$src" ]; then
        echo "  [SKIP] $example (file not found)"
        return
    fi
    
    obj="/tmp/test_$$.o"
    exe="/tmp/test_$$.exe"
    
    # Compile to DFIR + assemble + export
    output=$("$BOOTDIR/minica_test_build" "$src" "$obj" --aarch64 --mach-o 2>&1)
    if [ $? -ne 0 ]; then
        echo "  [FAIL] $example -- compile error"
        FAIL=$((FAIL + 1))
        return
    fi
    
    # Link
    ld -arch arm64 -o "$exe" "$obj" -l System \
       -syslibroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk \
       -e _main 2>/dev/null
    if [ $? -ne 0 ]; then
        echo "  [FAIL] $example -- link error"
        FAIL=$((FAIL + 1))
        rm -f "$obj"
        return
    fi
    
    # Run (exit codes are 0-255, mod 256)
    "$exe"
    actual=$?
    expected_mod=$(($expected % 256))
    
    if [ "$actual" -eq "$expected_mod" ]; then
        echo "  [PASS] $example -> exit $actual (expected $expected_mod)"
        PASS=$((PASS + 1))
    else
        echo "  [FAIL] $example -> exit $actual (expected $expected_mod)"
        FAIL=$((FAIL + 1))
    fi
    
    rm -f "$obj" "$exe"
}

echo "=== Example Tests ==="
echo ""

run_test "zero.al" 0
run_test "simple1.al" 3
run_test "arith.al" 13
run_test "subtract.al" 7
run_test "multiply.al" 42
run_test "bitops.al" 8
run_test "comparison.al" 1
run_test "multi_var.al" 10
run_test "large_num.al" 3000

echo ""
echo "=== Results: $PASS passed, $FAIL failed ==="
run_test "if_test.al" 1
run_test "if_else.al" 2
run_test "while_test.al" 5
