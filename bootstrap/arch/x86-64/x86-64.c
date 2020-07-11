/*_
 * Copyright (c) 2020 Hirochika Asai <asai@jar.jp>
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdint.h>
#include <string.h>

#define REG_CODE(r)     ((r) & 0x7)
#define REG_REX(r)      (((r) >> 3) & 0x1)
#define REG_REX0(r)     (((r) >> 4) & 0x1)
#define REG_NE(r)       (((r) >> 5) & 0x1)
#define REG_SIZE(r)     (((r) >> 16) & 0xffff)

#define REG_ENCODE(code, rex, rex0, ne, size)                           \
    ((code) | ((rex) << 3) | ((rex0) << 4) | ((ne) << 5) | ((size) << 16))


/*
 * x86-64 registers
 */
typedef enum {
    REG_NONE = 0,
    /* IP */
    //REG_EIP,
    //REG_RIP,
    /* AX */
    REG_AL = REG_ENCODE(0, 0, 0, 0, 8),
    REG_AH = REG_ENCODE(4, 0, 0, 1, 8),
    REG_AX = REG_ENCODE(0, 0, 0, 0, 16),
    REG_EAX = REG_ENCODE(0, 0, 0, 0, 32),
    REG_RAX = REG_ENCODE(0, 0, 0, 0, 64),
    /* CX */
    REG_CL = REG_ENCODE(1, 0, 0, 0, 8),
    REG_CH = REG_ENCODE(5, 0, 0, 1, 8),
    REG_CX = REG_ENCODE(1, 0, 0, 0, 16),
    REG_ECX = REG_ENCODE(1, 0, 0, 0, 32),
    REG_RCX = REG_ENCODE(1, 0, 0, 0, 64),
    /* DX */
    REG_DL = REG_ENCODE(2, 0, 0, 0, 8),
    REG_DH = REG_ENCODE(6, 0, 0, 1, 8),
    REG_DX = REG_ENCODE(2, 0, 0, 0, 16),
    REG_EDX = REG_ENCODE(2, 0, 0, 0, 32),
    REG_RDX = REG_ENCODE(2, 0, 0, 0, 64),
    /* BX */
    REG_BL = REG_ENCODE(3, 0, 0, 0, 8),
    REG_BH = REG_ENCODE(7, 0, 0, 1, 8),
    REG_BX = REG_ENCODE(3, 0, 0, 0, 16),
    REG_EBX = REG_ENCODE(3, 0, 0, 0, 32),
    REG_RBX = REG_ENCODE(3, 0, 0, 0, 64),
    /* SP */
    REG_SPL = REG_ENCODE(4, 0, 1, 0, 8),
    REG_SP = REG_ENCODE(4, 0, 0, 0, 16),
    REG_ESP = REG_ENCODE(4, 0, 0, 0, 32),
    REG_RSP = REG_ENCODE(4, 0, 0, 0, 64),
    /* BP */
    REG_BPL = REG_ENCODE(5, 0, 1, 0, 8),
    REG_BP = REG_ENCODE(5, 0, 0, 0, 16),
    REG_EBP = REG_ENCODE(5, 0, 0, 0, 32),
    REG_RBP = REG_ENCODE(5, 0, 0, 0, 64),
    /* SI */
    REG_SIL = REG_ENCODE(6, 0, 1, 0, 8),
    REG_SI = REG_ENCODE(6, 0, 0, 0, 16),
    REG_ESI = REG_ENCODE(6, 0, 0, 0, 32),
    REG_RSI = REG_ENCODE(6, 0, 0, 0, 64),
    /* DI */
    REG_DIL = REG_ENCODE(7, 0, 1, 0, 8),
    REG_DI = REG_ENCODE(7, 0, 0, 0, 16),
    REG_EDI = REG_ENCODE(7, 0, 0, 0, 32),
    REG_RDI = REG_ENCODE(7, 0, 0, 0, 64),
    /* R8 */
    REG_R8L = REG_ENCODE(0, 1, 0, 0, 8),
    REG_R8W = REG_ENCODE(0, 1, 0, 0, 16),
    REG_R8D = REG_ENCODE(0, 1, 0, 0, 32),
    REG_R8 = REG_ENCODE(0, 1, 0, 0, 64),
    /* R9 */
    REG_R9L = REG_ENCODE(1, 1, 0, 0, 8),
    REG_R9W = REG_ENCODE(1, 1, 0, 0, 16),
    REG_R9D = REG_ENCODE(1, 1, 0, 0, 32),
    REG_R9 = REG_ENCODE(1, 1, 0, 0, 64),
    /* R10 */
    REG_R10L = REG_ENCODE(2, 1, 0, 0, 8),
    REG_R10W = REG_ENCODE(2, 1, 0, 0, 16),
    REG_R10D = REG_ENCODE(2, 1, 0, 0, 32),
    REG_R10 = REG_ENCODE(2, 1, 0, 0, 64),
    /* R11 */
    REG_R11L = REG_ENCODE(3, 1, 0, 0, 8),
    REG_R11W = REG_ENCODE(3, 1, 0, 0, 16),
    REG_R11D = REG_ENCODE(3, 1, 0, 0, 32),
    REG_R11 = REG_ENCODE(3, 1, 0, 0, 64),
    /* R12 */
    REG_R12L = REG_ENCODE(4, 1, 0, 0, 8),
    REG_R12W = REG_ENCODE(4, 1, 0, 0, 16),
    REG_R12D = REG_ENCODE(4, 1, 0, 0, 32),
    REG_R12 = REG_ENCODE(4, 1, 0, 0, 64),
    /* R13 */
    REG_R13L = REG_ENCODE(5, 1, 0, 0, 8),
    REG_R13W = REG_ENCODE(5, 1, 0, 0, 16),
    REG_R13D = REG_ENCODE(5, 1, 0, 0, 32),
    REG_R13 = REG_ENCODE(5, 1, 0, 0, 64),
    /* R14 */
    REG_R14L = REG_ENCODE(6, 1, 0, 0, 8),
    REG_R14W = REG_ENCODE(6, 1, 0, 0, 16),
    REG_R14D = REG_ENCODE(6, 1, 0, 0, 32),
    REG_R14 = REG_ENCODE(6, 1, 0, 0, 64),
    /* R15 */
    REG_R15L = REG_ENCODE(7, 1, 0, 0, 8),
    REG_R15W = REG_ENCODE(7, 1, 0, 0, 16),
    REG_R15D = REG_ENCODE(7, 1, 0, 0, 32),
    REG_R15 = REG_ENCODE(7, 1, 0, 0, 64),
    /* Segment registers */
    REG_CS,
    REG_DS,
    REG_ES,
    REG_FS,
    REG_GS,
    /* Flags */
    //REG_FLAGS,
    //REG_EFLAGS,
    //REG_RFLAGS,
    /* ST */
    REG_ST0 = REG_ENCODE(0, 0, 0, 0, 80),
    REG_ST1 = REG_ENCODE(1, 0, 0, 0, 80),
    REG_ST2 = REG_ENCODE(2, 0, 0, 0, 80),
    REG_ST3 = REG_ENCODE(3, 0, 0, 0, 80),
    REG_ST4 = REG_ENCODE(4, 0, 0, 0, 80),
    REG_ST5 = REG_ENCODE(5, 0, 0, 0, 80),
    REG_ST6 = REG_ENCODE(6, 0, 0, 0, 80),
    REG_ST7 = REG_ENCODE(7, 0, 0, 0, 80),
    /* MM */
    REG_MM0 = REG_ENCODE(0, 0, 0, 0, 64),
    REG_MM1 = REG_ENCODE(1, 0, 0, 0, 64),
    REG_MM2 = REG_ENCODE(2, 0, 0, 0, 64),
    REG_MM3 = REG_ENCODE(3, 0, 0, 0, 64),
    REG_MM4 = REG_ENCODE(4, 0, 0, 0, 64),
    REG_MM5 = REG_ENCODE(5, 0, 0, 0, 64),
    REG_MM6 = REG_ENCODE(6, 0, 0, 0, 64),
    REG_MM7 = REG_ENCODE(7, 0, 0, 0, 64),
    /* XMM */
    REG_XMM0 = REG_ENCODE(0, 0, 0, 0, 128),
    REG_XMM1 = REG_ENCODE(1, 0, 0, 0, 128),
    REG_XMM2 = REG_ENCODE(2, 0, 0, 0, 128),
    REG_XMM3 = REG_ENCODE(3, 0, 0, 0, 128),
    REG_XMM4 = REG_ENCODE(4, 0, 0, 0, 128),
    REG_XMM5 = REG_ENCODE(5, 0, 0, 0, 128),
    REG_XMM6 = REG_ENCODE(6, 0, 0, 0, 128),
    REG_XMM7 = REG_ENCODE(7, 0, 0, 0, 128),
    REG_XMM8 = REG_ENCODE(0, 1, 0, 0, 128),
    REG_XMM9 = REG_ENCODE(1, 1, 0, 0, 128),
    REG_XMM10 = REG_ENCODE(2, 1, 0, 0, 128),
    REG_XMM11 = REG_ENCODE(3, 1, 0, 0, 128),
    REG_XMM12 = REG_ENCODE(4, 1, 0, 0, 128),
    REG_XMM13 = REG_ENCODE(5, 1, 0, 0, 128),
    REG_XMM14 = REG_ENCODE(6, 1, 0, 0, 128),
    REG_XMM15 = REG_ENCODE(7, 1, 0, 0, 128),
    /* YMM */
    REG_YMM0 = REG_ENCODE(0, 0, 0, 0, 256),
    REG_YMM1 = REG_ENCODE(1, 0, 0, 0, 256),
    REG_YMM2 = REG_ENCODE(2, 0, 0, 0, 256),
    REG_YMM3 = REG_ENCODE(3, 0, 0, 0, 256),
    REG_YMM4 = REG_ENCODE(4, 0, 0, 0, 256),
    REG_YMM5 = REG_ENCODE(5, 0, 0, 0, 256),
    REG_YMM6 = REG_ENCODE(6, 0, 0, 0, 256),
    REG_YMM7 = REG_ENCODE(7, 0, 0, 0, 256),
    REG_YMM8 = REG_ENCODE(0, 1, 0, 0, 256),
    REG_YMM9 = REG_ENCODE(1, 1, 0, 0, 256),
    REG_YMM10 = REG_ENCODE(2, 1, 0, 0, 256),
    REG_YMM11 = REG_ENCODE(3, 1, 0, 0, 256),
    REG_YMM12 = REG_ENCODE(4, 1, 0, 0, 256),
    REG_YMM13 = REG_ENCODE(5, 1, 0, 0, 256),
    REG_YMM14 = REG_ENCODE(6, 1, 0, 0, 256),
    REG_YMM15 = REG_ENCODE(7, 1, 0, 0, 256),
} x86_64_reg_t;

