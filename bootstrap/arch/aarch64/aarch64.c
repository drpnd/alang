/*_
 * Copyright (c) 2022-2026 Hirochika Asai <asai@jar.jp>
 * All rights reserved.
 * MIT License
 */

#include "../../arch.h"
#include "../../ir.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*======================================================================
 * AArch64 instruction encoding helpers
 *======================================================================*/

/* All AArch64 instructions are 32-bit, little-endian. */

/* Register file: X0-X28 usable for SSA values, X29=FP, X30=LR, X31=SP/XZR */
#define AARCH64_MAX_REGS 25

/*
 * Emit a 32-bit instruction (little-endian).
 */
/* Text buffer */
typedef struct {
    uint8_t *buf;
    size_t size;
    size_t cap;
} textbuf_t;

static int
tb_init(textbuf_t *tb)
{
    tb->cap = 256;
    tb->buf = malloc(tb->cap);
    if (!tb->buf) return -1;
    tb->size = 0;
    return 0;
}

static int
tb_ensure(textbuf_t *tb, size_t need)
{
    if (tb->size + need > tb->cap) {
        size_t nc = tb->cap;
        while (nc < tb->size + need) nc *= 2;
        uint8_t *nb = realloc(tb->buf, nc);
        if (!nb) return -1;
        tb->buf = nb;
        tb->cap = nc;
    }
    return 0;
}

static int
tb_emit(textbuf_t *tb, const uint8_t *data, size_t len)
{
    if (tb_ensure(tb, len) < 0) return -1;
    memcpy(tb->buf + tb->size, data, len);
    tb->size += len;
    return 0;
}

static int
emit32(textbuf_t *tb, uint32_t insn)
{
    return tb_emit(tb, (uint8_t*)&insn, 4);
}

/*======================================================================
 * Label resolution system
 *======================================================================*/

typedef struct {
    char *name;
    size_t offset;
} label_entry_t;

typedef struct {
    label_entry_t *items;
    int count;
    int cap;
} label_map_t;

typedef struct {
    size_t off;       /* offset of branch instruction in text */
    char *target;     /* target label name */
    int reg;          /* register for CBZ (-1 for B) */
} branch_patch_t;

typedef struct {
    branch_patch_t *items;
    int count;
    int cap;
} patch_list_t;

typedef struct {
    textbuf_t tb;
    arch_code_t *code;
    ir_object_t *ir_obj;
    label_map_t labels;
    patch_list_t patches;
    int max_ssa;
    struct {
        size_t *adr_off;
        int *str_idx;
        int count;
        int cap;
    } str_patches;
    struct {
        size_t *adr_off;
        int *sym_idx;
        int count;
        int cap;
    } global_patches;
} asm_ctx_t;

static void
label_map_add(label_map_t *m, const char *name, size_t off)
{
    if (m->count >= m->cap) {
        m->cap = m->cap ? m->cap * 2 : 16;
        m->items = realloc(m->items, m->cap * sizeof(label_entry_t));
    }
    m->items[m->count].name = strdup(name);
    m->items[m->count].offset = off;
    m->count++;
}

static size_t
label_map_lookup(label_map_t *m, const char *name)
{
    for (int i = 0; i < m->count; i++) {
        if (strcmp(m->items[i].name, name) == 0) {
            return m->items[i].offset;
        }
    }
    return (size_t)-1;
}

static void
patch_list_add(patch_list_t *p, size_t off, const char *target, int reg)
{
    if (p->count >= p->cap) {
        p->cap = p->cap ? p->cap * 2 : 16;
        p->items = realloc(p->items, p->cap * sizeof(branch_patch_t));
    }
    p->items[p->count].off = off;
    p->items[p->count].target = strdup(target);
    p->items[p->count].reg = reg;
    p->count++;
}

static void
resolve_patches(asm_ctx_t *ctx)
{
    for (int i = 0; i < ctx->patches.count; i++) {
        branch_patch_t *p = &ctx->patches.items[i];
        size_t target = label_map_lookup(&ctx->labels, p->target);
        if (target == (size_t)-1) {
            fprintf(stderr, "warning: unresolved label %s\n", p->target);
            continue;
        }
        int32_t disp = (int32_t)(target - p->off);
        if (p->reg < 0) {
            /* B instruction: imm26 = disp / 4 */
            int32_t imm26 = (disp / 4) & 0x3FFFFFF;
            uint32_t insn = (0x05U << 26) | imm26;
            memcpy(ctx->tb.buf + p->off, &insn, 4);
        } else {
            /* CBZ instruction: imm19 = disp / 4 */
            int32_t imm19 = (disp / 4) & 0x7FFFF;
            uint32_t insn = (1U << 31) | (0x34U << 24) | (imm19 << 5) | (p->reg & 31);
            memcpy(ctx->tb.buf + p->off, &insn, 4);
        }
    }
}

static void
asm_ctx_free(asm_ctx_t *ctx)
{
    for (int i = 0; i < ctx->labels.count; i++) free(ctx->labels.items[i].name);
    free(ctx->labels.items);
    for (int i = 0; i < ctx->patches.count; i++) free(ctx->patches.items[i].target);
    free(ctx->patches.items);
    free(ctx->str_patches.adr_off);
    free(ctx->str_patches.str_idx);
}

/*
 * Parse SSA id "%N" to integer N.
 */
static int
ssa_id(const char *id)
{
    if (!id || id[0] != '%') return -1;
    return atoi(id + 1);
}

/*
 * AArch64 calling convention:
 *   Arguments: X0-X7 (first 8 args)
 *   Return: X0
 *   Caller-saved (temporaries): X0-X18
 *   Callee-saved: X19-X28, X29 (FP), X30 (LR)
 *
 * SSA register allocation:
 *   %0-%7   → X0-X7 (argument registers, reused as temps)
 *   %8-%18  → X8-X18 (temporary registers)
 *   %19-%28 → X19-X28 (callee-saved, need save/restore)
 */

/*
 * Argument registers (AArch64 ABI)
 */
static const int arg_regs[8] = {0, 1, 2, 3, 4, 5, 6, 7};

/*
 * Map SSA id to AArch64 register (0-28).
 * For IDs >= 29, wrap around: use (id % 29) as a temporary.
 * This is NOT correct in general (register collisions), but works
 * for testing. Proper register spilling is a TODO.
 */
static textbuf_t *g_tb = NULL;
static int g_spill_off = 0;  /* base offset from FP for spill slots */

/* SSA to register mapping. Skip X16/X17 (IP0/IP1) and X18 (platform reg).
 * %0-%15  → X0-X15
 * %16-%24 → X19-X28 (skip X16, X17, X18) */
static int
ssa_to_reg(int id)
{
    if (id < 0) return 31;
    if (id < 16) return id;
    if (id < 25) return id + 3;  /* 16→X19, 17→X20, ... 24→X28 */
    return -1;  /* spilled */
}

static int
spill_load(int id, int reg)
{
    int off = g_spill_off - (id - AARCH64_MAX_REGS) * 8;
    if (off >= -256 && off <= 255) {
        uint32_t insn = (0xF8U << 24) | (1U << 22) | ((off & 0x1FF) << 12) | (29 << 5) | (reg & 31);
        emit32(g_tb, insn);
    } else {
        /* Large offset: sub/add xreg, x29, #hi; ldur xreg, [xreg, #lo]
         * Using the destination register itself as the address temp is safe
         * because the final LDUR overwrites the address with the loaded value.
         * (Previously used X9, which clobbers SSA value 9.) */
        int hi = (off / 256) * 256;
        int lo = off - hi;
        if (hi >= 0) {
            emit32(g_tb, (1U<<31)|(0x11U<<24)|((hi&0xFFF)<<10)|(29<<5)|(reg&31));
        } else {
            emit32(g_tb, (1U<<31)|(0x51U<<24)|(((-hi)&0xFFF)<<10)|(29<<5)|(reg&31));
        }
        emit32(g_tb, (0xF8U<<24)|(1U<<22)|((lo&0x1FF)<<12)|((reg&31)<<5)|(reg&31));
    }
    return reg;
}

static void
spill_store(int reg, int id)
{
    int off = g_spill_off - (id - AARCH64_MAX_REGS) * 8;
    if (off >= -256 && off <= 255) {
        uint32_t insn = (0xF8U << 24) | ((off & 0x1FF) << 12) | (29 << 5) | (reg & 31);
        emit32(g_tb, insn);
    } else {
        /* Large offset: sub/add xtmp, x29, #hi; stur xreg, [xtmp, #lo]
         * Use X17 as address temp to avoid conflict when reg == X16.
         * X16/X17 (IP0/IP1) are never used for SSA values. */
        int tmp = (reg == 16) ? 17 : 16;
        int hi = (off / 256) * 256;
        int lo = off - hi;
        if (hi >= 0) {
            emit32(g_tb, (1U<<31)|(0x11U<<24)|((hi&0xFFF)<<10)|(29<<5)|tmp);
        } else {
            emit32(g_tb, (1U<<31)|(0x51U<<24)|(((-hi)&0xFFF)<<10)|(29<<5)|tmp);
        }
        emit32(g_tb, (0xF8U<<24)|((lo&0x1FF)<<12)|((tmp&31)<<5)|(reg&31));
    }
}

/*
 * Check if a register is callee-saved (X19-X28)
 */
__attribute__((unused))
static int is_callee_saved(int reg)
{
    return reg >= 19 && reg <= 28;
}

/*
 * Emit function prologue:
 *   stp x29, x30, [sp, #-16]!   ; save FP and LR
 *   mov x29, sp                   ; set up frame pointer
 *   stp x19, x20, [sp, #-16]!    ; save callee-saved regs (as needed)
 *   ...
 */
/* Forward declarations */
static int emit_ret(textbuf_t *tb);
static int emit_nop(textbuf_t *tb);
static int emit_orr_reg(textbuf_t *tb, int rd, int rn, int rm, int sf);
static int operand_reg_or_imm_scratch(asm_ctx_t *ctx, ir_operand_t *op, int scratch);
static int operand_reg_or_imm(asm_ctx_t *ctx, ir_operand_t *op);
static void str_patch_add(asm_ctx_t *ctx, size_t adr_off, int str_idx);
static void global_patch_add(asm_ctx_t *ctx, size_t adr_off, int sym_idx);
static int emit_bl(textbuf_t *tb, int32_t offset);


/* Save caller-saved registers (X0-X18) to stack.
 * Reserves 16 extra bytes at [SP] for variadic arg area (for printf).
 * Saved registers start at [SP, #16]. */
static void
emit_caller_save(asm_ctx_t *ctx)
{
    int max_cs = ctx->max_ssa < 18 ? ctx->max_ssa : 18;
    int n_cs = max_cs + 1;
    if (n_cs % 2 != 0) n_cs++;
    int save_size = n_cs * 8 + 32;  /* +16 for variadic area, +16 for alignment */
    uint32_t sub = (1U << 31) | (0x51U << 24) |
                  ((save_size & 0xFFF) << 10) | (31U << 5) | 31;
    emit32(&ctx->tb, sub);
    for (int i = 0; i <= max_cs; i++) {
        int off = 16 + i * 8;  /* saved regs start at [SP, #16] */
        uint32_t str = (0xF9U << 24) | (((off / 8) & 0xFFF) << 10) |
                      (31U << 5) | i;
        emit32(&ctx->tb, str);
    }
}

