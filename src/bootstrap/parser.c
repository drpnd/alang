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

#include "parser.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Prototype declarations */
static al_stmt_t * _parse_stmt(al_parser_t *);
static al_decl_vec_t * _parse_decl_vec(al_parser_t *);
static al_decl_t * _parse_decl(al_parser_t *);

/*
 * Initialize vector
 */
static struct vector *
_vector_init(void)
{
    struct vector * v;

    v = malloc(sizeof(struct vector));
    if ( NULL == v ) {
        return NULL;
    }
    v->size = 0;
    v->max_size = 1024;
    v->elems = malloc(sizeof(void *) * v->max_size);
    if ( NULL == v->elems ) {
        free(v);
        return NULL;
    }

    return v;
}

/*
 * Release statemement
 */
void
stmt_release(al_stmt_t *stmt)
{
    /* FIXME */
    switch ( stmt->type ) {
    default:
        ;
    }
    free(stmt);
}

/*
 * Vector release
 */
void
stmt_vec_release(al_stmt_vec_t *v)
{
    ssize_t i;

    for ( i = 0; i < (ssize_t)v->size; i++ ) {
        stmt_release(v->elems[i]);
    }
    free(v->elems);
    free(v);
}

/*
 * Release declaration
 */
void
decl_release(al_decl_t *decl)
{
    /* FIXME */
    switch ( decl->type ) {
    default:
        ;
    }
    free(decl);
}

/*
 * Vector release
 */
void
decl_vec_release(al_decl_vec_t *v)
{
    ssize_t i;

    for ( i = 0; i < (ssize_t)v->size; i++ ) {
        decl_release(v->elems[i]);
    }
    free(v->elems);
    free(v);
}

/*
 * Vector release
 */
void
identifier_vec_release(al_identifier_vec_t *v)
{
    ssize_t i;
    al_identifier_t *id;

    for ( i = 0; i < (ssize_t)v->size; i++ ) {
        id = v->elems[i];
        if ( NULL != id->type ) {
            free(id->type);
        }
        free(id->id);
        free(id);
    }
    free(v->elems);
    free(v);
}

/*
 * Push
 */
static int
_vector_push(struct vector *v, void *val)
{
    void **nelems;
    size_t sz;

    if ( v->size + 1 >= v->max_size ) {
        /* Realloc */
        sz = v->max_size + 1024;
        nelems = realloc(v->elems, sizeof(void *) * sz);
        if ( NULL == nelems ) {
            return -1;
        }
        v->elems = nelems;
        v->max_size = sz;
    }
    v->elems[v->size] = val;
    v->size++;

    return 0;
}

/*
 * Get the current token
 */
static al_token_t *
_cur_token(al_parser_t *parser)
{
    if ( NULL == parser->cur ) {
        return NULL;
    }
    return parser->cur->tok;
}

/*
 * Get the next token
 */
static al_token_t *
_next_token(al_parser_t *parser)
{
    parser->cur = parser->cur->next;
    return _cur_token(parser);
}

/*
 * Eat token
 */
static int
_eat(al_parser_t *parser, al_token_type_t type)
{
    al_token_t *tok;

    tok = _cur_token(parser);
    if ( NULL == tok ) {
        return -1;
    }
    if ( type != tok->type ) {
        return -1;
    }
    (void)_next_token(parser);

    return 0;
}

/*
 * Eat end of statement
 */
static int
_eateos(al_parser_t *parser)
{
    al_token_t *tok;

    tok = _cur_token(parser);
    if ( NULL == tok ) {
        return -1;
    }
    if ( TOK_SEMICOLON != tok->type && TOK_NEWLINE != tok->type ) {
        return -1;
    }
    (void)_next_token(parser);

    return 0;
}

/*
 * Skip statement delimiters
 */
static void
_skip_stmt_delims(al_parser_t *parser)
{
    al_token_t *tok;

    while ( NULL != (tok = _cur_token(parser)) ) {
        if ( TOK_NEWLINE == tok->type || TOK_SEMICOLON == tok->type ) {
            /* Ignore newlines and semicolons */
            (void)_next_token(parser);
        } else {
            /* Other tokens */
            break;
        }
    }
}

/*
 * Is opening expression token?
 */
