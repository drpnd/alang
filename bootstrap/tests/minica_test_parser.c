/*_
 * Copyright (c) 2021 Hirochika Asai <asai@jar.jp>
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

#include "../compile.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/*
 * usage -- print usage and exit
 */
void
usage(const char *prog)
{
    fprintf(stderr, "Usage: %s <file>\n", prog);
    exit(EXIT_FAILURE);
}

code_file_t * minica_parse(FILE *);

/* Declarations */
static void _expr(expr_t *);
static void _expr_list(expr_list_t *);
static void _inner_block(inner_block_t *);

static char *
_type(type_t *t)
{
    switch ( t->type ) {
    case TYPE_PRIMITIVE_I8:
        return "i8";
    case TYPE_PRIMITIVE_U8:
        return "u8";
    case TYPE_PRIMITIVE_I16:
        return "i16";
    case TYPE_PRIMITIVE_U16:
        return "u16";
    case TYPE_PRIMITIVE_I32:
        return "i32";
    case TYPE_PRIMITIVE_U32:
        return "u32";
    case TYPE_PRIMITIVE_I64:
        return "i64";
    case TYPE_PRIMITIVE_U64:
        return "u64";
    case TYPE_PRIMITIVE_FP32:
        return "fp32";
    case TYPE_PRIMITIVE_FP64:
        return "fp64";
    case TYPE_PRIMITIVE_STRING:
        return "string";
    case TYPE_PRIMITIVE_BOOL:
        return "bool";
    case TYPE_STRUCT:
        return "struct";
    case TYPE_UNION:
        return "union";
    case TYPE_ENUM:
        return "enum";
    case TYPE_ID:
        return t->u.id;
    }
    return "(unknown type)";
}

static void
_decl(decl_t *decl)
{
    printf("%s : %s", decl->id, _type(decl->type));
}

static void
_args(arg_list_t *args)
{
    arg_t *a;

    a = args->head;
    while ( NULL != a ) {
        _decl(a->decl);
        a = a->next;
    }
}

static void
_assign(op_t *op)
{
    if ( FIX_INFIX != op->fix ) {
        printf("error\n");
        exit(EXIT_FAILURE);
    }

    _expr(op->e0);
    printf(" := ");
    _expr(op->e1);
}

static void
_add(op_t *op)
{
    switch ( op->fix ) {
    case FIX_PREFIX:
        printf("+");
        _expr(op->e0);
        break;
    case FIX_INFIX:
        _expr(op->e0);
        printf("+");
        _expr(op->e1);
        break;
    default:
        printf("Error\n");
        exit(EXIT_FAILURE);
    }
}

static void
_sub(op_t *op)
{
    switch ( op->fix ) {
    case FIX_PREFIX:
        printf("-");
        _expr(op->e0);
        break;
    case FIX_INFIX:
        _expr(op->e0);
        printf("-");
        _expr(op->e1);
        break;
    default:
        printf("Error\n");
        exit(EXIT_FAILURE);
    }
}

static void
_mul(op_t *op)
{
    if ( FIX_INFIX != op->fix ) {
        printf("Error\n");
        exit(EXIT_FAILURE);
    }
    _expr(op->e0);
    printf("*");
    _expr(op->e1);
}

static void
_div(op_t *op)
{
    if ( FIX_INFIX != op->fix ) {
        printf("Error\n");
        exit(EXIT_FAILURE);
    }
    _expr(op->e0);
    printf("/");
    _expr(op->e1);
}

static void
_mod(op_t *op)
{
    if ( FIX_INFIX != op->fix ) {
        printf("Error\n");
        exit(EXIT_FAILURE);
    }
    _expr(op->e0);
    printf("%%");
    _expr(op->e1);
}

static void
_op(op_t *op)
{
    switch ( op->type ) {
    case OP_ASSIGN:
        _assign(op);
        break;
    case OP_ADD:
        _add(op);
        break;
    case OP_SUB:
        _sub(op);
        break;
    case OP_MUL:
        _mul(op);
        break;
    case OP_DIV:
        _div(op);
        break;
    case OP_MOD:
        _mod(op);
        break;
    case OP_NOT:
        printf("!\n");
        break;
    case OP_AND:
        printf("&\n");
        break;
    case OP_OR:
        printf("|\n");
        break;
    case OP_XOR:
        printf("^\n");
        break;
    case OP_COMP:
        printf("~\n");
        break;
    case OP_LSHIFT:
        printf("<<\n");
        break;
    case OP_RSHIFT:
        printf(">>\n");
        break;
    case OP_CMP_EQ:
        printf("==\n");
        break;
    case OP_CMP_NEQ:
        printf("!=\n");
        break;
    case OP_CMP_GT:
        printf(">\n");
        break;
    case OP_CMP_LT:
        printf("<\n");
        break;
    case OP_CMP_GEQ:
        printf(">=\n");
        break;
    case OP_CMP_LEQ:
        printf("<=\n");
        break;
    case OP_INC:
        printf("++\n");
        break;
    case OP_DEC:
        printf("--\n");
        break;
    }
}

