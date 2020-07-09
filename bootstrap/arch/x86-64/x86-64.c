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

/*
 * x86-64 registers
 */
typedef enum {
    REG_UNKNOWN = -1,
    /* IP */
    REG_EIP = 0,
    REG_RIP,
    /* AX */
    REG_AL,
    REG_AH,
    REG_AX,
    REG_EAX,
    REG_RAX,
    /* CX */
    REG_CL,
    REG_CH,
    REG_CX,
    REG_ECX,
    REG_RCX,
    /* DX */
    REG_DL,
    REG_DH,
    REG_DX,
    REG_EDX,
    REG_RDX,
    /* BX */
    REG_BL,
    REG_BH,
    REG_BX,
    REG_EBX,
    REG_RBX,
    /* SP */
    REG_SPL,
    REG_SP,
    REG_ESP,
    REG_RSP,
    /* BP */
    REG_BPL,
    REG_BP,
    REG_EBP,
    REG_RBP,
    /* SI */
    REG_SIL,
    REG_SI,
    REG_ESI,
    REG_RSI,
    /* DI */
    REG_DIL,
    REG_DI,
    REG_EDI,
    REG_RDI,
    /* R8 */
    REG_R8L,
    REG_R8W,
    REG_R8D,
    REG_R8,
    /* R9 */
    REG_R9L,
    REG_R9W,
    REG_R9D,
    REG_R9,
    /* R10 */
    REG_R10L,
    REG_R10W,
    REG_R10D,
    REG_R10,
    /* R11 */
    REG_R11L,
    REG_R11W,
    REG_R11D,
    REG_R11,
    /* R12 */
    REG_R12L,
    REG_R12W,
    REG_R12D,
    REG_R12,
    /* R13 */
    REG_R13L,
    REG_R13W,
    REG_R13D,
    REG_R13,
    /* R14 */
    REG_R14L,
    REG_R14W,
    REG_R14D,
    REG_R14,
    /* R15 */
    REG_R15L,
    REG_R15W,
    REG_R15D,
    REG_R15,
    /* Segment registers */
    REG_CS,
    REG_DS,
    REG_ES,
    REG_FS,
    REG_GS,
    /* Flags */
    REG_FLAGS,
    REG_EFLAGS,
    REG_RFLAGS,
    /* MM */
    REG_MM0,
    REG_MM1,
    REG_MM2,
    REG_MM3,
    REG_MM4,
    REG_MM5,
    REG_MM6,
    REG_MM7,
    /* XMM */
    REG_XMM0,
    REG_XMM1,
    REG_XMM2,
    REG_XMM3,
    REG_XMM4,
    REG_XMM5,
    REG_XMM6,
    REG_XMM7,
} x86_64_reg_t;

typedef enum {
    REX_NE,
    REX_NONE,
    REX_TRUE,
    REX_FALSE,
} x86_64_rex_t;

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
_encode_modrm(uint8_t *modrm, int reg, int mod, int rm)
{
    if ( reg < 0 || reg > 7 || mod < 0 || mod > 3 || rm < 0 || rm > 7 ) {
        return -1;
    }
    *modrm = (mod << 6) | (reg << 3) | rm;

    return 0;
}

/*
 * Encode SIB
 */
static int
_encode_sib(uint8_t *sib, int base, int idx, int ss)
{
    if ( base < 0 || base > 7 || idx < 0 || idx > 7 || ss < 0 || ss > 3 ) {
        return -1;
    }
    *sib = (ss << 6) | (idx << 3) | base;

    return 0;
}

/*
 * Convert register to the code
 */
