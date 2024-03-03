/*_
 * Copyright (c) 2019-2024 Hirochika Asai <asai@jar.jp>
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
static compiler_error_t *
_error_new(compiler_error_code_t, pos_t);
static compiler_instr_t *
_instr_new(void);
static void
_instr_delete(compiler_instr_t *);
static compiler_var_table_t *
_var_table_initialize(compiler_var_table_t *);
static void
_var_table_release(compiler_var_table_t *);
static compiler_env_t *
_env_new(compiler_t *);
static void
_env_delete(compiler_env_t *);
static compiler_val_t *
_expr(compiler_t *, compiler_env_t *, expr_t *);
static compiler_val_t *
_expr_list(compiler_t *, compiler_env_t *, expr_list_t *);
static compiler_val_t *
_inner_block(compiler_t *, compiler_env_t *, inner_block_t *);
#if 0
static int
_add_code_symbol(compiler_t *, const char *, ir_instr_t *, size_t);
static int
_add_data_symbol(compiler_t *, const char *, uint8_t *, size_t);
#endif
static compiler_block_t *
_outer_block_entry(compiler_t *, outer_block_entry_t *);
static compiler_block_t *
_outer_block(compiler_t *, outer_block_t *);
static compiler_block_t *
_st(compiler_t *, st_t *);

/*
 * _error_new -- allocate a new error
 */
static compiler_error_t *
_error_new(compiler_error_code_t code, pos_t pos)
{
    compiler_error_t *err;

    err = malloc(sizeof(compiler_error_t));
    if ( err == NULL ) {
        return NULL;
    }
    memset(err, 0, sizeof(compiler_error_t));
    err->err = code;
    err->pos = pos;
    err->next = NULL;

    return err;
}

/*
 * _instr_new -- allocate a new instruction
 */
static compiler_instr_t *
_instr_new(void)
{
    compiler_instr_t *instr;

    instr = malloc(sizeof(compiler_instr_t));
    if ( instr == NULL ) {
        return NULL;
    }
    memset(instr, 0, sizeof(compiler_instr_t));

    return instr;
}

/*
 * _instr_delete -- delete an instruction
 */
static void
_instr_delete(compiler_instr_t *instr)
{
    free(instr);
}

/*
 * _var_table_init -- initialize a variable table
 */
static compiler_var_table_t *
_var_table_initialize(compiler_var_table_t *t)
{
    if ( t == NULL ) {
        t = malloc(sizeof(compiler_var_table_t));
        if ( t == NULL ) {
            return NULL;
        }
        t->_allocated = 1;
    } else {
        t->_allocated = 0;
    }
    t->top = NULL;

    return t;
}

/*
 * _var_table_release -- release a variable table
 */
static void
_var_table_release(compiler_var_table_t *t)
{
    if ( t->_allocated ) {
        free(t);
    }
}

/*
 * _symbol_add -- add a symbol
 */
static int
_symbol_add(compiler_t *c, compiler_symbol_t *s)
{
    size_t i;
    size_t n;
    compiler_symbol_t **symbols;

    i = c->symbols.n;
    n = i + 1;
    symbols = realloc(c->symbols.symbols, sizeof(compiler_symbol_t *) * n);
    if ( symbols == NULL ) {
        return -1;
    }
    symbols[i] = s;

    c->symbols.symbols = symbols;
    c->symbols.n = n;

    return 0;
}

/*
 * _env_new -- allocate a new environment
 */
static compiler_env_t *
_env_new(compiler_t *c)
{
    compiler_env_t *env;

    env = malloc(sizeof(compiler_env_t));
    if ( env == NULL ) {
        c->err.code = COMPILER_NOMEM;
        return NULL;
    }
    env->vars = _var_table_initialize(NULL);
    if ( env->vars == NULL ) {
        free(env);
        c->err.code = COMPILER_NOMEM;
        return NULL;
    }

    env->prev = NULL;
    env->retval = NULL;

    env->code.head = NULL;
    env->code.tail = NULL;

    env->opt.max_reg_id = -1;

    return env;
}

/*
 * _env_delete -- delete an environment
 */
static void
_env_delete(compiler_env_t *env)
{
    /* Delete the variables */
    _var_table_release(env->vars);
    free(env);
}

/*
 * _type2size -- resolve the size of the type
 */
static ssize_t
_type2size(compiler_t *c, type_t *type)
{
    ssize_t sz;

    sz = -1;
    switch (type->type) {
    case TYPE_PRIMITIVE_I8:
    case TYPE_PRIMITIVE_U8:
        sz = 8;
        break;
    case TYPE_PRIMITIVE_I16:
    case TYPE_PRIMITIVE_U16:
        sz = 16;
        break;
    case TYPE_PRIMITIVE_I32:
    case TYPE_PRIMITIVE_U32:
    case TYPE_PRIMITIVE_FP32:
        sz = 32;
        break;
    case TYPE_PRIMITIVE_I64:
    case TYPE_PRIMITIVE_U64:
    case TYPE_PRIMITIVE_FP64:
        sz = 64;
        break;
    }

    return sz;
}

/*
 * _type2reg -- resolve the corresponding register to the type
 */