static void
_literal(literal_t *lit)
{
    switch ( lit->type ) {
    case LIT_HEXINT:
        printf("0x%s", lit->u.n);
        break;
    case LIT_DECINT:
        printf("%s", lit->u.n);
        break;
    case LIT_OCTINT:
        printf("0%s", lit->u.n);
        break;
    case LIT_FLOAT:
        printf("%s", lit->u.n);
        break;
    case LIT_STRING:
        printf("%s", lit->u.s);
        break;
    case LIT_BOOL:
        printf("%s", lit->u.b == BOOL_TRUE ? "true" : "false");
        break;
    case LIT_NIL:
        printf("nil");
        break;
    }
}

static void
_id(const char *id)
{
    printf("%s", id);
}

static void
_expr(expr_t *e)
{
    switch ( e->type ) {
    case EXPR_ID:
        _id(e->u.id);
        break;
    case EXPR_DECL:
        _decl(e->u.decl);
        break;
    case EXPR_LITERAL:
        _literal(e->u.lit);
        break;
    case EXPR_OP:
        _op(e->u.op);
        break;
    case EXPR_SWITCH:
        printf("SWITCH\n");
        break;
    case EXPR_IF:
        printf("IF\n");
        break;
    case EXPR_CALL:
        printf("CALL\n");
        break;
    case EXPR_REF:
        printf("REF\n");
        break;
    case EXPR_MEMBER:
        printf("MEMBER\n");
        break;
    case EXPR_LIST:
        _expr_list(e->u.list);
        break;
    }
}

static void
_expr_list(expr_list_t *exprs)
{
    expr_t *e;

    e = exprs->head;
    while ( NULL != e ) {
        _expr(e);
        e = e->next;
    }
}

static void
_while(stmt_while_t *w)
{
    printf("while ");
    _expr(w->cond);
    printf("{\n");
    _inner_block(w->block);
    printf("}\n");
}

static void
_return(expr_t *e)
{
    printf("Return:");
    _expr(e);
    printf("\n");
}

static void
_stmt(stmt_t *stmt)
{
    switch ( stmt->type ) {
    case STMT_WHILE:
        _while(&stmt->u.whilestmt);
        break;
    case STMT_EXPR:
        _expr(stmt->u.expr);
        break;
    case STMT_EXPR_LIST:
        _expr_list(stmt->u.exprs);
        break;
    case STMT_BLOCK:
        _inner_block(stmt->u.block);
        break;
    case STMT_RETURN:
        _return(stmt->u.expr);
        break;
    }
    printf("\n");
}

static void
_inner_block(inner_block_t *block)
{
    stmt_t *stmt;

    stmt = block->stmts->head;
    while ( NULL != stmt ) {
        _stmt(stmt);
        stmt = stmt->next;
    }
}

static void
_func(func_t *fn)
{
    printf("Function: %s\n", fn->id);
    printf("Arguments: ");
    _args(fn->args);
    printf("\n");
    printf("Return values: ");
    _args(fn->rets);
    printf("\n");
    printf("Block: ");
    _inner_block(fn->block);
    printf("\n");
}

static void
_coroutine(coroutine_t *cr)
{
    printf("Coroutine: %s\n", cr->id);
}

static void
_module(module_t *md)
{
    printf("Module: %s\n", md->id);
}

static void
_directive(directive_t *dr)
{
    printf("Directive\n");
}

static void
_outer_block_entry(outer_block_entry_t *e)
{
    switch ( e->type ) {
    case OUTER_BLOCK_FUNC:
        _func(e->u.fn);
        break;
    case OUTER_BLOCK_COROUTINE:
        _coroutine(e->u.cr);
        break;
    case OUTER_BLOCK_MODULE:
        _module(e->u.md);
        break;
    case OUTER_BLOCK_DIRECTIVE:
        _directive(e->u.dr);
        break;
    }
}

static void
_outer_block(outer_block_t *block)
{
    outer_block_entry_t *e;

    e = block->head;
    while ( NULL != e ) {
        _outer_block_entry(e);
        e = e->next;
    }
}

static void
_display_ast(code_file_t *code)
{
    _outer_block(code->block);
}

/*
 * Main routine for the parser test
 */
int
main(int argc, const char *const argv[])
{
    FILE *fp;
    code_file_t *code;

    if ( argc < 2 ) {
        fp = stdin;
        /* stdio is not supported. */
        usage(argv[0]);
    } else {
        /* Open the specified file */
        fp = fopen(argv[1], "r");
        if ( NULL == fp ) {
            perror("fopen");
            exit(EXIT_FAILURE);
        }
    }

    /* Parse the specified file */
    code = minica_parse(fp);
    if ( NULL == code ) {
        exit(EXIT_FAILURE);
    }

    /* Print out the AST */
    _display_ast(code);

    return EXIT_SUCCESS;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