/* Restore caller-saved registers from stack */
static void
emit_caller_restore(asm_ctx_t *ctx)
{
    int max_cs = ctx->max_ssa < 18 ? ctx->max_ssa : 18;
    int n_cs = max_cs + 1;
    if (n_cs % 2 != 0) n_cs++;
    int save_size = n_cs * 8 + 32;
    for (int i = 0; i <= max_cs; i++) {
        int off = 16 + i * 8;
        uint32_t ldr = (0xF9U << 24) | (1U << 22) |
                      (((off / 8) & 0xFFF) << 10) | (31U << 5) | i;
        emit32(&ctx->tb, ldr);
    }
    uint32_t add = (1U << 31) | (0x11U << 24) |
                  ((save_size & 0xFFF) << 10) | (31U << 5) | 31;
    emit32(&ctx->tb, add);
}

static int
emit_prologue(asm_ctx_t *ctx, int nargs, int max_ssa)
{
    /* Save FP (X29) and LR (X30) */
    /* stp x29, x30, [sp, #-16]! = 0xA9BF7BFD */
    emit32(&ctx->tb, 0xA9BF7BFD);
    /* mov x29, sp = add x29, sp, #0 = 0x910003FD */
    emit32(&ctx->tb, 0x910003FD);

    /* Always save all callee-saved registers X19-X28 (10 regs = 80 bytes) */
    int saved_count = 10;
    int alloc_size = 80;
    /* sub sp, sp, #80 */
    uint32_t sub = (1U << 31) | (0x51U << 24) |
                  ((alloc_size & 0xFFF) << 10) | (31U << 5) | 31;
    emit32(&ctx->tb, sub);
    for (int j = 0; j < saved_count; j++) {
        int reg = 19 + j;
        int offset = j * 8;
        uint32_t str = (0xF9U << 24) | (((offset / 8) & 0xFFF) << 10) |
                      (31U << 5) | reg;
        emit32(&ctx->tb, str);
    }

    return 0;
}

/*
 * Emit function epilogue:
 *   restore callee-saved regs
 *   ldp x29, x30, [sp], #16
 *   ret
 */
static int
emit_epilogue(asm_ctx_t *ctx, int max_ssa)
{
    /* Restore SP to point to callee-saved area:
     * SP = X29 - 80 (callee-saved area is 80 bytes below FP) */
    /* mov sp, x29 = add sp, x29, #0 */
    emit32(&ctx->tb, (1U << 31) | (0x11U << 24) | (29U << 5) | 31);
    /* sub sp, sp, #80 */
    emit32(&ctx->tb, (1U << 31) | (0x51U << 24) | ((80 & 0xFFF) << 10) | (31U << 5) | 31);
    /* Always restore all callee-saved registers X19-X28 */
    int saved_count = 10;
    int alloc_size = 80;
    for (int j = 0; j < saved_count; j++) {
            int reg = 19 + j;
            int offset = j * 8;
            /* LDR Xt, [sp, #offset] — bit 22=1 for load (not store) */
            uint32_t ldr = (0xF9U << 24) | (1U << 22) | (((offset / 8) & 0xFFF) << 10) |
                          (31U << 5) | reg;
            emit32(&ctx->tb, ldr);
        }
        /* add sp, sp, #alloc_size */
        uint32_t add = (1U << 31) | (0x11U << 24) |
                       ((alloc_size & 0xFFF) << 10) | (31U << 5) | 31;
        emit32(&ctx->tb, add);

    /* ldp x29, x30, [sp], #16 = 0xA8C17BFD */
    emit32(&ctx->tb, 0xA8C17BFD);
    /* ret */
    emit_ret(&ctx->tb);
    return 0;
}

/*
 * Get register from operand.
 */
static int
operand_reg(ir_operand_t *op)
{
    if (op->type == IR_OPERAND_REG) {
        int id = ssa_id(op->u.reg.id);
        int r = ssa_to_reg(id);
        if (r < 0) return spill_load(id, 17);
        return r;
    }
    return 31;
}

/*
 * Get immediate value from operand.
 */
static int64_t
operand_imm(ir_operand_t *op, int *ok)
{
    *ok = 1;
    if (op->type != IR_OPERAND_IMM) { *ok = 0; return 0; }
    switch (op->u.imm.type) {
    case IR_IMM_I8:  case IR_IMM_S8:  return op->u.imm.u.s8;
    case IR_IMM_I16: case IR_IMM_S16: return op->u.imm.u.s16;
    case IR_IMM_I32: case IR_IMM_S32: return op->u.imm.u.s32;
    case IR_IMM_I64: case IR_IMM_S64: return op->u.imm.u.s64;
    case IR_IMM_BOOL: return op->u.imm.u.bval ? 1 : 0;
    default: *ok = 0; return 0;
    }
}

/*======================================================================
 * AArch64 instruction encoders
 *======================================================================*/

/*
 * MOVZ Xd, #imm16 (64-bit)
 * Format: sf 10 100101 hw imm16 Rd
 * 64-bit: sf=1, hw=00 (shift 0)
 */
static int
emit_movz(textbuf_t *tb, int rd, uint16_t imm16, int hw)
{
    uint32_t insn = (1U << 31) | (2U << 29) | (0x25U << 23) | ((hw & 3) << 21) |
                    ((uint32_t)imm16 << 5) | (rd & 31);
    return emit32(tb, insn);
}

/*
 * MOVK Xd, #imm16, lsl shift (keep other bits)
 * Format: sf 11 100101 hw imm16 Rd
 */
static int
emit_movk(textbuf_t *tb, int rd, uint16_t imm16, int hw)
{
    uint32_t insn = (1U << 31) | (3U << 29) | (0x25U << 23) | ((hw & 3) << 21) |
                    ((uint32_t)imm16 << 5) | (rd & 31);
    return emit32(tb, insn);
}

/*
 * Load a 64-bit immediate using MOVZ + MOVK (1-4 instructions).
 */
static int
emit_load_imm64(textbuf_t *tb, int rd, int64_t val)
{
    uint64_t v = (uint64_t)val;
    uint16_t parts[4];

    parts[0] = v & 0xFFFF;
    parts[1] = (v >> 16) & 0xFFFF;
    parts[2] = (v >> 32) & 0xFFFF;
    parts[3] = (v >> 48) & 0xFFFF;

    /* Always emit MOVZ for the low 16 bits */
    emit_movz(tb, rd, parts[0], 0);

    /* Emit MOVK for non-zero higher parts */
    if (parts[1]) emit_movk(tb, rd, parts[1], 1);
    if (parts[2]) emit_movk(tb, rd, parts[2], 2);
    if (parts[3]) emit_movk(tb, rd, parts[3], 3);

    return 0;
}

/*
 * ADD (register): sf 0 0 0 1 0 1 1 0 0 0 Rm 0 0 0 0 0 0 Rn Rd
 */
static int
emit_add_reg(textbuf_t *tb, int rd, int rn, int rm, int sf)
{
    uint32_t insn = ((uint32_t)sf << 31) | (0x0BU << 24) |
                    ((rm & 31) << 16) | ((rn & 31) << 5) | (rd & 31);
    return emit32(tb, insn);
}

/*
 * SUB (register): sf 1 0 0 1 0 1 1 0 0 0 Rm 0 0 0 0 0 0 Rn Rd
 */
static int
emit_sub_reg(textbuf_t *tb, int rd, int rn, int rm, int sf)
{
    uint32_t insn = ((uint32_t)sf << 31) | (0x4BU << 24) |
                    ((rm & 31) << 16) | ((rn & 31) << 5) | (rd & 31);
    return emit32(tb, insn);
}

/*
 * MUL: sf 1 0 0 1 1 0 1 1 0 1 0 Rm 0 1 1 1 1 1 Rn Rd
 */
static int
emit_mul_reg(textbuf_t *tb, int rd, int rn, int rm, int sf)
{
    uint32_t insn = ((uint32_t)sf << 31) | (0x1BU << 24) |
                    ((rm & 31) << 16) | (0x1FU << 10) |
                    ((rn & 31) << 5) | (rd & 31);
    return emit32(tb, insn);
}

/*
 * SDIV: sf 0 0 0 1 1 0 1 0 1 1 0 0 0 1 1 Rm 0 0 0 0 1 1 Rn Rd
 */
static int
emit_sdiv_reg(textbuf_t *tb, int rd, int rn, int rm, int sf)
{
    uint32_t insn = ((uint32_t)sf << 31) | (0x1AC00C00U) |
                    ((rm & 31) << 16) | ((rn & 31) << 5) | (rd & 31);
    return emit32(tb, insn);
}

/*
 * UDIV: sf 0 0 0 1 1 0 1 0 1 1 0 0 0 1 0 Rm 0 0 0 0 1 1 Rn Rd
 */
static int
emit_udiv_reg(textbuf_t *tb, int rd, int rn, int rm, int sf)
{
    uint32_t insn = ((uint32_t)sf << 31) | (0x1AC00800U) |
                    ((rm & 31) << 16) | ((rn & 31) << 5) | (rd & 31);
    return emit32(tb, insn);
}

/*
 * MSUB: sf 1 0 0 1 1 0 1 1 0 0 0 1 Ra Rm 0 0 0 0 1 1 Rn Rd  (Rd = Ra - Rn * Rm)
 */
static int
emit_msub_reg(textbuf_t *tb, int rd, int rn, int rm, int ra, int sf)
{
    uint32_t insn = ((uint32_t)sf << 31) | (0x1B008000U) |
                    ((rm & 31) << 16) | ((ra & 31) << 10) |
                    ((rn & 31) << 5) | (rd & 31);
    return emit32(tb, insn);
}

/*
 * LSLV: sf 0 0 0 1 1 0 1 0 1 1 0 0 1 0 0 Rm 0 0 1 0 0 0 Rn Rd
 */
static int
emit_lsl_reg(textbuf_t *tb, int rd, int rn, int rm, int sf)
{
    uint32_t insn = ((uint32_t)sf << 31) | (0x1AC02000U) |
                    ((rm & 31) << 16) | ((rn & 31) << 5) | (rd & 31);
    return emit32(tb, insn);
}

/*
 * LSRV: sf 0 0 0 1 1 0 1 0 1 1 0 0 1 0 1 Rm 0 0 1 0 0 0 Rn Rd
 */
static int
emit_lsr_reg(textbuf_t *tb, int rd, int rn, int rm, int sf)
{
    uint32_t insn = ((uint32_t)sf << 31) | (0x1AC02400U) |
                    ((rm & 31) << 16) | ((rn & 31) << 5) | (rd & 31);
    return emit32(tb, insn);
}

/*
 * AND: sf 0 0 0 1 0 1 0 0 0 0 Rm 0 0 0 0 0 0 Rn Rd
 */
static int
emit_and_reg(textbuf_t *tb, int rd, int rn, int rm, int sf)
{
    uint32_t insn = ((uint32_t)sf << 31) | (0x0AU << 24) |
                    ((rm & 31) << 16) | ((rn & 31) << 5) | (rd & 31);
    return emit32(tb, insn);
}

