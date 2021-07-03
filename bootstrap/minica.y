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
#include "lex.yy.h"

code_file_t *code;
void yyerror(yyscan_t, const char*);

%}

%union {
    code_file_t *file;
    module_t *module;
    outer_block_t *oblock;
    outer_block_entry_t *obent;
    inner_block_t *iblock;
    char *numval;
    char *idval;
    char *strval;
    type_t *type;
    directive_t *directive;
    decl_t *decl;
    decl_list_t *decl_list;
    expr_list_t *exprs;
    expr_t *expr;
    switch_case_t *swcase;
    switch_block_t *swblock;
    literal_t *lit;
    var_t *var;
    var_list_t *varlist;
    val_t *val;
    arg_list_t *args;
    arg_t *arg;
    enum_elem_t *enum_elem;
    coroutine_t *coroutine;
    func_t *func;
    stmt_t *stmt;
    stmt_list_t *stmts;
}

%token <numval>         TOK_LIT_FLOAT
%token <numval>         TOK_LIT_HEXINT TOK_LIT_DECINT TOK_LIT_OCTINT
%token <idval>          TOK_ID
%token <strval>         TOK_LIT_STR
%token TOK_ADD TOK_SUB TOK_MUL TOK_DIV TOK_MOD TOK_DIVMOD TOK_INC TOK_DEC
%token TOK_DEF
%token TOK_LAND TOK_LOR TOK_NOT
%token TOK_LPAREN TOK_RPAREN TOK_LBRACE TOK_RBRACE TOK_LBRACKET TOK_RBRACKET
%token TOK_LCHEVRON TOK_RCHEVRON
%token TOK_IF TOK_ELSE TOK_WHILE TOK_SWITCH TOK_CASE TOK_DEFAULT
%token TOK_EQ_EQ TOK_NEQ TOK_LEQ TOK_GEQ
%token TOK_EQ TOK_COMMA TOK_ATMARK
%token TOK_MODULE TOK_USE TOK_INCLUDE TOK_FN TOK_COROUTINE TOK_RETURN
%token TOK_BIT_OR TOK_BIT_AND TOK_BIT_XOR TOK_BIT_LSHIFT TOK_BIT_RSHIFT
%token TOK_TYPE_I8 TOK_TYPE_I16 TOK_TYPE_I32 TOK_TYPE_I64
%token TOK_TYPEDEF TOK_STRUCT TOK_UNION TOK_ENUM
%token TOK_TYPE_FP32 TOK_TYPE_FP64 TOK_TYPE_STRING TOK_TYPE_BOOL
%token TOK_NIL TOK_TRUE TOK_FALSE
%token TOK_COLON TOK_SEMICOLON

%type <file> file
%type <module> module
%type <iblock> inner_block else_block
%type <oblock> outer_block
%type <obent> outer_entry
%type <idval> identifier
%type <var> variable
%type <decl> declaration
%type <args> args funcargs
%type <arg> arg
%type <directive> directive struct_def union_def enum_def use typedef
%type <decl_list> decl_list
%type <enum_elem> enum_list enum_elem
%type <type> primitive_type type
%type <exprs> expr_list
%type <expr> expression control_expr switch_expr if_expr
%type <expr> assign_expr or_test and_test comparison_eq comparison
%type <expr> or_expr xor_expr and_expr shift_expr
%type <expr> primary a_expr m_expr u_expr atom call
%type <swblock> switch_block
%type <swcase> switch_case
%type <func> function
%type <coroutine> coroutine
%type <stmt> statement stmt_decl stmt_while stmt_expr stmt_return
%type <stmts> statements
%type <lit> literal

%left TOK_ADD TOK_SUB TOK_MUL TOK_DIV TOK_MOD TOK_DIVMOD
%left TOK_BIT_LSHIFT TOK_BIT_RSHIFT
%left TOK_LAND TOK_LOR
%left TOK_EQ_EQ TOK_NEQ TOK_LCHEVRON TOK_RCHEVRON TOK_LEQ TOK_GEQ
%left TOK_BIT_OR TOK_BIT_XOR TOK_BIT_AND
%left TOK_COMMA
%left TOK_INC TOK_DEC
%right TOK_DEF TOK_EQ
%nonassoc TOK_NOT TOK_ATMARK
%nonassoc UNOP ELSENOP

