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

#include "tokenizer.h"
#include "token.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*
 * These macros push a simple token (zero-value-length token) to stack.
 */
#define RETURN_ON_ERROR(code, ret)              \
    do {                                        \
        if ( (code) < 0 ) { return ret; }       \
    } while (0)
#define IS_KEYWORD_CHAR(c) (isalnum(c) || '_' == (c))


/*
 * Current character
 */
int
tokenizer_cur(al_tokenizer_t *t)
{
    if ( (size_t)t->off < t->sz ) {
        return t->buf[t->off];
    } else {
        return EOF;
    }
}

/*
 * Next character
 */
int
tokenizer_next(al_tokenizer_t *t)
{
    if ( (size_t)t->off < t->sz ) {
        t->off++;
    }
    return tokenizer_cur(t);
}

/*
 * Push new token
 */
static int
_push_token_to_list(al_tokenizer_t *t, al_token_t *tok)
{
    al_token_entry_t *ent;

    ent = malloc(sizeof(al_token_entry_t));
    if ( NULL == ent ) {
        return -1;
    }
    ent->tok = tok;
    ent->next = NULL;

    if ( NULL == t->tokens->tail ) {
        t->tokens->head = ent;
        t->tokens->tail = ent;
    } else {
        t->tokens->tail->next = ent;
        t->tokens->tail = ent;
    }

    return 0;
}
static int
_push_token(al_tokenizer_t *t, al_token_type_t type)
{
    al_token_t *tok;
    int ret;

    tok = malloc(sizeof(al_token_t));
    if ( NULL == tok ) {
        return -1;
    }
    tok->type = type;

    ret = _push_token_to_list(t, tok);
    if ( ret < 0 ) {
        free(tok);
    }

    return 0;
}
static int
_push_token_id(al_tokenizer_t *t, const char *id)
{
    al_token_t *tok;
    int ret;

    tok = malloc(sizeof(al_token_t));
    if ( NULL == tok ) {
        return -1;
    }
    tok->type = TOK_ID;
    tok->u.id = strdup(id);
    if ( NULL == tok->u.id ) {
        free(tok);
        return -1;
    }

    ret = _push_token_to_list(t, tok);
    if ( ret < 0 ) {
        free(tok->u.id);
        free(tok);
        return -1;
    }

    return 0;
}
static int
_push_token_int(al_tokenizer_t *t, int_t val)
{
    al_token_t *tok;
    int ret;

    tok = malloc(sizeof(al_token_t));
    if ( NULL == tok ) {
        return -1;
    }
    tok->type = TOK_INT;
    tok->u.i = val;

    ret = _push_token_to_list(t, tok);
    if ( ret < 0 ) {
        free(tok);
        return -1;
    }

    return 0;
}
static int
_push_token_float(al_tokenizer_t *t, fp_t x)
{
    al_token_t *tok;
    int ret;

    tok = malloc(sizeof(al_token_t));
    if ( NULL == tok ) {
        return -1;
    }
    tok->type = TOK_FLOAT;
    tok->u.f = x;

    ret = _push_token_to_list(t, tok);
    if ( ret < 0 ) {
        free(tok);
        return -1;
    }

    return 0;
}
static int
_push_token_char(al_tokenizer_t *t, unsigned char val)
{
    al_token_t *tok;
    int ret;

    tok = malloc(sizeof(al_token_t));
    if ( NULL == tok ) {
        return -1;
    }
    tok->type = TOK_LIT_CHAR;
    tok->u.c = val;

    ret = _push_token_to_list(t, tok);
    if ( ret < 0 ) {
        free(tok);
        return -1;
    }

    return 0;
}
static int
_push_token_str(al_tokenizer_t *t, unsigned char *s, off_t len)
{
    al_token_t *tok;
    int ret;

    tok = malloc(sizeof(al_token_t));
    if ( NULL == tok ) {
        return -1;
    }
    tok->type = TOK_LIT_STR;
    tok->u.s.s = s;
    tok->u.s.len = len;

    ret = _push_token_to_list(t, tok);
    if ( ret < 0 ) {
        free(tok);
        return -1;
    }

    return 0;
}

/*
 * Scan number
 */
