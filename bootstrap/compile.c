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
