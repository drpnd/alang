/*_
 * Copyright (c) 2019-2021 Hirochika Asai <asai@jar.jp>
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
#include "compile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COMPILE_ERROR_RETURN(c, msg)        \
    do {                                    \
        fprintf(stderr, "Parse error: ");   \
        fprintf(stderr, msg);               \
        fprintf(stderr, "\n");              \
        return -1;                          \
    } while ( 0 )

/* Declarations */
static compiler_val_t * _expr(compiler_t *, compiler_env_t *, expr_t *);
static compiler_val_t *
_expr_list(compiler_t *, compiler_env_t *, expr_list_t *);
static int _inner_block(compiler_t *, compiler_env_t *, inner_block_t *);

/*
 * _instr_new -- allocate a new instruction
 */
static compiler_instr_t *
_instr_new(void)
{
    compiler_instr_t *instr;

    instr = malloc(sizeof(compiler_instr_t));
    if ( NULL == instr ) {
        return NULL;
    }
    memset(instr, 0, sizeof(compiler_instr_t));

    return instr;
}

/*
 * _env_new -- allocate a new environment
 */
static compiler_env_t *
_env_new(compiler_t *c)
{
    compiler_env_t *env;

    env = malloc(sizeof(compiler_env_t));
    if ( NULL == env ) {
        return NULL;
    }
    env->vars = malloc(sizeof(compiler_var_table_t));
    if ( NULL == env->vars ) {
        free(env);
        return NULL;
    }
    env->vars->top = NULL;
    env->prev = NULL;

    env->code.head = NULL;
    env->code.tail = NULL;

    return env;
}

/*
 * _var_new -- allocate a new variable
 */
static compiler_var_t *
_var_new(const char *id, type_t *type)
{
    compiler_var_t *var;

    var = malloc(sizeof(compiler_var_t));
    if ( NULL == var ) {
        return NULL;
    }
    var->id = strdup(id);
    if ( NULL == var->id ) {
        free(var);
        return NULL;
    }
    var->type = type;

    return var;
}

/*
 * _var_delete -- deallocate the specified variable
 */
static void
_var_delete(compiler_var_t *var)
{
    free(var->id);
    free(var);
}

/*
 * _var_add -- add a variable to the stack
 */
static int
_var_add(compiler_env_t *env, compiler_var_t *var)
{
    var->next = env->vars->top;
    env->vars->top = var;

    return 0;
}

/*
 * _var_search -- search a variable from the stack
 */
static compiler_var_t *
_var_search(compiler_env_t *env, const char *id)
{
    compiler_var_t *var;

    var = env->vars->top;
    while ( NULL != var ) {
        if ( 0 == strcmp(id, var->id) ) {
            /* Found a variable with the specified identifier */
            return var;
        }
        var = var->next;
    }

    /* Not found */
    return NULL;
}

/*
 * _val_new -- allocate a new value
 */
static compiler_val_t *
_val_new(void)
{
    compiler_val_t *val;

    val = malloc(sizeof(compiler_val_t));
    if ( NULL == val ) {
        return NULL;
    }
    memset(val, 0, sizeof(compiler_val_t));

    return val;
}

/*
 * _val_list_new -- allocate a new value list
 */
static compiler_val_list_t *
_val_list_new(compiler_val_t *val)
{
    compiler_val_list_t *l;

    l = malloc(sizeof(compiler_val_list_t));
    if ( NULL == l ) {
        return NULL;
    }
    l->head = val;
    l->tail = val;

    return l;
}

/*
 * _val_list_append -- append a value to the list
 */
static compiler_val_list_t *
_val_list_append(compiler_val_list_t *l, compiler_val_t *val)
{
    if ( NULL == l->head ) {
        l->head = val;
        l->tail = val;
    } else {
        l->tail->next = val;
        l->tail = val;
    }

    return l;
}

/*
 * _append_instr
 */
static int
_append_instr(compiler_code_t *code, compiler_instr_t *instr)
{
    if ( NULL == code->head ) {
        code->head = instr;
        code->tail = instr;
    } else {
        code->tail->next = instr;
        code->tail = instr;
    }

    return 0;
}

/*
 * _id -- parse an identifier
 */
static compiler_val_t *
_id(compiler_t *c, compiler_env_t *env, const char *id)
{
    compiler_val_t *val;
    compiler_var_t *var;

    var = _var_search(env, id);
    if ( NULL == var ) {
        return NULL;
    }

    val = _val_new();
    if ( NULL == val ) {
        return NULL;
    }
    val->type = VAL_VAR;
    val->u.var = var;

    return val;
}

