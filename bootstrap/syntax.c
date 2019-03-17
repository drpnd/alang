/*_
 * Copyright (c) 2019 Hirochika Asai <asai@jar.jp>
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
#include <stdlib.h>
#include <string.h>

/*
 * literal_new_int -- allocate an integer literal
 */
literal_t *
literal_new_int(int v)
{
    literal_t *lit;

    lit = malloc(sizeof(literal_t));
    if ( NULL == lit ) {
        return NULL;
    }
    lit->type = LIT_INT;
    lit->u.i = v;

    return lit;
}

/*
 * literal_new_float -- allocate a float literal
 */
literal_t *
literal_new_float(float v)
{
    literal_t *lit;

    lit = malloc(sizeof(literal_t));
    if ( NULL == lit ) {
        return NULL;
    }
    lit->type = LIT_FLOAT;
    lit->u.f = v;

    return lit;
}

/*
 * literal_new_string -- allocate a string literal
 */
literal_t *
literal_new_string(const char *v)
{
    literal_t *lit;

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

    return lit;
}

/*
 * var_id_new -- allocate an ID variable
 */
var_t *
var_id_new(char *id, int ptr)
{
    var_t *v;

    v = malloc(sizeof(var_t));
    if ( NULL == v ) {
        return NULL;
    }
    v->type = ptr ? VAR_PTR : VAR_ID;
    v->u.id = strdup(id);
    if ( NULL == v->u.id ) {
        free(v);
        return NULL;
    }

    return v;
}

/*
 * val_literal_new -- allocate a literal value
 */
val_t *
val_literal_new(literal_t *lit)
{
    val_t *v;

    v = malloc(sizeof(val_t));
    if ( NULL == v ) {
        return NULL;
    }
    v->type = VAL_LITERAL;
    v->u.lit = lit;

    return v;
}

/*
 * val_variable_new -- allocate a literal value
 */
val_t *
val_variable_new(var_t *var)
{
    val_t *v;

    v = malloc(sizeof(val_t));
    if ( NULL == v ) {
        return NULL;
    }
    v->type = VAL_VAR;
    v->u.var = var;

    return v;
}

/*
 * type_primitive_new -- allocate a type
 */
type_t *
type_primitive_new(type_type_t tt)
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
 * type_id_new -- allocate a type
 */
type_t *
type_id_new(type_type_t tt, const char *id)
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

    return dcl;
}

/*
 * arg_new -- allocate an argument
 */
arg_t *
arg_new(decl_t *dcl)
{
    arg_t *arg;

    arg = malloc(sizeof(arg_t));
    if ( NULL == arg ) {
        return NULL;
    }
    arg->decl = dcl;
    arg->next = NULL;

    return arg;
}

/*
 * arg_prepend -- prepend an argument to the list
 */
arg_t *
arg_prepend(arg_t *arg, arg_t *args)
{
    arg->next = args;
    return arg;
}

/*
 * expr_new -- allocate an expression
 */
expr_t *
expr_new(void)
{
    expr_t *e;

    e = malloc(sizeof(expr_t));
    if ( NULL == e ) {
        return NULL;
    }

    return e;
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
 * func_new -- allocate a function
 */
func_t *
func_new(char *id, arg_t *args, arg_t *rets, stmt_list_t *block)
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
 * stmt_new_expr -- allocate a statement
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
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
