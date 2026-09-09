/*_
 * Copyright (c) 2019-2026 Hirochika Asai <asai@jar.jp>
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

/*
 * compiler.c — AST to DFIR compiler
 *
 * Lowers the syntax tree (syntax.h) into Data Flow IR (ir.h).
 * The output is an ir_object_t containing functions, coroutines, and graphs.
 */

#include "syntax.h"
#include "compile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*======================================================================
 * Compiler context
 *======================================================================*/

/*
 * Variable binding in a scope.
 */
typedef struct _cvar {
    char *name;             /* variable name */
    ir_reg_type_t type;     /* IR register type */
    int ssa_id;             /* SSA value id (unique per variable instance) */
    struct _cvar *next;     /* next variable in scope chain */
} cvar_t;

/*
 * Scope (environment).
 */
typedef struct _scope {
    cvar_t *vars;           /* variable list (stack) */
    struct _scope *parent;  /* parent scope */
} scope_t;

/*
 * Block builder — accumulates instructions for the current basic block.
 */
typedef struct _bb {
    char label[64];         /* block label (e.g., "$entry", "S0_start") */
    ir_instr_ent_t *head;   /* first instruction */
    ir_instr_ent_t *tail;   /* last instruction */
    size_t count;           /* number of instructions */
    struct _bb *next;       /* next block in function */
} bb_t;

/*
 * Function builder — accumulates blocks for a function or coroutine.
 */
typedef struct _fnb {
    char name[256];         /* function name */
    ir_func_type_t kind;    /* IR_FUNC_FUNC or IR_FUNC_COROUTINE */
    bb_t *blocks;           /* list of blocks */
    bb_t *cur;              /* current block being built */
    int ssa_counter;        /* SSA value counter */
    int state_counter;      /* coroutine state counter */
    int nargs;              /* number of arguments */
    int is_coro;            /* 1 if compiling a coroutine */
    int state_var_ssa;      /* SSA id of the state variable (for coroutines) */
    /* List of state labels for the dispatch switch */
    char dispatch_labels[64][64];  /* state label names */
    int nstates;            /* number of states */
} fnb_t;

/*
 * Compiler state.
 */
typedef struct {
    ir_object_t *ir;        /* output IR object */
    fnb_t *fn;              /* current function builder (NULL at top level) */
    scope_t *scope;         /* current scope */
    int error;              /* error flag */
} dfir_compiler_t;

/*======================================================================
 * Forward declarations
 *======================================================================*/

static ir_reg_t _ssa(dfir_compiler_t *c, ir_reg_type_t type);
static ir_reg_type_t _type2reg(type_t *type);
static void _emit(dfir_compiler_t *c, ir_opcode_t opc, ir_reg_t *result,
                  int nops, ir_operand_t *ops);

static scope_t *_scope_new(scope_t *parent);
static void _scope_free(scope_t *s);
static cvar_t *_scope_lookup(scope_t *s, const char *name);
static int _scope_bind(scope_t *s, const char *name, ir_reg_type_t type,
                       int ssa_id);

static bb_t *_bb_new(const char *label);
static void _bb_free(bb_t *b);
static int _bb_emit(bb_t *b, ir_opcode_t opc, ir_reg_t *result,
                    int nops, ir_operand_t *ops);

static fnb_t *_fnb_new(const char *name, ir_func_type_t kind);
static void _fnb_free(fnb_t *f);
static bb_t *_fnb_add_block(fnb_t *f, const char *label);

static ir_operand_t _op_reg(ir_reg_t reg);
static ir_operand_t _op_imm_i32(int32_t val);
static ir_operand_t _op_imm_i64(int64_t val);
static ir_operand_t _op_imm_f64(double val);
static ir_operand_t _op_imm_bool(bool val);
static ir_operand_t _op_imm_str(const char *str);
static ir_operand_t _op_label(const char *label);

/* Expression compilation returns an SSA register */
static ir_reg_t _expr(dfir_compiler_t *c, expr_t *e);
static void _stmt(dfir_compiler_t *c, stmt_t *stmt);
static void _inner_block(dfir_compiler_t *c, inner_block_t *block);
static void _func(dfir_compiler_t *c, func_t *fn);
static void _coroutine(dfir_compiler_t *c, coroutine_t *cr);
static void _directive(dfir_compiler_t *c, directive_t *dr);
static void _graph(dfir_compiler_t *c, graph_decl_t *gd);

/*======================================================================
 * Scope management
 *======================================================================*/

static scope_t *
_scope_new(scope_t *parent)
{
    scope_t *s = malloc(sizeof(scope_t));
    if (s) {
        s->vars = NULL;
        s->parent = parent;
    }
    return s;
}

static void
_scope_free(scope_t *s)
{
    if (!s) return;
    cvar_t *v = s->vars;
    while (v) {
        cvar_t *next = v->next;
        free(v->name);
        free(v);
        v = next;
    }
    free(s);
}

static cvar_t *
_scope_lookup(scope_t *s, const char *name)
{
    while (s) {
        cvar_t *v = s->vars;
        while (v) {
            if (strcmp(v->name, name) == 0) return v;
            v = v->next;
        }
        s = s->parent;
    }
    return NULL;
}

static int
_scope_bind(scope_t *s, const char *name, ir_reg_type_t type, int ssa_id)
{
    /* Check for duplicate in current scope only */
    cvar_t *v = s->vars;
    while (v) {
        if (strcmp(v->name, name) == 0) return -1;
        v = v->next;
    }

    cvar_t *nv = malloc(sizeof(cvar_t));
    if (!nv) return -1;
    nv->name = strdup(name);
    nv->type = type;
    nv->ssa_id = ssa_id;
    nv->next = s->vars;
    s->vars = nv;
    return 0;
}

/*======================================================================
 * Basic block builder
 *======================================================================*/

static bb_t *
_bb_new(const char *label)
{
    bb_t *b = malloc(sizeof(bb_t));
    if (b) {
        memset(b, 0, sizeof(bb_t));
        if (label) {
            strncpy(b->label, label, sizeof(b->label) - 1);
        }
    }
    return b;
}

