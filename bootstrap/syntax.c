/*_
 * Copyright (c) 2019,2021-2022,2024,2026 Hirochika Asai <asai@jar.jp>
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

#include "syntax.h"
#include "y.tab.h"
#include "lex.yy.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Prototype declarations */
static literal_t *
_literal_new(void *scanner);
static expr_t *
_expr_new(void *);

/*
 * _literal_new -- allocate a new literal
 */
static literal_t *
_literal_new(void *scanner)
{
    literal_t *lit;
    YYLTYPE *loc;

    lit = malloc(sizeof(literal_t));
    if ( lit == NULL ) {
        return NULL;
    }
    lit->next = NULL;

    loc = yyget_lloc(scanner);
    lit->pos.first_line = loc->first_line;
    lit->pos.first_column = loc->first_column;
    lit->pos.last_line = loc->last_line;
    lit->pos.last_column = loc->last_column;

    return lit;
}

/*
 * _expr_new -- allocate a new expression
 */
static expr_t *
_expr_new(void *scanner)
{
    expr_t *e;
    YYLTYPE *loc;

    /* Allocate an expression */
    e = malloc(sizeof(expr_t));
    if ( e == NULL ) {
        return NULL;
    }
    e->next = NULL;

    /* Save the scanner location */
    loc = yyget_lloc(scanner);
    e->pos.first_line = loc->first_line;
    e->pos.first_column = loc->first_column;
    e->pos.last_line = loc->last_line;
    e->pos.last_column = loc->last_column;

    return e;
}

/*
 * literal_new_int -- allocate an integer literal
 */
literal_t *
literal_new_int(void *scanner, const char *v, int type)
{
    literal_t *lit;

    lit = _literal_new(scanner);
    if ( lit == NULL ) {
        return NULL;
    }
    lit->type = type;
    lit->u.n = strdup(v);
    if ( lit->u.n == NULL ) {
        free(lit);
        return NULL;
    }

    return lit;
}

/*
 * literal_new_float -- allocate a float literal
 */
literal_t *
literal_new_float(void *scanner, const char *v)
{
    literal_t *lit;

    lit = _literal_new(scanner);
    if ( lit == NULL ) {
        return NULL;
    }
    lit->type = LIT_FLOAT;
    lit->u.n = strdup(v);
    if ( NULL == lit->u.n ) {
        free(lit);
        return NULL;
    }

    return lit;
}

/*
 * literal_new_string -- allocate a string literal
 */
literal_t *
literal_new_string(void *scanner, const char *v)
{
    literal_t *lit;

    lit = _literal_new(scanner);
    if ( lit == NULL ) {
        return NULL;
    }
    lit->type = LIT_STRING;
    lit->u.s = strdup(v);;
    if ( lit->u.s == NULL ) {
        free(lit);
        return NULL;
    }

    return lit;
}

/*
 * literal_new_bool -- allocate a bool literal
 */
literal_t *
literal_new_bool(void *scanner, bool_t bool)
{
    literal_t *lit;

    lit = _literal_new(scanner);
    if ( lit == NULL ) {
        return NULL;
    }
    lit->type = LIT_BOOL;
    lit->u.b = bool;

    return lit;
}


/*
 * literal_release -- free a literal
 */
void
literal_release(literal_t *lit)
{
    switch ( lit->type ) {

    case LIT_BININT:
    case LIT_DECINT:
    case LIT_HEXINT:
    case LIT_FLOAT:
        free(lit->u.n);
        break;
    case LIT_STRING:
        free(lit->u.s);
        break;
    case LIT_BOOL:
        break;
    }
    free(lit);
}

/*
 * literal_set_new -- allocate a literal set
 */
literal_set_t *
literal_set_new(void)
{
    literal_set_t *set;

    set = malloc(sizeof(literal_set_t));
    if ( NULL == set ) {
        return NULL;
    }
    set->head = NULL;
    set->tail = NULL;

    return set;
}

/*
 * literal_set_add -- add a literal to the specified set
 */
literal_set_t *
literal_set_add(literal_set_t *set, literal_t *lit)
{
    if ( NULL == set->head ) {
        set->head = lit;
        set->tail = lit;
    } else {
        set->tail->next = lit;
        set->tail = lit;
    }

    return set;
}

