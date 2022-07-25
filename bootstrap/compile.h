/*_
 * Copyright (c) 2019-2022 Hirochika Asai <asai@jar.jp>
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
#include "ir.h"
#include <stdio.h>
#include <stdint.h>

typedef struct _val compiler_val_t;
typedef struct _compiler_env compiler_env_t;

/*
 * Reference operand
 */
typedef struct {
    compiler_val_t *val;
    compiler_val_t *off;
} operand_ref_val_t;

/*
 * Reference operand
 */
typedef struct {
    compiler_val_t *val;
    int off;
} operand_ref_imm_t;

/*
 * Operand
 */
typedef struct {
    operand_type_t type;
    union {
        compiler_val_t *val;
        operand_ref_val_t *refval;
        operand_ref_imm_t *refimm;
    } u;
} operand_t;

/*
 * Instruction
 */
typedef struct _instr compiler_instr_t;
struct _instr {
    opcode_t opcode;
    operand_t operands[4];
    ir_instr_t ir;
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
    REG_I64_PAIR,
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
    int arg;
    int ret;
    compiler_var_t *next;
};

/*
 * Variable table
 */
typedef struct {
    compiler_var_t *top;    /* Stack */
    int _allocated;
} compiler_var_table_t;

/*
 * Value type
 */
typedef enum {
    VAL_NIL,
    VAL_VAR,
    VAL_LITERAL,
    VAL_REG,
    VAL_REG_SET,
    VAL_LIST,
    VAL_COND,
} compiler_val_type_t;

/*
 * Value
 */
typedef struct {
    compiler_val_t *head;
    compiler_val_t *tail;
} compiler_val_list_t;
typedef struct {
    int n;
    compiler_val_t **vals;
} compiler_val_cond_t;
struct _val {
    compiler_val_type_t type;
    union {
        compiler_var_t *var;
        compiler_val_list_t *list;
        compiler_val_cond_t *conds;
        literal_t *lit;
    } u;
    /* Linked list */
    compiler_val_t *next;
    /* For compiler use */
    struct {
        int id;
        type_t *type;
    } opt;
};

/*
 * Interference graph
 */
typedef struct {
    int pair[2];
} compiler_edge_t;
typedef struct {
    struct {
        size_t n;
        compiler_val_t **vals;
    } v;
    struct {
        compiler_edge_t *edges;
    } e;
} compiler_ig_t;

/*
 * Constant values
 */
typedef enum {
    DATA_I8,
    DATA_I16,
    DATA_I32,
    DATA_I64,
    DATA_STRING,
} compiler_data_type_t;
typedef struct {
    union {
        unsigned char *s;
        uint8_t b;
        uint16_t w;
        uint32_t d;
        uint64_t q;
    } u;
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
    /* Value of the latest statement */
    compiler_val_t *retval;
    /* For optimization */
    struct {
        int max_id;
    } opt;
    compiler_ig_t ig;
};

/*
 * Type list
 */
typedef struct {
    type_t *type;
    size_t size;
} compiler_type_t;

/*
 * Error code
 */
typedef enum {
    COMPILER_ERROR_UNKNOWN = -1,
    COMPILER_NOMEM,
    COMPILER_DUPLICATE_VARIABLE,
    COMPILER_SYNTAX_ERROR,
} compiler_error_code_t;

/*
 * Error stack
 */
typedef struct _error compiler_error_t;
typedef struct _error {
    compiler_error_code_t err;
    pos_t pos;
    compiler_error_t *next;
};

/*
 * Symbols
 */
typedef struct {
    char *label;
    ir_instr_t *code;
} compiler_symbol_t;

/*
 * Compiler
 */
typedef struct {
    /* Compiled code blocks */
    compiler_block_t *blocks;
    /* Assembler */
    /* Linker */
    FILE *fout;
    /* Error code */
    compiler_error_code_t err;
    pos_t pos;
} compiler_t;

#ifdef __cplusplus
extern "C" {
#endif

    compiler_t * compile(st_t *);

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
