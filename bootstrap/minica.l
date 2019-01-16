/*_
 * Copyright (c) 2018-2019 Hirochika Asai <asai@jar.jp>
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
#include "y.tab.h"
#define YY_USER_ACTION yylloc.first_line = yylloc.last_line = yylineno;

int
yywrap(void)
{
    return 1;
}
%}
%option yylineno
%%
    /* Arithmetic operations */
"+"     return TOK_ADD;
"-"     return TOK_SUB;
"*"     return TOK_MUL;
"/"     return TOK_DIV;
    /* Bitwise operations */
"|"     return TOK_BIT_OR;
"&"     return TOK_BIT_AND;
"<<"    return TOK_BIT_LSHIFT;
">>"    return TOK_BIT_RSHIFT;
    /* Definition */
":="    return TOK_DEF;
    /* Control sequences */
[\t ]   ;                       /* ignore whitespaces */
"\n"    return TOK_NEWLINE;
    /* Identifier */
[A-Za-z\_]* {
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
%%

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