/*
 * type_new_primitive -- allocate a type
 */
type_t *
type_new_primitive(type_type_t tt)
{
    type_t *t;

    t = malloc(sizeof(type_t));
    if ( NULL == t ) {
        return NULL;
    }
    t->type = tt;

    return t;
}

/*
 * type_new_struct -- allocate a new struct type
 */
type_t *
type_new_struct(const char *id)
{
    type_t *t;

    t = malloc(sizeof(type_t));
    if ( NULL == t ) {
        return NULL;
    }
    t->type = TYPE_STRUCT;
    t->id = strdup(id);
    if ( NULL == t->id ) {
        free(t);
        return NULL;
    }

    return t;
}


/*
 * type_new_enum -- allocate a new enum type
 */
type_t *
type_new_enum(const char *id)
{
    type_t *t;

    t = malloc(sizeof(type_t));
    if ( NULL == t ) {
        return NULL;
    }
    t->type = TYPE_ENUM;
    t->id = strdup(id);
    if ( NULL == t->id ) {
        free(t);
        return NULL;
    }

    return t;
}

/*
 * type_new_id -- allocate a type
 */
type_t *
type_new_id(const char *id)
{
    type_t *t;

    t = malloc(sizeof(type_t));
    if ( NULL == t ) {
        return NULL;
    }
    t->type = TYPE_ID;
    t->id = strdup(id);
    if ( NULL == t->id ) {
        free(t);
        return NULL;
    }

    return t;
}

/*
 * decl_new -- allocate a declaration
 */
decl_t *
decl_new(const char *id, type_t *type)
{
    decl_t *dcl;

    dcl = malloc(sizeof(decl_t));
    if ( NULL == dcl ) {
        return NULL;
    }
    dcl->id = strdup(id);
    if ( NULL == dcl->id ) {
        free(dcl);
        return NULL;
    }
    dcl->type = type;
    dcl->next = NULL;

    return dcl;
}

/*
 * decl_list_new -- alloate a declaration entry
 */
decl_list_t *
decl_list_new(decl_t *dcl)
{
    decl_list_t *list;

    list = malloc(sizeof(decl_list_t));
    if ( NULL == list ) {
        return NULL;
    }
    list->head = dcl;
    list->tail = dcl;

    return list;
}

/*
 * decl_list_append -- append an entry to a declaration list
 */
decl_list_t *
decl_list_append(decl_list_t *list, decl_t *dcl)
{
    if ( NULL == list->head ) {
        list->head = dcl;
        list->tail = dcl;
    } else {
        list->tail->next = dcl;
        list->tail = dcl;
    }

    return list;
}

/*
 * arg_new -- allocate an argument
 */
arg_t *
arg_new(void *scanner, decl_t *dcl)
{
    arg_t *arg;
    YYLTYPE *loc;

    arg = malloc(sizeof(arg_t));
    if ( NULL == arg ) {
        return NULL;
    }
    arg->decl = dcl;
    arg->next = NULL;

    loc = yyget_lloc(scanner);
    arg->pos.first_line = loc->first_line;
    arg->pos.first_column = loc->first_column;
    arg->pos.last_line = loc->last_line;
    arg->pos.last_column = loc->last_column;

    return arg;
}

/*
 * arg_list_new -- allocate an argument list
 */
arg_list_t *
arg_list_new(arg_t *arg)
{
    arg_list_t *list;

    list = malloc(sizeof(arg_list_t));
    if ( NULL == list ) {
        return NULL;
    }
    list->head = NULL;
    list->tail = NULL;

    if ( NULL != arg ) {
        return arg_list_append(list, arg);
    }

    return list;
}

/*
 * arg_list_append -- append an argument to the list
 */
arg_list_t *
arg_list_append(arg_list_t *list, arg_t *arg)
{
    if ( NULL == list->head ) {
        list->head = arg;
        list->tail = arg;
    } else {
        list->tail->next = arg;
        list->tail = arg;
    }

    return list;
}

/*
 * directive_struct_new -- allocate a struct data structure
 */
