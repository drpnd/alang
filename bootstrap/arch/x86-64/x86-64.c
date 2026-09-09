/*_
 * Copyright (c) 2020-2026 Hirochika Asai <asai@jar.jp>
 * All rights reserved.
 * MIT License
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "reg.h"
#include "instr.h"
#include "../../arch.h"

/*     return (1<<6) | (w<<3) | (r<<2) | (x<<1) | b; */
#define REX             (1<<6)
#define REX_W           (1<<3)
#define REX_R           (1<<2)
#define REX_X           (1<<1)
#define REX_B           (1)

void *x86_64_initialize(void *);
int x86_64_load_instr(void *);

/*
 * Encode ModR/M
 */
static int
_modrm(int reg, int mod, int rm)
{
    if ( reg < 0 || reg > 7 || mod < 0 || mod > 3 || rm < 0 || rm > 7 ) {
        return -1;
    }
    return (mod << 6) | (reg << 3) | rm;
}

/*
 * Encode SIB
 */
static int
_sib(int base, int idx, int ss)
{
    if ( base < 0 || base > 7 || idx < 0 || idx > 7 || ss < 0 || ss > 3 ) {
        return -1;
    }
    return (ss << 6) | (idx << 3) | base;
}

/*
 * REX prefix
 */
static int
_rex(int rex, x86_64_reg_t r, x86_64_reg_t rmbase, x86_64_reg_t s)
{
    rex |= REG_REX(r) ? REX_R : 0;
    rex |= REG_REX(s) ? REX_X : 0;
    rex |= REG_REX(rmbase) ? REX_B : 0;

    if ( rex ) {
        if ( REG_NE(r) || REG_NE(s) || REG_NE(rmbase) ) {
            return -1;
        }
        rex |= REX;
    }
    return rex;
}

/*
 * Encode ModR/M without SIB
 */
static int
_encode_modrm(uint8_t *code, int *rex, int mod, x86_64_reg_t reg,
              x86_64_reg_t rm)
{
    int ret, modrm;
    ret = _rex(*rex, reg, rm, REG_NONE);
    if ( ret < 0 ) return -1;
    modrm = _modrm(REG_CODE(reg), mod, REG_CODE(rm));
    if ( modrm < 0 ) return -1;
    *code = modrm;
    *rex = ret;
    return 1;
}

/*
 * Encode ModR/M with SIB
 */
static int
_encode_modrm_sib(uint8_t *code, int *rex, int mod, x86_64_reg_t reg,
                  x86_64_reg_t base, x86_64_reg_t idx, int ss)
{
    int ret, modrm, sib;
    ret = _rex(*rex, reg, base, idx);
    if ( ret < 0 ) return -1;
    modrm = _modrm(REG_CODE(reg), mod, 4);
    if ( modrm < 0 ) return -1;
    sib = _sib(REG_CODE(base), REG_CODE(idx), ss);
    if ( sib < 0 ) return -1;
    code[0] = modrm;
    code[1] = sib;
    *rex = ret;
    return 2;
}

/*
 * _encode_rm_reg -- RR
 */
static int
_encode_rm_reg(uint8_t *code, int *rex, x86_64_operand_t op1,
               x86_64_operand_t op2)
{
    if ( op1.type != X86_64_OPERAND_REG || op2.type != X86_64_OPERAND_REG ) {
        return -1;
    }
    return _encode_modrm(code, rex, 3, op1.u.reg, op2.u.reg);
}

/*
 * _encode_rm_mem -- Encode memory operand
 */
static int
_encode_rm_mem(uint8_t *code, int *rex, x86_64_operand_t op1,
               x86_64_operand_t op2)
{
    int size, ss, mod, ret;

    if ( op1.type != X86_64_OPERAND_REG || op2.type != X86_64_OPERAND_MEM ) {
        return -1;
    }
    switch ( op2.u.mem.scale ) {
    case 1: ss = 0; break;
    case 2: ss = 1; break;
    case 4: ss = 2; break;
    case 8: ss = 3; break;
    default: return -1;
    }
    if ( op2.u.mem.disp == 0 ) {
        mod = 0;
    } else if ( op2.u.mem.disp <= 0x7f && op2.u.mem.disp >= -0x80 ) {
        mod = 1;
    } else {
        mod = 2;
    }
    if ( (op2.u.mem.sindex != REG_NONE && op2.u.mem.base != REG_NONE) || ss )  {
        ret = _encode_modrm_sib(code, rex, mod, op1.u.reg,
                                op2.u.mem.base, op2.u.mem.sindex, ss);
        if ( ret < 0 ) return -1;
        size = ret;
    } else if ( op2.u.mem.sindex == REG_NONE && op2.u.mem.base == REG_NONE ) {
        ret = _encode_modrm(code, rex, op1.u.reg, REG_NONE, 5);
        if ( ret < 0 ) return -1;
        size = ret;
        mod = 2;
    } else {
        ret = _encode_modrm(code, rex, mod, op1.u.reg, op2.u.mem.sindex);
        if ( ret < 0 ) return -1;
        size = ret;
    }
    switch ( mod ) {
    case 1:
        memcpy(code + size, &op2.u.mem.disp, 1);
        size += 1;
        break;
    case 2:
        memcpy(code + size, &op2.u.mem.disp, 4);
        size += 4;
        break;
    default:
        break;
    }
    return size;
}