/*     return (1<<6) | (w<<3) | (r<<2) | (x<<1) | b; */
#define REX             (1<<6)
#define REX_W           (1<<3)
#define REX_R           (1<<2)
#define REX_X           (1<<1)
#define REX_B           (1)

/* Group 1 */
#define LOCK            0xf0
#define REPNE           0xf2
#define REPNZ           REPNE
#define REP             0xf3
#define REPE            REP
#define REPZ            REP

/* Group 2 */
#define OVERRIDE_CS     0x2e
#define OVERRIDE_SS     0x36
#define OVERRIDE_DS     0x3e
#define OVERRIDE_ES     0x26
#define OVERRIDE_FS     0x64
#define OVERRIDE_GS     0x65
#define BRANCH_NOT_TAKEN    0x2e /* only with jcc */
#define BRANCH_TAKEN    0x3e    /* only with jcc */

/* Group 3 */
#define OVERRIDE_OPERAND_SIZE   0x66
/* Group 4 */
#define OVERRIDE_ADDR_SIZE  0x67

typedef struct {
    int l;
    uint8_t a[32];
} instr_t;

typedef enum {
    OPERAND_REG,
    OPERAND_MEM,
    OPERAND_IMM,
} operand_type_t;

typedef struct {
    int reg;
    int sindex;
    int scale;
    int32_t disp;
} operand_mem_t;

