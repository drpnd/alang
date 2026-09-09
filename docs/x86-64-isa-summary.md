# x86-64 Instruction Set Architecture — Assembler Implementation Summary

> Compiled for the **alang** project's rule-driven x86-64 assembler
> (`bootstrap/arch/x86-64/`). This document summarizes the encoding model,
> notation, register scheme, and instruction-table conventions an assembler
> must implement, and maps each concept onto the existing `.idef` /
> `instr.c` / `reg.h` machinery in the tree.

## Sources

The authoritative basis is the Intel® 64 and IA-32 Architectures Software
Developer's Manual (SDM), **Volume 2: Instruction Set Reference**. The
December 2023 edition was used as the primary reference (verified via the
`felixcloutier.com/x86` mirror, which is derived from that edition).

Companion/secondary references worth consulting:

| Source | What it is good for |
|---|---|
| Intel SDM Vol. 2, Ch. 1–3 | Opcode maps, operand-size/address-size rules, REX, ModR/M, SIB |
| AMD64 Architecture Programmer's Manual Vol. 3 | AMD-specific extensions, clearer description of REX and RIP-relative addressing |
| OSDev wiki: *X86-64 Instruction Encoding* | Compact reference of the instruction format and legacy prefixes |
| `bootstrap/arch/x86-64/idefs/*.idef` | The project's own condensed SDM tables (one mnemonic per file) |

---

## 1. Instruction encoding format

Every x86-64 instruction is a byte sequence in the following order:

```
[Legacy prefixes]  [REX]  Opcode  [ModR/M]  [SIB]  [Displacement]  [Immediate]
  0–4 bytes          0–1    1–3      0–1      0–1      0–4            0–4
```

This ordering is mandatory; the assembler must emit fields left-to-right.

### 1.1 Legacy prefixes (0–4 bytes, optional)

| Prefix | Byte(s) | Effect |
|---|---|---|
| Segment override | `2E` CS, `36` SS, `3E` DS, `26` ES, `64` FS, `65` GS | Override default segment for memory access |
| Operand-size override | `66` | Flip default operand size 16↔32 (also used as a SIMD opcode prefix, e.g. `66 0F 38 F6` = ADCX) |
| Address-size override | `67` | Flip default address size 32↔64 |
| LOCK | `F0` | Atomic read-modify-write (only certain `r/m, r` or `r/m, imm` forms) |
| REP/REPNE | `F3` / `F2` | String ops, or opcode-extension prefix for some SSE/AVX |

### 1.2 REX prefix (0–1 byte, 64-bit mode only)

REX is mandatory whenever an instruction references the extended registers
(R8–R15, SIL/DIL/BPL/SPL) or 64-bit operand size is required. It is the
last prefix before the opcode.

```
 7  6  5  4  3  2  1  0
+--+--+--+--+--+--+--+--+
|0 |1 |0 |W |R |X |B |  |
+--+--+--+--+--+--+--+--+
 0  4  0   ^  ^  ^  ^
            |  |  |
            |  |  +-- B: extends ModRM.r/m or SIB.base or opcode.reg
            |  +----- X: extends SIB.index
            +-------- R: extends ModRM.reg
                     W: 1 = 64-bit operand size (REX.W)
```

- **REX.W** (`0x48`) is the most common REX form — "operate on 64 bits".
- A REX prefix is also needed *just* to access `SPL/BPL/SIL/DIL` (the low
  bytes of RSP/RBP/RSI/RDI), which require `REX0 = 1` (the project's
  `REG_REX0` bit) even with W=0.
- If REX is present, the high bytes `AH/BH/CH/DH` are **inaccessible**.
  This is why the `.idef` files carry a separate `r/m8*` row guarded by
  `REX` (see `add.idef`: `REX 80 /0 ib | MI | r/m8*,imm8`).

The project encodes a REX decision per register in `reg.h`:

```c
#define REG_CODE(r)  ((r) & 0x7)      /* low 3 bits -> ModRM/SIB field  */
#define REG_REX(r)   (((r) >> 3) & 1) /* sets REX.R / REX.B / REX.X      */
#define REG_REX0(r)  (((r) >> 4) & 1) /* REX needed for SPL/BPL/SIL/DIL */
#define REG_NE(r)    (((r) >> 5) & 1) /* "no encoding" - AH/CH/DH/BH    */
#define REG_SIZE(r)  (((r) >> 16) & 0xffff)
```