%locations

%lex-param { void *scanner }
%parse-param { void *scanner }

%start file

%%

/* Syntax and parser implementation below */
file:           outer_block
                {
                    $$ = code_file_new($1);
                }
                ;

/* Outer blocks */
outer_block:    outer_entry
                {
                    $$ = outer_block_new($1);
                }
        |       outer_block outer_entry
                {
                    $1->tail->next = $2;
                    $1->tail = $2;
                    $$ = $1;
                }
                ;
outer_entry:    directive
                {
                    outer_block_entry_t *block;
                    block = outer_block_entry_new(OUTER_BLOCK_DIRECTIVE);
                    block->u.dr = $1;
                    $$ = block;
                }
        |       coroutine
                {
                    outer_block_entry_t *block;
                    block = outer_block_entry_new(OUTER_BLOCK_COROUTINE);
                    block->u.cr = $1;
                    $$ = block;
                    context_t *context;
                    context = yyget_extra(scanner);
                    coroutine_vec_add(&code->coroutines, $1);
                }
        |       function
                {
                    outer_block_entry_t *block;
                    block = outer_block_entry_new(OUTER_BLOCK_FUNC);
                    block->u.fn = $1;
                    $$ = block;
                }
        |       module
                {
                    outer_block_entry_t *block;
                    block = outer_block_entry_new(OUTER_BLOCK_MODULE);
                    block->u.md = $1;
                    $$ = block;
                }
                ;

/* Directives */
directive:      include
                {
                    $$ = NULL;
                }
        |       use
                {
                    $$ = $1;
                }
        |       struct_def
                {
                    $$ = $1;
                }
        |       union_def
                {
                    $$ = $1;
                }
        |       enum_def
                {
                    $$ = $1;
                }
        |       typedef
                {
                    $$ = $1;
                }
                ;

include:        TOK_INCLUDE TOK_LIT_STR
                {
                    yyerror(scanner,
                            "The include directive is not implemented.");
                }
                ;
use:            TOK_USE identifier
                {
                    context_t *context;
                    context = yyget_extra(scanner);
                    //compile_use_extern(context, $2);
                    $$ = directive_use_new($2);
                }
                ;
typedef:        TOK_TYPEDEF type identifier
                {
                    context_t *context;
                    context = yyget_extra(scanner);
                    typedef_define(context, $2, $3);
                    $$ = directive_typedef_new($2, $3);
                }
                ;
struct_def:     TOK_STRUCT identifier TOK_LBRACE decl_list TOK_RBRACE
                {
                    $$ = directive_struct_new($2, $4);
                }
        |       TOK_STRUCT TOK_LBRACE decl_list TOK_RBRACE
                {
                    $$ = directive_struct_new(NULL, $3);
                }
                ;
union_def:      TOK_UNION identifier TOK_LBRACE decl_list TOK_RBRACE
                {
                    $$ = directive_union_new($2, $4);
                }
        |       TOK_UNION TOK_LBRACE decl_list TOK_RBRACE
                {
                    $$ = directive_union_new(NULL, $3);
                }
                ;
decl_list:      decl_list declaration
                {
                    $$ = decl_list_append($1, $2);
                }
        |       declaration TOK_SEMICOLON
                {
                    $$ = decl_list_new($1);
                }
        |       declaration
                {
                    $$ = decl_list_new($1);
                }
                ;
enum_def:       TOK_ENUM identifier TOK_LBRACE enum_list TOK_RBRACE
                {
                    $$ = directive_enum_new($2, $4);
                }
                ;
enum_list:      enum_elem TOK_COMMA enum_list
                {
                    $$ = enum_elem_prepend($1, $3);
                }
        |       enum_elem
                {
                    $$ = $1;
                }
                ;
enum_elem:      identifier
                {
                    $$ = enum_elem_new($1);
                }
                ;

/* Module */
module:         TOK_MODULE identifier TOK_LBRACE outer_block TOK_RBRACE
                {
                    context_t *context;
                    module_t *module;
                    module_t *cur;
                    module = module_new($2, $4);
                    if ( NULL == module ) {
                        yyerror(scanner, "Cannot initialize a new module.");
                    }
                    context = yyget_extra(scanner);
                    cur = context->cur;
                    context->cur = module;
                    module->parent = cur;
                    $$ = module;
                }
                ;