static int
_scan_number(al_tokenizer_t *t, int f)
{
    int c;
    uint64_t a;
    uint64_t b;
    uint64_t v;
    fp_t x;

    a = 0;
    b = 0;

    c = t->cur(t);
    if ( '0' == c ) {
        /* Hexal or octal number */
        c = t->next(t);
        if ( 'x' == c ) {
            /* Hexal number */
            (void)t->next(t);
            for ( ;; ) {
                c = t->cur(t);
                if ( c >= '0' && c <= '9' ) {
                    v = c - '0';
                } else if ( c >= 'a' && c <= 'f' ) {
                    v = c - 'a' + 10;
                } else if ( c >= 'A' && c <= 'F' ) {
                    v = c - 'A' + 10;
                } else {
                    break;
                }
                a = a * 16 + v;
                (void)t->next(t);
            }
        } else {
            /* Octal number */
            for ( ;; ) {
                c = t->cur(t);
                if ( c >= '0' && c <= '7' ) {
                    v = c - '0';
                } else {
                    break;
                }
                a = a * 8 + v;
                (void)t->next(t);
            }
        }

    } else {
        /* Decimal or floating point */
        if ( !f ) {
            for ( ;; ) {
                c = t->cur(t);
                if ( c >= '0' && c <= '9' ) {
                    v = c - '0';
                } else {
                    break;
                }
                a = a * 10 + v;
                (void)t->next(t);
            }
            c = t->cur(t);
            if ( '.' == c ) {
                /* Floating point */
                f = 1;
                (void)t->next(t);
            }
        }
        if ( f ) {
            /* Floating point */
            x = a;
            b = 10;
            for ( ;; ) {
                c = t->cur(t);
                if ( c >= '0' && c <= '9' ) {
                    v = c - '0';
                } else {
                    break;
                }
                x = x + (fp_t)v / b;
                b = b * 10;
                (void)t->next(t);
            }
        }
    }

    if ( f ) {
        /* Floating point */
        RETURN_ON_ERROR(_push_token_float(t, x), -AL_EINVALTOK);
    } else {
        /* Integer */
        RETURN_ON_ERROR(_push_token_int(t, a), -AL_EINVALTOK);
    }

    return 1;
}

/*
 * Scan keyword
 */
static int
_scan_keyword(al_tokenizer_t *t)
{
    int c;
    char buf[TOK_KW_MAXLEN];
    off_t pos;
    al_token_type_t type;

    /* Scan keyword to the buffer */
    pos = 0;
    c = t->cur(t);
    while ( IS_KEYWORD_CHAR(c) ) {
        buf[pos++] = c;
        c = t->next(t);
        /* Keyword size exceeds the maximum length */
        if ( pos >= TOK_KW_MAXLEN ) {
            return -AL_EINVALTOK;
        }
    }
    buf[pos] = '\0';

    /* Keywords */
    if ( 0 == strcmp(buf, "nil") ) {
        type = TOK_NIL;
    } else if ( 0 == strcmp(buf, "true") ) {
        type = TOK_TRUE;
    } else if ( 0 == strcmp(buf, "false") ) {
        type = TOK_FALSE;
    } else if ( 0 == strcmp(buf, "or") ) {
        type = TOK_KW_OR;
    } else if ( 0 == strcmp(buf, "and") ) {
        type = TOK_KW_AND;
    } else if ( 0 == strcmp(buf, "not") ) {
        type = TOK_KW_AND;
    } else if ( 0 == strcmp(buf, "fn") ) {
        type = TOK_KW_FN;
    } else if ( 0 == strcmp(buf, "import") ) {
        type = TOK_KW_IMPORT;
    } else if ( 0 == strcmp(buf, "package") ) {
        type = TOK_KW_PACKAGE;
    } else if ( 0 == strcmp(buf, "return") ) {
        type = TOK_KW_RETURN;
    } else if ( 0 == strcmp(buf, "continue") ) {
        type = TOK_KW_CONTINUE;
    } else if ( 0 == strcmp(buf, "break") ) {
        type = TOK_KW_BREAK;
    } else if ( 0 == strcmp(buf, "if") ) {
        type = TOK_KW_IF;
    } else if ( 0 == strcmp(buf, "else") ) {
        type = TOK_KW_ELSE;
    } else if ( 0 == strcmp(buf, "while") ) {
        type = TOK_KW_WHILE;
    } else if ( 0 == strcmp(buf, "for") ) {
        type = TOK_KW_FOR;
    } else {
        type = TOK_ID;
    }

    if ( TOK_ID == type ) {
        RETURN_ON_ERROR(_push_token_id(t, buf), -AL_EINVALTOK);
    } else {
        RETURN_ON_ERROR(_push_token(t, type), -AL_EINVALTOK);
    }

    return 1;
}

