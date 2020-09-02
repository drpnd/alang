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

#ifndef _ARCH_X86_64_INSTR_H
#define _ARCH_X86_64_INSTR_H

#include <stdint.h>
#include "reg.h"

/*
 * Operand type
 */
typedef enum {
    X86_64_OPERAND_REG,
    X86_64_OPERAND_MEM,
    X86_64_OPERAND_IMM,
    X86_64_OPERAND_INDIRECT,
} x86_64_operand_type_t;

/*
 * Memory operand
 */
typedef struct {
    int base;
    int sindex;
    int scale;
    int32_t disp;
} x86_64_operand_mem_t;

/*
 * Operand
 */
typedef struct {
    x86_64_operand_type_t type;
    union {
        int reg;
        x86_64_operand_mem_t mem;
        uint32_t imm;
    } u;
} x86_64_operand_t;

#endif /* _ARCH_X86_64_INSTR_H */

    /*
     * Local variables:
     * tab-width: 4
     * c-basic-offset: 4
     * End:
     * vim600: sw=4 ts=4 fdm=marker
     * vim<600: sw=4 ts=4
     */
