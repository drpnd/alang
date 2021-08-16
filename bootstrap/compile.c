/*_
 * Copyright (c) 2019-2021 Hirochika Asai <asai@jar.jp>
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
#include "compile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if 0
int compile_expr(compiler_t *, expr_t *);

/*
 * compile_infix -- compile an infix operator
 */
int
compile_infix(compiler_t *c, op_type_t type, expr_t *e0, expr_t *e1)
{
    int ret;

    ret = compile_expr(c, e0);
    if ( ret < 0 ) {
        return ret;
    }
    ret = compile_expr(c, e1);
    if ( ret < 0 ) {
        return ret;
    }

    switch ( type ) {
    case OP_ADD:
        break;
    case OP_SUB:
        break;
    case OP_MUL:
        break;
    case OP_DIV:
        break;
    default:
        return -1;
    }

    return 0;
}

/*
 * compile_prefix -- compile a prefix operator
 */
int
compile_prefix(compiler_t *c, op_type_t type, expr_t *e)
{
    return -1;
}

/*
 * compile_op -- compile an operator
 */
int
compile_op(compiler_t *c, op_t *op)
{
    int ret;

    switch ( op->fix ) {
    case FIX_INFIX:
        ret = compile_infix(c, op->type, op->e0, op->e1);
        break;
    case FIX_PREFIX:
        ret = compile_prefix(c, op->type, op->e0);
        break;
    default:
        ret = -1;
    }

    return ret;
}

/*
 * compile_expr -- compile an expression
 */
int
compile_expr(compiler_t *c, expr_t *e)
{
    int ret;

    switch ( e->type ) {
    case EXPR_VAL:
        /* Value */
        switch ( e->u.val->type ) {
        case VAL_LITERAL:
            break;
        case VAL_VAR:
            break;
        }
        break;
    case EXPR_OP:
        /* Operator */
        ret = compile_op(c, e->u.op);
        if ( ret < 0 ) {
            return ret;
        }
        break;
    }

    return 0;
}

/*
 * compile_assign -- compile an assign statement
 */
int
compile_assign(compiler_t *c, stmt_assign_t *s)
{
    int ret;

    /* Variable */
    switch ( s->var->type ) {
    case VAR_ID:
        printf("var id %s\n", s->var->u.id);
        break;
    case VAR_PTR:
        printf("var ptr\n");
        break;
    case VAR_DECL:
        printf("var decl %s\n", s->var->u.decl->id);
        break;
    default:
        return -1;
    }
    ret = compile_expr(c, s->e);
    if ( ret < 0 ) {
        return ret;
    }

    return 0;
}

/*
 * compile_stmt -- compile a statement
 */
int
compile_stmt(compiler_t *c, compiler_block_t *b, stmt_t *s)
{
    int ret;

    switch ( s->type ) {
    case STMT_DECL:
        /* Type declaration */
        return -1;
        break;
    case STMT_ASSIGN:
        /* Assign variable */
        ret = compile_assign(c, &s->u.assign);
        if ( ret < 0 ) {
            return -1;
        }
        break;
    case STMT_EXPR:
        /* Experssion */
        return -1;
        break;
    }

    return 0;
}

/*
 * compile_func -- compile a function
 */
int
compile_func(compiler_t *c, func_t *fn)
{
    compiler_block_t *b;
    stmt_t *s;
    int ret;
    (void)c;
    (void)fn->id;

    /* Allocate a block */
    b = malloc(sizeof(compiler_block_t));
    b->type = BLOCK_FUNC;
    b->label = strdup(fn->id);
    if ( NULL == b->label ) {
        free(b);
        return -1;
    }

#if 0
    /* All statements in the block */
    s = fn->block->head;
    while ( NULL != s ) {
        ret = compile_stmt(c, b, s);
        if ( ret < 0 ) {
            return ret;
        }
        /* Next statement */
        s = s->next;
    }
#endif

    return 0;
}

/*
 * compile_coroutine -- compile a coroutine
 */