static int
_is_expr_open_token(al_token_t *tok)
{
    switch ( tok->type ) {
    case TOK_ID:
    case TOK_NIL:
    case TOK_TRUE:
    case TOK_FALSE:
    case TOK_LIT_STR:
    case TOK_LIT_CHAR:
    case TOK_INT:
    case TOK_FLOAT:
    case TOK_NOT:
    case TOK_PLUS:
    case TOK_MINUS:
    case TOK_TILDE:
    case TOK_LPAREN:
    case TOK_RPAREN:
    case TOK_LBRACE:
    case TOK_RBRACE:
    case TOK_AT:
    case TOK_KW_NOT:
        return 1;
    default:
        return 0;
    }
}

/*
 * Is opening statement token?
 */
static int
_is_stmt_open_token(al_token_t *tok)
{
    switch ( tok->type ) {
    case TOK_KW_RETURN:
    case TOK_KW_CONTINUE:
    case TOK_KW_BREAK:
    case TOK_KW_IF:
    case TOK_KW_ELSE:
    case TOK_KW_WHILE:
    case TOK_KW_FOR:
        return 1;
    default:
        return _is_expr_open_token(tok);
    }
}

/*
 * Operator
 */
static al_expr_t *
_expr_op_prefix(al_op_type_t op, al_expr_t *a)
{
    al_expr_t *e;

    e = malloc(sizeof(al_expr_t));
    if ( NULL == e ) {
        return NULL;
    }
    e->type = EXPR_OP;
    e->u.op.type = op;
    e->u.op.fix = FIX_PREFIX;
    e->u.op.e0 = a;
    e->u.op.e1 = NULL;

    return e;
}
static al_expr_t *
_expr_op_infix(al_op_type_t op, al_expr_t *a, al_expr_t *b)
{
    al_expr_t *e;

    e = malloc(sizeof(al_expr_t));
    if ( NULL == e ) {
        return NULL;
    }
    e->type = EXPR_OP;
    e->u.op.type = op;
    e->u.op.fix = FIX_INFIX;
    e->u.op.e0 = a;
    e->u.op.e1 = b;

    return e;
}

/*
 * Parse identifier
 */
al_identifier_t *
_parse_identifier(al_parser_t *parser)
{
    char *s;
    char *t;
    al_token_t *tok;
    al_identifier_t *id;

    tok = _cur_token(parser);
    if ( TOK_ID != tok->type ) {
        return NULL;
    }

    s = strdup(tok->u.id);
    if ( NULL == s ) {
        return NULL;
    }
    _eat(parser, TOK_ID);

    tok = _cur_token(parser);
    t = NULL;
    if ( TOK_ID == tok->type ) {
        /* Type definition */
        tok = _cur_token(parser);
        t = strdup(tok->u.id);
        if ( NULL == t ) {
            free(s);
            return NULL;
        }
        _eat(parser, TOK_ID);
    }

    id = malloc(sizeof(al_identifier_t));
    if ( NULL == id ) {
        if ( NULL != t ) {
            free(t);
        }
        free(s);
        return NULL;
    }
    id->id = s;
    id->type = t;

    return id;
}

/*
 * Parse parameter list
 */
al_identifier_vec_t *
_parse_parameter_list(al_parser_t *parser)
{
    al_token_t *tok;
    al_identifier_t *id;
    al_identifier_vec_t *params;
    int ret;

    params = _vector_init();
    if ( NULL == params ) {
        return NULL;
    }

    /* No parameters specified */
    tok = _cur_token(parser);
    if ( TOK_ID != tok->type ) {
        return params;
    }

    for ( ;; ) {
        tok = _cur_token(parser);
        if ( TOK_ID != tok->type ) {
            identifier_vec_release(params);
            return NULL;
        }
        id = _parse_identifier(parser);
        ret = _vector_push(params, id);
        if ( ret < 0 ) {
            if ( NULL != id->type ) {
                free(id->type);
            }
            free(id->id);
            free(id);
            identifier_vec_release(params);
            return NULL;
        }
        tok = _cur_token(parser);
        if ( TOK_COMMA != tok->type ) {
            break;
        }
        (void)_next_token(parser);
    }

    return params;
}

/*
 * atom ::=
 *      identifier | literal
 */
