/*_
 * Copyright (c) 2017-2018 Hirochika Asai <asai@jar.jp>
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

#include "syntax.h"
#include "compiler.h"
#include "pe.h"
#include "mach-o.h"
#include <stdlib.h>

/*
 * Initialize the compiler
 */
al_compiler_t *
compiler_init(al_compiler_t *compiler, al_decl_vec_t *program)
{
    if ( NULL == compiler ) {
        compiler = malloc(sizeof(al_compiler_t));
        if ( NULL == compiler ) {
            return NULL;
        }
        compiler->_allocated = 1;
    } else {
        compiler->_allocated = 0;
    }
    compiler->program = program;

    return compiler;
}

/*
 * Release the compiler
 */
void
compiler_release(al_compiler_t *compiler)
{
    if ( compiler->_allocated ) {
        free(compiler);
    }
}

/*
 * Function
 */
al_object_t *
compiler_compile_function(al_compiler_t *compiler, al_decl_t *decl)
{
    ssize_t i;
    al_identifier_t *id;

    /* Function name */
    printf("function: %s %s\n", decl->u.fn.f->id, decl->u.fn.f->type);

    /* Define the arguments */
    for ( i = 0; i < (ssize_t)decl->u.fn.ps->size; i++ ) {
        id = decl->u.fn.ps->elems[i];
        printf("arg %s %s\n", id->id, id->type);
    }

    /* Define the return values */
    for ( i = 0; i < (ssize_t)decl->u.fn.rv->size; i++ ) {
        id = decl->u.fn.rv->elems[i];
        printf("ret %s %s\n", id->id, id->type);
    }

    for ( i = 0; i < (ssize_t)decl->u.fn.b->size; i++ ) {
        printf("xxxx\n");
    }

    return NULL;
}

/*
 * Package
 */
al_object_t *
compiler_compile_package(al_compiler_t *compiler, al_decl_t *decl)
{
    printf("package: %s\n", decl->u.package.name);

    return NULL;
}

/*
 * Compile
 */
al_object_t *
compiler_compile(al_decl_vec_t *program)
{
    ssize_t i;
    al_decl_t *decl;
    al_compiler_t compiler;

    //pe_out();
    //mach_o_test(stdout);
    //return 0;
    if ( NULL == compiler_init(&compiler, program) ) {
        return NULL;
    }

    for ( i = 0; i < (ssize_t)program->size; i++ ) {
        decl = program->elems[i];
        switch ( decl->type ) {
        case DECL_FN:
            compiler_compile_function(&compiler, decl);
            break;
        case DECL_PACKAGE:
            compiler_compile_package(&compiler, decl);
            break;
        case DECL_IMPORT:
            printf("import\n");
            break;
        }
    }

    return 0;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