/*
 * _literal -- parse a literal
 */
static compiler_val_t *
_literal(compiler_t *c, compiler_env_t *env, literal_t *lit)
{
    compiler_val_t *val;

    val = _val_new();
    if ( NULL == val ) {
        return NULL;
    }
    val->type = VAL_LITERAL;
    val->u.lit = lit;

    return val;
}

/*
 * _decl -- parse a declaration
 */
static compiler_val_t *
_decl(compiler_t *c, compiler_env_t *env, decl_t *decl)
{
    compiler_val_t *val;
    compiler_var_t *var;
    int ret;

    /* Allocate a new variable */
    var = _var_new(decl->id, decl->type);
    if ( NULL == var ) {
        return NULL;
    }

    /* Allocate a new value */
    val = _val_new();
    if ( NULL == val ) {
        _var_delete(var);
        return NULL;
    }
    val->type = VAL_VAR;
    val->u.var = var;

    /* Add the variable to the table */
    ret = _var_add(env, var);
    if ( ret < 0 ) {
        _var_delete(var);
        free(val);
        return NULL;
    }

    return val;
}

/*
 * _args -- parse arguments
 */
static int
_args(compiler_t *c, compiler_env_t *env, arg_list_t *args)
{
    arg_t *a;
    compiler_val_t *val;

    a = args->head;
    while ( NULL != a ) {
        val = _decl(c, env, a->decl);
        if ( NULL == val ) {
            return -1;
        }
        a = a->next;
    }

    return 0;
}

/*
 * _assign -- parse an assignment instruction
 */
static compiler_val_t *
_assign(compiler_t *c, compiler_env_t *env, op_t *op)
{
    compiler_val_t *v0;
    compiler_val_t *v1;
    compiler_instr_t *instr;
    int ret;

    if ( FIX_INFIX != op->fix ) {
        return NULL;
    }

    /* Evaluate the expressions */
    v0 = _expr(c, env, op->e0);
    v1 = _expr(c, env, op->e1);
    if ( VAL_VAR != v0->type ) {
        return NULL;
    }

    /* Assign */
    instr = _instr_new();
    if ( NULL == instr ) {
        return NULL;
    }
    instr->opcode = OPCODE_MOV;
    instr->operands[0].type = OPERAND_VAL;
    instr->operands[0].u.val = v1;
    instr->operands[1].type = OPERAND_VAL;
    instr->operands[1].u.val = v0;
    ret = _append_instr(&env->code, instr);
    if ( ret < 0 ) {
        return NULL;
    }

    return v0;
}

/*
 * _op_infix -- parse an infix operation
 */
static compiler_val_t *
_op_infix(compiler_t *c, compiler_env_t *env, op_t *op, opcode_t opcode)
{
    compiler_val_t *vr;
    compiler_val_t *v0;
    compiler_val_t *v1;
    compiler_instr_t *instr;
    int ret;

    if ( FIX_INFIX != op->fix ) {
        return NULL;
    }

    /* Evaluate the expressions */
    v0 = _expr(c, env, op->e0);
    v1 = _expr(c, env, op->e1);

    /* Allocate a new value */
    vr = _val_new();
    if ( NULL == vr ) {
        return NULL;
    }
    vr->type = VAL_REG;

    /* Add an instruction */
    instr = _instr_new();
    if ( NULL == instr ) {
        return NULL;
    }
    instr->opcode = opcode;
    instr->operands[0].type = OPERAND_VAL;
    instr->operands[0].u.val = v0;
    instr->operands[1].type = OPERAND_VAL;
    instr->operands[1].u.val = v1;
    instr->operands[2].type = OPERAND_VAL;
    instr->operands[2].u.val = vr;
    ret = _append_instr(&env->code, instr);
    if ( ret < 0 ) {
        return NULL;
    }

    return vr;
}

/*
 * _op_prefix -- parse an prefix operation
 */
static compiler_val_t *
_op_prefix(compiler_t *c, compiler_env_t *env, op_t *op, opcode_t opcode)
{
    compiler_val_t *vr;
    compiler_val_t *v;
    compiler_instr_t *instr;
    int ret;

    if ( FIX_PREFIX != op->fix ) {
        return NULL;
    }

    /* Evaluate the expressions */
    v = _expr(c, env, op->e0);

    /* Allocate a new value */
    vr = _val_new();
    if ( NULL == vr ) {
        return NULL;
    }
    vr->type = VAL_REG;

    /* Add an instruction */
    instr = _instr_new();
    if ( NULL == instr ) {
        return NULL;
    }
    instr->opcode = opcode;
    instr->operands[0].type = OPERAND_VAL;
    instr->operands[0].u.val = v;
    instr->operands[1].type = OPERAND_VAL;
    instr->operands[1].u.val = vr;
    ret = _append_instr(&env->code, instr);
    if ( ret < 0 ) {
        return NULL;
    }

    return vr;
}

