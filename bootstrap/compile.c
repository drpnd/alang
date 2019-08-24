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
#include "compile.h"
#include <stdio.h>
#include <stdlib.h>

/*
 * compile_op -- compile an operator
 */
int
compile_op(compiler_t *c, op_t *op)
{
    switch ( op->fix ) {
    case FIX_INFIX:
        (void)op->e0;
        (void)op->e1;
        break;
    case FIX_PREFIX:
        (void)op->e0;
        break;
    }

    return 0;
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
 * compile_stmt -- compile a statement
 */
int
compile_stmt(compiler_t *c, stmt_t *s)
{
    switch ( s->type ) {
    case STMT_DECL:
        /* Type declaration */
        break;
    case STMT_ASSIGN:
        /* Assign variable */
        break;
    case STMT_EXPR:
        /* Experssion */
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
    stmt_t *s;
    int ret;
    (void)c;
    (void)fn->id;

    /* All statements in the block */
    s = fn->block->head;
    while ( NULL != s ) {
        ret = compile_stmt(c, s);
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
    stmt_t *s;
    (void)c;
    (void)cr->id;

    /* All statements in the block */
    s = cr->block->head;
    while ( NULL != s ) {
        compile_stmt(c, s);
        /* Next statement */
        s = s->next;
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
    int ret;

    /* Allocate compiler */
    c = malloc(sizeof(compiler_t));
    if ( NULL == c ) {
        return EXIT_FAILURE;
    }
    c->fout = NULL;

    /* Test output */
    ssize_t i;
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
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