static ir_reg_type_t
_type2reg(compiler_t *c, type_t *type)
{
    ir_reg_type_t rtype;

    rtype = IR_REG_UNDEF;
    switch ( type->type ) {
    case TYPE_PRIMITIVE_I8:
    case TYPE_PRIMITIVE_U8:
        rtype = IR_REG_I8;
        break;
    case TYPE_PRIMITIVE_I16:
    case TYPE_PRIMITIVE_U16:
        rtype = IR_REG_I16;
        break;
    case TYPE_PRIMITIVE_I32:
    case TYPE_PRIMITIVE_U32:
        rtype = IR_REG_I32;
        break;

    case TYPE_PRIMITIVE_I64:
    case TYPE_PRIMITIVE_U64:
        rtype = IR_REG_I64;
        break;
    case TYPE_PRIMITIVE_FP32:
        rtype = IR_REG_FP32;
        break;
    case TYPE_PRIMITIVE_FP64:
        rtype = IR_REG_FP64;
        break;
    case TYPE_PRIMITIVE_STRING:
        rtype = IR_REG_PTR;
        break;
    case TYPE_PRIMITIVE_BOOL:
        rtype = IR_REG_BOOL;
        break;
    case TYPE_ENUM:
        rtype = IR_REG_I64;
        break;
    }

    return rtype;
}

/*
 * _var_new -- allocate a new variable
 */
static compiler_var_t *
_var_new(compiler_t *c, const char *id, type_t *type)
{
    compiler_var_t *var;
    ssize_t sz;
    ir_reg_type_t rtype;

    /* Allocate a new variable */
    var = malloc(sizeof(compiler_var_t));
    if ( var == NULL ) {
        return NULL;
    }

    /* Resolve the register type from the specified type */
    rtype = _type2reg(c, type);
    if ( rtype == IR_REG_UNDEF ) {
        return NULL;
    }
    var->irreg.type = rtype;
    var->irreg.assigned = 1;
    var->irreg.id = strdup(id);

    var->type = type;
    var->arg = 0;
    var->ret = 0;
    var->next = NULL;

    return var;
}

/*
 * _var_delete -- deallocate the specified variable
 */
static void
_var_delete(compiler_var_t *var)
{
    free(var->irreg.id);
    free(var);
}

/*
 * _var_add -- add a variable to the stack
 */
