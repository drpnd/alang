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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include "instr.h"

#ifndef BASEDIR
#define BASEDIR ""
#endif

#define OPCODE_REXW         0x101
#define OPCODE_DIGIT_PREFIX 0x200
#define OPCODE_REGISTER     0x300
#define OPCODE_CB           0x401
#define OPCODE_CW           0x402
#define OPCODE_CD           0x404
#define OPCODE_CP           0x406
#define OPCODE_CO           0x408
#define OPCODE_CT           0x40a
#define OPCODE_IB           0x501
#define OPCODE_IW           0x502
#define OPCODE_ID           0x504
#define OPCODE_IO           0x508
#define OPCODE_RB           0x601
#define OPCODE_RW           0x602
#define OPCODE_RD           0x604
#define OPCODE_RO           0x608
#define OPCODE_ST_PREFIX    0x700

#define OPERAND_REL8        0x101
#define OPERAND_REL16       0x102
#define OPERAND_REL32       0x104
#define OPERAND_REL64       0x108
#define OPERAND_PTR16_16    0x202
#define OPERAND_PTR16_32    0x204
#define OPERAND_PTR16_64    0x208
#define OPERAND_R8          0x301
#define OPERAND_R8X         0x301
#define OPERAND_R16         0x302
#define OPERAND_R32         0x304
#define OPERAND_R64         0x308
#define OPERAND_IMM8        0x401
#define OPERAND_IMM16       0x402
#define OPERAND_IMM32       0x404
#define OPERAND_IMM64       0x408
#define OPERAND_RM8         0x501
#define OPERAND_RM8X        0x501
#define OPERAND_RM16        0x502
#define OPERAND_RM32        0x504
#define OPERAND_RM64        0x508
#define OPERAND_M           0x600
#define OPERAND_M8          0x601
#define OPERAND_M16         0x602
#define OPERAND_M32         0x604
#define OPERAND_M64         0x608
#define OPERAND_M128        0x610
#define OPERAND_M16_16      0x702
#define OPERAND_M16_32      0x704
#define OPERAND_M16_64      0x708
#define OPERAND_M16A16      0x802
#define OPERAND_M16A32      0x804
#define OPERAND_M16A64      0x808
#define OPERAND_M32A32      0x814
#define OPERAND_MOFFS8      0x901
#define OPERAND_MOFFS16     0x902
#define OPERAND_MOFFS32     0x904
#define OPERAND_MOFFS64     0x908
#define OPERAND_SREG        0xa00
#define OPERAND_M32FP       0xb04
#define OPERAND_M64FP       0xb08
#define OPERAND_M80FP       0xb0a
#define OPERAND_M16INT      0xc02
#define OPERAND_M32INT      0xc04
#define OPERAND_M64INT      0xc08
#define OPERAND_ST(i)       (0xd00 + (i))
#define OPERAND_MM          0xe00
#define OPERAND_MM_M32      0xe04
#define OPERAND_MM_M64      0xe08
#define OPERAND_XMM         0xf00
#define OPERAND_XMM_M32     0xf04
#define OPERAND_XMM_M64     0xf08
#define OPERAND_XMM_M128    0xf10
#define OPERAND_AL          0x1001
#define OPERAND_AX          0x1002
#define OPERAND_EAX         0x1004
#define OPERAND_RAX         0x1008

#define OPCODE_MAX_SIZE     16

/*
 * Encode type
 */
enum encode_type {
    ENCODE_M,
    ENCODE_RM,
    ENCODE_MR,
    ENCODE_OI,
    ENCODE_MI,
    ENCODE_D,
};
static __inline__ int
_operand_num_by_encode_type(enum encode_type enc)
{
    switch ( enc ) {
    case ENCODE_M:
    case ENCODE_D:
        return 1;
    case ENCODE_RM:
    case ENCODE_MR:
    case ENCODE_OI:
    case ENCODE_MI:
        return 2;
    }

    return -1;
}

/*
 * Opcode
 */
struct opcode {
    size_t size;
    int opcode[OPCODE_MAX_SIZE];
};

/*
 * Encode: M
 */
