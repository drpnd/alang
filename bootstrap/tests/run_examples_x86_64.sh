#!/bin/sh
# Compile, link, and run each example on x86-64 Mach-O (via Rosetta).
# Only includes tests known to work on x86-64 (limited to 14 SSA registers).

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
    
    obj="/tmp/test_x86_$$.o"
    exe="/tmp/test_x86_$$.exe"
    
    # Compile to DFIR + assemble + export (x86-64 Mach-O)
    output=$("$BOOTDIR/minica_test_build" "$src" "$obj" --x86-64 --mach-o 2>&1)
    if [ $? -ne 0 ]; then
        echo "  [FAIL] $example -- compile error"
        FAIL=$((FAIL + 1))
        return
    fi
    
    # Link (x86-64 Mach-O via universal ld)
    ld -arch x86_64 -o "$exe" "$obj" -l System \
       -syslibroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk \
       -e _main 2>/dev/null
    if [ $? -ne 0 ]; then
        echo "  [FAIL] $example -- link error"
        FAIL=$((FAIL + 1))
        rm -f "$obj"
        return
    fi
    
    # Run with 5-second timeout (macOS has no timeout, use background+kill)
    "$exe" &
    pid=$!
    sleep 5
    if kill -0 $pid 2>/dev/null; then
        echo "  [FAIL] $example -- timeout (hanging)"
        kill -9 $pid 2>/dev/null
        wait $pid 2>/dev/null
        FAIL=$((FAIL + 1))
        rm -f "$obj" "$exe"
        return
    fi
    wait $pid
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

echo "=== x86-64 Example Tests ==="
echo ""

# Tests known to work on x86-64 (SSA register count <= 14)
run_test "zero.al" 0
run_test "simple1.al" 3
run_test "arith.al" 13
run_test "subtract.al" 7
run_test "multiply.al" 42
run_test "bitops.al" 8
run_test "comparison.al" 1
run_test "multi_var.al" 10
run_test "large_num.al" 184

echo ""
echo "=== Results: $PASS passed, $FAIL failed ==="

run_test "if_test.al" 1
run_test "if_else.al" 2
run_test "while_test.al" 5
run_test "func_call.al" 7
run_test "func_call2.al" 14
run_test "func_if.al" 7
run_test "for_test.al" 10
run_test "break_test.al" 10
run_test "while_break.al" 5
run_test "div_mod.al" 5
run_test "fibonacci.al" 55
run_test "factorial.al" 120
run_test "negate.al" 214
run_test "for_nested.al" 6
run_test "continue_test.al" 9
run_test "struct_test.al" 42
run_test "struct_test2.al" 30
run_test "enum_test.al" 0
run_test "enum_match.al" 42
run_test "enum_match2.al" 1
run_test "enum_match3.al" 3
run_test "enum_tuple.al" 1
