/*_
 * Copyright (c) 2017 Hirochika Asai <asai@jar.jp>
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

#ifndef _PARSER_H
#define _PARSER_H

#include "token.h"
#include <stddef.h>

/*
 * struct vector
 */
struct vector {
    /* Elements */
    void **elems;
    /* Size of elements */
    size_t size;
    /* Allocated size */
    size_t max_size;
};

typedef struct vector al_expr_vec_t;
typedef struct vector al_stmt_vec_t;

/*
 * Operator type
 */
typedef enum {
    OP_OR,
    OP_XOR,
    OP_AND,
    OP_AMP,
    OP_LSHIFT,
    OP_RSHIFT,
    OP_PLUS,
    OP_MINUS,
    OP_MUL,
    OP_DIV,
    OP_MOD,
    OP_TILDE,
    OP_KW_NOT,
    OP_LT,
    OP_GT,
    OP_EQ_EQ,
    OP_GEQ,
    OP_LEQ,
    OP_NEQ,
    OP_BAR,
    OP_ASSIGN,
} al_op_type_t;

/*
 * Fix type
 */
typedef enum {
    FIX_PREFIX,                 /* Prefix */
    FIX_INFIX,                  /* Infix */
} al_fix_type_t;

/*
 * Operator
 */
typedef struct {
    al_op_type_t type;
    al_fix_type_t fix;
    al_expr_vec_t *args;
} al_expr_op_t;

/* Identifier */
typedef struct {
    char *id;
    char *type;
} al_identifier_t;
typedef struct vector al_identifier_vec_t;

/*
 * Expression type
 */
typedef enum {
    EXPR_VAR,                   /* Variable */
    EXPR_NIL,                   /* nil */
    EXPR_FALSE,                 /* false */
    EXPR_TRUE,                  /* true */
    EXPR_INT,                   /* Integer value */
    EXPR_FLOAT,                 /* Float value */
    EXPR_STRING,                /* String */
    EXPR_OP,                    /* Operator */
    EXPR_CALL,                  /* Function call */
} al_expr_type_t;


typedef struct _expr al_expr_t;

/*
 * Attribute referer
 */
typedef struct {
    al_expr_t *e;
    al_identifier_t *f;
} al_expr_attrref_t;

/*
 * Call
 */
typedef struct _expr_call {
    al_expr_t *func;
    al_expr_vec_t *args;
} al_expr_call_t;

/*
 * Expression
 */
struct _expr {
    al_expr_type_t type;
    union {
        al_identifier_t *var;   /* Variable */
        int_t i;                /* Integer */
        fp_t f;                 /* Float */
        al_string_t s;          /* String */
        al_expr_vec_t *disp;
        al_expr_op_t op;
        al_expr_attrref_t attr;
        al_expr_vec_t *sub;
        al_expr_call_t call;
    } u;
};

/*
 * Statement type
 */
typedef enum {
    STMT_EXPR,
    STMT_ASSIGN,
    STMT_RETURN,
    STMT_BREAK,
    STMT_CONTINUE,
    STMT_IF,
    STMT_WHILE,
    STMT_FOR,
    STMT_FN,
} al_stmt_type_t;

typedef struct _stmt al_stmt_t;

/*
 * Assign statement
 */
typedef struct {
    al_expr_t *target;
    al_expr_t *val;
} al_stmt_assign_t;

/*
 * If branch
 */
typedef struct {
    al_expr_t *e;
    al_stmt_vec_t *s;
} al_stmt_if_branch_vec_t;

/*
 * While
 */
typedef struct {
    al_expr_t *e;
    al_stmt_vec_t *s;
} al_stmt_while_t;

/*
 * For
 */
typedef struct {
    al_expr_t *e0;
    al_expr_t *e1;
    al_expr_t *e2;
    al_stmt_vec_t *b;
} al_stmt_for_t;

/*
 * Define function
 */
typedef struct {
    al_identifier_t *f;
    al_identifier_vec_t *ps;
    al_stmt_vec_t *b;
} al_stmt_fn_t;

/*
 * Statement
 */
struct _stmt {
    al_stmt_type_t type;
    union {
        al_expr_t *e;
        al_stmt_assign_t a;
        al_stmt_if_branch_vec_t *i;
        al_stmt_while_t w;
        al_stmt_for_t f;
        al_stmt_fn_t fn;
    } u;
};

/*
 * Parser
 */
typedef struct {
    /* Tokens */
    al_token_list_t *tokens;

    /* Current token */
    al_token_entry_t *cur;

    /* Allocated */
    int _allocated;

    /* Program */
    al_stmt_vec_t *program;
} al_parser_t;

#ifdef __cplusplus
extern "C" {
#endif

    al_parser_t * parser_init(al_parser_t *, al_token_list_t *);
    void parser_release(al_parser_t *);

    al_stmt_vec_t * parser_parse(al_token_list_t *);

#ifdef __cplusplus
}
#endif

#endif /* _PARSER_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