static int
_reg2code(uint8_t *code, x86_64_rex_t *rex, int *size,
          x86_64_reg_t reg)
{
    /* Code & REX prefix */
    switch ( reg ) {
    case REG_AL:
    case REG_AX:
    case REG_EAX:
    case REG_RAX:
        *code = 0;
        *rex = REX_NONE;
        break;
    case REG_CL:
    case REG_CX:
    case REG_ECX:
    case REG_RCX:
        *code = 1;
        *rex = REX_NONE;
        break;
    case REG_DL:
    case REG_DX:
    case REG_EDX:
    case REG_RDX:
        *code = 2;
        *rex = REX_NONE;
        break;
    case REG_BL:
    case REG_BX:
    case REG_EBX:
    case REG_RBX:
        *code = 3;
        *rex = REX_NONE;
        break;
    case REG_AH:
        *code = 4;
        *rex = REX_NE;
        break;
    case REG_SP:
    case REG_ESP:
    case REG_RSP:
        *code = 4;
        *rex = REX_NONE;
        break;
    case REG_SPL:
        *code = 4;
        *rex = REX_FALSE;
        break;
    case REG_CH:
        *code = 5;
        *rex = REX_NE;
        break;
    case REG_BP:
    case REG_EBP:
    case REG_RBP:
        *code = 5;
        *rex = REX_NONE;
        break;
    case REG_BPL:
        *code = 5;
        *rex = REX_FALSE;
        break;
    case REG_DH:
        *code = 6;
        *rex = REX_NE;
        break;
    case REG_SI:
    case REG_ESI:
    case REG_RSI:
        *code = 6;
        *rex = REX_NONE;
        break;
    case REG_SIL:
        *code = 6;
        *rex = REX_FALSE;
        break;
    case REG_BH:
        *code = 7;
        *rex = REX_NE;
        break;
    case REG_DI:
    case REG_EDI:
    case REG_RDI:
        *code = 7;
        *rex = REX_NONE;
        break;
    case REG_DIL:
        *code = 7;
        *rex = REX_FALSE;
        break;
    case REG_R8L:
    case REG_R8W:
    case REG_R8D:
    case REG_R8:
        *code = 0;
        *rex = REX_TRUE;
        break;
    case REG_R9L:
    case REG_R9W:
    case REG_R9D:
    case REG_R9:
        *code = 1;
        *rex = REX_TRUE;
        break;
    case REG_R10L:
    case REG_R10W:
    case REG_R10D:
    case REG_R10:
        *code = 2;
        *rex = REX_TRUE;
        break;
    case REG_R11L:
    case REG_R11W:
    case REG_R11D:
    case REG_R11:
        *code = 3;
        *rex = REX_TRUE;
        break;
    case REG_R12L:
    case REG_R12W:
    case REG_R12D:
    case REG_R12:
        *code = 4;
        *rex = REX_TRUE;
        break;
    case REG_R13L:
    case REG_R13W:
    case REG_R13D:
    case REG_R13:
        *code = 5;
        *rex = REX_TRUE;
        break;
    case REG_R14L:
    case REG_R14W:
    case REG_R14D:
    case REG_R14:
        *code = 6;
        *rex = REX_TRUE;
        break;
    case REG_R15L:
    case REG_R15W:
    case REG_R15D:
    case REG_R15:
        *code = 7;
        *rex = REX_TRUE;
        break;
    default:
        return -1;
    }

    /* Size */
    switch ( reg ) {
    case REG_AL:
    case REG_AH:
    case REG_CL:
    case REG_CH:
    case REG_DL:
    case REG_DH:
    case REG_BL:
    case REG_BH:
    case REG_SPL:
    case REG_BPL:
    case REG_SIL:
    case REG_DIL:
    case REG_R8L:
    case REG_R9L:
    case REG_R10L:
    case REG_R11L:
    case REG_R12L:
    case REG_R13L:
    case REG_R14L:
    case REG_R15L:
        *size = 1;
        break;
    case REG_AX:
    case REG_CX:
    case REG_DX:
    case REG_BX:
    case REG_SP:
    case REG_BP:
    case REG_SI:
    case REG_DI:
    case REG_R8W:
    case REG_R9W:
    case REG_R10W:
    case REG_R11W:
    case REG_R12W:
    case REG_R13W:
    case REG_R14W:
    case REG_R15W:
        *size = 2;
        break;
    case REG_EAX:
    case REG_ECX:
    case REG_EDX:
    case REG_EBX:
    case REG_ESP:
    case REG_EBP:
    case REG_ESI:
    case REG_EDI:
    case REG_R8D:
    case REG_R9D:
    case REG_R10D:
    case REG_R11D:
    case REG_R12D:
    case REG_R13D:
    case REG_R14D:
    case REG_R15D:
        *size = 4;
        break;
    case REG_RAX:
    case REG_RCX:
    case REG_RDX:
    case REG_RBX:
    case REG_RSP:
    case REG_RBP:
    case REG_RSI:
    case REG_RDI:
    case REG_R8:
    case REG_R9:
    case REG_R10:
    case REG_R11:
    case REG_R12:
    case REG_R13:
    case REG_R14:
    case REG_R15:
        *size = 8;;
        break;
    default:
        return -1;
    }

    return 0;
}


/*
 * /digit: r/m
 */
static int
_encode_digit(uint8_t *code, uint8_t digit, uint8_t mod, uint8_t rm)
{
    return _encode_modrm(code, digit, mod, rm);
}

/*
 * /r: register + r/m
 */
static int
_encode_r(uint8_t *code, uint8_t reg, uint8_t mod, uint8_t rm)
{
    return _encode_modrm(code, reg, mod, rm);
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
 * RM
 */
int
encode_rm(uint8_t *code, operand_t op1, operand_t op2)
{
    if ( op1.type != OPERAND_REG ) {
        return -1;
    }
    if ( op2.type != OPERAND_REG && op2.type != OPERAND_MEM ) {
        return -1;
    }

    if ( op2.type == OPERAND_MEM ) {
        /* Memory */
        if ( op2.u.mem.disp <= 0x7f && op2.u.mem.disp >= -0x80 ) {
            /* 1-byte displacement */
        } else {
            /* 4-byte displacement */
        }
    }

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
