/*_
 * Copyright (c) 2019-2021 Hirochika Asai <asai@jar.jp>
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

#ifndef _COMPILE_H
#define _COMPILE_H

#include "syntax.h"
#include <stdio.h>
#include <stdint.h>

typedef struct _val compiler_val_t;
typedef struct _compiler_env compiler_env_t;

/*
 * Assembler operations
 */
typedef struct {
    int (*assemble)(void *);
} assembler_ops_t;

/*
 * Assembler
 */
typedef struct {
    void *spec;
    assembler_ops_t ops;
} assembler_t;

/*
 * Loader operations
 */
typedef struct {
    int (*export)(void *);
} ld_ops_t;

/*
 * Linker
 */
typedef struct {
    void *spec;
    ld_ops_t ops;
} linker_t;

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
} opcode_t;

/*
 * Operand type
 */
typedef enum {
    OPERAND_VAL,
    OPERAND_I8,
    OPERAND_I16,
    OPERAND_I32,
    OPERAND_I64,
} operand_type_t;

/*
 * Operand
 */
typedef struct {
    operand_type_t type;
    union {
        compiler_val_t *val;
    } u;
} operand_t;

/*
 * Instruction
 */
typedef struct _instr compiler_instr_t;
struct _instr {
    opcode_t opcode;
    operand_t operands[4];
    compiler_instr_t *next;
};

/*
 * Block type
 */
typedef enum {
    BLOCK_FUNC,
    BLOCK_COROUTINE,
} compiler_block_type_t;

/*
 * Block
 */
typedef struct _compiler_block compiler_block_t;
struct _compiler_block {
    compiler_block_type_t type;
    char *label;
    compiler_env_t *env;
    compiler_block_t *next;
};

/*
 * Code
 */
typedef struct {
    compiler_instr_t *head;
    compiler_instr_t *tail;
} compiler_code_t;

/*
 * Register type
 */
typedef enum {
    REG_I8,
    REG_I16,
    REG_I32,
    REG_I64,
    REG_I64_SET,
    REG_FP32,
    REG_FP64,
    REG_MEM,
} reg_type_t;

/*
 * Variable
 */
typedef struct _var compiler_var_t;
struct _var {
    char *id;
    type_t *type;
    size_t size;
    compiler_var_t *next;
};

/*
 * Variable table
 */
typedef struct {
    compiler_var_t *top;    /* Stack */
} compiler_var_table_t;

/*
 * Value type
 */
typedef enum {
    VAL_VAR,
    VAL_LITERAL,
    VAL_REG,
    VAL_REG_SET,
    VAL_LIST,
} compiler_val_type_t;

/*
 * Value
 */
typedef struct {
    compiler_val_t *head;
    compiler_val_t *tail;
} compiler_val_list_t;
struct _val {
    compiler_val_type_t type;
    union {
        compiler_var_t *var;
        compiler_val_list_t *list;
        literal_t *lit;
    } u;
    compiler_val_t *next;
};

/*
 * Constant values
 */
typedef struct {
} compiler_data_t;

/*
 * Environment
 */
struct _compiler_env {
    /* Variables */
    compiler_var_table_t *vars;
    /* Instructions */
    compiler_code_t code;
    /* Pointer to the stacked environement below */
    compiler_env_t *prev;
};

/*
 * Type list
 */
typedef struct {
    type_t *type;
    size_t size;
} compiler_type_t;

/*
 * Compiler
 */
typedef struct {
    /* Compiler */
    /* Assembler */
    /* Linker */
    FILE *fout;
} compiler_t;

#ifdef __cplusplus
extern "C" {
#endif

    compiler_t * compile(code_file_t *);
    int compile_use_extern(context_t *, const char *);

#ifdef __cplusplus
}
#endif

#endif /* _COMPILE_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
