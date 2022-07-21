/*_
 * Copyright (c) 2019-2022 Hirochika Asai <asai@jar.jp>
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
    if ( NULL == t ) {
        t = malloc(sizeof(compiler_var_table_t));
        if ( NULL == t ) {
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
 * _env_new -- allocate a new environment
 */
static compiler_env_t *
_env_new(compiler_t *c)
{
    compiler_env_t *env;

    env = malloc(sizeof(compiler_env_t));
    if ( NULL == env ) {
        c->err = COMPILER_NOMEM;
        return NULL;
    }
    env->vars = _var_table_initialize(NULL);
    if ( NULL == env->vars ) {
        free(env);
        c->err = COMPILER_NOMEM;
        return NULL;
    }

    env->prev = NULL;
    env->retval = NULL;

    env->code.head = NULL;
    env->code.tail = NULL;

    env->opt.max_id = -1;

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
    var->next = NULL;

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
    compiler_var_t *v;

    /* Duplicate check */
    v = env->vars->top;
    while ( v != NULL ) {
        if ( strcmp(var->id, v->id) == 0 ) {
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
    val->type = VAL_NIL;
    val->opt.id = -1;
    val->opt.type = NULL;

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
    if ( NULL == val ) {
        return NULL;
    }
    val->type = VAL_NIL;

    return val;
}

/*
 * _val_new_var -- allocate a new variable value
 */
static compiler_val_t *
_val_new_var(var_t *var)
{
    compiler_val_t *val;

    val = _val_new();
    if ( NULL == val ) {
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
    if ( NULL == val ) {
        return NULL;
    }
    val->type = VAL_LITERAL;
    val->u.lit = lit;

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
 * _val_list_delete -- delete a value list
 */
static void
_val_list_delete(compiler_val_list_t *l)
{
    compiler_val_t *v;
    compiler_val_t *nv;

    v = l->head;
    while ( NULL != v ) {
        nv = v->next;
        free(v);
        v = nv;
    }

    free(l);
}

/*
 * _val_cond_new -- allocate a new value condition set
 */
static compiler_val_cond_t *
_val_cond_new(int n)
{
    compiler_val_cond_t *cset;

    cset = malloc(sizeof(compiler_val_cond_t));
    if ( NULL == cset ) {
        return NULL;
    }
    cset->n = n;
    cset->vals = malloc(sizeof(compiler_val_t *) * n);
    if ( NULL == cset->vals ) {
        free(cset);
        return NULL;
    }
    memset(cset->vals, 0, sizeof(compiler_val_t *) * n);

    return cset;
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
 * _instr_mov -- allocate a new mov instruction
 */
static compiler_instr_t *
_instr_mov(operand_t *op0, operand_t *op1)
{
    compiler_instr_t *instr;

    instr = _instr_new();
    if ( NULL == instr ) {
        return NULL;
    }
    instr->opcode = OPCODE_MOV;
    memcpy(&instr->operands[0], op0, sizeof(operand_t));
    memcpy(&instr->operands[1], op1, sizeof(operand_t));

    return instr;
}

/*
 * _instr_infix -- allocate a new infix instruction
 */
static compiler_instr_t *
_instr_infix(opcode_t opcode, operand_t *op0, operand_t *op1, operand_t *op2)
{
    compiler_instr_t *instr;

    /* Add an instruction */
    instr = _instr_new();
    if ( NULL == instr ) {
        return NULL;
    }
    instr->opcode = opcode;
    memcpy(&instr->operands[0], op0, sizeof(operand_t));
    memcpy(&instr->operands[1], op1, sizeof(operand_t));
    memcpy(&instr->operands[2], op2, sizeof(operand_t));

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
_decl(compiler_t *c, compiler_env_t *env, decl_t *decl, pos_t *pos, int arg,
      int retflag)
{
    compiler_val_t *val;
    compiler_var_t *var;
    int ret;

    /* Allocate a new variable */
    var = _var_new(decl->id, decl->type);
    if ( NULL == var ) {
        c->err = COMPILER_NOMEM;
        memcpy(&c->pos, pos, sizeof(pos_t));
        return NULL;
    }
    var->arg = arg;
    var->ret = retflag;

    /* Allocate a new value */
    val = _val_new();
    if ( NULL == val ) {
        _var_delete(var);
        c->err = COMPILER_NOMEM;
        memcpy(&c->pos, pos, sizeof(pos_t));
        return NULL;
    }
    val->type = VAL_VAR;
    val->u.var = var;

    /* Add the variable to the table */
    ret = _var_add(env, var);
    if ( ret < 0 ) {
        /* Already exists */
        c->err = COMPILER_DUPLICATE_VARIABLE;
        memcpy(&c->pos, pos, sizeof(pos_t));
        /* Release the variable and value */
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
_args(compiler_t *c, compiler_env_t *env, arg_list_t *args, int retvals)
{
    arg_t *a;
    compiler_val_t *val;

    a = args->head;
    while ( NULL != a ) {
        if ( retvals ) {
            val = _decl(c, env, a->decl, &a->pos, 0, 1);
        } else {
            val = _decl(c, env, a->decl, &a->pos, 1, 0);
        }
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
_assign(compiler_t *c, compiler_env_t *env, op_t *op, pos_t *pos)
{
    compiler_val_t *v0;
    compiler_val_t *v1;
    operand_t op0;
    operand_t op1;
    compiler_instr_t *instr;
    int ret;

    if ( FIX_INFIX != op->fix ) {
        /* Syntax error */
        c->err = COMPILER_SYNTAX_ERROR;
        memcpy(&c->pos, pos, sizeof(pos_t));
        return NULL;
    }

    /* Evaluate the expressions */
    v0 = _expr(c, env, op->e0);
    v1 = _expr(c, env, op->e1);
    if ( VAL_VAR != v0->type ) {
        /* Syntax error */
        c->err = COMPILER_SYNTAX_ERROR;
        memcpy(&c->pos, pos, sizeof(pos_t));
        return NULL;
    }

    /* Assign */
    op0.type = OPERAND_VAL;
    op0.u.val = v1;
    op1.type = OPERAND_VAL;
    op1.u.val = v0;
    instr = _instr_mov(&op0, &op1);
    if ( NULL == instr ) {
        return NULL;
    }
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
_op_infix(compiler_t *c, compiler_env_t *env, op_t *op, opcode_t opcode,
          pos_t *pos)
{
    compiler_val_t *vr;
    compiler_val_t *v0;
    compiler_val_t *v1;
    compiler_instr_t *instr;
    operand_t op0;
    operand_t op1;
    operand_t op2;
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

    /* Prepare operands */
    op0.type = OPERAND_VAL;
    op0.u.val = v0;
    op1.type = OPERAND_VAL;
    op1.u.val = v1;
    op2.type = OPERAND_VAL;
    op2.u.val = vr;

    /* Add an instruction */
    instr = _instr_infix(opcode, &op0, &op1, &op2);
    if ( instr == NULL ) {
        return NULL;
    }
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
_op_prefix(compiler_t *c, compiler_env_t *env, op_t *op, opcode_t opcode,
           pos_t *pos)
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
_divmod(compiler_t *c, compiler_env_t *env, op_t *op, opcode_t opcode,
        pos_t *pos)
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
_incdec(compiler_t *c, compiler_env_t *env, op_t *op, opcode_t opcode,
        pos_t *pos)
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
        vr = _val_new();
        if ( NULL == vr ) {
            return NULL;
        }
        vr->type = VAL_REG;

        /* Copy the value to a register */
        op0.type = OPERAND_VAL;
        op0.u.val = val;
        op1.type = OPERAND_VAL;
        op1.u.val = vr;
        instr = _instr_mov(&op0, &op1);
        if ( instr == NULL  ) {
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
}

/*
 * _op -- parse an operator
 */
static compiler_val_t *
_op(compiler_t *c, compiler_env_t *env, op_t *op, pos_t *pos)
{
    compiler_val_t *val;

    val = NULL;
    switch ( op->type ) {
    case OP_ASSIGN:
        val = _assign(c, env, op, pos);
        break;
    case OP_ADD:
        val = _op_infix(c, env, op, OPCODE_ADD, pos);
        break;
    case OP_SUB:
        val = _op_infix(c, env, op, OPCODE_SUB, pos);
        break;
    case OP_MUL:
        val = _op_infix(c, env, op, OPCODE_MUL, pos);
        break;
    case OP_DIV:
        val = _divmod(c, env, op, OPCODE_DIV, pos);
        break;
    case OP_MOD:
        val = _divmod(c, env, op, OPCODE_MOD, pos);
        break;
    case OP_NOT:
        val = _op_prefix(c, env, op, OPCODE_NOT, pos);
        break;
    case OP_LAND:
        val = _op_infix(c, env, op, OPCODE_LAND, pos);
        break;
    case OP_LOR:
        val = _op_infix(c, env, op, OPCODE_LOR, pos);
        break;
    case OP_AND:
        val = _op_infix(c, env, op, OPCODE_AND, pos);
        break;
    case OP_OR:
        val = _op_infix(c, env, op, OPCODE_OR, pos);
        break;
    case OP_XOR:
        val = _op_infix(c, env, op, OPCODE_XOR, pos);
        break;
    case OP_COMP:
        val = _op_prefix(c, env, op, OPCODE_COMP, pos);
        break;
    case OP_LSHIFT:
        val = _op_infix(c, env, op, OPCODE_LSHIFT, pos);
        break;
    case OP_RSHIFT:
        val = _op_infix(c, env, op, OPCODE_RSHIFT, pos);
        break;
    case OP_CMP_EQ:
        val = _op_infix(c, env, op, OPCODE_CMP_EQ, pos);
        break;
    case OP_CMP_NEQ:
        val = _op_infix(c, env, op, OPCODE_CMP_NEQ, pos);
        break;
    case OP_CMP_GT:
        val = _op_infix(c, env, op, OPCODE_CMP_GT, pos);
        break;
    case OP_CMP_LT:
        val = _op_infix(c, env, op, OPCODE_CMP_LT, pos);
        break;
    case OP_CMP_GEQ:
        val = _op_infix(c, env, op, OPCODE_CMP_GEQ, pos);
        break;
    case OP_CMP_LEQ:
        val = _op_infix(c, env, op, OPCODE_CMP_LEQ, pos);
        break;
    case OP_INC:
        val = _incdec(c, env, op, OPCODE_INC, pos);
        break;
    case OP_DEC:
        val = _incdec(c, env, op, OPCODE_DEC, pos);
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
    compiler_val_cond_t *cset;
    ssize_t n;

    /* Create a new environemt */
    nenv = _env_new(c);
    if ( NULL == nenv ) {
        return NULL;
    }
    nenv->prev = env;

    /* Parse the condition */
    cond = _expr(c, env, sw->cond);

    /* Count the number of cases */
    n = 0;
    cs = sw->block->head;
    while ( NULL != cs ) {
        n++;
        cs = cs->next;
    }

    /* Initialize the return value */
    rv = _val_new();
    if ( NULL == rv ) {
        return NULL;
    }

    /* Allocate a n-conditional-value set value */
    cset = _val_cond_new(n);
    if ( NULL == cset ) {
        return NULL;
    }
    rv->type = VAL_COND;
    rv->u.conds = cset;

    /* Parse the code block */
    n = 0;
    cs = sw->block->head;
    while ( NULL != cs ) {
        lset = cs->lset;
        cset->vals[n] = _inner_block(c, nenv, cs->block);
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
    compiler_val_cond_t *cset;

    /* Create a new environment */
    nenv = _env_new(c);
    if ( NULL == nenv ) {
        return NULL;
    }
    nenv->prev = env;

    /* Parse the condition */
    cond = _expr(c, env, ife->cond);

    /* Initialize the return value */
    rv = _val_new();
    if ( NULL == rv ) {
        return NULL;
    }

    /* Allocate a two-conditional-value set value */
    cset = _val_cond_new(2);
    if ( NULL == cset ) {
        return NULL;
    }
    rv->type = VAL_COND;
    rv->u.conds = cset;

    /* Parse the code block */
    cset->vals[0] = _inner_block(c, nenv, ife->bif);
    cset->vals[1] = _inner_block(c, nenv, ife->belse);

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
    if ( NULL == nenv ) {
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

    if ( NULL == val || NULL == arg  ) {
        return NULL;
    }
    /* Check the type of the value */
    if ( VAL_VAR != val->type ) {
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

    if ( NULL == val ) {
        return NULL;
    }
    /* Check the type of the value */
    if ( VAL_VAR != val->type ) {
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
        val = _decl(c, env, e->u.decl, &e->pos, 0, 0);
        break;
    case EXPR_LITERAL:
        val = _literal(c, env, e->u.lit);
        break;
    case EXPR_OP:
        val = _op(c, env, e->u.op, &e->pos);
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

    if ( NULL == e ) {
        /* FIXME: Get the last statement value */
        val = NULL;
    } else {
        val = _expr(c, env, e);
        if ( NULL == val ) {
            return NULL;
        }
    }

    /* return instruction */
    instr = _instr_new();
    if ( NULL == instr ) {
        return NULL;
    }
    instr->opcode = OPCODE_RET;
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
        /* Create a new environemt */
        nenv = _env_new(c);
        if ( NULL == nenv ) {
            return NULL;
        }
        nenv->prev = env;
        val = _inner_block(c, nenv, stmt->u.block);
        if ( NULL == val ) {

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
    while ( NULL != stmt ) {
        rv = _stmt(c, env, stmt);
        if ( NULL == rv ) {
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

    /* Allocate a new environment */
    env = _env_new(c);
    if ( NULL == env ) {
        return NULL;
    }

    /* Parse arguments and return values */
    ret = _args(c, env, fn->args, 0);
    if ( ret < 0 ) {
        return NULL;
    }
    ret = _args(c, env, fn->rets, 1);
    if ( ret < 0 ) {
        return NULL;
    }

    /* Parse the inner block */
    val = _inner_block(c, env, fn->block);
    if ( val == NULL ) {
        return NULL;
    }

    /* Allocate a block */
    block = malloc(sizeof(compiler_block_t));
    if ( block == NULL ) {
        c->err = COMPILER_NOMEM;
        return NULL;
    }
    block->label = strdup(fn->id);
    if ( block->label == NULL ) {
        c->err = COMPILER_NOMEM;
        return NULL;
    }
    block->type = BLOCK_FUNC;
    block->env = env;
    block->next = NULL;

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

    /* Allocate a new environment */
    env = _env_new(c);
    if ( NULL == env ) {
        return NULL;
    }

    /* Parse arguments and return values */
    ret = _args(c, env, cr->args, 0);
    if ( ret < 0 ) {
        return NULL;
    }
    ret = _args(c, env, cr->rets, 1);
    if ( ret < 0 ) {
        return NULL;
    }

    /* Parse the inner block */
    val = _inner_block(c, env, cr->block);
    if ( val == NULL ) {
        return NULL;
    }

    /* Allocate a block */
    block = malloc(sizeof(compiler_block_t));
    if ( block == NULL ) {
        c->err = COMPILER_NOMEM;
        return NULL;
    }
    block->label = strdup(cr->id);
    if ( block->label == NULL ) {
        c->err = COMPILER_NOMEM;
        return NULL;
    }
    block->type = BLOCK_COROUTINE;
    block->env = env;
    block->next = NULL;

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
    while ( NULL != e ) {
        /* Parse an outer block entry */
        b = _outer_block_entry(c, e);
        if ( NULL == b ) {
            /* FIXME: Free the compiled blocks */
            return NULL;
        }
        /* Implement the block handler */
        if ( NULL != pb ) {
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
compile(st_t *st)
{
    compiler_t *c;
    compiler_block_t *b;

    /* Allocate a compiler instance */
    c = malloc(sizeof(compiler_t));
    if ( NULL == c ) {
        return NULL;
    }
    c->fout = NULL;
    c->blocks = NULL;

    /* Initialize the error handler */
    c->err = COMPILER_ERROR_UNKNOWN;

    /* Compile the syntax tree */
    b = _st(c, st);
    if ( NULL == b ) {
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
