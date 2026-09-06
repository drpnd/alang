/*_
 * Copyright (c) 2018-2024,2026 Hirochika Asai <asai@jar.jp>
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

%option reentrant
%option extra-type="void *"
%option header-file="lex.yy.h"
%option bison-bridge
%option bison-locations
%option yylineno

%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compile.h"
#include "y.tab.h"

#define STRING_CHUNK 4096

#define YY_USER_ACTION _update_locations(yylloc, yytext);

/* Declaration of yyget_extra for functions in this file */
void * yyget_extra(yyscan_t);
void yyerror(YYLTYPE *, yyscan_t, const char *);

/*
 * yywrap
 */
int
yywrap(yyscan_t scanner)
{
    return 1;
}

/*
 * _update_locations -- update yylloc
 */
static void
_update_locations(YYLTYPE *loc, char *s)
{
    int i;
    loc->first_line = loc->last_line;
    loc->first_column = loc->last_column;
    for ( i = 0; s[i] != '\0'; i++ ) {
        if ( s[i] == '\n' ) {
            loc->last_line++;
            loc->last_column = 0;
        } else {
            loc->last_column++;
        }
    }
}

/*
 * _clear_buffer -- clear string buffer
 */
static void
_clear_buffer(YYLTYPE *loc, yyscan_t scanner)
{
    context_t *context;

    context = yyget_extra(scanner);

    context->buffer.len = 0;
    if ( NULL == context->buffer.buf ) {
        context->buffer.buf = malloc(STRING_CHUNK);
        if ( NULL == context->buffer.buf ) {
            yyerror(loc, scanner, "Memory error");
        }
        context->buffer.size = STRING_CHUNK;
    }
}

/*
 * _append_buffer_str -- append string to the buffer
 */
static void
_append_buffer_str(YYLTYPE *loc, yyscan_t scanner, const char *s)
{
    size_t len;
    size_t nsize;
    char *ptr;
    context_t *context;

    context = yyget_extra(scanner);

    len = strlen(s);
    nsize = context->buffer.size;
    while ( context->buffer.len + len > nsize ) {
        nsize += STRING_CHUNK;
    }
    if ( nsize != context->buffer.size ) {
        ptr = realloc(context->buffer.buf, nsize);
        if ( NULL == ptr ) {
            yyerror(loc, scanner, "Memory error");
        }
        context->buffer.buf = ptr;
        context->buffer.size = nsize;
    }
    memcpy(context->buffer.buf + context->buffer.len, s, len);
    context->buffer.len += len;
}

/*
 * _append_buffer_char -- append a character to the buffer
 */
static void
_append_buffer_char(YYLTYPE *loc, yyscan_t scanner, char c)
{
    size_t nsize;
    char *ptr;
    context_t *context;

    context = yyget_extra(scanner);

    if ( context->buffer.len + 1 > context->buffer.size ) {
        nsize = context->buffer.size + STRING_CHUNK;
        nsize += STRING_CHUNK;
        ptr = realloc(context->buffer.buf, nsize);
        if ( NULL == ptr ) {
            yyerror(loc, scanner, "Memory error");
        }
        context->buffer.buf = ptr;
        context->buffer.size = nsize;
    }
    context->buffer.buf[context->buffer.len] = c;
    context->buffer.len++;
}

%}
%x string
%x comment
%x blockcomment
%%
    /* Range operators (must come before single-char operators) */
"!..="      return TOK_BANG_DOTS_EQ;
"!.."       return TOK_BANG_DOTS;
"..="       return TOK_DOTS_EQ;
".."        return TOK_DOTS;

    /* Arrow and pipe operators */
"->"        return TOK_ARROW;
"=>"        return TOK_FATARROW;
"|>"        return TOK_PIPE;
"<-"        return TOK_LARROW;

    /* Assignment compound operators */
"+="        return TOK_PLUS_EQ;
"-="        return TOK_MINUS_EQ;
"*="        return TOK_MUL_EQ;
"/="        return TOK_DIV_EQ;
"%="        return TOK_MOD_EQ;
"&="        return TOK_AND_EQ;
"|="        return TOK_OR_EQ;
"^="        return TOK_XOR_EQ;
"<<="       return TOK_LSHIFT_EQ;
">>="       return TOK_RSHIFT_EQ;

    /* Arithmetic operations */
"+"         return TOK_ADD;
"-"         return TOK_SUB;
"*"         return TOK_MUL;
"/"         return TOK_DIV;
"%"         return TOK_MOD;

    /* Bitwise operations */
"<<"        return TOK_BIT_LSHIFT;
">>"        return TOK_BIT_RSHIFT;
"~"         return TOK_BIT_NOT;

    /* Logical operations */
"||"        return TOK_LOR;
"&&"        return TOK_LAND;
"!"         return TOK_NOT;

    /* Parentheses, brackets, etc... */
"("         return TOK_LPAREN;
")"         return TOK_RPAREN;
"{"         return TOK_LBRACE;
"}"         return TOK_RBRACE;
"["         return TOK_LBRACKET;
"]"         return TOK_RBRACKET;
","         return TOK_COMMA;
"."         return TOK_DOT;
":"         return TOK_COLON;
";"         return TOK_SEMICOLON;

    /* Comparison */
