/*_
 * Copyright (c) 2018-2019,2021 Hirochika Asai <asai@jar.jp>
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
code_file_t *code;
%}

%union {
    char *numval;
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
    void *include;
    void *exprs;
}

%token <numval>         TOK_LIT_FLOAT
%token <numval>         TOK_LIT_HEXINT TOK_LIT_DECINT TOK_LIT_OCTINT
%token <idval>          TOK_ID
%token <strval>         TOK_LIT_STR
%token TOK_ADD TOK_SUB TOK_MUL TOK_DIV TOK_DEF
%token TOK_LAND TOK_LOR TOK_NOT
%token TOK_LPAREN TOK_RPAREN TOK_LBRACE TOK_RBRACE TOK_LBRACKET TOK_RBRACKET
%token TOK_LCHEVRON TOK_RCHEVRON
%token TOK_EQ_EQ TOK_NEQ TOK_LEQ TOK_GEQ
%token TOK_EQ TOK_COMMA TOK_ATMARK
%token TOK_PACKAGE TOK_MOD TOK_IMPORT TOK_INCLUDE TOK_FN TOK_COROUTINE
%token TOK_BIT_OR TOK_BIT_AND TOK_BIT_LSHIFT TOK_BIT_RSHIFT
%token TOK_TYPE_I8 TOK_TYPE_I16 TOK_TYPE_I32 TOK_TYPE_I64
%token TOK_STRUCT TOK_UNION TOK_ENUM
%token TOK_TYPE_FP32 TOK_TYPE_FP64 TOK_TYPE_STRING
%type <import> import
%type <include> include
%type <idval> identifier
%type <var> variable
%type <val> atom
%type <decl> declaration
%type <arg> arg args funcargs
%type <type> primitive type
%type <expr> or_test and_test not_test comparison
%type <expr> or_expr xor_expr and_expr shift_expr
%type <expr> primary expression a_expr m_expr u_expr
%type <func> function
%type <coroutine> coroutine
%type <stmt> statement stmt_decl stmt_assign stmt_expr
%type <stmts> statements
%type <void> package
%type <lit> literal
%type <exprs> exprs

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
                    import_vec_add(&code->imports, $1);
                }
        |       include
                {
                }
        |       coroutine
                {
                    coroutine_vec_add(&code->coroutines, $1);
                }
        |       function
                {
                    func_vec_add(&code->funcs, $1);
                }
                ;
statements:     statement
                {
                    $$ = stmt_list_new($1);
                }
        |       statement statements
                {
                    $$ = stmt_prepend($1, $2);
                }
                ;
package:        TOK_PACKAGE identifier
                {
                    int ret;
                    ret = package_define(code, $2);
                    if ( 0 != ret ) {
                        /* Already defined */
                        yyerror("Another package is already specified in this "
                                "file.");
                    }
                }
                ;
import:         TOK_IMPORT identifier
                {
                    $$ = import_new($2);
                }
                ;