static void
_bb_free(bb_t *b)
{
    if (!b) return;
    ir_instr_ent_t *e = b->head;
    while (e) {
        ir_instr_ent_t *next = e->next;
        /* Free operand resources */
        for (int i = 0; i < e->inst.noperands && i < IR_MAX_OPERANDS; i++) {
            if (e->inst.operands[i].type == IR_OPERAND_LABEL) {
                free(e->inst.operands[i].u.label);
            }
        }
        /* Free result register */
        for (int i = 0; i < e->inst.result.n && i < 2; i++) {
        }
        ir_instr_ent_delete(e);
        e = next;
    }
    free(b);
}

static int
_bb_emit(bb_t *b, ir_opcode_t opc, ir_reg_t *result, int nops,
         ir_operand_t *ops)
{
    ir_instr_ent_t *ent = ir_instr_ent_new();
    if (!ent) return -1;

    ent->inst.opcode = opc;
    ent->inst.noperands = nops;
    ent->next = NULL;

    if (result) {
        ent->inst.result.n = 1;
        ent->inst.result.reg[0] = *result;
    } else {
        ent->inst.result.n = 0;
    }

    for (int i = 0; i < nops && i < IR_MAX_OPERANDS; i++) {
        ent->inst.operands[i] = ops[i];
    }

    if (b->tail) {
        b->tail->next = ent;
    } else {
        b->head = ent;
    }
    b->tail = ent;
    b->count++;

    return 0;
}

/*======================================================================
 * Function builder
 *======================================================================*/

static fnb_t *
_fnb_new(const char *name, ir_func_type_t kind)
{
    fnb_t *f = malloc(sizeof(fnb_t));
    if (f) {
        memset(f, 0, sizeof(fnb_t));
        strncpy(f->name, name ? name : "anonymous", sizeof(f->name) - 1);
        f->kind = kind;
        f->ssa_counter = 0;
        f->state_counter = 0;
    }
    return f;
}

static void
_fnb_free(fnb_t *f)
{
    if (!f) return;
    bb_t *b = f->blocks;
    while (b) {
        bb_t *next = b->next;
        _bb_free(b);
        b = next;
    }
    free(f);
}

static bb_t *
_fnb_add_block(fnb_t *f, const char *label)
{
    bb_t *b = _bb_new(label);
    if (!b) return NULL;

    if (f->blocks) {
        bb_t *tail = f->blocks;
        while (tail->next) tail = tail->next;
        tail->next = b;
    } else {
        f->blocks = b;
    }
    f->cur = b;
    return b;
}

/*======================================================================
 * SSA and type helpers
 *======================================================================*/

static ir_reg_t
_ssa(dfir_compiler_t *c, ir_reg_type_t type)
{
    ir_reg_t reg;
    char buf[32];
    snprintf(buf, sizeof(buf), "%%%d", c->fn->ssa_counter++);
    ir_reg_init(&reg, type, buf);
    return reg;
}

static ir_reg_type_t
_type2reg(type_t *type)
{
    if (!type) return IR_REG_PTR;
    switch (type->type) {
    case TYPE_PRIMITIVE_I8:
    case TYPE_PRIMITIVE_U8:   return IR_REG_I8;
    case TYPE_PRIMITIVE_I16:
    case TYPE_PRIMITIVE_U16:  return IR_REG_I16;
    case TYPE_PRIMITIVE_I32:
    case TYPE_PRIMITIVE_U32:  return IR_REG_I32;
    case TYPE_PRIMITIVE_I64:
    case TYPE_PRIMITIVE_U64:  return IR_REG_I64;
    case TYPE_PRIMITIVE_F16:  return IR_REG_F16;
    case TYPE_PRIMITIVE_F32:  return IR_REG_F32;
    case TYPE_PRIMITIVE_F64:  return IR_REG_F64;
    case TYPE_PRIMITIVE_FP8:  return IR_REG_FP8;
    case TYPE_PRIMITIVE_FP4:  return IR_REG_FP4;
    case TYPE_PRIMITIVE_BOOL: return IR_REG_BOOL;
    case TYPE_PRIMITIVE_STRING: return IR_REG_STR;
    case TYPE_ENUM:           return IR_REG_I64;
    case TYPE_STRUCT:
    case TYPE_ID:
    case TYPE_REFERENCE:
    case TYPE_STREAM:
    case TYPE_CHAN:           return IR_REG_PTR;
    default:                  return IR_REG_PTR;
    }
}

/*======================================================================
 * Operand constructors
 *======================================================================*/

static ir_operand_t
_op_reg(ir_reg_t reg)
{
    ir_operand_t op;
    memset(&op, 0, sizeof(op));
    op.type = IR_OPERAND_REG;
    op.u.reg = reg;
    return op;
}

static ir_operand_t
_op_imm_i32(int32_t val)
{
    ir_operand_t op;
    memset(&op, 0, sizeof(op));
    op.type = IR_OPERAND_IMM;
    ir_imm_init(&op.u.imm, IR_IMM_I32);
    op.u.imm.u.s32 = val;
    return op;
}

static ir_operand_t
_op_imm_i64(int64_t val)
{
    ir_operand_t op;
    memset(&op, 0, sizeof(op));
    op.type = IR_OPERAND_IMM;
    ir_imm_init(&op.u.imm, IR_IMM_I64);
    op.u.imm.u.s64 = val;
    return op;
}

static ir_operand_t
_op_imm_f64(double val)
{
    ir_operand_t op;
    memset(&op, 0, sizeof(op));
    op.type = IR_OPERAND_IMM;
    ir_imm_init(&op.u.imm, IR_IMM_F64);
    op.u.imm.u.f64 = val;
    return op;
}

static ir_operand_t
_op_imm_bool(bool val)
{
    ir_operand_t op;
    memset(&op, 0, sizeof(op));
    op.type = IR_OPERAND_IMM;
    ir_imm_init(&op.u.imm, IR_IMM_BOOL);
    op.u.imm.u.bval = val;
    return op;
}

