/*_
 * Copyright (c) 2018-2019,2021 Hirochika Asai <asai@jar.jp>
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

%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "y.tab.h"
#define YY_USER_ACTION yylloc.first_line = yylloc.last_line = yylineno;

#define STRING_CHUNK 4096
/*
 * String
 */
struct string {
    size_t len;
    size_t size;
    char *buf;
};

void yyerror(char *);
int
yywrap(void)
{
    return 1;
}

static struct string buffer;

/*
 * Clear string buffer
 */
static void
_clear_buffer(void)
{
    buffer.len = 0;
    if ( NULL == buffer.buf ) {
        buffer.buf = malloc(STRING_CHUNK);
        if ( NULL == buffer.buf ) {
            yyerror("Memory error");
        }
        buffer.size = STRING_CHUNK;
    }
}

/*
 * Append string to the buffer
 */
static void
_append_buffer_str(const char *s)
{
    size_t len;
    size_t nsize;
    char *ptr;

    len = strlen(s);
    nsize = buffer.size;
    while ( buffer.len + len > nsize ) {
        nsize += STRING_CHUNK;
    }
    if ( nsize != buffer.size ) {
        ptr = realloc(buffer.buf, nsize);
        if ( NULL == ptr ) {
            yyerror("Memory error");
        }
        buffer.buf = ptr;
        buffer.size = nsize;
    }
    memcpy(buffer.buf + buffer.len, s, len);
    buffer.len += len;
}

/*
 * Append a character to the buffer
 */
static void
_append_buffer_char(char c)
{
    size_t len;
    size_t nsize;
    char *ptr;

    if ( buffer.len + 1 > buffer.size ) {
        nsize = buffer.size + STRING_CHUNK;
        nsize += STRING_CHUNK;
        ptr = realloc(buffer.buf, nsize);
        if ( NULL == ptr ) {
            yyerror("Memory error");
        }
        buffer.buf = ptr;
        buffer.size = nsize;
    }
    buffer.buf[buffer.len] = c;
    buffer.len++;
}

%}
%x string
%x comment
%x blockcomment
%option yylineno
%%
    /* Arithmetic operations */
"+"         return TOK_ADD;
"-"         return TOK_SUB;
"*"         return TOK_MUL;
"/"         return TOK_DIV;
    /* Bitwise operations */
"|"         return TOK_BIT_OR;
"&"         return TOK_BIT_AND;
"<<"        return TOK_BIT_LSHIFT;
">>"        return TOK_BIT_RSHIFT;
    /* Parentheses, brackets, etc... */
"("         return TOK_LPAREN;
")"         return TOK_RPAREN;
"{"         return TOK_LBRACE;
"}"         return TOK_RBRACE;
"["         return TOK_LBRACKET;
"]"         return TOK_RBRACKET;
","         return TOK_COMMA;
"@"         return TOK_ATMARK;
    /* Definition */
":="        return TOK_DEF;
    /* Control sequences */
[\r\n\t ]   ;                       /* ignore whitespaces */
    /* Primitive types */
"i8"        return TOK_TYPE_I8;
"i16"       return TOK_TYPE_I16;
"i32"       return TOK_TYPE_I32;
"i64"       return TOK_TYPE_I64;
"fp32"      return TOK_TYPE_FP32;
"fp64"      return TOK_TYPE_FP64;
"string"    return TOK_TYPE_STRING;
    /* Reserved keywords */
"package"   return TOK_PACKAGE;
"import"    return TOK_IMPORT;
"include"   return TOK_INCLUDE;
"fn"        return TOK_FN;
"coroutine" return TOK_COROUTINE;
"struct"    return TOK_STRUCT;
"union"     return TOK_UNION;
"enum"      return TOK_ENUM;
    /* Identifier */
[A-Za-z\_][A-Za-z\_0-9]* {
    char *val;
    val = strdup(yytext);
    if ( NULL == val ) {
        return *yytext;
    }
    yylval.idval = val;
    return TOK_ID;
}
    /* Numerical numbers */
[1-9][0-9]* {
    int val;
    sscanf(yytext, "%d", &val);
    yylval.intval = val;
    return TOK_LIT_INT;
}
0x[0-9a-fA-F]+ {
    int val;
    yylval.intval = strtol(yytext + 2, NULL, 16);
    return TOK_LIT_INT;
}
0[0-7]* {
    int val;
    yylval.intval = strtol(yytext, NULL, 8);
    return TOK_LIT_INT;
}
[0-9]+\.[0-9]+ {
    float val;
    sscanf(yytext, "%f", &val);
    yylval.floatval = val;
    return TOK_LIT_FLOAT;

}
\.[0-9]+ {
    float val;
    sscanf(yytext, "%f", &val);
    yylval.floatval = val;
    return TOK_LIT_FLOAT;

}
[0-9]+\. {
    float val;
    sscanf(yytext, "%f", &val);
    yylval.floatval = val;
    return TOK_LIT_FLOAT;
}
    /* String format */
\" { BEGIN string; _clear_buffer(); }
<string>[^\\"\n]* {
    _append_buffer_str(yytext);
}
<string>\\[0-7][0-7]?[0-7]? {
    _append_buffer_char(strtol(yytext + 1, 0, 8));
}
<string>\\x[0-9a-fA-F][0-9a-fA-F]? {
    _append_buffer_char(strtol(yytext + 2, 0, 16));
}
<string>\\[\\"] {
    _append_buffer_char(yytext[1]);
}
<string>\" {
    yylval.strval = strdup(buffer.buf);
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

%%

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
