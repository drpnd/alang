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

/*
 * literal_new_int -- allocate an integer literal
 */
literal_t *
literal_new_int(const char *v, int type)
{
    literal_t *lit;

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

    return lit;
}

/*
 * literal_new_float -- allocate a float literal
 */
literal_t *
literal_new_float(const char *v)
{
    literal_t *lit;

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
var_new_id(var_stack_t **stack, char *id)
{
    var_t *v;
    var_stack_t *s;

    /* Check the stack */
    s = *stack;
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
    v->type = VAR_ID;
    v->u.id = strdup(id);
    if ( NULL == v->u.id ) {
        free(v);
        return NULL;
    }
    v->next = NULL;

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
    s->next = *stack;
    *stack = s;

    return v;
}

/*
 * var_new_ptr -- allocate a pointer reference variable
 */
var_t *
var_new_ptr(var_stack_t **stack, var_t *var)
{
    var_t *v;

    v = malloc(sizeof(var_t));
    if ( NULL == v ) {
        return NULL;
    }
    v->type = VAR_PTR;
    v->u.ptr = var;
    v->next = NULL;

    return v;
}

/*
 * var_new_decl -- allocate a declaration variable
 */
var_t *
var_new_decl(var_stack_t **stack, decl_t *decl)
{
    var_t *v;
    var_stack_t *s;

    /* Check the stack */
    s = *stack;
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
    v->next = NULL;

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
    s->next = *stack;
    *stack = s;

    return v;
}

/*
 * var_new_call -- allocate a call variable
 */
var_t *
var_new_call(var_t *var, expr_list_t *exprs)
{
    var_t *v;

    v = malloc(sizeof(var_t));
    if ( NULL == v ) {
        return NULL;
    }
    v->type = VAR_CALL;
    v->u.call.var = var;
    v->u.call.exprs = exprs;
    v->next = NULL;

    return v;
}

/*
 * var_new_ref -- allocate a referencevariable
 */
var_t *
var_new_ref(var_t *var, expr_t *expr)
{
    var_t *v;

    v = malloc(sizeof(var_t));
    if ( NULL == v ) {
        return NULL;
    }
    v->type = VAR_REF;
    v->u.ref.var = var;
    v->u.ref.arg = expr;
    v->next = NULL;

    return v;
}

/*
 * var_list_new -- allocate a new variable list
 */
var_list_t *
var_list_new(var_t *var)
{
    var_list_t *list;

    list = malloc(sizeof(var_list_t));
    if ( NULL == list ) {
        return NULL;
    }
    list->head = var;
    list->tail = var;

    return list;
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
 * val_new_bool -- allocate a boolean value
 */
val_t *
val_new_bool(bool_t val)
{
    val_t *v;

    v = malloc(sizeof(val_t));
    if ( NULL == v ) {
        return NULL;
    }
    v->type = VAL_BOOL;
    v->u.boolean = val;

    return v;
}

/*
 * val_new_nil -- allocate a nil value
 */
val_t *
val_new_nil(void)
{
    val_t *v;

    v = malloc(sizeof(val_t));
    if ( NULL == v ) {
        return NULL;
    }
    v->type = VAL_NIL;

    return v;
}

/*
 * val_new_variables -- allocate a literal value
 */
val_t *
val_new_variables(var_list_t *vars)
{
    val_t *v;

    v = malloc(sizeof(val_t));
    if ( NULL == v ) {
        return NULL;
    }
    v->type = VAL_VAR;
    v->u.vars = vars;

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

    return dcl;
}

/*
 * decl_list_new -- alloate a declaration entry
 */
decl_list_t *
decl_list_new(decl_t *dcl)
{
    decl_list_t *ent;

    ent = malloc(sizeof(decl_list_t));
    if ( NULL == ent ) {
        return NULL;
    }
    ent->ent = dcl;
    ent->next = NULL;

    return ent;
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
 * arg_list_new -- allocate an argument list
 */
arg_list_t *
arg_list_new(void)
{
    arg_list_t *list;

    list = malloc(sizeof(arg_list_t));
    if ( NULL == list ) {
        return NULL;
    }
    list->head = NULL;
    list->tail = NULL;

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
directive_struct_new(const char *id, decl_list_t *list)
{
    directive_t *dir;

    dir = malloc(sizeof(directive_t));
    if ( NULL == dir ) {
        return NULL;
    }
    dir->type = DIRECTIVE_STRUCT;
    dir->u.st.id = strdup(id);
    if ( NULL == dir->u.st.id ) {
        free(dir);
        return NULL;
    }
    dir->u.st.list = list;

    return dir;
}

/*
 * directive_union_new -- allocate a union data structure
 */
directive_t *
directive_union_new(const char *id, decl_list_t *list)
{
    directive_t *dir;

    dir = malloc(sizeof(directive_t));
    if ( NULL == dir ) {
        return NULL;
    }
    dir->type = DIRECTIVE_UNION;
    dir->u.un.id = strdup(id);
    if ( NULL == dir->u.un.id ) {
        free(dir);
        return NULL;
    }
    dir->u.un.list = list;

    return dir;
}

/*
 * directive_enum_new -- allocate an enum data structure
 */
directive_t *
directive_enum_new(const char *id, enum_elem_t *list)
{
    directive_t *dir;

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

    return dir;
}

/*
 * directive_typedef_new -- allocate a new use statement
 */
directive_t *
directive_typedef_new(type_t *src, type_t *dst)
{
    directive_t *dir;

    dir = malloc(sizeof(directive_t));
    if ( NULL == dir ) {
        return NULL;
    }
    dir->type = DIRECTIVE_TYPEDEF;
    dir->u.td.src = src;
    dir->u.td.dst = dst;

    return dir;
}

/*
 * directive_use_new -- allocate a new use statement
 */
directive_t *
directive_use_new(const char *id)
{
    directive_t *dir;

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
        free(e);
        return NULL;
    }
    e->type = EXPR_OP;
    e->u.op = op;
    e->next = NULL;

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
        free(e);
        return NULL;
    }
    e->type = EXPR_OP;
    e->u.op = op;
    e->next = NULL;

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

    f->vars = NULL;

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
 * module_delete -- delete the module
 */
void
module_delete(module_t *module)
{
    free(module->id);
    free(module);
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
stmt_new_assign(var_list_t *vars, expr_t *e)
{
    stmt_t *stmt;

    stmt = malloc(sizeof(stmt_t));
    if ( NULL == stmt ) {
        return NULL;
    }
    stmt->type = STMT_ASSIGN;
    stmt->u.assign.vars = vars;
    stmt->u.assign.e = e;
    stmt->next = NULL;

    return stmt;
}

/*
 * stmt_new_if
 */
stmt_t *
stmt_new_if(expr_t *cond, inner_block_t *bif, inner_block_t *belse)
{
    stmt_t *stmt;

    stmt = malloc(sizeof(stmt_t));
    if ( NULL == stmt ) {
        return NULL;
    }
    stmt->type = STMT_IF;
    stmt->u.ifstmt.cond = cond;
    stmt->u.ifstmt.bif = bif;
    stmt->u.ifstmt.belse = belse;
    stmt->next = NULL;

    return stmt;
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
 * include_new -- allocate a new include statement
 */
void *
include_new(char *s)
{
    return NULL;
}

/*
 * typedef_define -- define a type
 */
int
typedef_define(context_t *context, type_t *type1, type_t *type2)
{
    return 0;
}

/*
 * code_file_new -- allocate a new code file
 */
code_file_t *
code_file_new(outer_block_t *block)
{
    code_file_t *code;

    code = malloc(sizeof(code_file_t));
    if ( NULL == code ) {
        return NULL;
    }

    return code;
}

/*
 * code_file_init -- initialie code file
 */
int
code_file_init(code_file_t *code)
{
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