static ir_operand_t
_op_imm_str(const char *str)
{
    ir_operand_t op;
    memset(&op, 0, sizeof(op));
    op.type = IR_OPERAND_IMM;
    ir_imm_init(&op.u.imm, IR_IMM_STR);
    op.u.imm.u.str = strdup(str);
    return op;
}

static ir_operand_t
_op_label(const char *label)
{
    ir_operand_t op;
    memset(&op, 0, sizeof(op));
    op.type = IR_OPERAND_LABEL;
    op.u.label = strdup(label);
    return op;
}

/*======================================================================
 * Emit helper
 *======================================================================*/

static void
_emit(dfir_compiler_t *c, ir_opcode_t opc, ir_reg_t *result, int nops,
      ir_operand_t *ops)
{
    if (!c->fn || !c->fn->cur) return;
    _bb_emit(c->fn->cur, opc, result, nops, ops);
}

/*======================================================================
 * Literal compilation
 *======================================================================*/

static ir_reg_t
_compile_literal(dfir_compiler_t *c, literal_t *lit)
{
    ir_reg_t result;
    ir_operand_t op;

    switch (lit->type) {
    case LIT_DECINT:
    case LIT_HEXINT:
    case LIT_BININT: {
        /* Parse integer */
        int64_t val = 0;
        if (lit->u.n) {
            if (lit->type == LIT_HEXINT) {
                val = strtoll(lit->u.n, NULL, 16);
            } else if (lit->type == LIT_BININT) {
                val = strtoll(lit->u.n, NULL, 2);
            } else {
                val = strtoll(lit->u.n, NULL, 10);
            }
        }
        ir_reg_type_t rt = (val >= INT32_MIN && val <= INT32_MAX)
                           ? IR_REG_I32 : IR_REG_I64;
        result = _ssa(c, rt);
        op = (rt == IR_REG_I32) ? _op_imm_i32((int32_t)val) : _op_imm_i64(val);
        _emit(c, IR_OPCODE_CONST, &result, 1, &op);
        break;
    }
    case LIT_FLOAT: {
        double val = 0.0;
        if (lit->u.n) val = strtod(lit->u.n, NULL);
        result = _ssa(c, IR_REG_F64);
        op = _op_imm_f64(val);
        _emit(c, IR_OPCODE_CONST, &result, 1, &op);
        break;
    }
    case LIT_STRING: {
        result = _ssa(c, IR_REG_STR);
        op = _op_imm_str(lit->u.s ? lit->u.s : "");
        _emit(c, IR_OPCODE_CONST, &result, 1, &op);
        break;
    }
    case LIT_BOOL: {
        result = _ssa(c, IR_REG_BOOL);
        op = _op_imm_bool(lit->u.b == BOOL_TRUE);
        _emit(c, IR_OPCODE_CONST, &result, 1, &op);
        break;
    }
    default:
        result = _ssa(c, IR_REG_I32);
        op = _op_imm_i32(0);
        _emit(c, IR_OPCODE_CONST, &result, 1, &op);
        break;
    }

    return result;
}

/*======================================================================
 * Expression compilation
 *======================================================================*/