directive_t *
directive_struct_new(void *scanner, const char *id, decl_list_t *list)
{
    directive_t *dir;
    YYLTYPE *loc;

    dir = malloc(sizeof(directive_t));
    if ( NULL == dir ) {
        return NULL;
    }
    dir->type = DIRECTIVE_STRUCT;
    if ( NULL != id ) {
        dir->u.st.id = strdup(id);
        if ( NULL == dir->u.st.id ) {
            free(dir);
            return NULL;
        }
    } else {
        dir->u.st.id = NULL;
    }
    dir->u.st.list = list;

    loc = yyget_lloc(scanner);
    dir->pos.first_line = loc->first_line;
    dir->pos.first_column = loc->first_column;
    dir->pos.last_line = loc->last_line;
    dir->pos.last_column = loc->last_column;

    return dir;
}


/*
 * directive_enum_new -- allocate an enum data structure
 */
directive_t *
directive_enum_new(void *scanner, const char *id, enum_elem_t *list)
{
    directive_t *dir;
    YYLTYPE *loc;

    dir = malloc(sizeof(directive_t));
    if ( NULL == dir ) {
        return NULL;
    }
    dir->type = DIRECTIVE_ENUM;
    dir->u.en.id = strdup(id);
    if ( NULL == dir->u.en.id ) {
        free(dir);
        return NULL;
    }
    dir->u.en.list = list;

    loc = yyget_lloc(scanner);
    dir->pos.first_line = loc->first_line;
    dir->pos.first_column = loc->first_column;
    dir->pos.last_line = loc->last_line;
    dir->pos.last_column = loc->last_column;

    return dir;
}

/*
 * directive_type_alias_new -- allocate a new use statement
 */
directive_t *
directive_type_alias_new(void *scanner, type_t *src, const char *dst)
{
    directive_t *dir;
    YYLTYPE *loc;

    dir = malloc(sizeof(directive_t));
    if ( NULL == dir ) {
        return NULL;
    }
    dir->type = DIRECTIVE_TYPE_ALIAS;
    dir->u.type_alias.src = src;
    dir->u.type_alias.dst = strdup(dst);
    if ( NULL == dir->u.type_alias.dst ) {
        free(dir);
        return NULL;
    }

    loc = yyget_lloc(scanner);
    dir->pos.first_line = loc->first_line;
    dir->pos.first_column = loc->first_column;
    dir->pos.last_line = loc->last_line;
    dir->pos.last_column = loc->last_column;

    return dir;
}


/*
 * enum_elem_new -- allocate a new enumerate element
 */
enum_elem_t *
enum_elem_new(const char *id)
{
    enum_elem_t *elem;

    elem = malloc(sizeof(enum_elem_t));
    if ( NULL == elem ) {
        return NULL;
    }
    elem->id = strdup(id);
    if ( NULL == elem->id ) {
        free(elem);
        return NULL;
    }
    elem->next = NULL;

    return elem;
}

/*
 * enum_elem_prepend -- prepend an enumerate element to the list
 */
enum_elem_t *
enum_elem_prepend(enum_elem_t *elem, enum_elem_t *list)
{
    elem->next = list;
    return elem;
}

/*
 * expr_new_id -- allocate an expression with an ID
 */
expr_t *
expr_new_id(void *scanner, const char *id)
{
    expr_t *e;

    e = _expr_new(scanner);
    if ( e == NULL ) {
        return NULL;
    }
    e->type = EXPR_ID;
    e->u.id = strdup(id);
    if ( e->u.id == NULL ) {
        free(e);
        return NULL;
    }

    return e;
}

/*
 * expr_new_decl -- allocate an expression with a declaration
 */
expr_t *
expr_new_decl(void *scanner, decl_t *decl)
{
    expr_t *e;

    e = _expr_new(scanner);
    if ( e == NULL ) {
        return NULL;
    }
    e->type = EXPR_DECL;
    e->u.decl = decl;

    return e;
}

/*
 * expr_new_literal -- allocate an expression with a literal
 */
expr_t *
expr_new_literal(void *scanner, literal_t *lit)
{
    expr_t *e;

    e = _expr_new(scanner);
    if ( e == NULL ) {
        return NULL;
    }
    e->type = EXPR_LITERAL;
    e->u.lit = lit;

    return e;
}

/*
 * expr_new_member -- allocate a member reference expression
 */