struct encode_m {
    int m;
};

/*
 * Encode: RM
 */
struct encode_rm {
    int r;
    int rm;
};

/*
 * Encode: MR
 */
struct encode_mr {
    int rm;
    int r;
};

/*
 * Encode: OI
 */
struct encode_oi {
    int r;
    int imm;
};

/*
 * Encode: MI
 */
struct encode_mi {
    int rm;
    int imm;
};

/*
 * Encode: D
 */
struct encode_d {
    int ptr;
};

/*
 * Encode
 */
struct encode {
    enum encode_type type;
    union {
        struct encode_m m;
        struct encode_rm rm;
        struct encode_mr mr;
        struct encode_oi oi;
        struct encode_mi mi;
        struct encode_d d;
    } u;
};

/*
 * Rule tuple
 */
struct rule {
    struct encode encode;
    struct opcode op;
    struct rule *next;
};

/*
 * Rules for a mnemonic
 */
struct mnemonic {
    char *mnemonic;
    struct rule *rules;
    struct mnemonic *next;
};

/*
 * Rules
 */
struct x86_64_instr_ruleset {
    struct mnemonic *mnemonics;
};

/*
 * Trim leading and trailing whitespaces
 */
static char *
_trim(char *s)
{
    char *ns;
    char *rs;

    rs = s;
    ns = s;

    /* Remove leading whitespaces */
    while ( *s ) {
        if ( isspace(*s) ) {
            s++;
        } else {
            break;
        }
    }

    /* Copy */
    while ( *s ) {
        *ns = *s;
        ns++;
        s++;
    }
    *ns = 0;

    /* Remove trailing whitespaces */
    ns--;
    while ( isspace(*ns) ) {
        *ns = '\0';
        ns--;
    }

    return rs;
}

/*
 * _ishexdigit -- check if the hexdecimal ascii character
 */
static int
_ishexdigit(int c)
{
    if ( '0' <= c && c <= '9' ) {
        return 1;
    } else if ( 'a' <= c && c <= 'f' ) {
        return 1;
    } else if ( 'A' <= c && c <= 'F' ) {
        return 1;
    } else {
        return 0;
    }
}

/*
 * _check_size
 */
static int
_check_size(int64_t val)
{
    if ( 0 == val ) {
        return 0;
    } else if ( val >= -0x80 && val < 0x80 ) {
        return 1;
    } else if ( val >= -0x8000 && val < 0x8000 ) {
        return 2;
    } else if ( val >= -0x80000000 && val < 0x80000000 ) {
        return 4;
    } else {
        return 8;
    }
}

/*
 * _free_rule
 */
static void
_free_rule(struct rule *rule)
{
    free(rule);
}

/*
 * _free_mnemonic
 */
static void
_free_mnemonic(struct mnemonic *mnemonic)
{
    struct rule *rule;
    struct rule *next;

    if ( mnemonic->mnemonic ) {
        free(mnemonic->mnemonic);
    }
    rule = mnemonic->rules;
    while ( NULL != rule ) {
        next = rule->next;
        _free_rule(rule);
        rule = next;
    }
    free(mnemonic);
}

/*
 * _parse_opcode_chunk -- parse an opcode chunk
 */