static ir_reg_t
_expr(dfir_compiler_t *c, expr_t *e)
{
    if (!e || c->error) {
        return _ssa(c, IR_REG_NONE);
    }

    switch (e->type) {
    case EXPR_LITERAL:
        return _compile_literal(c, e->u.lit);

    case EXPR_ID: {
        cvar_t *v = _scope_lookup(c->scope, e->u.id);
        if (!v) {
            fprintf(stderr, "error: undefined variable '%s'\n", e->u.id);
            c->error = 1;
            return _ssa(c, IR_REG_NONE);
        }
        ir_reg_t reg;
        ir_reg_init(&reg, v->type, NULL);
        char buf[64];
        snprintf(buf, sizeof(buf), "%%%d", v->ssa_id);
        reg.id = strdup(buf);
        return reg;
    }

    case EXPR_DECL: {
        /* let binding as expression */
        decl_t *d = e->u.decl;
        ir_reg_type_t rtype = _type2reg(d->type);
        ir_reg_t result = _ssa(c, rtype);
        _scope_bind(c->scope, d->id, rtype, c->fn->ssa_counter - 1);

        if (d->init) {
            ir_reg_t init_val = _expr(c, d->init);
            ir_operand_t ops[2];
            ops[0] = _op_reg(init_val);
            ops[1] = _op_reg(result);
            _emit(c, IR_OPCODE_MOV, NULL, 2, ops);
        }
        return result;
    }

    case EXPR_OP: {
        op_t *op = e->u.op;

        /* Assignment */
        if (op->type == OP_ASSIGN) {
            if (op->e0->type != EXPR_ID) {
                fprintf(stderr, "error: invalid assignment target\n");
                c->error = 1;
                return _ssa(c, IR_REG_NONE);
            }
            ir_reg_t val = _expr(c, op->e1);
            cvar_t *v = _scope_lookup(c->scope, op->e0->u.id);
            if (!v) {
                fprintf(stderr, "error: undefined variable '%s'\n",
                        op->e0->u.id);
                c->error = 1;
                return val;
            }
            /* MOV value to existing variable's SSA */
            ir_operand_t ops[2];
            ops[0] = _op_reg(val);
            ir_reg_t dst;
            char buf[64];
            snprintf(buf, sizeof(buf), "%%%d", v->ssa_id);
            ir_reg_init(&dst, v->type, buf);
            ops[1] = _op_reg(dst);
            _emit(c, IR_OPCODE_MOV, NULL, 2, ops);
            return val;
        }

        /* Prefix operations */
        if (op->fix == FIX_PREFIX) {
            ir_reg_t val = _expr(c, op->e0);
            ir_reg_t result;
            ir_opcode_t opc = IR_OPCODE_NEG;
            ir_operand_t ops[2];

            switch (op->type) {
            case OP_NOT:   opc = IR_OPCODE_NOT; break;
            case OP_SUB:   opc = IR_OPCODE_NEG; break;
            case OP_COMP:  opc = IR_OPCODE_NOT; break;
            default:
                fprintf(stderr, "error: unsupported prefix op\n");
                c->error = 1;
                return val;
            }

            result = _ssa(c, val.type);
            ops[0] = _op_reg(val);
            ops[1] = _op_reg(result);
            _emit(c, opc, &result, 1, ops);
            return result;
        }

        /* Infix binary operations */
        if (op->fix == FIX_INFIX) {
            ir_reg_t lhs = _expr(c, op->e0);
            ir_reg_t rhs = _expr(c, op->e1);
            ir_opcode_t opc;
            ir_reg_type_t rtype;

            switch (op->type) {
            case OP_ADD:      opc = IR_OPCODE_ADD;     rtype = lhs.type; break;
            case OP_SUB:      opc = IR_OPCODE_SUB;     rtype = lhs.type; break;
            case OP_MUL:      opc = IR_OPCODE_MUL;     rtype = lhs.type; break;
            case OP_DIV:      opc = IR_OPCODE_DIV;     rtype = lhs.type; break;
            case OP_MOD:      opc = IR_OPCODE_MOD;     rtype = lhs.type; break;
            case OP_AND:      opc = IR_OPCODE_AND;     rtype = lhs.type; break;
            case OP_OR:       opc = IR_OPCODE_OR;      rtype = lhs.type; break;
            case OP_XOR:      opc = IR_OPCODE_XOR;     rtype = lhs.type; break;
            case OP_LSHIFT:   opc = IR_OPCODE_SHL;     rtype = lhs.type; break;
            case OP_RSHIFT:   opc = IR_OPCODE_SHR;     rtype = lhs.type; break;
            case OP_LAND:     opc = IR_OPCODE_AND;     rtype = IR_REG_BOOL; break;
            case OP_LOR:      opc = IR_OPCODE_OR;      rtype = IR_REG_BOOL; break;
            case OP_CMP_EQ:   opc = IR_OPCODE_CMP_EQ;  rtype = IR_REG_BOOL; break;
            case OP_CMP_NEQ:  opc = IR_OPCODE_CMP_NE;  rtype = IR_REG_BOOL; break;
            case OP_CMP_GT:   opc = IR_OPCODE_CMP_GT;  rtype = IR_REG_BOOL; break;
            case OP_CMP_LT:   opc = IR_OPCODE_CMP_LT;  rtype = IR_REG_BOOL; break;
            case OP_CMP_GEQ:  opc = IR_OPCODE_CMP_GE;  rtype = IR_REG_BOOL; break;
            case OP_CMP_LEQ:  opc = IR_OPCODE_CMP_LE;  rtype = IR_REG_BOOL; break;
            default:
                fprintf(stderr, "error: unsupported infix op\n");
                c->error = 1;
                return rhs;
            }

            ir_reg_t result = _ssa(c, rtype);
            ir_operand_t ops[2];
            ops[0] = _op_reg(lhs);
            ops[1] = _op_reg(rhs);
            _emit(c, opc, &result, 2, ops);
            return result;
        }

        fprintf(stderr, "error: unsupported operation fix\n");
        c->error = 1;
        return _ssa(c, IR_REG_NONE);
    }

    case EXPR_CALL: {
        call_t *call = e->u.call;
        /* Evaluate arguments and collect as operands */
        ir_operand_t ops[IR_MAX_OPERANDS];
        int nargs = 0;
        if (call->exprs) {
            expr_t *arg = call->exprs->head;
            while (arg && nargs < IR_MAX_OPERANDS - 1) {
                ir_reg_t argval = _expr(c, arg);
                ops[nargs] = _op_reg(argval);
                nargs++;
                arg = arg->next;
            }
        }
        /* Add callee name as last operand */
        ops[nargs] = _op_imm_str(call->callee);
        nargs++;
        /* Emit call instruction: result = call @callee %arg0 %arg1 ... */
        ir_reg_t result = _ssa(c, IR_REG_I64);
        _emit(c, IR_OPCODE_CALL, &result, nargs, ops);
        return result;
    }

    case EXPR_IF: {
        if_t *ife = &e->u.ife;
        ir_reg_t cond = _expr(c, ife->cond);
        /* Emit conditional branch */
        char then_label[64], else_label[64], end_label[64];
        snprintf(then_label, sizeof(then_label), "$if_then_%d",
                 c->fn->ssa_counter);
        snprintf(else_label, sizeof(else_label), "$if_else_%d",
                 c->fn->ssa_counter);
        snprintf(end_label, sizeof(end_label), "$if_end_%d",
                 c->fn->ssa_counter);

        ir_operand_t br_ops[3];
        br_ops[0] = _op_reg(cond);
        br_ops[1] = _op_label(then_label);
        br_ops[2] = _op_label(ife->belse ? else_label : end_label);
        _emit(c, IR_OPCODE_BR_COND, NULL, 3, br_ops);

        /* Then block */
        _fnb_add_block(c->fn, then_label);
        if (ife->bif) _inner_block(c, ife->bif);
        ir_operand_t br_op = _op_label(end_label);
        _emit(c, IR_OPCODE_BR, NULL, 1, &br_op);

        /* Else block */
        if (ife->belse) {
            _fnb_add_block(c->fn, else_label);
            _inner_block(c, ife->belse);
            br_op = _op_label(end_label);
            _emit(c, IR_OPCODE_BR, NULL, 1, &br_op);
        }

        /* End block */
        _fnb_add_block(c->fn, end_label);
        return _ssa(c, IR_REG_NONE);
    }

    case EXPR_MATCH: {
        match_t *m = &e->u.match;
        ir_reg_t cond = _expr(c, m->cond);

        /* Emit switch instruction with match arms */
        char default_label[64];
        snprintf(default_label, sizeof(default_label), "$match_default_%d",
                 c->fn->ssa_counter);

        /* Count arms */
        int narms = 0;
        match_arm_t *arm = m->block->head;
        while (arm) { narms++; arm = arm->next; }
        (void)narms;

        /* Emit switch: switch %cond, $default */
        ir_operand_t sw_ops[2];
        sw_ops[0] = _op_reg(cond);
        sw_ops[1] = _op_label(default_label);
        _emit(c, IR_OPCODE_SWITCH, NULL, 2, sw_ops);
        free(sw_ops[1].u.label);

        /* Compile each arm */
        arm = m->block->head;
        int arm_idx = 0;
        while (arm) {
            char arm_label[64];
            snprintf(arm_label, sizeof(arm_label), "$match_arm_%d_%d",
                     c->fn->ssa_counter, arm_idx);
            _fnb_add_block(c->fn, arm_label);
            if (arm->block) _inner_block(c, arm->block);

            char end_label[64];
            snprintf(end_label, sizeof(end_label), "$match_end_%d",
                     c->fn->ssa_counter);
            ir_operand_t br_op = _op_label(end_label);
            _emit(c, IR_OPCODE_BR, NULL, 1, &br_op);

            arm = arm->next;
            arm_idx++;
        }

        /* Default block */
        _fnb_add_block(c->fn, default_label);
        char end_label[64];
        snprintf(end_label, sizeof(end_label), "$match_end_%d",
                 c->fn->ssa_counter);
        ir_operand_t br_op = _op_label(end_label);
        _emit(c, IR_OPCODE_BR, NULL, 1, &br_op);

        /* End block */
        _fnb_add_block(c->fn, end_label);
        return _ssa(c, IR_REG_NONE);
    }

    case EXPR_YIELD: {
        yield_t *y = e->u.yield_expr;
        ir_reg_t val;
        if (y->expr) {
            val = _expr(c, y->expr);
        } else {
            val = _ssa(c, IR_REG_NONE);
        }

        if (c->fn->is_coro) {
            /* In a coroutine, yield is a suspension point:
             * 1. Emit YIELD (send value to output port)
             * 2. Set state to next resume point
             * 3. Return Poll::Pending (ret 0)
             * 4. Create a new block for the resume point
             */
            ir_operand_t ops[2];
            if (y->port) {
                ops[0] = _op_imm_str(y->port);
            } else {
                ops[0] = _op_imm_str("out");
            }
            ops[1] = _op_reg(val);
            _emit(c, IR_OPCODE_YIELD, NULL, 2, ops);

            /* Return Pending (0) */
            ir_operand_t ret_op = _op_imm_i32(0);
            _emit(c, IR_OPCODE_RET, NULL, 1, &ret_op);

            /* Create resume block */
            char resume_label[64];
            snprintf(resume_label, sizeof(resume_label), "S%d_resume_%d",
                     c->fn->state_counter, c->fn->nstates);
            c->fn->state_counter++;
            strncpy(c->fn->dispatch_labels[c->fn->nstates], resume_label, 63);
            c->fn->nstates++;
            _fnb_add_block(c->fn, resume_label);
        } else {
            /* In a regular function, yield is just a send */
            ir_operand_t ops[2];
            if (y->port) {
                ops[0] = _op_imm_str(y->port);
            } else {
                ops[0] = _op_imm_str("out");
            }
            ops[1] = _op_reg(val);
            _emit(c, IR_OPCODE_YIELD, NULL, 2, ops);
        }
        return val;
    }

    case EXPR_CAST: {
        cast_t *cast = e->u.cast;
        ir_reg_t val = _expr(c, cast->expr);
        ir_reg_type_t target = _type2reg(cast->type);
        ir_reg_t result = _ssa(c, target);
        ir_operand_t ops[1];
        ops[0] = _op_reg(val);
        _emit(c, IR_OPCODE_CAST, &result, 1, ops);
        return result;
    }

    case EXPR_BLOCK: {
        scope_t *child = _scope_new(c->scope);
        c->scope = child;
        ir_reg_t result;
        ir_reg_init(&result, IR_REG_NONE, NULL);
        if (e->u.block) {
            _inner_block(c, e->u.block);
        }
        c->scope = child->parent;
        _scope_free(child);
        return result;
    }

    case EXPR_LET: {
        /* let binding as expression — same as EXPR_DECL */
        decl_t *d = e->u.decl;
        ir_reg_type_t rtype = _type2reg(d->type);
        ir_reg_t result = _ssa(c, rtype);
        _scope_bind(c->scope, d->id, rtype, c->fn->ssa_counter - 1);

        if (d->init) {
            ir_reg_t init_val = _expr(c, d->init);
            ir_operand_t ops[2];
            ops[0] = _op_reg(init_val);
            ops[1] = _op_reg(result);
            _emit(c, IR_OPCODE_MOV, NULL, 2, ops);
        }
        return result;
    }

    case EXPR_MEMBER: {
        member_t *mem = &e->u.mem;
        ir_reg_t obj = _expr(c, mem->e);
        /* FIXME: resolve field index from struct type */
        ir_reg_t result = _ssa(c, IR_REG_PTR);
        ir_operand_t ops[2];
        ops[0] = _op_reg(obj);
        ops[1] = _op_imm_i32(0);  /* FIXME: field index */
        _emit(c, IR_OPCODE_GET_FIELD, &result, 1, ops);
        return result;
    }

    case EXPR_REF: {
        ref_t *ref = e->u.ref;
        ir_reg_t arr = _expr(c, ref->var);
        ir_reg_t idx = _expr(c, ref->arg);
        ir_reg_t result = _ssa(c, arr.type);
        ir_operand_t ops[2];
        ops[0] = _op_reg(arr);
        ops[1] = _op_reg(idx);
        _emit(c, IR_OPCODE_GET_ELEM, &result, 2, ops);
        return result;
    }

    case EXPR_RANGE:
        /* FIXME: ranges not yet supported */
        return _ssa(c, IR_REG_NONE);

    case EXPR_LIST:
        if (e->u.list && e->u.list->head) {
            return _expr(c, e->u.list->head);
        }
        return _ssa(c, IR_REG_NONE);

    default:
        return _ssa(c, IR_REG_NONE);
    }
}