static int
_var_add(compiler_env_t *env, compiler_var_t *var)
{
    compiler_var_t *v;

    /* Duplicate check */
    v = env->vars->top;
    while ( v != NULL ) {
        /* Duplication check */
        if ( strcmp(var->irreg.id, v->irreg.id) == 0 ) {
            return -1;
        }
        v = v->next;
    }

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
    while ( var != NULL ) {
        if ( strcmp(id, var->irreg.id) == 0 ) {
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
    if ( val == NULL ) {
        return NULL;
    }
    memset(val, 0, sizeof(compiler_val_t));
    val->type = VAL_NIL;

    return val;
}

/*
 * _val_new_nil -- allocate a new nil value
 */
static compiler_val_t *
_val_new_nil(void)
{
    compiler_val_t *val;

    val = _val_new();
    if ( val == NULL ) {
        return NULL;
    }
    val->type = VAL_NIL;

    return val;
}

/*
 * _val_new_reg -- allocate a new register value
 */
static compiler_val_t *
_val_new_reg(compiler_env_t *env)
{
    compiler_val_t *val;

    val = _val_new();
    if ( val == NULL ) {
        return NULL;
    }
    val->type = VAL_REG;

    return val;
}

/*
 * _val_new_reg_set -- allocate a new register set value
 */
static compiler_val_t *
_val_new_reg_set(compiler_env_t *env)
{
    compiler_val_t *val;

    val = _val_new();
    if ( val == NULL ) {
        return NULL;
    }
    val->type = VAL_REG_SET;

    return val;
}

/*
 * _val_new_var -- allocate a new variable value
 */
static compiler_val_t *
_val_new_var(compiler_env_t *env, compiler_var_t *var)
{
    compiler_val_t *val;

    val = _val_new();
    if ( val == NULL ) {
        return NULL;
    }
    val->type = VAL_VAR;
    val->u.var = var;

    return val;
}

/*
 * _val_new_literal -- allocate a new literal value
 */
static compiler_val_t *
_val_new_literal(literal_t *lit)
{
    compiler_val_t *val;

    val = _val_new();
    if ( val == NULL ) {
        return NULL;
    }
    val->type = VAL_LITERAL;
    val->u.lit = lit;

    return val;
}

/*
 * _val_new_cond -- allocate a new value condition set
 */
static compiler_val_t *
_val_new_cond(int n)
{
    compiler_val_t *val;
    compiler_val_cond_t *cset;

    cset = malloc(sizeof(compiler_val_cond_t));
    if ( cset == NULL ) {
        return NULL;
    }
    cset->n = n;
    cset->vals = malloc(sizeof(compiler_val_t *) * n);
    if ( cset->vals == NULL ) {
        free(cset);
        return NULL;
    }
    memset(cset->vals, 0, sizeof(compiler_val_t *) * n);

    val = _val_new();
    if ( val == NULL ) {
        free(cset->vals);
        free(cset);
        return NULL;
    }
    val->type = VAL_COND;
    val->u.conds = cset;

    return val;
}

/*
 * _val_delete -- delete a value
 */
static void
_val_delete(compiler_val_t *val)
{
    switch ( val->type ) {
    case VAL_NIL:
        break;
    case VAL_VAR:
        break;
    case VAL_LITERAL:
        break;
    }
    free(val);
}

/*
 * _val_list_new -- allocate a new value list
 */
static compiler_val_list_t *
_val_list_new(compiler_val_t *val)
{
    compiler_val_list_t *l;

    l = malloc(sizeof(compiler_val_list_t));
    if ( l == NULL ) {
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
    if ( l->head == NULL ) {
        l->head = val;
        l->tail = val;
    } else {
        l->tail->next = val;
        l->tail = val;
    }

    return l;
}

/*
 * _val_list_delete -- delete a value list
 */
static void
_val_list_delete(compiler_val_list_t *l)
{
    compiler_val_t *v;
    compiler_val_t *nv;

    v = l->head;
    while ( v != NULL ) {
        nv = v->next;
        free(v);
        v = nv;
    }

    free(l);
}

/*
 * _append_instr
 */
static int
_append_instr(compiler_code_t *code, compiler_instr_t *instr)
{
    if ( code->head == NULL ) {
        code->head = instr;
        code->tail = instr;
    } else {
        code->tail->next = instr;
        code->tail = instr;
    }

    return 0;
}

/*
 * _instr_mov -- allocate a new mov instruction
 */
static compiler_instr_t *
_instr_mov(operand_t *op0, operand_t *op1)
{
    compiler_instr_t *instr;

    instr = _instr_new();
    if ( instr == NULL ) {
        return NULL;
    }
    instr->ir.opcode = IR_OPCODE_MOV;
    memcpy(&instr->operands[0], op0, sizeof(operand_t));
    memcpy(&instr->operands[1], op1, sizeof(operand_t));

    return instr;
}

/*
 * _instr_alloca -- allocate a new alloca instruction
 */
static ir_instr_t *
_instr_alloca(ir_operand_t *op0)
{
    ir_instr_t *instr;

    instr = ir_instr_new();
    if ( instr == NULL ) {
        return NULL;
    }
    instr->opcode = IR_OPCODE_ALLOCA;
    // instr->results[0];
    // instr->operands[0];

    return instr;
}

/*
 * _instr_add -- allocate a new add insruction
 */
static ir_instr_t *
_instr_add(ir_operand_t *op0, ir_operand_t *op1)
{
    return NULL;
}

/*
 * _instr_infix -- allocate a new infix instruction
 */
static ir_instr_t *
_instr_infix(ir_opcode_t opcode, ir_operand_t *op0, ir_operand_t *op1,
             ir_result_t *result)
{
    ir_instr_t *instr;

    /* Add an instruction */
    instr = ir_instr_new();
    if ( instr == NULL ) {
        return NULL;
    }
    instr->opcode = opcode;
    //memcpy(&instr->operands[0], op0, sizeof(operand_t));
    //memcpy(&instr->operands[1], op1, sizeof(operand_t));
    //memcpy(&instr->operands[2], op2, sizeof(operand_t));

    return instr;
}

/*
 * _operand -- convert a variable to an operand
 */
static int
_operand(compiler_t *c, compiler_env_t *env, ir_operand_t *operand)
{
    return -1;
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
    if ( var == NULL ) {
        return NULL;
    }

    val = _val_new_var(env, var);

    return val;
}

/*
 * _literal -- parse a literal
 */
static compiler_val_t *
_literal(compiler_t *c, compiler_env_t *env, literal_t *lit)
{
    compiler_val_t *val;

    val = _val_new_literal(lit);
    if ( val == NULL ) {
        return NULL;
    }

    return val;
}

/*
 * _decl -- parse a declaration
 */
static compiler_val_t *
_decl(compiler_t *c, compiler_env_t *env, decl_t *decl, pos_t pos, int arg,
      int retflag)
{
    compiler_val_t *val;
    compiler_var_t *var;
    compiler_error_t *err;
    int ret;

    /* Allocate a new variable */
    var = _var_new(c, decl->id, decl->type);
    if ( var == NULL ) {
        c->err.code = COMPILER_NOMEM;
        memcpy(&c->err.pos, &pos, sizeof(pos_t));
        return NULL;
    }
    var->arg = arg;
    var->ret = retflag;
    if ( arg ) {
        var->irreg.assigned = 1;
    }

    /* Add the variable to the table */
    ret = _var_add(env, var);
    if ( ret < 0 ) {
        /* Already exists (duplicate declaration), then raise an error */
        err = _error_new(COMPILER_DUPLICATE_VARIABLE, pos);
        if ( err == NULL ) {
            /* Failed to allocate an error */
            c->err_pool.err = COMPILER_NOMEM;
            c->err_pool.pos = pos;
        } else {
            /* Push the error to the stack */
            err->next = c->err_stack;
            c->err_stack = err;
        }
        /* Release the variable and value */
        _var_delete(var);
        return NULL;
    }

    /* Allocate a new value */
    val = _val_new_var(env, var);
    if ( val == NULL ) {
        c->err.code = COMPILER_NOMEM;
        memcpy(&c->err.pos, &pos, sizeof(pos_t));
        return NULL;
    }

    return val;
}

/*
 * _args -- parse function/coroutine arguments (including return values)
 */
static int
_args(compiler_t *c, compiler_env_t *env, arg_list_t *args, int retvals)
{
    arg_t *a;
    compiler_val_t *val;

    a = args->head;
    while ( a != NULL ) {
        if ( retvals ) {
            val = _decl(c, env, a->decl, a->pos, 0, 1);
        } else {
            val = _decl(c, env, a->decl, a->pos, 1, 0);
        }
        if ( val == NULL ) {
            return -1;
        }
        /* Release the unused value */
        _val_delete(val);
        a = a->next;
    }

    return 0;
}

/*
 * _assign -- parse an assignment instruction
 */
static compiler_val_t *
_assign(compiler_t *c, compiler_env_t *env, op_t *op, pos_t pos)
{
    compiler_val_t *v0;
    compiler_val_t *v1;
    operand_t op0;
    operand_t op1;
    compiler_instr_t *instr;
    int ret;

    /* Syntax check */
    if ( op->fix != FIX_INFIX ) {
        /* Syntax error */
        c->err.code = COMPILER_SYNTAX_ERROR;
        memcpy(&c->err.pos, &pos, sizeof(pos_t));
        return NULL;
    }

    /* Evaluate the expressions */
    v0 = _expr(c, env, op->e0);
    v1 = _expr(c, env, op->e1);
    if ( v0->type != VAL_VAR ) {
        /* Syntax error */
        _val_delete(v0);
        _val_delete(v1);
        c->err.code = COMPILER_SYNTAX_ERROR;
        memcpy(&c->err.pos, &pos, sizeof(pos_t));
        return NULL;
    }

    /* Assign */
    op0.type = OPERAND_VAL;
    op0.u.val = v1;
    op1.type = OPERAND_VAL;
    op1.u.val = v0;
    instr = _instr_mov(&op0, &op1);
    if ( instr == NULL ) {
        _val_delete(v0);
        _val_delete(v1);
        return NULL;
    }
    ret = _append_instr(&env->code, instr);
    if ( ret < 0 ) {
        /* FIXME: delete the instruction */
        return NULL;
    }

    return v0;
}

/*
 * _op_infix -- parse an infix operation
 */
static compiler_val_t *
_op_infix(compiler_t *c, compiler_env_t *env, op_t *op, ir_opcode_t opcode,
          pos_t pos)
{
    compiler_val_t *vr;
    compiler_val_t *v0;
    compiler_val_t *v1;
    ir_instr_t *instr;
    operand_t op0;
    operand_t op1;
    operand_t op2;
    int ret;

    if ( op->fix != FIX_INFIX ) {
        return NULL;
    }

    /* Evaluate the expressions */
    v0 = _expr(c, env, op->e0);
    v1 = _expr(c, env, op->e1);

    /* Allocate a new value */
    vr = _val_new_reg(env);
    if ( vr == NULL ) {
        _val_delete(v0);
        _val_delete(v1);
        return NULL;
    }
    /* FIXME: val to reg */

    /* Prepare operands */
    op0.type = OPERAND_VAL;
    op0.u.val = v0;
    op1.type = OPERAND_VAL;
    op1.u.val = v1;
    op2.type = OPERAND_VAL;
    op2.u.val = vr;

    ir_operand_t irop0;
    ir_operand_t irop1;
    ir_result_t res;

    /* Add an instruction */
    instr = _instr_infix(opcode, &irop0, &irop1, &res);
    if ( instr == NULL ) {
        _val_delete(v0);
        _val_delete(v1);
        _val_delete(vr);
        return NULL;
    }
    ret = _append_instr(&env->code, instr);
    if ( ret < 0 ) {
        /* FIXME: delete the instruction */
        return NULL;
    }

    return vr;
}

/*
 * _op_prefix -- parse an prefix operation
 */
static compiler_val_t *
_op_prefix(compiler_t *c, compiler_env_t *env, op_t *op, ir_opcode_t opcode,
           pos_t pos)
{
    compiler_val_t *vr;
    compiler_val_t *v;
    compiler_instr_t *instr;
    int ret;

    if ( op->fix != FIX_PREFIX ) {
        return NULL;
    }

    /* Evaluate the expressions */
    v = _expr(c, env, op->e0);

    /* Allocate a new value */
    vr = _val_new_reg(env);
    if ( vr == NULL ) {
        _val_delete(v);
        return NULL;
    }
    /* FIXME: val to reg */

    /* Add an instruction */
    instr = _instr_new();
    if ( instr == NULL ) {
        _val_delete(v);
        _val_delete(vr);
        return NULL;
    }
    instr->ir.opcode = opcode;
    instr->operands[0].type = OPERAND_VAL;
    instr->operands[0].u.val = v;
    instr->operands[1].type = OPERAND_VAL;
    instr->operands[1].u.val = vr;
    ret = _append_instr(&env->code, instr);
    if ( ret < 0 ) {
        /* FIXME: delete the instruction */
        return NULL;
    }

    return vr;
}

/*
 * _divmod -- paser a divide/modulo operation
 */
static compiler_val_t *
_divmod(compiler_t *c, compiler_env_t *env, op_t *op, ir_opcode_t opcode,
        pos_t pos)
{
    compiler_val_t *vr;
    compiler_val_t *v0;
    compiler_val_t *v1;
    ir_instr_t *instr;
    int ret;

    if ( op->fix != FIX_INFIX ) {
        return NULL;
    }

    /* Evaluate the expressions */
    v0 = _expr(c, env, op->e0);
    v1 = _expr(c, env, op->e1);

    /* Allocate a new value */
    vr = _val_new_reg_set(env);
    if ( vr == NULL ) {
        _val_delete(v0);
        _val_delete(v1);
        return NULL;
    }
    /* FIXME: val to reg */

    instr = ir_instr_new();
    if ( instr == NULL ) {
        _val_delete(v0);
        _val_delete(v1);
        _val_delete(vr);
        return NULL;
    }
    instr->opcode = opcode;
    instr->operands[0].type = OPERAND_TYPE_REG;
    //instr->operands[0].u.val = v0;
    //instr->operands[1].type = OPERAND_VAL;
    //instr->operands[1].u.val = v1;
    //instr->operands[2].type = OPERAND_VAL;
    //instr->operands[2].u.val = vr;
    //ret = _append_instr(&env->code, instr);
    //if ( ret < 0 ) {
    //    /* FIXME: delete the instruction */
    //    return NULL;
    //}

    return vr;
}

/*
 * _incdec -- parse an increment/decrement instruction
 */
static compiler_val_t *
_incdec(compiler_t *c, compiler_env_t *env, op_t *op, ir_opcode_t opcode,
        pos_t pos)
{
    compiler_instr_t *instr;
    compiler_val_t *val;
    compiler_val_t *vr;
    operand_t op0;
    operand_t op1;
    int ret;

    val = _expr(c, env, op->e0);
    if ( VAL_VAR != val->type ) {
        /* Only variable is allowed for this operation. */
        return NULL;
    }

    if ( FIX_SUFFIX == op->fix ) {
        /* Suffix: return the original value, then apply the operation to the
           variable */
        vr = _val_new_reg(env);
        if ( vr == NULL ) {
            return NULL;
        }

        /* Copy the value to a register */
        op0.type = OPERAND_VAL;
        op0.u.val = val;
        op1.type = OPERAND_VAL;
        op1.u.val = vr;
        instr = _instr_mov(&op0, &op1);
        if ( instr == NULL  ) {
            _val_delete(val);
            _val_delete(vr);
            return NULL;
        }
        ret = _append_instr(&env->code, instr);
        if ( ret < 0 ) {
            return NULL;
        }
    } else if ( FIX_PREFIX == op->fix ) {
        /* Prefix: apply the operation to the variable, then return the value */
    } else {
        /* Other than above, then raise an error */
        return NULL;
    }

    /* Add the inc/dec instruction */
    instr = _instr_new();
    if ( instr == NULL ) {
        /* FIXME: delete the values */
        return NULL;
    }
    instr->ir.opcode = opcode;
    instr->operands[0].type = OPERAND_VAL;
    instr->operands[0].u.val = val;
    ret = _append_instr(&env->code, instr);
    if ( ret < 0 ) {
        /* FIXME: delete the instruction */
        return NULL;
    }

    return val;
}

/*
 * _op -- parse an operator
 */
static compiler_val_t *
_op(compiler_t *c, compiler_env_t *env, op_t *op, pos_t pos)
{
    compiler_val_t *val;

    val = NULL;
    switch ( op->type ) {
    case OP_ASSIGN:
        val = _assign(c, env, op, pos);
        break;
    case OP_ADD:
        val = _op_infix(c, env, op, IR_OPCODE_ADD, pos);
        break;
    case OP_SUB:
        val = _op_infix(c, env, op, IR_OPCODE_SUB, pos);
        break;
    case OP_MUL:
        val = _op_infix(c, env, op, IR_OPCODE_MUL, pos);
        break;
    case OP_DIV:
        val = _divmod(c, env, op, IR_OPCODE_DIV, pos);
        break;
    case OP_MOD:
        val = _divmod(c, env, op, IR_OPCODE_MOD, pos);
        break;
    case OP_NOT:
        val = _op_prefix(c, env, op, IR_OPCODE_NOT, pos);
        break;
    case OP_LAND:
        val = _op_infix(c, env, op, IR_OPCODE_LAND, pos);
        break;
    case OP_LOR:
        val = _op_infix(c, env, op, IR_OPCODE_LOR, pos);
        break;
    case OP_AND:
        val = _op_infix(c, env, op, IR_OPCODE_AND, pos);
        break;
    case OP_OR:
        val = _op_infix(c, env, op, IR_OPCODE_OR, pos);
        break;
    case OP_XOR:
        val = _op_infix(c, env, op, IR_OPCODE_XOR, pos);
        break;
    case OP_COMP:
        val = _op_prefix(c, env, op, IR_OPCODE_COMP, pos);
        break;
    case OP_LSHIFT:
        val = _op_infix(c, env, op, IR_OPCODE_LSHIFT, pos);
        break;
    case OP_RSHIFT:
        val = _op_infix(c, env, op, IR_OPCODE_RSHIFT, pos);
        break;
    case OP_CMP_EQ:
        val = _op_infix(c, env, op, IR_OPCODE_CMP_EQ, pos);
        break;
    case OP_CMP_NEQ:
        val = _op_infix(c, env, op, IR_OPCODE_CMP_NEQ, pos);
        break;
    case OP_CMP_GT:
        val = _op_infix(c, env, op, IR_OPCODE_CMP_GT, pos);
        break;
    case OP_CMP_LT:
        val = _op_infix(c, env, op, IR_OPCODE_CMP_LT, pos);
        break;
    case OP_CMP_GEQ:
        val = _op_infix(c, env, op, IR_OPCODE_CMP_GEQ, pos);
        break;
    case OP_CMP_LEQ:
        val = _op_infix(c, env, op, IR_OPCODE_CMP_LEQ, pos);
        break;
    case OP_INC:
        val = _incdec(c, env, op, IR_OPCODE_INC, pos);
        break;
    case OP_DEC:
        val = _incdec(c, env, op, IR_OPCODE_DEC, pos);
        break;
    }

    return val;
}

/*
 * _switch -- parse a switch expression
 */
static compiler_val_t *
_switch(compiler_t *c, compiler_env_t *env, switch_t *sw)
{
    compiler_val_t *cond;
    compiler_val_t *rv;
    compiler_env_t *nenv;
    switch_case_t *cs;
    literal_set_t *lset;
    ssize_t n;

    /* Create a new environemt */
    nenv = _env_new(c);
    if ( nenv == NULL ) {
        return NULL;
    }
    nenv->prev = env;

    /* Parse the condition */
    cond = _expr(c, env, sw->cond);

    /* Count the number of cases */
    n = 0;
    cs = sw->block->head;
    while ( cs != NULL ) {
        n++;
        cs = cs->next;
    }

    /* Initialize the return value with a n-conditional-value set value */
    rv = _val_new_cond(n);
    if ( rv == NULL ) {
        return NULL;
    }

    /* Parse the code block */
    n = 0;
    cs = sw->block->head;
    while ( cs != NULL ) {
        lset = cs->lset;
        rv->u.conds->vals[n] = _inner_block(c, nenv, cs->block);
        n++;
        cs = cs->next;
    }

    return rv;
}

/*
 * _if -- parse an if expression
 */
static compiler_val_t *
_if(compiler_t *c, compiler_env_t *env, if_t *ife)
{
    compiler_val_t *cond;
    compiler_val_t *rv;
    compiler_env_t *nenv;

    /* Create a new environment */
    nenv = _env_new(c);
    if ( nenv == NULL ) {
        return NULL;
    }
    nenv->prev = env;

    /* Parse the condition */
    cond = _expr(c, env, ife->cond);

    /* Initialize the return value with a two-conditional-value set value */
    rv = _val_new_cond(2);
    if ( rv == NULL ) {
        return NULL;
    }

    /* Parse the code block */
    rv->u.conds->vals[0] = _inner_block(c, nenv, ife->bif);
    rv->u.conds->vals[1] = _inner_block(c, nenv, ife->belse);

    return rv;
}

/*
 * _call -- parse a call expression
 */
static compiler_val_t *
_call(compiler_t *c, compiler_env_t *env, call_t *call)
{
    compiler_val_t *rv;
    compiler_env_t *nenv;

    /* Create a new environment */
    nenv = _env_new(c);
    if ( nenv == NULL ) {
        return NULL;
    }
    nenv->prev = env;

    /* Initialize the return value */
    rv = NULL;

    return rv;
}

/*
 * _ref -- parse a reference
 */
static compiler_val_t *
_ref(compiler_t *c, compiler_env_t *env, ref_t *ref)
{
    compiler_val_t *val;
    compiler_val_t *arg;

    val = _expr(c, env, ref->var);
    arg = _expr(c, env, ref->arg);

    if ( val == NULL || arg == NULL  ) {
        return NULL;
    }
    /* Check the type of the value */
    if ( val->type != VAL_VAR ) {
        /* Type error */
        return NULL;
    }

    /* Reference to arg from val */

    return NULL;
}

/*
 * _member -- parse a member reference
 */
static compiler_val_t *
_member(compiler_t *c, compiler_env_t *env, member_t *mem)
{
    compiler_val_t *val;

    val = _expr(c, env, mem->e);

    if ( val == NULL ) {
        return NULL;
    }
    /* Check the type of the value */
    if ( val->type != VAL_VAR ) {
        /* Type error */
        return NULL;
    }

    /* Referecne to mem->id from val */

    /* Resolve the type of value */

    /* Resolve the member offset */

    return NULL;
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
        val = _decl(c, env, e->u.decl, e->pos, 0, 0);
        break;
    case EXPR_LITERAL:
        val = _literal(c, env, e->u.lit);
        break;
    case EXPR_OP:
        val = _op(c, env, e->u.op, e->pos);
        break;
    case EXPR_SWITCH:
        val = _switch(c, env, &e->u.sw);
        break;
    case EXPR_IF:
        val = _if(c, env, &e->u.ife);
        break;
    case EXPR_CALL:
        val = _call(c, env, e->u.call);
        break;
    case EXPR_REF:
        val = _ref(c, env, e->u.ref);
        break;
    case EXPR_MEMBER:
        val = _member(c, env, &e->u.mem);
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
    if ( val == NULL ) {
        return NULL;
    }
    l = _val_list_new(NULL);
    if ( l == NULL ) {
        free(val);
        return NULL;
    }

    e = exprs->head;
    while ( e != NULL ) {
        v = _expr(c, env, e);
        if ( v == NULL ) {
            return NULL;
        }
        l = _val_list_append(l, v);
        if ( l == NULL ) {
            return NULL;
        }
        e = e->next;
    }

    val->type = VAL_LIST;
    val->u.list = l;

    return val;
}

/*
 * _while -- parse a while statement
 */
static compiler_val_t *
_while(compiler_t *c, compiler_env_t *env, stmt_while_t *w)
{
    return NULL;
}

/*
 * _return -- parse a return statement
 */
static compiler_val_t *
_return(compiler_t *c, compiler_env_t *env, expr_t *e)
{
    compiler_instr_t *instr;
    compiler_val_t *val;

    if ( e == NULL ) {
        /* FIXME: Get the last statement value */
        val = NULL;
    } else {
        val = _expr(c, env, e);
        if ( val == NULL ) {
            return NULL;
        }
    }

    /* return instruction */
    instr = _instr_new();
    if ( instr == NULL ) {
        return NULL;
    }
    instr->ir.opcode = IR_OPCODE_RET;
    instr->operands[0].type = OPERAND_VAL;
    instr->operands[0].u.val = val;

    return val;
}

/*
 * _stmt -- parse a statement
 */
static compiler_val_t *
_stmt(compiler_t *c, compiler_env_t *env, stmt_t *stmt)
{
    compiler_env_t *nenv;
    compiler_val_t *val;

    val = NULL;
    switch ( stmt->type ) {
    case STMT_WHILE:
        val = _while(c, env, &stmt->u.whilestmt);
        env->retval = val;
        break;
    case STMT_EXPR:
        val = _expr(c, env, stmt->u.expr);
        env->retval = val;
        break;
    case STMT_EXPR_LIST:
        val = _expr_list(c, env, stmt->u.exprs);
        env->retval = val;
        break;
    case STMT_BLOCK:
        /* Create a new environemt (scope) */
        nenv = _env_new(c);
        if ( nenv == NULL ) {
            return NULL;
        }
        nenv->prev = env;
        val = _inner_block(c, nenv, stmt->u.block);
        if ( val == NULL ) {
            return NULL;
        }
        break;
    case STMT_RETURN:
        val = _return(c, env, stmt->u.expr);
        break;
    }

    return val;
}

/*
 * _inner_block -- parse an inner block
 */
static compiler_val_t *
_inner_block(compiler_t *c, compiler_env_t *env, inner_block_t *block)
{
    compiler_val_t *rv;
    stmt_t *stmt;

    stmt = block->stmts->head;
    while ( stmt != NULL ) {
        rv = _stmt(c, env, stmt);
        if ( rv == NULL ) {
            return NULL;
        }
        stmt = stmt->next;
    }

    return rv;
}

/*
 * _func -- parse a function definition
 */
static compiler_block_t *
_func(compiler_t *c, func_t *fn)
{
    int ret;
    compiler_env_t *env;
    compiler_block_t *block;
    compiler_val_t *val;
    ir_func_t *irfunc;

    /* Allocate a new function IR */
    irfunc = ir_func_new();
    if ( irfunc == NULL ) {
        return NULL;
    }
    irfunc->type = IR_FUNC_FUNC;

    /* Allocate a new environment */
    env = _env_new(c);
    if ( env == NULL ) {
        return NULL;
    }

    /* Parse arguments and return values */
    ret = _args(c, env, fn->args, 0);
    if ( ret < 0 ) {
        _env_delete(env);
        return NULL;
    }
    ret = _args(c, env, fn->rets, 1);
    if ( ret < 0 ) {
        _env_delete(env);
        return NULL;
    }

    /* Parse the inner block */
    val = _inner_block(c, env, fn->block);
    if ( val == NULL ) {
        _env_delete(env);
        return NULL;
    }

    /* Allocate a block */
    block = malloc(sizeof(compiler_block_t));
    if ( block == NULL ) {
        /* FIXME: free val */
        _env_delete(env);
        c->err.code = COMPILER_NOMEM;
        return NULL;
    }
    irfunc->name = strdup(fn->id);
    if ( irfunc->name == NULL ) {
        return NULL;
    }
    block->type = BLOCK_FUNC;
    block->env = env;
    block->next = NULL;
    block->func = irfunc;

    return block;
}

/*
 * _coroutine -- parse a coroutine definition
 */
static compiler_block_t *
_coroutine(compiler_t *c, coroutine_t *cr)
{
    int ret;
    compiler_env_t *env;
    compiler_block_t *block;
    compiler_val_t *val;
    ir_func_t *irfunc;

    /* Allocate a new function IR */
    irfunc = ir_func_new();
    if ( irfunc == NULL ) {
        return NULL;
    }
    irfunc->type = IR_FUNC_COROUTINE;

    /* Allocate a new environment */
    env = _env_new(c);
    if ( env == NULL ) {
        return NULL;
    }

    /* Parse arguments and return values */
    ret = _args(c, env, cr->args, 0);
    if ( ret < 0 ) {
        _env_delete(env);
        return NULL;
    }
    ret = _args(c, env, cr->rets, 1);
    if ( ret < 0 ) {
        _env_delete(env);
        return NULL;
    }

    /* Parse the inner block */
    val = _inner_block(c, env, cr->block);
    if ( val == NULL ) {
        _env_delete(env);
        return NULL;
    }

    /* Allocate a block */
    block = malloc(sizeof(compiler_block_t));
    if ( block == NULL ) {
        /* FIXME: free val */
        _env_delete(env);
        c->err.code = COMPILER_NOMEM;
        return NULL;
    }
    irfunc->name = strdup(cr->id);
    if ( irfunc->name == NULL ) {
        return NULL;
    }
    block->type = BLOCK_COROUTINE;
    block->env = env;
    block->next = NULL;
    block->func = irfunc;

    return block;
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
 * _use -- parse a use directive
 */
static int
_use(compiler_t *c, use_t *use)
{
    //use->id;
    return -1;
}

/*
 * _struct -- parse a struct directive
 */
static int
_struct(compiler_t *c, struct_t *st)
{
    //st->id;
    return -1;
}

/*
 * _union -- parse a union directive
 */
static int
_union(compiler_t *c, union_t *un)
{
    //un->id;
    return -1;
}

/*
 * _enum -- parse an enum directive
 */
static int
_enum(compiler_t *c, enum_t *en)
{
    //en0>id;
    return -1;
}

/*
 * _typedef -- parse a typedef directive
 */
static int
_typedef(compiler_t *c, typedef_t *td)
{
    //td->src;
    //td->dst;
    return -1;
}

/*
 * _directive -- parse a directive
 */
static int
_directive(compiler_t *c, directive_t *dr)
{
    int ret;

    switch ( dr->type ) {
    case DIRECTIVE_USE:
        ret = _use(c, &dr->u.use);
        break;
    case DIRECTIVE_STRUCT:
        ret = _struct(c, &dr->u.st);
        break;
    case DIRECTIVE_UNION:
        ret = _union(c, &dr->u.un);
        break;
    case DIRECTIVE_ENUM:
        ret = _enum(c, &dr->u.en);
        break;
    case DIRECTIVE_TYPEDEF:
        ret = _typedef(c, &dr->u.td);
        break;
    }
    COMPILE_ERROR_RETURN(c, "invalid directive");
}

/*
 * _outer_block_entry -- parse an outer block entry
 */
static compiler_block_t *
_outer_block_entry(compiler_t *c, outer_block_entry_t *e)
{
    int ret;
    compiler_block_t *block;

    ret = -1;
    block = NULL;
    switch ( e->type ) {
    case OUTER_BLOCK_FUNC:
        /* Function */
        block = _func(c, e->u.fn);
        if ( block != NULL ) {
            ret = 0;
        }
        break;
    case OUTER_BLOCK_COROUTINE:
        /* Coroutine */
        block = _coroutine(c, e->u.cr);
        if ( block != NULL ) {
            ret = 0;
        }
        break;
    case OUTER_BLOCK_MODULE:
        /* Module */
        ret = _module(c, e->u.md);
        break;
    case OUTER_BLOCK_DIRECTIVE:
        /* Directive */
        ret = _directive(c, e->u.dr);
        break;
    }

    if ( ret < 0 ) {
        /* Raise a parse error */
        return NULL;
    }

    return block;
}

/*
 * _free_instrs -- release the instructions
 */
static void
_free_instrs(compiler_t *c, compiler_instr_t *instrs)
{
    compiler_instr_t *i;
    compiler_instr_t *ni;

    i = instrs;
    while ( i != NULL ) {
        ni = i->next;
        /* Review: free values referred from this instruction */
        free(i);
        i = ni;
    }
}

/*
 * _free_blocks -- release the blocks
 */
static void
_free_blocks(compiler_t *c, compiler_block_t *b)
{
    compiler_env_t *env;
    compiler_env_t *penv;

    switch ( b->type ) {
    case BLOCK_FUNC:
    case BLOCK_COROUTINE:
        /* Release all environements */
        env = b->env;
        while ( env != NULL ) {
            penv = env->prev;
            _env_delete(env);
            env = penv;
        }
        b->env = NULL;
        break;
    }
    _free_blocks(c, b->next);
    free(b);
}

/*
 * _outer_block -- compile an outer block
 */
static compiler_block_t *
_outer_block(compiler_t *c, outer_block_t *block)
{
    compiler_block_t *b;
    compiler_block_t *tb;
    compiler_block_t *pb;
    outer_block_entry_t *e;

    /* Parse all outer block entries */
    e = block->head;
    pb = NULL;
    tb = NULL;
    while ( e != NULL ) {
        /* Parse an outer block entry */
        b = _outer_block_entry(c, e);
        if ( b == NULL ) {
            _free_blocks(c, tb);
            return NULL;
        }
        /* Link to the block list */
        if ( pb != NULL ) {
            pb->next = b;
        } else {
            tb = b;
        }
        pb = b;

        e = e->next;
    }

    return tb;
}

/*
 * _st -- compile the syntax treee
 */
static compiler_block_t *
_st(compiler_t *c, st_t *st)
{
    return _outer_block(c, st->block);
}

/*
 * compile -- compiile a syntax tree to the intermediate representation
 */
compiler_t *
minica_compile(st_t *st)
{
    compiler_t *c;
    compiler_block_t *b;

    /* Allocate a compiler instance */
    c = malloc(sizeof(compiler_t));
    if ( c == NULL ) {
        return NULL;
    }
    c->irobj = NULL;
    c->fout = NULL;
    c->blocks = NULL;
    c->symbols.n = 0;
    c->symbols.symbols = NULL;
    c->err_stack = NULL;

    /* Initialize the error handler */
    c->err.code = COMPILER_ERROR_UNKNOWN;

    /* Initialize the error stack */
    memset(&c->err_pool, 0, sizeof(compiler_error_t));
    c->err_pool.err = COMPILER_ERROR_UNKNOWN;

    /* Compile the syntax tree */
    b = _st(c, st);
    if ( b == NULL ) {
        return NULL;
    }
    c->blocks = b;

    return c;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