/* Coroutine & function */
coroutine:      TOK_COROUTINE identifier funcargs funcargs
                TOK_LBRACE inner_block TOK_RBRACE
                {
                    $$ = coroutine_new($2, $3, $4, $6);
                }
                ;
function:       TOK_FN identifier funcargs funcargs
                TOK_LBRACE inner_block TOK_RBRACE
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
                    arg_list_t *list;
                    list = arg_list_new();
                    $$ = arg_list_append(list, $1);
                }
        |       args TOK_COMMA arg
                {
                    $$ = arg_list_append($1, $3);
                }
        |
                {
                    $$ = arg_list_new();
                }
                ;
arg:            declaration
                {
                    $$ = arg_new($1);
                }
                ;

/* Inner block */
inner_block:    statements
                {
                    $$ = inner_block_new($1);
                }
                ;
statements:     statement
                {
                    $$ = stmt_list_new($1);
                }
        |       statement TOK_SEMICOLON
                {
                    $$ = stmt_list_new($1);
                }
        |       statements statement
                {
                    $$ = stmt_list_append($1, $2);
                }
                ;

/* Statements */
statement:      stmt_decl
                {
                    $$ = $1;
                }
        |       stmt_while
                {
                    $$ = $1;
                }
        |       stmt_expr
                {
                    $$ = $1;
                }
        |       stmt_return
                {
                    $$ = $1;
                }
        |       TOK_LBRACE inner_block TOK_RBRACE
                {
                    $$ = stmt_new_block($2);
                }
                ;
stmt_decl:      declaration
                {
                    $$ = stmt_new_decl($1);
                }
                ;
stmt_while:     TOK_WHILE expression TOK_LBRACE inner_block TOK_RBRACE
                {
                    $$ = stmt_new_while($2, $4);
                }
                ;
stmt_expr:      expression
                {
                    $$ = stmt_new_expr($1);
                }
                ;
stmt_return:    TOK_RETURN expression
                {
                    $$ = stmt_new_return($2);
                }
                ;

/* Expressions */
expr_list:      expression
                {
                    expr_list_t *list;
                    list = expr_list_new();
                    if ( NULL == list ) {
                        yyerror(scanner, "Memory error: expr_list_new()");
                    }
                    $$ = expr_list_append(list, $1);
                }
        |       expr_list TOK_COMMA expression
                {
                    $$ = expr_list_append($1, $3);
                }
                ;

expression:     control_expr
                {
                    $$ = $1;
                }
                ;

control_expr:   if_expr
                {
                    $$ = $1;
                }
        |       switch_expr
                {
                    $$ = $1;
                }
        |       assign_expr
                {
                    $$ = $1;
                }
                ;
if_expr:        TOK_IF expression TOK_LBRACE inner_block TOK_RBRACE else_block
                {
                    $$ = expr_new_if($2, $4, $6);
                }
                ;
else_block:     TOK_ELSE TOK_LBRACE inner_block TOK_RBRACE
                {
                    $$ = $3;
                }
        |       TOK_ELSE if_expr
                {
                    $$ = inner_block_new(stmt_list_new($2));
                }
        |       %prec ELSENOP
                {
                    $$ = NULL;
                }
                ;
switch_expr:    TOK_SWITCH expression TOK_LBRACE switch_block TOK_RBRACE
                {
                    $$ = expr_new_switch($2, $4);
                }
                ;
switch_block:   switch_block switch_case
                {
                    $$ = switch_block_append($1, $2);
                }
        |       switch_case
                {
                    switch_block_t *block;
                    block = switch_block_new();
                    if ( NULL == block ) {
                        yyerror(scanner, "Parse error: switch");
                    }
                    $$ = switch_block_append(block, $1);
                }
                ;
switch_case:    TOK_CASE literal TOK_COLON inner_block
                {
                    $$ = switch_case_new($2, $4);
                }
        |       TOK_DEFAULT TOK_COLON inner_block
                {
                    $$ = switch_case_new(NULL, $3);
                }
                ;
assign_expr:    primary TOK_DEF assign_expr
                {
                    $$ = expr_op_new_infix($1, $3, OP_ASSIGN);
                }
        |       or_test
                {
                    $$ = $1;
                }
                ;