/*======================================================================
 * Statement compilation
 *======================================================================*/

static void
_stmt(dfir_compiler_t *c, stmt_t *stmt)
{
    if (!stmt || c->error) return;

    switch (stmt->type) {
    case STMT_LET: {
        decl_t *d = stmt->u.let_decl;
        ir_reg_type_t rtype = _type2reg(d->type);
        ir_reg_t result = _ssa(c, rtype);
        _scope_bind(c->scope, d->id, rtype, c->fn->ssa_counter - 1);

        if (d->init) {
            ir_reg_t init_val = _expr(c, d->init);
            ir_operand_t ops[2];
            ops[0] = _op_reg(init_val);
            ops[1] = _op_reg(result);
            _emit(c, IR_OPCODE_MOV, NULL, 2, ops);
        }
        break;
    }

    case STMT_REASSIGN: {
        op_t *op = stmt->u.reassign;
        if (op->e0->type == EXPR_ID) {
            ir_reg_t val = _expr(c, op->e1);
            cvar_t *v = _scope_lookup(c->scope, op->e0->u.id);
            if (v) {
                ir_operand_t ops[2];
                ops[0] = _op_reg(val);
                ir_reg_t dst;
                char buf[64];
                snprintf(buf, sizeof(buf), "%%%d", v->ssa_id);
                ir_reg_init(&dst, v->type, buf);
                ops[1] = _op_reg(dst);
                _emit(c, IR_OPCODE_MOV, NULL, 2, ops);
            }
        }
        break;
    }

    case STMT_EXPR: {
        (void)_expr(c, stmt->u.expr);
        break;
    }

    case STMT_EXPR_LIST: {
        if (stmt->u.exprs) {
            expr_t *e = stmt->u.exprs->head;
            while (e) {
                (void)_expr(c, e);
                e = e->next;
            }
        }
        break;
    }

    case STMT_RETURN: {
        if (stmt->u.expr) {
            ir_reg_t val = _expr(c, stmt->u.expr);
            ir_operand_t ops[1];
            ops[0] = _op_reg(val);
            _emit(c, IR_OPCODE_RET, NULL, 1, ops);
        } else {
            _emit(c, IR_OPCODE_RET, NULL, 0, NULL);
        }
        break;
    }

    case STMT_WHILE: {
        stmt_while_t *w = &stmt->u.whilestmt;
        char cond_label[64], body_label[64], end_label[64];
        int id = c->fn->ssa_counter;
        snprintf(cond_label, sizeof(cond_label), "$while_cond_%d", id);
        snprintf(body_label, sizeof(body_label), "$while_body_%d", id);
        snprintf(end_label, sizeof(end_label), "$while_end_%d", id);

        /* Jump to condition */
        ir_operand_t br_op = _op_label(cond_label);
        _emit(c, IR_OPCODE_BR, NULL, 1, &br_op);

        /* Condition block */
        _fnb_add_block(c->fn, cond_label);
        ir_reg_t cond = _expr(c, w->cond);
        ir_operand_t br_ops[3];
        br_ops[0] = _op_reg(cond);
        br_ops[1] = _op_label(body_label);
        br_ops[2] = _op_label(end_label);
        _emit(c, IR_OPCODE_BR_COND, NULL, 3, br_ops);

        /* Body block */
        _fnb_add_block(c->fn, body_label);
        if (w->block) _inner_block(c, w->block);
        br_op = _op_label(cond_label);
        _emit(c, IR_OPCODE_BR, NULL, 1, &br_op);

        /* End block */
        _fnb_add_block(c->fn, end_label);
        break;
    }

    case STMT_FOR: {
        stmt_for_t *f = &stmt->u.forstmt;
        /* FIXME: implement for-in with iterator protocol */
        if (f->block) _inner_block(c, f->block);
        break;
    }

    case STMT_LOOP: {
        char loop_label[64], end_label[64];
        int id = c->fn->ssa_counter;
        snprintf(loop_label, sizeof(loop_label), "$loop_%d", id);
        snprintf(end_label, sizeof(end_label), "$loop_end_%d", id);

        ir_operand_t br_op = _op_label(loop_label);
        _emit(c, IR_OPCODE_BR, NULL, 1, &br_op);

        _fnb_add_block(c->fn, loop_label);
        if (stmt->u.loopblock) _inner_block(c, stmt->u.loopblock);
        br_op = _op_label(loop_label);
        _emit(c, IR_OPCODE_BR, NULL, 1, &br_op);

        _fnb_add_block(c->fn, end_label);
        break;
    }

    case STMT_BREAK:
        /* FIXME: emit br to loop end (need loop context) */
        break;

    case STMT_BLOCK: {
        scope_t *child = _scope_new(c->scope);
        c->scope = child;
        if (stmt->u.block) _inner_block(c, stmt->u.block);
        c->scope = child->parent;
        _scope_free(child);
        break;
    }

    default:
        break;
    }
}

