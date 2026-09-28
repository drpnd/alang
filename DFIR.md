# DFIR — Data Flow IR Specification

> **Version:** 0.4 (implemented)
>
> **Status:** Both bootstrap and self-hosting compilers implement aarch64 and x86-64 backends

## 1. Overview

DFIR (Data Flow IR) is a minimal, single static assignment (SSA) intermediate
representation designed for a data flow programming language that supports
both functions and coroutines as first-class constructs.

### 1.1 Design Goals

| Goal | Description |
|------|-------------|
| Minimal | 48 instructions total across three layers |
| SSA | Every value is assigned exactly once |
| Data-flow-native | Channels, `yield`, `await` are IR-level instructions |
| Backend-agnostic | Lowerable to Cranelift, C, or an interpreter |
| Debuggable | Human-readable textual form |

### 1.2 Compilation Pipeline

```
.al source → minica_parse() → AST → compile_to_dfir() → DFIR → ir_optimize() → backend → .o → ld → executable
                                                              ↓
                                                    aarch64 (Mach-O) / x86-64 (ELF)
```

DFIR is the **only** IR in the compiler. There is no separate LLVM step.
Both the bootstrap compiler (`bootstrap/`) and the self-hosting compiler
(`selfhost/parser.al`) implement the full pipeline from source to native
code for both aarch64 (macOS Mach-O) and x86-64 (ELF).

### 1.3 Three Layers

| Layer | Keyword | Represents |
|-------|---------|-----------|
| 1 | `func` | Pure functions (SSA basic blocks) |
| 2 | `coro` | Coroutine state machines (named states, suspension points) |
| 3 | `graph` | Data flow graph topology (nodes + edges, declarative metadata) |

---

## 2. Types

DFIR uses a simple type system mirroring the source language.

### 2.1 Primitive Types

| DFIR Type | Description | Size |
|-----------|-------------|------|
| `i8` `i16` `i32` `i64` | Signed integers | 1/2/4/8 bytes |
| `u8` `u16` `u32` `u64` | Unsigned integers | 1/2/4/8 bytes |
| `f16` `f32` `f64` | IEEE 754 floats | 2/4/8 bytes |
| `fp8` `fp4` | Low-precision floats (E4M3/E5M2 / E2M1) | 1/0.5 bytes |
| `bool` | Boolean | 1 byte |
| `char` | Unicode scalar value | 4 bytes |
| `str` | UTF-8 string (immutable, reference) | ptr |
| `()` | Unit type | 0 bytes |

### 2.2 Composite Types

| DFIR Type | Syntax | Description |
|-----------|--------|-------------|
| Struct | `{field: T, ...}` | Named field aggregate |
| Enum | `enum Name { Var(T), ... }` | Tagged union (sum type) |
| Tuple | `(T, U, ...)` | Anonymous positional aggregate |
| Array | `[T; N]` | Fixed-size array |
| Chan | `Chan<T>` | Bounded channel carrying `T` |
| Stream | `stream<T>` | Logical stream (lowers to `Chan<T>`) |
| Poll | `Poll` | Coroutine poll result: `Ready` or `Pending` |
| Option | `Option<T>` | `Some(T)` or `None` |
| Result | `Result<T, E>` | `Ok(T)` or `Err(E)` |
| Closure | `Closure<Captures, Args...> -> Ret` | Environment + function pointer |

### 2.3 Type Aliases

DFIR allows named type aliases for readability:

```
type %DoubleState = enum { S0_start, S1_recv, S2_send(i32), S3_done }
type %Poll = enum { Ready, Pending }
```

---

## 3. Values and Operands

All values in DFIR are SSA values — assigned exactly once.

### 3.1 Value Naming

| Prefix | Scope | Example |
|--------|-------|---------|
| `%` | Local SSA value (function/coroutine-scoped) | `%1`, `%result` |
| `@` | Global symbol (function, coroutine, graph) | `@add`, `@double`, `@pipeline` |
| `S` | Coroutine state label | `S0_start`, `S1_recv` |
| `$` | Basic block label (in `func`) | `$entry`, `$loop` |

### 3.2 Constants

```
%c = const i32 42
%s = const str "hello"
%b = const bool true
%n = const i32 0          ; null/none for Option
```

---

## 4. Layer 1: `dfir.func` — Pure Functions

### 4.1 Structure

```
func @name(param: T, ...) -> RetT {
  $entry:
    <instructions>
    <terminator>
  $block_name:
    <instructions>
    <terminator>
}
```

- Each function has one or more **basic blocks**.
- The first block is `$entry`.
- Every block ends with a **terminator** (`br`, `switch`, `ret`).
- Parameters are SSA values available from `$entry`.

### 4.2 Instruction Set

#### Constants & Values

| Instruction | Form | Description |
|-------------|------|-------------|
| `const` | `%r = const T <literal>` | Load a constant value |
| `phi` | `%r = phi T [%a, $b1], [%c, $b2], ...` | Merge values from predecessor blocks |

#### Arithmetic

| Instruction | Form | Description |
|-------------|------|-------------|
| `add` | `%r = add T %a, %b` | Addition |
| `sub` | `%r = sub T %a, %b` | Subtraction |
| `mul` | `%r = mul T %a, %b` | Multiplication |
| `div` | `%r = div T %a, %b` | Division (signed for `i*`, float for `f*`) |
| `udiv` | `%r = udiv T %a, %b` | Unsigned division |
| `rem` | `%r = rem T %a, %b` | Remainder (signed) |
| `urem` | `%r = urem T %a, %b` | Unsigned remainder |
| `neg` | `%r = neg T %a` | Negation |

#### Comparison

| Instruction | Form | Description |
|-------------|------|-------------|
| `eq` | `%r = eq T %a, %b` | Equal |
| `ne` | `%r = ne T %a, %b` | Not equal |
| `lt` | `%r = lt T %a, %b` | Less than (signed) |
| `le` | `%r = le T %a, %b` | Less than or equal |
| `gt` | `%r = gt T %a, %b` | Greater than |
| `ge` | `%r = ge T %a, %b` | Greater than or equal |

#### Logic / Bitwise

| Instruction | Form | Description |
|-------------|------|-------------|
| `and` | `%r = and T %a, %b` | Bitwise AND / logical AND |
| `or` | `%r = or T %a, %b` | Bitwise OR / logical OR |
| `xor` | `%r = xor T %a, %b` | Bitwise XOR |
| `not` | `%r = not T %a` | Bitwise NOT / logical NOT |
| `shl` | `%r = shl T %a, %b` | Shift left |
| `shr` | `%r = shr T %a, %b` | Shift right (arithmetic for `i*`, logical for `u*`) |

#### Memory

| Instruction | Form | Description |
|-------------|------|-------------|
| `alloc` | `%r = alloc T` | Allocate on stack |
| `load` | `%r = load T %ptr` | Load from pointer |
| `store` | `store T %val, %ptr` | Store to pointer |
| `memcpy` | `memcpy %dst, %src, %n` | Copy `n` bytes |
| `get_field` | `%r = get_field %struct, <index>` | Extract struct field by index |
| `set_field` | `set_field %struct, <index>, %val` | Set struct field (on mutable struct) |
| `get_elem` | `%r = get_elem %arr, %index` | Array element access |
| `make_struct` | `%r = make_struct T { %f0, %f1, ... }` | Construct a struct |
| `make_enum` | `%r = make_enum T <Variant>(%val)` | Construct an enum variant |
| `extract_variant` | `%r = extract_variant %enum_val` | Extract payload from enum |
| `check_variant` | `%r = check_variant %enum_val, <Variant>` | Check if enum matches variant (returns `bool`) |

#### Cast

| Instruction | Form | Description |
|-------------|------|-------------|
| `cast` | `%r = cast T2 %val` | Type conversion (widening, narrowing, int↔float) |

#### Control Flow (Terminators)

| Instruction | Form | Description |
|-------------|------|-------------|
| `br` | `br $label` | Unconditional branch |
| `br_cond` | `br_cond %cond, $then, $else` | Conditional branch |
| `switch` | `switch T %val, $default [ <lit>, $label ... ]` | Multi-way branch |
| `ret` | `ret T %val` | Return value |

#### Call

| Instruction | Form | Description |
|-------------|------|-------------|
| `call` | `%r = call @func(%a, %b, ...)` | Call another function |

### 4.3 Example

Source:
```
fn add(a: i32, b: i32) -> i32 { a + b }
```

DFIR:
```
func @add(a: i32, b: i32) -> i32 {
  $entry:
    %1 = add i32 %a, %b
    ret i32 %1
}
```

### 4.4 Closure Example

Source:
```
let n = 10;
source |> map(|x| x + n) |> sink
```

DFIR:
```
type %closure_0_env = { n: i32 }

func @closure_0(env: %closure_0_env, x: i32) -> i32 {
  $entry:
    %n = get_field %env, 0
    %r = add i32 %x, %n
    ret i32 %r
}
```

### 4.5 Pattern Matching Example

Source:
```
match x {
    0 => "zero",
    1 => "one",
    _ => "many",
}
```