static al_expr_t *
_parse_expr_atom(al_parser_t *parser)
{
    al_token_t *tok;
    al_expr_t *e;
    al_identifier_t *id;

    tok = _cur_token(parser);

    switch ( tok->type ) {
    case TOK_LPAREN:
        /* List of expression */
        printf("To be implemented\n");
        e = NULL;
        break;
    case TOK_ID:
        /* Variable */
        id = _parse_identifier(parser);
        if ( NULL == id ) {
            return NULL;
        }
        e = malloc(sizeof(al_expr_t));
        if ( NULL == e ) {
            return NULL;
        }
        e->type = EXPR_VAR;
        e->u.var = id;
        break;
    case TOK_LIT_STR:
        /* String literal */
        e = malloc(sizeof(al_expr_t));
        if ( NULL == e ) {
            return NULL;
        }
        e->type = EXPR_STRING;
        e->u.s.s = malloc(tok->u.s.len);
        if ( NULL == e->u.s.s ) {
            return NULL;
        }
        (void)memcpy(e->u.s.s, tok->u.s.s, tok->u.s.len);
        e->u.s.len = tok->u.s.len;
        (void)_next_token(parser);
        break;
    case TOK_INT:
        /* Integer literal */
        e = malloc(sizeof(al_expr_t));
        if ( NULL == e ) {
            return NULL;
        }
        e->type = EXPR_INT;
        e->u.i = tok->u.i;
        (void)_next_token(parser);
        break;
    case TOK_FLOAT:
        /* Floating point number */
        e = malloc(sizeof(al_expr_t));
        if ( NULL == e ) {
            return NULL;
        }
        e->type = EXPR_FLOAT;
        e->u.f = tok->u.f;
        (void)_next_token(parser);
        break;
    default:
        printf("Not implemented ATOM\n");
        return NULL;
    }

    return e;
}

/*
 * primary ::=
 *      atom
 */
static al_expr_t *
_parse_expr_primary(al_parser_t *parser)
{
    return _parse_expr_atom(parser);
}

/*
 * u_expr ::=
 *      primary | "-" u_expr | "+" u_expr | "~" u_expr
 */
static al_expr_t *
_parse_expr_u_expr(al_parser_t *parser)
{
    al_token_t *tok;
    al_expr_t *e0;
    al_expr_t *e;
    al_op_type_t op;
    int valid;

    tok = _cur_token(parser);
    valid = 1;
    switch ( tok->type ) {
    case TOK_PLUS:
        op = OP_PLUS;
        break;
    case TOK_MINUS:
        op = OP_DIV;
        break;
    case TOK_TILDE:
        op = OP_TILDE;
        break;
    default:
        valid = 0;
    }
    if ( valid ) {
        _next_token(parser);
        e0 = _parse_expr_u_expr(parser);
        if ( NULL == e0 ) {
            return NULL;
        }
        e = _expr_op_prefix(op, e0);
        if ( NULL == e ) {
            return NULL;
        }
        (void)_next_token(parser);
    } else {
        e = _parse_expr_primary(parser);
        if ( NULL == e ) {
            return NULL;
        }
    }

    return e;
}

/*
 * m_expr ::=
 *      u_expr ( ( "*" | "/" | "%" ) u_expr )*
 */
static al_expr_t *
_parse_expr_m_expr(al_parser_t *parser)
{
    al_token_t *tok;
    al_expr_t *e0;
    al_expr_t *e1;
    al_expr_t *e;
    al_op_type_t op;
    int valid;

    /* Left expression */
    e0 = _parse_expr_u_expr(parser);
    if ( NULL == e0 ) {
        return NULL;
    }

    /* Parse the right expression if *, / or % succeeds to the left
       expression */
    tok = _cur_token(parser);
    valid = 1;
    switch ( tok->type ) {
    case TOK_ASTERISK:
        op = OP_MUL;
        break;
    case TOK_SLASH:
        op = OP_DIV;
        break;
    case TOK_PERCENT:
        op = OP_MOD;
        break;
    default:
        valid = 0;
    }
    if ( valid ) {
        _next_token(parser);
        e1 = _parse_expr_m_expr(parser);
        if ( NULL == e1 ) {
            return NULL;
        }
        e = _expr_op_infix(op, e0, e1);
        if ( NULL == e ) {
            return NULL;
        }
        (void)_next_token(parser);
    } else {
        e = e0;
    }

    return e;
}

/*
 * a_expr ::=
 *      m_expr ( ( "+" | "-" ) m_expr )*
 */