or_test:        or_test TOK_LOR or_test
                {
                    $$ = expr_op_new_infix($1, $3, OP_OR);
                    if ( NULL == $$ ) {
                        yyerror(scanner, "Parse error: ||");
                    }
                }
        |       and_test
                {
                    $$ = $1;
                }
                ;
and_test:       and_test TOK_LAND and_test
                {
                    $$ = expr_op_new_infix($1, $3, OP_AND);
                    if ( NULL == $$ ) {
                        yyerror(scanner, "Parse error: &&");
                    }
                }
        |       or_expr
                {
                    $$ = $1;
                }
                ;
or_expr:        or_expr TOK_BIT_OR or_expr
                {
                    $$ = expr_op_new_infix($1, $3, OP_OR);
                    if ( NULL == $$ ) {
                        yyerror(scanner, "Parse error: ||");
                    }
                }
        |       xor_expr
                {
                    $$ = $1;
                }
                ;
xor_expr:       xor_expr TOK_BIT_XOR xor_expr
                {
                    $$ = expr_op_new_infix($1, $3, OP_XOR);
                    if ( NULL == $$ ) {
                        yyerror(scanner, "Parse error: ^");
                    }
                }
        |       and_expr
                {
                    $$ = $1;
                }
                ;
and_expr:       and_expr TOK_BIT_AND and_expr
                {
                    $$ = expr_op_new_infix($1, $3, OP_AND);
                    if ( NULL == $$ ) {
                        yyerror(scanner, "Parse error: &");
                    }
                }
        |       comparison_eq
                {
                    $$ = $1;
                }
                ;

comparison_eq:  comparison_eq TOK_EQ_EQ comparison_eq
                {
                    $$ = expr_op_new_infix($1, $3, OP_CMP_EQ);
                }
        |       comparison_eq TOK_NEQ comparison_eq
                {
                    $$ = expr_op_new_infix($1, $3, OP_CMP_NEQ);
                }
        |       comparison
                {
                    $$ = $1;
                }
                ;
comparison:     comparison TOK_LCHEVRON comparison
                {
                    $$ = expr_op_new_infix($1, $3, OP_CMP_LT);
                }
        |       comparison TOK_RCHEVRON comparison
                {
                    $$ = expr_op_new_infix($1, $3, OP_CMP_GT);
                }
        |       comparison TOK_LEQ comparison
                {
                    $$ = expr_op_new_infix($1, $3, OP_CMP_LEQ);
                }
        |       comparison TOK_GEQ comparison
                {
                    $$ = expr_op_new_infix($1, $3, OP_CMP_GEQ);
                }
        |       shift_expr
                {
                    $$ = $1;
                }
                ;
shift_expr:     shift_expr TOK_BIT_LSHIFT shift_expr
                {
                    $$ = expr_op_new_infix($1, $3, OP_LSHIFT);
                }
        |       shift_expr TOK_BIT_RSHIFT shift_expr
                {
                    $$ = expr_op_new_infix($1, $3, OP_RSHIFT);
                }
        |       a_expr %prec UNOP
                {
                    $$ = $1;
                }
                ;
a_expr:         a_expr TOK_ADD a_expr
                {
                    $$ = expr_op_new_infix($1, $3, OP_ADD);
                }
        |       a_expr TOK_SUB a_expr
                {
                    $$ = expr_op_new_infix($1, $3, OP_SUB);
                }
        |       m_expr
                {
                    $$ = $1;
                }
                ;
