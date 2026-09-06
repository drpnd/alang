/*_
 * Copyright (c) 2018-2024,2026 Hirochika Asai <asai@jar.jp>
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
    match_arm_t *match_arm;
    match_block_t *match_block;
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
%token <numval>         TOK_LIT_HEXINT TOK_LIT_DECINT TOK_LIT_BININT
%token <idval>          TOK_ID
%token <strval>         TOK_LIT_STR

    /* Arithmetic operations */
%token TOK_ADD TOK_SUB TOK_MUL TOK_DIV TOK_MOD
    /* Compound assignment */
%token TOK_PLUS_EQ TOK_MINUS_EQ TOK_MUL_EQ TOK_DIV_EQ TOK_MOD_EQ
%token TOK_AND_EQ TOK_OR_EQ TOK_XOR_EQ TOK_LSHIFT_EQ TOK_RSHIFT_EQ
    /* Bitwise operations */
%token TOK_BIT_OR TOK_BIT_AND TOK_BIT_XOR TOK_BIT_LSHIFT TOK_BIT_RSHIFT
%token TOK_BIT_NOT
    /* Logical operations */
%token TOK_LAND TOK_LOR TOK_NOT
    /* Range operators */
%token TOK_DOTS TOK_DOTS_EQ TOK_BANG_DOTS TOK_BANG_DOTS_EQ
    /* Arrow and pipe operators */
%token TOK_ARROW TOK_FATARROW TOK_PIPE TOK_LARROW
    /* Parentheses, brackets, etc... */
%token TOK_LPAREN TOK_RPAREN TOK_LBRACE TOK_RBRACE TOK_LBRACKET TOK_RBRACKET
%token TOK_COMMA TOK_DOT TOK_COLON TOK_SEMICOLON
    /* Comparison */
%token TOK_EQ_EQ TOK_NEQ TOK_LEQ TOK_GEQ TOK_LT TOK_GT
%token TOK_EQ
    /* Primitive types */
%token TOK_TYPE_I8 TOK_TYPE_I16 TOK_TYPE_I32 TOK_TYPE_I64
%token TOK_TYPE_U8 TOK_TYPE_U16 TOK_TYPE_U32 TOK_TYPE_U64
%token TOK_TYPE_F16 TOK_TYPE_F32 TOK_TYPE_F64
%token TOK_TYPE_FP4 TOK_TYPE_FP8
%token TOK_TYPE_STRING TOK_TYPE_BOOL
    /* Reserved keywords */
%token TOK_LET TOK_MUT
%token TOK_FN TOK_CORO TOK_RETURN TOK_BREAK
%token TOK_IF TOK_ELSE TOK_WHILE TOK_FOR TOK_LOOP TOK_MATCH TOK_IN
%token TOK_YIELD TOK_AWAIT
%token TOK_NODE TOK_SOURCE TOK_SINK TOK_GRAPH
%token TOK_STRUCT TOK_ENUM TOK_TYPE TOK_AS
%token TOK_TRUE TOK_FALSE
%token TOK_PUB TOK_MOD_KW TOK_USE

%type <file> file
%type <iblock> inner_block block suite else_block
%type <oblock> outer_block
%type <obent> outer_entry
%type <idval> identifier
%type <decl> declaration
%type <args> args funcargs retvals retval_named_list
%type <arg> arg retval_named
%type <directive> directive struct_def enum_def type_alias
%type <decl_list> decl_list field_list
%type <decl> field
%type <enum_elem> enum_variant_list enum_variant
%type <type> primitive_type type reference_type
%type <exprs> expr_list pattern_list type_list
%type <expr> expression control_expr
%type <expr> assign_expr or_test and_test comparison_eq comparison
%type <expr> or_expr xor_expr and_expr shift_expr
%type <expr> primary a_expr m_expr cast_expr u_expr p_expr atom
%type <expr> match_expr if_expr yield_expr range
%type <expr> pattern
%type <match_block> match_block
%type <match_arm> match_arm
%type <func> fndef
%type <coroutine> crdef
%type <stmt> statement
%type <stmts> statements
%type <lit> literal
%type <lset> literal_set

    /* Precedence (decreasing) */