static al_expr_t *
_parse_expr_a_expr(al_parser_t *parser)
{
    al_token_t *tok;
    al_expr_t *e0;
    al_expr_t *e1;
    al_expr_t *e;
    al_op_type_t op;
    int valid;

    /* Left expression */
    e0 = _parse_expr_m_expr(parser);
    if ( NULL == e0 ) {
        return NULL;
    }

    /* Parse the right expression if + or - succeeds to the left
       expression */
    tok = _cur_token(parser);
    valid = 1;
    switch ( tok->type ) {
    case TOK_PLUS:
        op = OP_PLUS;
        break;
    case TOK_MINUS:
        op = OP_MINUS;
        break;
    default:
        valid = 0;
    }
    if ( valid ) {
        _next_token(parser);
        e1 = _parse_expr_a_expr(parser);
        if ( NULL == e1 ) {
            return NULL;
        }
        e = _expr_op_infix(op, e0, e1);
        if ( NULL == e ) {
            return NULL;
        }
        (void)_next_token(parser);
    } else {
        e = e0;
    }

    return e;
}

/*
 * shift_expr ::=
 *      a_expr ( ( "<<" | ">>" ) a_expr )*
 */
static al_expr_t *
_parse_expr_shift_expr(al_parser_t *parser)
{
    al_token_t *tok;
    al_expr_t *e0;
    al_expr_t *e1;
    al_expr_t *e;
    al_op_type_t op;
    int valid;

    /* Left expression */
    e0 = _parse_expr_a_expr(parser);
    if ( NULL == e0 ) {
        return NULL;
    }

    /* Parse the right expression if shift operators succeeds to the left
       expression */
    tok = _cur_token(parser);
    valid = 1;
    switch ( tok->type ) {
    case TOK_LSHIFT:
        op = OP_LSHIFT;
        break;
    case TOK_RSHIFT:
        op = OP_RSHIFT;
        break;
    default:
        valid = 0;
    }
    if ( valid ) {
        _next_token(parser);
        e1 = _parse_expr_shift_expr(parser);
        if ( NULL == e1 ) {
            return NULL;
        }
        e = _expr_op_infix(op, e0, e1);
        if ( NULL == e ) {
            return NULL;
        }
        (void)_next_token(parser);
    } else {
        e = e0;
    }

    return e;
}

/*
 * and_expr ::=
 *      shift_expr ( "&" shift_expr )*
 */
static al_expr_t *
_parse_expr_and_expr(al_parser_t *parser)
{
    al_token_t *tok;
    al_expr_t *e0;
    al_expr_t *e1;
    al_expr_t *e;

    /* Left expression */
    e0 = _parse_expr_shift_expr(parser);
    if ( NULL == e0 ) {
        return NULL;
    }

    /* Parse the right expression if & succeeds to the left expression */
    tok = _cur_token(parser);
    if ( TOK_AMP == tok->type ) {
        _next_token(parser);
        e1 = _parse_expr_and_expr(parser);
        if ( NULL == e1 ) {
            return NULL;
        }
        e = _expr_op_infix(OP_BIT_AND, e0, e1);
        if ( NULL == e ) {
            return NULL;
        }
        (void)_next_token(parser);
    } else {
        e = e0;
    }

    return e;
}

/*
 * xor_expr ::=
 *      and_expr ( "^" xor_expr )*
 */
static al_expr_t *
_parse_expr_xor_expr(al_parser_t *parser)
{
    al_token_t *tok;
    al_expr_t *e0;
    al_expr_t *e1;
    al_expr_t *e;

    /* Left expression */
    e0 = _parse_expr_and_expr(parser);
    if ( NULL == e0 ) {
        return NULL;
    }

    /* Parse the right expression if ^ succeeds to the left expression */
    tok = _cur_token(parser);
    if ( TOK_HAT == tok->type ) {
        _next_token(parser);
        e1 = _parse_expr_xor_expr(parser);
        if ( NULL == e1 ) {
            return NULL;
        }
        e = _expr_op_infix(OP_BIT_XOR, e0, e1);
        if ( NULL == e ) {
            return NULL;
        }
        (void)_next_token(parser);
    } else {
        e = e0;
    }

    return e;
}

/*
 * or_expr ::=
 *      xor_expr ( "|" xor_expr )*
 */