int
compile_coroutine(compiler_t *c, coroutine_t *cr)
{
    compiler_block_t *b;
    stmt_t *s;
    int ret;
    (void)c;
    (void)cr->id;

    /* Allocate a block */
    b = malloc(sizeof(compiler_block_t));
    b->type = BLOCK_FUNC;
    b->label = strdup(cr->id);
    if ( NULL == b->label ) {
        free(b);
        return -1;
    }

#if 0
    /* All statements in the block */
    s = cr->block->head;
    while ( NULL != s ) {
        ret = compile_stmt(c, b, s);
        if ( ret < 0 ) {
            return ret;
        }
        /* Next statement */
        s = s->next;
    }
#endif

    return 0;
}

/*
 * compile_code -- compile code
 */
int
compile_code(compiler_t *c, code_file_t *code)
{
    int ret;
    ssize_t i;

    /* Test output */
    for ( i = 0; i < (ssize_t)code->funcs.n; i++ ) {
        printf("func: %s\n", code->funcs.vec[i]->id);
        ret = compile_func(c, code->funcs.vec[i]);
        if ( ret < 0 ) {
            fprintf(stderr, "Compile error\n");
        }
    }
    for ( i = 0; i < (ssize_t)code->coroutines.n; i++ ) {
        printf("coroutine: %s\n", code->coroutines.vec[i]->id);
    }

    return 0;
}
#endif

/* Declarations */
static int _inner_block(compiler_t *, compiler_env_t *, inner_block_t *);

/*
 * _env_new -- allocate a new environment
 */
static compiler_env_t *
_env_new(compiler_t *c)
{
    compiler_env_t *env;

    env = malloc(sizeof(compiler_env_t));
    if ( NULL == env ) {
        return NULL;
    }
    env->vars = malloc(sizeof(compiler_var_table_t));
    if ( NULL == env->vars ) {
        free(env);
        return NULL;
    }
    env->vars->top = NULL;
    env->prev = NULL;

    return env;
}

/*
 * _var_new -- allocate a new variable
 */
static compiler_var_t *
_var_new(const char *id, type_t *type)
{
    compiler_var_t *var;

    var = malloc(sizeof(compiler_var_t));
    if ( NULL == var ) {
        return NULL;
    }
    var->id = strdup(id);
    if ( NULL == var->id ) {
        free(var);
        return NULL;
    }
    var->type = type;

    return var;
}

/*
 * _var_delete -- deallocate the specified variable
 */
static void
_var_delete(compiler_var_t *var)
{
    free(var->id);
    free(var);
}

/*
 * _var_push -- push a variable to the stack
 */
static int
_var_push(compiler_env_t *env, compiler_var_t *var)
{
    var->next = env->vars->top;
    env->vars->top = var;

    return 0;
}

/*
 * _var_search -- search a variable from the stack
 */
static compiler_var_t *
_var_search(compiler_env_t *env, const char *id)
{
    compiler_var_t *var;

    var = env->vars->top;
    while ( NULL != var ) {
        if ( 0 == strcmp(id, var->id) ) {
            /* Found a variable with the specified identifier */
            return var;
        }
        var = var->next;
    }

    /* Not found */
    return NULL;
}

/*
 * _decl -- parse a declaration
 */
static int
_decl(compiler_t *c, compiler_env_t *env, decl_t *decl)
{
    compiler_var_t *var;
    int ret;

    /* Add a new variable */
    var = _var_new(decl->id, decl->type);
    if ( NULL == var ) {
        return -1;
    }
    ret = _var_push(env, var);
    if ( ret < 0 ) {
        _var_delete(var);
        return -1;
    }

    return 0;
}

/*
 * _args -- parse arguments
 */
static int
_args(compiler_t *c, compiler_env_t *env, arg_list_t *args)
{
    arg_t *a;
    int ret;

    a = args->head;
    while ( NULL != a ) {
        ret = _decl(c, env, a->decl);
        if ( ret < 0 ) {
            return -1;
        }
        a = a->next;
    }

    return 0;
}

/*
 * _expr -- parse an expression
 */
static int
_expr(compiler_t *c, compiler_env_t *env, expr_t *e)
{
    return -1;
}

/*
 * _expr_list -- parse an expression list
 */
static int
_expr_list(compiler_t *c, compiler_env_t *env, expr_list_t *exprs)
{
    return -1;
}

/*
 * _while -- parse a while statement
 */
static int
_while(compiler_t *c, compiler_env_t *env, stmt_while_t *w)
{
    return -1;
}

/*
 * _return -- parse a return statement
 */
static int
_return(compiler_t *c, compiler_env_t *env, expr_t *e)
{
    return -1;
}

