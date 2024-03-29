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
#include <unistd.h>

/*
 * Opcode
 */
typedef enum {
    /* Memory operations */
    IR_OPCODE_ALLOCA,   /* %reg = alloca <type> */
    IR_OPCODE_LOAD,     /* %reg = load <type>* <ptr> */
    IR_OPCODE_STORE,    /* store <type> <value> <type>* <ptr> */
    IR_OPCODE_MOV,      /* src,dst */
    /* Arithmetic operations */
    IR_OPCODE_ADD,      /* %reg = %op1,%op2 */
    IR_OPCODE_SUB,      /* %reg = op1,op2 */
    IR_OPCODE_MUL,      /* %reg = op1,op2 */
    IR_OPCODE_DIV,      /* %q,%r = op1,op2 */
    IR_OPCODE_MOD,      /* %r,%q = op1,op2 */
    IR_OPCODE_INC,      /* op */
    IR_OPCODE_DEC,      /* op */
    /* Logical operations */
    IR_OPCODE_NOT,      /* op,dst */
    IR_OPCODE_COMP,     /* op,dst */
    IR_OPCODE_LAND,     /* op1,op2,dst */
    IR_OPCODE_LOR,      /* op1,op2,dst */
    /* Bit-wise operations */
    IR_OPCODE_AND,      /* op1,op2,dst */
    IR_OPCODE_OR,       /* op1,op2,dst */
    IR_OPCODE_XOR,      /* op1,op2,dst */
    IR_OPCODE_LSHIFT,   /* op1,op2,dst */
    IR_OPCODE_RSHIFT,   /* op1,op2,dst */
    /* Controls */
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
 * Data type
 */
typedef enum {
    IR_DATA_DATA,
    IR_DATA_BSS,
    IR_DATA_RODATA,
} ir_data_type_t;

/*
 * Register type
 */
typedef enum {
    IR_REG_UNDEF = -1,
    IR_REG_PTR,
    IR_REG_I8,
    IR_REG_I16,
    IR_REG_I32,
    IR_REG_I64,
    IR_REG_FP32,
    IR_REG_FP64,
    IR_REG_BOOL,
} ir_reg_type_t;

/*
 * Register
 */
typedef struct {
    ir_reg_type_t type;
    int assigned;
    char *id;
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
    union {
        ir_reg_t reg;
        ir_imm_t imm;
        ir_ref_t ref;
    } u;
} ir_operand_t;

/*
 * Result
 */
typedef struct {
    int n;
    ir_reg_t reg[2];
} ir_result_t;

/*
 * Instruction
 */
typedef struct {
    ir_opcode_t opcode;
    ir_result_t result;
    ir_operand_t operands[4];
} ir_instr_t;

typedef struct _instr_ent ir_instr_ent_t;
typedef struct _block ir_block_t;
/*
 * Instruction entry
 */
struct _instr_ent {
    ir_instr_t inst;
    ir_instr_ent_t *next;
};

/*
 * Label
 */
typedef struct {
    char *label;
    ir_block_t *block;
} ir_label_t;

/*
 * Block
 */
struct _block {
    ir_label_t *label;
    size_t ninstr;
    /* Pointer to the insstruction */
    ir_instr_ent_t *instrs;
};

/*
 * Type of function
 */
typedef enum {
    IR_FUNC_FUNC,
    IR_FUNC_COROUTINE,
} ir_func_type_t;

/*
 * Function / Coroutine
 */
typedef struct _func ir_func_t;
struct _func {
    char *name;
    ir_func_type_t type;
    size_t nblocks;
    ir_block_t *blocks;
    ir_func_t *next;
};

/*
 * Data entry
 */
typedef struct {
    size_t len;
    uint8_t *d;
} ir_data_entry_t;

/*
 * Data table (global)
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
    size_t nfuncs;
    ir_func_t *funcs;
    ir_data_table_t data;
} ir_object_t;

#ifdef __cplusplus
extern "C" {
#endif

/* ir.c */
ir_object_t *
ir_object_new(void);
ir_func_t *
ir_func_new(void);
ir_instr_t *
ir_instr_new(void);
void
ir_instr_delete(ir_instr_t *);
ir_reg_t *
ir_reg_init(ir_reg_t *);
void
ir_reg_release(ir_reg_t *);
ir_imm_t *
ir_imm_init(ir_imm_t *, ir_imm_type_t type);
void
ir_imm_release(ir_imm_t *);
ir_operand_t *
ir_operand_new(void);
void
ir_operand_delete(ir_operand_t *);
int
ir_num_results(ir_opcode_t);
int
ir_num_operands(ir_opcode_t);

/* ir_debug.c */
int
ir_print_code(ir_object_t *);

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