typedef struct {
    operand_type_t type;
    union {
        int reg;
        operand_mem_t mem;
        uint32_t imm;
    } u;
} operand_t;

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
 * /digit: r/m
 */
static int
_encode_digit(uint8_t *code, uint8_t digit, uint8_t mod, uint8_t rm)
{
    int ret;
    ret = _modrm(digit, mod, rm);
    if ( ret < 0 ) {
        return -1;
    }
    *code = ret;

    return 0;
}

/*
 * /r: register + r/m
 */
static int
_encode_r(uint8_t *code, uint8_t reg, uint8_t mod, uint8_t rm)
{
    int ret;
    ret = _modrm(reg, mod, rm);
    if ( ret < 0 ) {
        return -1;
    }
    *code = ret;

    return 0;
}

/*
 * cb,cw,cd,cp,co,ct
 */
static int
_encode_cb(uint8_t *code, uint8_t c)
{
    *code = c;
    return 0;
}
static int
_encode_cw(uint8_t *code, uint8_t *c)
{
    memcpy(code, c, 2);
    return 0;
}
static int
_encode_cd(uint8_t *code, uint8_t *c)
{
    memcpy(code, c, 4);
    return 0;
}
static int
_encode_cp(uint8_t *code, uint8_t *c)
{
    memcpy(code, c, 6);
    return 0;
}
static int
_encode_co(uint8_t *code, uint8_t *c)
{
    memcpy(code, c, 8);
    return 0;
}
static int
_encode_ct(uint8_t *code, uint8_t *c)
{
    memcpy(code, c, 10);
    return 0;
}