/*
 * Scan one chara
 */
static int
_get_one_char(al_tokenizer_t *t)
{
    int c;
    int v;
    int w;
    int i;

    c = t->cur(t);
    if ( '\\' == c ) {
        /* Escaped character */
        c = t->next(t);
        if ( c >= '0' && c <= '7' ) {
            /* Octdecimal */
            v = 0;
            for ( i = 0; i < 3; i++ ) {
                c = t->cur(t);
                if ( c < '0' || c > '7' ) {
                    break;
                }
                v = v * 8 + (c - '0');
                (void)t->next(t);
            }
            return v;
        } else if ( 'x' == c ) {
            /* Hexdecimal */
            c = t->next(t);
            if ( (c < '0' || c > '9') && (c < 'a' || c > 'f')
                 && (c < 'A' || c > 'F') ) {
                return 'x';
            }
            v = 0;
            for ( i = 0; i < 2; i++ ) {
                c = t->cur(t);
                if ( c >= '0' && c <= '9' ) {
                    w = c - '0';
                } else if ( c >= 'a' && c <= 'f' ) {
                    w = c - 'a' + 10;
                } else if ( c >= 'A' && c <= 'F' ) {
                    w = c - 'A' + 10;
                } else {
                    break;
                }
                v = v * 16 + w;
                (void)t->next(t);
            }
            return v;
        } else {
            /* as-is */
            (void)t->next(t);
            return c;
        }
    } else {
        (void)t->next(t);
        return c;
    }
}

/*
 * Scan keyword
 */
static int
_scan_string(al_tokenizer_t *t)
{
    int v;
    unsigned char *s;
    unsigned char *ns;
    off_t pos;
    size_t sz;

    sz = 1024;
    s = malloc(sz);
    pos = 0;

    (void)t->next(t);
    while ( '"' != t->cur(t) ) {
        if ( (size_t)pos >= sz ) {
            sz += 1024;
            ns = realloc(s, sz);
            if ( NULL == ns ) {
                free(s);
                return -1;
            }
            s = ns;
        }

        v = _get_one_char(t);
        s[pos] = v;
        pos++;
    }
    (void)t->next(t);

    RETURN_ON_ERROR(_push_token_str(t, s, pos), -AL_EINVALTOK);

    return 1;
}

/*
 * Scan character
 */
static int
_scan_char(al_tokenizer_t *t)
{
    int c;
    int v;

    (void)t->next(t);
    v = _get_one_char(t);

    c = t->cur(t);
    if ( '\'' != c ) {
        return -1;
    }
    (void)t->next(t);

    RETURN_ON_ERROR(_push_token_char(t, v), -AL_EINVALTOK);

    return 1;
}

/*
 * Skip white spaces
 */
static void
_skip_whitespaces(al_tokenizer_t *t)
{
    int c;

    for ( ;; ) {
        c = t->cur(t);
        /* Skip ``\t'', ``\n'', ``\v'', ``\f'', ``\r'' and `` ''. */
        if ( !isspace(c) ) {
            /* Not a whitespace */
            break;
        } else if ( '\n' == c ) {
            /* New line */
            break;
        } else {
            (void)t->next(t);
        }
    }
}

/*
 * Skip line comment
 */
static void
_skip_comment_line(al_tokenizer_t *t)
{
    int c;

    for ( ;; ) {
        c = t->cur(t);
        if ( '\n' == c ) {
            /* New line */
            break;
        } else {
            (void)t->next(t);
        }
    }
}

/*
 * Skip block comment
 */
static void
_skip_comment_block(al_tokenizer_t *t)
{
    int c;

    for ( ;; ) {
        c = t->cur(t);
        if ( '*' == c ) {
            c = t->next(t);
            if ( '/' == c ) {
                break;
            }
        } else {
            (void)t->next(t);
        }
    }
}

/*
 * Next token
 */
