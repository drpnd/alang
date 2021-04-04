/*_
 * Copyright (c) 2019,2021 Hirochika Asai <asai@jar.jp>
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

#ifndef _SYNTAX_H
#define _SYNTAX_H

#include <stdio.h>
#include <unistd.h>

#define VECTOR_INIT_SIZE    32
#define VECTOR_DELTA        32

/*
 * Typedefs
 */
typedef struct _expr expr_t;
typedef struct _stmt stmt_t;
typedef struct _stmt_list stmt_list_t;

/*
 * Literal types
 */
typedef enum {
    LIT_INT,
    LIT_FLOAT,
    LIT_STRING,
} literal_type_t;

/*
 * Literals
 */
typedef struct {
    literal_type_t type;
    union {
        int i;
        float f;
        char *s;
    } u;
} literal_t;

/*
 * Type type
 */
typedef enum {
    TYPE_PRIMITIVE_I8,
    TYPE_PRIMITIVE_I16,
    TYPE_PRIMITIVE_I32,
    TYPE_PRIMITIVE_I64,
    TYPE_PRIMITIVE_FP32,
    TYPE_PRIMITIVE_FP64,
    TYPE_PRIMITIVE_STRING,
    TYPE_ID,
} type_type_t;

/*
 * Types
 */
typedef struct {
    type_type_t type;
    union {
        char *id;
    } u;
} type_t;

/*
 * Declarations
 */
typedef struct {
    char *id;
    type_t *type;
} decl_t;

/*
 * Variable type
 */
typedef enum {
    VAR_ID,
    VAR_PTR,
    VAR_DECL,
} var_type_t;

/*
 * Variables
 */
typedef struct {
    var_type_t type;
    union {
        char *id;
        decl_t *decl;
    } u;
} var_t;

/*
 * Variable sets
 */
typedef struct _var_stack var_stack_t;
struct _var_stack {
    char *id;
    var_t *var;
    var_stack_t *next;
};

/*
 * Value type
 */
typedef enum {
    VAL_LITERAL,
    VAL_VAR,
} val_type_t;

/*
 * Values
 */
typedef struct {
    val_type_t type;
    union {
        literal_t *lit;
        var_t *var;
    } u;
} val_t;

/*
 * Arguments
 */
typedef struct _arg arg_t;
struct _arg {
    decl_t *decl;
    arg_t *next;
};

/*
 * Function
 */
typedef struct {
    char *id;
    arg_t *args;
    arg_t *rets;
    stmt_list_t *block;
    /* Variables */
    var_stack_t *vars;
} func_t;

/*
 * Coroutine
 */
typedef struct {
    char *id;
    arg_t *args;
    arg_t *rets;
    stmt_list_t *block;
} coroutine_t;

/*
 * Operations
 */
typedef enum {
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
} op_type_t;

/*
 * Type of fixes
 */
typedef enum {
    FIX_INFIX,
    FIX_PREFIX,
} fix_t;

/*
 * Expression type
 */
typedef enum {
    EXPR_VAL,
    EXPR_OP,
} expr_type_t;

/*
 * Operation
 */
typedef struct {
    op_type_t type;
    fix_t fix;
    expr_t *e0;
    expr_t *e1;
} op_t;

/*
 * Expression
 */
struct _expr {
    expr_type_t type;
    union {
        val_t *val;
        op_t *op;
    } u;
};

/*
 * Statement type
 */
typedef enum {
    STMT_DECL,
    STMT_ASSIGN,
    STMT_EXPR,
} stmt_type_t;

/*
 * Assign statement
 */
typedef struct {
    var_t *var;
    expr_t *e;
} stmt_assign_t;

/*
 * Statement
 */
struct _stmt {
    stmt_type_t type;
    union {
        decl_t *decl;
        stmt_assign_t assign;
        expr_t *expr;
    } u;
    stmt_t *next;
};

/*
 * Statements
 */
struct _stmt_list {
    stmt_t *head;
    stmt_t *tail;
};

/*
 * Import
 */
typedef struct {
    char *id;
} import_t;

/*
 * Functions
 */
typedef struct {
    size_t n;
    size_t size;
    func_t **vec;
} func_vec_t;

/*
 * Coroutines
 */
typedef struct {
    size_t n;
    size_t size;
    coroutine_t **vec;
} coroutine_vec_t;

/*
 * Import statements
 */
typedef struct {
    size_t n;
    size_t size;
    import_t **vec;
} import_vec_t;

/*
 * File stack
 */
typedef struct _file_Stack file_stack_t;
struct _file_stack {
    char *fname;
    FILE *fp;
    off_t first_line;
    off_t first_column;
    off_t last_line;
    off_t last_column;
    file_stack_t *next;
};

/*
 * Code file
 */
typedef struct {
    char *package;
    func_vec_t funcs;
    coroutine_vec_t coroutines;
    import_vec_t imports;
} code_file_t;

#define COMPILER_ERROR(err)    do {                             \
        fprintf(stderr, "Fatal error on compiling the code\n"); \
        exit(err);                                              \
    } while ( 0 )

#ifdef __cplusplus
extern "C" {
#endif

    literal_t * literal_new_int(int);
    literal_t * literal_new_float(float);
    literal_t * literal_new_string(const char *);
    type_t * type_new_primitive(type_type_t);
    type_t * type_new_id(const char *);
    var_t * var_new_id(char *, int);
    var_t * var_new_decl(decl_t *);
    val_t * val_new_literal(literal_t *);
    val_t * val_new_variable(var_t *);
    decl_t * decl_new(const char *, type_t *);
    arg_t * arg_new(decl_t *);
    arg_t * arg_prepend(arg_t *, arg_t *);
    func_t * func_new(char *, arg_t *, arg_t *, stmt_list_t *);
    coroutine_t * coroutine_new(char *, arg_t *, arg_t *, stmt_list_t *);
    stmt_t * stmt_new_decl(decl_t *);
    stmt_t * stmt_new_assign(var_t *, expr_t *);
    stmt_t * stmt_new_expr(expr_t *);
    stmt_list_t * stmt_list_new(stmt_t *);
    stmt_list_t * stmt_prepend(stmt_t *, stmt_list_t *);
    op_t * op_new_infix(expr_t *, expr_t *, op_type_t);
    op_t * op_new_prefix(expr_t *, op_type_t);
    expr_t * expr_new_val(val_t *);
    expr_t * expr_op_new_infix(expr_t *, expr_t *, op_type_t);
    expr_t * expr_op_new_prefix(expr_t *, op_type_t);
    int func_vec_add(func_vec_t *, func_t *);
    int coroutine_vec_add(coroutine_vec_t *, coroutine_t *);
    int import_vec_add(import_vec_t *, import_t *);
    import_t * import_new(char *);
    void * include_new(char *);
    int package_define(code_file_t *, const char *);
    int code_file_init(code_file_t *);

#ifdef __cplusplus
}
#endif

#endif /* _SYNTAX_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
