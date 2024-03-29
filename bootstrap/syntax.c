/*_
 * Copyright (c) 2019,2021-2022 Hirochika Asai <asai@jar.jp>
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

/*
 * literal_new_int -- allocate an integer literal
 */
literal_t *
literal_new_int(void *scanner, const char *v, int type)
{
    literal_t *lit;
    YYLTYPE *loc;

    lit = malloc(sizeof(literal_t));
    if ( NULL == lit ) {
        return NULL;
    }
    lit->type = type;
    lit->u.n = strdup(v);
    if ( NULL == lit->u.n ) {
        free(lit);
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
 * literal_new_float -- allocate a float literal
 */
literal_t *
literal_new_float(void *scanner, const char *v)
{
    literal_t *lit;
    YYLTYPE *loc;

    lit = malloc(sizeof(literal_t));
    if ( NULL == lit ) {
        return NULL;
    }
    lit->type = LIT_FLOAT;
    lit->u.n = strdup(v);
    if ( NULL == lit->u.n ) {
        free(lit);
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
 * literal_new_string -- allocate a string literal
 */
literal_t *
literal_new_string(void *scanner, const char *v)
{
    literal_t *lit;
    YYLTYPE *loc;

    lit = malloc(sizeof(literal_t));
    if ( NULL == lit ) {
        return NULL;
    }
    lit->type = LIT_STRING;
    lit->u.s = strdup(v);;
    if ( NULL == lit->u.s ) {
        free(lit);
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
 * literal_new_bool -- allocate a bool literal
 */
literal_t *
literal_new_bool(void *scanner, bool_t bool)
{
    literal_t *lit;
    YYLTYPE *loc;

    lit = malloc(sizeof(literal_t));
    if ( NULL == lit ) {
        return NULL;
    }
    lit->type = LIT_BOOL;
    lit->u.b = bool;
    lit->next = NULL;

    loc = yyget_lloc(scanner);
    lit->pos.first_line = loc->first_line;
    lit->pos.first_column = loc->first_column;
    lit->pos.last_line = loc->last_line;
    lit->pos.last_column = loc->last_column;

    return lit;
}

/*
 * literal_new_nil -- allocate a nil literal
 */
literal_t *
literal_new_nil(void *scanner)
{
    literal_t *lit;
    YYLTYPE *loc;

    lit = malloc(sizeof(literal_t));
    if ( NULL == lit ) {
        return NULL;
    }
    lit->type = LIT_NIL;
    lit->next = NULL;

    loc = yyget_lloc(scanner);
    lit->pos.first_line = loc->first_line;
    lit->pos.first_column = loc->first_column;
    lit->pos.last_line = loc->last_line;
    lit->pos.last_column = loc->last_column;

    return lit;
}

/*
 * literal_release -- free a literal
 */
void
literal_release(literal_t *lit)
{
    switch ( lit->type ) {
    case LIT_OCTINT:
    case LIT_DECINT:
    case LIT_HEXINT:
    case LIT_FLOAT:
        free(lit->u.n);
        break;
    case LIT_STRING:
        free(lit->u.s);
        break;
    case LIT_BOOL:
    case LIT_NIL:
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
    t->u.id = strdup(id);
    if ( NULL == t->u.id ) {
        free(t);
        return NULL;
    }

    return t;
}

/*
 * type_new_union -- allocate a new union type
 */
type_t *
type_new_union(const char *id)
{
    type_t *t;

    t = malloc(sizeof(type_t));
    if ( NULL == t ) {
        return NULL;
    }
    t->type = TYPE_UNION;
    t->u.id = strdup(id);
    if ( NULL == t->u.id ) {
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
    t->u.id = strdup(id);
    if ( NULL == t->u.id ) {
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
    t->u.id = strdup(id);
    if ( NULL == t->u.id ) {
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
 * directive_union_new -- allocate a union data structure
 */
directive_t *
directive_union_new(void *scanner, const char *id, decl_list_t *list)
{
    directive_t *dir;
    YYLTYPE *loc;

    dir = malloc(sizeof(directive_t));
    if ( NULL == dir ) {
        return NULL;
    }
    dir->type = DIRECTIVE_UNION;
    if ( NULL != id ) {
        dir->u.un.id = strdup(id);
        if ( NULL == dir->u.un.id ) {
            free(dir);
            return NULL;
        }
    } else {
        dir->u.un.id = NULL;
    }
    dir->u.un.list = list;

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
 * directive_typedef_new -- allocate a new use statement
 */
directive_t *
directive_typedef_new(void *scanner, type_t *src, const char *dst)
{
    directive_t *dir;
    YYLTYPE *loc;

    dir = malloc(sizeof(directive_t));
    if ( NULL == dir ) {
        return NULL;
    }
    dir->type = DIRECTIVE_TYPEDEF;
    dir->u.td.src = src;
    dir->u.td.dst = strdup(dst);
    if ( NULL == dir->u.td.dst ) {
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
 * directive_use_new -- allocate a new use statement
 */
directive_t *
directive_use_new(void *scanner, const char *id)
{
    directive_t *dir;
    YYLTYPE *loc;

    dir = malloc(sizeof(directive_t));
    if ( NULL == dir ) {
        return NULL;
    }
    dir->type = DIRECTIVE_USE;
    dir->u.use.id = strdup(id);
    if ( NULL == dir->u.use.id ) {
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
    YYLTYPE *loc;

    e = malloc(sizeof(expr_t));
    if ( NULL == e ) {
        return NULL;
    }
    e->type = EXPR_ID;
    e->u.id = strdup(id);
    if ( NULL == e->u.id ) {
        free(e);
        return NULL;
    }
    e->next = NULL;

    loc = yyget_lloc(scanner);
    e->pos.first_line = loc->first_line;
    e->pos.first_column = loc->first_column;
    e->pos.last_line = loc->last_line;
    e->pos.last_column = loc->last_column;

    return e;
}

/*
 * expr_new_decl -- allocate an expression with a declaration
 */
expr_t *
expr_new_decl(void *scanner, decl_t *decl)
{
    expr_t *e;
    YYLTYPE *loc;

    e = malloc(sizeof(expr_t));
    if ( NULL == e ) {
        return NULL;
    }
    e->type = EXPR_DECL;
    e->u.decl = decl;
    e->next = NULL;

    loc = yyget_lloc(scanner);
    e->pos.first_line = loc->first_line;
    e->pos.first_column = loc->first_column;
    e->pos.last_line = loc->last_line;
    e->pos.last_column = loc->last_column;

    return e;
}

/*
 * expr_new_literal -- allocate an expression with a literal
 */
expr_t *
expr_new_literal(void *scanner, literal_t *lit)
{
    expr_t *e;
    YYLTYPE *loc;

    e = malloc(sizeof(expr_t));
    if ( NULL == e ) {
        return NULL;
    }
    e->type = EXPR_LITERAL;
    e->u.lit = lit;
    e->next = NULL;

    loc = yyget_lloc(scanner);
    e->pos.first_line = loc->first_line;
    e->pos.first_column = loc->first_column;
    e->pos.last_line = loc->last_line;
    e->pos.last_column = loc->last_column;

    return e;
}

/*
 * expr_new_member -- allocate a member reference expression
 */
expr_t *
expr_new_member(void *scanner, expr_t *pe, const char *id)
{
    expr_t *e;
    YYLTYPE *loc;

    e = malloc(sizeof(expr_t));
    if ( NULL == e ) {
        return NULL;
    }
    e->type = EXPR_MEMBER;
    e->u.mem.e = pe;
    e->u.mem.id = strdup(id);
    if ( NULL == e->u.mem.id ) {
        free(e);
        return NULL;
    }
    e->next = NULL;

    loc = yyget_lloc(scanner);
    e->pos.first_line = loc->first_line;
    e->pos.first_column = loc->first_column;
    e->pos.last_line = loc->last_line;
    e->pos.last_column = loc->last_column;

    return e;
}

/*
 * expr_new_call -- allocate a call expression
 */
expr_t *
expr_new_call(void *scanner, expr_t *callee, expr_list_t *exprs)
{
    expr_t *e;
    YYLTYPE *loc;

    e = malloc(sizeof(expr_t));
    if ( NULL == e ) {
        return NULL;
    }
    e->u.call = malloc(sizeof(call_t));
    if ( e->u.call ) {
        free(e);
        return NULL;
    }
    e->type = EXPR_CALL;
    e->u.call->callee = callee;
    e->u.call->exprs = exprs;
    e->next = NULL;

    loc = yyget_lloc(scanner);
    e->pos.first_line = loc->first_line;
    e->pos.first_column = loc->first_column;
    e->pos.last_line = loc->last_line;
    e->pos.last_column = loc->last_column;

    return e;
}

/*
 * expr_new_ref -- allocate a reference expression
 */
expr_t *
expr_new_ref(void *scanner, expr_t *var, expr_t *expr)
{
    expr_t *e;
    YYLTYPE *loc;

    e = malloc(sizeof(expr_t));
    if ( NULL == e ) {
        return NULL;
    }
    e->u.ref = malloc(sizeof(ref_t));
    if ( e->u.call ) {
        free(e);
        return NULL;
    }
    e->type = EXPR_REF;
    e->u.ref->var = var;
    e->u.ref->arg = expr;
    e->next = NULL;

    loc = yyget_lloc(scanner);
    e->pos.first_line = loc->first_line;
    e->pos.first_column = loc->first_column;
    e->pos.last_line = loc->last_line;
    e->pos.last_column = loc->last_column;

    return e;
}

/*
 * expr_new_switch -- allocate a switch expression
 */
expr_t *
expr_new_switch(void *scanner, expr_t *cond, switch_block_t *block)
{
    expr_t *e;
    YYLTYPE *loc;

    e = malloc(sizeof(expr_t));
    if ( NULL == e ) {
        return NULL;
    }
    e->type = EXPR_SWITCH;
    e->u.sw.cond = cond;
    e->u.sw.block = block;
    e->next = NULL;

    loc = yyget_lloc(scanner);
    e->pos.first_line = loc->first_line;
    e->pos.first_column = loc->first_column;
    e->pos.last_line = loc->last_line;
    e->pos.last_column = loc->last_column;

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
    YYLTYPE *loc;

    e = malloc(sizeof(expr_t));
    if ( NULL == e ) {
        return NULL;
    }
    e->type = EXPR_IF;
    e->u.ife.cond = cond;
    e->u.ife.bif = bif;
    e->u.ife.belse = belse;
    e->next = NULL;

    loc = yyget_lloc(scanner);
    e->pos.first_line = loc->first_line;
    e->pos.first_column = loc->first_column;
    e->pos.last_line = loc->last_line;
    e->pos.last_column = loc->last_column;

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
 * module_new -- allocate a module
 */
module_t *
module_new(const char *id, outer_block_t *block)
{
    module_t *m;

    m = malloc(sizeof(module_t));
    if ( NULL == m ) {
        return NULL;
    }
    m->id = strdup(id);
    if ( NULL == m->id ) {
        free(m);
        return NULL;
    }
    m->block = block;

    return m;
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
 * switch_case_new -- allocate a new case block
 */
switch_case_t *
switch_case_new(literal_set_t *set, inner_block_t *block)
{
    switch_case_t *c;

    c = malloc(sizeof(switch_case_t));
    if ( NULL == c ) {
        return NULL;
    }
    c->lset = set;
    c->block = block;
    c->next = NULL;

    return c;
}

/*
 * switch_block_new -- allocate a new switch block
 */
switch_block_t *
switch_block_new(void)
{
    switch_block_t *block;

    block = malloc(sizeof(switch_block_t));
    if ( NULL == block ) {
        return NULL;
    }
    block->head = NULL;
    block->tail = NULL;

    return block;
}

/*
 * switch_block_append -- append a switch case block to the switch block
 */
switch_block_t *
switch_block_append(switch_block_t *block, switch_case_t *c)
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
 * module_vec_add -- add a module block to the module vector
 */
int
module_vec_add(module_vec_t *vec, module_t *module)
{
    module_t **nvec;
    size_t resized;

    if ( vec->n >= vec->size ) {
        /* Resize */
        resized = vec->size + VECTOR_DELTA;
        nvec = realloc(vec->vec, resized * sizeof(module_t *));
        if ( NULL == nvec ) {
            return -1;
        }
        vec->vec = nvec;
        vec->size = resized;
    }
    vec->vec[vec->n] = module;
    vec->n++;

    return 0;
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