/*
 * _stmt -- parse a statement
 */
static int
_stmt(compiler_t *c, compiler_env_t *env, stmt_t *stmt)
{
    int ret;
    compiler_env_t *nenv;

    ret = -1;
    switch ( stmt->type ) {
    case STMT_WHILE:
        ret = _while(c, env, &stmt->u.whilestmt);
        break;
    case STMT_EXPR:
        ret = _expr(c, env, stmt->u.expr);
        break;
    case STMT_EXPR_LIST:
        ret = _expr_list(c, env, stmt->u.exprs);
        break;
    case STMT_BLOCK:
        /* Create a new environemt */
        nenv = _env_new(c);
        if ( NULL == nenv ) {
            return -1;
        }
        nenv->prev = env;
        ret = _inner_block(c, nenv, stmt->u.block);
        break;
    case STMT_RETURN:
        ret = _return(c, env, stmt->u.expr);
        break;
    }

    return ret;
}

/*
 * _inner_block -- parse an inner block
 */
static int
_inner_block(compiler_t *c, compiler_env_t *env, inner_block_t *block)
{
    stmt_t *stmt;
    int ret;

    stmt = block->stmts->head;
    while ( NULL != stmt ) {
        ret = _stmt(c, env, stmt);
        if ( ret < 0 ) {
            return -1;
        }
        stmt = stmt->next;
    }

    return 0;
}

/*
 * _func -- parse a function
 */
static int
_func(compiler_t *c, func_t *fn)
{
    int ret;
    compiler_env_t *env;

    /* Allocate a new environment */
    env = _env_new(c);
    if ( NULL == env ) {
        return -1;
    }

    /* Parse arguments and return values */
    ret = _args(c, env, fn->args);
    if ( ret < 0 ) {
        return -1;
    }
    ret = _args(c, env, fn->rets);
    if ( ret < 0 ) {
        return -1;
    }

    /* Parse the inner block */
    ret = _inner_block(c, env, fn->block);
    if ( ret < 0 ) {
        return -1;
    }

    return 0;
}

/*
 * _directive -- parse a directive
 */
static int
_directive(compiler_t *c, directive_t *dr)
{
    switch ( dr->type ) {
    case DIRECTIVE_USE:
        break;
    case DIRECTIVE_STRUCT:
        break;
    case DIRECTIVE_UNION:
        break;
    case DIRECTIVE_ENUM:
        break;
    case DIRECTIVE_TYPEDEF:
        break;
    }
    return -1;
}

/*
 * _outer_block_entry -- parse an outer block entry
 */
static int
_outer_block_entry(compiler_t *c, outer_block_entry_t *e)
{
    int ret;

    ret = -1;
    switch ( e->type ) {
    case OUTER_BLOCK_FUNC:
        ret = _func(c, e->u.fn);
        break;
    case OUTER_BLOCK_COROUTINE:
        //_coroutine(e->u.cr);
        break;
    case OUTER_BLOCK_MODULE:
        //_module(e->u.md);
        break;
    case OUTER_BLOCK_DIRECTIVE:
        ret = _directive(c, e->u.dr);
        break;
    }

    return ret;
}

/*
 * _outer_block -- compile an outer block
 */
static int
_outer_block(compiler_t *c, outer_block_t *block)
{
    compiler_env_t *env;
    outer_block_entry_t *e;

    /* Allocate the environment data structure for this block (i.e., scope) */
    env = malloc(sizeof(compiler_env_t));
    if ( NULL == env ) {
        return -1;
    }
    env->vars = NULL;

    /* Parse all outer block entries */
    e = block->head;
    while ( NULL != e ) {
        _outer_block_entry(c, e);
        e = e->next;
    }

    return 0;
}

/*
 * compile_code -- compile code
 */
int
compile_code(compiler_t *c, code_file_t *code)
{
    return _outer_block(c, code->block);
}

/*
 * compile -- compiile code
 */
int
compile(code_file_t *code)
{
    compiler_t *c;

    /* Allocate compiler */
    c = malloc(sizeof(compiler_t));
    if ( NULL == c ) {
        return EXIT_FAILURE;
    }
    c->fout = NULL;

    compile_code(c, code);

    return 0;
}

/*
　* compile_use_extern -- use an external package
　*/
int
compile_use_extern(context_t *context, const char *id)
{
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