static int
_parse_opcode_chunk(const char *token)
{
    int ret;

    /* Hexdecimal */
    if ( 2 == strlen(token) ) {
        if ( _ishexdigit(token[0]) && _ishexdigit(token[1]) ) {
            return strtol(token, NULL, 16);
        }
    }

    if ( 0 == strcasecmp("W", token) ) {
        /* REX.W */
        return OPCODE_REXW;
    } else if ( '/' == *token ) {
        token++;
        if ( 'r' == *token ) {
            ret = OPCODE_REGISTER;
        } else if ( '0' <= *token && *token <= '7' ) {
            ret = OPCODE_DIGIT_PREFIX + (*token - '0');
        } else {
            return -1;
        }
        token++;
        if ( '\0' != *token ) {
            return -1;
        }
        return ret;
    } else if ( 0 == strcasecmp("cb", token) ) {
        return OPCODE_CB;
    } else if ( 0 == strcasecmp("cw", token) ) {
        return OPCODE_CW;
    } else if ( 0 == strcasecmp("cd", token) ) {
        return OPCODE_CD;
    } else if ( 0 == strcasecmp("cp", token) ) {
        return OPCODE_CP;
    } else if ( 0 == strcasecmp("co", token) ) {
        return OPCODE_CO;
    } else if ( 0 == strcasecmp("ct", token) ) {
        return OPCODE_CT;
    } else if ( 0 == strcasecmp("ib", token) ) {
        return OPCODE_IB;
    } else if ( 0 == strcasecmp("iw", token) ) {
        return OPCODE_IW;
    } else if ( 0 == strcasecmp("id", token) ) {
        return OPCODE_ID;
    } else if ( 0 == strcasecmp("io", token) ) {
        return OPCODE_IO;
    } else if ( '+' == *token ) {
        token++;
        if ( 0 == strcasecmp("rb", token ) ) {
            return OPCODE_RB;
        } else if ( 0 == strcasecmp("rw", token ) ) {
            return OPCODE_RW;
        } else if ( 0 == strcasecmp("rd", token ) ) {
            return OPCODE_RD;
        } else if ( 0 == strcasecmp("ro", token ) ) {
            return OPCODE_RO;
        } else if ( '0' <= *token && *token <= '7' ) {
            ret = OPCODE_ST_PREFIX + (*token - '0');
            token++;
            if ( '\0' != *token ) {
                return -1;
            }
            return ret;
        } else {
            return -1;
        }
    }

    return -1;
}

/*
 * _parse_opcode -- parse an opcode field
 */
static int
_parse_opcode(struct opcode *op, const char *opcode)
{
    char *tok;
    char *savedptr;
    char *s;
    int c;

    /* Copy the opcode */
    s = strdup(opcode);
    if ( NULL == s ) {
        return -1;
    }

    tok = strtok_r(s, " ", &savedptr);
    op->size = 0;
    while ( NULL != tok ) {
        c = _parse_opcode_chunk(tok);
        if ( c < 0 ) {
            return -1;
        } else {
            if ( op->size >= OPCODE_MAX_SIZE ) {
                /* Exceed the maximum opcode size */
                return -1;
            }
            op->opcode[op->size] = c;
            op->size++;
        }
        tok = strtok_r(NULL, " ", &savedptr);
    }

    return 0;
}

/*
 * _parse_encode_type -- parse the encode type of an token
 */
static int
_parse_encode_type(const char *token)
{
    if ( 0 == strcasecmp("M", token) ) {
        return ENCODE_M;
    } else if ( 0 == strcasecmp("RM", token) ) {
        return ENCODE_RM;
    } else if ( 0 == strcasecmp("MR", token) ) {
        return ENCODE_MR;
    } else if ( 0 == strcasecmp("OI", token) ) {
        return ENCODE_OI;
    } else if ( 0 == strcasecmp("MI", token) ) {
        return ENCODE_MI;
    } else if ( 0 == strcasecmp("D", token) ) {
        return ENCODE_D;
    }

    return -1;
}

/* 
 * _parse_operand_chunk -- parse an operand chunk
 */