static void
_inner_block(dfir_compiler_t *c, inner_block_t *block)
{
    if (!block || !block->stmts) return;

    stmt_t *stmt = block->stmts->head;
    while (stmt) {
        _stmt(c, stmt);
        stmt = stmt->next;
    }
}

/*======================================================================
 * Function / Coroutine compilation
 *======================================================================*/

static void
_func(dfir_compiler_t *c, func_t *fn)
{
    fnb_t *fb = _fnb_new(fn->id, IR_FUNC_FUNC);
    fb->is_coro = 0;
    c->fn = fb;

    /* Create entry block */
    _fnb_add_block(fb, "$entry");

    /* Create scope and bind arguments */
    scope_t *s = _scope_new(c->scope);
    c->scope = s;

    if (fn->args) {
        arg_t *a = fn->args->head;
        while (a) {
            if (a->decl && a->decl->id) {
                ir_reg_type_t rtype = _type2reg(a->decl->type);
                (void)_ssa(c, rtype);
                _scope_bind(s, a->decl->id, rtype, fb->ssa_counter - 1);
            }
            a = a->next;
        }
    }

    /* Bind return values */
    if (fn->rets) {
        arg_t *r = fn->rets->head;
        while (r) {
            if (r->decl && r->decl->id) {
                ir_reg_type_t rtype = _type2reg(r->decl->type);
                (void)_ssa(c, rtype);
                _scope_bind(s, r->decl->id, rtype, fb->ssa_counter - 1);
            }
            r = r->next;
        }
    }

    /* Compile body */
    if (fn->block) {
        _inner_block(c, fn->block);
    }

    /* Ensure a ret at the end */
    if (fb->cur && (fb->cur->count == 0 ||
                    (fb->cur->tail &&
                     !ir_opcode_is_terminator(fb->cur->tail->inst.opcode)))) {
        /* If function has named return values, return the first one */
        if (fn->rets && fn->rets->head && fn->rets->head->decl &&
            fn->rets->head->decl->id) {
            cvar_t *rv = _scope_lookup(s, fn->rets->head->decl->id);
            if (rv) {
                ir_operand_t ops[1];
                ir_reg_t reg;
                char buf[64];
                snprintf(buf, sizeof(buf), "%%%d", rv->ssa_id);
                ir_reg_init(&reg, rv->type, buf);
                ops[0] = _op_reg(reg);
                _emit(c, IR_OPCODE_RET, NULL, 1, ops);
            } else {
                _emit(c, IR_OPCODE_RET, NULL, 0, NULL);
            }
        } else {
            _emit(c, IR_OPCODE_RET, NULL, 0, NULL);
        }
    }

    /* Transfer blocks to ir_func_t */
    ir_func_t *irf = ir_func_new();
    irf->name = strdup(fn->id);
    irf->type = IR_FUNC_FUNC;

    bb_t *b = fb->blocks;
    while (b) {
        ir_block_t *block = ir_block_new(b->label);
        /* Transfer instructions */
        ir_instr_ent_t *e = b->head;
        while (e) {
            ir_instr_ent_t *next = e->next;
            /* Append to block (transfer ownership) */
            if (block->instrs == NULL) {
                block->instrs = e;
            } else {
                block->last->next = e;
            }
            block->last = e;
            e->next = NULL;
            block->ninstr++;
            e = next;
        }
        /* Clear bb head/tail so _bb_free won't free the instructions */
        b->head = NULL;
        b->tail = NULL;
        b->count = 0;

        ir_func_add_block(irf, block);
        /* ir_func_add_block copies the struct by value; free only the wrapper */
        free(block);

        b = b->next;
    }
    fb->blocks = NULL;
    fb->cur = NULL;

    ir_object_add_func(c->ir, irf);
    c->scope = s->parent;
    _scope_free(s);
    _fnb_free(fb);
    c->fn = NULL;
}