/*
 * ORR: sf 0 1 0 1 0 1 0 0 0 0 Rm 0 0 0 0 0 0 Rn Rd
 */
static int
emit_orr_reg(textbuf_t *tb, int rd, int rn, int rm, int sf)
{
    uint32_t insn = ((uint32_t)sf << 31) | (0x2AU << 24) |
                    ((rm & 31) << 16) | ((rn & 31) << 5) | (rd & 31);
    return emit32(tb, insn);
}

/*
 * EOR: sf 1 1 0 0 1 0 1 0 0 0 Rm 0 0 0 0 0 0 Rn Rd
 */
static int
emit_eor_reg(textbuf_t *tb, int rd, int rn, int rm, int sf)
{
    uint32_t insn = ((uint32_t)sf << 31) | (0x4AU << 24) |
                    ((rm & 31) << 16) | ((rn & 31) << 5) | (rd & 31);
    return emit32(tb, insn);
}

/*
 * ORN (OR NOT): sf 0 0 1 0 1 0 1 0 0 0 Rm 0 0 0 0 0 0 Rn Rd
 * Used for NOT: orn rd, xzr, rm
 */
static int
emit_orn_reg(textbuf_t *tb, int rd, int rn, int rm, int sf)
{
    uint32_t insn = ((uint32_t)sf << 31) | (0x2AU << 24) | (1U << 21) |
                    ((rm & 31) << 16) | ((rn & 31) << 5) | (rd & 31);
    return emit32(tb, insn);
}

/*
 * SUBS (CMP): sf 1 1 0 1 0 1 1 0 0 0 Rm 0 0 0 0 0 1 Rn 1 1 1 1 1
 * CMP Rn, Rm = SUBS XZR, Rn, Rm
 */
static int
emit_cmp_reg(textbuf_t *tb, int rn, int rm, int sf)
{
    uint32_t insn = ((uint32_t)sf << 31) | (1U << 30) | (1U << 29) |
                    (0x0BU << 24) | ((rm & 31) << 16) |
                    ((rn & 31) << 5) | 31;  /* Rd = XZR */
    return emit32(tb, insn);
}

/*
 * CSET: Rd = cond
 * Format: sf 0 0 1 1 0 1 0 1 0 0 0 0 0 0 1 0 0 cond 0 0 0 0 1 Rm 1 1 1 1 1
 * (CSINC Rd, XZR, XZR, invert(cond))
 */
static int
emit_cset(textbuf_t *tb, int rd, int cond, int sf)
{
    /* CSINC Rd, XZR, XZR, invert(cond) */
    /* sf 00 110101 10 0 Rm cond 01 Rn Rd */
    uint32_t insn = ((uint32_t)sf << 31) | (0x1AU << 24) | (1U << 23) |
                    (31U << 16) | ((cond ^ 1) << 12) | (1U << 10) |
                    (31U << 5) | (rd & 31);
    return emit32(tb, insn);
}

/*
 * B (unconditional): 0 0 0 1 0 1 imm26
 */
static int
emit_b(textbuf_t *tb, int32_t offset)
{
    /* offset in bytes, must be divisible by 4 */
    uint32_t imm26 = ((offset / 4) & 0x3FFFFFF);
    uint32_t insn = (0x05U << 26) | imm26;
    return emit32(tb, insn);
}

/*
 * BL (branch with link): 1 0 0 1 0 1 imm26
 */
static int
emit_bl(textbuf_t *tb, int32_t offset)
{
    uint32_t imm26 = ((offset / 4) & 0x3FFFFFF);
    uint32_t insn = (0x25U << 26) | imm26;
    return emit32(tb, insn);
}

/*
 * B.cond: 0 1 0 1 0 1 0 0 imm19 0 cond
 */
static int
__attribute__((unused))
emit_bcond(textbuf_t *tb, int cond, int32_t offset)
{
    uint32_t imm19 = ((offset / 4) & 0x7FFFF) << 5;
    uint32_t insn = (0x54U << 24) | imm19 | (cond & 0xF);
    return emit32(tb, insn);
}

/*
 * RET: 1 1 0 1 0 1 1 0 0 0 1 1 1 1 1 0 0 0 0 0 0 0 Rn 0 0
 * RET X30 = 0xD65F03C0
 */
static int
emit_ret(textbuf_t *tb)
{
    return emit32(tb, 0xD65F03C0);
}

/*
 * NOP
 */
static int
emit_nop(textbuf_t *tb)
{
    return emit32(tb, 0xD503201F);
}

/*
 * LDR Xt, [xn, #imm]  (unsigned offset)
 */
static int
__attribute__((unused))
emit_ldr_imm(textbuf_t *tb, int rt, int rn, int imm)
{
    uint32_t insn = (0xF9U << 24) | (((imm / 8) & 0xFFF) << 10) |
                   ((rn & 31) << 5) | (rt & 31);
    return emit32(tb, insn);
}

/*
 * STR xt, [xn, #imm]  (unsigned offset)
 */
static int
__attribute__((unused))
emit_str_imm(textbuf_t *tb, int rt, int rn, int imm)
{
    uint32_t insn = (0xF9U << 24) | (((imm / 8) & 0xFFF) << 10) |
                   ((rn & 31) << 5) | (rt & 31);
    return emit32(tb, insn);
}

/* Condition codes */
#define COND_EQ 0
#define COND_NE 1
#define COND_GE 10
#define COND_LT 11
#define COND_GT 12
#define COND_LE 13

/*
 * Add a string literal to the data section.
 * Returns the string index for later reference.
 */
static int
add_string(asm_ctx_t *ctx, const char *str)
{
    /* Check if already exists */
    for (int i = 0; i < ctx->code->strings.n; i++) {
        if (strcmp(ctx->code->strings.items[i].str, str) == 0) {
            return i;
        }
    }
    /* Add new */
    int n = ctx->code->strings.n;
    ctx->code->strings.items = realloc(ctx->code->strings.items,
        (n + 1) * sizeof(*ctx->code->strings.items));
    ctx->code->strings.items[n].str = strdup(str);
    ctx->code->strings.items[n].offset = 0;  /* will be set during export */
    ctx->code->strings.n++;
    return n;
}

static void
str_patch_add(asm_ctx_t *ctx, size_t adr_off, int str_idx)
{
    if (ctx->str_patches.count >= ctx->str_patches.cap) {
        ctx->str_patches.cap = ctx->str_patches.cap ? ctx->str_patches.cap * 2 : 16;
        ctx->str_patches.adr_off = realloc(ctx->str_patches.adr_off,
            ctx->str_patches.cap * sizeof(size_t));
        ctx->str_patches.str_idx = realloc(ctx->str_patches.str_idx,
            ctx->str_patches.cap * sizeof(int));
    }
    ctx->str_patches.adr_off[ctx->str_patches.count] = adr_off;
    ctx->str_patches.str_idx[ctx->str_patches.count] = str_idx;
    ctx->str_patches.count++;
}

static void
global_patch_add(asm_ctx_t *ctx, size_t adr_off, int sym_idx)
{
    if (ctx->global_patches.count >= ctx->global_patches.cap) {
        ctx->global_patches.cap = ctx->global_patches.cap ? ctx->global_patches.cap * 2 : 16;
        ctx->global_patches.adr_off = realloc(ctx->global_patches.adr_off,
            ctx->global_patches.cap * sizeof(size_t));
        ctx->global_patches.sym_idx = realloc(ctx->global_patches.sym_idx,
            ctx->global_patches.cap * sizeof(int));
    }
    ctx->global_patches.adr_off[ctx->global_patches.count] = adr_off;
    ctx->global_patches.sym_idx[ctx->global_patches.count] = sym_idx;
    ctx->global_patches.count++;
}

/*======================================================================
 * Symbol and relocation helpers
 *======================================================================*/

static int
add_sym(arch_code_t *code, arch_sym_type_t type, const char *label,
        off_t pos, size_t size)
{
    arch_sym_t *ns = realloc(code->sym.syms,
                             (code->sym.n + 1) * sizeof(arch_sym_t));
    if (!ns) return -1;
    code->sym.syms = ns;
    code->sym.syms[code->sym.n].type = type;
    code->sym.syms[code->sym.n].label = strdup(label);
    code->sym.syms[code->sym.n].pos = pos;
    code->sym.syms[code->sym.n].size = size;
    code->sym.syms[code->sym.n].ref = NULL;
    code->sym.n++;
    return code->sym.n - 1;
}

static int
add_rel(arch_code_t *code, arch_rel_type_t type, off_t pos, int sym)
{
    arch_rel_t *nr = realloc(code->rel.rels,
                             (code->rel.n + 1) * sizeof(arch_rel_t));
    if (!nr) return -1;
    code->rel.rels = nr;
    code->rel.rels[code->rel.n].type = type;
    code->rel.rels[code->rel.n].pos = pos;
    code->rel.rels[code->rel.n].sym = sym;
    code->rel.n++;
    return 0;
}

/*======================================================================
 * DFIR to AArch64 code generation
 *======================================================================*/

static int is_float_imm(ir_operand_t *op);
static double operand_float_imm(ir_operand_t *op, int *ok);
static int emit_load_imm_double(textbuf_t *tb, int dst, double val);

static int
operand_reg_or_imm_scratch(asm_ctx_t *ctx, ir_operand_t *op, int scratch)
{
    if (op->type == IR_OPERAND_IMM) {
        if (is_float_imm(op)) {
            int fok;
            double fval = operand_float_imm(op, &fok);
            if (fok) {
                emit_load_imm_double(&ctx->tb, scratch, fval);
                return scratch;
            }
        }
        int ok;
        int64_t val = operand_imm(op, &ok);
        if (ok) {
            emit_load_imm64(&ctx->tb, scratch, val);
            return scratch;
        }
    }
    if (op->type == IR_OPERAND_REG) {
        int id = ssa_id(op->u.reg.id);
        int r = ssa_to_reg(id);
        if (r < 0) return spill_load(id, scratch);
        return r;
    }
    return 31;
}

static int
operand_reg_or_imm(asm_ctx_t *ctx, ir_operand_t *op)
{
    return operand_reg_or_imm_scratch(ctx, op, 17);
}


/*
 * Float instruction emitters (using D registers via FMOV bridge)
 * We store doubles in integer registers and bridge to D registers
 * for actual FP operations.
 */

/* FMOV Dd, Xn — move from general register to FP register
 * 1 00 11110 01 1 00 110 000000 Rn Rd = 0x9E670000 | (Rn << 5) | Rd */
static int
emit_fmov_dx(textbuf_t *tb, int dd, int xn)
{
    uint32_t insn = 0x9E670000U | ((xn & 31) << 5) | (dd & 31);
    return emit32(tb, insn);
}

/* FMOV Xd, Dn — move from FP register to general register
 * 1 00 11110 01 1 00 111 000000 Rn Rd = 0x9E780000 | (Rn << 5) | Rd */
static int
emit_fmov_xd(textbuf_t *tb, int xd, int dn)
{
    uint32_t insn = 0x9E780000U | ((dn & 31) << 5) | (xd & 31);
    return emit32(tb, insn);
}

