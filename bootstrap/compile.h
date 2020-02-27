/*_
 * Copyright (c) 2019-2020 Hirochika Asai <asai@jar.jp>
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
 * Register type
 */
typedef enum {
    REG_I8,
    REG_I16,
    REG_I32,
    REG_I64,
    MEM_I8,
    MEM_I16,
    MEM_I32,
    MEM_I64,
} register_type_t;

/*
 * Variable
 */
typedef struct {
    var_t *var;
    register_type_t reg;
} scope_var_t;

/*
 * Scope
 */
typedef struct {
    scope_var_t var;
} scope_t;

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
        scope_var_t var;
        int64_t i;
    } u;
} operand_t;

/*
 * Instruction
 */
typedef struct {
    opcode_t opcode;
    operand_t operands[3];
} instr_t;

/*
 * Compiler
 */
typedef struct {
    /* Assembler */
    /* Linker */
    FILE *fout;
} compiler_t;

#ifdef __cplusplus
extern "C" {
#endif

    int compile(code_file_t *);

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