static void
_coroutine(dfir_compiler_t *c, coroutine_t *cr)
{
    fnb_t *fb = _fnb_new(cr->id, IR_FUNC_COROUTINE);
    fb->is_coro = 1;
    fb->state_counter = 0;
    fb->nstates = 0;
    c->fn = fb;

    /* Create scope and bind arguments */
    scope_t *s = _scope_new(c->scope);
    c->scope = s;

    /* First SSA value (%0) is the state variable */
    ir_reg_t state_reg = _ssa(c, IR_REG_I32);
    fb->state_var_ssa = fb->ssa_counter - 1;
    _scope_bind(s, "__state", IR_REG_I32, fb->state_var_ssa);

    /* Bind regular arguments (starting from %1) */
    if (cr->args) {
        arg_t *a = cr->args->head;
        while (a) {
            if (a->decl && a->decl->id) {
                ir_reg_type_t rtype = _type2reg(a->decl->type);
                (void)_ssa(c, rtype);
                _scope_bind(s, a->decl->id, rtype, fb->ssa_counter - 1);
            }
            a = a->next;
        }
    }

    /* Bind return values */
    if (cr->rets) {
        arg_t *r = cr->rets->head;
        while (r) {
            if (r->decl && r->decl->id) {
                ir_reg_type_t rtype = _type2reg(r->decl->type);
                (void)_ssa(c, rtype);
                _scope_bind(s, r->decl->id, rtype, fb->ssa_counter - 1);
            }
            r = r->next;
        }
    }

    /* Create the dispatch block (entry point) */
    _fnb_add_block(fb, "S_dispatch");
    /* State 0 is the start state */
    strncpy(fb->dispatch_labels[0], "S0_start", 63);
    fb->nstates = 1;

    /* Create the start state block */
    _fnb_add_block(fb, "S0_start");

    /* Compile body */
    if (cr->block) {
        _inner_block(c, cr->block);
    }

    /* Ensure ret at the end — return Poll::Ready (1) */
    if (fb->cur && (!fb->cur->tail ||
                    !ir_opcode_is_terminator(fb->cur->tail->inst.opcode))) {
        /* Return Ready (1) */
        ir_operand_t ret_op = _op_imm_i32(1);
        _emit(c, IR_OPCODE_RET, NULL, 1, &ret_op);
    }

    /* Now build the dispatch chain in S_dispatch */
    /* Go back to the dispatch block and emit compare-and-branch chain */
    bb_t *dispatch = fb->blocks;  /* first block is S_dispatch */
    bb_t *saved_cur = fb->cur;
    fb->cur = dispatch;

    /* For each state (except 0), emit: if %state == i then br S{i}_label */
    for (int i = 1; i < fb->nstates; i++) {
        /* %cmp = cmp_eq %state, i */
        ir_reg_t cmp_result = _ssa(c, IR_REG_BOOL);
        ir_operand_t cmp_ops[2];
        cmp_ops[0] = _op_reg(state_reg);
        cmp_ops[1] = _op_imm_i32(i);
        _emit(c, IR_OPCODE_CMP_EQ, &cmp_result, 2, cmp_ops);

        /* br_cond %cmp, $state_label, $fallthrough */
        /* Fall through to next comparison or default */
        ir_operand_t br_ops[3];
        br_ops[0] = _op_reg(cmp_result);
        br_ops[1] = _op_label(fb->dispatch_labels[i]);
        if (i + 1 < fb->nstates) {
            /* Fall through to next comparison block */
            char next_label[64];
            snprintf(next_label, sizeof(next_label), "$dispatch_%d", i + 1);
            br_ops[2] = _op_label(next_label);
            _emit(c, IR_OPCODE_BR_COND, NULL, 3, br_ops);
            _fnb_add_block(fb, next_label);
        } else {
            /* Last comparison: fall through to default (S0_start) */
            br_ops[2] = _op_label(fb->dispatch_labels[0]);
            _emit(c, IR_OPCODE_BR_COND, NULL, 3, br_ops);
        }
    }

    /* If only 1 state (no yields), just branch to S0_start */
    if (fb->nstates <= 1) {
        ir_operand_t br_op = _op_label(fb->dispatch_labels[0]);
        _emit(c, IR_OPCODE_BR, NULL, 1, &br_op);
    }

    fb->cur = saved_cur;

    /* Transfer to ir_func_t */
    ir_func_t *irf = ir_func_new();
    irf->name = strdup(cr->id);
    irf->type = IR_FUNC_COROUTINE;

    bb_t *b = fb->blocks;
    while (b) {
        ir_block_t *block = ir_block_new(b->label);
        ir_instr_ent_t *e = b->head;
        while (e) {
            ir_instr_ent_t *next = e->next;
            if (block->instrs == NULL) {
                block->instrs = e;
            } else {
                block->last->next = e;
            }
            block->last = e;
            e->next = NULL;
            block->ninstr++;
            e = next;
        }
        b->head = NULL;
        b->tail = NULL;
        b->count = 0;

        ir_func_add_block(irf, block);
        /* ir_func_add_block copies the struct by value; free only the wrapper */
        free(block);

        b = b->next;
    }
    fb->blocks = NULL;
    fb->cur = NULL;

    ir_object_add_func(c->ir, irf);

    c->scope = s->parent;
    _scope_free(s);
    _fnb_free(fb);
    c->fn = NULL;
}

