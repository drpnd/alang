/*_
 * Copyright (c) 2020,2022-2023 Hirochika Asai <asai@jar.jp>
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

#ifndef _ARCH_H
#define _ARCH_H

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

/*
 * Architecture
 */
typedef enum {
    ARCH_CPU_X86_64,
    ARCH_CPU_AARCH64,
} arch_cpu_t;

/*
 * Loadable format
 */
typedef enum {
    ARCH_LD_ELF,
    ARCH_LD_MACH_O,
} arch_loader_t;

/*
 * Relocation type
 */
typedef enum {
    ARCH_REL_PC32,
    ARCH_REL_BRANCH,
} arch_rel_type_t;

/*
 * Relocation
 */
typedef struct {
    arch_rel_type_t type;
    off_t pos;
    int sym;
} arch_rel_t;

/*
 * Symbol type
 */
typedef enum {
    ARCH_SYM_LOCAL,
    ARCH_SYM_GLOBAL,
    ARCH_SYM_FUNC,
} arch_sym_type_t;

/*
 * Symbol
 */
typedef struct {
    arch_sym_type_t type;
    char *label;
    off_t pos;
    size_t size;
    uint64_t *ref;
} arch_sym_t;

/*
 * Architecture-specific code
 */
typedef struct {
    /* CPU architecture */
    arch_cpu_t cpu;

    /* Text */
    struct {
        uint8_t *s;
        size_t size;
    } text;

    /* Data */
    struct {
        uint8_t *s;
        size_t size;
    } data;

    /* Symbols */
    struct {
        int n;
        arch_sym_t *syms;
    } sym;

    /* Relocations */
    struct {
        int n;
        arch_rel_t *rels;
    } rel;

} arch_code_t;

/*
 * Architecture-specific function
 */
typedef struct {
    arch_loader_t loader;
    void *assemble;
    int (*export)(FILE *, arch_code_t *);
} arch_t;

#ifdef __cplusplus
extern "C" {
#endif

/* arch/x86-64.c */
int
x86_64_test(uint8_t *);

/* ld/mach-o.c */
int
mach_o_export(FILE *, arch_code_t *);

/* ld/elf.c */
int
elf_export(FILE *, arch_code_t *);

#ifdef __cplusplus
}
#endif

#endif /* _ARCH_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
