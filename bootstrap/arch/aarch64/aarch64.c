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
compile_instr(textbuf_t *tb, arch_code_t *code, ir_instr_t *inst)
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
        return emit_load_imm64(tb, dst, imm);

    case IR_OPCODE_MOV:
        /* operands[0] = source, operands[1] = destination */
        src0 = operand_reg(&inst->operands[0]);
        dst = operand_reg(&inst->operands[1]);
        return emit_orr_reg(tb, dst, 31, src0, 1);  /* mov = orr rd, xzr, rm */

    case IR_OPCODE_ADD:
        src0 = operand_reg(&inst->operands[0]);
        src1 = operand_reg(&inst->operands[1]);
        return emit_add_reg(tb, dst, src0, src1, 1);

    case IR_OPCODE_SUB:
        src0 = operand_reg(&inst->operands[0]);
        src1 = operand_reg(&inst->operands[1]);
        return emit_sub_reg(tb, dst, src0, src1, 1);

    case IR_OPCODE_MUL:
        src0 = operand_reg(&inst->operands[0]);
        src1 = operand_reg(&inst->operands[1]);
        return emit_mul_reg(tb, dst, src0, src1, 1);

    case IR_OPCODE_NEG:
        src0 = operand_reg(&inst->operands[0]);
        return emit_sub_reg(tb, dst, 31, src0, 1);  /* sub rd, xzr, src0 */

    case IR_OPCODE_NOT:
        src0 = operand_reg(&inst->operands[0]);
        return emit_orn_reg(tb, dst, 31, src0, 1);  /* orn rd, xzr, src0 */

    case IR_OPCODE_AND:
        src0 = operand_reg(&inst->operands[0]);
        src1 = operand_reg(&inst->operands[1]);
        return emit_and_reg(tb, dst, src0, src1, 1);

    case IR_OPCODE_OR:
        src0 = operand_reg(&inst->operands[0]);
        src1 = operand_reg(&inst->operands[1]);
        return emit_orr_reg(tb, dst, src0, src1, 1);

    case IR_OPCODE_XOR:
        src0 = operand_reg(&inst->operands[0]);
        src1 = operand_reg(&inst->operands[1]);
        return emit_eor_reg(tb, dst, src0, src1, 1);

    case IR_OPCODE_CMP_EQ:
        src0 = operand_reg(&inst->operands[0]);
        src1 = operand_reg(&inst->operands[1]);
        emit_cmp_reg(tb, src0, src1, 1);
        return emit_cset(tb, dst, COND_EQ, 1);

    case IR_OPCODE_CMP_NE:
        src0 = operand_reg(&inst->operands[0]);
        src1 = operand_reg(&inst->operands[1]);
        emit_cmp_reg(tb, src0, src1, 1);
        return emit_cset(tb, dst, COND_NE, 1);

    case IR_OPCODE_CMP_LT:
        src0 = operand_reg(&inst->operands[0]);
        src1 = operand_reg(&inst->operands[1]);
        emit_cmp_reg(tb, src0, src1, 1);
        return emit_cset(tb, dst, COND_LT, 1);

    case IR_OPCODE_CMP_LE:
        src0 = operand_reg(&inst->operands[0]);
        src1 = operand_reg(&inst->operands[1]);
        emit_cmp_reg(tb, src0, src1, 1);
        return emit_cset(tb, dst, COND_LE, 1);

    case IR_OPCODE_CMP_GT:
        src0 = operand_reg(&inst->operands[0]);
        src1 = operand_reg(&inst->operands[1]);
        emit_cmp_reg(tb, src0, src1, 1);
        return emit_cset(tb, dst, COND_GT, 1);

    case IR_OPCODE_CMP_GE:
        src0 = operand_reg(&inst->operands[0]);
        src1 = operand_reg(&inst->operands[1]);
        emit_cmp_reg(tb, src0, src1, 1);
        return emit_cset(tb, dst, COND_GE, 1);

    case IR_OPCODE_BR:
        /* b label — placeholder offset 0, needs relocation */
        return emit_b(tb, 0);

    case IR_OPCODE_BR_COND:
        /* cbz/cbnz or: cmp + b.cond + b */
        src0 = operand_reg(&inst->operands[0]);
        emit_cmp_reg(tb, src0, 31, 1);  /* cmp src0, xzr */
        emit_bcond(tb, COND_NE, 8);     /* b.ne then (skip next) */
        emit_b(tb, 8);                  /* b else (skip 2 instructions) */
        return 0;

    case IR_OPCODE_RET:
        return emit_ret(tb);

    case IR_OPCODE_CALL:
        {
            const char *callee = NULL;
            if (inst->operands[0].type == IR_OPERAND_IMM &&
                inst->operands[0].u.imm.type == IR_IMM_STR) {
                callee = inst->operands[0].u.imm.u.str;
            }
            int symidx = -1;
            if (callee) {
                for (int i = 0; i < code->sym.n; i++) {
                    if (code->sym.syms[i].label &&
                        strcmp(code->sym.syms[i].label, callee) == 0) {
                        symidx = i;
                        break;
                    }
                }
                if (symidx < 0) {
                    symidx = add_sym(code, ARCH_SYM_FUNC, callee, 0, 0);
                }
            }
            off_t relpos = tb->size;
            emit_bl(tb, 0);  /* placeholder */
            if (symidx >= 0) {
                add_rel(code, ARCH_REL_BRANCH, relpos, symidx);
            }
            return 0;
        }

    case IR_OPCODE_ALLOCA:
        /* sub sp, sp, #16 (approximate) */
        /* SUB (immediate): sf 1 0 0 1 0 0 0 1 0 0 sh imm12 Rn Rd */
        {
            uint32_t insn = (1U << 31) | (0x22U << 24) | (16 << 10) |
                            (31U << 5) | 31;
            return emit32(tb, insn);
        }

    case IR_OPCODE_LOAD:
        /* LDR Xt, [Xn] — 1 1 1 1 1 0 0 1 0 1 imm12 Rn Rt (imm12=0) */
        src0 = operand_reg(&inst->operands[0]);
        {
            uint32_t insn = (0xF9U << 24) | ((src0 & 31) << 5) | (dst & 31);
            return emit32(tb, insn);
        }

    case IR_OPCODE_STORE:
        /* STR Xt, [Xn] — 1 1 1 1 1 0 0 1 0 0 imm12 Rn Rt */
        src0 = operand_reg(&inst->operands[0]);
        src1 = operand_reg(&inst->operands[1]);
        {
            uint32_t insn = (0xF8U << 24) | ((src0 & 31) << 5) | (src1 & 31);
            return emit32(tb, insn);
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
        return emit_nop(tb);

    default:
        return emit_nop(tb);
    }
}

/*
 * aarch64_assemble -- assemble from DFIR to AArch64 machine code
 */
int
aarch64_assemble(ir_object_t *obj, arch_code_t *code)
{
    textbuf_t tb;
    ir_func_t *func;

    if (!obj || !code) return -1;

    if (tb_init(&tb) < 0) return -1;

    /* Walk all functions */
    func = obj->funcs;
    while (func) {
        off_t func_start = tb.size;
        int symidx = add_sym(code, ARCH_SYM_FUNC, func->name, func_start, 0);

        for (size_t bi = 0; bi < func->nblocks; bi++) {
            ir_block_t *blk = &func->blocks[bi];
            ir_instr_ent_t *ent = blk->instrs;
            while (ent) {
                if (compile_instr(&tb, code, &ent->inst) < 0) {
                    free(tb.buf);
                    return -1;
                }
                ent = ent->next;
            }
        }

        code->sym.syms[symidx].size = tb.size - func_start;
        func = func->next;
    }

    code->text.s = tb.buf;
    code->text.size = tb.size;
    code->cpu = ARCH_CPU_AARCH64;

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