/*
 * _divmod -- paser a divide/modulo operation
 */
static compiler_val_t *
_divmod(compiler_t *c, compiler_env_t *env, op_t *op, opcode_t opcode)
{
    compiler_val_t *vr;
    compiler_val_t *v0;
    compiler_val_t *v1;
    compiler_instr_t *instr;
    int ret;

    if ( FIX_INFIX != op->fix ) {
        return NULL;
    }

    /* Evaluate the expressions */
    v0 = _expr(c, env, op->e0);
    v1 = _expr(c, env, op->e1);

    /* Allocate a new value */
    vr = _val_new();
    if ( NULL == vr ) {
        return NULL;
    }
    vr->type = VAL_REG_SET;

    instr = _instr_new();
    if ( NULL == instr ) {
        return NULL;
    }
    instr->opcode = opcode;
    instr->operands[0].type = OPERAND_VAL;
    instr->operands[0].u.val = v0;
    instr->operands[1].type = OPERAND_VAL;
    instr->operands[1].u.val = v1;
    instr->operands[2].type = OPERAND_VAL;
    instr->operands[2].u.val = vr;
    ret = _append_instr(&env->code, instr);
    if ( ret < 0 ) {
        return NULL;
    }

    return vr;
}

/*
 * _incdec -- parse an increment/decrement instruction
 */
static compiler_val_t *
_incdec(compiler_t *c, compiler_env_t *env, op_t *op, opcode_t opcode)
{
    compiler_instr_t *instr;
    compiler_val_t *val;
    compiler_val_t *vr;
    int ret;

    val = _expr(c, env, op->e0);
    if ( VAL_VAR != val->type ) {
        /* Only variable is allowed for this operation. */
        return NULL;
    }

    if ( FIX_PREFIX == op->fix ) {
        instr = _instr_new();
        if ( NULL == instr ) {
            return NULL;
        }
        instr->opcode = opcode;
        instr->operands[0].type = OPERAND_VAL;
        instr->operands[0].u.val = val;
        ret = _append_instr(&env->code, instr);
        if ( ret < 0 ) {
            return NULL;
        }

        return val;
    } else if ( FIX_SUFFIX == op->fix ) {
        vr = _val_new();
        if ( NULL == vr ) {
            return NULL;
        }
        vr->type = VAL_REG;

        /* Copy the value to a register */
        instr = _instr_new();
        if ( NULL == instr ) {
            return NULL;
        }
        instr->opcode = OPCODE_MOV;
        instr->operands[0].type = OPERAND_VAL;
        instr->operands[0].u.val = val;
        instr->operands[1].type = OPERAND_VAL;
        instr->operands[1].u.val = vr;
        ret = _append_instr(&env->code, instr);
        if ( ret < 0 ) {
            return NULL;
        }

        instr = _instr_new();
        if ( NULL == instr ) {
            return NULL;
        }
        instr->opcode = opcode;
        instr->operands[0].type = OPERAND_VAL;
        instr->operands[0].u.val = val;
        ret = _append_instr(&env->code, instr);
        if ( ret < 0 ) {
            return NULL;
        }

        return val;
    } else {
        return NULL;
    }
}

/*
 * _op -- parse an operator
 */
