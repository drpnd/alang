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
    float floatval;
    char *idval;
    char *strval;
    void *type;
    void *decl;
    void *expr;
    void *lit;
    void *var;
    void *val;
    void *arg;
    void *func;
    void *stmt;
    void *stmts;
}
%token <intval>         TOK_LIT_INT
%token <floatval>       TOK_LIT_FLOAT
%token <idval>          TOK_ID
%token <strval>         TOK_LIT_STR
%token TOK_ADD TOK_SUB TOK_MUL TOK_DIV TOK_DEF
%token TOK_LPAREN TOK_RPAREN TOK_LBRACE TOK_RBRACE TOK_LBRACKET TOK_RBRACKET
%token TOK_COMMA TOK_ATMARK
%token TOK_PACKAGE TOK_IMPORT TOK_FN
%token TOK_BIT_OR TOK_BIT_AND TOK_BIT_LSHIFT TOK_BIT_RSHIFT
%token TOK_TYPE_I8 TOK_TYPE_I16 TOK_TYPE_I32 TOK_TYPE_I64
%token TOK_TYPE_FP32 TOK_TYPE_FP64 TOK_TYPE_STRING
%type <idval> identifier
%type <var> variable
%type <val> value
%type <decl> declaration
%type <arg> arg args funcargs
%type <type> primitive type
%type <expr> primary expression a_expr m_expr
%type <func> function
%type <stmt> stmt_expr
%type <stmts> blocks
%type <void> package
%type <lit> literal
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
function:       TOK_FN identifier funcargs funcargs
                TOK_LBRACE blocks TOK_RBRACE
                {
                    $$ = func_new($2, $3, $4, $6);
                    printf("> fn %s\n", $2);
                }
                ;
funcargs:       TOK_LPAREN args TOK_RPAREN
                {
                    $$ = $2;
                }
                ;
args:           arg
                {
                    $$ = $1;
                }
        |       arg TOK_COMMA args
                {
                    $$ = arg_prepend($1, $3);
                }
        |
                {
                    $$ = NULL;
                }
                ;
arg:            declaration
                {
                    $$ = arg_new($1);
                }
                ;
statement:      declaration
        |       stmt_assign
        |       stmt_expr
                ;
stmt_assign:    variable TOK_DEF expression
                ;
stmt_expr:      expression
                {
                    $$ = stmt_new_expr($1);
                }
                ;
expression:     a_expr
                {
                    $$ = $1;
                }
                ;
a_expr:         a_expr TOK_ADD m_expr
                {
                    $$ = expr_op_new_infix($1, $3, OP_ADD);
                }
        |       a_expr TOK_SUB m_expr
                {
                    $$ = expr_op_new_infix($1, $3, OP_SUB);
                }
        |       m_expr
                {
                    $$ = $1;
                }
                ;
m_expr:         m_expr TOK_MUL primary
                {
                    $$ = expr_op_new_infix($1, $3, OP_MUL);
                }
        |       m_expr TOK_DIV primary
                {
                    $$ = expr_op_new_infix($1, $3, OP_DIV);
                }
        |       primary
                {
                    $$ = $1;
                }
                ;
primary:        value
                {
                    $$ = expr_new_val($1);
                }
                ;
variable:       declaration
                {
                    $$ = var_decl_new($1);
                }
        |       identifier
                {
                    $$ = var_id_new($1, 0);
                }
        |       TOK_ATMARK identifier
                {
                    $$ = var_id_new($2, 1);
                }
        ;
declaration:    identifier type
                {
                    $$ = decl_new($1, $2);
                }
                ;
identifier:     TOK_ID
                {
                    $$ = $1;
                }
                ;
type:           primitive
                {
                    $$ = $1;
                }
        |       identifier
                {
                    $$ = type_id_new($1);
                }
                ;
value:          literal
                {
                    $$ = val_literal_new($1);
                }
        |       variable
                {
                    $$ = val_variable_new($1);
                }
                ;
primitive:      TOK_TYPE_I8
                {
                    $$ = type_primitive_new(TYPE_PRIMITIVE_I8);
                }
        |       TOK_TYPE_I16
                {
                    $$ = type_primitive_new(TYPE_PRIMITIVE_I16);
                }
        |       TOK_TYPE_I32
                {
                    $$ = type_primitive_new(TYPE_PRIMITIVE_I32);
                }
        |       TOK_TYPE_I64
                {
                    $$ = type_primitive_new(TYPE_PRIMITIVE_I64);
                }
        |       TOK_TYPE_FP32
                {
                    $$ = type_primitive_new(TYPE_PRIMITIVE_FP32);
                }
        |       TOK_TYPE_FP64
                {
                    $$ = type_primitive_new(TYPE_PRIMITIVE_FP64);
                }
        |       TOK_TYPE_STRING
                {
                    $$ = type_primitive_new(TYPE_PRIMITIVE_STRING);
                }
                ;
literal:        TOK_LIT_INT
                {
                    $$ = literal_new_int($1);
                }
        |       TOK_LIT_FLOAT
                {
                    $$ = literal_new_float($1);
                }
        |       TOK_LIT_STR
                {
                    $$ = literal_new_string($1);
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
