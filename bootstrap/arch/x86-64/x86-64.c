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
    struct {
        size_t *lea_off;
        int *str_idx;
        int count;
        int cap;
    } str_patches;
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
    free(ctx->str_patches.lea_off);
    free(ctx->str_patches.str_idx);
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
__attribute__((unused))
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
__attribute__((unused))
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
__attribute__((unused))
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

static int
emit_add_rsp(textbuf_t *tb, int32_t size)
{
    /* REX.W + 0x81 /0 (ADD r/m64, imm32) with RSP */
    uint8_t buf[8];
    int pos = 0;
    buf[pos++] = REX | REX_W;
    buf[pos++] = 0x81;
    buf[pos++] = _modrm(0, 3, REG_CODE(REG_RSP));
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
__attribute__((unused))
static const x86_64_reg_t arg_regs[6] = {
    REG_RDI, REG_RSI, REG_RDX, REG_RCX, REG_R8, REG_R9
};

/*
 * Emit function prologue:
 *   push rbp
 *   mov rbp, rsp
 *   sub rsp, <stack_size>
 */

/* Save caller-saved registers (SSA %0-%8) to stack before a call.
 * %0=RAX, %1=RCX, %2=RDX, %3=RSI, %4=RDI, %5=R8, %6=R9, %7=R10, %8=R11 */
static void
emit_caller_save(asm_ctx_t *ctx)
{
    int max_cs = ctx->max_ssa < 8 ? ctx->max_ssa : 8;
    int n_cs = max_cs + 1;
    if (n_cs % 2 != 0) n_cs++;
    int save_size = n_cs * 8;
    emit_sub_rsp(&ctx->tb, save_size);
    for (int i = 0; i <= max_cs; i++) {
        x86_64_reg_t reg = ssa_to_reg(i);
        if (reg == REG_NONE) continue;
        int offset = i * 8;
        uint8_t buf[8];
        int pos = 0;
        int rex = REX_W | (REG_REX(reg) ? REX_R : 0) | REX;
        buf[pos++] = rex;
        buf[pos++] = 0x89;
        if (offset == 0) {
            buf[pos++] = _modrm(REG_CODE(reg), 0, REG_CODE(REG_RSP));
            buf[pos++] = _sib(REG_CODE(REG_RSP), REG_CODE(REG_RSP), 0);
        } else if (offset < 128) {
            buf[pos++] = _modrm(REG_CODE(reg), 1, REG_CODE(REG_RSP));
            buf[pos++] = _sib(REG_CODE(REG_RSP), REG_CODE(REG_RSP), 0);
            buf[pos++] = (uint8_t)(int8_t)offset;
        } else {
            buf[pos++] = _modrm(REG_CODE(reg), 2, REG_CODE(REG_RSP));
            buf[pos++] = _sib(REG_CODE(REG_RSP), REG_CODE(REG_RSP), 0);
            memcpy(buf + pos, &offset, 4); pos += 4;
        }
        tb_emit(&ctx->tb, buf, pos);
    }
}

/* Restore caller-saved registers from stack after a call. */
static void
emit_caller_restore(asm_ctx_t *ctx)
{
    int max_cs = ctx->max_ssa < 8 ? ctx->max_ssa : 8;
    int n_cs = max_cs + 1;
    if (n_cs % 2 != 0) n_cs++;
    int save_size = n_cs * 8;
    for (int i = 0; i <= max_cs; i++) {
        x86_64_reg_t reg = ssa_to_reg(i);
        if (reg == REG_NONE) continue;
        int offset = i * 8;
        uint8_t buf[8];
        int pos = 0;
        int rex = REX_W | (REG_REX(reg) ? REX_R : 0) | REX;
        buf[pos++] = rex;
        buf[pos++] = 0x8B;
        if (offset == 0) {
            buf[pos++] = _modrm(REG_CODE(reg), 0, REG_CODE(REG_RSP));
            buf[pos++] = _sib(REG_CODE(REG_RSP), REG_CODE(REG_RSP), 0);
        } else if (offset < 128) {
            buf[pos++] = _modrm(REG_CODE(reg), 1, REG_CODE(REG_RSP));
            buf[pos++] = _sib(REG_CODE(REG_RSP), REG_CODE(REG_RSP), 0);
            buf[pos++] = (uint8_t)(int8_t)offset;
        } else {
            buf[pos++] = _modrm(REG_CODE(reg), 2, REG_CODE(REG_RSP));
            buf[pos++] = _sib(REG_CODE(REG_RSP), REG_CODE(REG_RSP), 0);
            memcpy(buf + pos, &offset, 4); pos += 4;
        }
        tb_emit(&ctx->tb, buf, pos);
    }
    emit_add_rsp(&ctx->tb, save_size);
}

static int
emit_prologue(asm_ctx_t *ctx, int nargs, int max_ssa)
{
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

    /* Move arguments from ABI registers (RDI, RSI, RDX, RCX, R8, R9)
     * to their SSA register locations. SSA %0=RAX, %1=RCX, %2=RDX,
     * %3=RSI, %4=RDI, %5=R8, %6=R9. Move in reverse to avoid clobbering. */
    {
        static const x86_64_reg_t abi_args[6] = {
            REG_RDI, REG_RSI, REG_RDX, REG_RCX, REG_R8, REG_R9
        };
        int n = nargs < 6 ? nargs : 6;
        /* Use R10 as temp if needed, move in reverse */
        for (int i = n - 1; i >= 0; i--) {
            x86_64_reg_t src = abi_args[i];
            x86_64_reg_t dst = ssa_to_reg(i);
            if (dst == REG_NONE || dst == src) continue;
            /* Check if dst is used as a later src (cycle) */
            int has_cycle = 0;
            for (int j = 0; j < i; j++) {
                if (ssa_to_reg(j) == dst && abi_args[j] == src) {
                    has_cycle = 1; break;
                }
            }
            if (has_cycle) {
                /* Use R10 as temp */
                emit_mov_rr(&ctx->tb, REG_R10, src);
                emit_mov_rr(&ctx->tb, dst, REG_R10);
            } else {
                emit_mov_rr(&ctx->tb, dst, src);
            }
        }
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
 * Add a string literal to the data section.
 */
static int
add_string(asm_ctx_t *ctx, const char *str)
{
    for (int i = 0; i < ctx->code->strings.n; i++) {
        if (strcmp(ctx->code->strings.items[i].str, str) == 0) return i;
    }
    int n = ctx->code->strings.n;
    ctx->code->strings.items = realloc(ctx->code->strings.items,
        (n + 1) * sizeof(*ctx->code->strings.items));
    ctx->code->strings.items[n].str = strdup(str);
    ctx->code->strings.items[n].offset = 0;
    ctx->code->strings.n++;
    return n;
}

static void
str_patch_add(asm_ctx_t *ctx, size_t lea_off, int str_idx)
{
    if (ctx->str_patches.count >= ctx->str_patches.cap) {
        ctx->str_patches.cap = ctx->str_patches.cap ? ctx->str_patches.cap * 2 : 16;
        ctx->str_patches.lea_off = realloc(ctx->str_patches.lea_off,
            ctx->str_patches.cap * sizeof(size_t));
        ctx->str_patches.str_idx = realloc(ctx->str_patches.str_idx,
            ctx->str_patches.cap * sizeof(int));
    }
    ctx->str_patches.lea_off[ctx->str_patches.count] = lea_off;
    ctx->str_patches.str_idx[ctx->str_patches.count] = str_idx;
    ctx->str_patches.count++;
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
 * Get a register for an operand, loading immediates into a temp register.
 */
static int64_t operand_imm(ir_operand_t *op, int *ok);
static int emit_mov_imm(textbuf_t *tb, x86_64_reg_t reg, int64_t val);

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
static x86_64_reg_t operand_reg_or_imm_scratch(asm_ctx_t *ctx, ir_operand_t *op, x86_64_reg_t scratch);
static x86_64_reg_t operand_reg_or_imm(asm_ctx_t *ctx, ir_operand_t *op);
static void str_patch_add(asm_ctx_t *ctx, size_t lea_off, int str_idx);

static x86_64_reg_t
operand_reg_or_imm_scratch(asm_ctx_t *ctx, ir_operand_t *op, x86_64_reg_t scratch)
{
    if (op->type == IR_OPERAND_IMM) {
        int ok;
        int64_t val = operand_imm(op, &ok);
        if (ok) {
            emit_mov_imm(&ctx->tb, scratch, val);
            return scratch;
        }
    }
    return operand_reg(op);
}

static x86_64_reg_t
__attribute__((unused))
operand_reg_or_imm(asm_ctx_t *ctx, ir_operand_t *op)
{
    return operand_reg_or_imm_scratch(ctx, op, REG_R10);
}


/* Try to emit a binary op with immediate second operand.
 * Returns 1 if handled, 0 if not (caller should use register form).
 * subopcode: 0=add, 1=or, 4=and, 5=sub, 6=xor */
static int
try_emit_binop_imm(asm_ctx_t *ctx, x86_64_reg_t dst, x86_64_reg_t src0,
                   ir_operand_t *src1_op, uint8_t subopcode)
{
    if (src1_op->type != IR_OPERAND_IMM) return 0;
    int ok;
    int64_t val = operand_imm(src1_op, &ok);
    if (!ok) return 0;

    /* mov dst, src0 */
    if (dst != src0) {
        emit_mov_rr(&ctx->tb, dst, src0);
    }
    /* op dst, imm32 — REX.W 0x81 /subopcode imm32 */
    {
        uint8_t buf[8];
        int pos = 0;
        int rex = REX_W | (REG_REX(dst) ? REX_B : 0) | REX;
        buf[pos++] = rex;
        buf[pos++] = 0x81;
        buf[pos++] = _modrm(subopcode, 3, REG_CODE(dst));
        int32_t imm32 = (int32_t)val;
        memcpy(buf + pos, &imm32, 4); pos += 4;
        tb_emit(&ctx->tb, buf, pos);
    }
    return 1;
}

static int
compile_instr(asm_ctx_t *ctx, ir_instr_t *inst)
{
    x86_64_reg_t dst, src0, src1;
    int ok;
    int64_t imm;
    /* size_t reloff; -- unused */

    dst = REG_NONE;
    if (inst->result.n > 0) {
        dst = ssa_to_reg(ssa_id(inst->result.reg[0].id));
    }

    switch (inst->opcode) {
    case IR_OPCODE_CONST:
        if (inst->operands[0].type == IR_OPERAND_IMM &&
            inst->operands[0].u.imm.type == IR_IMM_STR) {
            /* String constant: emit LEA with relocation */
            const char *str = inst->operands[0].u.imm.u.str;
            if (!str) str = "";
            int sid = add_string(ctx, str);
            /* lea dst, [rip + 0] — placeholder, patched later */
            uint8_t buf[7];
            int rex = REX | (REG_REX(dst) ? REX_R : 0);
            buf[0] = rex;
            buf[1] = 0x8D;  /* LEA */
            buf[2] = _modrm(0, REG_CODE(dst), 5);  /* mod=0, rm=5 = RIP-relative */
            tb_emit(&ctx->tb, buf, 3);
            size_t off = ctx->tb.size;
            tb_u32(&ctx->tb, 0);  /* placeholder disp32 */
            str_patch_add(ctx, off, sid);
            return 0;
        }
        imm = operand_imm(&inst->operands[0], &ok);
        if (!ok) return -1;
        return emit_mov_imm(&ctx->tb, dst, imm);

    case IR_OPCODE_MOV:
        /* operands[0] = source, operands[1] = destination */
        src0 = operand_reg(&inst->operands[0]);
        dst = operand_reg(&inst->operands[1]);
        return emit_mov_rr(&ctx->tb, dst, src0);

    case IR_OPCODE_ADD:
        /* If src0 is immediate, load it directly into dst */
        if (inst->operands[0].type == IR_OPERAND_IMM) {
            int ok; int64_t v = operand_imm(&inst->operands[0], &ok);
            if (ok) { emit_mov_imm(&ctx->tb, dst, v); src0 = dst; }
            else src0 = operand_reg_or_imm_scratch(ctx, &inst->operands[0], REG_R10);
        } else src0 = operand_reg(&inst->operands[0]);
        if (try_emit_binop_imm(ctx, dst, src0, &inst->operands[1], 0))
            return 0;
        src1 = operand_reg_or_imm_scratch(ctx, &inst->operands[1], REG_R11);
        emit_mov_rr(&ctx->tb, dst, src0);
        emit_rr(&ctx->tb, 0x01, dst, src1, 1);  /* dst += src1 */
        return 0;

    case IR_OPCODE_SUB:
        if (inst->operands[0].type == IR_OPERAND_IMM) {
            int ok; int64_t v = operand_imm(&inst->operands[0], &ok);
            if (ok) { emit_mov_imm(&ctx->tb, dst, v); src0 = dst; }
            else src0 = operand_reg_or_imm_scratch(ctx, &inst->operands[0], REG_R10);
        } else src0 = operand_reg(&inst->operands[0]);
        if (try_emit_binop_imm(ctx, dst, src0, &inst->operands[1], 5))
            return 0;
        src1 = operand_reg_or_imm_scratch(ctx, &inst->operands[1], REG_R11);
        emit_mov_rr(&ctx->tb, dst, src0);
        emit_rr(&ctx->tb, 0x29, dst, src1, 1);  /* dst -= src1 */
        return 0;

    case IR_OPCODE_MUL:
        src0 = operand_reg_or_imm_scratch(ctx, &inst->operands[0], REG_R10);
        src1 = operand_reg_or_imm_scratch(ctx, &inst->operands[1], REG_R11);
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
        /* Handle immediate operand: load into dst first, then negate */
        if (inst->operands[0].type == IR_OPERAND_IMM) {
            int ok; int64_t v = operand_imm(&inst->operands[0], &ok);
            if (ok) {
                emit_mov_imm(&ctx->tb, dst, v);
                src0 = dst;
            } else {
                src0 = operand_reg(&inst->operands[0]);
                emit_mov_rr(&ctx->tb, dst, src0);
            }
        } else {
            src0 = operand_reg(&inst->operands[0]);
            /* Copy to dst first to avoid clobbering src0 */
            emit_mov_rr(&ctx->tb, dst, src0);
        }
        /* neg r/m64 — F7 /3 */
        {
            uint8_t buf[4];
            int pos = 0;
            int rex = REX_W | (REG_REX(dst) ? REX_B : 0) | REX;
            int modrm = _modrm(3, 3, REG_CODE(dst));
            buf[pos++] = rex;
            buf[pos++] = 0xF7;
            buf[pos++] = modrm;
            return tb_emit(&ctx->tb, buf, pos);
        }

    case IR_OPCODE_NOT:
        src0 = operand_reg(&inst->operands[0]);
        /* not r/m64 — F7 /2 */
        {
            emit_mov_rr(&ctx->tb, dst, src0);
            uint8_t buf[4];
            int pos = 0;
            int rex = REX_W | (REG_REX(dst) ? REX_B : 0) | REX;
            int modrm = _modrm(2, 3, REG_CODE(dst));
            buf[pos++] = rex;
            buf[pos++] = 0xF7;
            buf[pos++] = modrm;
            return tb_emit(&ctx->tb, buf, pos);
        }

    case IR_OPCODE_AND:
        if (inst->operands[0].type == IR_OPERAND_IMM) {
            int ok; int64_t v = operand_imm(&inst->operands[0], &ok);
            if (ok) { emit_mov_imm(&ctx->tb, dst, v); src0 = dst; }
            else src0 = operand_reg_or_imm_scratch(ctx, &inst->operands[0], REG_R10);
        } else src0 = operand_reg(&inst->operands[0]);
        if (try_emit_binop_imm(ctx, dst, src0, &inst->operands[1], 4))
            return 0;
        src1 = operand_reg_or_imm_scratch(ctx, &inst->operands[1], REG_R11);
        emit_mov_rr(&ctx->tb, dst, src0);
        emit_rr(&ctx->tb, 0x21, dst, src1, 1);  /* dst &= src1 */
        return 0;

    case IR_OPCODE_OR:
        if (inst->operands[0].type == IR_OPERAND_IMM) {
            int ok; int64_t v = operand_imm(&inst->operands[0], &ok);
            if (ok) { emit_mov_imm(&ctx->tb, dst, v); src0 = dst; }
            else src0 = operand_reg_or_imm_scratch(ctx, &inst->operands[0], REG_R10);
        } else src0 = operand_reg(&inst->operands[0]);
        if (try_emit_binop_imm(ctx, dst, src0, &inst->operands[1], 1))
            return 0;
        src1 = operand_reg_or_imm_scratch(ctx, &inst->operands[1], REG_R11);
        emit_mov_rr(&ctx->tb, dst, src0);
        emit_rr(&ctx->tb, 0x09, dst, src1, 1);  /* dst |= src1 */
        return 0;

    case IR_OPCODE_XOR:
        if (inst->operands[0].type == IR_OPERAND_IMM) {
            int ok; int64_t v = operand_imm(&inst->operands[0], &ok);
            if (ok) { emit_mov_imm(&ctx->tb, dst, v); src0 = dst; }
            else src0 = operand_reg_or_imm_scratch(ctx, &inst->operands[0], REG_R10);
        } else src0 = operand_reg(&inst->operands[0]);
        if (try_emit_binop_imm(ctx, dst, src0, &inst->operands[1], 6))
            return 0;
        src1 = operand_reg_or_imm_scratch(ctx, &inst->operands[1], REG_R11);
        emit_mov_rr(&ctx->tb, dst, src0);
        emit_rr(&ctx->tb, 0x31, dst, src1, 1);  /* dst ^= src1 */
        return 0;

    case IR_OPCODE_SHL:
        src0 = operand_reg(&inst->operands[0]);
        src1 = operand_reg_or_imm_scratch(ctx, &inst->operands[1], REG_R11);
        {
            /* mov dst, src0; mov cl, src1_low; shl dst, cl */
            emit_mov_rr(&ctx->tb, dst, src0);
            /* mov rcx, src1 (to get shift count in CL) */
            emit_mov_rr(&ctx->tb, REG_RCX, src1);
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
        src1 = operand_reg_or_imm_scratch(ctx, &inst->operands[1], REG_R11);
        {
            emit_mov_rr(&ctx->tb, dst, src0);
            /* mov rcx, src1 (to get shift count in CL) */
            emit_mov_rr(&ctx->tb, REG_RCX, src1);
            uint8_t buf[4];
            int pos = 0;
            int rex = REX_W | (REG_REX(dst) ? REX_B : 0) | REX;
            buf[pos++] = rex;
            buf[pos++] = 0xD3;
            buf[pos++] = _modrm(7, 3, REG_CODE(dst));
            return tb_emit(&ctx->tb, buf, pos);
        }

    case IR_OPCODE_CMP_EQ:
        src0 = operand_reg_or_imm_scratch(ctx, &inst->operands[0], REG_R10);
        src1 = operand_reg_or_imm_scratch(ctx, &inst->operands[1], REG_R11);
        emit_cmp_rr(&ctx->tb, src0, src1);
        return emit_setcc_movzx(&ctx->tb, 0x04, dst);  /* sete */

    case IR_OPCODE_CMP_NE:
        src0 = operand_reg_or_imm_scratch(ctx, &inst->operands[0], REG_R10);
        src1 = operand_reg_or_imm_scratch(ctx, &inst->operands[1], REG_R11);
        emit_cmp_rr(&ctx->tb, src0, src1);
        return emit_setcc_movzx(&ctx->tb, 0x05, dst);  /* setne */

    case IR_OPCODE_CMP_LT:
        src0 = operand_reg_or_imm_scratch(ctx, &inst->operands[0], REG_R10);
        src1 = operand_reg_or_imm_scratch(ctx, &inst->operands[1], REG_R11);
        emit_cmp_rr(&ctx->tb, src0, src1);
        return emit_setcc_movzx(&ctx->tb, 0x0C, dst);  /* setl */

    case IR_OPCODE_CMP_LE:
        src0 = operand_reg_or_imm_scratch(ctx, &inst->operands[0], REG_R10);
        src1 = operand_reg_or_imm_scratch(ctx, &inst->operands[1], REG_R11);
        emit_cmp_rr(&ctx->tb, src0, src1);
        return emit_setcc_movzx(&ctx->tb, 0x0E, dst);  /* setle */

    case IR_OPCODE_CMP_GT:
        src0 = operand_reg_or_imm_scratch(ctx, &inst->operands[0], REG_R10);
        src1 = operand_reg_or_imm_scratch(ctx, &inst->operands[1], REG_R11);
        emit_cmp_rr(&ctx->tb, src0, src1);
        return emit_setcc_movzx(&ctx->tb, 0x0F, dst);  /* setg */

    case IR_OPCODE_CMP_GE:
        src0 = operand_reg_or_imm_scratch(ctx, &inst->operands[0], REG_R10);
        src1 = operand_reg_or_imm_scratch(ctx, &inst->operands[1], REG_R11);
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
        /* test reg, reg; jz $else; jmp $then */
        src0 = operand_reg(&inst->operands[0]);
        emit_rr(&ctx->tb, 0x85, src0, src0, 1);  /* test reg, reg */
        /* jz rel32: 0F 84 cd */
        tb_byte(&ctx->tb, 0x0F);
        tb_byte(&ctx->tb, 0x84);
        size_t patch_off = ctx->tb.size;
        tb_u32(&ctx->tb, 0);  /* placeholder for jz $else */
        if (inst->noperands > 2 && inst->operands[2].type == IR_OPERAND_LABEL) {
            patch_list_add(&ctx->patches, patch_off, inst->operands[2].u.label, 6);
        }
        /* jmp $then */
        tb_byte(&ctx->tb, 0xE9);  /* jmp rel32 */
        size_t patch_off2 = ctx->tb.size;
        tb_u32(&ctx->tb, 0);  /* placeholder for jmp $then */
        if (inst->noperands > 1 && inst->operands[1].type == IR_OPERAND_LABEL) {
            patch_list_add(&ctx->patches, patch_off2, inst->operands[1].u.label, 5);
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
                emit_caller_save(ctx);
                if (nargs > 0) {
                    ir_operand_t *arg = &inst->operands[0];
                    if (arg->type == IR_OPERAND_IMM &&
                        arg->u.imm.type == IR_IMM_STR) {
                        /* String argument */
                        const char *str = arg->u.imm.u.str ? arg->u.imm.u.str : "";
                        if (is_println) {
                            /* println(string): use puts */
                            int sid = add_string(ctx, str);
                            /* lea rdi, [rip+disp32] — RIP-relative addressing */
                            uint8_t buf[7];
                            int pos = 0;
                            buf[pos++] = REX_W | REX;
                            buf[pos++] = 0x8D;
                            buf[pos++] = _modrm(7, 0, 5);  /* mod=0, reg=RDI, rm=5(RIP) */
                            buf[pos++] = 0; buf[pos++] = 0; buf[pos++] = 0; buf[pos++] = 0;
                            size_t off = ctx->tb.size;
                            tb_emit(&ctx->tb, buf, pos);
                            str_patch_add(ctx, off + 3, sid);
                            /* call puts */
                            int si = add_sym(ctx->code, ARCH_SYM_GLOBAL, "puts", 0, 0);
                            size_t rp;
                            emit_call(&ctx->tb, &rp);
                            add_rel(ctx->code, ARCH_REL_BRANCH, rp, si);
                        } else {
                            /* print(string): use printf with string as format */
                            int sid = add_string(ctx, str);
                            uint8_t buf[7];
                            int pos = 0;
                            buf[pos++] = REX_W | REX;
                            buf[pos++] = 0x8D;
                            buf[pos++] = _modrm(7, 0, 5);  /* lea rdi, [rip+disp32] */
                            buf[pos++] = 0; buf[pos++] = 0; buf[pos++] = 0; buf[pos++] = 0;
                            size_t off = ctx->tb.size;
                            tb_emit(&ctx->tb, buf, pos);
                            str_patch_add(ctx, off + 3, sid);
                            int si = add_sym(ctx->code, ARCH_SYM_GLOBAL, "printf", 0, 0);
                            size_t rp;
                            emit_call(&ctx->tb, &rp);
                            add_rel(ctx->code, ARCH_REL_BRANCH, rp, si);
                        }
                    } else {
                        /* Integer argument */
                        int ok;
                        int64_t val = operand_imm(arg, &ok);
                        if (ok) {
                            /* Constant: convert to string, use puts */
                            char buf32[32];
                            if (is_println) {
                                snprintf(buf32, sizeof(buf32), "%lld\n", (long long)val);
                            } else {
                                snprintf(buf32, sizeof(buf32), "%lld", (long long)val);
                            }
                            int sid = add_string(ctx, buf32);
                            uint8_t buf[7];
                            int pos = 0;
                            buf[pos++] = REX_W | REX;
                            buf[pos++] = 0x8D;
                            buf[pos++] = _modrm(7, 0, 5);  /* lea rdi, [rip+disp32] */
                            buf[pos++] = 0; buf[pos++] = 0; buf[pos++] = 0; buf[pos++] = 0;
                            size_t off = ctx->tb.size;
                            tb_emit(&ctx->tb, buf, pos);
                            str_patch_add(ctx, off + 3, sid);
                            int si = add_sym(ctx->code, ARCH_SYM_GLOBAL, "puts", 0, 0);
                            size_t rp;
                            emit_call(&ctx->tb, &rp);
                            add_rel(ctx->code, ARCH_REL_BRANCH, rp, si);
                        } else {
                            /* Variable: use printf("%d\n", val) or printf("%d", val) */
                            const char *fmt = is_println ? "%d\n" : "%d";
                            /* RDI = format string */
                            int sid_fmt = add_string(ctx, fmt);
                            uint8_t buf[7];
                            int pos = 0;
                            buf[pos++] = REX_W | REX;
                            buf[pos++] = 0x8D;
                            buf[pos++] = _modrm(7, 0, 5);  /* lea rdi, [rip+disp32] */
                            buf[pos++] = 0; buf[pos++] = 0; buf[pos++] = 0; buf[pos++] = 0;
                            size_t off1 = ctx->tb.size;
                            tb_emit(&ctx->tb, buf, pos);
                            str_patch_add(ctx, off1 + 3, sid_fmt);
                            /* RSI = value */
                            int val_reg;
                            if (arg->type == IR_OPERAND_IMM) {
                                int ok2;
                                int64_t v = operand_imm(arg, &ok2);
                                if (ok2) {
                                    emit_mov_imm(&ctx->tb, REG_RSI, v);
                                    val_reg = REG_RSI;
                                } else {
                                    val_reg = REG_RAX;
                                }
                            } else {
                                val_reg = operand_reg(arg);
                                if (val_reg != REG_RSI) {
                                    emit_mov_rr(&ctx->tb, REG_RSI, val_reg);
                                }
                            }
                            /* Align stack to 16 bytes (printf is variadic) */
                            emit_sub_rsp(&ctx->tb, 8);
                            int si = add_sym(ctx->code, ARCH_SYM_GLOBAL, "printf", 0, 0);
                            size_t rp;
                            emit_call(&ctx->tb, &rp);
                            add_rel(ctx->code, ARCH_REL_BRANCH, rp, si);
                            emit_add_rsp(&ctx->tb, 8);
                        }
                    }
                }
                emit_caller_restore(ctx);
                return 0;
            }

            /* Regular function call */
            static const x86_64_reg_t x86_arg_regs[6] = {
                REG_RDI, REG_RSI, REG_RDX, REG_RCX, REG_R8, REG_R9
            };

            /* Find or create symbol for callee */
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
            /* Save caller-saved registers before the call */
            emit_caller_save(ctx);
            /* Move arguments to argument registers */
            for (int i = 0; i < nargs && i < 6; i++) {
                ir_operand_t *arg = &inst->operands[i];
                if (arg->type == IR_OPERAND_IMM && arg->u.imm.type == IR_IMM_STR) {
                    const char *str = arg->u.imm.u.str ? arg->u.imm.u.str : "";
                    int sid = add_string(ctx, str);
                    uint8_t buf[7];
                    int pos = 0;
                    buf[pos++] = REX_W | (REG_REX(x86_arg_regs[i]) ? REX_R : 0) | REX;
                    buf[pos++] = 0x8D;
                    buf[pos++] = _modrm(0, REG_CODE(x86_arg_regs[i]), 7);
                    buf[pos++] = 0; buf[pos++] = 0; buf[pos++] = 0; buf[pos++] = 0;
                    size_t off = ctx->tb.size;
                    tb_emit(&ctx->tb, buf, pos);
                    str_patch_add(ctx, off + 3, sid);
                } else if (arg->type == IR_OPERAND_IMM) {
                    int ok;
                    int64_t val = operand_imm(arg, &ok);
                    if (ok) {
                        emit_mov_imm(&ctx->tb, x86_arg_regs[i], val);
                    }
                } else {
                    int src_reg = operand_reg(arg);
                    if (src_reg != x86_arg_regs[i]) {
                        emit_mov_rr(&ctx->tb, x86_arg_regs[i], src_reg);
                    }
                }
            }
            /* Align stack to 16 bytes */
            emit_sub_rsp(&ctx->tb, 8);
            size_t relpos;
            emit_call(&ctx->tb, &relpos);
            emit_add_rsp(&ctx->tb, 8);
            if (symidx >= 0) {
                add_rel(ctx->code, ARCH_REL_BRANCH, relpos, symidx);
            }
            /* Save return value to RBX (callee-saved, not clobbered by restore)
             * before restoring caller-saved registers. */
            int dst_reg = REG_RAX;
            if (inst->result.n > 0 && inst->result.reg[0].id) {
                dst_reg = ssa_to_reg(ssa_id(inst->result.reg[0].id));
            }
            int need_save_ret = (dst_reg != REG_NONE);
            if (need_save_ret) {
                /* mov rbx, rax — save return value to callee-saved register */
                emit_mov_rr(&ctx->tb, REG_RBX, REG_RAX);
            }
            emit_caller_restore(ctx);
            if (need_save_ret) {
                /* mov dst, rbx — move return value to destination */
                emit_mov_rr(&ctx->tb, dst_reg, REG_RBX);
            }
            return 0;
        }

    case IR_OPCODE_ALLOCA: {
        /* Allocate stack space. Default 16 bytes, or use operand size. */
        int size = 16;
        if (inst->noperands > 0 && inst->operands[0].type == IR_OPERAND_IMM) {
            int ok;
            int64_t val = operand_imm(&inst->operands[0], &ok);
            if (ok && val > 0) {
                size = ((val + 15) / 16) * 16;
            }
        }
        emit_sub_rsp(&ctx->tb, size);
        /* mov dst, rsp — result is the allocated pointer */
        emit_mov_rr(&ctx->tb, dst, REG_RSP);
        return 0;
    }

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

    case IR_OPCODE_GET_ELEM: {
        /* GET_ELEM: dst = arr[idx]
         * mov dst, [base + idx*8]
         * Encoding: REX.W 8B /r with SIB: base=RSP, index=idx, scale=3 (8) */
        int base_reg = operand_reg(&inst->operands[0]);
        int idx_reg = operand_reg(&inst->operands[1]);
        /* mov dst, [base + idx*8] = REX.W 8B ModRM(mod=0, reg=dst, rm=4(SIB))
         * SIB(scale=3, index=idx, base=base) */
        uint8_t buf[4];
        int pos = 0;
        int rex = REX_W | (REG_REX(dst) ? REX_R : 0);
        if (REG_REX(base_reg)) rex |= REX_B;
        if (REG_REX(idx_reg)) rex |= REX_X;
        buf[pos++] = rex | REX;
        buf[pos++] = 0x8B;  /* MOV r64, r/m64 */
        buf[pos++] = _modrm(0, REG_CODE(dst), 4);  /* mod=0, rm=4 = SIB follows */
        buf[pos++] = _sib(REG_CODE(base_reg), REG_CODE(idx_reg), 3);  /* scale=3 (8), index, base */
        return tb_emit(&ctx->tb, buf, pos);
    }

    case IR_OPCODE_SET_ELEM: {
        /* SET_ELEM: arr[idx] = val
         * mov [base + idx*8], val
         * REX.W 89 /r with SIB */
        int base_reg = operand_reg(&inst->operands[0]);
        int idx_reg = operand_reg(&inst->operands[1]);
        int val_reg = operand_reg(&inst->operands[2]);
        uint8_t buf[4];
        int pos = 0;
        int rex = REX_W | (REG_REX(val_reg) ? REX_R : 0);
        if (REG_REX(base_reg)) rex |= REX_B;
        if (REG_REX(idx_reg)) rex |= REX_X;
        buf[pos++] = rex | REX;
        buf[pos++] = 0x89;  /* MOV r/m64, r64 */
        buf[pos++] = _modrm(0, REG_CODE(val_reg), 4);  /* mod=0, rm=4 = SIB */
        buf[pos++] = _sib(REG_CODE(base_reg), REG_CODE(idx_reg), 3);
        return tb_emit(&ctx->tb, buf, pos);
    }

    case IR_OPCODE_DIV: {
        /* Signed division: idiv rm divides RDX:RAX by rm.
         * Result (quotient) in RAX, remainder in RDX.
         * idiv clobbers both RAX and RDX. cqto also clobbers RDX.
         * Save both RAX and RDX via push, restore RDX after. */
        src0 = operand_reg_or_imm_scratch(ctx, &inst->operands[0], REG_R10);
        src1 = operand_reg_or_imm_scratch(ctx, &inst->operands[1], REG_R11);
        /* Save RDX (clobbered by cqto) and RAX (clobbered by idiv) */
        tb_byte(&ctx->tb, 0x52);  /* push rdx */
        tb_byte(&ctx->tb, 0x50);  /* push rax */
        emit_mov_rr(&ctx->tb, REG_RAX, src0);
        /* cqo: REX.W 0x99 — sign-extend RAX into RDX:RAX */
        {
            uint8_t cqo[] = { REX_W | REX, 0x99 };
            tb_emit(&ctx->tb, cqo, 2);
        }
        /* idiv src1: REX.W 0xF7 /7 */
        {
            uint8_t buf[4];
            int pos = 0;
            int rex = REX_W | (REG_REX(src1) ? REX_B : 0) | REX;
            buf[pos++] = rex;
            buf[pos++] = 0xF7;
            buf[pos++] = _modrm(7, 3, REG_CODE(src1));
            tb_emit(&ctx->tb, buf, pos);
        }
        /* Save quotient to R10 (temp, callee-managed), then restore RAX/RDX,
         * then move result to dst. */
        emit_mov_rr(&ctx->tb, REG_R10, REG_RAX);
        tb_byte(&ctx->tb, 0x58);  /* pop rax */
        tb_byte(&ctx->tb, 0x5A);  /* pop rdx */
        emit_mov_rr(&ctx->tb, dst, REG_R10);
        return 0;
    }

    case IR_OPCODE_UDIV: {
        /* Unsigned division: div rm divides RDX:RAX by rm.
         * idiv clobbers both RAX and RDX. Use push/pop to save RDX. */
        src0 = operand_reg_or_imm_scratch(ctx, &inst->operands[0], REG_R10);
        src1 = operand_reg_or_imm_scratch(ctx, &inst->operands[1], REG_R11);
        if (dst != REG_RDX) {
            tb_byte(&ctx->tb, 0x52);  /* push rdx */
        }
        emit_mov_rr(&ctx->tb, REG_RAX, src0);
        /* xor rdx, rdx */
        emit_rr(&ctx->tb, 0x31, REG_RDX, REG_RDX, 1);
        /* div src1: REX.W 0xF7 /6 */
        {
            uint8_t buf[4];
            int pos = 0;
            int rex = REX_W | (REG_REX(src1) ? REX_B : 0) | REX;
            buf[pos++] = rex;
            buf[pos++] = 0xF7;
            buf[pos++] = _modrm(6, 3, REG_CODE(src1));
            tb_emit(&ctx->tb, buf, pos);
        }
        if (dst != REG_RAX) emit_mov_rr(&ctx->tb, dst, REG_RAX);
        if (dst != REG_RDX) {
            tb_byte(&ctx->tb, 0x5A);  /* pop rdx */
        }
        return 0;
    }

    case IR_OPCODE_MOD: {
        /* Signed modulo: same as DIV but result is in RDX (remainder).
         * idiv clobbers both RAX (quotient) and RDX (remainder).
         * cqto also clobbers RDX before idiv runs.
         * Save both RAX and RDX via push, restore RAX after. */
        src0 = operand_reg_or_imm_scratch(ctx, &inst->operands[0], REG_R10);
        src1 = operand_reg_or_imm_scratch(ctx, &inst->operands[1], REG_R11);
        /* Save RDX (clobbered by cqto) and RAX (clobbered by idiv) */
        tb_byte(&ctx->tb, 0x52);  /* push rdx */
        tb_byte(&ctx->tb, 0x50);  /* push rax */
        emit_mov_rr(&ctx->tb, REG_RAX, src0);
        /* cqo */
        {
            uint8_t cqo[] = { REX_W | REX, 0x99 };
            tb_emit(&ctx->tb, cqo, 2);
        }
        /* idiv src1 */
        {
            uint8_t buf[4];
            int pos = 0;
            int rex = REX_W | (REG_REX(src1) ? REX_B : 0) | REX;
            buf[pos++] = rex;
            buf[pos++] = 0xF7;
            buf[pos++] = _modrm(7, 3, REG_CODE(src1));
            tb_emit(&ctx->tb, buf, pos);
        }
        /* Save remainder to R10 (temp), then restore RAX/RDX,
         * then move result to dst. */
        emit_mov_rr(&ctx->tb, REG_R10, REG_RDX);
        tb_byte(&ctx->tb, 0x58);  /* pop rax */
        tb_byte(&ctx->tb, 0x5A);  /* pop rdx */
        emit_mov_rr(&ctx->tb, dst, REG_R10);
        return 0;
    }

    case IR_OPCODE_GET_FIELD: {
        src0 = operand_reg_or_imm_scratch(ctx, &inst->operands[0], REG_R10);
        int field_idx = 0;
        if (inst->noperands > 1 && inst->operands[1].type == IR_OPERAND_IMM) {
            int ok;
            field_idx = (int)operand_imm(&inst->operands[1], &ok);
            if (!ok) field_idx = 0;
        }
        int field_reg = src0 + field_idx;
        if (dst != field_reg) {
            emit_mov_rr(&ctx->tb, dst, field_reg);
        }
        return 0;
    }

    case IR_OPCODE_SET_FIELD: {
        src0 = operand_reg_or_imm_scratch(ctx, &inst->operands[0], REG_R10);
        int field_idx = 0;
        if (inst->noperands > 1 && inst->operands[1].type == IR_OPERAND_IMM) {
            int ok;
            field_idx = (int)operand_imm(&inst->operands[1], &ok);
            if (!ok) field_idx = 0;
        }
        int field_reg = src0 + field_idx;
        int val_reg = REG_RAX;
        if (inst->noperands > 2) {
            val_reg = operand_reg_or_imm_scratch(ctx, &inst->operands[2], REG_R11);
        }
        if (field_reg != val_reg) {
            emit_mov_rr(&ctx->tb, field_reg, val_reg);
        }
        return 0;
    }

    case IR_OPCODE_MAKE_STRUCT: {
        if (inst->noperands > 0) {
            src0 = operand_reg_or_imm_scratch(ctx, &inst->operands[0], REG_R10);
            if (dst != src0) {
                emit_mov_rr(&ctx->tb, dst, src0);
            }
        }
        return 0;
    }

    case IR_OPCODE_MAKE_ENUM: {
        if (inst->noperands > 0) {
            if (inst->operands[0].type == IR_OPERAND_IMM) {
                int ok;
                int64_t val = operand_imm(&inst->operands[0], &ok);
                emit_mov_imm(&ctx->tb, dst, val);
            } else {
                src0 = operand_reg_or_imm_scratch(ctx, &inst->operands[0], REG_R10);
                if (dst != src0) {
                    emit_mov_rr(&ctx->tb, dst, src0);
                }
            }
            /* Store data values in consecutive registers */
            for (int i = 1; i < inst->noperands - 1; i++) {
                int data_dst = dst + i;
                if (inst->operands[i].type == IR_OPERAND_IMM) {
                    int ok;
                    int64_t val = operand_imm(&inst->operands[i], &ok);
                    emit_mov_imm(&ctx->tb, data_dst, val);
                } else {
                    int src = operand_reg_or_imm_scratch(ctx, &inst->operands[i], REG_R11);
                    if (data_dst != src) {
                        emit_mov_rr(&ctx->tb, data_dst, src);
                    }
                }
            }
        }
        return 0;
    }

    case IR_OPCODE_CHECK_VARIANT: {
        src0 = operand_reg_or_imm_scratch(ctx, &inst->operands[0], REG_R10);
        src1 = operand_reg_or_imm_scratch(ctx, &inst->operands[1], REG_R11);
        emit_cmp_rr(&ctx->tb, src0, src1);
        return emit_setcc_movzx(&ctx->tb, 0x84, dst);  /* setz */
    }

    case IR_OPCODE_EXTRACT_VARIANT: {
        src0 = operand_reg_or_imm_scratch(ctx, &inst->operands[0], REG_R10);
        int field_idx = 0;
        if (inst->noperands > 1 && inst->operands[1].type == IR_OPERAND_IMM) {
            int ok;
            field_idx = (int)operand_imm(&inst->operands[1], &ok);
            if (!ok) field_idx = 0;
        }
        int data_reg = src0 + 1 + field_idx;
        if (dst != data_reg) {
            emit_mov_rr(&ctx->tb, dst, data_reg);
        }
        return 0;
    }

    /* Unhandled opcodes -- emit NOP for now */
    case IR_OPCODE_PHI:
    case IR_OPCODE_SWITCH:
    case IR_OPCODE_MEMCPY:
    case IR_OPCODE_UREM:
    case IR_OPCODE_CAST:
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

    /* Append string data to text section */
    for (int i = 0; i < ctx.code->strings.n; i++) {
        ctx.code->strings.items[i].offset = ctx.tb.size;
        size_t slen = strlen(ctx.code->strings.items[i].str) + 1;
        tb_emit(&ctx.tb, (uint8_t*)ctx.code->strings.items[i].str, slen);
    }

    /* Patch LEA instructions with string offsets (RIP-relative) */
    for (int i = 0; i < ctx.str_patches.count; i++) {
        size_t lea_off = ctx.str_patches.lea_off[i];
        int sid = ctx.str_patches.str_idx[i];
        size_t str_off = ctx.code->strings.items[sid].offset;
        /* RIP-relative: disp32 = target - (lea_off + 4) */
        int32_t disp = (int32_t)(str_off - (lea_off + 4));
        memcpy(ctx.tb.buf + lea_off, &disp, 4);
    }

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