"=="        return TOK_EQ_EQ;
"!="        return TOK_NEQ;
"<="        return TOK_LEQ;
">="        return TOK_GEQ;
"<"         return TOK_LT;
">"         return TOK_GT;
"="         return TOK_EQ;

    /* Bitwise (single-char, must come after compound) */
"|"         return TOK_BIT_OR;
"&"         return TOK_BIT_AND;
"^"         return TOK_BIT_XOR;

    /* Control sequences */
[\r\n\t ]   ;                       /* ignore whitespaces */

    /* Primitive types */
"i8"        return TOK_TYPE_I8;
"i16"       return TOK_TYPE_I16;
"i32"       return TOK_TYPE_I32;
"i64"       return TOK_TYPE_I64;
"u8"        return TOK_TYPE_U8;
"u16"       return TOK_TYPE_U16;
"u32"       return TOK_TYPE_U32;
"u64"       return TOK_TYPE_U64;
"f16"       return TOK_TYPE_F16;
"f32"       return TOK_TYPE_F32;
"f64"       return TOK_TYPE_F64;
"fp4"       return TOK_TYPE_FP4;
"fp8"       return TOK_TYPE_FP8;
"string"    return TOK_TYPE_STRING;
"bool"      return TOK_TYPE_BOOL;

    /* Reserved keywords */
"fn"        return TOK_FN;
"coro"      return TOK_CORO;
"let"       return TOK_LET;
"mut"       return TOK_MUT;
"if"        return TOK_IF;
"else"      return TOK_ELSE;
"match"     return TOK_MATCH;
"for"       return TOK_FOR;
"in"        return TOK_IN;
"while"     return TOK_WHILE;
"loop"      return TOK_LOOP;
"break"     return TOK_BREAK;
"return"    return TOK_RETURN;
"yield"     return TOK_YIELD;
"await"     return TOK_AWAIT;
"node"      return TOK_NODE;
"source"    return TOK_SOURCE;
"sink"      return TOK_SINK;
"graph"     return TOK_GRAPH;
"struct"    return TOK_STRUCT;
"enum"      return TOK_ENUM;
"type"      return TOK_TYPE;
"as"        return TOK_AS;
"true"      return TOK_TRUE;
"false"     return TOK_FALSE;
"pub"       return TOK_PUB;
"mod"       return TOK_MOD_KW;
"use"       return TOK_USE;

    /* Identifier */
[A-Za-z\_][A-Za-z\_0-9]* {
    char *val;
    val = strdup(yytext);
    if ( NULL == val ) {
        return *yytext;
    }
    yylval->idval = val;
    return TOK_ID;
}

    /* Binary numbers */
0b[01]+ {
    char *val;
    val = strdup(yytext + 2);
    if ( NULL == val ) {
        return *yytext;
    }
    yylval->numval = val;
    return TOK_LIT_BININT;
}
    /* Hexadecimal numbers */
0x[0-9a-fA-F]+ {
    char *val;
    val = strdup(yytext + 2);
    if ( NULL == val ) {
        return *yytext;
    }
    yylval->numval = val;
    return TOK_LIT_HEXINT;
}
    /* Decimal numbers */
[0-9]+ {
    char *val;
    val = strdup(yytext);
    if ( NULL == val ) {
        return *yytext;
    }
    yylval->numval = val;
    return TOK_LIT_DECINT;
}

    /* Floating point */
[0-9]+\.[0-9]+ {
    char *val;
    val = strdup(yytext);
    if ( NULL == val ) {
        return *yytext;
    }
    yylval->numval = val;
    return TOK_LIT_FLOAT;
}
    /* Floating point (<1.0) */
\.[0-9]+ {
    char *val;
    val = strdup(yytext);
    if ( NULL == val ) {
        return *yytext;
    }
    yylval->numval = val;
    return TOK_LIT_FLOAT;
}
    /* Floating point (d.0) */
[0-9]+\. {
    char *val;
    val = strdup(yytext);
    if ( NULL == val ) {
        return *yytext;
    }
    yylval->numval = val;
    return TOK_LIT_FLOAT;
}

    /* String format */
\" { BEGIN string; _clear_buffer(yylloc, yyscanner); }
<string>[^\\"\n]* {
    _append_buffer_str(yylloc, yyscanner, yytext);
}
<string>\\[0-7][0-7]?[0-7]? {
    _append_buffer_char(yylloc, yyscanner, strtol(yytext + 1, 0, 8));
}
<string>\\x[0-9a-fA-F][0-9a-fA-F]? {
    _append_buffer_char(yylloc, yyscanner, strtol(yytext + 2, 0, 16));
}
<string>\\[\\"] {
    _append_buffer_char(yylloc, yyscanner, yytext[1]);
}
<string>\" {
    context_t *context;
    context = yyget_extra(yyscanner);
    yylval->strval = strdup(context->buffer.buf);
    BEGIN 0;
    return TOK_LIT_STR;
}

    /* Line comment */
\/\/ { BEGIN comment; }
<comment>[^\n]*\n {
    BEGIN 0;
}

    /* Block comment */
\/\* { BEGIN blockcomment; }
<blockcomment>.*\*\/ {
    BEGIN 0;
}

    /* EOF */
<<EOF>> {
    yyterminate();
}

%%

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