static al_expr_t *
_parse_expr_or_expr(al_parser_t *parser)
{
    al_token_t *tok;
    al_expr_t *e0;
    al_expr_t *e1;
    al_expr_t *e;

    /* Left expression */
    e0 = _parse_expr_xor_expr(parser);
    if ( NULL == e0 ) {
        return NULL;
    }

    /* Parse the right expression if | succeeds to the left expression */
    tok = _cur_token(parser);
    if ( TOK_BAR == tok->type ) {
        (void)_next_token(parser);
        e1 = _parse_expr_or_expr(parser);
        if ( NULL == e1 ) {
            return NULL;
        }
        e = _expr_op_infix(OP_BIT_OR, e0, e1);
        if ( NULL == e ) {
            return NULL;
        }
        (void)_next_token(parser);
    } else {
        e = e0;
    }

    return e;
}

/*
 * comparison ::=
 *      or_expr [ comp_operator or_expr ]
 */
static al_expr_t *
_parse_expr_comparison(al_parser_t *parser)
{
    al_token_t *tok;
    al_expr_t *e0;
    al_expr_t *e1;
    al_expr_t *e;
    al_op_type_t op;
    int valid;

    /* Left expression */
    e0 = _parse_expr_or_expr(parser);
    if ( NULL == e0 ) {
        return NULL;
    }

    /* Parse the right expression if a comparison operator succeeds to the left
       expression */
    tok = _cur_token(parser);
    valid = 1;
    switch ( tok->type ) {
    case TOK_LT:
        op = OP_LT;
        break;
    case TOK_GT:
        op = OP_GT;
        break;
    case TOK_EQ_EQ:
        op = OP_EQ_EQ;
        break;
    case TOK_LEQ:
        op = OP_LEQ;
        break;
    case TOK_GEQ:
        op = OP_GEQ;
        break;
    case TOK_NEQ:
        op = OP_NEQ;
        break;
    default:
        valid = 0;
    }
    if ( valid ) {
        (void)_next_token(parser);
        e1 = _parse_expr_or_expr(parser);
        if ( NULL == e1 ) {
            return NULL;
        }
        e = _expr_op_infix(op, e0, e1);
        if ( NULL == e ) {
            return NULL;
        }
        (void)_next_token(parser);
    } else {
        e = e0;
    }

    return e;
}

/*
 * not_test ::=
 *      comparison | "not" not_test
 */
static al_expr_t *
_parse_expr_not_test(al_parser_t *parser)
{
    al_token_t *tok;
    al_expr_t *e0;
    al_expr_t *e;

    tok = _cur_token(parser);
    if ( TOK_NOT == tok->type ) {
        (void)_next_token(parser);
        e0 = _parse_expr_not_test(parser);
        if ( NULL == e0 ) {
            return NULL;
        }
        e = _expr_op_prefix(OP_NOT, e0);
        if ( NULL == e ) {
            return NULL;
        }
        (void)_next_token(parser);
    } else {
        e = _parse_expr_comparison(parser);
        if ( NULL == e ) {
            return NULL;
        }
    }

    return e;
}

/*
 * and_test ::=
 *      not_test [ "and" and_test ]
 */
static al_expr_t *
_parse_expr_and_test(al_parser_t *parser)
{
    al_token_t *tok;
    al_expr_t *e0;
    al_expr_t *e1;
    al_expr_t *e;

    /* Left expression */
    e0 = _parse_expr_not_test(parser);
    if ( NULL == e0 ) {
        return NULL;
    }

    /* Parse the right expression if "or" succeeds to the left expression */
    tok = _cur_token(parser);
    if ( TOK_KW_OR == tok->type ) {
        (void)_next_token(parser);
        e1 = _parse_expr_and_test(parser);
        if ( NULL == e1 ) {
            return NULL;
        }
        e = _expr_op_infix(OP_AND, e0, e1);
        if ( NULL == e ) {
            return NULL;
        }
        (void)_next_token(parser);
    } else {
        e = e0;
    }

    return e;
}

/*
 * or_test ::=
 *      and_test [ "or" or_test ]
 */
static al_expr_t *
_parse_expr_or_test(al_parser_t *parser)
{
    al_token_t *tok;
    al_expr_t *e0;
    al_expr_t *e1;
    al_expr_t *e;

    /* Left expression */
    e0 = _parse_expr_and_test(parser);
    if ( NULL == e0 ) {
        return NULL;
    }

    /* Parse the right expression if "or" succeeds to the left expression */
    tok = _cur_token(parser);
    if ( TOK_KW_OR == tok->type ) {
        (void)_next_token(parser);
        e1 = _parse_expr_or_test(parser);
        if ( NULL == e1 ) {
            return NULL;
        }
        e = _expr_op_infix(OP_OR, e0, e1);
        if ( NULL == e ) {
            return NULL;
        }
        (void)_next_token(parser);
    } else {
        e = e0;
    }

    return e;
}