static compiler_val_t *
_op(compiler_t *c, compiler_env_t *env, op_t *op)
{
    compiler_val_t *val;

    val = NULL;
    switch ( op->type ) {
    case OP_ASSIGN:
        val = _assign(c, env, op);
        break;
    case OP_ADD:
        val = _op_infix(c, env, op, OPCODE_ADD);
        break;
    case OP_SUB:
        val = _op_infix(c, env, op, OPCODE_SUB);
        break;
    case OP_MUL:
        val = _op_infix(c, env, op, OPCODE_MUL);
        break;
    case OP_DIV:
        val = _divmod(c, env, op, OPCODE_DIV);
        break;
    case OP_MOD:
        val = _divmod(c, env, op, OPCODE_MOD);
        break;
    case OP_NOT:
        val = _op_prefix(c, env, op, OPCODE_NOT);
        break;
    case OP_LAND:
        val = _op_infix(c, env, op, OPCODE_LAND);
        break;
    case OP_LOR:
        val = _op_infix(c, env, op, OPCODE_LOR);
        break;
    case OP_AND:
        val = _op_infix(c, env, op, OPCODE_AND);
        break;
    case OP_OR:
        val = _op_infix(c, env, op, OPCODE_OR);
        break;
    case OP_XOR:
        val = _op_infix(c, env, op, OPCODE_XOR);
        break;
    case OP_COMP:
        val = _op_prefix(c, env, op, OPCODE_COMP);
        break;
    case OP_LSHIFT:
        val = _op_infix(c, env, op, OPCODE_LSHIFT);
        break;
    case OP_RSHIFT:
        val = _op_infix(c, env, op, OPCODE_RSHIFT);
        break;
    case OP_CMP_EQ:
        val = _op_infix(c, env, op, OPCODE_CMP_EQ);
        break;
    case OP_CMP_NEQ:
        val = _op_infix(c, env, op, OPCODE_CMP_NEQ);
        break;
    case OP_CMP_GT:
        val = _op_infix(c, env, op, OPCODE_CMP_GT);
        break;
    case OP_CMP_LT:
        val = _op_infix(c, env, op, OPCODE_CMP_LT);
        break;
    case OP_CMP_GEQ:
        val = _op_infix(c, env, op, OPCODE_CMP_GEQ);
        break;
    case OP_CMP_LEQ:
        val = _op_infix(c, env, op, OPCODE_CMP_LEQ);
        break;
    case OP_INC:
        val = _incdec(c, env, op, OPCODE_INC);
        break;
    case OP_DEC:
        val = _incdec(c, env, op, OPCODE_DEC);
        break;
    }

    return val;
}

/*
 * _expr -- parse an expression
 */
static compiler_val_t *
_expr(compiler_t *c, compiler_env_t *env, expr_t *e)
{
    compiler_val_t *val;

    val = NULL;
    switch ( e->type ) {
    case EXPR_ID:
        val = _id(c, env, e->u.id);
        break;
    case EXPR_DECL:
        val = _decl(c, env, e->u.decl);
        break;
    case EXPR_LITERAL:
        val = _literal(c, env, e->u.lit);
        break;
    case EXPR_OP:
        val = _op(c, env, e->u.op);
        break;
    case EXPR_SWITCH:
        //printf("SWITCH\n");
        break;
    case EXPR_IF:
        //printf("IF\n");
        break;
    case EXPR_CALL:
        //printf("CALL\n");
        break;
    case EXPR_REF:
        //printf("REF\n");
        break;
    case EXPR_MEMBER:
        //printf("MEMBER\n");
        break;
    case EXPR_LIST:
        val = _expr_list(c, env, e->u.list);
        break;
    }

    return val;
}

/*
 * _expr_list -- parse an expression list
 */
static compiler_val_t *
_expr_list(compiler_t *c, compiler_env_t *env, expr_list_t *exprs)
{
    expr_t *e;
    compiler_val_t *val;
    compiler_val_t *v;
    compiler_val_list_t *l;

    val = _val_new();
    if ( NULL == val ) {
        return NULL;
    }
    l = _val_list_new(NULL);
    if ( NULL == l ) {
        free(val);
        return NULL;
    }

    e = exprs->head;
    while ( NULL != e ) {
        v = _expr(c, env, e);
        if ( NULL == v ) {
            return NULL;
        }
        l = _val_list_append(l, v);
        if ( NULL == l ) {
            return NULL;
        }
        e = e->next;
    }

    v->type = VAL_LIST;
    v->u.list = l;

    return v;
}

/*
 * _while -- parse a while statement
 */
static int
_while(compiler_t *c, compiler_env_t *env, stmt_while_t *w)
{
    return -1;
}

/*
 * _return -- parse a return statement
 */
static int
_return(compiler_t *c, compiler_env_t *env, expr_t *e)
{
    return -1;
}

/*
 * _stmt -- parse a statement
 */