/*
 * _encode_rm -- RM operand encoding
 */
static int
_encode_rm(uint8_t *code, int *rex, x86_64_operand_t op1,
           x86_64_operand_t op2)
{
    if ( op1.type != X86_64_OPERAND_REG ) return -1;
    if ( op2.type != X86_64_OPERAND_REG && op2.type != X86_64_OPERAND_MEM ) {
        return -1;
    }
    if ( op2.type == X86_64_OPERAND_REG ) {
        return _encode_rm_reg(code, rex, op1, op2);
    }
    return _encode_rm_mem(code, rex, op1, op2);
}

/*
 * Temporary test function
 */
int
x86_64_test(uint8_t *code)
{
    int rex, ret;
    x86_64_operand_t op1 = { .type = X86_64_OPERAND_REG, .u.reg = REG_RAX };
    x86_64_operand_t op2 = { .type = X86_64_OPERAND_REG, .u.reg = REG_RDI };

    rex = REX_W;
    ret = _encode_rm(code + 2, &rex, op1, op2);
    if ( ret < 0 ) return -1;
    code[0] = rex;
    code[1] = 0x89;
    return ret + 2;
}

/*======================================================================
 * DFIR to x86-64 code generation
 *======================================================================*/

/*
 * Text buffer (dynamically growing)
 */
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
tb_byte(textbuf_t *tb, uint8_t b)
{
    return tb_emit(tb, &b, 1);
}

static int
tb_u32(textbuf_t *tb, uint32_t val)
{
    return tb_emit(tb, (uint8_t*)&val, 4);
}

/*
 * SSA register allocator
 * Maps SSA value IDs ("%0", "%1", ...) to x86-64 registers.
 * Uses first 14 GP registers; overflow goes to stack.
 */
#define X86_MAX_REGS 14
static const x86_64_reg_t ssa_regs[X86_MAX_REGS] = {
    REG_RAX, REG_RCX, REG_RDX, REG_RSI, REG_RDI,
    REG_R8,  REG_R9,  REG_R10, REG_R11, REG_R12,
    REG_R13, REG_R14, REG_R15, REG_RBX
};

/*
 * Parse SSA id string "%N" to integer N.
 * Returns -1 if invalid.
 */
static int
ssa_id(const char *id)
{
    if (!id || id[0] != '%') return -1;
    return atoi(id + 1);
}

/*
 * Map SSA id to x86-64 register.
 * Returns REG_NONE if out of range (should use stack).
 */
static x86_64_reg_t
ssa_to_reg(int id)
{
    if (id < 0 || id >= X86_MAX_REGS) return REG_NONE;
    return ssa_regs[id];
}

/*
 * Emit REX + opcode + ModR/M for reg-reg instruction.
 * E.g: add dst, src => REX.W + 0x01 + ModR/M(src, dst)
 */
static int
emit_rr(textbuf_t *tb, uint8_t opcode, x86_64_reg_t dst, x86_64_reg_t src,
        int rex_w)
{
    int rex = rex_w ? REX_W : 0;
    uint8_t buf[4];
    x86_64_operand_t op1 = { .type = X86_64_OPERAND_REG, .u.reg = src };
    x86_64_operand_t op2 = { .type = X86_64_OPERAND_REG, .u.reg = dst };
    int n = _encode_rm_reg(buf + 1, &rex, op1, op2);
    if (n < 0) return -1;
    buf[0] = rex;
    buf[1 + n] = opcode;
    /* Layout: REX, opcode, ModR/M -- but x86 order is REX, opcode, modrm */
    /* Actually: REX prefix, then opcode, then ModR/M */
    uint8_t out[4];
    int pos = 0;
    if (rex) out[pos++] = rex;
    out[pos++] = opcode;
    out[pos++] = buf[1]; /* ModR/M byte */
    return tb_emit(tb, out, pos);
}