### 1.3 Opcode (1–3 bytes)

- **1-byte**: `00`–`FF` (the bulk of the classic ISA).
- **2-byte**: `0F xx` (most SSE/SSE2 and many system instructions).
- **3-byte**: `0F 38 xx` or `0F 3A xx` (SSSE3/SSE4/AES/AVX extension space).

Some opcodes embed a register field directly in the low 3 bits (`B0+rb`,
`B8+rd`, `90+r`). The `.idef` notation calls this `+rb` / `+rw` / `+rd` /
`+ro` (see §3).

### 1.4 ModR/M byte (0–1 byte)

```
 7  6   5  4 3   2 1 0
+------+-------+------+
| mod  | reg   | r/m  |
+------+-------+------+
```

- **mod** (2 bits): addressing mode.
  - `00` → `[r/m]` with no displacement (special cases below)
  - `01` → `[r/m + disp8]`
  - `10` → `[r/m + disp32]`
  - `11` → register direct (r/m field is a register, not memory)
- **reg** (3 bits): a register operand, *or* an opcode extension (`/0`..`/7`).
- **r/m** (3 bits): the other register/memory operand; extended by REX.B.

Special `mod=00` cases that the assembler **must** handle:

| r/m (mod=00) | Meaning |
|---|---|
| `101` (`5`) | **RIP-relative** `[disp32]` — the only 64-bit-mode way to do `[disp]` with no base. Displacement is a 32-bit signed offset from the *next* instruction. |
| `100` (`4`) | **SIB follows** — the addressing uses a (base,index,scale) triple instead of a plain base. |

### 1.5 SIB byte (0–1 byte)

Required whenever the memory operand uses `base + index*scale` or whenever
`r/m = 4` and `mod != 11`.

```
 7  6   5  4 3   2 1 0
+------+-------+------+
|scale | index | base |
+------+-------+------+
```

- **scale** (2 bits): `00`=×1, `01`=×2, `10`=×4, `11`=×8.
- **index**: index register (extended by REX.X). `100` = no index.
- **base**: base register (extended by REX.B). `101` with `mod=00` = no
  base (disp32 only).

The project's `x86_64_operand_mem_t` models this directly:

```c
typedef struct {
    int base;     /* -1 if none            */
    int sindex;   /* -1 if none            */
    int scale;    /* 1, 2, 4, or 8         */
    int32_t disp;
} x86_64_operand_mem_t;
```

### 1.6 Displacement (0–4 bytes) and Immediate (0–4 bytes)

- **Displacement**: 1 byte (`disp8`, sign-extended) or 4 bytes (`disp32`).
  No 2- or 8-byte displacement exists in x86-64.
- **Immediate**: 1, 2, 4, or (only for `MOV r64, imm64`) 8 bytes.
  Immediates larger than the operand are **not allowed**; immediates
  smaller than the operand are **sign-extended** (except `MOV` to a
  16/32-bit register uses zero-extension of the *result* on write).

---

## 2. Operand-size determination

The effective operand size for an instruction is decided by, in priority:

1. **REX.W** — forces 64-bit operand size (the `.idef` `W` token).
2. **`66` prefix** — flips 32↔16 (so with no REX.W: `66` → 16-bit, default → 32-bit).
3. **Default** — 32-bit in 64-bit mode (except far branches, `PUSH`/`POP` of
   an immediate, and a few others which default to 64).

Address size defaults to 64-bit in 64-bit mode and is flipped by `67`.

### Sign/zero-extension rules (critical for `imm` and `disp`)

| Source width → Target width | Behaviour |
|---|---|
| `imm8` → 16/32/64 | sign-extended |
| `imm8` → 8-bit (`r/m8`) | direct (the `r/m8, imm8` forms) |
| `imm32` → 64-bit (REX.W) | **sign-extended** (no imm64 except `MOV`) |
| `disp8` → 64-bit address | sign-extended |
| 8/16-bit write to 32/64-bit register | **zero-extended** (clears upper bits) |
| 8/16-bit write to 8/16-bit register | leaves upper bits unchanged |

