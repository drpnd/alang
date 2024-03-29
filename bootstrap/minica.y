/*_
 * Copyright (c) 2018-2019,2021-2022 Hirochika Asai <asai@jar.jp>
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
#include "y.tab.h"
#include "lex.yy.h"
#include "minica.h"

void yyerror(YYLTYPE *, yyscan_t, const char *);

#define ERROR_ON_NULL(val, msg)             \
    do {                                    \
        if ( NULL == (val) ) {              \
            yyerror(&yylloc, scanner, msg); \
        }                                   \
    } while ( 0 )

%}

%union {
    st_t *file;
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
    literal_set_t *lset;
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
%token TOK_ADD TOK_SUB TOK_MUL TOK_DIV TOK_MOD TOK_INC TOK_DEC
%token TOK_DEF
%token TOK_LAND TOK_LOR TOK_NOT
%token TOK_LPAREN TOK_RPAREN TOK_LBRACE TOK_RBRACE TOK_LBRACKET TOK_RBRACKET
%token TOK_LCHEVRON TOK_RCHEVRON
%token TOK_IF TOK_ELSE TOK_WHILE TOK_SWITCH TOK_CASE TOK_DEFAULT
%token TOK_EQ_EQ TOK_NEQ TOK_LEQ TOK_GEQ
%token TOK_EQ TOK_COMMA TOK_DOT TOK_ATMARK
%token TOK_MODULE TOK_USE TOK_INCLUDE TOK_FN TOK_COROUTINE TOK_RETURN
%token TOK_BIT_OR TOK_BIT_AND TOK_BIT_XOR TOK_BIT_LSHIFT TOK_BIT_RSHIFT
%token TOK_BIT_NOT
%token TOK_TYPE_I8 TOK_TYPE_I16 TOK_TYPE_I32 TOK_TYPE_I64
%token TOK_TYPE_U8 TOK_TYPE_U16 TOK_TYPE_U32 TOK_TYPE_U64
%token TOK_TYPE TOK_TYPEDEF TOK_STRUCT TOK_UNION TOK_ENUM
%token TOK_TYPE_FP32 TOK_TYPE_FP64 TOK_TYPE_STRING TOK_TYPE_BOOL
%token TOK_NIL TOK_TRUE TOK_FALSE
%token TOK_COLON TOK_SEMICOLON

%type <file> file
%type <module> module
%type <iblock> inner_block suite else_block
%type <oblock> outer_block
%type <obent> outer_entry
%type <idval> identifier
%type <decl> declaration
%type <args> args funcargs retvals
%type <arg> arg
%type <directive> directive struct_def union_def enum_def use typedef
%type <decl_list> decl_list
%type <enum_elem> enum_list enum_elem
%type <type> primitive_type type
%type <exprs> expr_list
%type <expr> expression control_expr switch_expr if_expr
%type <expr> assign_expr or_test and_test comparison_eq comparison
%type <expr> or_expr xor_expr and_expr shift_expr
%type <expr> primary a_expr m_expr u_expr p_expr atom
%type <swblock> switch_block
%type <swcase> switch_case
%type <func> fndef
%type <coroutine> crdef
%type <stmt> statement stmt_while stmt_expr_list stmt_return
%type <stmts> statements
%type <lit> literal
%type <lset> literal_set

%nonassoc TOK_LPAREN
%right SNOP
%left TOK_DOT TOK_INC TOK_DEC
%left UNOP
%right TOK_NOT TOK_BIT_NOT TOK_ATMARK
%left TOK_MUL TOK_DIV TOK_MOD
%left TOK_ADD TOK_SUB
%left TOK_BIT_LSHIFT TOK_BIT_RSHIFT
%left TOK_LCHEVRON TOK_RCHEVRON TOK_LEQ TOK_GEQ
%left TOK_EQ_EQ TOK_NEQ
%left TOK_BIT_AND
%left TOK_BIT_XOR
%left TOK_BIT_OR
%left TOK_LAND
%left TOK_LOR
%right TOK_DEF TOK_EQ
%left TOK_COMMA

%nonassoc ELSENOP RETNOP

%pure-parser
%locations

%lex-param { void *scanner }
%parse-param { void *scanner }

%start start

%%

/* Syntax and parser implementation below */
start:          file
                {
                    context_t *context;
                    context = yyget_extra(scanner);
                    context->st = $1;
                }
                ;
file:           outer_block
                {
                    $$ = st_new($1);
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
        |       crdef
                {
                    outer_block_entry_t *block;
                    block = outer_block_entry_new(OUTER_BLOCK_COROUTINE);
                    block->u.cr = $1;
                    $$ = block;
                }
        |       fndef
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
                    yyerror(&yylloc, scanner,
                            "The include directive is not implemented.");
                }
                ;