static int
_parse_operand_chunk(const char *token)
{
    if ( 0 == strcasecmp("rel8", token) ) {
        /* rel8 */
        return OPERAND_REL8;
    } else if ( 0 == strcasecmp("rel16", token) ) {
        /* rel16 */
        return OPERAND_REL16;
    } else if ( 0 == strcasecmp("rel32", token) ) {
        /* rel32 */
        return OPERAND_REL32;
    } else if ( 0 == strcasecmp("rel64", token) ) {
        /* rel64 */
        return OPERAND_REL64;
    } else if ( 0 == strcasecmp("ptr16:16", token) ) {
        /* ptr16:16 */
        return OPERAND_PTR16_16;
    } else if ( 0 == strcasecmp("ptr16:32", token) ) {
        /* ptr16:32 */
        return OPERAND_PTR16_32;
    } else if ( 0 == strcasecmp("ptr16:64", token) ) {
        /* ptr16:64 */
        return OPERAND_PTR16_64;
    } else if ( 0 == strcasecmp("r8", token) ) {
        /* r8 */
        return OPERAND_R8;
    } else if ( 0 == strcasecmp("r8*", token) ) {
        /* r8* */
        return OPERAND_R8X;
    } else if ( 0 == strcasecmp("r16", token) ) {
        /* r16 */
        return OPERAND_R16;
    } else if ( 0 == strcasecmp("r32", token) ) {
        /* r32 */
        return OPERAND_R32;
    } else if ( 0 == strcasecmp("r64", token) ) {
        /* r64 */
        return OPERAND_R64;
    } else if ( 0 == strcasecmp("r/m8", token) ) {
        /* r/m8 */
        return OPERAND_RM8;
    } else if ( 0 == strcasecmp("r/m8*", token) ) {
        /* r/m8* */
        return OPERAND_RM8X;
    } else if ( 0 == strcasecmp("r/m16", token) ) {
        /* r/m16 */
        return OPERAND_RM16;
    } else if ( 0 == strcasecmp("r/m32", token) ) {
        /* r/m32 */
        return OPERAND_RM32;
    } else if ( 0 == strcasecmp("r/m64", token) ) {
        /* r/m64 */
        return OPERAND_RM64;
    } else if ( 0 == strcasecmp("imm8", token) ) {
        /* imm8 */
        return OPERAND_IMM8;
    } else if ( 0 == strcasecmp("imm16", token) ) {
        /* imm16 */
        return OPERAND_IMM16;
    } else if ( 0 == strcasecmp("imm32", token) ) {
        /* imm32 */
        return OPERAND_IMM32;
    } else if ( 0 == strcasecmp("imm64", token) ) {
        /* imm64 */
        return OPERAND_IMM64;
    } else if ( 0 == strcasecmp("m16:16", token) ) {
        /* m16:16 */
        return OPERAND_M16_16;
    } else if ( 0 == strcasecmp("m16:32", token) ) {
        /* m16:32 */
        return OPERAND_M16_32;
    } else if ( 0 == strcasecmp("m16:64", token) ) {
        /* m16:64 */
        return OPERAND_M16_64;
    } else if ( 0 == strcasecmp("al", token) ) {
        /* al */
        return OPERAND_AL;
    } else if ( 0 == strcasecmp("ax", token) ) {
        /* ax */
        return OPERAND_AX;
    } else if ( 0 == strcasecmp("eax", token) ) {
        /* eax */
        return OPERAND_EAX;
    } else if ( 0 == strcasecmp("rax", token) ) {
        /* rax */
        return OPERAND_RAX;
    }

    return -1;
}

/*
 * _parse_operand
 */
int
_parse_operand(struct encode *encode, int enc, const char *operands)
{
    char *s;
    char *tok;
    char *savedptr;
    int arr[3];
    int operand;
    int n;

    /* Duplicate the operand string */
    s = strdup(operands);
    if ( NULL == s ) {
        return -1;
    }

    n = 0;
    tok = strtok_r(s, ",", &savedptr);
    while ( NULL != tok ) {
        if ( n < 3 ) {
            operand = _parse_operand_chunk(_trim(tok));
            if ( operand < 0 ) {
                return -1;
            }
            arr[n] = operand;
            n++;
        }
        tok = strtok_r(NULL, ",", &savedptr);
    }

    encode->type = enc;
    switch ( enc ) {
    case ENCODE_RM:
        if ( n != 2 ) {
            return -1;
        }
        encode->u.rm.r = arr[0];
        encode->u.rm.rm = arr[1];
        break;
    case ENCODE_MR:
        if ( n != 2 ) {
            return -1;
        }
        encode->u.mr.rm = arr[0];
        encode->u.mr.r = arr[1];
        break;
    case ENCODE_OI:
        if ( n != 2 ) {
            return -1;
        }
        encode->u.oi.r = arr[0];
        encode->u.oi.imm = arr[1];
        break;
    case ENCODE_MI:
        if ( n != 2 ) {
            return -1;
        }
        encode->u.mi.rm = arr[0];
        encode->u.mi.imm = arr[1];
        break;
    case ENCODE_M:
        if ( n != 1 ) {
            return -1;
        }
        encode->u.m.m = arr[0];
        break;
    case ENCODE_D:
        if ( n != 1 ) {
            return -1;
        }
        encode->u.d.ptr = arr[0];
        break;
    default:
        return -1;
    }

    return 0;
}

