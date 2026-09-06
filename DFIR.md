# DFIR — Data Flow IR Specification

> **Version:** 0.1 (draft)
>
> **Status:** Work in progress

## 1. Overview

DFIR (Data Flow IR) is a minimal, single static assignment (SSA) intermediate
representation designed for a data flow programming language that supports
both functions and coroutines as first-class constructs.

### 1.1 Design Goals

| Goal | Description |
|------|-------------|
| Minimal | ~25 instructions total across three layers |
| SSA | Every value is assigned exactly once |
| Data-flow-native | Channels, `yield`, `await` are IR-level instructions |
| Backend-agnostic | Lowerable to Cranelift, C, or an interpreter |
| Debuggable | Human-readable textual form |

### 1.2 Compilation Pipeline

```
Source → AST → DFIR → (optimizer passes) → Native Code
                                   ↓
                        Cranelift / C backend / Interpreter
```

DFIR is the **only** IR in the compiler. There is no separate LLVM step.

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

DFIR supports a small set of optimization passes that operate on the IR:

| Pass | Layer | Description |
|------|-------|-------------|
| Constant folding | `func`, `coro` | Evaluate constant expressions at compile time |
| Copy propagation | `func`, `coro` | Replace `%b = %a; ... %b` with `%a` |
| Dead code elimination | `func`, `coro` | Remove unused instructions and unreachable states/blocks |
| Inline | `func` | Inline small `func` calls into callers |
| State minimization | `coro` | Merge redundant states; remove unreachable states |
| Node fusion | `graph` | Fuse adjacent `map`/`filter` coros into a single coro |
| Channel elision | `graph` | When two fused nodes share a channel, remove the channel and pass values directly |

### 8.1 Pass Ordering

```
1. Inline           ; expand small function calls
2. Constant folding ; fold constants
3. Copy propagation ; simplify SSA chains
4. Dead code elim   ; remove unreachable code
5. State minimization ; simplify coro state machines
6. Node fusion      ; merge adjacent graph nodes
7. Channel elision  ; remove unnecessary channels
```

After optimization, DFIR is lowered to the target backend (Cranelift or C).

---

## 9. Backend Lowering

DFIR is backend-agnostic. The planned backends are:

| Backend | Use Case | Status |
|---------|----------|--------|
| Cranelift | Fast compilation, development builds | Planned |
| C | Production builds via gcc/clang optimization | Planned |
| Interpreter | Debugging, REPL, testing | Planned |

### 9.1 Lowering Rules

| DFIR Construct | Cranelift | C |
|----------------|-----------|---|
| `func` | `Function` with `Block`s | C function with `goto` for blocks |
| `coro` state | Switch-dispatch on state enum | `switch(state)` in a function |
| `graph` | Runtime API calls | Runtime API calls |
| `recv`/`send` | Runtime channel API | Runtime channel API |
| `phi` | CR-style block parameters | Variable assigned in each predecessor |
| `alloc` | Stack slot | C stack variable |

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

## 12. Open Questions

- [ ] Should DFIR support a binary serialization format (for caching)?
- [ ] Should `dfir.graph` support dynamic (runtime) graph modification?
- [ ] Should there be a `dfir.extern` declaration for FFI?
- [ ] What is the exact `phi` semantics for coro states (across poll calls)?
- [ ] Should node fusion produce a new `dfir.coro` or inline into the graph?
- [ ] How are generic types instantiated in DFIR (monomorphization)?

---

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

**Total: 44 instructions** (across all categories and layers).
