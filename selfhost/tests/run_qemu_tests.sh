#!/bin/bash
# QEMU Linux emulation tests for alang cross-compiled binaries
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
QEMU_DIR="/tmp/qemu-linux"
KERNEL="$QEMU_DIR/Image.debian"
COMPILER=/tmp/parser_test

PASS=0
FAIL=0
TOTAL=0

QEMU=""
for p in /opt/local/bin/qemu-system-aarch64 /usr/local/bin/qemu-system-aarch64 $(which qemu-system-aarch64 2>/dev/null); do
    if [ -x "$p" ]; then QEMU="$p"; break; fi
done
if [ -z "$QEMU" ]; then echo "ERROR: qemu-system-aarch64 not found"; exit 1; fi
if [ ! -f "$KERNEL" ]; then
    echo "Downloading Linux kernel..."
    mkdir -p "$QEMU_DIR"
    curl -sL --max-time 120 -o "$KERNEL" "https://deb.debian.org/debian/dists/bookworm/main/installer-arm64/current/images/netboot/debian-installer/arm64/linux"
fi
if [ ! -f "$COMPILER" ]; then
    cd "$ROOT/bootstrap" && make minica_test_build > /dev/null 2>&1; cd "$ROOT"
    ./bootstrap/minica_test_build selfhost/parser.al /tmp/parser.o --aarch64 --mach-o 2>/dev/null
    ld -arch arm64 -platform_version macos 14.0 14.0 -o "$COMPILER" /tmp/parser.o -l System -syslibroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk -e _main 2>/dev/null
fi

run_qemu_test() {
    local name="$1"
    local src="$2"
    local expected="$3"
    TOTAL=$((TOTAL + 1))
    local al_file="$QEMU_DIR/${name}.al"
    local elf_file="$QEMU_DIR/${name}.elf"
    local cpio_file="$QEMU_DIR/${name}.cpio"
    local log_file="$QEMU_DIR/${name}.log"
    echo "$src" > "$al_file"
    if ! $COMPILER "$al_file" "$elf_file" --target=linux --exec 2>/dev/null; then
        echo "FAIL: $name (compile)"; FAIL=$((FAIL + 1)); return 1
    fi
    mkdir -p "$QEMU_DIR/${name}_initramfs"
    cp "$elf_file" "$QEMU_DIR/${name}_initramfs/init"
    chmod +x "$QEMU_DIR/${name}_initramfs/init"
    (cd "$QEMU_DIR/${name}_initramfs" && find . | cpio -o -H newc 2>/dev/null) > "$cpio_file"
    $QEMU -M virt -cpu cortex-a72 -m 256M -kernel "$KERNEL" -initrd "$cpio_file" \
        -append "console=ttyAMA0 panic=1" -nographic -no-reboot > "$log_file" 2>&1 &
    local qpid=$!
    sleep 8
    kill $qpid 2>/dev/null; wait $qpid 2>/dev/null
    local exit_hex
    exit_hex=$(grep -o 'exitcode=0x[0-9a-f]*' "$log_file" | head -1 | sed 's/exitcode=0x//')
    if [ -z "$exit_hex" ]; then
        echo "FAIL: $name (no exit code)"; FAIL=$((FAIL + 1)); return 1
    fi
    # Wait status format: low byte = signal, high byte = exit code
    local signal=$(( 0x$exit_hex & 0x7f ))
    local exit_dec=$(( (0x$exit_hex >> 8) & 0xff ))
    if [ $signal -ne 0 ]; then
        echo "FAIL: $name (signal $signal)"; FAIL=$((FAIL + 1)); return 1
    fi
    if [ "$exit_dec" -eq "$expected" ]; then
        echo "PASS: $name (exit=$exit_dec)"; PASS=$((PASS + 1))
    else
        echo "FAIL: $name (exit=$exit_dec, expected=$expected)"; FAIL=$((FAIL + 1))
    fi
}

echo "============================================"
echo "  alang QEMU Linux Emulation Tests"
echo "============================================"
echo ""
echo "--- Basic ---"
run_qemu_test "q_ret42" 'fn main() (r: i32) { mut r = 42 __syscall(93, r) }' 42
run_qemu_test "q_ret0" 'fn main() (r: i32) { mut r = 0 __syscall(93, r) }' 0
run_qemu_test "q_ret1" 'fn main() (r: i32) { mut r = 1 __syscall(93, r) }' 1
run_qemu_test "q_ret255" 'fn main() (r: i32) { mut r = 255 __syscall(93, r) }' 255
echo ""
echo "--- Arithmetic ---"
run_qemu_test "q_add" 'fn main() (r: i32) { let a: i32 = 20 let b: i32 = 22 mut r = a + b __syscall(93, r) }' 42
run_qemu_test "q_mul" 'fn main() (r: i32) { let a: i32 = 6 let b: i32 = 7 mut r = a * b __syscall(93, r) }' 42
run_qemu_test "q_sub" 'fn main() (r: i32) { let a: i32 = 100 let b: i32 = 58 mut r = a - b __syscall(93, r) }' 42
echo ""
echo "--- Control Flow ---"
run_qemu_test "q_if" 'fn main() (r: i32) { let x: i32 = 5 mut r = 0 if x > 3 { mut r = 42 } __syscall(93, r) }' 42
run_qemu_test "q_while" 'fn main() (r: i32) { let i: i32 = 0 mut i = 0 let s: i32 = 0 mut s = 0 while i < 10 { mut s = s + i mut i = i + 1 } mut r = s __syscall(93, r) }' 45
echo ""
echo "--- Function Calls ---"
run_qemu_test "q_fib" 'fn fib(n: i32) (r: i32) { if n <= 1 { mut r = n } else { let a: i32 = 0 let b: i32 = 0 mut a = fib(n - 1) mut b = fib(n - 2) mut r = a + b } }
fn main() (r: i32) { mut r = fib(10) __syscall(93, r) }' 55
echo ""
echo "--- Enum/Match ---"
run_qemu_test "q_enum" 'enum Opt { Some(i32), None }
fn main() (r: i32) { let x = Some(42) mut r = 0 match x { Some(v) => mut r = v, None => mut r = 0 } __syscall(93, r) }' 42
echo ""
echo "--- Memory ---"
run_qemu_test "q_malloc" 'fn main() (r: i32) { let buf = __malloc(64) __mem_store(buf, 42) let v = __mem_load(buf) mut r = v __syscall(93, r) }' 42
echo ""
echo "============================================"
echo "  Results: $PASS/$TOTAL passed, $FAIL failed"
echo "============================================"
exit $FAIL