/*
 * Emit REX + opcode + ModR/M for reg-imm instruction.
 * Uses 0x81 /0 for add, /1 for or, /4 for and, /5 for sub, /6 for xor
 */
static int
__attribute__((unused))
emit_ri(textbuf_t *tb, uint8_t subopcode, x86_64_reg_t reg, int32_t imm,
        int rex_w)
{
    int rex = rex_w ? REX_W : 0;
    uint8_t out[8];
    int pos = 0;
    int modrm;

    /* Encode register in ModR/M with mod=3 */
    rex |= REG_REX(reg) ? REX_B : 0;
    modrm = _modrm(subopcode, 3, REG_CODE(reg));
    if (modrm < 0) return -1;

    if (rex) out[pos++] = rex | (rex ? REX : 0);
    out[pos++] = 0x81;
    out[pos++] = modrm;
    memcpy(out + pos, &imm, 4);
    pos += 4;

    return tb_emit(tb, out, pos);
}

/*
 * Emit mov reg, imm64 (or imm32 zero-extended)
 */
static int
emit_mov_imm(textbuf_t *tb, x86_64_reg_t reg, int64_t val)
{
    uint8_t out[10];
    int pos = 0;
    int rex;

    if (val >= INT32_MIN && val <= INT32_MAX) {
        /* mov r/m64, imm32 (0xC7 /0) — sign-extends to 64-bit */
        rex = REX_W | (REG_REX(reg) ? REX_B : 0) | REX;
        out[pos++] = rex;
        out[pos++] = 0xC7;
        out[pos++] = _modrm(0, 3, REG_CODE(reg));
        int32_t v32 = (int32_t)val;
        memcpy(out + pos, &v32, 4);
        pos += 4;
    } else {
        /* mov r64, imm64 (REX.W + 0xB8+r) */
        rex = REX_W | (REG_REX(reg) ? REX_B : 0) | REX;
        out[pos++] = rex;
        out[pos++] = 0xB8 + REG_CODE(reg);
        memcpy(out + pos, &val, 8);
        pos += 8;
    }
    return tb_emit(tb, out, pos);
}

/*
 * Emit mov reg, reg (0x89 /r)
 */
static int
emit_mov_rr(textbuf_t *tb, x86_64_reg_t dst, x86_64_reg_t src)
{
    return emit_rr(tb, 0x89, dst, src, 1);
}

/*
 * Emit ret
 */
static int
emit_ret(textbuf_t *tb)
{
    return tb_byte(tb, 0xC3);
}

/*
 * Emit call (PC-relative)
 * E8 cd — call rel32
 * Returns offset of the rel32 for relocation.
 */
static int
emit_call(textbuf_t *tb, size_t *reloff)
{
    if (tb_byte(tb, 0xE8) < 0) return -1;
    *reloff = tb->size;
    return tb_u32(tb, 0);  /* placeholder, filled by relocation */
}

/*
 * Emit jmp (PC-relative)
 * E9 cd — jmp rel32
 */
static int
emit_jmp(textbuf_t *tb, size_t *reloff)
{
    if (tb_byte(tb, 0xE9) < 0) return -1;
    *reloff = tb->size;
    return tb_u32(tb, 0);
}

/*
 * Emit conditional jump
 * 0F 8x cd — jcc rel32
 */
static int
emit_jcc(textbuf_t *tb, uint8_t cc, size_t *reloff)
{
    if (tb_byte(tb, 0x0F) < 0) return -1;
    if (tb_byte(tb, 0x80 | cc) < 0) return -1;
    *reloff = tb->size;
    return tb_u32(tb, 0);
}

/* Condition codes */
#define JCC_EQ  0x04
#define JCC_NE  0x05
#define JCC_LT  0x0C
#define JCC_LE  0x0E
#define JCC_GT  0x0F
#define JCC_GE  0x0D

/*
 * Emit cmp reg, reg (0x39 /r)
 */
static int
emit_cmp_rr(textbuf_t *tb, x86_64_reg_t a, x86_64_reg_t b)
{
    return emit_rr(tb, 0x39, a, b, 1);
}

