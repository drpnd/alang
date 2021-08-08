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
    OPCODE_ADD,
} opcode_t;

/*
 * Operand type
 */
typedef enum {
    OPERAND_VAR,
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
        int64_t i;
    } u;
} operand_t;

/*
 * Instruction
 */
typedef struct _instr compiler_instr_t;
struct _instr {
    opcode_t opcode;
    operand_t operands[3];
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
typedef struct {
    compiler_block_type_t type;
    const char *label;
    compiler_instr_t *instr;
} compiler_block_t;

/*
 * Register type
 */
typedef enum {
    VAR_REG_I8,
    VAR_REG_I16,
    VAR_REG_I32,
    VAR_REG_I64,
    VAR_REG_FP32,
    VAR_REG_FP64,
    VAR_MEM,
} var_type_t;

/*
 * Variable
 */
typedef struct _var compiler_var_t;
struct _var {
    var_type_t reg;
    char *id;
    size_t size;
    compiler_var_t *prev;
    compiler_var_t *next;
};

/*
 * Variable stack
 */
typedef struct {
    compiler_var_t *top;
} compiler_var_stack_t;

/*
 * Environment
 */
typedef struct {
    /* Variables */
    compiler_var_stack_t *vars;
} compiler_env_t;

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

    int compile(code_file_t *);
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
