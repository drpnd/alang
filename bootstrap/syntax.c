/*_
 * Copyright (c) 2019,2021 Hirochika Asai <asai@jar.jp>
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Global variable */
var_stack_t *stack;

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
 * var_new_id -- allocate an ID variable
 */
var_t *
var_new_id(char *id, int ptr)
{
    var_t *v;
    var_stack_t *s;

    /* Check the stack */
    s = stack;
    while ( NULL != s ) {
        if ( 0 == strcmp(s->id, id) ) {
            /* Found */
            return s->var;
        }
        s = s->next;
    }

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

    s = malloc(sizeof(var_stack_t));
    if ( NULL == s ) {
        free(v->u.id);
        free(v);
        return NULL;
    }
    s->id = strdup(id);
    if ( NULL == s->id ) {
        free(v->u.id);
        free(v);
        free(s);
        return NULL;
    }
    s->var = v;
    s->next = stack;
    stack = s;

    return v;
}

/*
 * var_new_decl -- allocate a declaration variable
 */
var_t *
var_new_decl(decl_t *decl)
{
    var_t *v;
    var_stack_t *s;

    /* Check the stack */
    s = stack;
    while ( NULL != s ) {
        if ( 0 == strcmp(s->id, decl->id) ) {
            /* Already defined */
            fprintf(stderr, "Variable %s is already defined.\n", s->id);
            return NULL;
        }
        s = s->next;
    }

    v = malloc(sizeof(var_t));
    if ( NULL == v ) {
        return NULL;
    }
    v->type = VAR_DECL;
    v->u.decl = decl;

    s = malloc(sizeof(var_stack_t));
    if ( NULL == s ) {
        free(v);
        return NULL;
    }
    s->id = strdup(decl->id);
    if ( NULL == s->id ) {
        free(v);
        free(s);
        return NULL;
    }
    s->var = v;
    s->next = stack;
    stack = s;

    return v;
}

/*
 * val_new_literal -- allocate a literal value
 */
val_t *
val_new_literal(literal_t *lit)
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
 * val_new_variable -- allocate a literal value
 */
val_t *
val_new_variable(var_t *var)
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
 * expr_new_val -- allocate an expression with a value
 */
