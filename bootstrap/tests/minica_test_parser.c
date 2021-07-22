/*_
 * Copyright (c) 2021 Hirochika Asai <asai@jar.jp>
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

#include "../compile.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/*
 * usage -- print usage and exit
 */
void
usage(const char *prog)
{
    fprintf(stderr, "Usage: %s <file>\n", prog);
    exit(EXIT_FAILURE);
}

code_file_t * minica_parse(FILE *);

static void
_func(func_t *fn)
{
    printf("Function: %s\n", fn->id);
}

static void
_coroutine(coroutine_t *cr)
{
    printf("Coroutine: %s\n", cr->id);
}

static void
_module(module_t *md)
{
    printf("Module: %s\n", md->id);
}

static void
_directive(directive_t *dr)
{
    printf("Directive\n");
}

static void
_outer_block_entry(outer_block_entry_t *e)
{
    switch ( e->type ) {
    case OUTER_BLOCK_FUNC:
        _func(e->u.fn);
        break;
    case OUTER_BLOCK_COROUTINE:
        _coroutine(e->u.cr);
        break;
    case OUTER_BLOCK_MODULE:
        _module(e->u.md);
        break;
    case OUTER_BLOCK_DIRECTIVE:
        _directive(e->u.dr);
        break;
    }
}

static void
_outer_block(outer_block_t *block)
{
    outer_block_entry_t *e;

    e = block->head;
    while ( NULL != e ) {
        _outer_block_entry(e);
        e = e->next;
    }
}

static void
_display_ast(code_file_t *code)
{
    _outer_block(code->block);
}

/*
 * Main routine for the parser test
 */
int
main(int argc, const char *const argv[])
{
    FILE *fp;
    code_file_t *code;

    if ( argc < 2 ) {
        fp = stdin;
        /* stdio is not supported. */
        usage(argv[0]);
    } else {
        /* Open the specified file */
        fp = fopen(argv[1], "r");
        if ( NULL == fp ) {
            perror("fopen");
            exit(EXIT_FAILURE);
        }
    }

    /* Parse the specified file */
    code = minica_parse(fp);
    if ( NULL == code ) {
        exit(EXIT_FAILURE);
    }

    /* Print out the AST */
    _display_ast(code);

    return EXIT_SUCCESS;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
