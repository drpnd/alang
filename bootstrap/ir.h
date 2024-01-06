/*_
 * Copyright (c) 2022-2024 Hirochika Asai <asai@jar.jp>
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
    IR_OPCODE_MOV,      /* src,dst */
    IR_OPCODE_ADD,      /* op1,op2,dst */
    IR_OPCODE_SUB,      /* op1,op2,dst */
    IR_OPCODE_MUL,      /* op1,op2,dst */
    IR_OPCODE_DIV,      /* op1,op2,{q,r} */
    IR_OPCODE_MOD,      /* op1,op2,{r,q} */
    IR_OPCODE_INC,      /* op */
    IR_OPCODE_DEC,      /* op */
    IR_OPCODE_NOT,      /* op,dst */
    IR_OPCODE_COMP,     /* op,dst */
    IR_OPCODE_LAND,     /* op1,op2,dst */
    IR_OPCODE_LOR,      /* op1,op2,dst */
    IR_OPCODE_AND,      /* op1,op2,dst */
    IR_OPCODE_OR,       /* op1,op2,dst */
    IR_OPCODE_XOR,      /* op1,op2,dst */
    IR_OPCODE_LSHIFT,   /* op1,op2,dst */
    IR_OPCODE_RSHIFT,   /* op1,op2,dst */
    IR_OPCODE_CMP_EQ,   /* op1,op2,dst */
    IR_OPCODE_CMP_NEQ,  /* op1,op2,dst */
    IR_OPCODE_CMP_GT,   /* op1,op2,dst */
    IR_OPCODE_CMP_LT,   /* op1,op2,dst */
    IR_OPCODE_CMP_GEQ,  /* op1,op2,dst */
    IR_OPCODE_CMP_LEQ,  /* op1,op2,dst */
    IR_OPCODE_RET,      /* no operands */
    IR_OPCODE_YIELD,    /* no operands */
} ir_opcode_t;

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
    //ir_operand_size_t size;
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
    ir_opcode_t opcode;
    ir_operand_t operands[4];
} ir_instr_t;

/*
 * Data entry
 */
typedef struct {
    size_t len;
    uint8_t *d;
} ir_data_entry_t;

/*
 * Data table
 */
typedef struct {
    size_t n;
    size_t used;
    ir_data_entry_t *entries;
} ir_data_table_t;

/*
 * IR object
 */
typedef struct {
    ir_instr_t *instrs;
    ir_data_table_t data;
} ir_object_t;

#ifdef __cplusplus
extern "C" {
#endif

ir_instr_t *
ir_instr_new(void);
void
ir_instr_delete(ir_instr_t *);
ir_reg_t *
ir_reg_init(ir_reg_t *);
void
ir_reg_release(ir_reg_t *);
ir_imm_t *
ir_imm_init(ir_imm_t *);
void
ir_imm_release(ir_imm_t *);
ir_operand_t *
ir_operand_new(void);
void
ir_operand_delete(ir_operand_t *);
int ir_num_operands(ir_opcode_t);

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
