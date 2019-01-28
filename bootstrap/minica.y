/*_
 * Copyright (c) 2018-2019 Hirochika Asai <asai@jar.jp>
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

/* Headers and data type definitions */
%{
#include <stdio.h>
#include <stdlib.h>
#include "syntax.h"
int yylex();
int yyerror(char const *);
%}
%union {
    int intval;
    char *idval;
    void *expr;
}
%token <intval>         TOK_LIT_INT
%token <idval>          TOK_ID
%token TOK_ADD TOK_SUB TOK_MUL TOK_DIV TOK_DEF
%token TOK_LPAREN TOK_RPAREN TOK_LBRACE TOK_RBRACE TOK_LBRACKET TOK_RBRACKET
%token TOK_COMMA
%token TOK_PACKAGE TOK_IMPORT TOK_FN
%token TOK_BIT_OR TOK_BIT_AND TOK_BIT_LSHIFT TOK_BIT_RSHIFT
%type <idval> identifier
%type <void> package function
%locations
%%
/* Syntax and parser */
file:           blocks
                ;
blocks:         block
        |       block blocks
                ;
block:          package
        |       import
        |       function
        |       statement
                ;
package:        TOK_PACKAGE identifier
                {
                    printf("> package %s\n", $2);
                }
                ;
import:         TOK_IMPORT identifier
                {
                    printf("> import %s\n", $2);
                }
                ;
function:       TOK_FN identifier TOK_LPAREN args TOK_RPAREN
                TOK_LPAREN args TOK_RPAREN TOK_LBRACE blocks TOK_RBRACE
                {
                    printf("> fn %s\n", $2);
                }
                ;
args:           arg
                {
                }
        |       arg TOK_COMMA args
                {
                }
                ;
arg:            identifier identifier
                {
                    printf("%s %s\n", $1, $2);
                }
                ;
statement:      declaration
                ;
declaration:    identifier identifier
                {
                    printf("%s %s\n", $1, $2);
                }
                ;
identifier:     TOK_ID
                {
                    $$ = $1;
                }
                ;
%%

/*
 * Print usage and exit
 */
void
usage(const char *prog)
{
    fprintf(stderr, "Usage: %s <file>\n", prog);
    exit(EXIT_FAILURE);
}

/*
 * Error handler
 */
int
yyerror(char const *str)
{
    extern char *yytext;

    fprintf(stderr, "Parser error near %s (Line: %d)\n", yytext,
            yylloc.first_line);

    return 0;
}

/*
 * Main routine
 */
int
main(int argc, const char *const argv[])
{
    extern int yyparse(void);
    extern FILE *yyin;

    if ( argc < 2 ) {
        yyin = stdin;
        /* stdio is not supported. */
        usage(argv[0]);
    } else {
        /* Open the specified file */
        yyin = fopen(argv[1], "r");
        if ( NULL == yyin ) {
            perror("fopen");
            exit(EXIT_FAILURE);
        }
    }
    /* Parse the input file */
    if ( yyparse() ) {
        fprintf(stderr, "Parse error!\n");
        exit(EXIT_FAILURE);
    }
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