%nonassoc TOK_LPAREN
%right SNOP
%left TOK_DOT
%left UNOP
%right TOK_NOT TOK_BIT_NOT
%left TOK_AS
%left TOK_MUL TOK_DIV TOK_MOD
%left TOK_ADD TOK_SUB
%left TOK_BIT_LSHIFT TOK_BIT_RSHIFT
%left TOK_LT TOK_GT TOK_LEQ TOK_GEQ
%left TOK_EQ_EQ TOK_NEQ
%left TOK_BIT_AND
%left TOK_BIT_XOR
%left TOK_BIT_OR
%left TOK_LAND
%left TOK_LOR
%nonassoc RANGE
%left TOK_PIPE
%right TOK_LET TOK_MUT
%left TOK_COMMA

%nonassoc ELSENOP RETNOP

%define api.pure
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
                ;

/* Directives */
directive:      struct_def
                {
                    $$ = $1;
                }
        |       enum_def
                {
                    $$ = $1;
                }
        |       type_alias
                {
                    $$ = $1;
                }
                ;

type_alias:     TOK_TYPE identifier TOK_EQ type
                {
                    $$ = directive_type_alias_new(scanner, $4, $2);
                }
                ;
struct_def:     TOK_STRUCT identifier TOK_LBRACE field_list TOK_RBRACE
                {
                    $$ = directive_struct_new(scanner, $2, $4);
                }
        |       TOK_STRUCT identifier TOK_LBRACE TOK_RBRACE
                {
                    $$ = directive_struct_new(scanner, $2, NULL);
                }
                ;
field_list:     field_list TOK_COMMA field
                {
                    $$ = decl_list_append($1, $3);
                }
        |       field
                {
                    $$ = decl_list_new($1);
                }
        |       field TOK_SEMICOLON
                {
                    $$ = decl_list_new($1);
                }
        |       field_list TOK_COMMA
                {
                    $$ = $1;
                }
                ;
field:          identifier TOK_COLON type
                {
                    $$ = decl_new($1, $3);
                }
        |       TOK_MUT identifier TOK_COLON type
                {
                    $$ = decl_new($2, $4);
                }
                ;

/* Enum with variant data */
enum_def:       TOK_ENUM identifier TOK_LBRACE enum_variant_list TOK_RBRACE
                {
                    /* TODO: Update to support variant data */
                    $$ = directive_enum_new(scanner, $2, $4);
                }
        |       TOK_ENUM identifier TOK_LBRACE TOK_RBRACE
                {
                    $$ = directive_enum_new(scanner, $2, NULL);
                }
                ;
enum_variant_list:
                enum_variant
                {
                    $$ = $1;
                }
        |       enum_variant_list TOK_COMMA enum_variant
                {
                    $$ = enum_elem_prepend($3, $1);
                }
        |       enum_variant_list TOK_COMMA
                {
                    $$ = $1;
                }
                ;
enum_variant:   identifier
                {
                    /* Unit variant */
                    $$ = enum_elem_new($1);
                }
        |       identifier TOK_LPAREN type_list TOK_RPAREN
                {
                    /* Tuple variant -- TODO: store type list */
                    $$ = enum_elem_new($1);
                }
        |       identifier TOK_LBRACE field_list TOK_RBRACE
                {
                    /* Struct variant -- TODO: store field list */
                    $$ = enum_elem_new($1);
                }
                ;