/* FADD Dd, Dn, Dm — 0x1E652800 | (Rm << 16) | (Rn << 5) | Rd */
static int
emit_fadd(textbuf_t *tb, int dd, int dn, int dm)
{
    uint32_t insn = 0x1E652800U | ((dm & 31) << 16) | ((dn & 31) << 5) | (dd & 31);
    return emit32(tb, insn);
}

/* FSUB Dd, Dn, Dm — 0x1E653800 | (Rm << 16) | (Rn << 5) | Rd */
static int
emit_fsub(textbuf_t *tb, int dd, int dn, int dm)
{
    uint32_t insn = 0x1E653800U | ((dm & 31) << 16) | ((dn & 31) << 5) | (dd & 31);
    return emit32(tb, insn);
}

/* FMUL Dd, Dn, Dm — 0x1E600800 | (Rm << 16) | (Rn << 5) | Rd */
static int
emit_fmul(textbuf_t *tb, int dd, int dn, int dm)
{
    uint32_t insn = 0x1E600800U | ((dm & 31) << 16) | ((dn & 31) << 5) | (dd & 31);
    return emit32(tb, insn);
}

/* FDIV Dd, Dn, Dm — 0x1E601800 | (Rm << 16) | (Rn << 5) | Rd */
static int
emit_fdiv(textbuf_t *tb, int dd, int dn, int dm)
{
    uint32_t insn = 0x1E601800U | ((dm & 31) << 16) | ((dn & 31) << 5) | (dd & 31);
    return emit32(tb, insn);
}

/* FCMP Dn, Dm — 0x1E652000 | (Rm << 16) | (Rn << 5) */
static int
emit_fcmp(textbuf_t *tb, int dn, int dm)
{
    uint32_t insn = 0x1E602000U | ((dm & 31) << 16) | ((dn & 31) << 5);
    return emit32(tb, insn);
}

/* Load immediate double into register via literal pool approach.
 * Use FMOV Dd, #imm when possible, otherwise load via X register. */
static int
emit_load_imm_double(textbuf_t *tb, int dst, double val)
{
    /* Try FMOV Dd, #imm (only works for certain values)
     * For now, use the X register bridge: load bits into X, FMOV to D */
    /* Use dst as temp X register, then bridge to D0, operate, bridge back */
    /* Actually, load the 64-bit bit pattern into the dst X register */
    union { double d; int64_t i; } u;
    u.d = val;
    emit_load_imm64(tb, dst, u.i);
    return dst;
}

/* Check if an instruction's operands are float type */
static int
is_float_op(ir_instr_t *inst)
{
    if (inst->result.n > 0 && inst->result.reg[0].type == IR_REG_F64)
        return 1;
    if (inst->result.n > 0 && inst->result.reg[0].type == IR_REG_F32)
        return 1;
    return 0;
}

/* Check if an immediate operand is a float */
static int
is_float_imm(ir_operand_t *op)
{
    return op->type == IR_OPERAND_IMM &&
           (op->u.imm.type == IR_IMM_F64 || op->u.imm.type == IR_IMM_F32);
}

/* Get float immediate value */
static double
operand_float_imm(ir_operand_t *op, int *ok)
{
    if (op->type == IR_OPERAND_IMM) {
        if (op->u.imm.type == IR_IMM_F64) { *ok = 1; return op->u.imm.u.f64; }
        if (op->u.imm.type == IR_IMM_F32) { *ok = 1; return (double)op->u.imm.u.f32; }
    }
    *ok = 0;
    return 0.0;
}


static int
is_float_cmp(ir_instr_t *inst)
{
    for (int i = 0; i < inst->noperands && i < IR_MAX_OPERANDS; i++) {
        if (inst->operands[i].type == IR_OPERAND_REG &&
            inst->operands[i].u.reg.type == IR_REG_F64)
            return 1;
        if (inst->operands[i].type == IR_OPERAND_REG &&
            inst->operands[i].u.reg.type == IR_REG_F32)
            return 1;
        if (is_float_imm(&inst->operands[i]))
            return 1;
    }
    return 0;
}