use:            TOK_USE identifier
                {
                    $$ = directive_use_new(scanner, $2);
                }
                ;
typedef:        TOK_TYPEDEF type identifier
                {
                    $$ = directive_typedef_new(scanner, $2, $3);
                }
        |       TOK_TYPE type TOK_DEF identifier
                {
                    $$ = directive_typedef_new(scanner, $2, $4);
                }
                ;
struct_def:     TOK_STRUCT identifier TOK_LBRACE decl_list TOK_RBRACE
                {
                    $$ = directive_struct_new(scanner, $2, $4);
                }
        |       TOK_STRUCT TOK_LBRACE decl_list TOK_RBRACE
                {
                    $$ = directive_struct_new(scanner, NULL, $3);
                }
                ;
union_def:      TOK_UNION identifier TOK_LBRACE decl_list TOK_RBRACE
                {
                    $$ = directive_union_new(scanner, $2, $4);
                }
        |       TOK_UNION TOK_LBRACE decl_list TOK_RBRACE
                {
                    $$ = directive_union_new(scanner, NULL, $3);
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
                    $$ = directive_enum_new(scanner, $2, $4);
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
                    ERROR_ON_NULL(module, "Cannot initialize a new module.");
                    context = yyget_extra(scanner);
                    cur = context->cur;
                    context->cur = module;
                    module->parent = cur;
                    $$ = module;
                }
                ;

/* Coroutine & function */
crdef:          TOK_COROUTINE identifier funcargs retvals suite
                {
                    $$ = coroutine_new($2, $3, $4, $5);
                }
                ;
fndef:          TOK_FN identifier funcargs retvals suite
                {
                    $$ = func_new($2, $3, $4, $5);
                }
                ;
funcargs:       TOK_LPAREN args TOK_RPAREN
                {
                    $$ = $2;
                }
                ;
retvals:        TOK_LPAREN args TOK_RPAREN
                {
                    $$ = $2;
                }
        |       %prec RETNOP
                {
                    $$ = NULL;
                }
args:           arg
                {
                    $$ = arg_list_new($1);
                }
        |       args TOK_COMMA arg
                {
                    $$ = arg_list_append($1, $3);
                }
        |
                {
                    $$ = arg_list_new(NULL);
                }
                ;
arg:            declaration
                {
                    $$ = arg_new(scanner, $1);
                }
                ;