/*
 * Parse expression
 */
static al_expr_t *
_parse_expr(al_parser_t *parser)
{
    return _parse_expr_or_test(parser);
}

/*
 * Parser statement block
 */
static al_stmt_vec_t *
_parse_stmt_block(al_parser_t *parser)
{
    al_token_t *tok;
    al_stmt_t *stmt;
    al_stmt_vec_t *stmt_vec;
    int ret;

    /* Eat a left bracket */
    if ( _eat(parser, TOK_LBRACE) < 0 ) {
        return NULL;
    }

    stmt_vec = (al_stmt_vec_t *)_vector_init();
    if ( NULL == stmt_vec ) {
        return NULL;
    }
    for ( ;; ) {
        /* While a token is available */
        _skip_stmt_delims(parser);

        /* Until right bracket is found */
        tok = _cur_token(parser);
        if ( TOK_RBRACE == tok->type ) {
            break;
        }

        /* Statement */
        stmt = _parse_stmt(parser);
        if ( NULL == stmt ) {
            /* Error */
            stmt_vec_release(stmt_vec);
            return NULL;
        }
        /* Push the statement to the vector */
        ret = _vector_push(stmt_vec, stmt);
        if ( ret < 0 ) {
            stmt_release(stmt);
            stmt_vec_release(stmt_vec);
            return NULL;
        }
    }
    tok = _cur_token(parser);
    /* Eat a right bracket */
    if ( _eat(parser, TOK_RBRACE) < 0 ) {
        return NULL;
    }

    return stmt_vec;
}

static al_stmt_t *
_parse_stmt_return(al_parser_t *parser)
{
    return NULL;
}
static al_stmt_t *
_parse_stmt_break(al_parser_t *parser)
{
    return NULL;
}
static al_stmt_t *
_parse_stmt_continue(al_parser_t *parser)
{
    return NULL;
}
static al_stmt_t *
_parse_stmt_if(al_parser_t *parser)
{
    return NULL;
}
static al_stmt_t *
_parse_stmt_while(al_parser_t *parser)
{
    return NULL;
}
static al_stmt_t *
_parse_stmt_for(al_parser_t *parser)
{
    return NULL;
}
static al_stmt_t *
_parse_stmt_expr(al_parser_t *parser)
{
    al_stmt_t *stmt;
    al_expr_t *expr;
    al_token_t *tok;

    stmt = malloc(sizeof(al_stmt_t));
    if ( NULL == stmt ) {
        return NULL;
    }

    /* Parse expression */
    expr = _parse_expr(parser);
    if ( NULL == expr ) {
        free(stmt);
        return NULL;
    }
    tok = _cur_token(parser);
    if ( TOK_DEF == tok->type ) {
        /* Assignment */
        stmt->type = STMT_ASSIGN;
        stmt->u.a.targets = expr;
        (void)_next_token(parser);
        expr = _parse_expr(parser);
        if ( NULL == expr ) {
            free(stmt);
            return NULL;
        }
        stmt->u.a.val = expr;
    } else {
        stmt->type = STMT_EXPR;
        stmt->u.e = expr;
    }

    return stmt;
}

/*
 * Statement
 */
static al_stmt_t *
_parse_stmt(al_parser_t *parser)
{
    al_token_t *tok;
    al_stmt_t *stmt;

    /* Get the current token */
    tok = _cur_token(parser);
    if ( NULL == tok ) {
        return NULL;
    }

    switch ( tok->type ) {
    case TOK_KW_RETURN:
        /* return */
        stmt = _parse_stmt_return(parser);
        if ( _eateos(parser) ) {
            stmt_release(stmt);
            return NULL;
        }
        break;
    case TOK_KW_BREAK:
        /* break */
        stmt = _parse_stmt_break(parser);
        if ( _eateos(parser) ) {
            stmt_release(stmt);
            return NULL;
        }
        break;
    case TOK_KW_CONTINUE:
        /* continue */
        stmt = _parse_stmt_continue(parser);
        if ( _eateos(parser) ) {
            stmt_release(stmt);
            return NULL;
        }
        break;
    case TOK_KW_IF:
        /* if */
        stmt = _parse_stmt_if(parser);
        break;
    case TOK_KW_WHILE:
        /* while */
        stmt = _parse_stmt_while(parser);
        break;
    case TOK_KW_FOR:
        /* for */
        stmt = _parse_stmt_for(parser);
        break;
    default:
        /* Expression */
        stmt = _parse_stmt_expr(parser);
    }

    return stmt;
}