static int
_stmt(compiler_t *c, compiler_env_t *env, stmt_t *stmt)
{
    int ret;
    compiler_env_t *nenv;
    compiler_val_t *val;

    ret = -1;
    switch ( stmt->type ) {
    case STMT_WHILE:
        ret = _while(c, env, &stmt->u.whilestmt);
        break;
    case STMT_EXPR:
        val = _expr(c, env, stmt->u.expr);
        ret = 0;
        break;
    case STMT_EXPR_LIST:
        val = _expr_list(c, env, stmt->u.exprs);
        ret = 0;
        break;
    case STMT_BLOCK:
        /* Create a new environemt */
        nenv = _env_new(c);
        if ( NULL == nenv ) {
            return -1;
        }
        nenv->prev = env;
        ret = _inner_block(c, nenv, stmt->u.block);
        break;
    case STMT_RETURN:
        ret = _return(c, env, stmt->u.expr);
        break;
    }

    return ret;
}

/*
 * _inner_block -- parse an inner block
 */
static int
_inner_block(compiler_t *c, compiler_env_t *env, inner_block_t *block)
{
    stmt_t *stmt;
    int ret;

    stmt = block->stmts->head;
    while ( NULL != stmt ) {
        ret = _stmt(c, env, stmt);
        if ( ret < 0 ) {
            return -1;
        }
        stmt = stmt->next;
    }

    return 0;
}

/*
 * _func -- parse a function
 */
static int
_func(compiler_t *c, func_t *fn)
{
    int ret;
    compiler_env_t *env;

    /* Allocate a new environment */
    env = _env_new(c);
    if ( NULL == env ) {
        COMPILE_ERROR_RETURN(c, "memory");
        return -1;
    }

    /* Parse arguments and return values */
    ret = _args(c, env, fn->args);
    if ( ret < 0 ) {
        return -1;
    }
    ret = _args(c, env, fn->rets);
    if ( ret < 0 ) {
        return -1;
    }

    /* Parse the inner block */
    ret = _inner_block(c, env, fn->block);
    if ( ret < 0 ) {
        return -1;
    }

    return 0;
}

/*
 * _coroutine -- parse a coroutine
 */
static int
_coroutine(compiler_t *c, coroutine_t *cr)
{
    return -1;
}

/*
 * _module -- parse a module
 */
static int
_module(compiler_t *c, module_t *md)
{
    return -1;
}

/*
 * _directive -- parse a directive
 */
static int
_directive(compiler_t *c, directive_t *dr)
{
    switch ( dr->type ) {
    case DIRECTIVE_USE:
        break;
    case DIRECTIVE_STRUCT:
        break;
    case DIRECTIVE_UNION:
        break;
    case DIRECTIVE_ENUM:
        break;
    case DIRECTIVE_TYPEDEF:
        break;
    }
    COMPILE_ERROR_RETURN(c, "invalid directive");
}

/*
 * _outer_block_entry -- parse an outer block entry
 */
static int
_outer_block_entry(compiler_t *c, outer_block_entry_t *e)
{
    int ret;

    ret = -1;
    switch ( e->type ) {
    case OUTER_BLOCK_FUNC:
        ret = _func(c, e->u.fn);
        break;
    case OUTER_BLOCK_COROUTINE:
        ret = _coroutine(c, e->u.cr);
        break;
    case OUTER_BLOCK_MODULE:
        ret = _module(c, e->u.md);
        break;
    case OUTER_BLOCK_DIRECTIVE:
        ret = _directive(c, e->u.dr);
        break;
    }

    return ret;
}

/*
 * _outer_block -- compile an outer block
 */
static int
_outer_block(compiler_t *c, outer_block_t *block)
{
    compiler_env_t *env;
    outer_block_entry_t *e;
    int ret;

    /* Allocate the environment data structure for this block (i.e., scope) */
    env = malloc(sizeof(compiler_env_t));
    if ( NULL == env ) {
        COMPILE_ERROR_RETURN(c, "memory");
    }
    memset(env, 0, sizeof(compiler_env_t));
    env->vars = NULL;

    /* Parse all outer block entries */
    e = block->head;
    while ( NULL != e ) {
        ret = _outer_block_entry(c, e);
        if ( ret < 0 ) {
            return -1;
        }
        e = e->next;
    }

    return 0;
}

/*
 * compile_code -- compile code
 */
int
compile_code(compiler_t *c, code_file_t *code)
{
    return _outer_block(c, code->block);
}

/*
 * compile -- compiile code
 */
int
compile(code_file_t *code)
{
    compiler_t *c;

    /* Allocate compiler */
    c = malloc(sizeof(compiler_t));
    if ( NULL == c ) {
        return EXIT_FAILURE;
    }
    c->fout = NULL;

    return compile_code(c, code);
}

/*
　* compile_use_extern -- use an external package
　*/
int
compile_use_extern(context_t *context, const char *id)
{
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