static int
compile_instr(asm_ctx_t *ctx, ir_instr_t *inst)
{
    int dst, src0, src1;
    int ok;
    int64_t imm;

    int rspill = 0;
    dst = 31;
    if (inst->result.n > 0) {
        int id = ssa_id(inst->result.reg[0].id);
        dst = ssa_to_reg(id);
        if (dst < 0) { dst = 16; rspill = id; }
    }

    switch (inst->opcode) {
    case IR_OPCODE_CONST:
        if (inst->operands[0].type == IR_OPERAND_IMM &&
            inst->operands[0].u.imm.type == IR_IMM_STR) {
            const char *str = inst->operands[0].u.imm.u.str;
            if (!str) str = "";
            /* Check if this is a global variable reference */
            ir_global_t *glob = ir_object_find_global(ctx->ir_obj, str);
            if (glob) {
                int si = -1;
                for (int s = 0; s < ctx->code->sym.n; s++) {
                    if (ctx->code->sym.syms[s].label &&
                        strcmp(ctx->code->sym.syms[s].label, str) == 0) {
                        si = s; break;
                    }
                }
                if (si < 0) si = add_sym(ctx->code, ARCH_SYM_GLOBAL, str, 0, 8);
                /* ADRP Xdst, #0 — page address (patched by linker) */
                size_t adrp_off = ctx->tb.size;
                uint32_t adrp = (1U << 31) | (1U << 28) | (dst & 31);
                emit32(&ctx->tb, adrp);
                add_rel(ctx->code, ARCH_REL_AARCH64_PAGE21, adrp_off, si);
                break;
            }
            /* String constant: store in data section, emit ADR to reference it */
            int sid = add_string(ctx, str);
            /* ADR Xd, #0 — placeholder, will be patched with string offset */
            /* ADR: 0 immlo 10000 immhi Rd */
            size_t off = ctx->tb.size;
            uint32_t adr = (1U << 28) | (dst & 31);  /* immlo=0, immhi=0 */
            emit32(&ctx->tb, adr);
            /* We need a special relocation type for string refs */
            /* For now, use ARCH_REL_PC32 with the string index as sym */
            /* But strings aren't in the symbol table... use a convention */
            /* Store: rel.pos = text offset, rel.sym = -(sid+1) (negative = string) */
            str_patch_add(ctx, off, sid);
            break;
        }
        /* Check for float immediate */
        if (is_float_imm(&inst->operands[0])) {
            int fok;
            double fval = operand_float_imm(&inst->operands[0], &fok);
            if (fok) {
                { int __rc = emit_load_imm_double(&ctx->tb, dst, fval); if (rspill) spill_store(dst, rspill); return __rc; }
            }
        }
        imm = operand_imm(&inst->operands[0], &ok);
        if (!ok) return -1;
        { int __rc = emit_load_imm64(&ctx->tb, dst, imm); if (rspill) spill_store(dst, rspill); return __rc; }

    case IR_OPCODE_MOV: {
        /* operands[0] = source, operands[1] = destination.
         * Bug fix: when destination is spilled, operand_reg loads the OLD
         * value from the spill slot into X16, clobbering the source if it
         * was also loaded into X16. Fix: use X17 for source, X16 for dest. */
        if (inst->operands[1].type == IR_OPERAND_REG) {
            int mov_dst_id = ssa_id(inst->operands[1].u.reg.id);
            int mov_dst_reg = ssa_to_reg(mov_dst_id);
            if (mov_dst_reg < 0) {
                /* Destination is spilled: load source into X17, copy to X16,
                 * then store X16 to destination spill slot */
                src0 = operand_reg_or_imm_scratch(ctx, &inst->operands[0], 17);
                emit_orr_reg(&ctx->tb, 16, 31, src0, 1);  /* mov x16, src0 */
                spill_store(16, mov_dst_id);
                return 0;
            }
            /* Destination is in a register: normal path */
            src0 = operand_reg_or_imm_scratch(ctx, &inst->operands[0], 16);
            { int __rc = emit_orr_reg(&ctx->tb, mov_dst_reg, 31, src0, 1); if (rspill) spill_store(mov_dst_reg, rspill); return __rc; }
        }
        /* Fallback for non-register destination (shouldn't happen for MOV) */
        src0 = operand_reg(&inst->operands[0]);
        dst = operand_reg(&inst->operands[1]);
        { int __rc = emit_orr_reg(&ctx->tb, dst, 31, src0, 1); if (rspill) spill_store(dst, rspill); return __rc; }
    }

    case IR_OPCODE_ADD:
        if (is_float_op(inst)) {
            src0 = operand_reg_or_imm_scratch(ctx, &inst->operands[0], 16);
            src1 = operand_reg_or_imm_scratch(ctx, &inst->operands[1], 17);
            emit_fmov_dx(&ctx->tb, 0, src0);
            emit_fmov_dx(&ctx->tb, 1, src1);
            emit_fadd(&ctx->tb, 0, 0, 1);
            { int __rc = emit_fmov_xd(&ctx->tb, dst, 0); if (rspill) spill_store(dst, rspill); return __rc; }
        }
        src0 = operand_reg_or_imm_scratch(ctx, &inst->operands[0], 16);
        src1 = operand_reg_or_imm_scratch(ctx, &inst->operands[1], 17);
        { int __rc = emit_add_reg(&ctx->tb, dst, src0, src1, 1); if (rspill) spill_store(dst, rspill); return __rc; }

    case IR_OPCODE_SUB:
        if (is_float_op(inst)) {
            src0 = operand_reg_or_imm_scratch(ctx, &inst->operands[0], 16);
            src1 = operand_reg_or_imm_scratch(ctx, &inst->operands[1], 17);
            emit_fmov_dx(&ctx->tb, 0, src0);
            emit_fmov_dx(&ctx->tb, 1, src1);
            emit_fsub(&ctx->tb, 0, 0, 1);
            { int __rc = emit_fmov_xd(&ctx->tb, dst, 0); if (rspill) spill_store(dst, rspill); return __rc; }
        }
        src0 = operand_reg_or_imm_scratch(ctx, &inst->operands[0], 16);
        src1 = operand_reg_or_imm_scratch(ctx, &inst->operands[1], 17);
        { int __rc = emit_sub_reg(&ctx->tb, dst, src0, src1, 1); if (rspill) spill_store(dst, rspill); return __rc; }

    case IR_OPCODE_MUL:
        if (is_float_op(inst)) {
            src0 = operand_reg_or_imm_scratch(ctx, &inst->operands[0], 16);
            src1 = operand_reg_or_imm_scratch(ctx, &inst->operands[1], 17);
            emit_fmov_dx(&ctx->tb, 0, src0);
            emit_fmov_dx(&ctx->tb, 1, src1);
            emit_fmul(&ctx->tb, 0, 0, 1);
            { int __rc = emit_fmov_xd(&ctx->tb, dst, 0); if (rspill) spill_store(dst, rspill); return __rc; }
        }
        src0 = operand_reg_or_imm_scratch(ctx, &inst->operands[0], 16);
        src1 = operand_reg_or_imm_scratch(ctx, &inst->operands[1], 17);
        { int __rc = emit_mul_reg(&ctx->tb, dst, src0, src1, 1); if (rspill) spill_store(dst, rspill); return __rc; }

    case IR_OPCODE_DIV:
        if (is_float_op(inst)) {
            src0 = operand_reg_or_imm_scratch(ctx, &inst->operands[0], 16);
            src1 = operand_reg_or_imm_scratch(ctx, &inst->operands[1], 17);
            emit_fmov_dx(&ctx->tb, 0, src0);
            emit_fmov_dx(&ctx->tb, 1, src1);
            emit_fdiv(&ctx->tb, 0, 0, 1);
            { int __rc = emit_fmov_xd(&ctx->tb, dst, 0); if (rspill) spill_store(dst, rspill); return __rc; }
        }
        src0 = operand_reg_or_imm_scratch(ctx, &inst->operands[0], 16);
        src1 = operand_reg_or_imm_scratch(ctx, &inst->operands[1], 17);
        { int __rc = emit_sdiv_reg(&ctx->tb, dst, src0, src1, 1); if (rspill) spill_store(dst, rspill); return __rc; }

    case IR_OPCODE_UDIV:
        src0 = operand_reg_or_imm_scratch(ctx, &inst->operands[0], 16);
        src1 = operand_reg_or_imm_scratch(ctx, &inst->operands[1], 17);
        { int __rc = emit_udiv_reg(&ctx->tb, dst, src0, src1, 1); if (rspill) spill_store(dst, rspill); return __rc; }

    case IR_OPCODE_MOD: {
        /* mod a, b = a - (a / b) * b  =>  MSUB Rd=Ra-Rn*Rm */
        src0 = operand_reg_or_imm_scratch(ctx, &inst->operands[0], 16);
        src1 = operand_reg_or_imm_scratch(ctx, &inst->operands[1], 17);
        emit_sdiv_reg(&ctx->tb, dst, src0, src1, 1);
        { int __rc = emit_msub_reg(&ctx->tb, dst, dst, src1, src0, 1); if (rspill) spill_store(dst, rspill); return __rc; }
    }

    case IR_OPCODE_SHL:
        src0 = operand_reg_or_imm_scratch(ctx, &inst->operands[0], 16);
        src1 = operand_reg_or_imm_scratch(ctx, &inst->operands[1], 17);
        { int __rc = emit_lsl_reg(&ctx->tb, dst, src0, src1, 1); if (rspill) spill_store(dst, rspill); return __rc; }

    case IR_OPCODE_SHR:
        src0 = operand_reg_or_imm_scratch(ctx, &inst->operands[0], 16);
        src1 = operand_reg_or_imm_scratch(ctx, &inst->operands[1], 17);
        { int __rc = emit_lsr_reg(&ctx->tb, dst, src0, src1, 1); if (rspill) spill_store(dst, rspill); return __rc; }

    case IR_OPCODE_NEG:
        /* neg dst, src0 = sub dst, xzr, src0 */
        src0 = operand_reg_or_imm_scratch(ctx, &inst->operands[0], 16);
        { int __rc = emit_sub_reg(&ctx->tb, dst, 31, src0, 1);  /* sub rd, xzr, src0 */; if (rspill) spill_store(dst, rspill); return __rc; }

    case IR_OPCODE_NOT:
        src0 = operand_reg(&inst->operands[0]);
        { int __rc = emit_orn_reg(&ctx->tb, dst, 31, src0, 1);  /* orn rd, xzr, src0 */; if (rspill) spill_store(dst, rspill); return __rc; }

    case IR_OPCODE_AND:
        src0 = operand_reg_or_imm_scratch(ctx, &inst->operands[0], 16);
        src1 = operand_reg_or_imm_scratch(ctx, &inst->operands[1], 17);
        { int __rc = emit_and_reg(&ctx->tb, dst, src0, src1, 1); if (rspill) spill_store(dst, rspill); return __rc; }

    case IR_OPCODE_OR:
        src0 = operand_reg_or_imm_scratch(ctx, &inst->operands[0], 16);
        src1 = operand_reg_or_imm_scratch(ctx, &inst->operands[1], 17);
        { int __rc = emit_orr_reg(&ctx->tb, dst, src0, src1, 1); if (rspill) spill_store(dst, rspill); return __rc; }

    case IR_OPCODE_XOR:
        src0 = operand_reg(&inst->operands[0]);
        src1 = operand_reg_or_imm(ctx, &inst->operands[1]);
        { int __rc = emit_eor_reg(&ctx->tb, dst, src0, src1, 1); if (rspill) spill_store(dst, rspill); return __rc; }

    case IR_OPCODE_CMP_EQ:
        src0 = operand_reg_or_imm_scratch(ctx, &inst->operands[0], 16);
        src1 = operand_reg_or_imm_scratch(ctx, &inst->operands[1], 17);
        if (is_float_cmp(inst)) {
            emit_fmov_dx(&ctx->tb, 0, src0);
            emit_fmov_dx(&ctx->tb, 1, src1);
            emit_fcmp(&ctx->tb, 0, 1);
            { int __rc = emit_cset(&ctx->tb, dst, COND_EQ, 1); if (rspill) spill_store(dst, rspill); return __rc; }
        }
        emit_cmp_reg(&ctx->tb, src0, src1, 1);
        { int __rc = emit_cset(&ctx->tb, dst, COND_EQ, 1); if (rspill) spill_store(dst, rspill); return __rc; }

    case IR_OPCODE_CMP_NE:
        src0 = operand_reg_or_imm_scratch(ctx, &inst->operands[0], 16);
        src1 = operand_reg_or_imm_scratch(ctx, &inst->operands[1], 17);
        if (is_float_cmp(inst)) {
            emit_fmov_dx(&ctx->tb, 0, src0);
            emit_fmov_dx(&ctx->tb, 1, src1);
            emit_fcmp(&ctx->tb, 0, 1);
            { int __rc = emit_cset(&ctx->tb, dst, COND_NE, 1); if (rspill) spill_store(dst, rspill); return __rc; }
        }
        emit_cmp_reg(&ctx->tb, src0, src1, 1);
        { int __rc = emit_cset(&ctx->tb, dst, COND_NE, 1); if (rspill) spill_store(dst, rspill); return __rc; }

    case IR_OPCODE_CMP_LT:
        src0 = operand_reg_or_imm_scratch(ctx, &inst->operands[0], 16);
        src1 = operand_reg_or_imm_scratch(ctx, &inst->operands[1], 17);
        if (is_float_cmp(inst)) {
            emit_fmov_dx(&ctx->tb, 0, src0);
            emit_fmov_dx(&ctx->tb, 1, src1);
            emit_fcmp(&ctx->tb, 0, 1);
            { int __rc = emit_cset(&ctx->tb, dst, COND_LT, 1); if (rspill) spill_store(dst, rspill); return __rc; }
        }
        emit_cmp_reg(&ctx->tb, src0, src1, 1);
        { int __rc = emit_cset(&ctx->tb, dst, COND_LT, 1); if (rspill) spill_store(dst, rspill); return __rc; }

    case IR_OPCODE_CMP_LE:
        src0 = operand_reg_or_imm_scratch(ctx, &inst->operands[0], 16);
        src1 = operand_reg_or_imm_scratch(ctx, &inst->operands[1], 17);
        if (is_float_cmp(inst)) {
            emit_fmov_dx(&ctx->tb, 0, src0);
            emit_fmov_dx(&ctx->tb, 1, src1);
            emit_fcmp(&ctx->tb, 0, 1);
            { int __rc = emit_cset(&ctx->tb, dst, COND_LE, 1); if (rspill) spill_store(dst, rspill); return __rc; }
        }
        emit_cmp_reg(&ctx->tb, src0, src1, 1);
        { int __rc = emit_cset(&ctx->tb, dst, COND_LE, 1); if (rspill) spill_store(dst, rspill); return __rc; }

    case IR_OPCODE_CMP_GT:
        src0 = operand_reg_or_imm_scratch(ctx, &inst->operands[0], 16);
        src1 = operand_reg_or_imm_scratch(ctx, &inst->operands[1], 17);
        if (is_float_cmp(inst)) {
            emit_fmov_dx(&ctx->tb, 0, src0);
            emit_fmov_dx(&ctx->tb, 1, src1);
            emit_fcmp(&ctx->tb, 0, 1);
            { int __rc = emit_cset(&ctx->tb, dst, COND_GT, 1); if (rspill) spill_store(dst, rspill); return __rc; }
        }
        emit_cmp_reg(&ctx->tb, src0, src1, 1);
        { int __rc = emit_cset(&ctx->tb, dst, COND_GT, 1); if (rspill) spill_store(dst, rspill); return __rc; }

    case IR_OPCODE_CMP_GE:
        src0 = operand_reg_or_imm_scratch(ctx, &inst->operands[0], 16);
        src1 = operand_reg_or_imm_scratch(ctx, &inst->operands[1], 17);
        if (is_float_cmp(inst)) {
            emit_fmov_dx(&ctx->tb, 0, src0);
            emit_fmov_dx(&ctx->tb, 1, src1);
            emit_fcmp(&ctx->tb, 0, 1);
            { int __rc = emit_cset(&ctx->tb, dst, COND_GE, 1); if (rspill) spill_store(dst, rspill); return __rc; }
        }
        emit_cmp_reg(&ctx->tb, src0, src1, 1);
        { int __rc = emit_cset(&ctx->tb, dst, COND_GE, 1); if (rspill) spill_store(dst, rspill); return __rc; }

    case IR_OPCODE_BR: {
        /* b label — emit placeholder, save patch for resolution */
        size_t off = ctx->tb.size;
        emit_b(&ctx->tb, 0);
        if (inst->noperands > 0 && inst->operands[0].type == IR_OPERAND_LABEL) {
            patch_list_add(&ctx->patches, off, inst->operands[0].u.label, -1);
        }
        break;
    }

    case IR_OPCODE_BR_COND: {
        /* test cond; if false, jump to $else; if true, jump to $then */
        src0 = operand_reg(&inst->operands[0]);
        /* CBZ Xt, $else — if cond==0, branch to else */
        size_t off = ctx->tb.size;
        uint32_t cbz = (1U << 31) | (0x34U << 24) | (src0 & 31);
        emit32(&ctx->tb, cbz);
        if (inst->noperands > 2 && inst->operands[2].type == IR_OPERAND_LABEL) {
            patch_list_add(&ctx->patches, off, inst->operands[2].u.label, src0);
        }
        /* B $then — unconditional branch to then block */
        size_t off2 = ctx->tb.size;
        emit_b(&ctx->tb, 0);
        if (inst->noperands > 1 && inst->operands[1].type == IR_OPERAND_LABEL) {
            patch_list_add(&ctx->patches, off2, inst->operands[1].u.label, -1);
        }
        break;
    }

    case IR_OPCODE_RET:
        /* Move return value to X0 if needed */
        if (inst->noperands > 0) {
            if (inst->operands[0].type == IR_OPERAND_IMM) {
                /* Load immediate into X0 */
                int ok;
                int64_t val = operand_imm(&inst->operands[0], &ok);
                if (ok) emit_load_imm64(&ctx->tb, 0, val);
            } else {
                src0 = operand_reg(&inst->operands[0]);
                if (src0 != 0) {
                    emit_orr_reg(&ctx->tb, 0, 31, src0, 1);  /* mov x0, src0 */
                }
            }
        }
        /* emit_epilogue does "mov sp, x29" which restores SP past all
         * stack allocations, so no manual spill dealloc needed. */
        emit_epilogue(ctx, ctx->max_ssa);
        break;

    case IR_OPCODE_CALL:
        {
            /* Last operand is callee name, preceding operands are args */
            int nargs = inst->noperands - 1;
            const char *callee = NULL;
            if (inst->noperands > 0 &&
                inst->operands[inst->noperands - 1].type == IR_OPERAND_IMM &&
                inst->operands[inst->noperands - 1].u.imm.type == IR_IMM_STR) {
                callee = inst->operands[inst->noperands - 1].u.imm.u.str;
            }

            /* Handle builtins: print, println */
            if (callee && (strcmp(callee, "print") == 0 ||
                           strcmp(callee, "println") == 0)) {
                int is_println = (strcmp(callee, "println") == 0);
                /* Save caller-saved registers around builtin calls */
                emit_caller_save(ctx);
                if (nargs > 0) {
                    ir_operand_t *arg = &inst->operands[0];
                    if (arg->type == IR_OPERAND_IMM &&
                        arg->u.imm.type == IR_IMM_STR) {
                        /* String argument */
                        const char *str = arg->u.imm.u.str ? arg->u.imm.u.str : "";
                        if (is_println) {
                            /* println(string): use puts (adds newline) */
                            int sid = add_string(ctx, str);
                            size_t off = ctx->tb.size;
                            emit32(&ctx->tb, (1U << 28) | 0);
                            str_patch_add(ctx, off, sid);
                            int si = add_sym(ctx->code, ARCH_SYM_GLOBAL, "puts", 0, 0);
                            off_t rp = ctx->tb.size;
                            emit_bl(&ctx->tb, 0);
                            add_rel(ctx->code, ARCH_REL_BRANCH, rp, si);
                        } else {
                            /* print(string): use printf with the string as
                             * the format string directly (no %s needed).
                             * This avoids variadic ABI issues with X1. */
                            int sid_str = add_string(ctx, str);
                            size_t off = ctx->tb.size;
                            emit32(&ctx->tb, (1U << 28) | 0);  /* ADR X0, #0 */
                            str_patch_add(ctx, off, sid_str);
                            /* BL printf */
                            int si = add_sym(ctx->code, ARCH_SYM_GLOBAL, "printf", 0, 0);
                            off_t rp = ctx->tb.size;
                            emit_bl(&ctx->tb, 0);
                            add_rel(ctx->code, ARCH_REL_BRANCH, rp, si);
                        }
                    } else {
                        /* Integer argument: convert to string at compile time
                         * if it's a constant, or use printf for variables.
                         * For constants: puts("42\n") — simple and correct.
                         * For variables: printf("%d\n", val) */
                        int ok;
                        int64_t val = operand_imm(arg, &ok);
                        if (ok) {
                            /* Constant: convert to string and use puts */
                            char buf[32];
                            if (is_println) {
                                snprintf(buf, sizeof(buf), "%lld\n", (long long)val);
                            } else {
                                snprintf(buf, sizeof(buf), "%lld", (long long)val);
                            }
                            int sid = add_string(ctx, buf);
                            size_t off = ctx->tb.size;
                            emit32(&ctx->tb, (1U << 28) | 0);
                            str_patch_add(ctx, off, sid);
                            int si = add_sym(ctx->code, ARCH_SYM_GLOBAL, "puts", 0, 0);
                            off_t rp = ctx->tb.size;
                            emit_bl(&ctx->tb, 0);
                            add_rel(ctx->code, ARCH_REL_BRANCH, rp, si);
                        } else {
                            /* Variable: macOS arm64 printf is variadic —
                             * arguments go on the stack, not in X1. */
                            int is_float = is_float_imm(arg) ||
                                (arg->type == IR_OPERAND_REG &&
                                 (arg->u.reg.type == IR_REG_F64 ||
                                  arg->u.reg.type == IR_REG_F32));
                            const char *fmt;
                            if (is_float) {
                                fmt = is_println ? "%f\n" : "%f";
                            } else {
                                fmt = is_println ? "%d\n" : "%d";
                            }
                            /* Load value */
                            int val_reg;
                            if (is_float && arg->type == IR_OPERAND_IMM) {
                                int fok;
                                double fv = operand_float_imm(arg, &fok);
                                if (fok) {
                                    emit_load_imm_double(&ctx->tb, 8, fv);
                                    val_reg = 8;
                                } else {
                                    val_reg = 31;
                                }
                            } else if (arg->type == IR_OPERAND_IMM) {
                                int ok;
                                int64_t v = operand_imm(arg, &ok);
                                if (ok) {
                                    emit_load_imm64(&ctx->tb, 8, v);
                                    val_reg = 8;
                                } else {
                                    val_reg = 31;
                                }
                            } else {
                                val_reg = operand_reg(arg);
                            }
                            /* Store value to [sp] (variadic arg area at [SP, #0]) */
                            emit32(&ctx->tb, 0xF9000000 | (31 << 5) | (val_reg & 31));
                            /* X0 = format string */
                            int sid_fmt = add_string(ctx, fmt);
                            size_t off1 = ctx->tb.size;
                            emit32(&ctx->tb, (1U << 28) | 0);
                            str_patch_add(ctx, off1, sid_fmt);
                            /* BL printf */
                            int si = add_sym(ctx->code, ARCH_SYM_GLOBAL, "printf", 0, 0);
                            off_t rp = ctx->tb.size;
                            emit_bl(&ctx->tb, 0);
                            add_rel(ctx->code, ARCH_REL_BRANCH, rp, si);
                        }
                    }
                }
                /* Restore caller-saved registers after builtin call */
                emit_caller_restore(ctx);
                break;
            }
            /* Save caller-saved registers (X0-X18) around the call.
             * We save all registers up to max_ssa (capped at 18) to the
             * stack, plus one extra slot for the return value. */
            int max_cs = ctx->max_ssa < 18 ? ctx->max_ssa : 18;
            int n_cs = max_cs + 1;  /* X0 through X(max_cs) */
            /* Round up to even for alignment */
            if (n_cs % 2 != 0) n_cs++;
            int save_size = n_cs * 8 + 32;  /* +16 for return value, +16 for variadic/alignment */

            /* sub sp, sp, #save_size */
            {
                uint32_t sub = (1U << 31) | (0x51U << 24) |
                              ((save_size & 0xFFF) << 10) | (31U << 5) | 31;
                emit32(&ctx->tb, sub);
            }
            /* Save caller-saved registers: str xi, [sp, #16+i*8] */
            for (int i = 0; i <= max_cs; i++) {
                int off = 16 + i * 8;
                uint32_t str = (0xF9U << 24) | (((off / 8) & 0xFFF) << 10) |
                              (31U << 5) | i;
                emit32(&ctx->tb, str);
            }

            /* Move arguments to argument registers X0-X7.
             * We use a two-phase approach to avoid register collisions:
             * Phase 1: Load all register arguments from the stack (where
             *          they were saved before argument setup) into scratch
             *          registers X8-X15.
             * Phase 2: Move from scratch registers to argument registers.
             * For immediate arguments, load directly into arg registers.
             * For string arguments, emit ADR directly into arg registers. */
            /* Move arguments to argument registers X0-X7.
             * Strategy: process in two phases to avoid collisions.
             * Phase 1: Save all caller-saved register args to scratch (X8-X15).
             * Phase 2: Move from scratch/immediate/callee-saved to arg regs. */
            int scratch_base = 8;
            /* Phase 1: Save caller-saved register args to scratch regs */
            for (int i = 0; i < nargs && i < 8; i++) {
                ir_operand_t *arg = &inst->operands[i];
                if (arg->type == IR_OPERAND_REG) {
                    int src_reg = ssa_to_reg(ssa_id(arg->u.reg.id));
                    if (src_reg < 0) {
                        /* Spilled: load into scratch */
                        spill_load(ssa_id(arg->u.reg.id), scratch_base + i);
                    } else if (src_reg < 19) {
                        /* Caller-saved (X0-X15): reload from saved stack slot */
                        int off = 16 + src_reg * 8;
                        uint32_t ldr = (0xF9U << 24) | (1U << 22) |
                                      (((off / 8) & 0xFFF) << 10) |
                                      (31U << 5) | (scratch_base + i);
                        emit32(&ctx->tb, ldr);
                    } else {
                        /* Callee-saved (X19-X28): value survives the call,
                         * just remember the register for phase 2 */
                    }
                }
            }
            /* Phase 2: Move to argument registers */
            for (int i = 0; i < nargs && i < 8; i++) {
                ir_operand_t *arg = &inst->operands[i];
                if (arg->type == IR_OPERAND_IMM && arg->u.imm.type == IR_IMM_STR) {
                    const char *str = arg->u.imm.u.str ? arg->u.imm.u.str : "";
                    int sid = add_string(ctx, str);
                    size_t off = ctx->tb.size;
                    uint32_t adr = (1U << 28) | (arg_regs[i] & 31);
                    emit32(&ctx->tb, adr);
                    str_patch_add(ctx, off, sid);
                } else if (arg->type == IR_OPERAND_IMM) {
                    int ok;
                    int64_t val = operand_imm(arg, &ok);
                    if (ok) emit_load_imm64(&ctx->tb, arg_regs[i], val);
                } else if (arg->type == IR_OPERAND_REG) {
                    int id = ssa_id(arg->u.reg.id);
                    int src_reg = ssa_to_reg(id);
                    if (src_reg < 0) {
                        /* Was spilled: move from scratch */
                        emit_orr_reg(&ctx->tb, arg_regs[i], 31, scratch_base + i, 1);
                    } else if (src_reg < 19) {
                        /* Was caller-saved: move from scratch */
                        emit_orr_reg(&ctx->tb, arg_regs[i], 31, scratch_base + i, 1);
                    } else {
                        /* Callee-saved: move directly (value is preserved) */
                        if (src_reg != arg_regs[i]) {
                            emit_orr_reg(&ctx->tb, arg_regs[i], 31, src_reg, 1);
                        }
                    }
                }
            }

            /* Find or create symbol */
            int symidx = -1;
            if (callee) {
                for (int i = 0; i < ctx->code->sym.n; i++) {
                    if (ctx->code->sym.syms[i].label &&
                        strcmp(ctx->code->sym.syms[i].label, callee) == 0) {
                        symidx = i;
                        break;
                    }
                }
                if (symidx < 0) {
                    symidx = add_sym(ctx->code, ARCH_SYM_FUNC, callee, 0, 0);
                }
            }

            /* BL (branch with link) */
            off_t relpos = ctx->tb.size;
            emit_bl(&ctx->tb, 0);  /* placeholder, resolved by relocation */
            if (symidx >= 0) {
                add_rel(ctx->code, ARCH_REL_BRANCH, relpos, symidx);
            }

            /* Save return value to extra stack slot */
            {
                int ret_off = 16 + n_cs * 8;  /* past saved regs */
                uint32_t str = (0xF9U << 24) | (((ret_off / 8) & 0xFFF) << 10) |
                              (31U << 5) | 0;
                emit32(&ctx->tb, str);
            }

            /* Restore caller-saved registers: ldr xi, [sp, #16+i*8] */
            for (int i = 0; i <= max_cs; i++) {
                int off = 16 + i * 8;
                uint32_t ldr = (0xF9U << 24) | (1U << 22) |
                              (((off / 8) & 0xFFF) << 10) | (31U << 5) | i;
                emit32(&ctx->tb, ldr);
            }

            /* Move return value from stack slot to destination register */
            if (dst != 31) {
                int ret_off = 16 + n_cs * 8;
                uint32_t ldr = (0xF9U << 24) | (1U << 22) |
                              (((ret_off / 8) & 0xFFF) << 10) | (31U << 5) | dst;
                emit32(&ctx->tb, ldr);
            }

            /* add sp, sp, #save_size */
            {
                uint32_t add = (1U << 31) | (0x11U << 24) |
                              ((save_size & 0xFFF) << 10) | (31U << 5) | 31;
                emit32(&ctx->tb, add);
            }
            break;
        }

    case IR_OPCODE_ALLOCA: {
        /* Allocate stack space. Default 16 bytes, or use operand size. */
        int size = 16;
        if (inst->noperands > 0 && inst->operands[0].type == IR_OPERAND_IMM) {
            int ok;
            int64_t val = operand_imm(&inst->operands[0], &ok);
            if (ok && val > 0) {
                /* Round up to 16-byte alignment */
                size = ((val + 15) / 16) * 16;
                if (size > 0xFFF) size = 0xFFF;
            }
        }
        /* sub sp, sp, #size */
        uint32_t insn = (1U << 31) | (0x51U << 24) | ((size & 0xFFF) << 10) |
                        (31U << 5) | 31;
        emit32(&ctx->tb, insn);
        /* mov dst, sp = add dst, sp, #0 */
        if (dst != 31) {
            uint32_t mov_sp = (1U << 31) | (0x11U << 24) | (31U << 5) | (dst & 31);
            emit32(&ctx->tb, mov_sp);
        }
        break;
    }

    case IR_OPCODE_LOAD: {
        /* LDR Xt, [Xn] — 11 111 0 01 01 imm12 Rn Rt (imm12=0)
         * Base = 0xF9400000 (bit 22 set for LDR) */
        int base_reg;
        if (inst->operands[0].type == IR_OPERAND_IMM &&
            inst->operands[0].u.imm.type == IR_IMM_STR) {
            const char *str = inst->operands[0].u.imm.u.str;
            if (!str) str = "";
            ir_global_t *glob = ir_object_find_global(ctx->ir_obj, str);
            if (glob) {
                int si = -1;
                for (int s = 0; s < ctx->code->sym.n; s++) {
                    if (ctx->code->sym.syms[s].label &&
                        strcmp(ctx->code->sym.syms[s].label, str) == 0) {
                        si = s; break;
                    }
                }
                if (si < 0) si = add_sym(ctx->code, ARCH_SYM_GLOBAL, str, 0, 8);
                /* ADRP X16, #0 + LDR Xd, [X16, #0] */
                size_t adrp_off = ctx->tb.size;
                uint32_t adrp = (1U << 31) | (1U << 28) | (16 & 31);
                emit32(&ctx->tb, adrp);
                add_rel(ctx->code, ARCH_REL_AARCH64_PAGE21, adrp_off, si);
                size_t ldr_off = ctx->tb.size;
                uint32_t ldr = (0xF9U << 24) | (1U << 22) | ((16 & 31) << 5) | (dst & 31);
                emit32(&ctx->tb, ldr);
                add_rel(ctx->code, ARCH_REL_AARCH64_PAGEOFF12, ldr_off, si);
                break;
            } else {
                int sid = add_string(ctx, str);
                size_t off = ctx->tb.size;
                uint32_t adr = (1U << 28) | (16 & 31);
                emit32(&ctx->tb, adr);
                str_patch_add(ctx, off, sid);
                base_reg = 16;
            }
        } else {
            base_reg = operand_reg(&inst->operands[0]);
        }
        uint32_t insn = (0xF9U << 24) | (1U << 22) | ((base_reg & 31) << 5) | (dst & 31);
        { int __rc = emit32(&ctx->tb, insn); if (rspill) spill_store(dst, rspill); return __rc; }
    }

    case IR_OPCODE_STORE: {
        /* STR Xt, [Xn] — 11 111 0 01 00 imm12 Rn Rt (imm12=0)
         * Base = 0xF9000000 (bit 22 clear for STR)
         * operands[0] = address (Rn), operands[1] = value (Rt) */
        int addr_reg;
        if (inst->operands[0].type == IR_OPERAND_IMM &&
            inst->operands[0].u.imm.type == IR_IMM_STR) {
            const char *str = inst->operands[0].u.imm.u.str;
            if (!str) str = "";
            ir_global_t *glob = ir_object_find_global(ctx->ir_obj, str);
            if (glob) {
                int si = -1;
                for (int s = 0; s < ctx->code->sym.n; s++) {
                    if (ctx->code->sym.syms[s].label &&
                        strcmp(ctx->code->sym.syms[s].label, str) == 0) {
                        si = s; break;
                    }
                }
                if (si < 0) si = add_sym(ctx->code, ARCH_SYM_GLOBAL, str, 0, 8);
                /* Load value first, then ADRP+STR */
                int val_reg = operand_reg_or_imm_scratch(ctx, &inst->operands[1], 17);
                size_t adrp_off = ctx->tb.size;
                uint32_t adrp = (1U << 31) | (1U << 28) | (16 & 31);
                emit32(&ctx->tb, adrp);
                add_rel(ctx->code, ARCH_REL_AARCH64_PAGE21, adrp_off, si);
                size_t str_off = ctx->tb.size;
                uint32_t str_insn = (0xF9U << 24) | ((16 & 31) << 5) | (val_reg & 31);
                emit32(&ctx->tb, str_insn);
                add_rel(ctx->code, ARCH_REL_AARCH64_PAGEOFF12, str_off, si);
                break;
            } else {
                addr_reg = operand_reg_or_imm_scratch(ctx, &inst->operands[0], 16);
            }
        } else {
            addr_reg = operand_reg(&inst->operands[0]);
        }
        int val_reg = operand_reg_or_imm_scratch(ctx, &inst->operands[1], 17);
        uint32_t insn = (0xF9U << 24) | ((addr_reg & 31) << 5) | (val_reg & 31);
        { int __rc = emit32(&ctx->tb, insn); if (rspill) spill_store(dst, rspill); return __rc; }
    }

    case IR_OPCODE_LOAD8: {
        /* LDRB Wt, [Xbase, Xidx] — load byte at base+idx
         * Encoding: 00111000 01 011010 1 Rm Rn Rt (register offset, size=00)
         * = 0x38606800 | (Rm << 16) | (Rn << 5) | Rt
         * If base is a string immediate, emit ADR to X16 first. */
        int base_reg;
        if (inst->operands[0].type == IR_OPERAND_IMM &&
            inst->operands[0].u.imm.type == IR_IMM_STR) {
            const char *str = inst->operands[0].u.imm.u.str ? inst->operands[0].u.imm.u.str : "";
            int sid = add_string(ctx, str);
            size_t off = ctx->tb.size;
            uint32_t adr = (1U << 28) | (16 & 31);  /* ADR X16, #0 */
            emit32(&ctx->tb, adr);
            str_patch_add(ctx, off, sid);
            base_reg = 16;
        } else {
            base_reg = operand_reg(&inst->operands[0]);
        }
        int idx_reg = operand_reg_or_imm_scratch(ctx, &inst->operands[1], 17);
        /* LDRB Wt, [Xbase, Xidx] */
        uint32_t insn = 0x38606800U | ((idx_reg & 31) << 16) | ((base_reg & 31) << 5) | (dst & 31);
        { int __rc = emit32(&ctx->tb, insn); if (rspill) spill_store(dst, rspill); return __rc; }
    }

    case IR_OPCODE_STORE8: {
        /* STRB Wt, [Xbase, Xidx] — store byte at base+idx
         * Encoding: 00111000 00 011010 1 Rm Rn Rt (register offset, size=00)
         * = 0x38206800 | (Rm << 16) | (Rn << 5) | Rt
         * If base is a string immediate, emit ADR to X16 first. */
        int base_reg;
        if (inst->operands[0].type == IR_OPERAND_IMM &&
            inst->operands[0].u.imm.type == IR_IMM_STR) {
            const char *str = inst->operands[0].u.imm.u.str ? inst->operands[0].u.imm.u.str : "";
            int sid = add_string(ctx, str);
            size_t off = ctx->tb.size;
            uint32_t adr = (1U << 28) | (16 & 31);  /* ADR X16, #0 */
            emit32(&ctx->tb, adr);
            str_patch_add(ctx, off, sid);
            base_reg = 16;
        } else {
            base_reg = operand_reg(&inst->operands[0]);
        }
        int idx_reg = operand_reg_or_imm_scratch(ctx, &inst->operands[1], 17);
        int val_reg = operand_reg_or_imm_scratch(ctx, &inst->operands[2], 18);
        /* STRB Wval, [Xbase, Xidx] */
        uint32_t insn = 0x38206800U | ((idx_reg & 31) << 16) | ((base_reg & 31) << 5) | (val_reg & 31);
        { int __rc = emit32(&ctx->tb, insn); if (rspill) spill_store(dst, rspill); return __rc; }
    }

    case IR_OPCODE_GET_FIELD: {
        /* Struct base may be an immediate after copy propagation */
        src0 = operand_reg_or_imm_scratch(ctx, &inst->operands[0], 16);
        int field_idx = 0;
        if (inst->noperands > 1 && inst->operands[1].type == IR_OPERAND_IMM) {
            field_idx = (int)inst->operands[1].u.imm.u.s32;
        }
        int field_reg = src0 + field_idx;
        if (dst != field_reg) {
            emit_orr_reg(&ctx->tb, dst, 31, field_reg, 1);
        }
        break;
    }

    case IR_OPCODE_SET_FIELD: {
        src0 = operand_reg_or_imm_scratch(ctx, &inst->operands[0], 16);
        int field_idx = 0;
        if (inst->noperands > 1 && inst->operands[1].type == IR_OPERAND_IMM) {
            field_idx = (int)inst->operands[1].u.imm.u.s32;
        }
        int field_reg = src0 + field_idx;
        int val_reg = 31;
        if (inst->noperands > 2) {
            val_reg = operand_reg_or_imm_scratch(ctx, &inst->operands[2], 17);
        }
        { int __rc = emit_orr_reg(&ctx->tb, field_reg, 31, val_reg, 1); if (rspill) spill_store(dst, rspill); return __rc; }
    }

    case IR_OPCODE_MAKE_STRUCT: {
        if (inst->noperands > 0) {
            src0 = operand_reg_or_imm_scratch(ctx, &inst->operands[0], 16);
            if (dst != src0) {
                emit_orr_reg(&ctx->tb, dst, 31, src0, 1);
            }
        }
        break;
    }

    case IR_OPCODE_MAKE_ENUM: {
        /* MAKE_ENUM: result = variant_index [+ data values]
         * operands[0] = variant index (imm)
         * operands[1..N-1] = data values (for tuple variants)
         * operands[N-1] = enum name (str)
         * The variant index goes in the result register.
         * Data values go in scratch registers X16, X17 (not dst+1,
         * which may collide with other SSA values). */
        if (inst->noperands > 0) {
            /* Load variant index into result register */
            if (inst->operands[0].type == IR_OPERAND_IMM) {
                int64_t val = operand_imm(&inst->operands[0], &(int){1});
                emit_load_imm64(&ctx->tb, dst, val);
            } else {
                src0 = operand_reg_or_imm_scratch(ctx, &inst->operands[0], 16);
                emit_orr_reg(&ctx->tb, dst, 31, src0, 1);
            }
            /* Store data values in scratch registers (X16, X17) */
            int scratch_regs[] = {16, 17};
            for (int i = 1; i < inst->noperands - 1 && i - 1 < 2; i++) {
                int data_dst = scratch_regs[i - 1];
                if (inst->operands[i].type == IR_OPERAND_IMM) {
                    int64_t val = operand_imm(&inst->operands[i], &(int){1});
                    emit_load_imm64(&ctx->tb, data_dst, val);
                } else {
                    int src = operand_reg_or_imm_scratch(ctx, &inst->operands[i], 16);
                    emit_orr_reg(&ctx->tb, data_dst, 31, src, 1);
                }
            }
        }
        break;
    }

    case IR_OPCODE_CHECK_VARIANT: {
        /* CHECK_VARIANT: result = (enum_value == expected_index)
         * operands[0] = enum value, operands[1] = expected index (imm) */
        src0 = operand_reg_or_imm_scratch(ctx, &inst->operands[0], 16);
        src1 = operand_reg_or_imm_scratch(ctx, &inst->operands[1], 17);
        emit_cmp_reg(&ctx->tb, src0, src1, 1);
        { int __rc = emit_cset(&ctx->tb, dst, COND_EQ, 1); if (rspill) spill_store(dst, rspill); return __rc; }
    }

    case IR_OPCODE_EXTRACT_VARIANT: {
        /* EXTRACT_VARIANT: extract data field from a tuple variant
         * operands[0] = enum value (base register, holds variant index)
         * operands[1] = field index (which data value to extract)
         * Data was stored in X16/X17 by MAKE_ENUM. */
        int field_idx = 0;
        if (inst->noperands > 1 && inst->operands[1].type == IR_OPERAND_IMM) {
            field_idx = (int)inst->operands[1].u.imm.u.s32;
        }
        int scratch_regs[] = {16, 17};
        int data_reg = scratch_regs[field_idx % 2];
        if (dst != data_reg) {
            { int __rc = emit_orr_reg(&ctx->tb, dst, 31, data_reg, 1); if (rspill) spill_store(dst, rspill); return __rc; }
        }
        break;
    }

    /* Unhandled -- NOP */
    case IR_OPCODE_PHI:
    case IR_OPCODE_SWITCH:
    case IR_OPCODE_GET_ELEM: {
        /* GET_ELEM: dst = arr[idx]
         * LDR Xt, [Xbase, Xidx, LSL #3] — load 8 bytes from base + idx*8
         * Encoding: 1 11 1 1 0 0 1 0 1 1 Rm option S 10 Rn Rt
         * LDR (register): 0xF8606800 | (Rm << 16) | (Rn << 5) | Rt
         * option=011 (LSL), S=0, size=11 (64-bit) */
        src0 = operand_reg(&inst->operands[0]);
        int idx_reg = operand_reg_or_imm_scratch(ctx, &inst->operands[1], 16);
        uint32_t insn = 0xF8606800U | ((idx_reg & 31) << 16) | ((src0 & 31) << 5) | (dst & 31);
        if (rspill) spill_store(dst, rspill);
        return emit32(&ctx->tb, insn);
    }

    case IR_OPCODE_SET_ELEM: {
        /* SET_ELEM: arr[idx] = val
         * STR Xt, [Xbase, Xidx, LSL #3] */
        int base_reg = operand_reg(&inst->operands[0]);
        int idx_reg = operand_reg_or_imm_scratch(ctx, &inst->operands[1], 16);
        int val_reg = operand_reg_or_imm_scratch(ctx, &inst->operands[2], 17);
        uint32_t insn = 0xF8206800U | ((idx_reg & 31) << 16) | ((base_reg & 31) << 5) | (val_reg & 31);
        if (rspill) spill_store(dst, rspill);
        return emit32(&ctx->tb, insn);
    }

    case IR_OPCODE_MEMCPY:
    case IR_OPCODE_UREM:
    case IR_OPCODE_CAST:
    case IR_OPCODE_RECV:
    case IR_OPCODE_SEND:
    case IR_OPCODE_YIELD:
    case IR_OPCODE_AWAIT:
    case IR_OPCODE_SUSPEND:
        emit_nop(&ctx->tb); break;
    default:
        emit_nop(&ctx->tb); break;
    }
    if (rspill) spill_store(dst, rspill);
    return 0;
}

