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

#include <stdint.h>

/*
 * Token type
 */
typedef enum {

    /* Nil */
    TOK_NIL,                    /* nil */

    /* Literals */
    TOK_LIT_INT,
    TOK_LIT_STR,

    /* Types */
    TOK_I32,                    /* i32 */
    TOK_U32,                    /* u32 */
    TOK_I64,                    /* i64 */
    TOK_U64,                    /* u64 */
    TOK_FP16,                   /* fp16 */
    TOK_FP32,                   /* fp32 */
    TOK_FP64,                   /* fp64 */

    /* Expressions */
    TOK_EQ,                     /* = */
    TOK_DEF,                    /* := */
    TOK_EQEQ,                   /* == */
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
    TOK_XOR,                    /* ^ */
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

} al_token_type_t;

/*
 * Literals
 */
typedef struct {
    int64_t i64;
    uint64_t u64;
} al_lit_token_t;

/*
 * Token
 */
typedef struct {
    al_token_type_t type;
} al_token_t;

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif /* _TOKEN_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
