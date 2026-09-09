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
    size_t off;       /* offset of rel32 field in text */
    char *target;     /* target label name */
    int size;         /* instruction size (5 for jmp, 6 for jcc) */
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
    int max_ssa;
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
        if (strcmp(m->items[i].name, name) == 0) return m->items[i].offset;
    }
    return (size_t)-1;
}

static void
patch_list_add(patch_list_t *p, size_t off, const char *target, int size)
{
    if (p->count >= p->cap) {
        p->cap = p->cap ? p->cap * 2 : 16;
        p->items = realloc(p->items, p->cap * sizeof(branch_patch_t));
    }
    p->items[p->count].off = off;
    p->items[p->count].target = strdup(target);
    p->items[p->count].size = size;
    p->count++;
}

static void
resolve_patches(asm_ctx_t *ctx)
{
    for (int i = 0; i < ctx->patches.count; i++) {
        branch_patch_t *p = &ctx->patches.items[i];
        size_t target = label_map_lookup(&ctx->labels, p->target);
        if (target == (size_t)-1) continue;
        /* rel32 = target - (instruction_end) = target - (off + 4) */
        int32_t disp = (int32_t)(target - (p->off + 4));
        memcpy(ctx->tb.buf + p->off, &disp, 4);
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
 * x86-64 calling convention (System V AMD64):
 *   Arguments: RDI, RSI, RDX, RCX, R8, R9 (first 6 integer args)
 *   Return: RAX
 *   Caller-saved: RAX, RCX, RDX, RSI, RDI, R8-R11
 *   Callee-saved: RBX, RBP, R12-R15
 *
 * Our SSA register mapping already uses:
 *   %0=RAX, %1=RCX, %2=RDX, %3=RSI, %4=RDI,
 *   %5=R8, %6=R9, %7=R10, %8=R11, %9=R12,
 *   %10=R13, %11=R14, %12=R15, %13=RBX
 */

/* Argument registers in order */
static const x86_64_reg_t arg_regs[6] = {
    REG_RDI, REG_RSI, REG_RDX, REG_RCX, REG_R8, REG_R9
};

/*
 * Emit function prologue:
 *   push rbp
 *   mov rbp, rsp
 *   sub rsp, <stack_size>
 */
static int
emit_prologue(asm_ctx_t *ctx, int nargs, int max_ssa)
{
    (void)nargs;
    /* push rbp */
    tb_byte(&ctx->tb, 0x55);
    /* mov rbp, rsp (REX.W + 0x8B + ModR/M) */
    uint8_t buf[4];
    int rex = REX_W;
    buf[0] = rex | REX;
    buf[1] = 0x8B;
    buf[2] = _modrm(REG_CODE(REG_RBP), 3, REG_CODE(REG_RSP));
    tb_emit(&ctx->tb, buf, 3);

    /* Allocate stack space if we need to spill (approximate: 16 bytes alignment) */
    /* For now, allocate space for callee-saved regs that might be used */
    int stack_size = 0;
    /* %9-%13 are callee-saved (R12,R13,R14,R15,RBX) */
    if (max_ssa >= 9) {
        int saved = max_ssa - 8;
        if (saved > 5) saved = 5;
        stack_size = ((saved * 8 + 15) / 16) * 16;
    }
    if (stack_size > 0) {
        /* sub rsp, stack_size */
        buf[0] = REX | REX_W;
        buf[1] = 0x81;
        buf[2] = _modrm(5, 3, REG_CODE(REG_RSP));
        int32_t sz = stack_size;
        tb_emit(&ctx->tb, buf, 3);
        tb_emit(&ctx->tb, (uint8_t*)&sz, 4);
    }

    /* Save callee-saved registers to stack */
    /* %9=R12, %10=R13, %11=R14, %12=R15, %13=RBX */
    int slot = 0;
    for (int i = 9; i <= 13 && i <= max_ssa; i++) {
        x86_64_reg_t reg = ssa_to_reg(i);
        /* mov [rbp - offset], reg */
        int offset = -((slot + 1) * 8);
        int rex2 = REX_W | (REG_REX(reg) ? REX_R : 0) | REX;
        buf[0] = rex2;
        buf[1] = 0x89;
        buf[2] = _modrm(REG_CODE(reg), 1, REG_CODE(REG_RBP));  /* mod=1: disp8 */
        tb_emit(&ctx->tb, buf, 3);
        int8_t disp8 = (int8_t)offset;
        tb_emit(&ctx->tb, (uint8_t*)&disp8, 1);
        slot++;
    }

    return 0;
}

/*
 * Emit function epilogue:
 *   restore callee-saved registers
 *   mov rsp, rbp (or leave)
 *   pop rbp
 *   ret
 */
static int
emit_epilogue(asm_ctx_t *ctx, int max_ssa)
{
    /* Restore callee-saved registers */
    int slot = 0;
    for (int i = 9; i <= 13 && i <= max_ssa; i++) {
        x86_64_reg_t reg = ssa_to_reg(i);
        int offset = -((slot + 1) * 8);
        /* mov reg, [rbp - offset] */
        int rex2 = REX_W | (REG_REX(reg) ? REX_R : 0) | REX;
        uint8_t buf[4];
        buf[0] = rex2;
        buf[1] = 0x8B;
        buf[2] = _modrm(REG_CODE(reg), 1, REG_CODE(REG_RBP));
        tb_emit(&ctx->tb, buf, 3);
        int8_t disp8 = (int8_t)offset;
        tb_emit(&ctx->tb, (uint8_t*)&disp8, 1);
        slot++;
    }

    /* leave = mov rsp, rbp; pop rbp */
    tb_byte(&ctx->tb, 0xC9);
    /* ret */
    tb_byte(&ctx->tb, 0xC3);
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
compile_instr(asm_ctx_t *ctx, ir_instr_t *inst)
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
        return emit_mov_imm(&ctx->tb, dst, imm);

    case IR_OPCODE_MOV:
        /* operands[0] = source, operands[1] = destination */
        src0 = operand_reg(&inst->operands[0]);
        dst = operand_reg(&inst->operands[1]);
        return emit_mov_rr(&ctx->tb, dst, src0);

    case IR_OPCODE_ADD:
        src0 = operand_reg(&inst->operands[0]);
        src1 = operand_reg(&inst->operands[1]);
        /* add src0, src1 => result in src0, then mov to dst if different */
        emit_rr(&ctx->tb, 0x01, src0, src1, 1);  /* add src0, src1 */
        if (dst != src0) emit_mov_rr(&ctx->tb, dst, src0);
        return 0;

    case IR_OPCODE_SUB:
        src0 = operand_reg(&inst->operands[0]);
        src1 = operand_reg(&inst->operands[1]);
        emit_rr(&ctx->tb, 0x29, src0, src1, 1);
        if (dst != src0) emit_mov_rr(&ctx->tb, dst, src0);
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
            emit_mov_rr(&ctx->tb, dst, src0);
            rex = REX_W;
            modrm = _modrm(REG_CODE(dst), 3, REG_CODE(src1));
            rex |= REG_REX(dst) ? REX_R : 0;
            rex |= REG_REX(src1) ? REX_B : 0;
            pos = 0;
            if (rex) buf[pos++] = rex | REX;
            buf[pos++] = 0x0F;
            buf[pos++] = 0xAF;
            buf[pos++] = modrm;
            return tb_emit(&ctx->tb, buf, pos);
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
            if (dst != src0) emit_mov_rr(&ctx->tb, dst, src0);
            return tb_emit(&ctx->tb, buf, pos);
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
            if (dst != src0) emit_mov_rr(&ctx->tb, dst, src0);
            return tb_emit(&ctx->tb, buf, pos);
        }

    case IR_OPCODE_AND:
        src0 = operand_reg(&inst->operands[0]);
        src1 = operand_reg(&inst->operands[1]);
        emit_rr(&ctx->tb, 0x21, src0, src1, 1);
        if (dst != src0) emit_mov_rr(&ctx->tb, dst, src0);
        return 0;

    case IR_OPCODE_OR:
        src0 = operand_reg(&inst->operands[0]);
        src1 = operand_reg(&inst->operands[1]);
        emit_rr(&ctx->tb, 0x09, src0, src1, 1);
        if (dst != src0) emit_mov_rr(&ctx->tb, dst, src0);
        return 0;

    case IR_OPCODE_XOR:
        src0 = operand_reg(&inst->operands[0]);
        src1 = operand_reg(&inst->operands[1]);
        emit_rr(&ctx->tb, 0x31, src0, src1, 1);
        if (dst != src0) emit_mov_rr(&ctx->tb, dst, src0);
        return 0;

    case IR_OPCODE_SHL:
        src0 = operand_reg(&inst->operands[0]);
        /* shl r/m, cl — D3 /4 */
        {
            /* mov dst, src0; shl dst, cl */
            emit_mov_rr(&ctx->tb, dst, src0);
            uint8_t buf[4];
            int pos = 0;
            int rex = REX_W | (REG_REX(dst) ? REX_B : 0) | REX;
            buf[pos++] = rex;
            buf[pos++] = 0xD3;
            buf[pos++] = _modrm(4, 3, REG_CODE(dst));
            return tb_emit(&ctx->tb, buf, pos);
        }

    case IR_OPCODE_SHR:
        src0 = operand_reg(&inst->operands[0]);
        /* sar r/m, cl — D3 /7 */
        {
            emit_mov_rr(&ctx->tb, dst, src0);
            uint8_t buf[4];
            int pos = 0;
            int rex = REX_W | (REG_REX(dst) ? REX_B : 0) | REX;
            buf[pos++] = rex;
            buf[pos++] = 0xD3;
            buf[pos++] = _modrm(7, 3, REG_CODE(dst));
            return tb_emit(&ctx->tb, buf, pos);
        }

    case IR_OPCODE_CMP_EQ:
        src0 = operand_reg(&inst->operands[0]);
        src1 = operand_reg(&inst->operands[1]);
        emit_cmp_rr(&ctx->tb, src0, src1);
        return emit_setcc_movzx(&ctx->tb, 0x04, dst);  /* sete */

    case IR_OPCODE_CMP_NE:
        src0 = operand_reg(&inst->operands[0]);
        src1 = operand_reg(&inst->operands[1]);
        emit_cmp_rr(&ctx->tb, src0, src1);
        return emit_setcc_movzx(&ctx->tb, 0x05, dst);  /* setne */

    case IR_OPCODE_CMP_LT:
        src0 = operand_reg(&inst->operands[0]);
        src1 = operand_reg(&inst->operands[1]);
        emit_cmp_rr(&ctx->tb, src0, src1);
        return emit_setcc_movzx(&ctx->tb, 0x0C, dst);  /* setl */

    case IR_OPCODE_CMP_LE:
        src0 = operand_reg(&inst->operands[0]);
        src1 = operand_reg(&inst->operands[1]);
        emit_cmp_rr(&ctx->tb, src0, src1);
        return emit_setcc_movzx(&ctx->tb, 0x0E, dst);  /* setle */

    case IR_OPCODE_CMP_GT:
        src0 = operand_reg(&inst->operands[0]);
        src1 = operand_reg(&inst->operands[1]);
        emit_cmp_rr(&ctx->tb, src0, src1);
        return emit_setcc_movzx(&ctx->tb, 0x0F, dst);  /* setg */

    case IR_OPCODE_CMP_GE:
        src0 = operand_reg(&inst->operands[0]);
        src1 = operand_reg(&inst->operands[1]);
        emit_cmp_rr(&ctx->tb, src0, src1);
        return emit_setcc_movzx(&ctx->tb, 0x0D, dst);  /* setge */

    case IR_OPCODE_BR: {
        /* jmp label — emit placeholder, save patch */
        tb_byte(&ctx->tb, 0xE9);  /* jmp rel32 */
        size_t patch_off = ctx->tb.size;
        tb_u32(&ctx->tb, 0);  /* placeholder */
        if (inst->noperands > 0 && inst->operands[0].type == IR_OPERAND_LABEL) {
            patch_list_add(&ctx->patches, patch_off, inst->operands[0].u.label, 5);
        }
        return 0;
    }

    case IR_OPCODE_BR_COND: {
        /* test reg, reg; jz $else — if cond==0, jump to else; then falls through */
        src0 = operand_reg(&inst->operands[0]);
        emit_rr(&ctx->tb, 0x85, src0, src0, 1);  /* test reg, reg */
        /* jz rel32: 0F 84 cd */
        tb_byte(&ctx->tb, 0x0F);
        tb_byte(&ctx->tb, 0x84);
        size_t patch_off = ctx->tb.size;
        tb_u32(&ctx->tb, 0);  /* placeholder */
        if (inst->noperands > 2 && inst->operands[2].type == IR_OPERAND_LABEL) {
            patch_list_add(&ctx->patches, patch_off, inst->operands[2].u.label, 6);
        }
        return 0;
    }

    case IR_OPCODE_RET:
        /* Move return value to RAX if needed */
        if (inst->noperands > 0) {
            if (inst->operands[0].type == IR_OPERAND_IMM) {
                /* Load immediate into RAX */
                int ok;
                int64_t val = operand_imm(&inst->operands[0], &ok);
                if (ok) emit_mov_imm(&ctx->tb, REG_RAX, val);
            } else {
                src0 = operand_reg(&inst->operands[0]);
                if (src0 != REG_RAX) {
                    emit_mov_rr(&ctx->tb, REG_RAX, src0);
                }
            }
        }
        /* Emit full epilogue */
        emit_epilogue(ctx, ctx->max_ssa);
        return 0;

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
            size_t relpos;
            emit_call(&ctx->tb, &relpos);
            if (symidx >= 0) {
                add_rel(ctx->code, ARCH_REL_BRANCH, relpos, symidx);
            }
            return 0;
        }

    case IR_OPCODE_ALLOCA:
        /* sub rsp, size — approximate with fixed 16-byte alignment */
        return emit_sub_rsp(&ctx->tb, 16);

    case IR_OPCODE_LOAD:
        /* mov reg, [reg] — 0x8B /r */
        src0 = operand_reg(&inst->operands[0]);
        {
            emit_mov_rr(&ctx->tb, dst, src0);  /* simplified: just mov */
            return 0;
        }

    case IR_OPCODE_STORE:
        /* mov [reg], reg — 0x89 /r */
        src0 = operand_reg(&inst->operands[0]);
        src1 = operand_reg(&inst->operands[1]);
        return emit_rr(&ctx->tb, 0x89, src0, src1, 1);

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
        return tb_byte(&ctx->tb, 0x90);  /* NOP */

    default:
        return tb_byte(&ctx->tb, 0x90);  /* NOP */
    }
}

/*
 * x86_64_assemble -- assemble from DFIR to x86-64 machine code
 */
int
x86_64_assemble(ir_object_t *obj, arch_code_t *code)
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

        /* Emit prologue */
        emit_prologue(&ctx, func->nargs, max_ssa);

        for (size_t bi = 0; bi < func->nblocks; bi++) {
            ir_block_t *blk = &func->blocks[bi];

            /* Record block label -> text offset */
            if (blk->label && blk->label->name) {
                label_map_add(&ctx.labels, blk->label->name, ctx.tb.size);
            }

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

        /* Emit epilogue (in case function doesn't end with RET) */
        emit_epilogue(&ctx, max_ssa);

        code->sym.syms[symidx].size = ctx.tb.size - func_start;
        func = func->next;
    }

    /* Resolve branch patches */
    resolve_patches(&ctx);

    code->text.s = ctx.tb.buf;
    code->text.size = ctx.tb.size;
    code->cpu = ARCH_CPU_X86_64;

    (void)x86_64_initialize;
    (void)x86_64_load_instr;
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
