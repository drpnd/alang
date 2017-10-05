/*_
 * Copyright (c) 2017 Hirochika Asai <asai@jar.jp>
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
    ssize_t i;

    e = tokens->head;
    while ( NULL != e ) {
        printf(" ");
        switch ( e->tok->type ) {
        case TOK_ID:
            printf("%s", e->tok->u.id);
            break;
        case TOK_NIL:
            printf("nil");
            break;
        case TOK_TRUE:
            printf("true");
            break;
        case TOK_FALSE:
            printf("false");
            break;
        case TOK_KW_OR:
            printf("or");
            break;
        case TOK_KW_AND:
            printf("and");
            break;
        case TOK_KW_NOT:
            printf("not");
            break;
        case TOK_KW_FN:
            printf("fn");
            break;
        case TOK_KW_RETURN:
            printf("return");
            break;
        case TOK_KW_CONTINUE:
            printf("continue");
            break;
        case TOK_KW_BREAK:
            printf("break");
            break;
        case TOK_KW_IF:
            printf("if");
            break;
        case TOK_KW_ELSE:
            printf("else");
            break;
        case TOK_KW_WHILE:
            printf("while");
            break;
        case TOK_KW_FOR:
            printf("for");
            break;
        case TOK_MINUS:
            printf("-");
            break;
        case TOK_PLUS:
            printf("+");
            break;
        case TOK_ASTERISK:
            printf("*");
            break;
        case TOK_SLASH:
            printf("/");
            break;
        case TOK_PERCENT:
            printf("%%");
            break;
        case TOK_AMP:
            printf("&");
            break;
        case TOK_BAR:
            printf("|");
            break;
        case TOK_TILDE:
            printf("~");
            break;
        case TOK_HAT:
            printf("^");
            break;
        case TOK_COMMA:
            printf(",");
            break;
        case TOK_PERIOD:
            printf(".");
            break;
        case TOK_NOT:
            printf("!");
            break;
        case TOK_NEQ:
            printf("!=");
            break;
        case TOK_AT:
            printf("@");
            break;
        case TOK_LT:
            printf("<");
            break;
        case TOK_LSHIFT:
            printf("<<");
            break;
        case TOK_LEQ:
            printf("<=");
            break;
        case TOK_GT:
            printf(">");
            break;
        case TOK_RSHIFT:
            printf(">>");
            break;
        case TOK_GEQ:
            printf(">=");
            break;
        case TOK_EQ:
            printf("=");
            break;
        case TOK_EQ_EQ:
            printf("==");
            break;
        case TOK_DEF:
            printf(":=");
            break;
        case TOK_LBRACKET:
            printf("[");
            break;
        case TOK_RBRACKET:
            printf("]");
            break;
        case TOK_LBRACE:
            printf("{");
            break;
        case TOK_RBRACE:
            printf("}");
            break;
        case TOK_LPAREN:
            printf("(");
            break;
        case TOK_RPAREN:
            printf(")");
            break;
        case TOK_COLON:
            printf(":");
            break;
        case TOK_SEMICOLON:
            printf(";");
            break;
        case TOK_NEWLINE:
            printf("\n");
            break;
        case TOK_FLOAT:
            printf("%lf", e->tok->u.f);
            break;
        case TOK_INT:
            printf("%llu", e->tok->u.i);
            break;
        case TOK_LIT_CHAR:
            printf("%x", e->tok->u.c);
            break;
        case TOK_LIT_STR:
            for ( i = 0; i < (ssize_t)e->tok->u.s.len; i++ ) {
                printf("%02x", e->tok->u.s.s[i]);
            }
            break;
        default:
            printf("[UNK]");
        }
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
        printf("XXX\n");
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