expr_t *
expr_new_member(void *scanner, expr_t *pe, const char *id)
{
    expr_t *e;

    e = _expr_new(scanner);
    if ( e == NULL ) {
        return NULL;
    }
    e->type = EXPR_MEMBER;
    e->u.mem.e = pe;
    e->u.mem.id = strdup(id);
    if ( NULL == e->u.mem.id ) {
        free(e);
        return NULL;
    }

    return e;
}

/*
 * expr_new_call -- allocate a call expression
 */
expr_t *
expr_new_call(void *scanner, const char *callee, expr_list_t *exprs)
{
    expr_t *e;

    e = _expr_new(scanner);
    if ( e == NULL ) {
        return NULL;
    }
    e->u.call = malloc(sizeof(call_t));
    if ( NULL == e->u.call ) {
        free(e);
        return NULL;
    }
    e->type = EXPR_CALL;
    e->u.call->callee = strdup(callee);
    if ( e->u.call->callee == NULL ) {
        free(e);
        free(e->u.call);
        return NULL;
    }
    e->u.call->exprs = exprs;

    return e;
}

/*
 * expr_new_ref -- allocate a reference expression
 */
expr_t *
expr_new_ref(void *scanner, expr_t *var, expr_t *expr)
{
    expr_t *e;

    e = _expr_new(scanner);
    if ( e == NULL ) {
        return NULL;
    }
    e->u.ref = malloc(sizeof(ref_t));
    if ( NULL == e->u.ref ) {
        free(e);
        return NULL;
    }
    e->type = EXPR_REF;
    e->u.ref->var = var;
    e->u.ref->arg = expr;

    return e;
}


/*
 * expr_new_if
 */
expr_t *
expr_new_if(void *scanner, expr_t *cond, inner_block_t *bif,
            inner_block_t *belse)
{
    expr_t *e;

    e = _expr_new(scanner);
    if ( e == NULL ) {
        return NULL;
    }
    e->type = EXPR_IF;
    e->u.ife.cond = cond;
    e->u.ife.bif = bif;
    e->u.ife.belse = belse;

    return e;
}

/*
 * expr_new_list
 */
expr_t *
expr_new_list(expr_list_t *list)
{
    expr_t *e;

    e = malloc(sizeof(expr_t));
    if ( NULL == e ) {
        return NULL;
    }
    e->type = EXPR_LIST;
    e->u.list = list;
    e->next = NULL;

    return e;
}

/*
 * expr_list_new -- allocate an expression list
 */
expr_list_t *
expr_list_new(void)
{
    expr_list_t *list;

    list = malloc(sizeof(expr_list_t));
    if ( NULL == list ) {
        return NULL;
    }
    list->head = NULL;
    list->tail = NULL;

    return list;
}

/*
 * expr_list_append -- append an expression to the list
 */
expr_list_t *
expr_list_append(expr_list_t *exprs, expr_t *expr)
{
    if ( NULL == exprs->head ) {
        exprs->head = expr;
        exprs->tail = expr;
    } else {
        exprs->tail->next = expr;
        exprs->tail = expr;
    }

    return exprs;
}

/*
 * op_new_infix -- allocate an infix operation
 */
op_t *
op_new_infix(expr_t *e0, expr_t *e1, op_type_t type)
{
    op_t *op;

    op = malloc(sizeof(op_t));
    if ( NULL == op ) {
        return NULL;
    }
    op->fix = FIX_INFIX;
    op->type = type;
    op->e0 = e0;
    op->e1 = e1;

    return op;
}

/*
 * op_new_prefix -- allocate a prefixed operation
 */
op_t *
op_new_prefix(expr_t *e0, op_type_t type)
{
    op_t *op;

    op = malloc(sizeof(op_t));
    if ( NULL == op ) {
        return NULL;
    }
    op->fix = FIX_PREFIX;
    op->type = type;
    op->e0 = e0;
    op->e1 = NULL;

    return op;
}

/*
 * op_new_suffix -- allocate a suffixed operation
 */
op_t *
op_new_suffix(expr_t *e0, op_type_t type)
{
    op_t *op;

    op = malloc(sizeof(op_t));
    if ( NULL == op ) {
        return NULL;
    }
    op->fix = FIX_SUFFIX;
    op->type = type;
    op->e0 = e0;
    op->e1 = NULL;

    return op;
}

