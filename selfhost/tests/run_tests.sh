#!/bin/bash
# Unit test suite for alang self-hosting compiler (L1)
# Tests individual compiler features by compiling small .al programs
# and verifying generated aarch64 instructions via otool.
#
# Usage:
#   ./selfhost/tests/run_tests.sh
#
# Prerequisites:
#   - Bootstrap compiler built: cd bootstrap && make minica_test_build
#   - L1 built: ./bootstrap/minica_test_build selfhost/parser.al /tmp/parser.o ...

set -e

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TMPDIR="$(mktemp -d)"
trap "rm -rf $TMPDIR" EXIT

PASS=0
FAIL=0
TOTAL=0

# Build L1 if not present
if [ ! -f /tmp/parser_test ]; then
    echo "Building L1..."
    cd "$ROOT/bootstrap" && make minica_test_build > /dev/null 2>&1
    cd "$ROOT"
    ./bootstrap/minica_test_build selfhost/parser.al /tmp/parser.o --aarch64 --mach-o 2>/dev/null
    ld -arch arm64 -platform_version macos 14.0 14.0 -o /tmp/parser_test /tmp/parser.o \
        -l System -syslibroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk -e _main 2>/dev/null
fi

COMPILER=/tmp/parser_test

# compile_and_check NAME SOURCE EXPECTED_INSTR
# Compiles SOURCE, links, disassembles, checks for EXPECTED_INSTR
compile_and_check() {
    local name="$1"
    local src="$2"
    local expected="$3"
    TOTAL=$((TOTAL + 1))
    
    local al_file="$TMPDIR/${name}.al"
    local o_file="$TMPDIR/${name}.o"
    local exe_file="$TMPDIR/${name}"
    
    echo "$src" > "$al_file"
    
    # Compile
    if ! $COMPILER "$al_file" "$o_file" --aarch64 --mach-o > "$TMPDIR/${name}.log" 2>&1; then
        echo "FAIL: $name - compilation failed"
        cat "$TMPDIR/${name}.log"
        FAIL=$((FAIL + 1))
        return 1
    fi
    
    # Link
    if ! ld -arch arm64 -platform_version macos 14.0 14.0 -o "$exe_file" "$o_file" \
        -l System -syslibroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk \
        -e _main 2>/dev/null; then
        echo "FAIL: $name - linking failed"
        FAIL=$((FAIL + 1))
        return 1
    fi
    
    # Disassemble and check
    local disasm
    disasm=$(otool -tv "$exe_file" 2>/dev/null)
    
    if echo "$disasm" | grep -q "$expected"; then
        echo "PASS: $name"
        PASS=$((PASS + 1))
    else
        echo "FAIL: $name - expected '$expected' in disassembly"
        echo "--- Disassembly ---"
        echo "$disasm" | sed -n '/_main:/,/^$/p' | head -20
        FAIL=$((FAIL + 1))
    fi
}

# compile_only NAME SOURCE
# Just checks that compilation succeeds and produces valid Mach-O
compile_only() {
    local name="$1"
    local src="$2"
    TOTAL=$((TOTAL + 1))
    
    local al_file="$TMPDIR/${name}.al"
    local o_file="$TMPDIR/${name}.o"
    
    echo "$src" > "$al_file"
    
    if $COMPILER "$al_file" "$o_file" --aarch64 --mach-o > "$TMPDIR/${name}.log" 2>&1; then
        # Check it's valid Mach-O
        if otool -h "$o_file" > /dev/null 2>&1; then
            echo "PASS: $name"
            PASS=$((PASS + 1))
        else
            echo "FAIL: $name - output is not valid Mach-O"
            FAIL=$((FAIL + 1))
        fi
    else
        echo "FAIL: $name - compilation failed"
        cat "$TMPDIR/${name}.log"
        FAIL=$((FAIL + 1))
    fi
}

HEADER='fn main(argc: i32, argv: i64) (r: i32) {'
FOOTER='    mut r = 0 }'

echo "============================================"
echo "  alang Self-Hosting Compiler Unit Tests"
echo "============================================"
echo ""

echo "--- Arithmetic Operators ---"
compile_and_check "add" \
    "$HEADER let a: i64 = 3 let b: i64 = 4 let c: i64 = 0 mut c = a + b $FOOTER" \
    "add"
compile_and_check "sub" \
    "$HEADER let a: i64 = 7 let b: i64 = 2 let c: i64 = 0 mut c = a - b $FOOTER" \
    "sub"
compile_and_check "mul" \
    "$HEADER let a: i64 = 3 let b: i64 = 4 let c: i64 = 0 mut c = a * b $FOOTER" \
    "mul"
compile_and_check "div" \
    "$HEADER let a: i64 = 12 let b: i64 = 4 let c: i64 = 0 mut c = a / b $FOOTER" \
    "sdiv"
compile_and_check "mod" \
    "$HEADER let a: i64 = 10 let b: i64 = 3 let c: i64 = 0 mut c = a % b $FOOTER" \
    "msub"

echo ""
echo "--- Bitwise Operators ---"
compile_and_check "bit_and" \
    "$HEADER let a: i64 = 255 let b: i64 = 15 let c: i64 = 0 mut c = a & b $FOOTER" \
    "and"