include:        TOK_INCLUDE TOK_LIT_STR
                {
                    $$ = include_new($2);
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
expression:     or_test
                {
                    $$ = $1;
                }
                ;
or_test:        or_test TOK_LOR and_test
                {
                    $$ = expr_op_new_infix($1, $3, OP_OR);
                }
        |       and_test
                {
                    $$ = $1;
                }
                ;
and_test:       and_test TOK_LAND not_test
                {
                    $$ = expr_op_new_infix($1, $3, OP_AND);
                }
        |       not_test
                {
                    $$ = $1;
                }
                ;
not_test:       TOK_NOT not_test
                {
                    $$ = expr_op_new_prefix($2, OP_NOT);
                }
        |       comparison
                {
                    $$ = $1;
                }
                ;
comparison:     or_expr
                {
                    $$ = $1;
                }
                ;
or_expr:        or_expr TOK_EQ_EQ xor_expr
                {
                    $$ = expr_op_new_infix($1, $3, OP_CMP_EQ);
                }
        |       or_expr TOK_NEQ xor_expr
                {
                    $$ = expr_op_new_infix($1, $3, OP_CMP_NEQ);
                }
        |       or_expr TOK_LCHEVRON xor_expr
                {
                    $$ = expr_op_new_infix($1, $3, OP_CMP_LT);
                }
        |       or_expr TOK_RCHEVRON xor_expr
                {
                    $$ = expr_op_new_infix($1, $3, OP_CMP_GT);
                }
        |       or_expr TOK_LEQ xor_expr
                {
                    $$ = expr_op_new_infix($1, $3, OP_CMP_LEQ);
                }
        |       or_expr TOK_GEQ xor_expr
                {
                    $$ = expr_op_new_infix($1, $3, OP_CMP_GEQ);
                }
        |       xor_expr
                {
                    $$ = $1;
                }
                ;
xor_expr:       and_expr
                {
                    $$ = $1;
                }
                ;
and_expr:       shift_expr
                {
                    $$ = $1;
                }
                ;
shift_expr:     shift_expr TOK_BIT_LSHIFT a_expr
                {
                    $$ = expr_op_new_infix($1, $3, OP_LSHIFT);
                }
        |       shift_expr TOK_BIT_RSHIFT a_expr
                {
                    $$ = expr_op_new_infix($1, $3, OP_RSHIFT);
                }
        |       a_expr
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
m_expr:         m_expr TOK_MUL u_expr
                {
                    $$ = expr_op_new_infix($1, $3, OP_MUL);
                }
        |       m_expr TOK_DIV u_expr
                {
                    $$ = expr_op_new_infix($1, $3, OP_DIV);
                }
        |
                m_expr TOK_MOD u_expr
                {
                    $$ = expr_op_new_infix($1, $3, OP_MOD);
                }
        |       u_expr
                {
                    $$ = $1;
                }
                ;
u_expr:         TOK_SUB u_expr
                {
                    $$ = expr_op_new_prefix($2, OP_SUB);
                }
        |
                TOK_ADD u_expr
                {
                    $$ = expr_op_new_prefix($2, OP_ADD);
                }
        |
                primary
                {
                    $$ = $1;
                }
                ;
primary:        atom
                {
                    $$ = expr_new_val($1);
                }
        |
                atom TOK_LPAREN exprs TOK_RPAREN
                {
                    $$ = NULL;
                }
        |
                atom TOK_LBRACKET expression TOK_RBRACKET
                {
                    $$ = NULL;
                }
                ;
atom:           literal
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
                    //printf("xxx %d %d\n", yylloc.first_line, yylloc.first_column);
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
literal:        TOK_LIT_HEXINT
                {
                    $$ = literal_new_int($1, LIT_HEXINT);
                }
        |
                TOK_LIT_DECINT
                {
                    $$ = literal_new_int($1, LIT_DECINT);
                }
        |
                TOK_LIT_OCTINT
                {
                    $$ = literal_new_int($1, LIT_OCTINT);
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
exprs:          expression
                {
                    $$ = $1;
                }
        |
                expression TOK_COMMA exprs
                {
                    $$ = expr_prepend($1, $3);
                }
        |
                {
                    $$ = NULL;
                }
                ;
%%

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
 * minica_parse -- parse the specified file
 */
code_file_t *
minica_parse(FILE *fp)
{
    extern int yyparse(void);
    extern FILE *yyin;
    int ret;

    /* Initialize the code file (output) */
    code = malloc(sizeof(code_file_t));
    if ( NULL == code ) {
        return NULL;
    }
    ret = code_file_init(code);
    if ( ret < 0 ) {
        free(code);
        return NULL;
    }

    /* Set the file pointer */
    yyin = fp;

    /* Parse the input file */
    if ( yyparse() ) {
        fprintf(stderr, "Parse error!\n");
        exit(EXIT_FAILURE);
    }

    /* Compile */
    compile(code);

    return code;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