/*
 * expr_op_new_infix -- allocate an infix operation expression
 */
expr_t *
expr_op_new_infix(void *scanner, expr_t *e0, expr_t *e1, op_type_t type)
{
    op_t *op;
    expr_t *e;
    YYLTYPE *loc;

    e = malloc(sizeof(expr_t));
    if ( NULL == e ) {
        return NULL;
    }
    op = op_new_infix(e0, e1, type);
    if ( NULL == op ) {
        free(e);
        return NULL;
    }
    e->type = EXPR_OP;
    e->u.op = op;
    e->next = NULL;

    loc = yyget_lloc(scanner);
    e->pos.first_line = loc->first_line;
    e->pos.first_column = loc->first_column;
    e->pos.last_line = loc->last_line;
    e->pos.last_column = loc->last_column;

    return e;
}

/*
 * expr_op_new_prefix -- allocate a prefixed operation expression
 */
expr_t *
expr_op_new_prefix(void *scanner, expr_t *e0, op_type_t type)
{
    op_t *op;
    expr_t *e;
    YYLTYPE *loc;

    e = malloc(sizeof(expr_t));
    if ( NULL == e ) {
        return NULL;
    }
    op = op_new_prefix(e0, type);
    if ( NULL == op ) {
        free(e);
        return NULL;
    }
    e->type = EXPR_OP;
    e->u.op = op;
    e->next = NULL;

    loc = yyget_lloc(scanner);
    e->pos.first_line = loc->first_line;
    e->pos.first_column = loc->first_column;
    e->pos.last_line = loc->last_line;
    e->pos.last_column = loc->last_column;

    return e;
}

/*
 * expr_op_new_suffix -- allocate a suffixed operation expression
 */
expr_t *
expr_op_new_suffix(void *scanner, expr_t *e0, op_type_t type)
{
    op_t *op;
    expr_t *e;
    YYLTYPE *loc;

    e = malloc(sizeof(expr_t));
    if ( NULL == e ) {
        return NULL;
    }
    op = op_new_suffix(e0, type);
    if ( NULL == op ) {
        free(e);
        return NULL;
    }
    e->type = EXPR_OP;
    e->u.op = op;
    e->next = NULL;

    loc = yyget_lloc(scanner);
    e->pos.first_line = loc->first_line;
    e->pos.first_column = loc->first_column;
    e->pos.last_line = loc->last_line;
    e->pos.last_column = loc->last_column;

    return e;
}

/*
 * func_new -- allocate a function
 */
func_t *
func_new(const char *id, arg_list_t *args, arg_list_t *rets,
         inner_block_t *block)
{
    func_t *f;

    f = malloc(sizeof(func_t));
    if ( NULL == f ) {
        return NULL;
    }
    f->id = strdup(id);
    if ( NULL == f->id ) {
        free(f);
        return NULL;
    }
    f->args = args;
    f->rets = rets;
    f->block = block;

    return f;
}

/*
 * coroutine_new -- allocate a coroutine
 */
coroutine_t *
coroutine_new(const char *id, arg_list_t *args, arg_list_t *rets,
              inner_block_t *block)
{
    coroutine_t *cr;

    cr = malloc(sizeof(coroutine_t));
    if ( NULL == cr ) {
        return NULL;
    }
    cr->id = strdup(id);
    if ( NULL == cr->id ) {
        free(cr);
        return NULL;
    }
    cr->args = args;
    cr->rets = rets;
    cr->block = block;

    return cr;
}


/*
 * outer_block_entry_new -- allocate an outer block entry with the specified
 * type
 */
outer_block_entry_t *
outer_block_entry_new(outer_block_entry_type_t type)
{
    outer_block_entry_t *block;

    block = malloc(sizeof(outer_block_entry_t));
    if ( NULL == block ) {
        return NULL;
    }
    block->type = type;
    block->next = NULL;

    return block;
}

/*
 * outer_block_entry_delete -- delete the outer block entry
 */
void
outer_block_entry_delete(outer_block_entry_t *block)
{
    free(block);
}