expr_t *
expr_new_val(val_t *val)
{
    expr_t *e;

    e = malloc(sizeof(expr_t));
    if ( NULL == e ) {
        return NULL;
    }
    e->type = EXPR_VAL;
    e->u.val = val;

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
 * expr_op_new_infix -- allocate an infix operation expression
 */
expr_t *
expr_op_new_infix(expr_t *e0, expr_t *e1, op_type_t type)
{
    op_t *op;
    expr_t *e;

    e = malloc(sizeof(expr_t));
    if ( NULL == e ) {
        return NULL;
    }
    op = op_new_infix(e0, e1, type);
    if ( NULL == op ) {
        return NULL;
    }
    e->type = EXPR_OP;
    e->u.op = op;

    return e;
}

/*
 * expr_op_new_prefix -- allocate a prefixed operation expression
 */
expr_t *
expr_op_new_prefix(expr_t *e0, op_type_t type)
{
    op_t *op;
    expr_t *e;

    e = malloc(sizeof(expr_t));
    if ( NULL == e ) {
        return NULL;
    }
    op = op_new_prefix(e0, type);
    if ( NULL == op ) {
        return NULL;
    }
    e->type = EXPR_OP;
    e->u.op = op;

    return e;
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

    f->vars = stack;

    return f;
}

/*
 * coroutine_new -- allocate a coroutine
 */
coroutine_t *
coroutine_new(char *id, arg_t *args, arg_t *rets, stmt_list_t *block)
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
 * stmt_new_decl -- allocate a declaration statement
 */
stmt_t *
stmt_new_decl(decl_t *decl)
{
    stmt_t *stmt;

    stmt = malloc(sizeof(stmt_t));
    if ( NULL == stmt ) {
        return NULL;
    }
    stmt->type = STMT_DECL;
    stmt->u.decl = decl;
    stmt->next = NULL;

    return stmt;
}

/*
 * stmt_new_assign -- allocate an assign statement
 */
stmt_t *
stmt_new_assign(var_t *var, expr_t *e)
{
    stmt_t *stmt;

    stmt = malloc(sizeof(stmt_t));
    if ( NULL == stmt ) {
        return NULL;
    }
    stmt->type = STMT_ASSIGN;
    stmt->u.assign.var = var;
    stmt->u.assign.e = e;
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
 * stmt_prepend -- prepend a statement to the list
 */
stmt_list_t *
stmt_prepend(stmt_t *stmt, stmt_list_t *block)
{
    stmt->next = block->head;
    block->head = stmt;

    return block;
}

/*
 * func_vec_add -- add a function to the function vector
 */
int
func_vec_add(func_vec_t *vec, func_t *func)
{
    func_t **nvec;
    size_t resized;

    if ( vec->n >= vec->size ) {
        /* Resize */
        resized = vec->size + VECTOR_DELTA;
        nvec = realloc(vec->vec, resized * sizeof(func_t *));
        if ( NULL == nvec ) {
            return -1;
        }
        vec->vec = nvec;
        vec->size = resized;
    }
    vec->vec[vec->n] = func;
    vec->n++;

    return 0;
}

/*
 * coroutine_vec_add -- add a coroutine to the coroutine vector
 */
int
coroutine_vec_add(coroutine_vec_t *vec, coroutine_t *cr)
{
    coroutine_t **nvec;
    size_t resized;

    if ( vec->n >= vec->size ) {
        /* Resize */
        resized = vec->size + VECTOR_DELTA;
        nvec = realloc(vec->vec, resized * sizeof(coroutine_t *));
        if ( NULL == nvec ) {
            return -1;
        }
        vec->vec = nvec;
        vec->size = resized;
    }
    vec->vec[vec->n] = cr;
    vec->n++;

    return 0;
}

/*
 * import_vec_add -- add an import statement to the import vector
 */
int
import_vec_add(import_vec_t *vec, import_t *import)
{
    import_t **nvec;
    size_t resized;

    if ( vec->n >= vec->size ) {
        /* Resize */
        resized = vec->size + VECTOR_DELTA;
        nvec = realloc(vec->vec, resized * sizeof(import_t *));
        if ( NULL == nvec ) {
            return -1;
        }
        vec->vec = nvec;
        vec->size = resized;
    }
    vec->vec[vec->n] = import;
    vec->n++;

    return 0;
}

/*
 * import_new -- allocate a new import statement
 */
import_t *
import_new(char *id)
{
    import_t *import;

    import = malloc(sizeof(import_t));
    if ( NULL == import ) {
        return NULL;
    }
    import->id = strdup(id);
    if ( NULL == import->id ) {
        free(import);
        return NULL;
    }

    return import;
}

/*
 * include_new -- allocate a new include statement
 */
void *
include_new(char *s)
{
    return NULL;
}

/*
 * Define package
 */
int
package_define(code_file_t *code, const char *package)
{
    if ( NULL != code->package ) {
        return -1;
    }
    code->package = strdup(package);
    if ( NULL == code->package ) {
        return -1;
    }

    return 0;
}

/*
 * code_file_init -- initialie code file
 */
int
code_file_init(code_file_t *code)
{
    code->package = NULL;

    /* Initialize functions */
    code->funcs.n = 0;
    code->funcs.size = VECTOR_INIT_SIZE;
    code->funcs.vec = malloc(VECTOR_INIT_SIZE * sizeof(func_t *));
    if ( NULL == code->funcs.vec ) {
        return -1;
    }

    /* Initialize coroutines */
    code->coroutines.n = 0;
    code->coroutines.size = VECTOR_INIT_SIZE;
    code->coroutines.vec = malloc(VECTOR_INIT_SIZE * sizeof(coroutine_t *));
    if ( NULL == code->coroutines.vec ) {
        free(code->funcs.vec);
        return -1;
    }

    /* Initialize imports */
    code->imports.n = 0;
    code->imports.size = VECTOR_INIT_SIZE;
    code->imports.vec = malloc(VECTOR_INIT_SIZE * sizeof(import_t *));
    if ( NULL == code->imports.vec ) {
        free(code->funcs.vec);
        free(code->coroutines.vec);
        return -1;
    }

    return 0;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