DFIR:
```
  $entry:
    switch i32 %x, $default [
      0, $case0
      1, $case1
    ]
  $case0:
    %r0 = const str "zero"
    br $join
  $case1:
    %r1 = const str "one"
    br $join
  $default:
    %r2 = const str "many"
    br $join
  $join:
    %result = phi str [%r0, $case0], [%r1, $case1], [%r2, $default]
    ret str %result
```

---

## 5. Layer 2: `dfir.coro` — Coroutine State Machines

### 5.1 Structure

A coroutine is compiled to a **stackless state machine**. The runtime calls
`poll()` to advance the coroutine by one step. Each `yield`/`await` becomes a
**suspension point** — a named state.

```
coro @name(
    state: %StateType,
    in: %Chan<T>,
    out: %Chan<U>,
    ...
) -> %Poll {
  state S0_name:
    <instructions>
    <transition>
  state S1_name:
    <instructions>
    <transition>
  ...
}
```

- `state` is the persistent state enum, storing the current state index and
  all local variables live across suspension points.
- Each `state` block is a basic block. When the coroutine suspends, control
  returns to the scheduler with `Poll::Pending`.

### 5.2 Coro-Specific Instructions

All `dfir.func` instructions are available inside coroutines, plus:

| Instruction | Form | Description |
|-------------|------|-------------|
| `recv` | `%r = recv %chan` | Receive from channel; returns `Option<T>` |
| `send` | `%r = send %chan, %val` | Send to channel; returns `Ok` or `Full` |
| `yield` | `yield %port, %val` | Alias for `send` on an output port |
| `await` | `%r = await %future` | Suspend until future is ready, then resume with value |
| `suspend` | `suspend` | Return `Poll::Pending` to scheduler |
| `ret` | `ret %Poll::Ready` | Coroutine is complete |

### 5.3 State Transitions

State transitions use `br` (unconditional) or `switch` (conditional):

```
; unconditional
br S1_next

; conditional (e.g., based on recv/send result)
switch %result -> Some(%x): S2_process, None: S3_done
switch %result -> Ok: S1_recv, Full: S0_retry
```

### 5.4 Generated State Enum

The compiler generates a state enum for each coroutine. It stores:
- The current state index.
- All local variables live across suspension points.

```
type %DoubleState = enum {
    S0_start,
    S1_recv,
    S2_send(i32),    // carries %x * 2 across suspension
    S3_done,
}
```

### 5.5 Example — Simple Coroutine

Source:
```
coro double(input: stream<i32>) -> stream<i32> {
    for x in input {
        yield x * 2;
    }
}
```

DFIR:
```
type %DoubleState = enum { S0_start, S1_recv, S2_send, S3_done }

coro @double(state: %DoubleState, in: %Chan<i32>, out: %Chan<i32>) -> %Poll {
  state S0_start:
    br S1_recv

  state S1_recv:
    %v = recv %in
    switch %v -> Some(%x): S2_send, None: S3_done

  state S2_send:
    %d = mul i32 %x, 2
    %ok = send %out, %d
    switch %ok -> Ok: S1_recv, Full: S2_send

  state S3_done:
    ret %Poll::Ready
}
```

### 5.6 Example — Multi-Port Coroutine

Source:
```
coro classifier(input: stream<Packet>) (tcp: stream<Packet>, udp: stream<Packet>) {
    for p in input {
        match p.protocol {
            "tcp" => yield tcp <- p,
            "udp" => yield udp <- p,
        }
    }
}
```

DFIR:
```
type %ClassifierState = enum { S0_start, S1_recv, S2_route, S_tcp, S_udp, S3_done }

coro @classifier(
    state: %ClassifierState,
    in: %Chan<Packet>,
    tcp: %Chan<Packet>,
    udp: %Chan<Packet>
) -> %Poll {
  state S0_start:
    br S1_recv

  state S1_recv:
    %v = recv %in
    switch %v -> Some(%p): S2_route, None: S3_done

  state S2_route:
    %proto = get_field %p, 0
    switch %proto, S1_recv [
      "tcp", S_tcp
      "udp", S_udp
    ]

  state S_tcp:
    %ok1 = send %tcp, %p
    switch %ok1 -> Ok: S1_recv, Full: S_tcp

  state S_udp:
    %ok2 = send %udp, %p
    switch %ok2 -> Ok: S1_recv, Full: S_udp

  state S3_done:
    ret %Poll::Ready
}
```

---

## 6. Layer 3: `dfir.graph` — Data Flow Graph Topology

### 6.1 Structure

A graph is a **static, declarative description** of nodes and edges. It is
metadata — not executable code. The runtime reads it to allocate channels
and spawn coroutines.

```
graph @<name> {
  node %id = @coro_or_func(args...) [ports...]
  edge %src.port -> %dst.port : chan<T, bufsize>
}
```

- Each `node` references a `dfir.func` or `dfir.coro`.
- Each `edge` declares a bounded channel with element type and buffer size.
- `source` and `sink` nodes reference built-in runtime functions.

### 6.2 Node Declaration

```
; Source node (built-in)
node %src = @source("file:input.csv")

; Map node referencing a coroutine
node %map = @map(@double)

; Filter node with a closure
node %filt = @filter(@closure_0)

; Sink node (built-in)
node %snk = @sink("stdout")

; Inline coroutine node
node %proc = @my_coro
```

### 6.3 Edge Declaration

```
edge %src.out -> %map.in   : chan<i32, 128>
edge %map.out -> %snk.in   : chan<i32, 128>
```

- `bufsize` is the channel buffer capacity (default: 128).
- Backpressure: when the buffer is full, the producer coroutine suspends.

### 6.4 Example — Simple Pipeline

Source:
```
source("input") |> map(double) |> sink("output")
```

DFIR:
```
graph @pipeline {
  node %src = @source("file:input")
  node %map = @map(@double)
  node %snk = @sink("stdout")

  edge %src.out -> %map.in : chan<i32, 128>
  edge %map.out -> %snk.in : chan<i32, 128>
}
```

### 6.5 Example — Fan-Out

Source:
```
graph {
    s = source("input");
    s |> parse(csv) |> sink("csv_out");
    s |> parse(json) |> sink("json_out");
}
```

DFIR:
```
graph @fanout {
  node %s      = @source("file:input")
  node %pcsv   = @parse(%CSV)
  node %pjson  = @parse(%JSON)
  node %scsv   = @sink("file:csv_out")
  node %sjson  = @sink("file:json_out")

  edge %s.out -> %pcsv.in    : chan<str, 128>
  edge %s.out -> %pjson.in   : chan<str, 128>
  edge %pcsv.out -> %scsv.in  : chan<Record, 128>
  edge %pjson.out -> %sjson.in : chan<Record, 128>
}
```

### 6.6 Graph Execution Semantics

At runtime, the graph is executed as follows:

1. **Channel allocation:** For each `edge`, allocate a bounded channel of
   the specified type and capacity.
2. **Coroutine spawning:** For each `node`, spawn a coroutine on the
   runtime's thread pool, passing the appropriate channel endpoints.
3. **Graph supervision:** A supervisor task monitors all nodes for
   completion or error. The graph is complete when all source nodes are
   exhausted and all channels are drained.

---

## 7. Module Structure

A DFIR module is a collection of types, functions, coroutines, and graphs.

```
module @my_program {

  ; --- Types ---
  type %Poll = enum { Ready, Pending }
  type %DoubleState = enum { S0_start, S1_recv, S2_send, S3_done }

  ; --- Functions ---
  func @add(a: i32, b: i32) -> i32 { ... }
  func @closure_0(env: %closure_0_env, x: i32) -> i32 { ... }

  ; --- Coroutines ---
  coro @double(state: %DoubleState, in: %Chan<i32>, out: %Chan<i32>) -> %Poll { ... }

  ; --- Graphs ---
  graph @pipeline { ... }
}
```

A module maps to a single compilation unit (one source file or crate).

---

## 8. Optimizer Passes

DFIR supports a set of optimization passes that operate on the IR.
All passes run to a fixpoint (until no changes are made):

| Pass | Status | Description |
|------|--------|-------------|
| Function inlining | ✅ Implemented | Inline small single-block function calls (≤8 blocks, ≤32 instrs) |
| Constant folding | ✅ Implemented | Evaluate constant binary/unary operations at compile time |
| Copy propagation | ✅ Implemented | Replace `%b = %a; ... %b` with `%a`; fold constants into operands |
| Constant branch elimination | ✅ Implemented | Replace `br_cond` with constant condition to unconditional `br` |
| Unreachable block elimination | ✅ Implemented | Remove blocks not reachable from entry (worklist-based) |
| Block merging | ✅ Implemented | Merge blocks connected by single-predecessor unconditional `br` |
| Dead code elimination | ✅ Implemented | Remove unused instructions without side effects |
| DCE after terminator | ✅ Implemented | Remove unreachable instructions after RET/BR |
| SSA compaction | ✅ Implemented | Renumber SSA IDs sequentially to eliminate gaps from optimization |
| State minimization | Planned | Merge redundant coroutine states |
| Node fusion | Planned | Fuse adjacent `map`/`filter` coros into a single coro |
| Channel elision | Planned | Remove unnecessary channels between fused nodes |

### 8.1 Pass Ordering