/*
 * outer_block_new -- allocate an outer block with the specified entry
 */
outer_block_t *
outer_block_new(outer_block_entry_t *ent)
{
    outer_block_t *block;

    block = malloc(sizeof(outer_block_t));
    if ( NULL == block ) {
        return NULL;
    }
    block->head = ent;
    block->tail = ent;

    return block;
}

/*
 * inner_block_new -- allocate an inner block with the specified statements
 */
inner_block_t *
inner_block_new(stmt_list_t *stmts)
{
    inner_block_t *block;

    block = malloc(sizeof(inner_block_t));
    if ( NULL == block ) {
        return NULL;
    }
    block->stmts = stmts;
    block->expr = NULL;
    block->next = NULL;

    return block;
}

/*
 * stmt_new_while
 */
stmt_t *
stmt_new_while(expr_t *cond, inner_block_t *block)
{
    stmt_t *stmt;

    stmt = malloc(sizeof(stmt_t));
    if ( NULL == stmt ) {
        return NULL;
    }
    stmt->type = STMT_WHILE;
    stmt->u.whilestmt.cond = cond;
    stmt->u.whilestmt.block = block;
    stmt->next = NULL;

    return stmt;
}

/*
 * stmt_new_expr -- allocate an expression statement
 */
stmt_t *
stmt_new_expr(expr_t *e)
{
    stmt_t *stmt;

    stmt = malloc(sizeof(stmt_t));
    if ( NULL == stmt ) {
        return NULL;
    }
    stmt->type = STMT_EXPR;
    stmt->u.expr = e;
    stmt->next = NULL;

    return stmt;
}

/*
 * stmt_new_expr_list -- allocate an expression list statement
 */
stmt_t *
stmt_new_expr_list(expr_list_t *e)
{
    stmt_t *stmt;

    stmt = malloc(sizeof(stmt_t));
    if ( NULL == stmt ) {
        return NULL;
    }
    stmt->type = STMT_EXPR_LIST;
    stmt->u.exprs = e;
    stmt->next = NULL;

    return stmt;
}

/*
 * stmt_new_return -- allocate a return statement
 */
stmt_t *
stmt_new_return(expr_t *e)
{
    stmt_t *stmt;

    stmt = malloc(sizeof(stmt_t));
    if ( NULL == stmt ) {
        return NULL;
    }
    stmt->type = STMT_RETURN;
    stmt->u.ret = e;
    stmt->next = NULL;

    return stmt;
}

/*
 * stmt_new_block -- allocate a block
 */
stmt_t *
stmt_new_block(inner_block_t *block)
{
    stmt_t *stmt;

    stmt = malloc(sizeof(stmt_t));
    if ( NULL == stmt ) {
        return NULL;
    }
    stmt->type = STMT_BLOCK;
    stmt->u.block = block;
    stmt->next = NULL;

    return stmt;
}

/*
 * stmt_list_new -- create a new statement list
 */
stmt_list_t *
stmt_list_new(stmt_t *stmt)
{
    stmt_list_t *block;

    block = malloc(sizeof(stmt_list_t));
    if ( NULL == block ) {
        return NULL;
    }
    block->head = stmt;
    block->tail = stmt;

    return block;
}

/*
 * stmt_list_append -- append a statement to the list
 */
stmt_list_t *
stmt_list_append(stmt_list_t *block, stmt_t *stmt)
{
    if ( NULL == block->head ) {
        block->head = stmt;
        block->tail = stmt;
    } else {
        block->tail->next = stmt;
        block->tail = stmt;
    }

    return block;
}

/*
 * match_arm_new -- allocate a new case block
 */
match_arm_t *
match_arm_new(expr_t *pattern, inner_block_t *block)
{
    match_arm_t *c;

    c = malloc(sizeof(match_arm_t));
    if ( NULL == c ) {
        return NULL;
    }
    c->pattern = pattern;
    c->block = block;
    c->next = NULL;

    return c;
}

/*
 * match_block_new -- allocate a new switch block
 */
match_block_t *
match_block_new(void)
{
    match_block_t *block;

    block = malloc(sizeof(match_block_t));
    if ( NULL == block ) {
        return NULL;
    }
    block->head = NULL;
    block->tail = NULL;

    return block;
}

