/*_
 * Copyright (c) 2017-2018 Hirochika Asai <asai@jar.jp>
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
#include "alang.h"
#include "tokenizer.h"
#include "parser.h"
#include "compiler.h"

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
 * Load the content of a file
 */
char *
load_file(const char *fname)
{
    FILE *fp;
    char buf[1024];
    char *content;
    char *nb;
    off_t off;
    size_t sz;
    size_t len;

    /* Open the input file */
    fp = fopen(fname, "r");
    if ( NULL == fp ) {
        return NULL;
    }

    /* Allocate memory */
    content = malloc(FILE_MEMSIZE_DELTA);
    if ( NULL == content ) {
        (void)fclose(fp);
        return NULL;
    }
    sz = FILE_MEMSIZE_DELTA;

    off = 0;
    while ( NULL != fgets(buf, sizeof(buf), fp) ) {
        len = strlen(buf);
        /* Expand the memory */
        while ( sz <= off + len ) {
            nb = realloc(content, sz + FILE_MEMSIZE_DELTA);
            if ( NULL == nb ) {
                free(content);
                (void)fclose(fp);
                return NULL;
            }
            sz += FILE_MEMSIZE_DELTA;
        }
        memcpy(content + off, buf, len);
        off += len;
        content[off] = '\0';
    }

    /* Close the file */
    (void)fclose(fp);

    return content;
}

/*
 * Output tokens
 */
void
print_tokens(al_token_list_t *tokens)
{
    al_token_entry_t *e;

    e = tokens->head;
    while ( NULL != e ) {
        printf(" ");
        al_print_token(e->tok);
        e = e->next;
    }
}

/*
 * Main routine
 */
int
main(int argc, const char *const argv[])
{
    const char *fname;
    char *content;
    al_token_list_t *tokens;
    al_stmt_vec_t *program;

    if ( argc < 2 ) {
        usage(argv[0]);
    }

    /* Input file */
    fname = argv[1];

    /* Load the content of the specified file */
    content = load_file(fname);
    if ( NULL == content ) {
        fprintf(stderr, "Failed to load the content of the file: %s\n", fname);
        return EXIT_FAILURE;
    }

    /* Run tokenizer */
    tokens = tokenizer_tokenize(content);
    if ( NULL == tokens ) {
        fprintf(stderr, "Failed to tokenize.\n");
        return EXIT_FAILURE;
    }
#if 0
    print_tokens(tokens);
#endif

    program = parser_parse(tokens);
    if ( NULL == program ) {
        fprintf(stderr, "Failed to parse the program.\n");
        return EXIT_FAILURE;
    }

    compiler_compile(program);

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