/*
 * Statements
 */
static al_stmt_vec_t *
_parse_stmt_vec(al_parser_t *parser)
{
    al_token_t *tok;
    al_stmt_vec_t *stmt_vec;
    al_stmt_t *stmt;
    int ret;

    stmt_vec = (al_stmt_vec_t *)_vector_init();
    if ( NULL == stmt_vec ) {
        return NULL;
    }

    /* While a token is available */
    while ( NULL != (tok = _cur_token(parser)) ) {
        if ( TOK_NEWLINE == tok->type || TOK_SEMICOLON == tok->type ) {
            /* Ignore newline */
            (void)_next_token(parser);
        } else {
            /* Statement */
            stmt = _parse_stmt(parser);
            if ( NULL == stmt ) {
                /* Error */
                stmt_vec_release(stmt_vec);
                return NULL;
            }
            /* Push the statement to the vector */
            ret = _vector_push(stmt_vec, stmt);
            if ( ret < 0 ) {
                stmt_release(stmt);
                stmt_vec_release(stmt_vec);
                return NULL;
            }
        }
    }

    return stmt_vec;
}

/*
 * Parse function
 */
static al_decl_t *
_parse_decl_fn(al_parser_t *parser)
{
    al_identifier_t *func;
    al_identifier_vec_t *args;
    al_identifier_vec_t *retvals;
    al_token_t *tok;
    al_decl_t *decl;
    al_stmt_vec_t *vec;

    if ( _eat(parser, TOK_KW_FN) < 0 ) {
        return NULL;
    }

    /* Name of the function */
    func = _parse_identifier(parser);
    if ( NULL == func ) {
        return NULL;
    }
    if ( NULL != func->type ) {
        free(func->id);
        if ( NULL != func->type ) {
            free(func->type);
        }
        free(func);
        return NULL;
    }

    if ( _eat(parser, TOK_LPAREN) < 0 ) {
        free(func->id);
        if ( NULL != func->type ) {
            free(func->type);
        }
        free(func);
        return NULL;
    }

    /* Arguments */
    args = _parse_parameter_list(parser);
    if ( NULL == args ) {
        free(func->id);
        if ( NULL != func->type ) {
            free(func->type);
        }
        free(func);
        return NULL;
    }

    tok = _cur_token(parser);
    if ( _eat(parser, TOK_RPAREN) < 0 ) {
        free(func->id);
        if ( NULL != func->type ) {
            free(func->type);
        }
        free(func);
        identifier_vec_release(args);
        return NULL;
    }

    /* Skip delimiters */
    _skip_stmt_delims(parser);

    if ( _eat(parser, TOK_LPAREN) < 0 ) {
        free(func->id);
        if ( NULL != func->type ) {
            free(func->type);
        }
        free(func);
        return NULL;
    }

    /* Return value */
    retvals = _parse_parameter_list(parser);

    if ( _eat(parser, TOK_RPAREN) < 0 ) {
        free(func->id);
        if ( NULL != func->type ) {
            free(func->type);
        }
        free(func);
        return NULL;
    }

    /* Skip delimiters */
    _skip_stmt_delims(parser);

    /* Block of statements */
    vec = _parse_stmt_block(parser);
    if ( NULL == vec ) {
        free(func->id);
        if ( NULL != func->type ) {
            free(func->type);
        }
        free(func);
        return NULL;
    }

    decl = malloc(sizeof(al_decl_t));
    if ( NULL == decl ) {
        return NULL;
    }
    decl->type = DECL_FN;
    decl->u.fn.f = func;
    decl->u.fn.ps = args;
    decl->u.fn.rv = retvals;
    decl->u.fn.b = vec;

    return decl;
}

/*
 * Parse import declaration
 */
