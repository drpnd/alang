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
    uint8_t s[] = {
        0x48, 0x89, 0xf8,       /* mov %rdi, %rax */
        0x48, 0xff, 0xc0,       /* inc %rax */
        0xc3,                   /* retq */
        0x90,
        0x48, 0x89, 0xf8,       /* mov %rdi, %rax */
        0x48, 0xff, 0xc0,       /* inc %rax */
        0x48, 0xff, 0xc0,       /* inc %rax */
        0xc3,                   /* retq */
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90
    };

    code.size = sizeof(s);
    code.s = malloc(code.size);
    if ( NULL == code.s ) {
        return -1;
    }
    memcpy(code.s, s, code.size);

    code.sym.n = 2;
    code.sym.syms = malloc(sizeof(arch_sym_t) * code.sym.n);
    if ( NULL == code.sym.syms ) {
        free(code.s);
        return -1;
    }
    code.sym.syms[0].label = "_func";
    code.sym.syms[0].pos = 0;
    code.sym.syms[1].label = "_func2";
    code.sym.syms[1].pos = 8;

    /* Open the output file */
    fp = fopen("mach-o-test.o", "w+");
    if ( NULL == fp ) {
        free(code.sym.syms);
        free(code.s);
        return -1;
    }

    ret = mach_o_export(fp, &code);
    if ( ret < 0 ) {
        fprintf(stderr, "Failed.\n");
    }

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