The project helper `_operand_imm()` and `_check_size()` reproduce the
"smallest fitting width" rule the assembler uses to pick `ib`/`iw`/`id`.

---

## 3. The `.idef` table notation (alang's condensed SDM)

Each `.idef` file is a pipe-delimited table; `instr.c::_instr_parse_file`
loads it. The columns are:

```
   <opcode>         | <Op/En> | <operands>      | <64-bit> | <legacy> | [<cpuid>]
```

### 3.1 Opcode column tokens

Parsed by `_parse_opcode_chunk()`:

| Token | Meaning | Internal constant |
|---|---|---|
| `XX` (two hex digits) | literal opcode byte | the byte value |
| `W` | REX.W prefix | `OPCODE_REXW` |
| `/0`..`/7` | ModRM.reg opcode extension | `OPCODE_DIGIT_PREFIX+n` |
| `/r` | ModRM.reg is a register | `OPCODE_REGISTER` |
| `cb` `cw` `cd` `cp` `co` `ct` | code immediate (1/2/4/6/8/10 bytes) | `OPCODE_C*` |
| `ib` `iw` `id` `io` | data immediate (1/2/4/8 bytes) | `OPCODE_I*` |
| `+rb` `+rw` `+rd` `+ro` | register encoded in opcode low bits | `OPCODE_R*` |
| `+0`..`+7` | ST(i) stack-register encoded in opcode | `OPCODE_ST_PREFIX+n` |

> **Verified against SDM**: e.g. `felixcloutier.com/x86/mov` shows
> `B0+ rb ib | MOV r8, imm8 | OI`, and `B8+ rd id | MOV r32, imm32 | OI`,
> and `REX.W + B8+ rd io | MOV r64, imm64 | OI` — matching the project's
> `mov.idef` rows exactly.

### 3.2 Op/En (operand encoding) column

The SDM "Instruction Operand Encoding" legend (verified on the MOV page)
defines how the operands map to the byte fields:

| Op/En | Operand 1 | Operand 2 | Project enum | #operands |
|---|---|---|---|---|
| **M** | ModRM:r/m | — | `ENCODE_M` | 1 |
| **RM** | ModRM:reg | ModRM:r/m | `ENCODE_RM` | 2 |
| **MR** | ModRM:r/m | ModRM:reg | `ENCODE_MR` | 2 |
| **OI** | opcode+rd | imm | `ENCODE_OI` | 2 |
| **MI** | ModRM:r/m | imm | `ENCODE_MI` | 2 |
| **D** | far ptr / rel | — | `ENCODE_D` | 1 |
| **I** | AL/AX/EAX/RAX | imm | *(special, see below)* | 2 |
| **FD** | AL/AX/EAX/RAX | moffs | *(not yet in enum)* | 2 |
| **TD** | moffs | AL/AX/EAX/RAX | *(not yet in enum)* | 2 |
| **NP** | — | — | *(no operands)* | 0 |

> **`I` is currently a gap.** The `.idef` files (e.g. `add.idef`) use
> `I` for the `al/ax/eax/rax, imm` short forms, but `_parse_encode_type()`
> in `instr.c` only knows `M/RM/MR/OI/MI/D`. The SDM MOV page shows `FD`/`TD`
> for the `moffs` forms; the alang `mov.idef` collapses those into a
> moffs-style operand not yet handled by the parser. See §6.

### 3.3 Operand column tokens

Parsed by `_parse_operand_chunk()`:

| Token | Operand class |
|---|---|
| `al` `ax` `eax` `rax` | fixed accumulator register |
| `r8` `r16` `r32` `r64` | any GP register of the given width |
| `r/m8` `r/m16` `r/m32` `r/m64` | GP register *or* memory (the ModRM operand) |
| `r8*` `r/m8*` | the REX-required byte-register forms (no AH/BH/CH/DH) |
| `imm8` `imm16` `imm32` `imm64` | immediate |
| `rel8` `rel16` `rel32` | relative branch displacement |
| `ptr16:16` `ptr16:32` `ptr16:64` | far pointer (selector:offset) |
| `m8` `m16` `m32` `m64` `m128` | memory operand only (no register) |
| `moffs8` `moffs16` `moffs32` `moffs64` | direct memory offset (no ModRM) |
| `m16:16` `m16:32` `m16:64` | far memory pointer |
| `Sreg` | segment register |
| `ST(i)` `MM` `XMM` `YMM` | FPU / MMX / SSE / AVX register |

### 3.4 Validity columns

- **64-bit mode**: `V` = valid, `N.E.` = not encodable, `N.S.` = not
  supported, `I` = invalid.
- **Compat/Legacy mode**: `V` / `N`.
- Optional **CPUID feature** column (e.g. `ADX`, `AVX`).

---

## 4. Register scheme (as used by `reg.h`)

The 16 GP registers of x86-64 are addressed by a 4-bit number
(`REX bit` concatenated with the 3-bit `code`):

| code | REX=0 | REX=1 |
|---|---|---|
| 0 | RAX/EAX/AX/AL | R8/R8D/R8W/R8L |
| 1 | RCX | R9 |
| 2 | RDX | R10 |
| 3 | RBX | R11 |
| 4 | RSP | R12 |
| 5 | RBP | R13 |
| 6 | RSI | R14 |
| 7 | RDI | R15 |

Each register variant packs the 3-bit code, the REX bit, the REX0 bit
(needed for SPL/BPL/SIL/DIL), the NE bit (AH/BH/CH/DH, which become
unreachable under REX), and the size, into one integer via `REG_ENCODE`.

Sizes the project already models: **8, 16, 32, 64** (GP), **80** (x87 ST),
**64** (MMX), **128** (XMM), **256** (YMM), plus 16-bit segment registers.

---

## 5. Encoding algorithm (end-to-end)

For a given `(mnemonic, operand_list)` the assembler must:

1. **Resolve operands** into the `x86_64_operand_t` union (REG / MEM / IMM).
   For each operand, determine its size (from the register's `REG_SIZE` or
   the memory/immediate width).

2. **Operand-size inference.** If no explicit size is given, infer it from
   the other operand (e.g. `ADD rax, 1` → 64-bit because of `rax`; the `imm`
   is then sign-extended).

3. **Rule match** (`x86_64_search` → `_search_rule`). Walk the mnemonic's
   rules in declaration order and find the first whose Op/En and operand
   sizes match. The matcher checks:
   - operand count vs `_operand_num_by_encode_type`,
   - operand kind (reg/mem/imm) vs the Op/En's expectation,
   - size compatibility.
   The project's current matchers (`_search_encode_rm/mr/mi/m/d`) are
   partial — they check kinds but **not yet sizes**, and `I`/`FD`/`TD`/`NP`
   are unhandled (see §6).

4. **Emit prefix.** Decide REX from `REG_REX`/`REG_REX0`/W; emit `66`/`67`
   if the rule's opcode calls for a size override.

5. **Emit opcode bytes** from the rule, substituting:
   - `/digit` and `/r` → the `reg` field of ModRM,
   - `+rb`/`+rw`/`+rd`/`+ro` → OR the register code into the opcode low
     3 bits (and set REX.B if reg ≥ R8),
   - `ib`/`iw`/`id`/`io` placeholders are *not* part of the opcode; they
     mark where the immediate goes.

6. **Emit ModR/M** (when Op/En needs it). Build `mod`, `reg`, `r/m`:
   - if the r/m operand is a register → `mod=11`,
   - if memory → choose mod by displacement size, handle SIB / RIP-relative
     as in §1.4–1.5.

7. **Emit SIB** when `r/m == 4` and `mod != 11` or an index is present.

8. **Emit displacement** (disp8 or disp32) when mod requires it or for
   RIP-relative.

9. **Emit immediate** in the width the opcode specifies.

---

## 6. Gaps and recommended next steps for the alang assembler

Cross-checking the SDM tables against the current `instr.c` shows these
known limitations to close:

1. **Missing Op/En `I`.** The `al/eax/rax, imm` short forms (`04 ib`,
   `05 id`, `W 05 id`) appear in every `.idef` but `_parse_encode_type`
   returns `-1` for `I`. Add `ENCODE_I` (1 reg operand = accumulator +
   1 imm operand). These are the most compact encodings for accumulator
   arithmetic, so they should be tried first.

2. **Missing Op/En `FD` / `TD`** (moffs accumulator moves) and **`NP`**
   (no-operand instructions like `RET`, `CLC`). `mov.idef` has `moffs`
   rows that are currently unparseable.

3. **Operand-size matching is absent.** `_search_encode_rm` only checks
   "is it a register"; it does not verify both operands are the same size,
   so `MOV rax, ecx` would wrongly match `8B /r` (RM) instead of erroring.
   Add a size-equality check (and a REX.W check for `r64` rows).

4. **REX emission not wired.** The matchers do not yet consult `REG_REX`
   / `REG_REX0` to emit the REX byte; `x86_64_asm()` is currently a stub
   returning 0. The encoding step (§5 steps 4–9) needs implementing.

5. **ModR/M / SIB / RIP-relative emission not implemented.** This is the
   bulk of an assembler's value: translating `x86_64_operand_mem_t`
   `(base, sindex, scale, disp)` into the correct `mod`/SIB combination,
   including the `r/m=5 mod=00` RIP-relative special case.

6. **`r/m8*` handling.** The `*` rows require a REX prefix even when W=0
   (to reach SPL/BPL/SIL/DIL). The matcher must set the REX0 requirement
   and forbid AH/BH/CH/DH when a REX is otherwise present.

7. **Branch fixups.** `rel8`/`rel32` need a two-pass layout: compute the
   target offset relative to the *end* of the encoded instruction, and
   shrink `rel32`→`rel8` where possible (the SDM shows both `EB cb` and
   `E9 cd` for `JMP`).

8. **Instruction coverage.** Only `adc/adcx/add/call/jmp/mov/xor` have
   `.idef` files. The SDM index lists ~1,500 mnemonics; for a usable
   assembler, prioritise the integer ALU (`sub/and/or/sbb/cmp/inc/dec/neg/not`,
   shifts/rotates, `imul/mul/idiv/div`), `mov` family, branches
   (`jcc`/`jmp`/`call`/`ret`), stack (`push`/`pop`), and `lea`.

---

## 7. Worked example: encoding `ADD RAX, 1`

1. Operands: `RAX` (reg, 64-bit), `1` (imm, fits in `imm8`).
2. Rule search in `add.idef`: row `W 83 /0 ib | MI | r/m64,imm8` matches
   (64-bit via REX.W, imm8, MI encoding).
3. Emit:
   - REX: `W=1`, B=0 (RAX is code 0) → `0x48`
   - Opcode: `0x83`
   - ModR/M: `mod=11` (register direct), `reg=/0` → `0b11_000_000` = `0xC0`
   - Immediate: `0x01` (sign-extended at runtime)
   → bytes: `48 83 C0 01`
4. *Alternative* accumulator short form (if `ENCODE_I` were supported):
   `W 05 id | I | rax,imm32` would give `48 05 01 00 00 00` — larger, so
   the `MI` form wins on size. The assembler should try the most specific
   (shortest) rule first.

---

## 8. Quick reference — instruction format at a glance

```
 +---------+-----+--------+--------+-----+--------------+----------+
 | Leg.pfx | REX | Opcode | ModR/M | SIB | Displacement | Immed.   |
 | 0-4     | 0-1 | 1-3    | 0-1    | 0-1 | 0,1,4        | 0,1,2,4,8|
 +---------+-----+--------+--------+-----+--------------+----------+

 REX:      0100WRXB   (W=opsize64, R=ext.ModRM.reg, X=ext.SIB.idx, B=ext.r/m)
 ModR/M:   mod reg r/m (mod: 00=[r/m] 01=[r/m+disp8] 10=[r/m+disp32] 11=reg)
 SIB:      scale index base
```

Always emit in this order; always pick the **shortest** valid encoding;
always honour the REX-before-opcode and no-`AH`-under-REX rules.
