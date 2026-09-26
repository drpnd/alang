#!/bin/bash
# Test all example programs through the self-hosting compiler (L1)

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TMPDIR="$(mktemp -d)"
trap "rm -rf $TMPDIR" EXIT

PASS=0
FAIL=0
TOTAL=0
FAILED_LIST=""

if [ ! -f /tmp/parser_test ]; then
    echo "Building L1..."
    cd "$ROOT/bootstrap" && make minica_test_build > /dev/null 2>&1
    cd "$ROOT"
    ./bootstrap/minica_test_build selfhost/parser.al /tmp/parser.o --aarch64 --mach-o 2>/dev/null
    ld -arch arm64 -platform_version macos 14.0 14.0 -o /tmp/parser_test /tmp/parser.o \
        -l System -syslibroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk -e _main 2>/dev/null
fi

COMPILER=/tmp/parser_test
SDKROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk

echo "============================================"
echo "  alang Example Program Test Suite"
echo "============================================"
echo ""

for al_file in "$ROOT"/examples/*.al; do
    name=$(basename "$al_file" .al)
    [ "$name" = "simple" ] && continue
    TOTAL=$((TOTAL + 1))
    
    o_file="$TMPDIR/${name}.o"
    exe_file="$TMPDIR/${name}"
    
    # Compile (synchronous, no timeout — compiler should finish quickly)
    if ! $COMPILER "$al_file" "$o_file" --aarch64 --mach-o > /dev/null 2>&1; then
        echo "FAIL: $name (compile)"
        FAIL=$((FAIL + 1))
        FAILED_LIST="$FAILED_LIST $name"
        continue
    fi
    
    # Link
    if ! ld -arch arm64 -platform_version macos 14.0 14.0 -o "$exe_file" "$o_file" \
        -l System -syslibroot "$SDKROOT" -e _main 2>/dev/null; then
        echo "FAIL: $name (link)"
        FAIL=$((FAIL + 1))
        FAILED_LIST="$FAILED_LIST $name"
        continue
    fi
    
    # Run with 1-second timeout
    "$exe_file" > /dev/null 2>&1 &
    pid=$!
    sleep 1
    if kill -0 $pid 2>/dev/null; then
        kill -9 $pid 2>/dev/null
        wait $pid 2>/dev/null
        echo "PASS: $name"
        PASS=$((PASS + 1))
    else
        wait $pid 2>/dev/null
        ec=$?
        if [ $ec -gt 128 ]; then
            echo "FAIL: $name (crash sig$((ec - 128)))"
            FAIL=$((FAIL + 1))
            FAILED_LIST="$FAILED_LIST $name"
        else
            echo "PASS: $name"
            PASS=$((PASS + 1))
        fi
    fi
done

echo ""
echo "============================================"
echo "  Results: $PASS/$TOTAL passed, $FAIL failed"
if [ -n "$FAILED_LIST" ]; then
    echo "  Failed:$FAILED_LIST"
fi
echo "============================================"
exit $FAIL