/* Suite */
suite:          TOK_LBRACE inner_block TOK_RBRACE
                {
                    $$ = $2;
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
statement:      stmt_while
                {
                    $$ = $1;
                }
        |       stmt_expr_list
                {
                    $$ = $1;
                }
        |       stmt_return
                {
                    $$ = $1;
                }
        |       suite
                {
                    $$ = stmt_new_block($1);
                }
                ;
stmt_while:     TOK_WHILE expression TOK_LBRACE inner_block TOK_RBRACE
                {
                    $$ = stmt_new_while($2, $4);
                }
                ;
stmt_expr_list: expr_list
                {
                    $$ = stmt_new_expr_list($1);
                }
                ;
stmt_return:    TOK_RETURN expression
                {
                    $$ = stmt_new_return($2);
                }
        |       TOK_RETURN TOK_SEMICOLON
                {
                    $$ = stmt_new_return(NULL);
                }
                ;

/* Expressions */
expr_list:      expression
                {
                    expr_list_t *list;
                    list = expr_list_new();
                    ERROR_ON_NULL(list, "Memory error: expr_list_new()");
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
if_expr:        TOK_IF expression suite else_block
                {
                    $$ = expr_new_if(scanner, $2, $3, $4);
                }
                ;
else_block:     TOK_ELSE suite
                {
                    $$ = $2;
                }
        |       TOK_ELSE if_expr
                {
                    $$ = inner_block_new(stmt_list_new(stmt_new_expr($2)));
                }
        |       %prec ELSENOP
                {
                    $$ = NULL;
                }
                ;
switch_expr:    TOK_SWITCH expression TOK_LBRACE switch_block TOK_RBRACE
                {
                    $$ = expr_new_switch(scanner, $2, $4);
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
                    ERROR_ON_NULL(block, "Parse error: switch");
                    $$ = switch_block_append(block, $1);
                }
                ;
switch_case:    TOK_CASE literal_set TOK_COLON inner_block
                {
                    ERROR_ON_NULL($2, "Parse error: case");
                    $$ = switch_case_new($2, $4);
                }
        |       TOK_DEFAULT TOK_COLON inner_block
                {
                    $$ = switch_case_new(NULL, $3);
                }
                ;
assign_expr:    primary TOK_DEF assign_expr
                {
                    $$ = expr_op_new_infix(scanner, $1, $3, OP_ASSIGN);
                }
        |       primary TOK_EQ assign_expr
                {
                    fprintf(stderr, "Warning: \"=\" is not recommended.\n");
                    $$ = expr_op_new_infix(scanner, $1, $3, OP_ASSIGN);
                }
        |       or_test
                {
                    $$ = $1;
                }
                ;
or_test:        or_test TOK_LOR or_test
                {
                    $$ = expr_op_new_infix(scanner, $1, $3, OP_LOR);
                    ERROR_ON_NULL($$, "Parse error: ||");
                }
        |       and_test
                {
                    $$ = $1;
                }
                ;
and_test:       and_test TOK_LAND and_test
                {
                    $$ = expr_op_new_infix(scanner, $1, $3, OP_LAND);
                    ERROR_ON_NULL($$, "Parse error: &&");
                }
        |       or_expr
                {
                    $$ = $1;
                }
                ;
or_expr:        or_expr TOK_BIT_OR or_expr
                {
                    $$ = expr_op_new_infix(scanner, $1, $3, OP_OR);
                    ERROR_ON_NULL($$, "Parse error: ||");
                }
        |       xor_expr
                {
                    $$ = $1;
                }
                ;
xor_expr:       xor_expr TOK_BIT_XOR xor_expr
                {
                    $$ = expr_op_new_infix(scanner, $1, $3, OP_XOR);
                    ERROR_ON_NULL($$, "Parse error: ^");
                }
        |       and_expr
                {
                    $$ = $1;
                }
                ;
and_expr:       and_expr TOK_BIT_AND and_expr
                {
                    $$ = expr_op_new_infix(scanner, $1, $3, OP_AND);
                    ERROR_ON_NULL($$, "Parse error: &");
                }
        |       comparison_eq
                {
                    $$ = $1;
                }
                ;

comparison_eq:  comparison_eq TOK_EQ_EQ comparison_eq
                {
                    $$ = expr_op_new_infix(scanner, $1, $3, OP_CMP_EQ);
                }
        |       comparison_eq TOK_NEQ comparison_eq
                {
                    $$ = expr_op_new_infix(scanner, $1, $3, OP_CMP_NEQ);
                }
        |       comparison
                {
                    $$ = $1;
                }
                ;
comparison:     comparison TOK_LCHEVRON comparison
                {
                    $$ = expr_op_new_infix(scanner, $1, $3, OP_CMP_LT);
                }
        |       comparison TOK_RCHEVRON comparison
                {
                    $$ = expr_op_new_infix(scanner, $1, $3, OP_CMP_GT);
                }
        |       comparison TOK_LEQ comparison
                {
                    $$ = expr_op_new_infix(scanner, $1, $3, OP_CMP_LEQ);
                }
        |       comparison TOK_GEQ comparison
                {
                    $$ = expr_op_new_infix(scanner, $1, $3, OP_CMP_GEQ);
                }
        |       shift_expr
                {
                    $$ = $1;
                }
                ;
shift_expr:     shift_expr TOK_BIT_LSHIFT shift_expr
                {
                    $$ = expr_op_new_infix(scanner, $1, $3, OP_LSHIFT);
                }
        |       shift_expr TOK_BIT_RSHIFT shift_expr
                {
                    $$ = expr_op_new_infix(scanner, $1, $3, OP_RSHIFT);
                }
        |       a_expr %prec UNOP
                {
                    $$ = $1;
                }
                ;
a_expr:         a_expr TOK_ADD a_expr
                {
                    $$ = expr_op_new_infix(scanner, $1, $3, OP_ADD);
                }
        |       a_expr TOK_SUB a_expr
                {
                    $$ = expr_op_new_infix(scanner, $1, $3, OP_SUB);
                }
        |       m_expr
                {
                    $$ = $1;
                }
                ;
m_expr:         m_expr TOK_MUL m_expr
                {
                    $$ = expr_op_new_infix(scanner, $1, $3, OP_MUL);
                }
        |       m_expr TOK_DIV m_expr
                {
                    $$ = expr_op_new_infix(scanner, $1, $3, OP_DIV);
                }
        |       m_expr TOK_MOD m_expr
                {
                    $$ = expr_op_new_infix(scanner, $1, $3, OP_MOD);
                }
        |       u_expr
                {
                    $$ = $1;
                }
                ;
u_expr:         TOK_SUB u_expr
                {
                    $$ = expr_op_new_prefix(scanner, $2, OP_SUB);
                }
        |       TOK_ADD u_expr
                {
                    $$ = expr_op_new_prefix(scanner, $2, OP_ADD);
                }
        |       TOK_NOT u_expr
                {
                    $$ = expr_op_new_prefix(scanner, $2, OP_NOT);
                }
        |       TOK_BIT_NOT u_expr
                {
                    $$ = expr_op_new_prefix(scanner, $2, OP_COMP);
                }
        |       TOK_INC u_expr
                {
                    $$ = expr_op_new_prefix(scanner, $2, OP_INC);
                }
        |       TOK_DEC u_expr
                {
                    $$ = expr_op_new_prefix(scanner, $2, OP_DEC);
                }
        |       TOK_BIT_AND u_expr
                {
                    $$ = expr_op_new_prefix(scanner, $2, OP_PTRREF);
                }
        |       TOK_ATMARK u_expr
                {
                    $$ = expr_op_new_prefix(scanner, $2, OP_PTRIND);
                }
        |       p_expr %prec SNOP
                {
                    $$ = $1;
                }
                ;
p_expr:         p_expr TOK_INC
                {
                    $$ = expr_op_new_suffix(scanner, $1, OP_INC);
                }
        |       p_expr TOK_DEC
                {
                    $$ = expr_op_new_suffix(scanner, $1, OP_DEC);
                }
        |       p_expr TOK_DOT identifier
                {
                    $$ = expr_new_member(scanner, $1, $3);
                }
        |       p_expr TOK_LPAREN expr_list TOK_RPAREN
                {
                    $$ = expr_new_call(scanner, $1, $3);
                }
        |       p_expr TOK_LBRACKET expression TOK_RBRACKET
                {
                    $$ = expr_new_ref(scanner, $1, $3);
                }
        |       primary
                {
                    $$ = $1;
                }
                ;
primary:        atom
                {
                    $$ = $1;
                }
        |       TOK_LPAREN expr_list TOK_RPAREN
                {
                    $$ = expr_new_list($2);
                }
                ;

atom:           literal
                {
                    $$ = expr_new_literal(scanner, $1);
                }
        |       identifier
                {
                    $$ = expr_new_id(scanner, $1);
                }
        |       declaration
                {
                    $$ = expr_new_decl(scanner, $1);
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
        |       TOK_TYPE_U8
                {
                    $$ = type_new_primitive(TYPE_PRIMITIVE_U8);
                }
        |       TOK_TYPE_I16
                {
                    $$ = type_new_primitive(TYPE_PRIMITIVE_I16);
                }
        |       TOK_TYPE_U16
                {
                    $$ = type_new_primitive(TYPE_PRIMITIVE_U16);
                }
        |       TOK_TYPE_I32
                {
                    $$ = type_new_primitive(TYPE_PRIMITIVE_I32);
                }
        |       TOK_TYPE_U32
                {
                    $$ = type_new_primitive(TYPE_PRIMITIVE_U32);
                }
        |       TOK_TYPE_I64
                {
                    $$ = type_new_primitive(TYPE_PRIMITIVE_I64);
                }
        |       TOK_TYPE_U64
                {
                    $$ = type_new_primitive(TYPE_PRIMITIVE_U64);
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

/* Literal set */
literal_set:    literal_set TOK_COMMA literal
                {
                    $$ = literal_set_add($1, $3);
                }
        |       literal
                {
                    $$ = literal_set_new();
                    $$ = literal_set_add($$, $1);
                }
                ;

/* Literal values */
literal:        TOK_LIT_HEXINT
                {
                    $$ = literal_new_int(scanner, $1, LIT_HEXINT);
                }
        |       TOK_LIT_DECINT
                {
                    $$ = literal_new_int(scanner, $1, LIT_DECINT);
                }
        |       TOK_LIT_OCTINT
                {
                    $$ = literal_new_int(scanner, $1, LIT_OCTINT);
                }
        |       TOK_LIT_FLOAT
                {
                    $$ = literal_new_float(scanner, $1);
                }
        |       TOK_LIT_STR
                {
                    $$ = literal_new_string(scanner, $1);
                }
        |       TOK_TRUE
                {
                    $$ = literal_new_bool(scanner, BOOL_TRUE);
                }
        |       TOK_FALSE
                {
                    $$ = literal_new_bool(scanner, BOOL_FALSE);
                }
        |       TOK_NIL
                {
                    $$ = literal_new_nil(scanner);
                }
                ;

%%

/*
 * yyerror -- error handler
 */
void
yyerror(YYLTYPE *yylloc, yyscan_t scanner, const char *str)
{
    int lineno;
    lineno = yyget_lineno(scanner);
    fprintf(stderr, "Parser error near Line %d: %s\n", lineno, str);
}

/*
 * minica_parse -- parse the specified file
 */
st_t *
minica_parse(FILE *fp)
{
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
    yylex_destroy(scanner);

    return context->st;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
