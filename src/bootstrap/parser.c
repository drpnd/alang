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
    if ( TOK_COLON == tok->type ) {
        /* Type definition */
        _eat(parser, TOK_COLON);
        tok = _cur_token(parser);
        if ( TOK_ID != tok->type ) {
            free(s);
            return NULL;
        }
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
al_identifier_vec_t *
_parse_parameter_list(al_parser_t *parser)
{
    al_token_t *tok;
    al_identifier_t *id;
    al_identifier_vec_t *params;
    int ret;

    tok = _cur_token(parser);
    if ( TOK_LPAREN != tok->type ) {
        return NULL;
    }

    params = _vector_init();
    if ( NULL == params ) {
        return NULL;
    }

    do {
        tok = _next_token(parser);
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
    } while ( TOK_COMMA == tok->type );

    tok = _cur_token(parser);
    if ( _eat(parser, TOK_RPAREN) < 0 ) {
        identifier_vec_release(params);
        return NULL;
    }

    return params;
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
_parse_stmt_fn(al_parser_t *parser)
{
    al_identifier_t *func;
    al_identifier_vec_t *args;

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

    /* Return value */


    printf("fn %s\n", func->id);
    return NULL;
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
    case TOK_KW_FN:
        /* fn */
        stmt = _parse_stmt_fn(parser);
        break;
    default:
        /* Expression */
        stmt = NULL;
    }

    return NULL;
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
al_stmt_vec_t *
parser_parse(al_token_list_t *tokens)
{
    al_parser_t *parser;
    al_stmt_vec_t *program;

    parser = parser_init(NULL, tokens);
    if ( NULL == parser ) {
        return NULL;
    }

    /* Parse */
    program = _parse_stmt_vec(parser);

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
