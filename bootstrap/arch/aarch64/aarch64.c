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
#define AARCH64_MAX_REGS 29

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
    label_map_t labels;
    patch_list_t patches;
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
 * Map SSA id to AArch64 register (0-28).
 */
static int
ssa_to_reg(int id)
{
    if (id < 0 || id >= AARCH64_MAX_REGS) return 31;  /* XZR as fallback */
    return id;
}

/*
 * Get register from operand.
 */
static int
operand_reg(ir_operand_t *op)
{
    if (op->type == IR_OPERAND_REG) {
        return ssa_to_reg(ssa_id(op->u.reg.id));
    }
    return 31;  /* XZR */
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

/* Condition codes */
#define COND_EQ 0
#define COND_NE 1
#define COND_GE 10
#define COND_LT 11
#define COND_GT 12
#define COND_LE 13

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

static int
compile_instr(asm_ctx_t *ctx, ir_instr_t *inst)
{
    int dst, src0, src1;
    int ok;
    int64_t imm;

    dst = 31;  /* XZR default */
    if (inst->result.n > 0) {
        dst = ssa_to_reg(ssa_id(inst->result.reg[0].id));
    }

    switch (inst->opcode) {
    case IR_OPCODE_CONST:
        imm = operand_imm(&inst->operands[0], &ok);
        if (!ok) return -1;
        return emit_load_imm64(&ctx->tb, dst, imm);

    case IR_OPCODE_MOV:
        /* operands[0] = source, operands[1] = destination */
        src0 = operand_reg(&inst->operands[0]);
        dst = operand_reg(&inst->operands[1]);
        return emit_orr_reg(&ctx->tb, dst, 31, src0, 1);  /* mov = orr rd, xzr, rm */

    case IR_OPCODE_ADD:
        src0 = operand_reg(&inst->operands[0]);
        src1 = operand_reg(&inst->operands[1]);
        return emit_add_reg(&ctx->tb, dst, src0, src1, 1);

    case IR_OPCODE_SUB:
        src0 = operand_reg(&inst->operands[0]);
        src1 = operand_reg(&inst->operands[1]);
        return emit_sub_reg(&ctx->tb, dst, src0, src1, 1);

    case IR_OPCODE_MUL:
        src0 = operand_reg(&inst->operands[0]);
        src1 = operand_reg(&inst->operands[1]);
        return emit_mul_reg(&ctx->tb, dst, src0, src1, 1);

    case IR_OPCODE_NEG:
        src0 = operand_reg(&inst->operands[0]);
        return emit_sub_reg(&ctx->tb, dst, 31, src0, 1);  /* sub rd, xzr, src0 */

    case IR_OPCODE_NOT:
        src0 = operand_reg(&inst->operands[0]);
        return emit_orn_reg(&ctx->tb, dst, 31, src0, 1);  /* orn rd, xzr, src0 */

    case IR_OPCODE_AND:
        src0 = operand_reg(&inst->operands[0]);
        src1 = operand_reg(&inst->operands[1]);
        return emit_and_reg(&ctx->tb, dst, src0, src1, 1);

    case IR_OPCODE_OR:
        src0 = operand_reg(&inst->operands[0]);
        src1 = operand_reg(&inst->operands[1]);
        return emit_orr_reg(&ctx->tb, dst, src0, src1, 1);

    case IR_OPCODE_XOR:
        src0 = operand_reg(&inst->operands[0]);
        src1 = operand_reg(&inst->operands[1]);
        return emit_eor_reg(&ctx->tb, dst, src0, src1, 1);

    case IR_OPCODE_CMP_EQ:
        src0 = operand_reg(&inst->operands[0]);
        src1 = operand_reg(&inst->operands[1]);
        emit_cmp_reg(&ctx->tb, src0, src1, 1);
        return emit_cset(&ctx->tb, dst, COND_EQ, 1);

    case IR_OPCODE_CMP_NE:
        src0 = operand_reg(&inst->operands[0]);
        src1 = operand_reg(&inst->operands[1]);
        emit_cmp_reg(&ctx->tb, src0, src1, 1);
        return emit_cset(&ctx->tb, dst, COND_NE, 1);

    case IR_OPCODE_CMP_LT:
        src0 = operand_reg(&inst->operands[0]);
        src1 = operand_reg(&inst->operands[1]);
        emit_cmp_reg(&ctx->tb, src0, src1, 1);
        return emit_cset(&ctx->tb, dst, COND_LT, 1);

    case IR_OPCODE_CMP_LE:
        src0 = operand_reg(&inst->operands[0]);
        src1 = operand_reg(&inst->operands[1]);
        emit_cmp_reg(&ctx->tb, src0, src1, 1);
        return emit_cset(&ctx->tb, dst, COND_LE, 1);

    case IR_OPCODE_CMP_GT:
        src0 = operand_reg(&inst->operands[0]);
        src1 = operand_reg(&inst->operands[1]);
        emit_cmp_reg(&ctx->tb, src0, src1, 1);
        return emit_cset(&ctx->tb, dst, COND_GT, 1);

    case IR_OPCODE_CMP_GE:
        src0 = operand_reg(&inst->operands[0]);
        src1 = operand_reg(&inst->operands[1]);
        emit_cmp_reg(&ctx->tb, src0, src1, 1);
        return emit_cset(&ctx->tb, dst, COND_GE, 1);

    case IR_OPCODE_BR: {
        /* b label — emit placeholder, save patch for resolution */
        size_t off = ctx->tb.size;
        emit_b(&ctx->tb, 0);
        if (inst->noperands > 0 && inst->operands[0].type == IR_OPERAND_LABEL) {
            patch_list_add(&ctx->patches, off, inst->operands[0].u.label, -1);
        }
        return 0;
    }

    case IR_OPCODE_BR_COND: {
        /* CBZ Xt, $else — if cond==0, branch to else; then falls through */
        src0 = operand_reg(&inst->operands[0]);
        size_t off = ctx->tb.size;
        /* CBZ: sf 011010 0 imm19 Rt = (1<<31)|(0x34<<24)|(imm19<<5)|Rt */
        uint32_t cbz = (1U << 31) | (0x34U << 24) | (src0 & 31);
        emit32(&ctx->tb, cbz);
        /* Else label is in operands[2] */
        if (inst->noperands > 2 && inst->operands[2].type == IR_OPERAND_LABEL) {
            patch_list_add(&ctx->patches, off, inst->operands[2].u.label, src0);
        }
        return 0;
    }

    case IR_OPCODE_RET:
        return emit_ret(&ctx->tb);

    case IR_OPCODE_CALL:
        {
            const char *callee = NULL;
            if (inst->operands[0].type == IR_OPERAND_IMM &&
                inst->operands[0].u.imm.type == IR_IMM_STR) {
                callee = inst->operands[0].u.imm.u.str;
            }
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
            off_t relpos = ctx->tb.size;
            emit_bl(&ctx->tb, 0);  /* placeholder */
            if (symidx >= 0) {
                add_rel(ctx->code, ARCH_REL_BRANCH, relpos, symidx);
            }
            return 0;
        }

    case IR_OPCODE_ALLOCA:
        /* sub sp, sp, #16 (approximate) */
        /* SUB (immediate): sf 1 0 0 1 0 0 0 1 0 0 sh imm12 Rn Rd */
        {
            uint32_t insn = (1U << 31) | (0x22U << 24) | (16 << 10) |
                            (31U << 5) | 31;
            return emit32(&ctx->tb, insn);
        }

    case IR_OPCODE_LOAD:
        /* LDR Xt, [Xn] — 1 1 1 1 1 0 0 1 0 1 imm12 Rn Rt (imm12=0) */
        src0 = operand_reg(&inst->operands[0]);
        {
            uint32_t insn = (0xF9U << 24) | ((src0 & 31) << 5) | (dst & 31);
            return emit32(&ctx->tb, insn);
        }

    case IR_OPCODE_STORE:
        /* STR Xt, [Xn] — 1 1 1 1 1 0 0 1 0 0 imm12 Rn Rt */
        src0 = operand_reg(&inst->operands[0]);
        src1 = operand_reg(&inst->operands[1]);
        {
            uint32_t insn = (0xF8U << 24) | ((src0 & 31) << 5) | (src1 & 31);
            return emit32(&ctx->tb, insn);
        }

    /* Unhandled — NOP */
    case IR_OPCODE_PHI:
    case IR_OPCODE_SWITCH:
    case IR_OPCODE_MEMCPY:
    case IR_OPCODE_UDIV:
    case IR_OPCODE_MOD:
    case IR_OPCODE_UREM:
    case IR_OPCODE_SHL:
    case IR_OPCODE_SHR:
    case IR_OPCODE_CAST:
    case IR_OPCODE_MAKE_STRUCT:
    case IR_OPCODE_GET_FIELD:
    case IR_OPCODE_SET_FIELD:
    case IR_OPCODE_GET_ELEM:
    case IR_OPCODE_MAKE_ENUM:
    case IR_OPCODE_EXTRACT_VARIANT:
    case IR_OPCODE_CHECK_VARIANT:
    case IR_OPCODE_RECV:
    case IR_OPCODE_SEND:
    case IR_OPCODE_YIELD:
    case IR_OPCODE_AWAIT:
    case IR_OPCODE_SUSPEND:
        return emit_nop(&ctx->tb);

    default:
        return emit_nop(&ctx->tb);
    }
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
    if (tb_init(&ctx.tb) < 0) return -1;

    /* Walk all functions */
    func = obj->funcs;
    while (func) {
        off_t func_start = ctx.tb.size;
        int symidx = add_sym(code, ARCH_SYM_FUNC, func->name, func_start, 0);

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

        code->sym.syms[symidx].size = ctx.tb.size - func_start;
        func = func->next;
    }

    /* Resolve branch patches */
    resolve_patches(&ctx);

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