/*
 * aarch64_assemble -- assemble from DFIR to AArch64 machine code
 */
int
aarch64_assemble(ir_object_t *obj, arch_code_t *code)
{
    asm_ctx_t ctx;
    ir_func_t *func;

    if (!obj || !code) return -1;

    memset(&ctx, 0, sizeof(ctx));
    ctx.code = code;
    ctx.ir_obj = obj;
    g_tb = &ctx.tb;
    if (tb_init(&ctx.tb) < 0) return -1;

    /* Walk all functions */
    func = obj->funcs;
    while (func) {
        /* Resolve branch patches from the PREVIOUS function before clearing */
        resolve_patches(&ctx);
        /* Clear label map and branch patches for each function
         * (labels are function-scoped, not global) */
        for (int i = 0; i < ctx.labels.count; i++) free(ctx.labels.items[i].name);
        free(ctx.labels.items);
        ctx.labels.items = NULL;
        ctx.labels.count = 0;
        ctx.labels.cap = 0;
        for (int i = 0; i < ctx.patches.count; i++) free(ctx.patches.items[i].target);
        free(ctx.patches.items);
        ctx.patches.items = NULL;
        ctx.patches.count = 0;
        ctx.patches.cap = 0;

        off_t func_start = ctx.tb.size;
        int symidx = add_sym(code, ARCH_SYM_FUNC, func->name, func_start, 0);

        /* Compute max SSA id used in this function */
        int max_ssa = 0;
        for (size_t bi = 0; bi < func->nblocks; bi++) {
            ir_instr_ent_t *e = func->blocks[bi].instrs;
            while (e) {
                if (e->inst.result.n > 0 && e->inst.result.reg[0].id) {
                    int id = ssa_id(e->inst.result.reg[0].id);
                    if (id > max_ssa) max_ssa = id;
                }
                for (int j = 0; j < e->inst.noperands; j++) {
                    if (e->inst.operands[j].type == IR_OPERAND_REG &&
                        e->inst.operands[j].u.reg.id) {
                        int id = ssa_id(e->inst.operands[j].u.reg.id);
                        if (id > max_ssa) max_ssa = id;
                    }
                }
                e = e->next;
            }
        }

        /* Store max_ssa for RET epilogue */
        ctx.max_ssa = max_ssa;

        /* Compute spill area */
        int n_spill = (max_ssa >= AARCH64_MAX_REGS) ?
                      (max_ssa - AARCH64_MAX_REGS + 1) : 0;
        int csz = 80;  /* always save X19-X28 = 10 * 8 */
        /* Spill slots at [X29, -(16+csz+8)], [X29, -(16+csz+16)], ... */
        g_spill_off = -(csz + 8);

        /* Emit prologue */
        emit_prologue(&ctx, func->nargs, max_ssa);
        if (n_spill > 0) {
            int sz = ((n_spill * 8 + 15) / 16) * 16;
            uint32_t sub = (1U<<31)|(0x51U<<24)|((sz&0xFFF)<<10)|(31<<5)|31;
            emit32(&ctx.tb, sub);
        }

        for (size_t bi = 0; bi < func->nblocks; bi++) {
            ir_block_t *blk = &func->blocks[bi];

            /* Record block label -> text offset */
            if (blk->label && blk->label->name) {
                label_map_add(&ctx.labels, blk->label->name, ctx.tb.size);
            }

            /* Emit instructions */
            ir_instr_ent_t *ent = blk->instrs;
            while (ent) {
                if (compile_instr(&ctx, &ent->inst) < 0) {
                    free(ctx.tb.buf);
                    asm_ctx_free(&ctx);
                    return -1;
                }
                ent = ent->next;
            }
        }

        emit_epilogue(&ctx, max_ssa);

        code->sym.syms[symidx].size = ctx.tb.size - func_start;
        func = func->next;
    }

    /* Resolve branch patches for the last function */
    resolve_patches(&ctx);

    /* Append string data to text section */
    size_t cur_off = ctx.tb.size;
    for (int i = 0; i < ctx.code->strings.n; i++) {
        ctx.code->strings.items[i].offset = cur_off;
        size_t slen = strlen(ctx.code->strings.items[i].str) + 1;
        tb_emit(&ctx.tb, (uint8_t*)ctx.code->strings.items[i].str, slen);
        cur_off += slen;
    }

    /* Patch ADR instructions with string offsets */
    for (int i = 0; i < ctx.str_patches.count; i++) {
        size_t adr_off = ctx.str_patches.adr_off[i];
        int sid = ctx.str_patches.str_idx[i];
        size_t str_off = ctx.code->strings.items[sid].offset;
        int32_t disp = (int32_t)(str_off - adr_off);
        /* ADR: immlo (bits 30-29) and immhi (bits 23-5) */
        uint32_t adr;
        memcpy(&adr, ctx.tb.buf + adr_off, 4);
        int32_t imm = disp / 1;  /* ADR uses byte offset */
        uint32_t immlo = (imm & 3);
        uint32_t immhi = ((imm >> 2) & 0x7FFFF);
        adr = (1U << 28) | (immlo << 29) | (immhi << 5) | (adr & 0x1F);
        memcpy(ctx.tb.buf + adr_off, &adr, 4);
    }

    /* Emit global variable data into data section */
    for (size_t i = 0; i < obj->nglobals; i++) {
        size_t gpos = code->data.size;
        int64_t val = obj->globals[i].init_val;
        uint8_t *p = (uint8_t*)&val;
        for (int b = 0; b < 8; b++) {
            code->data.s = realloc(code->data.s, code->data.size + 1);
            code->data.s[code->data.size] = p[b];
            code->data.size++;
        }
        /* Update symbol position (relative to data section) */
        for (int s = 0; s < code->sym.n; s++) {
            if (code->sym.syms[s].label &&
                strcmp(code->sym.syms[s].label, obj->globals[i].name) == 0) {
                code->sym.syms[s].pos = gpos;
                code->sym.syms[s].size = 8;
                /* Keep as ARCH_SYM_GLOBAL -> goes to .data section */
                break;
            }
        }
    }

    /* Transfer to code */
    code->text.s = ctx.tb.buf;
    code->text.size = ctx.tb.size;
    code->cpu = ARCH_CPU_AARCH64;

    asm_ctx_free(&ctx);
    return 0;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