/*
 * ib,iw,id,io
 */
static int
_encode_ib(uint8_t *code, uint8_t i)
{
    *code = i;
    return 0;
}
static int
_encode_iw(uint8_t *code, uint16_t i)
{
    *(uint16_t *)code = i;
    return 0;
}
static int
_encode_id(uint8_t *code, uint32_t i)
{
    *(uint32_t *)code = i;
    return 0;
}
static int
_encode_io(uint8_t *code, uint64_t i)
{
    *(uint64_t *)code = i;
    return 0;
}

/*
 * +rb,+rw,+rd,+ro
 * +i
 */
static int
_encode_rx(uint8_t *code, uint8_t op, uint8_t reg)
{
    *code = op + reg;
    return 0;
}

/*
 * REX prefix
 */
static int
_rex(int rex, x86_64_reg_t r, x86_64_reg_t rmbase, x86_64_reg_t s)
{
    /* REX.R: ModR/M reg */
    /* REX.X: SIB index */
    /* REX.B: ModR/M r/m or SIB base */
    rex = 0;
    rex |= REG_REX(r) ? REX_R : 0;
    rex |= REG_REX(s) ? REX_X : 0;
    rex |= REG_REX(rmbase) ? REX_B : 0;

    if ( rex ) {
        if ( REG_NE(r) || REG_NE(s) || REG_NE(rmbase) ) {
            /* Not encodable */
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
    int ret;
    int modrm;

    /* Resolve rex */
    ret = _rex(*rex, reg, rm, REG_NONE);
    if ( ret < 0 ) {
        /* Not encodable */
        return -1;
    }

    modrm = _modrm(REG_CODE(reg), mod, REG_CODE(rm));
    if ( modrm < 0 ) {
        return -1;
    }
    *code = ret;
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
    int ret;
    int modrm;
    int sib;

    /* Resolve rex */
    ret = _rex(*rex, reg, base, idx);
    if ( ret < 0 ) {
        /* Not encodable */
        return -1;
    }

    modrm = _modrm(REG_CODE(reg), mod, 4);
    if ( modrm < 0 ) {
        return -1;
    }

    sib = _sib(REG_CODE(base), REG_CODE(idx), ss);
    if ( sib < 0 ) {
        return -1;
    }
    code[0] = modrm;
    code[1] = sib;
    *rex = ret;

    return 2;
}

/*
 * RR
 */
static int
_encode_rm_reg(uint8_t *code, int *rex, operand_t op1, operand_t op2)
{
    int r;
    int rm;
    int ret;

    /* Check the operand type */
    if ( op1.type != OPERAND_REG || op2.type != OPERAND_REG ) {
        return -1;
    }

    /* First operand */
    r = op1.u.reg;

    /* Second operand */
    rm = op2.u.reg;

    /* Encode registers */
    ret = _encode_modrm(code, rex, 3, r, rm);
    if ( ret < 0 ) {
        return -1;
    }

    return ret;
}

/*
 * Encode memory
 */
static int
_encode_rm_mem(uint8_t *code, int *rex, operand_t op1, operand_t op2)
{
    int size;
    int ss;
    int mod;
    int ret;

    /* Check the operand type */
    if ( op1.type != OPERAND_REG || op2.type != OPERAND_MEM ) {
        return -1;
    }

    /* Resolve SS */
    switch ( op2.u.mem.scale ) {
    case 1:
        ss = 0;
        break;
    case 2:
        ss = 1;
        break;
    case 4:
        ss = 2;
        break;
    case 8:
        ss = 3;
        break;
    default:
        /* Invalid scale */
        return -1;
    }

    /* Check the displacement */
    if ( op2.u.mem.disp == 0 ) {
        /* No displacement */
        mod = 0;
    } else if ( op2.u.mem.disp <= 0x7f && op2.u.mem.disp >= -0x80 ) {
        /* 1-byte displacement */
        mod = 1;
    } else {
        /* 4-byte displacement */
        mod = 2;
    }

    /* Check if SIB is needed */
    if ( (op2.u.mem.sindex != REG_NONE && op2.u.mem.reg != REG_NONE) || ss )  {
        /* Encode SIB */
        ret = _encode_modrm_sib(code, rex, mod, op1.u.reg,
                                op2.u.mem.reg, op2.u.mem.sindex, ss);
        if ( ret < 0 ) {
            return -1;
        }
        size = ret;
    } else if ( op2.u.mem.sindex == REG_NONE && op2.u.mem.reg == REG_NONE ) {
        /* Displacement only (disp32 for any displacement values) */
        ret = _encode_modrm(code, rex, op1.u.reg, REG_NONE, 5);
        if ( ret < 0 ) {
            return -1;
        }
        size = ret;
        /* Force to use disp32 */
        mod = 2;
    } else {
        /* ss=0, then ModR/M is used without SIB */
        ret = _encode_modrm(code, rex, mod, op1.u.reg, op2.u.mem.sindex);
        if ( ret < 0 ) {
            return -1;
        }
        size = ret;
    }

    /* Add displacement */
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
        ;
    }

    return size;
}

/*
 * RM
 */
int
encode_rm(uint8_t *code, int *rex, operand_t op1, operand_t op2)
{
    /* Check the operand type */
    if ( op1.type != OPERAND_REG ) {
        return -1;
    }
    if ( op2.type != OPERAND_REG && op2.type != OPERAND_MEM ) {
        return -1;
    }

    /* Check the second operand */
    if ( op2.type == OPERAND_REG ) {
        return _encode_rm_reg(code, rex, op1, op2);
    } else if ( op2.type == OPERAND_MEM ) {
        return _encode_rm_mem(code, rex, op1, op2);
    }

    return -1;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