/*
 * Emit setcc + movzx to materialize a boolean.
 * 0F 9x /r — setcc r/m8
 * 0F B6 /r — movzx r32, r/m8
 */
static int
emit_setcc_movzx(textbuf_t *tb, uint8_t cc, x86_64_reg_t dst)
{
    int rex;
    uint8_t buf[8];
    int pos = 0;

    /* setcc dst_b8 */
    rex = REX | (REG_REX(dst) ? REX_B : 0);
    buf[pos++] = rex;
    buf[pos++] = 0x0F;
    buf[pos++] = 0x90 | cc;
    buf[pos++] = _modrm(0, 3, REG_CODE(dst));

    /* movzx dst_d, dst_b8 (zero-extend) */
    /* Use 0x0F B6 /r with REX to force 64-bit result */
    buf[pos++] = REX | REX_W | (REG_REX(dst) ? (REX_R | REX_B) : 0);
    buf[pos++] = 0x0F;
    buf[pos++] = 0xB6;
    buf[pos++] = _modrm(REG_CODE(dst), 3, REG_CODE(dst));

    (void)rex;
    return tb_emit(tb, buf, pos);
}

/*
 * Emit sub rsp, imm (stack allocation)
 */
static int
emit_sub_rsp(textbuf_t *tb, int32_t size)
{
    /* REX.W + 0x81 /5 (SUB r/m64, imm32) with RSP */
    uint8_t buf[8];
    int pos = 0;
    buf[pos++] = REX | REX_W;
    buf[pos++] = 0x81;
    buf[pos++] = _modrm(5, 3, REG_CODE(REG_RSP));
    memcpy(buf + pos, &size, 4);
    pos += 4;
    return tb_emit(tb, buf, pos);
}

/*
 * Symbol table builder
 */
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
    return code->sym.n - 1;  /* return index */
}

/*
 * Relocation builder
 */
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

/*
 * Get the register from an operand (assuming it's a register operand).
 */
static x86_64_reg_t
operand_reg(ir_operand_t *op)
{
    if (op->type == IR_OPERAND_REG) {
        int id = ssa_id(op->u.reg.id);
        return ssa_to_reg(id);
    }
    return REG_NONE;
}

/*
 * Get immediate value from an operand.
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

/*
 * Compile a single DFIR instruction to x86-64 machine code.
 */
