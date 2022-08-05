/*_
 * Copyright (c) 2022 Hirochika Asai <asai@jar.jp>
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

#ifndef _IR_H
#define _IR_H

#include <stdint.h>

/*
 * Opcode
 */
typedef enum {
    OPCODE_MOV,     /* src,dst */
    OPCODE_ADD,     /* op1,op2,dst */
    OPCODE_SUB,     /* op1,op2,dst */
    OPCODE_MUL,     /* op1,op2,dst */
    OPCODE_DIV,     /* op1,op2,{q,r} */
    OPCODE_MOD,     /* op1,op2,{r,q} */
    OPCODE_INC,     /* op */
    OPCODE_DEC,     /* op */
    OPCODE_NOT,     /* op,dst */
    OPCODE_COMP,    /* op,dst */
    OPCODE_LAND,    /* op1,op2,dst */
    OPCODE_LOR,     /* op1,op2,dst */
    OPCODE_AND,     /* op1,op2,dst */
    OPCODE_OR,      /* op1,op2,dst */
    OPCODE_XOR,     /* op1,op2,dst */
    OPCODE_LSHIFT,  /* op1,op2,dst */
    OPCODE_RSHIFT,  /* op1,op2,dst */
    OPCODE_CMP_EQ,  /* op1,op2,dst */
    OPCODE_CMP_NEQ, /* op1,op2,dst */
    OPCODE_CMP_GT,  /* op1,op2,dst */
    OPCODE_CMP_LT,  /* op1,op2,dst */
    OPCODE_CMP_GEQ, /* op1,op2,dst */
    OPCODE_CMP_LEQ, /* op1,op2,dst */
    OPCODE_RET,     /* no operands */
} opcode_t;

/*
 * Operand type
 */
typedef enum {
    OPERAND_VAL,
    OPERAND_REF,
    OPERAND_I8,
    OPERAND_I16,
    OPERAND_I32,
    OPERAND_I64,
    OPERAND_FP32,
    OPERAND_FP64,
} operand_type_t;

/*
 * Operand type
 */
typedef enum {
    OPERAND_TYPE_REG,
    OPERAND_TYPE_REF,
    OPERAND_TYPE_IMM,
} ir_operand_type_t;

/*
 * Operand size
 */
typedef enum {
    OPERAND_SIZE_AUTO,
    OPERAND_SIZE_I8,
    OPERAND_SIZE_I16,
    OPERAND_SIZE_I32,
    OPERAND_SIZE_I64,
    OPERAND_SIZE_FP32,
    OPERAND_SIZE_FP64,
} ir_operand_size_t;

/*
 * Immediate value type
 */
typedef enum {
    IR_IMM_I8,
    IR_IMM_S8,
    IR_IMM_I16,
    IR_IMM_S16,
    IR_IMM_I32,
    IR_IMM_S32,
    IR_IMM_I64,
    IR_IMM_S64,
} ir_imm_type_t;

/*
 * Register
 */
typedef struct {
    int nr;
} ir_reg_t;

/*
 * Immediate value
 */
typedef struct {
    ir_imm_type_t type;
    union {
        /* Integers */
        uint8_t u8;
        int8_t s8;
        uint16_t u16;
        int16_t s16;
        uint32_t u32;
        int32_t s32;
        uint64_t u64;
        int64_t s64;
    } u;
} ir_imm_t;

/*
 * Reference (pointer)
 */
typedef struct {
    ir_reg_t base;
    ir_reg_t index;
    int scale;
    int64_t disp;
} ir_ref_t;

/*
 * Operand
 */
typedef struct {
    ir_operand_type_t type;
    ir_operand_size_t size;
    union {
        ir_reg_t reg;
        ir_imm_t imm;
        ir_ref_t ref;
    } u;
} ir_operand_t;

/*
 * Instruction
 */
typedef struct {
    opcode_t opcode;
    ir_operand_t *operands[4];
} ir_instr_t;

#ifdef __cplusplus
extern "C" {
#endif

ir_instr_t *
ir_instr_new(void);
void
ir_instr_delete(ir_instr_t *);
ir_imm_t *
ir_imm_init(ir_imm_t *);
void
ir_imm_release(ir_imm_t *);
ir_operand_t *
ir_operand_new(void);
void
ir_operand_delete(ir_operand_t *);

#ifdef __cplusplus
}
#endif

#endif /* _IR_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
