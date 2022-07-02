/*_
 * Copyright (c) 2019,2021-2022 Hirochika Asai <asai@jar.jp>
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
typedef struct _decl decl_t;
typedef struct _var var_t;
typedef struct _expr expr_t;
typedef struct _expr_list expr_list_t;
typedef struct _stmt stmt_t;
typedef struct _stmt_list stmt_list_t;
typedef struct _module module_t;
typedef struct _outer_block outer_block_t;
typedef struct _outer_block_entry outer_block_entry_t;
typedef struct _inner_block inner_block_t;

/*
 * Position
 */
typedef struct {
    off_t first_line;
    off_t first_column;
    off_t last_line;
    off_t last_column;
} pos_t;

/*
 * Literal types
 */
typedef enum {
    LIT_HEXINT,
    LIT_DECINT,
    LIT_OCTINT,
    LIT_FLOAT,
    LIT_STRING,
    LIT_BOOL,
    LIT_NIL,
} literal_type_t;

/*
 * Boolean
 */
typedef enum {
    BOOL_FALSE,
    BOOL_TRUE,
} bool_t;

/*
 * Literals
 */
typedef struct _literal literal_t;
struct _literal {
    literal_type_t type;
    union {
        char *n;
        char *s;
        bool_t b;
    } u;
    pos_t pos;
    literal_t *next;
};

/*
 * Literal sets
 */
typedef struct {
    literal_t *head;
    literal_t *tail;
} literal_set_t;

/*
 * Type type
 */