static int
_next_token(al_tokenizer_t *t)
{
    int c;
    int c0;
    int ret;

    /* Skip whitespaces */
    _skip_whitespaces(t);

    c = t->cur(t);
    ret = 1;
    switch ( c ) {
    case EOF:
        /* End-of-File */
        return 0;
    case '\\':
        /* Escape */
        c0 = t->next(t);
        if ( '\r' == c0 ) {
            /* Skip CR */
            c0 = t->next(t);
        }
        if ( '\n' == c0 ) {
            /* Skip LF */
            (void)t->next(t);
        } else {
            /* Error: Invalid token */
            return -AL_EINVALTOK;
        }
        break;
    case '\n':
        /* Newline */
        RETURN_ON_ERROR(_push_token(t, TOK_NEWLINE), -AL_EINVALTOK);
        (void)t->next(t);
        break;
    case '[':
        /* Left bracket */
        RETURN_ON_ERROR(_push_token(t, TOK_LBRACKET), -AL_EINVALTOK);
        (void)t->next(t);
        break;
    case ']':
        /* Right bracket */
        RETURN_ON_ERROR(_push_token(t, TOK_RBRACKET), -AL_EINVALTOK);
        (void)t->next(t);
        break;
    case '{':
        /* Left brace */
        RETURN_ON_ERROR(_push_token(t, TOK_LBRACE), -AL_EINVALTOK);
        (void)t->next(t);
        break;
    case '}':
        /* Right brace */
        RETURN_ON_ERROR(_push_token(t, TOK_RBRACE), -AL_EINVALTOK);
        (void)t->next(t);
        break;
    case '(':
        /* Left parenthesis */
        RETURN_ON_ERROR(_push_token(t, TOK_LPAREN), -AL_EINVALTOK);
        (void)t->next(t);
        break;
    case ')':
        /* Right parenthesis */
        RETURN_ON_ERROR(_push_token(t, TOK_RPAREN), -AL_EINVALTOK);
        (void)t->next(t);
        break;
    case ',':
        /* Comma */
        RETURN_ON_ERROR(_push_token(t, TOK_COMMA), -AL_EINVALTOK);
        (void)t->next(t);
        break;
    case ':':
        /* Colon */
        c0 = t->next(t);
        if ( '=' == c0 ) {
            RETURN_ON_ERROR(_push_token(t, TOK_DEF), -AL_EINVALTOK);
            (void)t->next(t);
        } else {
            RETURN_ON_ERROR(_push_token(t, TOK_COLON), -AL_EINVALTOK);
        }
        break;
    case ';':
        /* Semicolon */
        RETURN_ON_ERROR(_push_token(t, TOK_SEMICOLON), -AL_EINVALTOK);
        (void)t->next(t);
        break;
    case '-':
        /* Minus */
        RETURN_ON_ERROR(_push_token(t, TOK_MINUS), -AL_EINVALTOK);
        (void)t->next(t);
        break;
    case '+':
        /* Plus */
        RETURN_ON_ERROR(_push_token(t, TOK_PLUS), -AL_EINVALTOK);
        (void)t->next(t);
        break;
    case '*':
        /* Asterisk */
        RETURN_ON_ERROR(_push_token(t, TOK_ASTERISK), -AL_EINVALTOK);
        (void)t->next(t);
        break;
    case '/':
        /* Slash */
        c0 = t->next(t);
        if ( '/' == c0 ) {
            /* Line comment */
            _skip_comment_line(t);
        } else if ( '*' == c0 ) {
            /* Block comment */
            _skip_comment_block(t);
        } else {
            RETURN_ON_ERROR(_push_token(t, TOK_SLASH), -AL_EINVALTOK);
        }
        break;
    case '%':
        /* Percent */
        RETURN_ON_ERROR(_push_token(t, TOK_PERCENT), -AL_EINVALTOK);
        (void)t->next(t);
        break;
    case '&':
        /* Ampersand */
        RETURN_ON_ERROR(_push_token(t, TOK_AMP), -AL_EINVALTOK);
        (void)t->next(t);
        break;
    case '|':
        /* Bar */
        RETURN_ON_ERROR(_push_token(t, TOK_BAR), -AL_EINVALTOK);
        (void)t->next(t);
        break;
    case '~':
        /* Tilde */
        RETURN_ON_ERROR(_push_token(t, TOK_TILDE), -AL_EINVALTOK);
        (void)t->next(t);
        break;
    case '^':
        /* Hat */
        RETURN_ON_ERROR(_push_token(t, TOK_HAT), -AL_EINVALTOK);
        (void)t->next(t);
        break;
    case '@':
        /* Ampersand */
        RETURN_ON_ERROR(_push_token(t, TOK_AT), -AL_EINVALTOK);
        (void)t->next(t);
        break;
    case '.':
        /* Period */
        c0 = t->next(t);
        if ( isdigit(c0) ) {
            ret = _scan_number(t, 1);
        } else {
            RETURN_ON_ERROR(_push_token(t, TOK_HAT), -AL_EINVALTOK);
        }
        break;
    case '!':
        /* Not */
        c0 = t->next(t);
        if ( '=' == c0 ) {
            RETURN_ON_ERROR(_push_token(t, TOK_NEQ), -AL_EINVALTOK);
            (void)t->next(t);
        } else {
            RETURN_ON_ERROR(_push_token(t, TOK_NOT), -AL_EINVALTOK);
        }
        break;
    case '<':
        /* Less than */
        c0 = t->next(t);
        if ( '=' == c0 ) {
            RETURN_ON_ERROR(_push_token(t, TOK_LEQ), -AL_EINVALTOK);
            (void)t->next(t);
        } else if ( '<' == c0 ) {
            RETURN_ON_ERROR(_push_token(t, TOK_LSHIFT), -AL_EINVALTOK);
            (void)t->next(t);
        } else {
            RETURN_ON_ERROR(_push_token(t, TOK_LT), -AL_EINVALTOK);
        }
        break;
    case '>':
        /* Greater than */
        c0 = t->next(t);
        if ( '=' == c0 ) {
            RETURN_ON_ERROR(_push_token(t, TOK_GEQ), -AL_EINVALTOK);
            (void)t->next(t);
        } else if ( '<' == c0 ) {
            RETURN_ON_ERROR(_push_token(t, TOK_RSHIFT), -AL_EINVALTOK);
            (void)t->next(t);
        } else {
            RETURN_ON_ERROR(_push_token(t, TOK_GT), -AL_EINVALTOK);
        }
        break;
    case '=':
        /* Equal */
        c0 = t->next(t);
        if ( '=' == c0 ) {
            RETURN_ON_ERROR(_push_token(t, TOK_EQ_EQ), -AL_EINVALTOK);
            (void)t->next(t);
        } else {
            RETURN_ON_ERROR(_push_token(t, TOK_EQ), -AL_EINVALTOK);
        }
        break;
    case '"':
        /* String */
        ret = _scan_string(t);
        break;
    case '\'':
        /* Character */
        ret = _scan_char(t);
        break;
    default:
        /* Default */
        if ( isdigit(c) ) {
            ret = _scan_number(t, 0);
        } else {
            ret = _scan_keyword(t);
        }
    }

    return ret;
}