static int
compile_instr(textbuf_t *tb, arch_code_t *code, ir_instr_t *inst)
{
    x86_64_reg_t dst, src0, src1;
    int ok;
    int64_t imm;
    size_t reloff;

    dst = REG_NONE;
    if (inst->result.n > 0) {
        dst = ssa_to_reg(ssa_id(inst->result.reg[0].id));
    }

    switch (inst->opcode) {
    case IR_OPCODE_CONST:
        imm = operand_imm(&inst->operands[0], &ok);
        if (!ok) return -1;
        return emit_mov_imm(tb, dst, imm);

    case IR_OPCODE_MOV:
        /* operands[0] = source, operands[1] = destination */
        src0 = operand_reg(&inst->operands[0]);
        dst = operand_reg(&inst->operands[1]);
        return emit_mov_rr(tb, dst, src0);

    case IR_OPCODE_ADD:
        src0 = operand_reg(&inst->operands[0]);
        src1 = operand_reg(&inst->operands[1]);
        /* add src0, src1 => result in src0, then mov to dst if different */
        emit_rr(tb, 0x01, src0, src1, 1);  /* add src0, src1 */
        if (dst != src0) emit_mov_rr(tb, dst, src0);
        return 0;

    case IR_OPCODE_SUB:
        src0 = operand_reg(&inst->operands[0]);
        src1 = operand_reg(&inst->operands[1]);
        emit_rr(tb, 0x29, src0, src1, 1);
        if (dst != src0) emit_mov_rr(tb, dst, src0);
        return 0;

    case IR_OPCODE_MUL:
        src0 = operand_reg(&inst->operands[0]);
        src1 = operand_reg(&inst->operands[1]);
        /* imul dst, src0, src1 — REX.W 0x0F AF /r */
        {
            uint8_t buf[4];
            int pos = 0;
            int rex = REX_W;
            int modrm = _modrm(REG_CODE(src0), 3, REG_CODE(dst));
            rex |= REG_REX(dst) ? REX_R : 0;
            rex |= REG_REX(src0) ? REX_B : 0;
            if (rex) buf[pos++] = rex | REX;
            buf[pos++] = 0x0F;
            buf[pos++] = 0xAF;
            buf[pos++] = modrm;
            (void)src1;  /* imul only uses dst and src0 in this encoding */
            /* Actually: imul dst, src0 — but we need src1 too */
            /* Use: mov dst, src0; imul dst, src1 */
            emit_mov_rr(tb, dst, src0);
            rex = REX_W;
            modrm = _modrm(REG_CODE(dst), 3, REG_CODE(src1));
            rex |= REG_REX(dst) ? REX_R : 0;
            rex |= REG_REX(src1) ? REX_B : 0;
            pos = 0;
            if (rex) buf[pos++] = rex | REX;
            buf[pos++] = 0x0F;
            buf[pos++] = 0xAF;
            buf[pos++] = modrm;
            return tb_emit(tb, buf, pos);
        }

    case IR_OPCODE_NEG:
        src0 = operand_reg(&inst->operands[0]);
        /* neg r/m64 — F7 /3 */
        {
            uint8_t buf[4];
            int pos = 0;
            int rex = REX_W | (REG_REX(src0) ? REX_B : 0) | REX;
            int modrm = _modrm(3, 3, REG_CODE(src0));
            buf[pos++] = rex;
            buf[pos++] = 0xF7;
            buf[pos++] = modrm;
            if (dst != src0) emit_mov_rr(tb, dst, src0);
            return tb_emit(tb, buf, pos);
        }

    case IR_OPCODE_NOT:
        src0 = operand_reg(&inst->operands[0]);
        /* not r/m64 — F7 /2 */
        {
            uint8_t buf[4];
            int pos = 0;
            int rex = REX_W | (REG_REX(src0) ? REX_B : 0) | REX;
            int modrm = _modrm(2, 3, REG_CODE(src0));
            buf[pos++] = rex;
            buf[pos++] = 0xF7;
            buf[pos++] = modrm;
            if (dst != src0) emit_mov_rr(tb, dst, src0);
            return tb_emit(tb, buf, pos);
        }

    case IR_OPCODE_AND:
        src0 = operand_reg(&inst->operands[0]);
        src1 = operand_reg(&inst->operands[1]);
        emit_rr(tb, 0x21, src0, src1, 1);
        if (dst != src0) emit_mov_rr(tb, dst, src0);
        return 0;

    case IR_OPCODE_OR:
        src0 = operand_reg(&inst->operands[0]);
        src1 = operand_reg(&inst->operands[1]);
        emit_rr(tb, 0x09, src0, src1, 1);
        if (dst != src0) emit_mov_rr(tb, dst, src0);
        return 0;

    case IR_OPCODE_XOR:
        src0 = operand_reg(&inst->operands[0]);
        src1 = operand_reg(&inst->operands[1]);
        emit_rr(tb, 0x31, src0, src1, 1);
        if (dst != src0) emit_mov_rr(tb, dst, src0);
        return 0;

    case IR_OPCODE_SHL:
        src0 = operand_reg(&inst->operands[0]);
        /* shl r/m, cl — D3 /4 */
        {
            /* mov dst, src0; shl dst, cl */
            emit_mov_rr(tb, dst, src0);
            uint8_t buf[4];
            int pos = 0;
            int rex = REX_W | (REG_REX(dst) ? REX_B : 0) | REX;
            buf[pos++] = rex;
            buf[pos++] = 0xD3;
            buf[pos++] = _modrm(4, 3, REG_CODE(dst));
            return tb_emit(tb, buf, pos);
        }

    case IR_OPCODE_SHR:
        src0 = operand_reg(&inst->operands[0]);
        /* sar r/m, cl — D3 /7 */
        {
            emit_mov_rr(tb, dst, src0);
            uint8_t buf[4];
            int pos = 0;
            int rex = REX_W | (REG_REX(dst) ? REX_B : 0) | REX;
            buf[pos++] = rex;
            buf[pos++] = 0xD3;
            buf[pos++] = _modrm(7, 3, REG_CODE(dst));
            return tb_emit(tb, buf, pos);
        }

    case IR_OPCODE_CMP_EQ:
        src0 = operand_reg(&inst->operands[0]);
        src1 = operand_reg(&inst->operands[1]);
        emit_cmp_rr(tb, src0, src1);
        return emit_setcc_movzx(tb, 0x04, dst);  /* sete */

    case IR_OPCODE_CMP_NE:
        src0 = operand_reg(&inst->operands[0]);
        src1 = operand_reg(&inst->operands[1]);
        emit_cmp_rr(tb, src0, src1);
        return emit_setcc_movzx(tb, 0x05, dst);  /* setne */

    case IR_OPCODE_CMP_LT:
        src0 = operand_reg(&inst->operands[0]);
        src1 = operand_reg(&inst->operands[1]);
        emit_cmp_rr(tb, src0, src1);
        return emit_setcc_movzx(tb, 0x0C, dst);  /* setl */

    case IR_OPCODE_CMP_LE:
        src0 = operand_reg(&inst->operands[0]);
        src1 = operand_reg(&inst->operands[1]);
        emit_cmp_rr(tb, src0, src1);
        return emit_setcc_movzx(tb, 0x0E, dst);  /* setle */

    case IR_OPCODE_CMP_GT:
        src0 = operand_reg(&inst->operands[0]);
        src1 = operand_reg(&inst->operands[1]);
        emit_cmp_rr(tb, src0, src1);
        return emit_setcc_movzx(tb, 0x0F, dst);  /* setg */

    case IR_OPCODE_CMP_GE:
        src0 = operand_reg(&inst->operands[0]);
        src1 = operand_reg(&inst->operands[1]);
        emit_cmp_rr(tb, src0, src1);
        return emit_setcc_movzx(tb, 0x0D, dst);  /* setge */

    case IR_OPCODE_BR:
        /* jmp label — placeholder, needs relocation */
        return emit_jmp(tb, &reloff);

    case IR_OPCODE_BR_COND:
        /* test src0, src0; jnz then; jmp else */
        src0 = operand_reg(&inst->operands[0]);
        {
            /* test reg, reg — 85 /r */
            emit_rr(tb, 0x85, src0, src0, 1);
            /* jnz (then) */
            emit_jcc(tb, JCC_NE, &reloff);
            /* jmp (else) */
            emit_jmp(tb, &reloff);
            return 0;
        }

    case IR_OPCODE_RET:
        return emit_ret(tb);

    case IR_OPCODE_CALL:
        /* call func — needs symbol lookup + relocation */
        {
            /* Find or create symbol for callee */
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
            size_t relpos;
            emit_call(tb, &relpos);
            if (symidx >= 0) {
                add_rel(code, ARCH_REL_BRANCH, relpos, symidx);
            }
            return 0;
        }

    case IR_OPCODE_ALLOCA:
        /* sub rsp, size — approximate with fixed 16-byte alignment */
        return emit_sub_rsp(tb, 16);

    case IR_OPCODE_LOAD:
        /* mov reg, [reg] — 0x8B /r */
        src0 = operand_reg(&inst->operands[0]);
        {
            emit_mov_rr(tb, dst, src0);  /* simplified: just mov */
            return 0;
        }

    case IR_OPCODE_STORE:
        /* mov [reg], reg — 0x89 /r */
        src0 = operand_reg(&inst->operands[0]);
        src1 = operand_reg(&inst->operands[1]);
        return emit_rr(tb, 0x89, src0, src1, 1);

    /* Unhandled opcodes — emit NOP for now */
    case IR_OPCODE_PHI:
    case IR_OPCODE_SWITCH:
    case IR_OPCODE_MEMCPY:
    case IR_OPCODE_UDIV:
    case IR_OPCODE_MOD:
    case IR_OPCODE_UREM:
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
        return tb_byte(tb, 0x90);  /* NOP */

    default:
        return tb_byte(tb, 0x90);  /* NOP */
    }
}

/*
 * x86_64_assemble -- assemble from DFIR to x86-64 machine code
 */
int
x86_64_assemble(ir_object_t *obj, arch_code_t *code)
{
    textbuf_t tb;
    ir_func_t *func;

    if (!obj || !code) return -1;

    if (tb_init(&tb) < 0) return -1;

    /* Walk all functions */
    func = obj->funcs;
    while (func) {
        /* Record function start offset */
        off_t func_start = tb.size;
        int symidx = add_sym(code, ARCH_SYM_FUNC, func->name, func_start, 0);

        /* Walk all blocks */
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

        /* Update symbol size */
        code->sym.syms[symidx].size = tb.size - func_start;

        func = func->next;
    }

    /* Transfer text buffer to code */
    code->text.s = tb.buf;
    code->text.size = tb.size;
    code->cpu = ARCH_CPU_X86_64;

    (void)x86_64_initialize;
    (void)x86_64_load_instr;
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