static void
_directive(dfir_compiler_t *c, directive_t *dr)
{
    (void)c;
    (void)dr;
    /* FIXME: register struct/enum/type alias definitions */
}

/*
 * _graph -- compile a graph declaration to DFIR graph IR
 *
 * Converts a pipe chain (source |> map(f) |> sink) into an ir_graph_t
 * with nodes and edges.
 */
static void
_graph(dfir_compiler_t *c, graph_decl_t *gd)
{
    ir_graph_t *graph = ir_graph_new(gd->id);
    if (!graph) return;

    /* Walk the pipe chain and create nodes + edges */
    graph_node_ref_t *node = gd->nodes;
    int node_idx = 0;
    char prev_name[64] = "";

    while (node) {
        char node_name[64];
        char func_ref[256];
        const char *port_names[] = {"in", "out"};

        snprintf(node_name, sizeof(node_name), "%%n%d", node_idx);

        /* Build function reference: @name or @name(args) */
        if (node->args && node->args->head) {
            /* For source/sink with string arguments, include the arg */
            expr_t *arg = node->args->head;
            if (arg->type == EXPR_LITERAL && arg->u.lit &&
                arg->u.lit->type == LIT_STRING && arg->u.lit->u.s) {
                snprintf(func_ref, sizeof(func_ref), "@%s(\"%s\")",
                         node->name, arg->u.lit->u.s);
            } else {
                snprintf(func_ref, sizeof(func_ref), "@%s", node->name);
            }
        } else {
            snprintf(func_ref, sizeof(func_ref), "@%s", node->name);
        }

        /* Add node: source/sink have 1 port, others have 2 (in, out) */
        int is_source = (strcmp(node->name, "source") == 0);
        int is_sink = (strcmp(node->name, "sink") == 0);
        if (is_source) {
            const char *ports[] = {"out"};
            ir_graph_add_node(graph, node_name, func_ref, 1, ports);
        } else if (is_sink) {
            const char *ports[] = {"in"};
            ir_graph_add_node(graph, node_name, func_ref, 1, ports);
        } else {
            ir_graph_add_node(graph, node_name, func_ref, 2, port_names);
        }

        /* Add edge from previous node to this one */
        if (node_idx > 0 && prev_name[0]) {
            /* Determine ports based on node types */
            const char *src_port = "out";
            const char *dst_port = "in";
            /* If previous was source, it only has "out" */
            /* If this is sink, it only has "in" */
            ir_graph_add_edge(graph, prev_name, src_port,
                             node_name, dst_port,
                             IR_REG_I32, 128);
        }

        strncpy(prev_name, node_name, sizeof(prev_name) - 1);
        node = node->next;
        node_idx++;
    }

    /* Add graph to IR object */
    ir_object_add_graph(c->ir, graph);
}

/*======================================================================
 * Top-level compilation
 *======================================================================*/

/*
 * Note: The minica_compile function is defined in compile.c.
 * This file provides the _func, _coroutine, _directive implementations.
 * To use this file instead of compile.c, replace minica_compile in compile.c
 * with a wrapper that calls the functions here.
 */

/*
 * compile_to_dfir — compile a syntax tree to DFIR
 *
 * This is the main entry point. It walks the AST and produces an ir_object_t
 * containing all functions, coroutines, and directives as DFIR.
 */
ir_object_t *
compile_to_dfir(st_t *st)
{
    dfir_compiler_t c;
    memset(&c, 0, sizeof(c));
    c.ir = ir_object_new();
    if (!c.ir) return NULL;

    c.scope = _scope_new(NULL);

    /* Walk outer block */
    outer_block_t *ob = st->block;
    if (ob) {
        outer_block_entry_t *e = ob->head;
        while (e) {
            switch (e->type) {
            case OUTER_BLOCK_FUNC:
                _func(&c, e->u.fn);
                break;
            case OUTER_BLOCK_COROUTINE:
                _coroutine(&c, e->u.cr);
                break;
            case OUTER_BLOCK_DIRECTIVE:
                _directive(&c, e->u.dr);
                break;
            case OUTER_BLOCK_GRAPH:
                _graph(&c, e->u.graph);
                break;
            }
            e = e->next;
        }
    }

    _scope_free(c.scope);

    if (c.error) {
        fprintf(stderr, "Compilation completed with errors\n");
    }

    return c.ir;
}