typedef enum {
    TYPE_PRIMITIVE_I8,
    TYPE_PRIMITIVE_U8,
    TYPE_PRIMITIVE_I16,
    TYPE_PRIMITIVE_U16,
    TYPE_PRIMITIVE_I32,
    TYPE_PRIMITIVE_U32,
    TYPE_PRIMITIVE_I64,
    TYPE_PRIMITIVE_U64,
    TYPE_PRIMITIVE_FP32,
    TYPE_PRIMITIVE_FP64,
    TYPE_PRIMITIVE_STRING,
    TYPE_PRIMITIVE_BOOL,
    TYPE_STRUCT,
    TYPE_UNION,
    TYPE_ENUM,
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
 * Type definition
 */
typedef struct {
    type_t type;
    char *id;
} type_def_t;

/*
 * Type table
 */
typedef struct {
} type_def_table_t;

/*
 * Declarations
 */
struct _decl {
    char *id;
    type_t *type;
    decl_t *next;
};

/*
 * Declaration list
 */
typedef struct {
    decl_t *head;
    decl_t *tail;
} decl_list_t;

/*
 * Function call
 */
typedef struct {
    expr_t *callee;
    expr_list_t *exprs;
} call_t;

/*
 * Array subscription
 */
typedef struct {
    expr_t *var;
    expr_t *arg;
} ref_t;

/*
 * Pointer type
 */
typedef enum {
    PTR_INDIRECTION,
    PTR_REFERENCE,
} ptr_type_t;

/*
 * Pointer indirection, reference
 */
typedef struct {
    ptr_type_t type;
    expr_t *e;
} ptr_t;

/*
 * Arguments
 */
typedef struct _arg arg_t;
struct _arg {
    decl_t *decl;
    arg_t *next;
};

/*
 * Argument list
 */
typedef struct {
    arg_t *head;
    arg_t *tail;
} arg_list_t;

/*
 * Struct data structure
 */
typedef struct {
    char *id;
    decl_list_t *list;
} struct_t;

/*
 * Union data structure
 */
typedef struct {
    char *id;
    decl_list_t *list;
} union_t;

/*
 * Enumerates
 */
typedef struct _enum_elem enum_elem_t;
struct _enum_elem {
    char *id;
    enum_elem_t *next;
};

/*
 * Enumerate
 */
typedef struct {
    char *id;
    enum_elem_t *list;
} enum_t;

/*
 * Typedef
 */
typedef struct {
    type_t *src;
    char *dst;
} typedef_t;

/*
 * Function
 */
typedef struct {
    char *id;
    arg_list_t *args;
    arg_list_t *rets;
    inner_block_t *block;
} func_t;

/*
 * Coroutine
 */
typedef struct {
    char *id;
    arg_list_t *args;
    arg_list_t *rets;
    inner_block_t *block;
} coroutine_t;

/*
 * Operations
 */
typedef enum {
    OP_ASSIGN,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_MOD,
    OP_NOT,
    OP_LAND,
    OP_LOR,
    OP_AND,
    OP_OR,
    OP_XOR,
    OP_COMP,
    OP_LSHIFT,
    OP_RSHIFT,
    OP_CMP_EQ,
    OP_CMP_NEQ,
    OP_CMP_GT,
    OP_CMP_LT,
    OP_CMP_GEQ,
    OP_CMP_LEQ,
    OP_INC,
    OP_DEC,
    OP_PTRREF,
    OP_PTRIND
} op_type_t;

/*
 * Type of fixes
 */
typedef enum {
    FIX_INFIX,
    FIX_PREFIX,
    FIX_SUFFIX,
} fix_t;

/*
 * Expression type
 */
typedef enum {
    EXPR_ID,
    EXPR_DECL,
    EXPR_LITERAL,
    EXPR_OP,
    EXPR_SWITCH,
    EXPR_IF,
    EXPR_CALL,
    EXPR_REF,
    EXPR_MEMBER,
    EXPR_LIST,
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
 * Case
 */
typedef struct _switch_case switch_case_t;
struct _switch_case {
    literal_set_t *lset;
    inner_block_t *block;
    switch_case_t *next;
};

/*
 * Switch block
 */
typedef struct {
    switch_case_t *head;
    switch_case_t *tail;
} switch_block_t;

/*
 * Switch expression
 */
typedef struct {
    expr_t *cond;
    switch_block_t *block;
} switch_t;

/*
 * If expression
 */
typedef struct {
    expr_t *cond;
    inner_block_t *bif;
    inner_block_t *belse;
} if_t;

/*
 * Member reference
 */
typedef struct {
    expr_t *e;
    char *id;
} member_t;

/*
 * Expression
 */
struct _expr {
    expr_type_t type;
    union {
        char *id;
        decl_t *decl;
        literal_t *lit;
        op_t *op;
        switch_t sw;
        if_t ife;
        member_t mem;
        call_t *call;
        ref_t *ref;
        expr_list_t *list;
    } u;
    expr_t *next;
    pos_t pos;
};

/*
 * Expression list
 */
struct _expr_list {
    expr_t *head;
    expr_t *tail;
};

/*
 * Statement type
 */
typedef enum {
    STMT_WHILE,
    STMT_EXPR,
    STMT_EXPR_LIST,
    STMT_BLOCK,
    STMT_RETURN,
} stmt_type_t;

/*
 * While statement
 */
typedef struct {
    expr_t *cond;
    inner_block_t *block;
} stmt_while_t;

/*
 * Statement
 */
struct _stmt {
    stmt_type_t type;
    union {
        stmt_while_t whilestmt;
        expr_t *expr;
        expr_list_t *exprs;
        inner_block_t *block;
        expr_t *ret;
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
 * Use
 */
typedef struct {
    char *id;
} use_t;

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
 * Directive type
 */
typedef enum {
    DIRECTIVE_USE,
    DIRECTIVE_STRUCT,
    DIRECTIVE_UNION,
    DIRECTIVE_ENUM,
    DIRECTIVE_TYPEDEF,
} directive_type_t;

/*
 * Directive
 */
typedef struct {
    directive_type_t type;
    union {
        use_t use;
        struct_t st;
        union_t un;
        enum_t en;
        typedef_t td;
    } u;
    pos_t pos;
} directive_t;

/*
 * Modules
 */
typedef struct {
    size_t n;
    size_t size;
    module_t **vec;
} module_vec_t;

/*
 * Inner block
 */
struct _inner_block {
    stmt_list_t *stmts;  /* statements */
    inner_block_t *next;
};

/*
 * Outer block entry type
 */
typedef enum {
    OUTER_BLOCK_FUNC,
    OUTER_BLOCK_COROUTINE,
    OUTER_BLOCK_MODULE,
    OUTER_BLOCK_DIRECTIVE,
} outer_block_entry_type_t;

/*
 * Module
 */
struct _module {
    char *id;
    outer_block_t *block;
    module_t *parent;           /* Stack */
};

/*
 * Outer block entry
 */
struct _outer_block_entry {
    outer_block_entry_type_t type;
    union {
        func_t *fn;
        coroutine_t *cr;
        module_t *md;
        directive_t *dr;
    } u;
    outer_block_entry_t *next;
};

/*
 * Outer block
 */
struct _outer_block {
    outer_block_entry_t *head;
    outer_block_entry_t *tail;
};

/*
 * Syntax tree
 */
typedef struct {
    outer_block_t *block;
    void *data;
} st_t;

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
 * String
 */
typedef struct {
    size_t len;
    size_t size;
    char *buf;
} string_t;

/*
 * Symbol
 */
typedef enum {
    CTX_SYMBOL_FUNC,
    CTX_SYMBOL_COROUTINE,
    CTX_SYMBOL_VAR,
} context_symbol_type_t;
typedef struct {
    context_symbol_type_t type;
    union {
        void *func;
        void *cr;
        void *var;
    } u;
} context_symbol_t;

/*
 * Symbols
 */
typedef struct {
    off_t i;
    size_t n;
    context_symbol_t *symbols;
} context_symbol_table_t;

/*
 * Compiler context for lexer and parser
 */
typedef struct {
    /* Lexer string buffer */
    string_t buffer;
    /* Parser's context */
    st_t *st;
    module_t *cur;
} context_t;

#define COMPILER_ERROR(err)    do {                             \
        fprintf(stderr, "Fatal error on compiling the code\n"); \
        exit(err);                                              \
    } while ( 0 )

#ifdef __cplusplus
extern "C" {
#endif

    literal_t * literal_new_int(void *, const char *, int);
    literal_t * literal_new_float(void *, const char *);
    literal_t * literal_new_string(void *, const char *);
    literal_t * literal_new_bool(void *, bool_t);
    literal_t * literal_new_nil(void *);
    literal_set_t * literal_set_new(void);
    literal_set_t * literal_set_add(literal_set_t *, literal_t *);
    type_t * type_new_primitive(type_type_t);
    type_t * type_new_struct(const char *);
    type_t * type_new_union(const char *);
    type_t * type_new_enum(const char *);
    type_t * type_new_id(const char *);
    decl_t * decl_new(const char *, type_t *);
    decl_list_t * decl_list_new(decl_t *);
    decl_list_t * decl_list_append(decl_list_t *, decl_t *);
    arg_t * arg_new(decl_t *);
    arg_list_t * arg_list_new(arg_t *);
    arg_list_t * arg_list_append(arg_list_t *, arg_t *);
    directive_t * directive_struct_new(void *, const char *, decl_list_t *);
    directive_t * directive_union_new(void *, const char *, decl_list_t *);
    directive_t * directive_enum_new(void *, const char *, enum_elem_t *);
    directive_t * directive_typedef_new(void *, type_t *, const char *);
    directive_t * directive_use_new(void *, const char *);
    enum_elem_t * enum_elem_new(const char *);
    enum_elem_t * enum_elem_prepend(enum_elem_t *, enum_elem_t *);
    func_t *
    func_new(const char *, arg_list_t *, arg_list_t *, inner_block_t *);
    coroutine_t *
    coroutine_new(const char *, arg_list_t *, arg_list_t *, inner_block_t *);
    module_t * module_new(const char *, outer_block_t *);
    outer_block_entry_t * outer_block_entry_new(outer_block_entry_type_t);
    outer_block_t * outer_block_new(outer_block_entry_t *);
    inner_block_t * inner_block_new(stmt_list_t *);
    stmt_t * stmt_new_while(expr_t *, inner_block_t *);
    stmt_t * stmt_new_expr(expr_t *);
    stmt_t * stmt_new_expr_list(expr_list_t *);
    stmt_t * stmt_new_return(expr_t *);
    stmt_t * stmt_new_block(inner_block_t *);
    stmt_list_t * stmt_list_new(stmt_t *);
    stmt_list_t * stmt_list_append(stmt_list_t *, stmt_t *);
    op_t * op_new_infix(expr_t *, expr_t *, op_type_t);
    op_t * op_new_prefix(expr_t *, op_type_t);
    op_t * op_new_suffix(expr_t *, op_type_t);
    expr_t * expr_new_id(void *, const char *);
    expr_t * expr_new_decl(void *, decl_t *);
    expr_t * expr_new_literal(void *, literal_t *);
    expr_t * expr_op_new_infix(void *, expr_t *, expr_t *, op_type_t);
    expr_t * expr_op_new_prefix(void *, expr_t *, op_type_t);
    expr_t * expr_op_new_suffix(void *, expr_t *, op_type_t);
    expr_t * expr_new_member(void *, expr_t *, const char *);
    expr_t * expr_new_call(void *, expr_t *, expr_list_t *);
    expr_t * expr_new_ref(void *, expr_t *, expr_t *);
    expr_t * expr_new_switch(void *, expr_t *, switch_block_t *);
    expr_t * expr_new_if(void *, expr_t *, inner_block_t *, inner_block_t *);
    expr_t * expr_new_list(expr_list_t *);
    expr_list_t * expr_list_new(void);
    expr_list_t * expr_list_append(expr_list_t *, expr_t *);
    switch_case_t * switch_case_new(literal_set_t *, inner_block_t *);
    switch_block_t * switch_block_new(void);
    switch_block_t * switch_block_append(switch_block_t *, switch_case_t *);
    st_t * st_new(outer_block_t *);

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
