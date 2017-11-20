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

#ifndef _TOKEN_H
#define _TOKEN_H

#include "itype.h"
#include <stdio.h>
#include <stdint.h>

#define TOK_KW_MAXLEN   1024

/*
 * Token type
 */
typedef enum {
    /* ID */
    TOK_ID,

    /* Newline */
    TOK_NEWLINE,                /* \n */

    /* Nil */
    TOK_NIL,                    /* nil */

    /* Boolean */
    TOK_TRUE,                   /* true */
    TOK_FALSE,                  /* false */

    /* Literals */
    TOK_LIT_STR,
    TOK_LIT_CHAR,
    TOK_INT,
    TOK_FLOAT,

    /* Expressions */
    TOK_EQ,                     /* = */
    TOK_DEF,                    /* := */
    TOK_EQ_EQ,                  /* == */
    TOK_NEQ,                    /* != */
    TOK_GT,                     /* > */
    TOK_GEQ,                    /* >= */
    TOK_LT,                     /* < */
    TOK_LEQ,                    /* <= */
    TOK_NOT,                    /* ! */
    TOK_PLUS,                   /* + */
    TOK_MINUS,                  /* - */
    TOK_ASTERISK,               /* * */
    TOK_SLASH,                  /* / */
    TOK_PERCENT,                /* % */

    /* Bit-wise operations */
    TOK_TILDE,                  /* ~ */
    TOK_LSHIFT,                 /* << */
    TOK_RSHIFT,                 /* >> */
    TOK_HAT,                    /* ^ */
    TOK_AMP,                    /* & */
    TOK_BAR,                    /* | */

    /* Symbol */
    TOK_LPAREN,                 /* ( */
    TOK_RPAREN,                 /* ) */
    TOK_LBRACE,                 /* { */
    TOK_RBRACE,                 /* } */
    TOK_LBRACKET,               /* [ */
    TOK_RBRACKET,               /* ] */
    TOK_PERIOD,                 /* . */
    TOK_COMMA,                  /* , */
    TOK_COLON,                  /* : */
    TOK_SEMICOLON,              /* ; */
    TOK_AT,                     /* @ */

    /* Keywords */
    TOK_KW_FN,                  /* fn */
    TOK_KW_IMPORT,              /* import */
    TOK_KW_PACKAGE,             /* package */
    TOK_KW_OR,                  /* or */
    TOK_KW_AND,                 /* and */
    TOK_KW_NOT,                 /* not */
    TOK_KW_RETURN,              /* return */
    TOK_KW_CONTINUE,            /* continue */
    TOK_KW_BREAK,               /* break */
    TOK_KW_IF,                  /* if */
    TOK_KW_ELSE,                /* else */
    TOK_KW_WHILE,               /* while */
    TOK_KW_FOR,                 /* for */

} al_token_type_t;

/*
 * Token
 */
typedef struct {
    al_token_type_t type;
    union {
        char *id;
        unsigned char c;
        al_string_t s;
        int_t i;
        fp_t f;
    } u;
} al_token_t;

/*
 * Token list
 */
typedef struct _token_entry al_token_entry_t;
struct _token_entry {
    al_token_t *tok;
    al_token_entry_t *next;
};
typedef struct {
    al_token_entry_t *head;
    al_token_entry_t *tail;
} al_token_list_t;


/*
 * Print the given token
 */
static __inline__ void
al_print_token(al_token_t *tok)
{
    ssize_t i;

    switch ( tok->type ) {
    case TOK_ID:
        printf("%s", tok->u.id);
        break;
    case TOK_NIL:
        printf("nil");
        break;
    case TOK_TRUE:
        printf("true");
        break;
    case TOK_FALSE:
        printf("false");
        break;
    case TOK_KW_OR:
        printf("or");
        break;
    case TOK_KW_AND:
        printf("and");
        break;
    case TOK_KW_NOT:
        printf("not");
        break;
    case TOK_KW_FN:
        printf("fn");
        break;
    case TOK_KW_RETURN:
        printf("return");
        break;
    case TOK_KW_CONTINUE:
        printf("continue");
        break;
    case TOK_KW_BREAK:
        printf("break");
        break;
    case TOK_KW_IF:
        printf("if");
        break;
    case TOK_KW_ELSE:
        printf("else");
        break;
    case TOK_KW_WHILE:
        printf("while");
        break;
    case TOK_KW_FOR:
        printf("for");
        break;
    case TOK_MINUS:
        printf("-");
        break;
    case TOK_PLUS:
        printf("+");
        break;
    case TOK_ASTERISK:
        printf("*");
        break;
    case TOK_SLASH:
        printf("/");
        break;
    case TOK_PERCENT:
        printf("%%");
        break;
    case TOK_AMP:
        printf("&");
        break;
    case TOK_BAR:
        printf("|");
        break;
    case TOK_TILDE:
        printf("~");
        break;
    case TOK_HAT:
        printf("^");
        break;
    case TOK_COMMA:
        printf(",");
        break;
    case TOK_PERIOD:
        printf(".");
        break;
    case TOK_NOT:
        printf("!");
        break;
    case TOK_NEQ:
        printf("!=");
        break;
    case TOK_AT:
        printf("@");
        break;
    case TOK_LT:
        printf("<");
        break;
    case TOK_LSHIFT:
        printf("<<");
        break;
    case TOK_LEQ:
        printf("<=");
        break;
    case TOK_GT:
        printf(">");
        break;
    case TOK_RSHIFT:
        printf(">>");
        break;
    case TOK_GEQ:
        printf(">=");
        break;
    case TOK_EQ:
        printf("=");
        break;
    case TOK_EQ_EQ:
        printf("==");
        break;
    case TOK_DEF:
        printf(":=");
        break;
    case TOK_LBRACKET:
        printf("[");
        break;
    case TOK_RBRACKET:
        printf("]");
        break;
    case TOK_LBRACE:
        printf("{");
        break;
    case TOK_RBRACE:
        printf("}");
        break;
    case TOK_LPAREN:
        printf("(");
        break;
    case TOK_RPAREN:
        printf(")");
        break;
    case TOK_COLON:
        printf(":");
        break;
    case TOK_SEMICOLON:
        printf(";");
        break;
    case TOK_NEWLINE:
        printf("\n");
        break;
    case TOK_FLOAT:
        printf("%" AL_PRIf, tok->u.f);
        break;
    case TOK_INT:
        printf("%" AL_PRIu, tok->u.i);
        break;
    case TOK_LIT_CHAR:
        printf("%02x", tok->u.c);
        break;
    case TOK_LIT_STR:
        for ( i = 0; i < (ssize_t)tok->u.s.len; i++ ) {
            printf("%02x", tok->u.s.s[i]);
        }
        break;
    default:
        printf("[UNK]");
    }
}


#endif /* _TOKEN_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