/*
 * Initialize the tokenizer
 */
al_tokenizer_t *
tokenizer_init(al_tokenizer_t *t, char *buf, size_t sz)
{
    al_token_list_t *l;

    l = malloc(sizeof(al_token_list_t));
    if ( NULL == l ) {
        return NULL;
    }

    if ( NULL == t ) {
        t = malloc(sizeof(al_tokenizer_t));
        if ( NULL == t ) {
            free(l);
            return NULL;
        }
        t->_allocated = 1;
    } else {
        t->_allocated = 0;
    }
    t->buf = buf;
    t->sz = sz;
    t->off = 0;
    t->cur = tokenizer_cur;
    t->next = tokenizer_next;

    /* Initialize token list */
    t->tokens = l;
    t->tokens->head = NULL;
    t->tokens->tail = NULL;

    return t;
}

/*
 * Release tokenizer
 */
void
tokenizer_release(al_tokenizer_t *t)
{
    if ( t->_allocated ) {
        free(t);
    }
}

/*
 * Release a token
 */
void
token_release(al_token_t *tok)
{
    if ( TOK_ID == tok->type ) {
        free(tok->u.id);
    }
    free(tok);
}

/*
 * Release the list of tokens
 */
void
token_list_release(al_token_list_t *l)
{
    al_token_entry_t *e;

    e = l->head;
    while ( NULL != e ) {
        token_release(e->tok);
        free(e);
        e = e->next;
    }
    free(l);
}

/*
 * Tokenize
 */
al_token_list_t *
tokenizer_tokenize(char *input)
{
    al_tokenizer_t *t;
    int ret;

    /* Initialize the tokenizer */
    t = tokenizer_init(NULL, input, strlen(input));
    if ( NULL == t ) {
        return NULL;
    }
    do {
        ret = _next_token(t);
    } while ( ret > 0 );

    if ( ret < 0 ) {
        /* Release tokenizer and tokens */
        token_list_release(t->tokens);
        tokenizer_release(t);
        return NULL;
    }
    free(t);

    return t->tokens;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