/*
 * match_block_append -- append a switch case block to the switch block
 */
match_block_t *
match_block_append(match_block_t *block, match_arm_t *c)
{
    if ( NULL == block->head ) {
        block->head = c;
        block->tail = c;
    } else {
        block->tail->next = c;
        block->tail = c;
    }

    return block;
}

/*
 * st_new -- allocate a new syntax tree
 */
st_t *
st_new(outer_block_t *block)
{
    st_t *st;

    st = malloc(sizeof(st_t));
    if ( NULL == st ) {
        return NULL;
    }
    st->block = block;

    return st;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */

/*
 * decl_new_init -- allocate a new declaration with initializer and mut flag
 */
decl_t *
decl_new_init(const char *id, type_t *type, expr_t *init, int is_mut)
{
    decl_t *decl;
    decl = decl_new(id, type);
    if ( NULL == decl ) {
        return NULL;
    }
    decl->init = init;
    decl->is_mut = is_mut;
    return decl;
}

/*
 * type_new_reference -- create a reference type (&T or &mut T)
 */
type_t *
type_new_reference(type_t *inner, int is_mut)
{
    type_t *type;
    type = malloc(sizeof(type_t));
    if ( NULL == type ) {
        return NULL;
    }
    type->type = TYPE_REFERENCE;
    type->is_mut = is_mut;
    type->inner = inner;
    type->id = NULL;
    return type;
}

/*
 * type_new_stream -- create a stream type (stream<T>)
 */
type_t *
type_new_stream(type_t *inner)
{
    type_t *type;
    type = malloc(sizeof(type_t));
    if ( NULL == type ) {
        return NULL;
    }
    type->type = TYPE_STREAM;
    type->is_mut = 0;
    type->inner = inner;
    type->id = NULL;
    return type;
}

/*
 * type_new_chan -- create a channel type (chan<T>)
 */
type_t *
type_new_chan(type_t *inner)
{
    type_t *type;
    type = malloc(sizeof(type_t));
    if ( NULL == type ) {
        return NULL;
    }
    type->type = TYPE_CHAN;
    type->is_mut = 0;
    type->inner = inner;
    type->id = NULL;
    return type;
}

/*
 * inner_block_new_expr -- create inner block with trailing expression
 */
inner_block_t *
inner_block_new_expr(stmt_list_t *stmts, expr_t *expr)
{
    inner_block_t *block;
    block = inner_block_new(stmts);
    if ( NULL == block ) {
        return NULL;
    }
    block->expr = expr;
    return block;
}

/*
 * stmt_new_let -- create a let statement
 */
stmt_t *
stmt_new_let(decl_t *decl)
{
    stmt_t *stmt;
    stmt = malloc(sizeof(stmt_t));
    if ( NULL == stmt ) {
        return NULL;
    }
    stmt->type = STMT_LET;
    stmt->u.let_decl = decl;
    stmt->next = NULL;
    return stmt;
}

/*
 * stmt_new_reassign -- create a reassignment statement
 */
stmt_t *
stmt_new_reassign(op_t *op)
{
    stmt_t *stmt;
    stmt = malloc(sizeof(stmt_t));
    if ( NULL == stmt ) {
        return NULL;
    }
    stmt->type = STMT_REASSIGN;
    stmt->u.reassign = op;
    stmt->next = NULL;
    return stmt;
}

/*
 * stmt_new_for -- create a for statement
 */
stmt_t *
stmt_new_for(expr_t *pattern, expr_t *iter, inner_block_t *block)
{
    stmt_t *stmt;
    stmt = malloc(sizeof(stmt_t));
    if ( NULL == stmt ) {
        return NULL;
    }
    stmt->type = STMT_FOR;
    stmt->u.forstmt.pattern = pattern;
    stmt->u.forstmt.iter = iter;
    stmt->u.forstmt.block = block;
    stmt->next = NULL;
    return stmt;
}

/*
 * stmt_new_loop -- create a loop statement
 */
stmt_t *
stmt_new_loop(inner_block_t *block)
{
    stmt_t *stmt;
    stmt = malloc(sizeof(stmt_t));
    if ( NULL == stmt ) {
        return NULL;
    }
    stmt->type = STMT_LOOP;
    stmt->u.loopblock = block;
    stmt->next = NULL;
    return stmt;
}

/*
 * stmt_new_break -- create a break statement
 */
stmt_t *
stmt_new_break(void)
{
    stmt_t *stmt;
    stmt = malloc(sizeof(stmt_t));
    if ( NULL == stmt ) {
        return NULL;
    }
    stmt->type = STMT_BREAK;
    stmt->next = NULL;
    return stmt;
}

/*
 * expr_new_match -- create a match expression (replaces expr_new_switch)
 */
expr_t *
expr_new_match(void *scanner, expr_t *cond, match_block_t *block)
{
    expr_t *expr;
    expr = _expr_new(scanner);
    if ( NULL == expr ) {
        return NULL;
    }
    expr->type = EXPR_MATCH;
    expr->u.match.cond = cond;
    expr->u.match.block = block;
    return expr;
}

/*
 * expr_new_yield -- create a yield expression
 */
expr_t *
expr_new_yield(void *scanner, const char *port, expr_t *e)
{
    expr_t *expr;
    yield_t *y;

    y = malloc(sizeof(yield_t));
    if ( NULL == y ) {
        return NULL;
    }
    y->port = NULL == port ? NULL : strdup(port);
    y->expr = e;

    expr = _expr_new(scanner);
    if ( NULL == expr ) {
        free(y);
        return NULL;
    }
    expr->type = EXPR_YIELD;
    expr->u.yield_expr = y;
    return expr;
}

/*
 * expr_new_cast -- create a cast expression
 */
expr_t *
expr_new_cast(void *scanner, expr_t *e, type_t *type)
{
    expr_t *expr;
    cast_t *c;

    c = malloc(sizeof(cast_t));
    if ( NULL == c ) {
        return NULL;
    }
    c->expr = e;
    c->type = type;

    expr = _expr_new(scanner);
    if ( NULL == expr ) {
        free(c);
        return NULL;
    }
    expr->type = EXPR_CAST;
    expr->u.cast = c;
    return expr;
}

/*
 * expr_new_range -- create a range expression
 */
expr_t *
expr_new_range(void *scanner, expr_t *start, expr_t *end, range_limits_t limits)
{
    expr_t *expr;
    range_t *r;

    r = malloc(sizeof(range_t));
    if ( NULL == r ) {
        return NULL;
    }
    r->start = start;
    r->end = end;
    r->limits = limits;

    expr = _expr_new(scanner);
    if ( NULL == expr ) {
        free(r);
        return NULL;
    }
    expr->type = EXPR_RANGE;
    expr->u.range = r;
    return expr;
}

/*
 * expr_new_block -- create a block expression
 */
expr_t *
expr_new_block(void *scanner, inner_block_t *block)
{
    expr_t *expr;
    expr = _expr_new(scanner);
    if ( NULL == expr ) {
        return NULL;
    }
    expr->type = EXPR_BLOCK;
    expr->u.block = block;
    return expr;
}

/*
 * graph_decl_new -- create a new graph declaration
 */
graph_decl_t *
graph_decl_new(const char *id, graph_node_ref_t *nodes)
{
    graph_decl_t *g;
    g = malloc(sizeof(graph_decl_t));
    if (NULL == g) return NULL;
    memset(g, 0, sizeof(graph_decl_t));
    g->id = id ? strdup(id) : NULL;
    g->nodes = nodes;
    return g;
}

/*
 * graph_node_ref_new -- create a new graph node reference
 */
graph_node_ref_t *
graph_node_ref_new(const char *name, expr_list_t *args)
{
    graph_node_ref_t *n;
    n = malloc(sizeof(graph_node_ref_t));
    if (NULL == n) return NULL;
    memset(n, 0, sizeof(graph_node_ref_t));
    n->name = name ? strdup(name) : NULL;
    n->args = args;
    return n;
}

/*
 * graph_node_ref_append -- append a node to the pipe chain
 */
graph_node_ref_t *
graph_node_ref_append(graph_node_ref_t *list, graph_node_ref_t *node)
{
    if (NULL == list) return node;
    graph_node_ref_t *tail = list;
    while (tail->next) tail = tail->next;
    tail->next = node;
    return list;
}