```
1. Function inlining      ; expand small function calls (before other passes)
2. Constant folding       ; fold constant expressions
3. Copy propagation       ; simplify SSA chains, fold constants into operands
4. Constant branch elim   ; replace constant br_cond with unconditional br
5. Unreachable block elim ; remove blocks not reachable from entry
6. Block merging          ; merge single-predecessor blocks
7. DCE after terminator   ; remove unreachable instructions after RET/BR
8. Dead code elimination  ; remove unused instructions
9. SSA compaction         ; renumber SSA IDs sequentially (after all other passes)
```

Passes 1-8 run in sequence per iteration, up to 10 iterations, until fixpoint.
SSA compaction (pass 9) runs once per iteration after all other passes, to
eliminate gaps in SSA IDs left by optimization. This keeps the maximum SSA ID
low so it fits within the backend's register file without spilling.

### 8.2 Inlining Details

- Only single-block functions are inlined (multi-block support is planned)
- Functions containing `CALL` instructions are not inlined
- Only one call is inlined per function per iteration
- SSA IDs are remapped by an offset to avoid conflicts
- `RET` in inlined body is replaced with `MOV` to call result register
- Both register and immediate call arguments are supported

---

## 9. Backend Lowering

DFIR is backend-agnostic. The implemented backends are:

| Backend | Use Case | Status |
|---------|----------|--------|
| aarch64 (Mach-O) | macOS Apple Silicon | ✅ Implemented |
| x86-64 (ELF) | Linux x86-64 | ✅ Implemented |
| Cranelift | Fast compilation | Planned |
| C | Production builds | Planned |
| Interpreter | Debugging, REPL | Planned |

### 9.1 Implemented Features by Backend

| Feature | aarch64 | x86-64 |
|---------|---------|--------|
| Arithmetic (add/sub/mul/div/mod) | ✅ | ✅ |
| Bitwise (and/or/xor/not/shl/shr) | ✅ | ✅ |
| Comparison (eq/ne/lt/le/gt/ge) | ✅ | ✅ |
| Control flow (br/br_cond/ret) | ✅ | ✅ |
| Function calls (with arg passing) | ✅ | ✅ |
| print/println builtins | ✅ | ✅ |
| String literals | ✅ | ✅ |
| Struct field access | ✅ | ✅ |
| Enum (make/check/extract) | ✅ | ✅ |
| Loop support (for/while/loop) | ✅ | ✅ |
| Break/continue | ✅ | ✅ |
| Callee-saved register preservation | ✅ | ✅ |
| Graph runtime | ✅ | N/A |

### 9.2 Lowering Rules

| DFIR Construct | aarch64 | x86-64 |
|----------------|---------|--------|
| `func` | Basic blocks with B/CBZ/Bcond | Basic blocks with JMP/Jcc |
| SSA registers | X0-X28 (direct mapping) | RAX-R15 (direct mapping) |
| `call` | BL with X0-X7 args | CALL with RDI/RSI/RDX/RCX/R8/R9 args |
| `const` | MOVZ/MOVK or ADR (strings) | MOV imm or LEA (strings) |
| `br` | B (patched offset) | JMP (patched offset) |
| `br_cond` | CBZ + B | Jcc |
| `ret` | RET (with epilogue) | RET (with epilogue) |
| `make_enum` | MOVZ + consecutive regs | MOV imm + consecutive regs |
| `get_field` | ORR (mov from base+idx reg) | MOV (from base+idx reg) |
| `check_variant` | CMP + CSET | CMP + SETZ |
| `print/println` | ADR + BL puts/printf | LEA + CALL puts/printf |

---

## 10. Textual Format (Debugging)

DFIR has a human-readable textual format (shown throughout this document)
used for:

- Debugging compiler output (`--emit-dfir` flag)
- IR-level testing (golden file tests)
- Documentation and education

### 10.1 Grammar (Informal)

```
module      := "module" "@" name "{" (decl)* "}"
decl        := typedef | func | coro | graph
typedef     := "type" "%" name "=" type_expr
func        := "func" "@" name "(" params ")" "->" type "{" block* "}"
coro        := "coro" "@" name "(" params ")" "->" type "{" state* "}"
graph       := "graph" "@" name "{" (node | edge)* "}"
block       := "$" name ":" instr* terminator
state       := "state" name ":" instr* transition
instr       := "%" name "=" opcode type? operand* | opcode operand*
terminator  := "br" target | "br_cond" val target target | "switch" ... | "ret" val
transition  := "br" state | "switch" ... | "ret" val
node        := "node" "%" name "=" "@" name "(" args ")"
edge        := "edge" "%" name "." port "->" "%" name "." port ":" "chan" "<" type "," int ">"
```

---

## 11. Design Rationale

### 11.1 Why a Custom IR Instead of LLVM?

| Factor | LLVM IR | DFIR |
|--------|---------|------|
| Data flow awareness | No — channels are opaque calls | Yes — `recv`/`send` are first-class |
| Graph topology | No representation | `dfir.graph` layer |
| Coroutine states | Implicit (lowered to functions) | Explicit named states |
| Instruction count | ~200+ | ~25 |
| Build dependency | Heavy (LLVM ~1GB) | None (self-contained) |
| Backend flexibility | Tied to LLVM | Cranelift, C, interpreter |

### 11.2 Why SSA?

- Enables straightforward copy propagation and dead code elimination.
- No need for register allocation before optimization — SSA values are
  virtual registers.
- Well-understood theory with simple algorithms.

### 11.3 Why Stackless Coroutines?

- Lower memory: each suspended coroutine only needs its state enum, not a
  full stack.
- Faster context switch: no stack pointer swap.
- Same model as Rust's `async fn` and C++20 coroutines.

---

## 12. Source Language Features

The bootstrap compiler supports the following source language (`.al`) features:

### 12.1 Types

| Type | Syntax | Status |
|------|--------|--------|
| Integers | `i8 i16 i32 i64 u8 u16 u32 u64` | ✅ |
| Floats | `f16 f32 f64` | Parsed (backend partial) |
| Boolean | `bool` | ✅ |
| String | `string` | ✅ |
| Struct | `struct Name { field: T, ... }` | ✅ |
| Enum (unit) | `enum Name { Var1, Var2, ... }` | ✅ |
| Enum (tuple) | `enum Name { Var(T), ... }` | ✅ |
| Enum (struct) | `enum Name { Var{ f: T, ... } }` | Parsed |
| Reference | `&T`, `&mut T` | Parsed |
| Stream | `stream<T>` | Parsed |
| Channel | `chan<T>` | Parsed |

### 12.2 Statements

| Statement | Syntax | Status |
|-----------|--------|--------|
| Let binding | `let x: T = expr` | ✅ |
| Reassignment | `mut x = expr` | ✅ |
| Field assignment | `mut p.x = expr` | ✅ |
| If/else | `if cond { } else { }` | ✅ |
| While loop | `while cond { }` | ✅ |
| For loop | `for x in 0..N { }` | ✅ |
| Infinite loop | `loop { }` | ✅ |
| Break | `break` | ✅ |
| Continue | `continue` | ✅ |
| Return | `return expr` | ✅ |
| Match | `match x { Pat => expr, ... }` | ✅ |
| Expression | `expr` | ✅ |

### 12.3 Expressions

| Expression | Syntax | Status |
|------------|--------|--------|
| Literals | `42`, `3.14`, `"hello"`, `true` | ✅ |
| Identifiers | `x` | ✅ |
| Binary ops | `+ - * / % & \| ^ << >>` | ✅ |
| Comparison | `== != < <= > >=` | ✅ |
| Logical | `&& \|\| !` | ✅ |
| Function call | `f(a, b)` | ✅ |
| Enum variant | `Some(42)` | ✅ |
| Struct field | `p.x` | ✅ |
| Array index | `a[i]` | Parsed |
| Range | `0..5`, `0..=5` | ✅ |
| If expression | `if c { a } else { b }` | ✅ |
| Match expression | `match x { ... }` | ✅ |
| Cast | `expr as T` | Parsed |
| Yield | `yield expr` | Parsed |

### 12.4 Builtins

| Builtin | Syntax | Description |
|---------|--------|-------------|
| `print` | `print(expr)` | Print string or integer without newline |
| `println` | `println(expr)` | Print string or integer with newline |
| `__mem_load` | `__mem_load(ptr)` | Load 8 bytes from memory address |
| `__mem_store` | `__mem_store(ptr, val)` | Store 8 bytes to memory address |
| `__byte_load` | `__byte_load(ptr, idx)` | Load 1 byte from ptr+idx |
| `__byte_store` | `__byte_store(ptr, idx, val)` | Store 1 byte to ptr+idx |
| `__malloc` | `__malloc(size)` | Allocate memory via mmap |
| `__alloca` | `__alloca(size)` | Allocate on stack (16-byte aligned) |
| `__syscall` | `__syscall(num, args...)` | Direct syscall (up to 6 args) |
| `__str_eq` | `__str_eq(s1, s2)` | String equality (1=equal, 0=not) |
| `__str_len` | `__str_len(s)` | String length (null-terminated) |
| `source` | `source("file:path")` | Graph source node |
| `sink` | `sink("stdout")` | Graph sink node |

See **Appendix B: Builtins Reference** for detailed codegen documentation.

### 12.5 Graph Syntax

```
graph main {
    source("file:input") |> double |> sink("stdout")
}
```

