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

#include "../arch.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/*
 * Main routine for the linker test
 */
int
main(int argc, const char *const argv[])
{
    int ret;
    arch_code_t code;
    FILE *fp;
    int lus;

    /* x86-64 code definitions */
    uint8_t s[] = {
        0x48, 0x89, 0xf8,       /* mov %rdi, %rax */
        0x48, 0xff, 0xc0,       /* inc %rax */
        0xc3,                   /* retq */
        0x90,
        0x48, 0x89, 0xf8,       /* mov %rdi, %rax */
        0x48, 0xff, 0xc0,       /* inc %rax */
        0x48, 0xff, 0xc0,       /* inc %rax */
        0xc3,                   /* retq */
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
        0x48, 0x8d, 0x3d, 0x00, 0x00, 0x00, 0x00, /* lea 0x0(%rip),%rdi */
        0x48, 0x8b, 0x07,       /* mov (%rdi),%rax */
        0x48, 0xff, 0xc0,       /* inc %rax */
        0x48, 0x89, 0x07,       /* mov %rax,(%rdi) */
        0xc3,                   /* retq */
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90
    };

    /* Pares options */
    lus = 0;
    if ( argc >= 2 ) {
        if ( 0 == strcmp(argv[1], "-fleading-underscore") ) {
            lus = 1;
        }
    }

    code.cpu = ARCH_CPU_X86_64;

    code.text.size = sizeof(s);
    code.text.s = malloc(code.text.size);
    if ( NULL == code.text.s ) {
        return -1;
    }
    memcpy(code.text.s, s, code.text.size);

    code.data.size = 8;
    code.data.s = malloc(code.data.size);
    if ( NULL == code.data.s ) {
        free(code.text.s);
        return -1;
    }
    memset(code.data.s, 0, code.data.size);

    code.sym.n = 5;
    code.sym.syms = malloc(sizeof(arch_sym_t) * code.sym.n);
    if ( NULL == code.sym.syms ) {
        free(code.text.s);
        free(code.data.s);
        return -1;
    }
    code.sym.syms[0].type = ARCH_SYM_FUNC;
    code.sym.syms[0].label = lus ? "_func" : "func";
    code.sym.syms[0].pos = 0;
    code.sym.syms[0].size = 8;
    code.sym.syms[1].type = ARCH_SYM_FUNC;
    code.sym.syms[1].label = lus ? "_func2" : "func2";
    code.sym.syms[1].pos = 8;
    code.sym.syms[1].size = 16;
    code.sym.syms[2].type = ARCH_SYM_FUNC;
    code.sym.syms[2].label = lus ? "_func3" : "func3";
    code.sym.syms[2].pos = 24;
    code.sym.syms[2].size = 24;
    code.sym.syms[3].type = ARCH_SYM_LOCAL;
    code.sym.syms[3].label = "data1";
    code.sym.syms[3].pos = 0;
    code.sym.syms[3].size = 8;
    code.sym.syms[4].type = ARCH_SYM_GLOBAL;
    code.sym.syms[4].label = "data2";
    code.sym.syms[4].pos = 0;
    code.sym.syms[4].size = 8;

    code.rel.n = 1;
    code.rel.rels = malloc(sizeof(arch_rel_t) * code.rel.n);
    code.rel.rels[0].type = ARCH_REL_PC32;
    code.rel.rels[0].pos = 27;
    code.rel.rels[0].sym = 3;

    /* Open the output file */
    fp = fopen("mach-o-test.o", "w+");
    if ( NULL == fp ) {
        free(code.sym.syms);
        free(code.rel.rels);
        free(code.text.s);
        free(code.data.s);
        return -1;
    }

    ret = mach_o_export(fp, &code);
    if ( ret < 0 ) {
        fprintf(stderr, "Failed.\n");
    }

    fclose(fp);

    /* Open the output file */
    fp = fopen("elf-test.o", "w+");
    if ( NULL == fp ) {
        free(code.sym.syms);
        free(code.rel.rels);
        free(code.text.s);
        free(code.data.s);
        return -1;
    }

    ret = elf_export(fp, &code);
    if ( ret < 0 ) {
        fprintf(stderr, "Failed.\n");
    }

    fclose(fp);

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
