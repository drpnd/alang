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
#include "compile.h"

int yylex();
int yyerror(char const *);
code_file_t code;
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
    void *coroutine;
    void *func;
    void *stmt;
    void *stmts;
    void *import;
}

%token <intval>         TOK_LIT_INT
%token <floatval>       TOK_LIT_FLOAT
%token <idval>          TOK_ID
%token <strval>         TOK_LIT_STR
%token TOK_ADD TOK_SUB TOK_MUL TOK_DIV TOK_DEF
%token TOK_LPAREN TOK_RPAREN TOK_LBRACE TOK_RBRACE TOK_LBRACKET TOK_RBRACKET
%token TOK_COMMA TOK_ATMARK
%token TOK_PACKAGE TOK_IMPORT TOK_FN TOK_COROUTINE
%token TOK_BIT_OR TOK_BIT_AND TOK_BIT_LSHIFT TOK_BIT_RSHIFT
%token TOK_TYPE_I8 TOK_TYPE_I16 TOK_TYPE_I32 TOK_TYPE_I64
%token TOK_TYPE_FP32 TOK_TYPE_FP64 TOK_TYPE_STRING
%type <import> import
%type <idval> identifier
%type <var> variable
%type <val> value
%type <decl> declaration
%type <arg> arg args funcargs
%type <type> primitive type
%type <expr> primary expression a_expr m_expr
%type <func> function
%type <coroutine> coroutine
%type <stmt> statement stmt_decl stmt_assign stmt_expr
%type <stmts> statements
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
                {
                    import_vec_add(&code.imports, $1);
                }
        |       coroutine
                {
                    coroutine_vec_add(&code.coroutines, $1);
                }
        |       function
                {
                    func_vec_add(&code.funcs, $1);
                }
                ;
statements:     statement
                {
                    $$ = $1;
                }
        |       statement statements
                {
                    $$ = stmt_prepend($1, $2);
                }
                ;
package:        TOK_PACKAGE identifier
                {
                    printf("> package %s\n", $2);
                }
                ;
import:         TOK_IMPORT identifier
                {
                    $$ = import_new($2);
                }
                ;
coroutine:      TOK_COROUTINE identifier funcargs funcargs
                TOK_LBRACE statements TOK_RBRACE
                {
                    $$ = coroutine_new($2, $3, $4, $6);
                }
                ;
function:       TOK_FN identifier funcargs funcargs
                TOK_LBRACE statements TOK_RBRACE
                {
                    $$ = func_new($2, $3, $4, $6);
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
statement:      stmt_decl
                {
                    $$ = $1;
                }
        |       stmt_assign
                {
                    $$ = $1;
                }
        |       stmt_expr
                {
                    $$ = $1;
                }
                ;
stmt_decl:      declaration
                {
                    $$ = stmt_new_decl($1);
                }
                ;
stmt_assign:    variable TOK_DEF expression
                {
                    $$ = stmt_new_assign($1, $3);
                }
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
value:          literal
                {
                    $$ = val_new_literal($1);
                }
        |       variable
                {
                    $$ = val_new_variable($1);
                }
                ;
variable:       declaration
                {
                    $$ = var_new_decl($1);
                }
        |       identifier
                {
                    $$ = var_new_id($1, 0);
                }
        |       TOK_ATMARK identifier
                {
                    $$ = var_new_id($2, 1);
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
                    $$ = type_new_id($1);
                }
                ;
primitive:      TOK_TYPE_I8
                {
                    $$ = type_new_primitive(TYPE_PRIMITIVE_I8);
                }
        |       TOK_TYPE_I16
                {
                    $$ = type_new_primitive(TYPE_PRIMITIVE_I16);
                }
        |       TOK_TYPE_I32
                {
                    $$ = type_new_primitive(TYPE_PRIMITIVE_I32);
                }
        |       TOK_TYPE_I64
                {
                    $$ = type_new_primitive(TYPE_PRIMITIVE_I64);
                }
        |       TOK_TYPE_FP32
                {
                    $$ = type_new_primitive(TYPE_PRIMITIVE_FP32);
                }
        |       TOK_TYPE_FP64
                {
                    $$ = type_new_primitive(TYPE_PRIMITIVE_FP64);
                }
        |       TOK_TYPE_STRING
                {
                    $$ = type_new_primitive(TYPE_PRIMITIVE_STRING);
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

    /* Initialize */
    code_file_init(&code);

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

    /* Compile */
    compile(&code);

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