Graph pipelines compile to an executable `main` function that:
1. Generates input values (source: counter 0..9)
2. Applies each transform function in sequence
3. Prints each result (sink: println)

### 12.6 Match with Data Extraction

```
enum Option { Some(i32), None }

fn main() (r: i32) {
    let x = Some(42)
    match x {
        Some(v) => mut r = v,  // v is bound to the extracted data
        None => mut r = 0
    }
}
```

---

## 13. Open Questions

- [ ] Should DFIR support a binary serialization format (for caching)?
- [ ] Should `dfir.graph` support dynamic (runtime) graph modification?
- [ ] Should there be a `dfir.extern` declaration for FFI?
- [ ] What is the exact `phi` semantics for coro states (across poll calls)?
- [ ] Should node fusion produce a new `dfir.coro` or inline into the graph?
- [ ] How are generic types instantiated in DFIR (monomorphization)?

---

## 14. Builtins Reference

The self-hosting compiler (`parser.al`) implements a set of built-in functions
that are recognized by name and compiled to inline assembly. Builtins with the
`__` prefix are low-level operations; `print`/`println` are higher-level I/O
builtins.

### 14.1 Builtin Dispatch

The compiler dispatches builtins through two functions:

- **`is_builtin_name(name)`** — Checks if a name is a builtin by examining the
  first few bytes of the name string. Recognizes:
  - `__byte_*` (prefix `__b`)
  - `__mem_*` (prefix `__me` for mem_load, `__ma` for malloc)
  - `__alloca` (prefix `__al`)
  - `__str_*` (prefix `__st` for str_eq, `__sy` for syscall)
  - `print` / `println` (prefix `p`)

- **`gen_call_builtin(name, arg_count)`** — Dispatches to the appropriate
  code generator based on the name's byte pattern. The `print`/`println`
  builtins are handled separately in `gen_call()` based on argument type
  (string vs. integer).

### 14.2 Complete Builtin Table

| Builtin | Signature | Codegen | Description |
|---------|-----------|---------|-------------|
| `__mem_load` | `(ptr: i64) -> i64` | `LDR X0, [X0]` | Load 8 bytes from memory address `ptr`. Pops 1 arg, returns loaded value in X0. |
| `__mem_store` | `(ptr: i64, val: i64) -> void` | `STR X0, [X1]` | Store 8 bytes `val` to address `ptr`. Pops 2 args (ptr, val), reorders to STR. |
| `__byte_load` | `(ptr: i64, idx: i64) -> i64` | `LDRB X0, [X0, X1]` | Load 1 byte from `ptr + idx`. Pops 2 args, returns byte value in X0. |
| `__byte_store` | `(ptr: i64, idx: i64, val: i64) -> void` | `STRB X0, [X1, X2]` | Store 1 byte `val` to `ptr + idx`. Pops 3 args. |
| `__malloc` | `(size: i64) -> i64` | `mmap` syscall | Allocate memory via `mmap(0, size, PROT_RW, MAP_PRIVATE\|MAP_ANON, -1, 0)`. Returns pointer in X0. |
| `__alloca` | `(size: i64) -> i64` | `SUB SP, SP, #aligned` | Allocate on stack. Size is rounded up to 16-byte alignment (`(size + 15) & ~15`). Returns stack pointer. |
| `__syscall` | `(num, a0..a5) -> i64` | `SVC` instruction | Direct syscall. Pops up to 6 args to X0-X5, syscall number to X6. On macOS: `MOV X16, X6; SVC #0x80`. On Linux/FreeBSD: `MOV X8, X6; SVC #0`. |
| `__str_eq` | `(s1: i64, s2: i64) -> i64` | Inline loop | Byte-by-byte string comparison. Returns 1 if equal, 0 if not. Uses caller-saved register save/restore. |
| `__str_len` | `(s: i64) -> i64` | Inline loop | Computes length of null-terminated string. Scans bytes until `\\0`, returns count in X0. |
| `print` | `(val) -> void` | `write` syscall | Print string or integer. For strings: computes strlen, calls `write(1, str, len)`. For integers: converts to decimal string on stack, calls `write`. |
| `println` | `(val) -> void` | `write` syscall × 2 | Same as `print`, then writes a newline (`\\n`) via a second `write(1, "\\n", 1)` syscall. |

### 14.3 Print Type Dispatch

The `print`/`println` builtins inspect the argument's AST type to determine
whether to emit string or integer printing code:

- If the argument is a **string literal** (AST kind = 2) or an expression
  wrapping a string literal, `gen_print_builtin()` is called, which computes
  `strlen` inline and calls `write(1, str, len)`.
- Otherwise, `gen_print_int_builtin()` is called, which converts the integer
  to a decimal string on the stack (handling negative numbers and zero) and
  calls `write(1, buf, len)`.

### 14.4 Malloc / Memory Allocation

`__malloc(size)` emits a direct `mmap` syscall:

| Parameter | Value |
|-----------|-------|
| `addr` | 0 (kernel chooses) |
| `size` | from argument |
| `prot` | 3 (`PROT_READ \| PROT_WRITE`) |
| `flags` | 4098 (`MAP_PRIVATE \| MAP_ANONYMOUS`) |
| `fd` | -1 |
| `offset` | 0 |

Syscall numbers: macOS = 197, Linux = 222, FreeBSD = 477.

### 14.5 Syscall Convention

| Platform | Syscall # Register | Instruction |
|----------|-------------------|-------------|
| macOS (aarch64) | X16 | `SVC #0x80` |
| Linux (aarch64) | X8 | `SVC #0` |
| FreeBSD (aarch64) | X8 | `SVC #0` |

Arguments are passed in X0-X5. Return value is in X0.

---

## 15. Enum Support

### 15.1 Declaration Syntax

```
enum Name {
    Variant(Type),     // variant with payload
    Variant2,          // bare variant (no payload)
    Variant3(Type),    // another variant with payload
    ...
}
```

### 15.2 Internal Representation

Enum variants are stored in three parallel arrays during compilation:

| Array | Contents |
|-------|----------|
| `g_enum_name` | Variant name string pointer |
| `g_enum_tag` | Tag ID (sequential integer starting from 0) |
| `g_enum_has_arg` | 1 if variant has a payload, 0 if bare |

The `enum_add(name, tag, has_arg)` function registers a new variant. Tag IDs
are assigned sequentially starting from 0, in declaration order.

### 15.3 Lookup

- **`enum_lookup(name)`** — Searches the variant table by name. Returns the
  tag ID if found, or -1 if not found.
- **`enum_has_arg_lookup(name)`** — Returns 1 if the variant has a payload
  argument, 0 otherwise.

### 15.4 Constructor Codegen

When the compiler encounters a call expression like `Some(42)`, it first
checks `enum_lookup()`. If the name matches a variant, `gen_enum_constructor()`
is called instead of a normal function call.

**Variant with argument** (e.g., `Some(42)`):

1. Pop the argument value from the stack → X0, then push it back (to preserve
   it during allocation).
2. Call `gen_enum_alloc()` — allocates 16 bytes via `mmap`.
3. Pop the argument → X1.
4. Store the argument at `[ptr + 8]` (offset 8).
5. Store the tag ID at `[ptr + 0]` (offset 0).
6. Result pointer is in X0.

**Bare variant** (e.g., `None`):

1. If there are arguments on the stack, discard them.
2. Call `gen_enum_alloc()` — allocates 16 bytes via `mmap`.
3. Store the tag ID at `[ptr + 0]` (offset 0).
4. Result pointer is in X0.

### 15.5 Memory Layout

All enum values occupy **16 bytes**:

```
Offset 0:  Tag ID (i64)     — identifies the variant
Offset 8:  Payload (i64)    — the variant's argument (if any)
```

Bare variants leave offset 8 uninitialized.

### 15.6 Enum Allocation

`gen_enum_alloc()` allocates 16 bytes using the same `mmap` approach as
`__malloc`. It saves/restores caller-saved registers around the syscall and
adjusts the stack pointer afterward.

---

## 16. Match Expression

### 16.1 Syntax

```
match scrutinee {
    Pattern => body,
    Pattern(var) => body,   // variant with argument binding
    ...
}
```

- `Pattern` is an enum variant name.
- `var` binds the variant's payload to a local variable.
- `body` is a statement or expression.
- Cases are separated by commas.

### 16.2 AST Representation

The match expression is represented in the AST as:

| Field | Contents |
|-------|----------|
| `g_ast_a` | Scrutinee expression |
| `g_ast_b` | Linked list of case nodes |

Each case node (AST kind = 20 for linked list nodes) contains:

| Field | Contents |
|-------|----------|
| `g_ast_val` | Pattern (variant name) |
| `g_ast_a` | Bind variable (or 0 if no binding) |
| `g_ast_b` | Body statement |

The `emit_match(scrutinee, first_case)` and `emit_case(pattern, bind_var, body)`
functions construct these AST nodes.

### 16.3 Codegen (`gen_match`)

The match codegen works as follows:

1. **Evaluate scrutinee** — `gen_expr(scrut)` computes the scrutinee value
   (a pointer to the enum's 16-byte allocation) and pushes it onto the stack.

2. **Iterate over cases** — For each case:
   a. Load the enum pointer from the stack: `LDR X1, [SP]`
   b. Dereference to get the pointer value: `LDR X1, [X1]`
   c. Look up the pattern's tag: `tag = enum_lookup(pat)`
   d. If `tag >= 0` (pattern is a known variant):
      - Load the tag into a register: `MOV X2, #tag`
      - Compare: `CMP X1, X2`
      - If not equal, branch to next case (conditional branch, patched later).
      - If the variant has an argument and a bind variable:
        - Load the enum pointer from stack: `LDR X2, [SP]`
        - Load the payload: `LDR X0, [X2, #8]` (offset 8 = payload)
        - Store the payload to the local variable: `STUR X0, [FP, -(off+1)*8]`
      - Generate the body code: `gen_stmt(body)`
      - Emit an unconditional branch to the end (patched later).
   e. If `tag < 0` (pattern is not a variant — could be a literal match in
      future extensions), the case is skipped.

3. **Patch all end-of-case branches** — After all cases are emitted, all
   the unconditional branches to "done" are patched to point to the current
   code position.

4. **Clean up** — Pop the scrutinee from the stack (`gen_pop_discard()`).

### 16.4 Variable Binding

For variants with arguments (e.g., `Some(v)`), the payload is loaded from
`[enum_ptr + 8]` and stored to the local variable's stack slot at
`[FP - (offset + 1) * 8]`. If the variable doesn't exist yet, it is
created with `var_add()`.

### 16.5 Example

Source:
```
enum Option { Some(i32), None }

fn main() (r: i32) {
    let x = Some(42)
    match x {
        Some(v) => mut r = v,
        None => mut r = 0
    }
}
```

Generated assembly (conceptual):
```
    ; Evaluate scrutinee: x → pointer to enum (16 bytes)
    ; Push pointer on stack

    ; Case 1: Some(v)
    LDR X1, [SP]          ; load enum pointer
    LDR X1, [X1]          ; dereference to get pointer value
    MOV X2, #0            ; tag for Some = 0
    CMP X1, X2
    B.NE case2            ; if not Some, skip to next case
    LDR X2, [SP]          ; reload enum pointer
    LDR X0, [X2, #8]      ; load payload (offset 8)
    STUR X0, [FP, #-8]    ; store to local variable v
    ; ... body: mut r = v ...
    B done

    ; Case 2: None
case2:
    LDR X1, [SP]
    LDR X1, [X1]
    MOV X2, #1            ; tag for None = 1
    CMP X1, X2
    B.NE default
    ; ... body: mut r = 0 ...
    B done

done:
    ; Pop scrutinee from stack
```

---

## 17. Self-Compilation Chain

The alang compiler achieves self-hosting through an iterative bootstrap
process. The bootstrap C compiler (`minica_test_build`) compiles the
self-hosting compiler source (`parser.al`) into a native binary. Each
generation of the compiler can then compile the next.

### 17.1 Bootstrap Levels

| Level | Compiler | Description |
|-------|----------|-------------|
| **L0** | `minica_test_build` (C) | Bootstrap compiler written in C, compiled with `make minica_test_build` |
| **L1** | L0 compiles `parser.al` | First native alang compiler binary |
| **L2** | L1 compiles `parser.al` | Second generation compiler |
| **L3** | L2 compiles `parser.al` | Third generation compiler |
| **L4** | L3 compiles `parser.al` | Fourth generation compiler |

### 17.2 Build Process

```bash
# Step 1: Build the C bootstrap compiler (L0)
cd bootstrap && make minica_test_build

# Step 2: L0 compiles parser.al → L1 object file
./bootstrap/minica_test_build selfhost/parser.al /tmp/L1.o --aarch64 --mach-o

# Step 3: Link L1
ld -arch arm64 -platform_version macos 14.0 14.0 -o /tmp/L1 /tmp/L1.o \
    -l System -syslibroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk \
    -e _main

# Step 4: L1 compiles parser.al → L2
/tmp/L1 selfhost/parser.al /tmp/L2.o --aarch64 --mach-o
ld -arch arm64 -platform_version macos 14.0 14.0 -o /tmp/L2 /tmp/L2.o \
    -l System -syslibroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk \
    -e _main

# Step 5: L2 compiles parser.al → L3, L3 compiles → L4 (repeat)
```

### 17.3 Fixed Point Verification

The self-compilation chain reaches a **fixed point** when consecutive
generations produce byte-identical object files:

```
L3.o == L4.o  (byte-identical)
```

This demonstrates that the compiler is fully self-hosting: it can compile
its own source code reproducibly, and the output has stabilized.

### 17.4 L1 Usage in Tests

The test scripts (`selfhost/tests/run_tests.sh` and
`selfhost/tests/run_examples.sh`) build L1 and use it as the compiler for
testing individual compiler features and example programs.

---

## 18. Stack Frame Layout

### 18.1 Prologue

Every function begins with the following prologue (aarch64):

```asm
STP  FP, LR, [SP, #-16]!    ; Save frame pointer and link register
ADD  FP, SP, #0              ; Set FP = SP (frame base)
SUB  SP, SP, #2048           ; Allocate 2048 bytes for local variables
```

Encoding:
- `STP X29, X30, [SP, #-16]!` — pre-index store, pushes FP and LR
- `ADD X29, SP, #0` — establishes the frame pointer
- `SUB SP, SP, #2048` — reserves 2048 bytes for locals

### 18.2 Epilogue

Every function ends with the following epilogue:

```asm
ADD  SP, SP, #2048           ; Deallocate local variable space
LDP  FP, LR, [SP], #16       ; Restore frame pointer and link register
RET                           ; Return to caller
```

Encoding:
- `ADD SP, SP, #2048` — restores SP to pre-local-alloc position
- `LDP X29, X30, [SP], #16` — post-index load, pops FP and LR
- `RET` — returns via LR

### 18.3 Local Variable Storage

Local variables are stored on the stack relative to the frame pointer (FP/X29):

```
Local variable at index `offset`:
    Address = [FP - (offset + 1) * 8]
```

- The first local variable (offset 0) is at `[FP - 8]`.
- The second (offset 1) is at `[FP - 16]`.
- And so on, growing downward.

With 2048 bytes of local space, up to 256 local variables (i64-sized) can
be stored.

### 18.4 Stack Frame Diagram

```
High addresses
    ┌──────────────────┐
    │ Caller's frame   │
    ├──────────────────┤
    │ LR (return addr) │  ← [FP + 8]
    │ FP (saved)       │  ← [FP + 0] = FP
    ├──────────────────┤
    │ Local var 0      │  ← [FP - 8]
    │ Local var 1      │  ← [FP - 16]
    │ Local var 2      │  ← [FP - 24]
    │ ...              │
    │ Local var 255    │  ← [FP - 2048] = SP
    ├──────────────────┤
    │ Caller-save area │  ← SP grows down during calls
    │ (160 bytes)      │
    └──────────────────┘
Low addresses
```

### 18.5 Caller-Saved Register Save/Restore

During function calls and syscalls, the compiler saves and restores
caller-saved registers to preserve them across the call.

**`gen_caller_save()`** emits:

```asm
SUB  SP, SP, #160            ; Allocate 160-byte save area
STP  X0,  X1,  [SP, #16]     ; Save X0-X1
STP  X2,  X3,  [SP, #32]     ; Save X2-X3
STP  X4,  X5,  [SP, #48]     ; Save X4-X5
STP  X6,  X7,  [SP, #64]     ; Save X6-X7
STP  X8,  X9,  [SP, #80]     ; Save X8-X9
STP  X10, X11, [SP, #96]     ; Save X10-X11
STP  X12, X13, [SP, #112]    ; Save X12-X13
STP  X14, X15, [SP, #128]    ; Save X14-X15
```

- Registers X0 through X15 are saved (16 registers, 128 bytes).
- Slots `[SP, #0]` and `[SP, #8]` are reserved: `[SP, #8]` is used as a
  return-value save slot (`gen_save_retval` / `gen_load_retval`).

**`gen_caller_restore()`** emits the corresponding LDP instructions to
restore all 16 registers, followed by `ADD SP, SP, #160` (via `gen_add_sp()`).

**Return value handling:**
- `gen_save_retval()`: `STR X0, [SP, #8]` — saves return value before restore.
- `gen_load_retval()`: `LDR X0, [SP, #8]` — restores return value after
  register restore. (X19, used for return values in some paths, is
  callee-saved and survives calls without explicit save.)

---

## 19. Platform Support

The alang compiler supports multiple OS/ISA combinations. Both the
self-hosting compiler (`parser.al`) and the bootstrap compiler
(`minica_test_build`) support **aarch64** and **x86-64** backends.

### 19.1 Supported Targets

#### Self-Hosting Compiler (aarch64 + x86-64)

| Flag | Platform | ISA | `g_target_os` | `g_target_isa` | Object Format |
|------|----------|-----|---------------|----------------|---------------|
| (default) | macOS | aarch64 | 0 | 0 | Mach-O |
| `--target linux` | Linux | aarch64 | 1 | 0 | ELF |
| `--target freebsd` | FreeBSD | aarch64 | 2 | 0 | ELF |
| `--x86-64 --target linux` | Linux | x86-64 | 1 | 1 | ELF |
| `--x86-64 --exec` | Linux | x86-64 | 1 | 1 | ELF (ET_EXEC) |
| `--aarch64 --exec` | Linux | aarch64 | 1 | 0 | ELF (ET_EXEC) |

#### Bootstrap Compiler (aarch64 + x86-64)

| Flags | Platform | ISA | Object Format |
|-------|----------|-----|---------------|
| `--aarch64 --mach-o` (default) | macOS | aarch64 | Mach-O |
| `--aarch64 --elf` | Linux/FreeBSD | aarch64 | ELF |
| `--x86-64 --elf` | Linux | x86-64 | ELF |
| `--x86-64 --exec` | Linux | x86-64 | ELF (ET_EXEC) |

The `--exec` flag (both compilers) produces a static ELF executable
(ET_EXEC with PT_LOAD) instead of a relocatable object (ET_REL), for
direct QEMU testing. The entry point is at the `main` function offset.

### 19.2 Syscall Numbers

Syscall numbers differ across operating systems. The self-hosting compiler
uses two sets of globals:

- **Runtime syscalls** (`SC_*`): Used by the compiler's own runtime
  (always macOS aarch64, since the compiler runs on macOS Apple Silicon)
- **Target syscalls** (`TSC_*`): Used by code generation for the target
  platform (set per `--target` and `--x86-64`)

#### aarch64

| Syscall | macOS | Linux | FreeBSD | Runtime Global | Target Global |
|---------|-------|-------|---------|----------------|---------------|
| `read` | 3 | 63 | 3 | `SC_READ` | -- |
| `write` | 4 | 64 | 4 | `SC_WRITE` | `TSC_WRITE` |
| `open` | 5 | 56 | 5 | `SC_OPEN` | -- |
| `close` | 6 | 57 | 6 | `SC_CLOSE` | -- |
| `mmap` | 197 | 222 | 477 | `SC_MMAP` | `TSC_MMAP` |
| `exit` | 1 | 93 | 1 | `SC_EXIT` | -- |

#### x86-64

| Syscall | macOS | Linux | FreeBSD | Target Global |
|---------|-------|-------|---------|---------------|
| `read` | 0x2000003 | 0 | 3 | -- |
| `write` | 0x2000004 | 1 | 4 | `TSC_WRITE` |
| `open` | 0x2000005 | 2 | 5 | -- |
| `close` | 0x2000006 | 3 | 6 | -- |
| `mmap` | 0x20000C5 | 9 | 477 | `TSC_MMAP` |
| `exit` | 0x2000001 | 60 | 1 | -- |

> **Note:** macOS x86-64 syscalls have a 0x02000000 class offset.
> The self-hosting compiler currently targets Linux x86-64 only
> (since the host is macOS aarch64). The bootstrap compiler also
> targets Linux x86-64.

### 19.3 Syscall Calling Conventions (ABI)

#### aarch64 ABI

| OS | Syscall # Register | Trap Instruction | Arg Registers | Return Register |
|----|-------------------|-----------------|---------------|----------------|
| macOS | X16 | `SVC #0x80` | X0, X1, X2, X3, X4, X5 | X0 |
| Linux | X8 | `SVC #0` | X0, X1, X2, X3, X4, X5 | X0 |
| FreeBSD | X8 | `SVC #0` | X0, X1, X2, X3, X4, X5 | X0 |

Self-hosting compiler codegen sequence:
1. Pop args from stack into X0-X5 (reverse order)
2. Pop syscall number into X6
3. Save caller-saved registers (X0-X15) to stack (160-byte save area)
4. Reload args from saved area into X0-X5
5. Move syscall number to target register:
   - macOS: `MOV X16, X6`
   - Linux/FreeBSD: `MOV X8, X6`
6. Emit trap instruction:
   - macOS: `SVC #0x80`
   - Linux/FreeBSD: `SVC #0`
7. Save return value (X0) to stack at [SP+8]
8. Restore caller-saved registers
9. Load return value into X0 from [SP+8]
10. Deallocate 160-byte save area (`ADD SP, SP, #160`)

#### x86-64 ABI (Linux)

| OS | Syscall # Register | Trap Instruction | Arg Registers | Return Register |
|----|-------------------|-----------------|---------------|----------------|
| Linux | RAX | `SYSCALL` (0x0F 0x05) | RDI, RSI, RDX, R10, R8, R9 | RAX |

> **Note:** x86-64 uses **R10** instead of RCX for the 4th argument
> because the `SYSCALL` instruction clobbers RCX (stores return
> address) and R11 (stores RFLAGS).

Self-hosting compiler codegen sequence:
1. Pop args from stack into logical registers (reverse order)
2. Pop syscall number into logical reg 6
3. Save caller-saved registers (8 regs: RAX, RCX, RDX, RBX, RSI, RDI, R8, R9)
   to stack (160-byte save area at offsets 16-72)
4. Reload args from saved area into x86-64 ABI registers:
   - Arg 0 → RDI (logical 0 → x86_reg 7)
   - Arg 1 → RSI (logical 1 → x86_reg 6)
   - Arg 2 → RDX (logical 2 → x86_reg 2)
   - Arg 3 → R10 (logical 3 → x86_reg 10)
   - Arg 4 → R8  (logical 4 → x86_reg 8)
   - Arg 5 → R9  (logical 5 → x86_reg 9)
   - Syscall # → RAX (logical 6 → x86_reg 0)
5. Emit `SYSCALL` instruction (bytes: `0x0F 0x05`)
6. Save return value (RAX) to [RSP+8]
7. Restore caller-saved registers
8. Load return value into RAX from [RSP+8]
9. Deallocate 160-byte save area (`ADD RSP, 160`)

### 19.4 Register Mapping (x86-64)

The self-hosting compiler uses aarch64-style logical register numbers
(0-31) internally. The `x86_reg()` function maps these to x86-64
physical registers:

| Logical Reg | aarch64 Name | x86-64 Physical | x86-64 Encoding | Role |
|-------------|-------------|-----------------|-----------------|------|
| 0 | X0 | RAX | 0 | Return value / arg 0 |
| 1 | X1 | RCX | 1 | Arg 1 (shift count via CL) |
| 2 | X2 | RDX | 2 | Arg 2 / div remainder |
| 3 | X3 | RBX | 3 | Arg 3 (callee-saved) |
| 4 | X4 | RSI | 6 | Arg 4 (pointer source) |
| 5 | X5 | RDI | 7 | Arg 5 (pointer dest) |
| 6 | X6 | R8 | 8 | Extra arg |
| 7 | X7 | R9 | 9 | Extra arg |
| 17 | X17 (IP1) | R10 | 10 | Scratch / syscall arg4 |
| 19 | X19 | RBX | 3 | Callee-saved (global base) |
| 29 | X29 (FP) | RBP | 5 | Frame pointer |
| 30 | X30 (LR) | RAX | 0 | Link register (mapped to RAX) |
| 31 | X31 (SP/XZR) | RSP | 4 | Stack pointer / zero |

> **Note:** R11 is used as a scratch register for 3-operand emulation
> (e.g., `IMUL R11, src; MOV dst, ra; SUB dst, R11` for MSUB).
> R10 is used as scratch for MOVK emulation (OR with shifted immediate).

#### x86-64 Condition Code Mapping

The self-hosting compiler uses aarch64 condition codes internally.
The `gen_bcond` and `gen_cmp_*` functions map them to x86-64:

| aarch64 Cond | Meaning | x86-64 Cond | x86-64 SETcc |
|-------------|---------|------------|-------------|
| 0 (EQ) | Equal | 4 (JE/JZ) | SETE |
| 1 (NE) | Not equal | 5 (JNE/JNZ) | SETNE |
| 10 (GE) | Signed >= | 13 (JGE) | SETGE |
| 11 (LT) | Signed < | 12 (JL) | SETL |
| 12 (GT) | Signed > | 15 (JG) | SETG |
| 13 (LE) | Signed <= | 14 (JLE) | SETLE |

### 19.5 x86-64 Instruction Encodings

The self-hosting compiler emits x86-64 machine code directly via
`emit_byte()`. Key encoding patterns:

#### REX Prefix

```
REX = 0x40 | (W << 3) | (R << 2) | (X << 1) | B
```
- W=1: 64-bit operand size
- R: Extension of ModR/M reg field (for R8-R15)
- X: Extension of SIB index
- B: Extension of ModR/M r/m or opcode reg field

#### Common Instruction Formats

| Instruction | Encoding | Bytes |
|-------------|----------|-------|
| `MOV r64, imm64` | REX.W + B8+rd + imm64 | 10 |
| `MOV r64, r64` | REX.W + 89 + ModR/M(3, src, dst) | 3-4 |
| `ADD r64, r64` | REX.W + 01 + ModR/M(3, src, dst) | 3-4 |
| `SUB r64, r64` | REX.W + 29 + ModR/M(3, src, dst) | 3-4 |
| `IMUL r64, r64` | REX.W + 0F AF + ModR/M(3, dst, src) | 4-5 |
| `AND/OR/XOR r64, r64` | REX.W + 21/09/31 + ModR/M | 3-4 |
| `CMP r64, r64` | REX.W + 39 + ModR/M(3, src, dst) | 3-4 |
| `SHL/SHR r64, imm8` | REX.W + C1 + ModR/M(3, 4/5, dst) + imm8 | 4-5 |
| `SHL/SHR r64, CL` | REX.W + D3 + ModR/M(3, 4/5, dst) | 3-4 |
| `NEG r64` | REX.W + F7 + ModR/M(3, 3, dst) | 3-4 |
| `NOT r64` | REX.W + F7 + ModR/M(3, 2, dst) | 3-4 |
| `CDQ` | 99 | 1 |
| `IDIV r64` | REX.W + F7 + ModR/M(3, 7, src) | 3-4 |
| `ADD/SUB r64, imm32` | REX.W + 81 + ModR/M(3, 0/5, dst) + imm32 | 7 |
| `PUSH r64` | 50+rd (or 41 50+rd for R8-R15) | 1-2 |
| `POP r64` | 58+rd (or 41 58+rd for R8-R15) | 1-2 |
| `RET` | C3 | 1 |
| `SYSCALL` | 0F 05 | 2 |
| `JMP rel32` | E9 + rel32 | 5 |
| `CALL rel32` | E8 + rel32 | 5 |
| `Jcc rel32` | 0F 80+cc + rel32 | 6 |
| `SETcc r8` | REX + 0F 90+cc + ModR/M(3, 0, dst) | 4 |
| `MOVZX r64, r8` | REX.W + 0F B6 + ModR/M(3, dst, src) | 4-5 |
| `MOV r64, [base+disp32]` | REX.W + 8B + ModR/M(2, dst, base) + disp32 | 7 |
| `MOV [base+disp32], r64` | REX.W + 89 + ModR/M(2, src, base) + disp32 | 7 |
| `MOVZX r64, byte [base+disp32]` | REX.W + 0F B6 + ModR/M(2, dst, base) + disp32 | 7-8 |
| `MOV byte [base+disp32], r8` | REX + 88 + ModR/M(2, src, base) + disp32 | 6-7 |
| `LEA r64, [RIP+disp32]` | REX.W + 8D + ModR/M(0, dst, 5) + disp32 | 7 |

#### Branch Offset Adjustment

x86-64 branches use relative offsets from the **end** of the instruction,
while aarch64 uses offsets from the **start** of the instruction.
The self-hosting compiler adjusts:

| Instruction | aarch64 Size | x86-64 Size | Offset Adjustment |
|-------------|-------------|-------------|-------------------|
| `B` (unconditional) | 4 | 5 (JMP rel32) | `offset - 5` |
| `BL` (call) | 4 | 5 (CALL rel32) | `offset - 5` |
| `B.cond` | 4 | 6 (Jcc rel32) | `offset - 6` |

Patch functions (`patch_b`, `patch_bcond`) write the rel32 value at
the correct offset within the instruction:
- `patch_b`: writes 4 bytes at `pos + 1` (after 0xE9 opcode)
- `patch_bcond`: writes 4 bytes at `pos + 2` (after 0x0F 0x80+cc)

#### 3-Operand Emulation

aarch64 instructions are 3-operand (`dst = src0 op src1`), but x86-64
instructions are 2-operand (`dst op= src`). The self-hosting compiler
handles this:

- **Commutative ops** (ADD, MUL, AND, OR, XOR): If `dst == rm`, swap
  operands: `ADD dst, rn` instead of `MOV dst, rn; ADD dst, rm`.
- **Non-commutative ops** (SUB): If `dst == rm`, use scratch R11:
  `MOV R11, rm; MOV dst, rn; SUB dst, R11`.
- **General case** (`dst != rn && dst != rm`): `MOV dst, rn; OP dst, rm`.

#### String Literal Addressing

aarch64 uses `ADR Xd, label` (PC-relative, 4 bytes). x86-64 uses
`LEA RAX, [RIP+disp32]` (7 bytes: `48 8D 05 + disp32`).

The `patch_str_adrs` function patches the displacement after all code
is emitted:
- aarch64: patches 4-byte ADR instruction at `adr_pos`
- x86-64: patches 4-byte disp32 at `adr_pos + 3`, value = `str_off - adr_pos - 7`

### 19.6 Stack Frame Layout

#### aarch64 Stack Frame (2048 bytes)

```
High Address
  [Saved X29, X30]     ← Frame pointer (X29) points here
  [2048 bytes locals]  ← Variables at X29 + offset
Low Address (SP)
```

- Prologue: `STP X29, X30, [SP, #-16]!; MOV X29, SP; SUB SP, SP, #2048`
- Epilogue: `ADD SP, SP, #2048; LDP X29, X30, [SP], #16; RET`

#### x86-64 Stack Frame (2048 bytes)

```
High Address
  [Saved RBP]          ← Frame pointer (RBP) points here
  [2048 bytes locals]  ← Variables at RBP - offset
Low Address (RSP)
```

- Prologue: `PUSH RBP; MOV RBP, RSP; SUB RSP, 2048`
- Epilogue: `ADD RSP, 2048; POP RBP; RET`

#### Caller-Saved Register Area (160 bytes)

Both ISAs allocate a 160-byte save area for caller-saved registers
before function calls and syscalls:

| Offset | aarch64 | x86-64 |
|--------|---------|--------|
| 0-7 | (unused) | (unused) |
| 8-15 | X0 (return value) | RAX (return value) |
| 16-23 | X0 | RAX |
| 24-31 | X1 | RCX |
| 32-39 | X2 | RDX |
| 40-47 | X3 | RBX |
| 48-55 | X4 | RSI |
| 56-63 | X5 | RDI |
| 64-71 | X6 | R8 |
| 72-79 | X7 | R9 |
| 80-135 | X8-X15 | (unused) |
| 136-159 | (padding) | (padding) |

### 19.7 mmap Flags

The `__malloc` builtin calls `mmap` with platform-specific flags.
The `T_MAP_FLAGS` global holds the complete flags value:

| OS | MAP_ANON Value | Full Flags Value | `T_MAP_FLAGS` |
|----|---------------|-----------------|---------------|
| macOS | 0x1000 | `MAP_PRIVATE | MAP_ANON` = 0x1002 | 4098 |
| Linux | 0x20 | `MAP_PRIVATE | MAP_ANONYMOUS` = 0x22 | 34 |
| FreeBSD | 0x1000 | `MAP_PRIVATE | MAP_ANON` = 0x100E | 4110 |

mmap parameters for `__malloc(size)`:

| Parameter | Value |
|-----------|-------|
| `addr` | 0 (kernel chooses) |
| `size` | from argument |
| `prot` | 3 (`PROT_READ | PROT_WRITE`) |
| `flags` | `T_MAP_FLAGS` (platform-dependent, see above) |
| `fd` | -1 |
| `offset` | 0 |

### 19.8 Output Formats

| Compiler | Platform | ISA | Format | Flag |
|----------|----------|-----|--------|------|
| Self-hosting | macOS | aarch64 | Mach-O | (default) |
| Self-hosting | Linux | aarch64 | ELF (ET_REL) | `--target=linux --elf` |
| Self-hosting | Linux | aarch64 | ELF (ET_EXEC) | `--target=linux --exec` |
| Self-hosting | FreeBSD | aarch64 | ELF (ET_REL) | `--target=freebsd --elf` |
| Self-hosting | FreeBSD | aarch64 | ELF (ET_EXEC) | `--target=freebsd --exec` |
| Self-hosting | Linux | x86-64 | ELF (ET_REL) | `--x86-64 --target=linux --elf` |
| Self-hosting | Linux | x86-64 | ELF (ET_EXEC) | `--x86-64 --target=linux --exec` |
| Bootstrap | macOS | aarch64 | Mach-O | `--aarch64 --mach-o` |
| Bootstrap | Linux | aarch64 | ELF | `--aarch64 --elf` |
| Bootstrap | Linux | x86-64 | ELF | `--x86-64 --elf` |
| Bootstrap | Linux | x86-64 | ELF (ET_EXEC) | `--x86-64 --exec` |

The `--exec` flag produces a static ELF executable (ET_EXEC) with a PT_LOAD
program header, load address 0x400000, and entry point at the `main`
function offset. This allows the binary to run directly as `/init` in a
QEMU initramfs without a linker.

For `--exec` mode, the compiler also emits an exit syscall at the end of
`main()` to properly terminate the process:
- aarch64 Linux: `MOV X8, #93; SVC #0` (exit)
- aarch64 macOS: `MOV X16, #1; SVC #0x80` (exit)
- x86-64 Linux: `MOV RAX, #60; SYSCALL` (exit_group)

### 19.9 Runtime (No libc)

Both the self-hosting and bootstrap compilers use **direct syscalls**
exclusively -- no C library dependency. All I/O operations are implemented
via `__syscall`. Memory allocation uses `mmap` directly via `__malloc`.

Self-hosting compiler runtime functions in `parser.al`:

| Function | Syscall | Description |
|----------|---------|-------------|
| `sys_write(fd, buf, len)` | `SC_WRITE` | Write to file descriptor |
| `sys_read(fd, buf, len)` | `SC_READ` | Read from file descriptor |
| `sys_open(path, flags, mode)` | `SC_OPEN` | Open file |
| `sys_close(fd)` | `SC_CLOSE` | Close file descriptor |
| `sys_mmap(...)` | `SC_MMAP` | Memory map |
| `sys_exit(code)` | `SC_EXIT` | Terminate process |
| `malloc(size)` | `SC_MMAP` | Allocate memory (mmap wrapper) |
| `free(ptr)` | -- | No-op (memory not freed) |
| `fopen(path, mode)` | `SC_OPEN` | Open file, return fd |
| `fclose(fd)` | `SC_CLOSE` | Close file descriptor |
| `fread(buf, size, nmemb, fd)` | `SC_READ` | Read from file |
| `fwrite(buf, size, nmemb, fd)` | `SC_WRITE` | Write to file |
| `puts(str)` | `SC_WRITE` | Print null-terminated string |

### 19.10 QEMU Emulation Testing

The compiler supports QEMU-based runtime testing for generated binaries:

| Platform | QEMU Binary | Kernel Source | Status |
|----------|-------------|---------------|--------|
| aarch64 Linux | `qemu-system-aarch64` | Debian netboot `linux` | 12/12 tests pass |
| x86-64 Linux | `qemu-system-x86_64` | Debian `vmlinuz-amd64` | 1/1 verified (arith.al) |
| FreeBSD | `qemu-system-aarch64` | FreeBSD bootonly ISO | Verification only |

QEMU tests compile programs as static ELF executables (`--exec` flag),
package them as `/init` in a minimal initramfs (cpio archive), boot the
Linux kernel in QEMU, and verify exit codes. The kernel panics after
`/init` exits (expected behavior for PID 1), and the exit code is
extracted from the panic message (`exitcode=0xNNNN`).

Test script: `selfhost/tests/run_qemu_tests.sh`


## Appendix A: Full Instruction Reference

| # | Instruction | Category | Layer | Form |
|---|-------------|----------|-------|------|
| 1 | `const` | Value | func, coro | `%r = const T <lit>` |
| 2 | `phi` | Value | func | `%r = phi T [v, b]...` |
| 3 | `add` | Arith | func, coro | `%r = add T %a, %b` |
| 4 | `sub` | Arith | func, coro | `%r = sub T %a, %b` |
| 5 | `mul` | Arith | func, coro | `%r = mul T %a, %b` |
| 6 | `div` | Arith | func, coro | `%r = div T %a, %b` |
| 7 | `udiv` | Arith | func, coro | `%r = udiv T %a, %b` |
| 8 | `rem` | Arith | func, coro | `%r = rem T %a, %b` |
| 9 | `urem` | Arith | func, coro | `%r = urem T %a, %b` |
| 10 | `neg` | Arith | func, coro | `%r = neg T %a` |
| 11 | `eq` | Cmp | func, coro | `%r = eq T %a, %b` |
| 12 | `ne` | Cmp | func, coro | `%r = ne T %a, %b` |
| 13 | `lt` | Cmp | func, coro | `%r = lt T %a, %b` |
| 14 | `le` | Cmp | func, coro | `%r = le T %a, %b` |
| 15 | `gt` | Cmp | func, coro | `%r = gt T %a, %b` |
| 16 | `ge` | Cmp | func, coro | `%r = ge T %a, %b` |
| 17 | `and` | Logic | func, coro | `%r = and T %a, %b` |
| 18 | `or` | Logic | func, coro | `%r = or T %a, %b` |
| 19 | `xor` | Logic | func, coro | `%r = xor T %a, %b` |
| 20 | `not` | Logic | func, coro | `%r = not T %a` |
| 21 | `shl` | Logic | func, coro | `%r = shl T %a, %b` |
| 22 | `shr` | Logic | func, coro | `%r = shr T %a, %b` |
| 23 | `alloc` | Memory | func, coro | `%r = alloc T` |
| 24 | `load` | Memory | func, coro | `%r = load T %ptr` |
| 25 | `store` | Memory | func, coro | `store T %val, %ptr` |
| 26 | `memcpy` | Memory | func, coro | `memcpy %dst, %src, %n` |
| 27 | `get_field` | Struct | func, coro | `%r = get_field %s, <idx>` |
| 28 | `set_field` | Struct | func, coro | `set_field %s, <idx>, %v` |
| 29 | `get_elem` | Array | func, coro | `%r = get_elem %arr, %idx` |
| 30 | `make_struct` | Struct | func, coro | `%r = make_struct T { ... }` |
| 31 | `make_enum` | Enum | func, coro | `%r = make_enum T <Var>(%v)` |
| 32 | `extract_variant` | Enum | func, coro | `%r = extract_variant %e` |
| 33 | `check_variant` | Enum | func, coro | `%r = check_variant %e, <Var>` |
| 34 | `cast` | Cast | func, coro | `%r = cast T2 %v` |
| 35 | `call` | Call | func, coro | `%r = call @f(%a, ...)` |
| 36 | `br` | CF | func, coro | `br $label` / `br S_name` |
| 37 | `br_cond` | CF | func | `br_cond %c, $t, $f` |
| 38 | `switch` | CF | func, coro | `switch T %v, $def [ lit, $lbl... ]` |
| 39 | `ret` | CF | func, coro | `ret T %v` |
| 40 | `recv` | Chan | coro | `%r = recv %chan` |
| 41 | `send` | Chan | coro | `%r = send %chan, %val` |
| 42 | `yield` | Chan | coro | `yield %port, %val` |
| 43 | `await` | Async | coro | `%r = await %future` |
| 44 | `suspend` | Async | coro | `suspend` |

**Total: 48 instructions** (across all categories and layers).

### Implementation Status

| Category | Implemented in Backends |
|----------|------------------------|
| Constants & Values (const, mov) | ✅ aarch64, x86-64 |
| Arithmetic (add/sub/mul/div/udiv/mod) | ✅ aarch64, x86-64 |
| Comparison (eq/ne/lt/le/gt/ge) | ✅ aarch64, x86-64 |
| Logic/Bitwise (and/or/xor/not/shl/shr) | ✅ aarch64, x86-64 |
| Memory (load/store/alloca) | ✅ aarch64, x86-64 |
| Struct (get_field/set_field/make_struct) | ✅ aarch64, x86-64 |
| Enum (make_enum/check_variant/extract_variant) | ✅ aarch64, x86-64 |
| Control Flow (br/br_cond/ret) | ✅ aarch64, x86-64 |
| Call | ✅ aarch64, x86-64 |
| Print/println builtins | ✅ aarch64, x86-64 |
| Early return (return mid-function) | ✅ aarch64, x86-64 |
| NEG (negation, including immediates) | ✅ aarch64, x86-64 |
| Caller-saved register save/restore | ✅ aarch64, x86-64 |
| SSA compaction | ✅ All backends |
| DCE after terminator | ✅ All backends |
| Coroutine (recv/send/yield/await/suspend) | Parsed (backend: NOP) |
| Graph runtime | ✅ aarch64, x86-64 (linear pipelines) |

### Test Coverage

| Suite | Tests | Status |
|-------|-------|--------|
| IR unit tests | 22 | ✅ All pass |
| aarch64 example tests | 57 | ✅ All pass |
| x86-64 example tests | 44 | ✅ All pass |
| **Total** | **123** | **0 failures** |

### Example Programs (57 total)

| Category | Examples |
|----------|---------|
| Arithmetic | zero, simple1, arith, subtract, multiply, bitops, comparison, multi_var, large_num, negate |
| Control flow | if_test, if_else, while_test, for_test, for_nested, break_test, continue_test, while_break |
| Functions | func_call, func_call2, func_if, early_return |
| Recursion | recursive_fib, recursive_fib_ret, fibonacci, factorial, gcd |
| Algorithms | primes, power, collatz, sum_digits |
| Div/Mod | div_mod |
| Structs | struct_test, struct_test2 |
| Enums | enum_test, enum_match, enum_match2, enum_match3, enum_tuple, enum_extract |
| Print | print_test, print_int, print_expr, print_combined, print_mixed |
| Coroutines | coro_simple, coro_ret, coro_main |
| Graph pipeline | pipeline |
| Division/modulo | div_mod |

### Compiler Architecture

```
compiler.c (2,707 lines)     — AST → DFIR compiler
optimize.c (1,420 lines)     — 7 optimizer passes (const fold, copy prop, DCE, etc.)
ir.c/ir.h (407 lines)        — DFIR data structures and utilities
arch/aarch64/aarch64.c       — AArch64 backend (Mach-O)
arch/x86-64/x86-64.c         — x86-64 backend (Mach-O/ELF)
```

### Key Design Decisions

1. **SSA register allocation**: SSA IDs map directly to hardware registers.
   Compaction keeps IDs low. No separate register allocator needed (yet).

2. **Caller-saved save/restore**: Both backends save/restore caller-saved
   registers around function calls (BL on aarch64, CALL on x86-64) using
   stack-based save areas. Return values preserved via callee-saved temp.

3. **3-operand emulation**: x86-64 has 2-operand instructions (dst op= src),
   but DFIR is 3-operand (dst = src0 op src1). Fixed by emitting
   `mov dst, src0; op dst, src1` and using immediate-form instructions.

4. **idiv clobber handling**: x86-64 `idiv` clobbers RAX and RDX.
   Both registers are saved via push/pop around DIV/MOD operations.

5. **String table**: Strings are appended to the text section with per-string
   offsets. LEA instructions use RIP-relative addressing patched after assembly.