m_expr:         m_expr TOK_MUL m_expr
                {
                    $$ = expr_op_new_infix($1, $3, OP_MUL);
                }
        |       m_expr TOK_DIV m_expr
                {
                    $$ = expr_op_new_infix($1, $3, OP_DIV);
                }
        |       m_expr TOK_MOD m_expr
                {
                    $$ = expr_op_new_infix($1, $3, OP_MOD);
                }
        |       m_expr TOK_DIVMOD m_expr
                {
                    $$ = expr_op_new_infix($1, $3, OP_DIVMOD);
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
        |       TOK_ADD u_expr
                {
                    $$ = expr_op_new_prefix($2, OP_ADD);
                }
        |       TOK_NOT u_expr
                {
                    $$ = expr_op_new_prefix($2, OP_NOT);
                }
        |       TOK_INC u_expr
                {
                    $$ = expr_op_new_prefix($2, OP_INC);
                }
        |       TOK_DEC u_expr
                {
                    $$ = expr_op_new_prefix($2, OP_DEC);
                }
        |       u_expr TOK_INC
                {
                    $$ = expr_op_new_suffix($1, OP_INC);
                }
        |       u_expr TOK_DEC
                {
                    $$ = expr_op_new_suffix($1, OP_DEC);
                }
        |       call
                {
                    $$ = $1;
                }
        |       primary
                {
                    $$ = $1;
                }
                ;
call:           primary TOK_LPAREN expr_list TOK_RPAREN
                {
                    $$ = expr_new_call($1, $3);
                }
                ;
primary:        atom
                {
                    $$ = $1;
                }
        |       primary TOK_LBRACKET expression TOK_RBRACKET
                {
                    $$ = expr_new_ref($1, $3);
                }
                ;

atom:           literal
                {
                    $$ = expr_new_val(val_new_literal($1));
                }
        |       variable
                {
                    $$ = expr_new_val(val_new_variable($1));
                }
                ;
variable:       identifier
                {
                    context_t *context;
                    context = yyget_extra(scanner);
                    $$ = var_new_id(&context->vars, $1);
                }
                ;
declaration:    identifier TOK_COLON type
                {
                    $$ = decl_new($1, $3);
                }
                ;
identifier:     TOK_ID
                {
                    $$ = $1;
                }
                ;

/* Types */
type:           primitive_type
                {
                    $$ = $1;
                }
        |       TOK_STRUCT identifier
                {
                    $$ = type_new_struct($2);
                }
        |       TOK_UNION identifier
                {
                    $$ = type_new_union($2);
                }
        |       TOK_ENUM identifier
                {
                    $$ = type_new_enum($2);
                }
        |       identifier
                {
                    $$ = type_new_id($1);
                }
                ;
primitive_type: TOK_TYPE_I8
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
        |       TOK_TYPE_BOOL
                {
                    $$ = type_new_primitive(TYPE_PRIMITIVE_BOOL);
                }
                ;

/* Literal values */
literal:        TOK_LIT_HEXINT
                {
                    $$ = literal_new_int($1, LIT_HEXINT);
                }
        |       TOK_LIT_DECINT
                {
                    $$ = literal_new_int($1, LIT_DECINT);
                }
        |       TOK_LIT_OCTINT
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
        |       TOK_TRUE
                {
                    $$ = literal_new_bool(BOOL_TRUE);
                }
        |       TOK_FALSE
                {
                    $$ = literal_new_bool(BOOL_FALSE);
                }
        |       TOK_NIL
                {
                    $$ = literal_new_nil();
                }
                ;

%%

/*
 * yyerror -- error handler
 */
void
yyerror(yyscan_t scanner, const char *str)
{
    int lineno;
    lineno = yyget_lineno(scanner);
    fprintf(stderr, "Parser error near Line %d: %s\n", lineno, str);
}

/*
 * minica_parse -- parse the specified file
 */
code_file_t *
minica_parse(FILE *fp)
{
    int ret;
    yyscan_t scanner;
    context_t *context;
    module_t *module;

    /* Allocate space for context */
    context = malloc(sizeof(context_t));
    if ( NULL == context ) {
        return NULL;
    }
    memset(context, 0, sizeof(context_t));

    /* New module */
    module = module_new("", NULL);
    if ( NULL == module ) {
        free(context);
        return NULL;
    }
    context->cur = module;

    /* Initialize the code file (output) */
    code = malloc(sizeof(code_file_t));
    if ( NULL == code ) {
        free(context);
        return NULL;
    }
    ret = code_file_init(code);
    if ( ret < 0 ) {
        free(context);
        free(code);
        return NULL;
    }

    /* Initialize the scanner with the extra data context */
    yylex_init_extra(context, &scanner);

    /* Set the file pointer */
    yyset_in(fp, scanner);

    /* Parse the input file */
    if ( yyparse(scanner) ) {
        fprintf(stderr, "Parse error: yyparse()\n");
        exit(EXIT_FAILURE);
    }

    /* Destroy the scanner */
    free(context);
    yylex_destroy(scanner);

    /* Compile */
    //compile(code);

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