static al_decl_t *
_parse_decl_import(al_parser_t *parser)
{
    al_decl_t *decl;
    al_token_t *tok;

    if ( _eat(parser, TOK_KW_IMPORT) < 0 ) {
        return NULL;
    }

    decl = malloc(sizeof(al_decl_t));
    if ( NULL == decl ) {
        return NULL;
    }
    decl->type = DECL_IMPORT;

    /* Get the current token */
    tok = _cur_token(parser);
    if ( NULL == tok ) {
        return NULL;
    }
    if ( TOK_LIT_STR != tok->type ) {
        return NULL;
    }

    /* Copy the string */
    decl->u.import.package.s = malloc(tok->u.s.len);
    if ( NULL == decl->u.import.package.s ) {
        return NULL;
    }
    (void)memcpy(decl->u.import.package.s, tok->u.s.s, tok->u.s.len);
    decl->u.import.package.len = tok->u.s.len;
    (void)_next_token(parser);

    return decl;
}

/*
 * Parse package declaration
 */
static al_decl_t *
_parse_decl_package(al_parser_t *parser)
{
    al_decl_t *decl;
    al_token_t *tok;

    if ( _eat(parser, TOK_KW_PACKAGE) < 0 ) {
        return NULL;
    }

    decl = malloc(sizeof(al_decl_t));
    if ( NULL == decl ) {
        return NULL;
    }
    decl->type = DECL_PACKAGE;

    /* Get the current token */
    tok = _cur_token(parser);
    if ( NULL == tok ) {
        return NULL;
    }
    if ( TOK_ID != tok->type ) {
        return NULL;
    }

    /* Copy the string */
    decl->u.package.name = strdup(tok->u.id);
    if ( NULL == decl->u.package.name ) {
        return NULL;
    }
    (void)_next_token(parser);

    return decl;
}

/*
 * Declaration
 */
static al_decl_t *
_parse_decl(al_parser_t *parser)
{
    al_token_t *tok;
    al_decl_t *decl;

    /* Get the current token */
    tok = _cur_token(parser);
    if ( NULL == tok ) {
        return NULL;
    }

    switch ( tok->type ) {
    case TOK_KW_FN:
        /* fn */
        decl = _parse_decl_fn(parser);
        break;
    case TOK_KW_IMPORT:
        /* import */
        decl = _parse_decl_import(parser);
        break;
    case TOK_KW_PACKAGE:
        /* package */
        decl = _parse_decl_package(parser);
        break;
    default:
        /* Expression */
        decl = NULL;
    }

    return decl;
}

/*
 * Parser declarations
 */
static al_decl_vec_t *
_parse_decl_vec(al_parser_t *parser)
{
    al_decl_vec_t *decl_vec;
    al_token_t *tok;
    al_decl_t *decl;
    int ret;

    decl_vec = (al_decl_vec_t *)_vector_init();
    if ( NULL == decl_vec ) {
        return NULL;
    }

    /* While a token is available */
    while ( NULL != (tok = _cur_token(parser)) ) {
        if ( TOK_NEWLINE == tok->type || TOK_SEMICOLON == tok->type ) {
            /* Ignore newline */
            (void)_next_token(parser);
        } else {
            /* Statement */
            decl = _parse_decl(parser);
            if ( NULL == decl ) {
                /* Error */
                decl_vec_release(decl_vec);
                return NULL;
            }
            /* Push the statement to the vector */
            ret = _vector_push(decl_vec, decl);
            if ( ret < 0 ) {
                decl_release(decl);
                decl_vec_release(decl_vec);
                return NULL;
            }
        }
    }

    return decl_vec;
}

/*
 * Initialize parser
 */
al_parser_t *
parser_init(al_parser_t *parser, al_token_list_t *tokens)
{
    if ( NULL == parser ) {
        parser = malloc(sizeof(al_parser_t));
        if ( NULL == parser ) {
            return NULL;
        }
        parser->_allocated = 1;
    } else {
        parser->_allocated = 0;
    }
    parser->tokens = tokens;
    parser->cur = tokens->head;
    parser->program = NULL;

    return parser;
}

/*
 * Release the parser instance
 */
void
parser_release(al_parser_t *parser)
{
    if ( parser->_allocated ) {
        free(parser);
    }
}

/*
 * Parse
 */
al_decl_vec_t *
parser_parse(al_token_list_t *tokens)
{
    al_parser_t *parser;
    al_decl_vec_t *program;

    parser = parser_init(NULL, tokens);
    if ( NULL == parser ) {
        return NULL;
    }

    /* Parse */
    program = _parse_decl_vec(parser);

    return program;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