/*
 * Parse an instruction definition file
 */
static struct mnemonic *
_instr_parse_file(const char *m, const char *fname)
{
    struct mnemonic *mnemonic;
    struct rule *rule;
    FILE *fp;
    char buf[1024];
    char *tok;
    char *savedptr;
    char *cols[5];
    int n;
    int enc;
    int ret;

    mnemonic = malloc(sizeof(struct mnemonic));
    if ( NULL == mnemonic ) {
        return NULL;
    }
    mnemonic->mnemonic = strdup(m);
    if ( NULL == mnemonic ) {
        free(mnemonic);
        return NULL;
    }
    mnemonic->rules = NULL;
    mnemonic->next = NULL;

    fp = fopen(fname, "r");
    if ( NULL == fp ) {
        free(mnemonic->mnemonic);
        free(mnemonic);
        return NULL;
    }

    while ( !feof(fp) ) {
        if ( NULL == fgets(buf, sizeof(buf), fp) ) {
            if ( !feof(fp) ) {
                /* Error */
                fclose(fp);
                _free_mnemonic(mnemonic);
                return NULL;
            }
            break;
        }
        /* Parse this line */
        _trim(buf);
        if ( 0 == strncmp("//", buf, 2) ) {
            /* Comment */
            continue;
        }
        /* New rule */
        rule = malloc(sizeof(struct rule));
        if ( NULL == rule ) {
            fclose(fp);
            _free_mnemonic(mnemonic);
            return NULL;
        }
        rule->op.size = 0;
        rule->next = NULL;

        n = 0;
        tok = strtok_r(buf, "|", &savedptr);
        while ( NULL != tok ) {
            if ( n < 5 ) {
                cols[n] = _trim(tok);
                n++;
            }
            tok = strtok_r(NULL, "|", &savedptr);
        }
        if ( 5 != n ) {
            /* Invalid line */
            fprintf(stderr, "Invalid instruction rule\n");
            free(rule);
            continue;
        }

        /* Parse the encode type */
        enc = _parse_encode_type(cols[1]);
        if ( enc < 0 ) {
            free(rule);
            continue;
        }
        /* Parse the opcode */
        ret = _parse_opcode(&rule->op, cols[0]);
        if ( 0 != ret ) {
            free(rule);
            continue;
        }
        /* Parse the operand */
        ret = _parse_operand(&rule->encode, enc, cols[2]);
        if ( 0 != ret ) {
            free(rule);
            continue;
        }

        rule->next = mnemonic->rules;
        mnemonic->rules = rule;
    }

    fclose(fp);

    return mnemonic;
}

/*
 * _search_encode_m
 */
static int
_search_encode_m(struct rule *rule, int n, x86_64_operand_t *ops)
{
    int scale;

    /* Assertion */
    if ( rule->encode.type != ENCODE_M ) {
        return -1;
    }

    /* Check the number of operands */
    if ( n != 1 ) {
        return -1;
    }

    /* Check the operand type */
    if ( ops[0].type != X86_64_OPERAND_MEM ) {
        return -1;
    }
    /* Check the operand size */
    if ( ops[0].u.mem.base < 0 || ops[0].u.mem.base > 7 ) {
        return -1;
    }
    switch ( ops[0].u.mem.scale ) {
    case 1:
        scale = 0;
        break;
    case 2:
        scale = 1;
        break;
    case 4:
        scale = 2;
        break;
    case 8:
        scale = 3;
        break;
    default:
        /* Invalid scale */
        return -1;
    }

    switch ( rule->encode.u.m.m ) {
    case OPERAND_M16_16:
        printf("m16_16 Scale: %d\n", ops[0].u.mem.scale);
        break;
    case OPERAND_M16_32:
        printf("m16_32 Scale: %d\n", ops[0].u.mem.scale);
        break;
    case OPERAND_M16_64:
        printf("m16_64 Scale: %d\n", ops[0].u.mem.scale);
        break;
    default:
        return -1;
    }

    return 0;
}

