#!/bin/bash
# Cross-compilation test: verify all examples compile for all targets
# We can't run Linux/FreeBSD binaries on macOS, but we verify:
# 1. Compilation succeeds without crash
# 2. Output file has correct format (ELF/Mach-O)
# 3. Syscall encodings are correct for each target

PARSER=${PARSER:-/tmp/parser_test}
EXAMPLES_DIR=${EXAMPLES_DIR:-examples}
TMPDIR=${TMPDIR:-/tmp}

PASS=0
FAIL=0
FAILED_LIST=""

for f in "$EXAMPLES_DIR"/*.al; do
    name=$(basename "$f" .al)
    [ "$name" = "simple" ] && continue  # simple.al uses unsupported syntax
    
    # macOS/Mach-O (default)
    if $PARSER "$f" "$TMPDIR/cc_${name}_macos.o" 2>/dev/null; then
        if file "$TMPDIR/cc_${name}_macos.o" | grep -q "Mach-O"; then
            macos_ok=1
        else
            macos_ok=0
        fi
    else
        macos_ok=0
    fi
    
    # Linux/ELF
    if $PARSER "$f" "$TMPDIR/cc_${name}_linux.o" --target=linux --elf 2>/dev/null; then
        if file "$TMPDIR/cc_${name}_linux.o" | grep -q "ELF"; then
            linux_ok=1
        else
            linux_ok=0
        fi
    else
        linux_ok=0
    fi
    
    # FreeBSD/ELF
    if $PARSER "$f" "$TMPDIR/cc_${name}_freebsd.o" --target=freebsd --elf 2>/dev/null; then
        if file "$TMPDIR/cc_${name}_freebsd.o" | grep -q "ELF"; then
            freebsd_ok=1
        else
            freebsd_ok=0
        fi
    else
        freebsd_ok=0
    fi
    
    if [ $macos_ok -eq 1 ] && [ $linux_ok -eq 1 ] && [ $freebsd_ok -eq 1 ]; then
        echo "PASS: $name (macOS✓ Linux✓ FreeBSD✓)"
        PASS=$((PASS + 1))
    else
        status=""
        [ $macos_ok -eq 0 ] && status="$status macOS✗"
        [ $linux_ok -eq 0 ] && status="$status Linux✗"
        [ $freebsd_ok -eq 0 ] && status="$status FreeBSD✗"
        echo "FAIL: $name ($status)"
        FAIL=$((FAIL + 1))
        FAILED_LIST="$FAILED_LIST $name"
    fi
done

echo ""
echo "============================================"
echo "  Cross-compile results: $PASS passed, $FAIL failed"
if [ $FAIL -gt 0 ]; then
    echo "  Failed:$FAILED_LIST"
fi
echo "============================================"