/* Types */
type:           primitive_type
                {
                    $$ = $1;
                }
        |       reference_type
                {
                    $$ = $1;
                }
        |       TOK_STRUCT identifier
                {
                    $$ = type_new_struct($2);
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
reference_type: TOK_BIT_AND type
                {
                    $$ = type_new_reference($2, 0);
                }
        |       TOK_BIT_AND TOK_MUT type
                {
                    $$ = type_new_reference($3, 1);
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
        |       TOK_TYPE_F16
                {
                    $$ = type_new_primitive(TYPE_PRIMITIVE_F16);
                }
        |       TOK_TYPE_F32
                {
                    $$ = type_new_primitive(TYPE_PRIMITIVE_F32);
                }
        |       TOK_TYPE_F64
                {
                    $$ = type_new_primitive(TYPE_PRIMITIVE_F64);
                }
        |       TOK_TYPE_FP4
                {
                    $$ = type_new_primitive(TYPE_PRIMITIVE_FP4);
                }
        |       TOK_TYPE_FP8
                {
                    $$ = type_new_primitive(TYPE_PRIMITIVE_FP8);
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

/* Type list (for enum tuple variants) */
type_list:      type
                {
                    expr_list_t *list;
                    list = expr_list_new();
                    ERROR_ON_NULL(list, "Memory error: type_list");
                    /* TODO: proper type list */
                    $$ = list;
                }
        |       type_list TOK_COMMA type
                {
                    /* TODO: append type */
                    $$ = $1;
                }
                ;

/* Coroutine & function */
crdef:          TOK_CORO identifier funcargs retvals suite
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
retvals:        TOK_LPAREN retval_named_list TOK_RPAREN
                {
                    $$ = $2;
                }
        |       TOK_LPAREN TOK_RPAREN
                {
                    $$ = arg_list_new(NULL);
                }
        |       %prec RETNOP
                {
                    $$ = NULL;
                }
                ;
retval_named_list:
                retval_named
                {
                    $$ = arg_list_new($1);
                }
        |       retval_named_list TOK_COMMA retval_named
                {
                    $$ = arg_list_append($1, $3);
                }
        |       retval_named_list TOK_COMMA
                {
                    $$ = $1;
                }
                ;
retval_named:   type
                {
                    /* Unnamed return: just a type */
                    $$ = arg_new(scanner, decl_new(NULL, $1));
                }
        |       identifier TOK_COLON type
                {
                    /* Named return: name: type */
                    $$ = arg_new(scanner, decl_new($1, $3));
                }
                ;
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
arg:            identifier TOK_COLON type
                {
                    $$ = arg_new(scanner, decl_new($1, $3));
                }
        |       TOK_MUT identifier TOK_COLON type
                {
                    $$ = arg_new(scanner, decl_new($2, $4));
                }
                ;

/* Blocks */
block:          TOK_LBRACE statements TOK_RBRACE
                {
                    $$ = inner_block_new($2);
                }
        |       TOK_LBRACE statements expression TOK_RBRACE
                {
                    /* Block with trailing expression */
                    stmt_t *expr_stmt;
                    stmt_list_t *stmts;
                    expr_list_t *elist;
                    elist = expr_list_new();
                    ERROR_ON_NULL(elist, "Memory error: block expr list");
                    elist = expr_list_append(elist, $3);
                    expr_stmt = stmt_new_expr_list(elist);
                    ERROR_ON_NULL(expr_stmt, "Memory error: block expr");
                    stmts = stmt_list_append($2, expr_stmt);
                    $$ = inner_block_new(stmts);
                }
        |       TOK_LBRACE expression TOK_RBRACE
                {
                    /* Block with only trailing expression */
                    stmt_t *expr_stmt;
                    stmt_list_t *stmts;
                    expr_list_t *elist;
                    elist = expr_list_new();
                    ERROR_ON_NULL(elist, "Memory error: block expr list");
                    elist = expr_list_append(elist, $2);
                    expr_stmt = stmt_new_expr_list(elist);
                    ERROR_ON_NULL(expr_stmt, "Memory error: block expr");
                    stmts = stmt_list_new(expr_stmt);
                    $$ = inner_block_new(stmts);
                }
        |       TOK_LBRACE TOK_RBRACE
                {
                    $$ = inner_block_new(NULL);
                }
                ;
suite:          block
                {
                    $$ = $1;
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
statement:      declaration
                {
                    $$ = stmt_new_let($1);
                    ERROR_ON_NULL($$, "Memory error: declaration stmt");
                }
        |       expression
                {
                    expr_list_t *list;
                    list = expr_list_new();
                    ERROR_ON_NULL(list, "Memory error: expr stmt");
                    $$ = stmt_new_expr_list(expr_list_append(list, $1));
                }
        |       TOK_RETURN expression
                {
                    $$ = stmt_new_return($2);
                }
        |       TOK_RETURN
                {
                    $$ = stmt_new_return(NULL);
                }
        |       TOK_BREAK
                {
                    $$ = stmt_new_break();
                }
        |       TOK_WHILE expression block
                {
                    $$ = stmt_new_while($2, $3);
                }
        |       TOK_FOR pattern TOK_IN expression block
                {
                    $$ = stmt_new_for($2, $4, $5);
                }
        |       TOK_LOOP block
                {
                    $$ = stmt_new_loop($2);
                }
        |       block
                {
                    $$ = stmt_new_block($1);
                }
        |       fndef
                {
                    /* TODO: nested function definition */
                    $$ = stmt_new_expr_list(expr_list_new());
                }
        |       crdef
                {
                    /* TODO: nested coroutine definition */
                    $$ = stmt_new_expr_list(expr_list_new());
                }
                ;

/* Declaration (let binding) */
declaration:    TOK_LET identifier TOK_COLON type TOK_EQ expression
                {
                    $$ = decl_new_init($2, $4, $6, 0);
                }
        |       TOK_LET identifier TOK_EQ expression
                {
                    $$ = decl_new_init($2, NULL, $4, 0);
                }
        |       TOK_LET TOK_MUT identifier TOK_COLON type TOK_EQ expression
                {
                    $$ = decl_new_init($3, $5, $7, 1);
                }
        |       TOK_LET TOK_MUT identifier TOK_EQ expression
                {
                    $$ = decl_new_init($3, NULL, $5, 1);
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
        |
                {
                    $$ = expr_list_new();
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
        |       match_expr
                {
                    $$ = $1;
                }
        |       yield_expr
                {
                    $$ = $1;
                }
        |       assign_expr
                {
                    $$ = $1;
                }
                ;
if_expr:        TOK_IF expression block else_block
                {
                    $$ = expr_new_if(scanner, $2, $3, $4);
                }
                ;
else_block:     TOK_ELSE block
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

/* Match expression (replaces switch) */
match_expr:     TOK_MATCH expression TOK_LBRACE match_block TOK_RBRACE
                {
                    $$ = expr_new_match(scanner, $2, $4);
                }
                ;
match_block:    match_block match_arm
                {
                    $$ = match_block_append($1, $2);
                }
        |       match_arm
                {
                    match_block_t *block;
                    block = match_block_new();
                    ERROR_ON_NULL(block, "Parse error: match");
                    $$ = match_block_append(block, $1);
                }
        |
                {
                    match_block_t *block;
                    block = match_block_new();
                    ERROR_ON_NULL(block, "Parse error: empty match");
                    $$ = block;
                }
                ;
match_arm:      pattern TOK_FATARROW expression
                {
                    $$ = match_arm_new($1, inner_block_new(
                        stmt_list_new(stmt_new_expr($3))));
                }
        |       match_arm TOK_COMMA
                {
                    $$ = $1;
                }
                ;

/* Yield expression */
yield_expr:     TOK_YIELD expression
                {
                    $$ = expr_new_yield(scanner, NULL, $2);
                }
        |       TOK_YIELD identifier TOK_LARROW expression
                {
                    $$ = expr_new_yield(scanner, $2, $4);
                }
        |       TOK_YIELD
                {
                    /* yield unit */
                    $$ = expr_new_literal(scanner,
                        literal_new_bool(scanner, BOOL_FALSE));
                }
                ;

/* Patterns (for match arms) */
pattern:        literal
                {
                    /* Literal pattern */
                    $$ = expr_new_literal(scanner, $1);
                }
        |       identifier
                {
                    $$ = expr_new_id(scanner, $1);
                }
        |       TOK_MUT identifier
                {
                    $$ = expr_new_id(scanner, $2);
                }
                ;
pattern_list:   pattern
                {
                    expr_list_t *list;
                    list = expr_list_new();
                    $$ = expr_list_append(list, $1);
                }
        |       pattern_list TOK_COMMA pattern
                {
                    $$ = expr_list_append($1, $3);
                }
        |
                {
                    $$ = expr_list_new();
                }
                ;

/* Assignment and reassignment */
assign_expr:    TOK_MUT identifier TOK_EQ expression
                {
                    /* mut x = expr (reassignment) */
                    $$ = expr_op_new_infix(scanner,
                        expr_new_id(scanner, $2), $4, OP_ASSIGN);
                }
        |       TOK_MUT identifier TOK_PLUS_EQ expression
                {
                    $$ = expr_op_new_infix(scanner,
                        expr_new_id(scanner, $2), $4, OP_ADD);
                }
        |       TOK_MUT identifier TOK_MINUS_EQ expression
                {
                    $$ = expr_op_new_infix(scanner,
                        expr_new_id(scanner, $2), $4, OP_SUB);
                }
        |       TOK_MUT identifier TOK_MUL_EQ expression
                {
                    $$ = expr_op_new_infix(scanner,
                        expr_new_id(scanner, $2), $4, OP_MUL);
                }
        |       TOK_MUT identifier TOK_DIV_EQ expression
                {
                    $$ = expr_op_new_infix(scanner,
                        expr_new_id(scanner, $2), $4, OP_DIV);
                }
        |       TOK_MUT identifier TOK_MOD_EQ expression
                {
                    $$ = expr_op_new_infix(scanner,
                        expr_new_id(scanner, $2), $4, OP_MOD);
                }
        |       or_test
                {
                    $$ = $1;
                }
                ;

/* Range expression */
range:          expression TOK_DOTS expression
                {
                    $$ = expr_new_range(scanner, $1, $3, RANGE_HALF_OPEN);
                }
        |       expression TOK_DOTS_EQ expression
                {
                    $$ = expr_new_range(scanner, $1, $3, RANGE_CLOSED);
                }
        |       expression TOK_BANG_DOTS expression
                {
                    $$ = expr_new_range(scanner, $1, $3, RANGE_OPEN);
                }
        |       expression TOK_BANG_DOTS_EQ expression
                {
                    $$ = expr_new_range(scanner, $1, $3, RANGE_HALF_OPEN_LEFT);
                }
        |       expression TOK_DOTS
                {
                    $$ = expr_new_range(scanner, $1, NULL, RANGE_HALF_OPEN);
                }
        |       TOK_DOTS expression
                {
                    $$ = expr_new_range(scanner, NULL, $2, RANGE_HALF_OPEN);
                }
        |       TOK_DOTS
                {
                    $$ = expr_new_range(scanner, NULL, NULL, RANGE_HALF_OPEN);
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
                    ERROR_ON_NULL($$, "Parse error: |");
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
comparison:     comparison TOK_LT comparison
                {
                    $$ = expr_op_new_infix(scanner, $1, $3, OP_CMP_LT);
                }
        |       comparison TOK_GT comparison
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
        |       cast_expr
                {
                    $$ = $1;
                }
                ;
cast_expr:      cast_expr TOK_AS type
                {
                    $$ = expr_new_cast(scanner, $1, $3);
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
        |       TOK_MUL u_expr
                {
                    /* Dereference: *expr */
                    $$ = expr_op_new_prefix(scanner, $2, OP_PTRIND);
                }
        |       TOK_BIT_AND u_expr
                {
                    /* Borrow: &expr */
                    $$ = expr_op_new_prefix(scanner, $2, OP_PTRREF);
                }
        |       p_expr %prec SNOP
                {
                    $$ = $1;
                }
                ;
p_expr:         p_expr TOK_DOT identifier
                {
                    $$ = expr_new_member(scanner, $1, $3);
                }
        |       identifier TOK_LPAREN expr_list TOK_RPAREN
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
literal:        TOK_LIT_BININT
                {
                    $$ = literal_new_int(scanner, $1, LIT_HEXINT);
                }
        |       TOK_LIT_HEXINT
                {
                    $$ = literal_new_int(scanner, $1, LIT_HEXINT);
                }
        |       TOK_LIT_DECINT
                {
                    $$ = literal_new_int(scanner, $1, LIT_DECINT);
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
                ;

identifier:     TOK_ID
                {
                    $$ = $1;
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


    /* Allocate space for context */
    context = malloc(sizeof(context_t));
    if ( NULL == context ) {
        return NULL;
    }
    memset(context, 0, sizeof(context_t));


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