compile_and_check "bit_or" \
    "$HEADER let a: i64 = 240 let b: i64 = 15 let c: i64 = 0 mut c = a | b $FOOTER" \
    "orr"
compile_and_check "bit_xor" \
    "$HEADER let a: i64 = 255 let b: i64 = 15 let c: i64 = 0 mut c = a ^ b $FOOTER" \
    "eor"

echo ""
echo "--- Shift Operators ---"
compile_and_check "lshift" \
    "$HEADER let a: i64 = 1 let b: i64 = 8 let c: i64 = 0 mut c = a << b $FOOTER" \
    "lsl"
compile_and_check "rshift" \
    "$HEADER let a: i64 = 256 let b: i64 = 4 let c: i64 = 0 mut c = a >> b $FOOTER" \
    "lsr"

echo ""
echo "--- Comparison Operators ---"
compile_and_check "cmp_eq" \
    "$HEADER let a: i64 = 5 let b: i64 = 5 let c: i64 = 0 mut c = a == b $FOOTER" \
    "cset"
compile_and_check "cmp_ne" \
    "$HEADER let a: i64 = 5 let b: i64 = 3 let c: i64 = 0 mut c = a != b $FOOTER" \
    "cset"
compile_and_check "cmp_lt" \
    "$HEADER let a: i64 = 3 let b: i64 = 5 let c: i64 = 0 mut c = a < b $FOOTER" \
    "cset"
compile_and_check "cmp_gt" \
    "$HEADER let a: i64 = 5 let b: i64 = 3 let c: i64 = 0 mut c = a > b $FOOTER" \
    "cset"

echo ""
echo "--- Hex Literals ---"
compile_and_check "hex_small" \
    "$HEADER let a: i64 = 0x0F $FOOTER" \
    '#0xf'
compile_and_check "hex_large" \
    "$HEADER let a: i64 = 0xFFFF $FOOTER" \
    '#0xffff'
compile_and_check "hex_with_zero" \
    "$HEADER let a: i64 = 0x80000400 $FOOTER" \
    '#0x4'

echo ""
echo "--- Byte Operations (used by emit32/write32) ---"
compile_and_check "byte_extract" \
    "$HEADER let v: i64 = 4277009103 let b1: i64 = 0 mut b1 = (v >> 8) & 255 $FOOTER" \
    "lsr"
compile_and_check "byte_mask" \
    "$HEADER let v: i64 = 4277009103 let b0: i64 = 0 mut b0 = v & 255 $FOOTER" \
    "and"

echo ""
echo "--- Function Calls ---"
compile_only "fn_call" \
    'fn helper(x: i64) (r: i64) { mut r = x + 1 }
fn main(argc: i32, argv: i64) (r: i32) { let v: i64 = 0 mut v = helper(5) mut r = 0 }'

echo ""
echo "--- Control Flow ---"
compile_only "if_stmt" \
    "$HEADER let a: i64 = 5 if a > 3 { let b: i64 = 0 mut b = 1 } $FOOTER"
compile_only "while_loop" \
    "$HEADER let i: i64 = 0 while i < 10 { mut i = i + 1 } $FOOTER"

echo ""

echo "--- Logical Operators ---"
compile_and_check "logic_or" "
    $HEADER let a: i64 = 0 let b: i64 = 5 let c: i64 = 0 mut c = a || b $FOOTER" "
    "orr""
compile_and_check "logic_and" "
    $HEADER let a: i64 = 5 let b: i64 = 3 let c: i64 = 0 mut c = a && b $FOOTER" "
    "and""

echo "--- Mach-O Output Validation ---"
compile_only "valid_macho" "$HEADER $FOOTER"

# Verify Mach-O magic bytes
TOTAL=$((TOTAL + 1))
echo "$HEADER $FOOTER" > "$TMPDIR/magic_test.al"
$COMPILER "$TMPDIR/magic_test.al" "$TMPDIR/magic_test.o" --aarch64 --mach-o >/dev/null 2>/dev/null
magic=$(xxd -l 4 "$TMPDIR/magic_test.o" | awk '{print $2}')
if [ "$magic" = "cffa" ]; then
    echo "PASS: macho_magic_bytes"
    PASS=$((PASS + 1))
else
    echo "FAIL: macho_magic_bytes - got $magic, expected cffa"
    FAIL=$((FAIL + 1))
fi

# Verify _main symbol is defined (not undefined)
TOTAL=$((TOTAL + 1))
sym_type=$(nm -m "$TMPDIR/magic_test.o" 2>/dev/null | grep " _main$" | grep -o "external")
if echo "$sym_type" | grep -q "external"; then
    echo "PASS: main_symbol_defined"
    PASS=$((PASS + 1))
else
    echo "FAIL: main_symbol_defined - _main is $sym_type"
    FAIL=$((FAIL + 1))
fi

echo ""
echo "============================================"
echo "  Results: $PASS/$TOTAL passed, $FAIL failed"
echo "============================================"
if [ $FAIL -eq 0 ]; then
    echo "All tests passed!"
    exit 0
else
    echo "$FAIL test(s) failed."
    exit 1
fi
