/*_
 * Copyright (c) 2018 Hirochika Asai <asai@jar.jp>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "code.h"
#include "pe.h"
#include "mach-o.h"
#include "elf.h"

#define FILE_MEMSIZE_DELTA  4096

/*
 * Print out the help message and quit the program
 */
void
usage(const char *prog)
{
    fprintf(stderr, "Usage: %s <file>\n", prog);
    exit(EXIT_FAILURE);
}

/*
 * Main routine
 */
int
main(int argc, const char *const argv[])
{
    const char *file;
    FILE *fp;
    struct code code;

    if ( argc != 2 ) {
        usage(argv[0]);
    }
    file = argv[1];

    fp = fopen(file, "w");
    if ( NULL == fp ) {
        perror("fopen():");
        return -1;
    }

    code.bin.text = (uint8_t *)"\x48\x31\xc0\x48\xff\xc0\xc3\x90\x48\x31\xc0\x8b\x05\x00\x00\x00\x00\x48\xff\xc0\x89\x05\x00\x00\x00\x00\xc3\x90\x90";
    code.bin.len = 30;
    code.symbols.ents = malloc(sizeof(struct code_symbol) * 3);
    if ( NULL == code.symbols.ents ) {
        return -1;
    }
    code.symbols.ents[0].name = "_func";
    code.symbols.ents[0].pos = 0;
    code.symbols.ents[1].name = "_func2";
    code.symbols.ents[1].pos = 8;
    code.symbols.ents[2].name = "_func2.i";
    code.symbols.ents[2].pos = 0;
    code.symbols.size = 2;

    code.dsyms.ents = malloc(sizeof(struct code_symbol) * 1);
    if ( NULL == code.dsyms.ents ) {
        return -1;
    }
    code.dsyms.ents[0].name = "_func2.i";
    code.dsyms.ents[0].size = 4;
    code.dsyms.ents[0].pos = 0;
    code.dsyms.size = 1;

    elf_test2(&code, fp);
    //mach_o_test2(&code, fp);
    //mach_o_test(fp);

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