/*
 * _search_rule -- search a matching rule
 */
static int
_search_rule(struct mnemonic *mnemonic, int n, x86_64_operand_t *ops)
{
    struct rule *rule;
    int ret;

    rule = mnemonic->rules;
    while ( NULL != rule ) {
        /* Check the operand size first */
        if ( n != _operand_num_by_encode_type(rule->encode.type) ) {
            continue;
        }
        ret = _search_encode_m(rule, n, ops);
        rule = rule->next;
    }

    return -1;
}

/*
 * x86_64_search -- find a rule corresponding to the set of the mnemonic and
 * operands.
 */
int
x86_64_search(struct x86_64_instr_ruleset *ruleset, const char *mne, int n,
              x86_64_operand_t *ops)
{
    struct mnemonic *mnemonic;

    mnemonic = ruleset->mnemonics;

    while ( NULL != mnemonic ) {
        if ( 0 == strcmp(mne, mnemonic->mnemonic) ) {
            /* Found */
            return _search_rule(mnemonic, n, ops);
        }
        mnemonic = mnemonic->next;
    }

    return -1;
}

/*
 * x86_64_load_instr -- load all instructions
 */
int
x86_64_load_instr(void)
{
    struct x86_64_instr_ruleset ruleset;
    struct mnemonic *mnemonic;
    static const char *mnemonics[] = {"adc", "add", "call", "jmp", "mov"};
    int i;
    char fname[128];

    ruleset.mnemonics = NULL;

    for ( i = 0; i < sizeof(mnemonics) / sizeof(mnemonics[0]); i++ ) {
        snprintf(fname, sizeof(fname), BASEDIR "/arch/x86-64/idefs/%s.idef",
                 mnemonics[i]);
        mnemonic = _instr_parse_file(mnemonics[i], fname);
        if ( NULL == mnemonic ) {
            return -1;
        }
        mnemonic->next = ruleset.mnemonics;
        ruleset.mnemonics = mnemonic;
    }

    /* Output */
    struct rule *rule;
    mnemonic = ruleset.mnemonics;
    while ( NULL != mnemonic ) {
        printf("* %s\n", mnemonic->mnemonic);
        rule = mnemonic->rules;
        while ( NULL != rule ) {
            printf("Encode: %x / ", rule->encode.type);
            switch ( rule->encode.type ) {
            case ENCODE_M:
                printf("M %x", rule->encode.u.m.m);
                break;
            case ENCODE_RM:
                printf("RM %x %x", rule->encode.u.rm.r, rule->encode.u.rm.rm);
                break;
            case ENCODE_MR:
                printf("MR %x %x", rule->encode.u.mr.rm, rule->encode.u.mr.r);
                break;
            case ENCODE_OI:
                printf("OI %x %x", rule->encode.u.oi.r, rule->encode.u.oi.imm);
                break;
            case ENCODE_MI:
                printf("MI %x %x", rule->encode.u.mi.rm, rule->encode.u.mi.imm);
                break;
            case ENCODE_D:
                printf("D %x", rule->encode.u.d.ptr);
                break;
            }
            printf(" /");
            for ( i = 0; i < rule->op.size; i++ ) {
                printf(" %x", rule->op.opcode[i]);
            }
            printf("\n");
            rule = rule->next;
        }
        mnemonic = mnemonic->next;
    }

    /* Search */
    x86_64_operand_t ops[1];
    int ret;
    ops[0].type = X86_64_OPERAND_MEM;
    ops[0].u.mem.base = 0;
    ops[0].u.mem.sindex = 0;
    ops[0].u.mem.scale = 1;
    ops[0].u.mem.disp = 0;
    ret = x86_64_search(&ruleset, "call", 1, ops);
    printf("Search: %d\n", ret);

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
