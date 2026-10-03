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
    "asr"

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
    "asr"
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

echo "--- Enum Constructors ---"
compile_only "enum_construct" \
    'enum Opt { Some(i32), None }
fn main() (r: i32) { let x = Some(42) mut r = 0 }'
compile_only "enum_bare" \
    'enum Opt { Some(i32), None }
fn main() (r: i32) { let x = None mut r = 0 }'
compile_only "enum_match_bind" \
    'enum Opt { Some(i32), None }
fn main() (r: i32) { let x = Some(42) match x { Some(v) => mut r = v, None => mut r = 0 } }'

echo ""
echo "--- Match Expressions ---"
compile_only "match_3cases" \
    'enum C { Red, Green, Blue }
fn main() (r: i32) { let c = Green match c { Red => mut r = 1, Green => mut r = 2, Blue => mut r = 3 } }'
compile_only "match_fallthrough" \
    'enum C { A, B, C }
fn main() (r: i32) { let x = C match x { A => mut r = 1, B => mut r = 2 } }'

echo ""
echo "--- Spill (>25 locals) ---"
compile_only "spill_33" \
    'fn main() (r: i32) { let a0: i32 = 1 let a1: i32 = 2 let a2: i32 = 3 let a4: i32 = 5 let a5: i32 = 6 let a6: i32 = 7 let a7: i32 = 8 let a8: i32 = 9 let a9: i32 = 10 let a10: i32 = 11 let a11: i32 = 12 let a12: i32 = 13 let a13: i32 = 14 let a14: i32 = 15 let a15: i32 = 16 let a16: i32 = 17 let a17: i32 = 18 let a18: i32 = 19 let a19: i32 = 20 let a20: i32 = 21 let a21: i32 = 22 let a22: i32 = 23 let a23: i32 = 24 let a24: i32 = 25 let a25: i32 = 26 let a26: i32 = 27 let a27: i32 = 28 let a28: i32 = 29 let a29: i32 = 30 let a30: i32 = 31 let a31: i32 = 32 let a32: i32 = 33 mut r = a0 + a32 }'

echo ""
echo "--- String Builtins ---"
compile_only "str_len" \
    'fn main() (r: i32) { let s = "hello" let n = __str_len(s) mut r = 0 }'
compile_only "str_eq" \
    'fn main() (r: i32) { let eq = __str_eq("a", "b") mut r = 0 }'
compile_only "str_empty" \
    'fn main() (r: i32) { let s = "" let n = __str_len(s) mut r = 0 }'

echo ""
echo "--- Memory Builtins ---"
compile_only "mem_store_load" \
    'fn main() (r: i32) { let buf = __malloc(64) __mem_store(buf, 42) let v = __mem_load(buf) mut r = 0 }'
compile_only "byte_store_load" \
    'fn main() (r: i32) { let buf = __malloc(16) __byte_store(buf, 0, 72) let v = __byte_load(buf, 0) mut r = 0 }'
compile_only "alloca_test" \
    'fn main() (r: i32) { let buf = __alloca(32) __mem_store(buf, 99) mut r = 0 }'

echo ""
echo "--- Large Constants ---"
compile_and_check "const_32bit" \
    "$HEADER let a: i64 = 0x12345678 $FOOTER" \
    "movk"
compile_and_check "const_48bit" \
    "$HEADER let a: i64 = 0x123456789ABC $FOOTER" \
    "movk"
compile_and_check "const_64bit" \
    "$HEADER let a: i64 = 0x123456789ABCDEF $FOOTER" \
    "movk"

echo ""
echo "--- Negative Numbers ---"
compile_and_check "neg_one" \
    "$HEADER let a: i64 = -1 $FOOTER" \
    "neg"
compile_and_check "neg_large" \
    "$HEADER let a: i64 = -42 $FOOTER" \
    "neg"

echo ""
echo "--- Deep Control Flow ---"
compile_only "nested_if_4" \
    'fn main(argc: i32, argv: i64) (r: i32) { if argc == 1 { mut r = 1 } else { if argc == 2 { mut r = 2 } else { if argc == 3 { mut r = 3 } else { if argc == 4 { mut r = 4 } else { mut r = 99 } } } } }'
compile_only "while_break_continue" \
    'fn main() (r: i32) { let i: i32 = 0 mut i = 0 while i < 100 { mut i = i + 1 if i == 5 { } else { if i == 10 { break } } } mut r = i }'
compile_only "deep_recursion" \
    'fn f(n: i32) (r: i32) { if n <= 0 { mut r = 0 } else { mut r = f(n - 1) + 1 } }
fn main() (r: i32) { mut r = f(10) }'

echo ""
echo "--- Many Arguments ---"
compile_only "fn_8args" \
    'fn s8(a: i32, b: i32, c: i32, d: i32, e: i32, f: i32, g: i32, h: i32) (r: i32) { mut r = a + b + c + d + e + f + g + h }
fn main() (r: i32) { mut r = s8(1, 2, 3, 4, 5, 6, 7, 8) }'

echo ""
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
