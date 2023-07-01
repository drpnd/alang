/*_
 * Copyright (c) 2021-2023 Hirochika Asai <asai@jar.jp>
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

#include "../compile.h"
#include "../minica.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/*
 * usage -- print usage and exit
 */
void
usage(const char *prog)
{
    fprintf(stderr, "Usage: %s <file>\n", prog);
    exit(EXIT_FAILURE);
}

/* Declarations */
static void _display_val(compiler_env_t *, compiler_val_t *);

static int
_is_reg(operand_t *op)
{
    if ( OPERAND_VAL == op->type ) {
        if ( VAL_REG == op->u.val->type ) {
            return 1;
        }
    }
    return 0;
}

static void
_analyze_operand(compiler_env_t *env, operand_t *operand, compiler_ig_t *ig)
{
    if ( _is_reg(operand) ) {
        /* Register */
    }
    if ( OPERAND_VAL == operand->type ) {
        if ( operand->u.val->opt.id < 0 ) {
            operand->u.val->opt.id = ++env->opt.max_var_id;
        }
        if ( NULL != ig ) {
            ig->v.vals[operand->u.val->opt.id] = operand->u.val;
        }
    }
}

static void
_analyze_instruction(compiler_env_t *env, compiler_instr_t *instr,
                     compiler_ig_t *ig)
{
    switch ( instr->ir.opcode ) {
    case IR_OPCODE_MOV:
        _analyze_operand(env, &instr->operands[0], ig);
        _analyze_operand(env, &instr->operands[1], ig);
        break;
    case IR_OPCODE_ADD:
        _analyze_operand(env, &instr->operands[0], ig);
        _analyze_operand(env, &instr->operands[1], ig);
        _analyze_operand(env, &instr->operands[2], ig);
        break;
    case IR_OPCODE_SUB:
        _analyze_operand(env, &instr->operands[0], ig);
        _analyze_operand(env, &instr->operands[1], ig);
        _analyze_operand(env, &instr->operands[2], ig);
        break;
    case IR_OPCODE_MUL:
        _analyze_operand(env, &instr->operands[0], ig);
        _analyze_operand(env, &instr->operands[1], ig);
        _analyze_operand(env, &instr->operands[2], ig);
        break;
    case IR_OPCODE_DIV:
        _analyze_operand(env, &instr->operands[0], ig);
        _analyze_operand(env, &instr->operands[1], ig);
        _analyze_operand(env, &instr->operands[2], ig);
        break;
    case IR_OPCODE_INC:
        _analyze_operand(env, &instr->operands[0], ig);
        break;
    case IR_OPCODE_DEC:
        _analyze_operand(env, &instr->operands[0], ig);
        break;
    default:
        ;
    }
}

static void
_analyze_registers(compiler_env_t *env)
{
    compiler_instr_t *instr;

    /* Count the number of values (registers) */
    instr = env->code.head;
    while ( NULL != instr ) {
        _analyze_instruction(env, instr, NULL);
        instr = instr->next;
    }

    printf("max_var_id: %d\n", env->opt.max_var_id);

    /* Build an interference graph */
    compiler_ig_t ig;
    ig.v.n = env->opt.max_var_id;
    ig.v.vals = malloc(sizeof(compiler_val_t *) * ig.v.n);
    if ( NULL == ig.v.vals ) {
        return;
    }
    /* Register to ID */
    instr = env->code.head;
    while ( NULL != instr ) {
        _analyze_instruction(env, instr, &ig);
        instr = instr->next;
    }
}

static void
_display_literal(literal_t *lit)
{
    switch ( lit->type ) {
    case LIT_HEXINT:
        printf("0x%s", lit->u.n);
        break;
    case LIT_DECINT:
        printf("%s", lit->u.n);
        break;
    case LIT_OCTINT:
        printf("0%s", lit->u.n);
        break;
    case LIT_FLOAT:
        printf("%s", lit->u.n);
        break;
    case LIT_STRING:
        printf("%s", lit->u.s);
        break;
    case LIT_BOOL:
        printf("%s", lit->u.b == BOOL_TRUE ? "true" : "false");
        break;
    case LIT_NIL:
        printf("nil");
        break;
    }
}

static void
_display_val_list(compiler_env_t *env, compiler_val_list_t *list)
{
    compiler_val_t *cur;

    cur = list->head;
    while ( cur != NULL ) {
        _display_val(env, cur);
        cur = cur->next;
    }
}

static void
_display_val(compiler_env_t *env, compiler_val_t *val)
{
    switch ( val->type ) {
    case VAL_NIL:
        printf("nil");
        break;
    case VAL_VAR:
        /* Variable */
        printf("[var]");
        break;
    case VAL_LITERAL:
        _display_literal(val->u.lit);
        break;
    case VAL_REG:
        printf("%%");
        break;
    case VAL_REG_SET:
        printf("(%%,%%)");
        break;
    case VAL_LIST:
        _display_val_list(env, val->u.list);
        break;
    case VAL_COND:
        printf("[cond]");
        break;
    }
}

static void
_display_operand(compiler_env_t *env, operand_t *operand)
{
    printf(" ");
    switch ( operand->type ) {
    case OPERAND_VAL:
        _display_val(env, operand->u.val);
        break;
    case OPERAND_REF:
        printf("[ref]");
        break;
    case OPERAND_I8:
    case OPERAND_I16:
    case OPERAND_I32:
    case OPERAND_I64:
    case OPERAND_FP32:
    case OPERAND_FP64:
        printf("[imm]");
        break;
    }
}

static void
_display_instr(compiler_env_t *env, compiler_instr_t *instr)
{
    switch ( instr->ir.opcode ) {
    case IR_OPCODE_MOV:
        printf("mov");
        _display_operand(env, &instr->operands[0]);
        _display_operand(env, &instr->operands[1]);
        break;
    case IR_OPCODE_ADD:
        printf("add");
        _display_operand(env, &instr->operands[0]);
        _display_operand(env, &instr->operands[1]);
        _display_operand(env, &instr->operands[2]);
        break;
    case IR_OPCODE_SUB:
        printf("sub");
        _display_operand(env, &instr->operands[0]);
        _display_operand(env, &instr->operands[1]);
        _display_operand(env, &instr->operands[2]);
        break;
    case IR_OPCODE_MUL:
        printf("mul");
        _display_operand(env, &instr->operands[0]);
        _display_operand(env, &instr->operands[1]);
        _display_operand(env, &instr->operands[2]);
        break;
    case IR_OPCODE_DIV:
        printf("div");
        _display_operand(env, &instr->operands[0]);
        _display_operand(env, &instr->operands[1]);
        _display_operand(env, &instr->operands[2]);
        break;
    case IR_OPCODE_MOD:
        printf("mod");
        _display_operand(env, &instr->operands[0]);
        _display_operand(env, &instr->operands[1]);
        _display_operand(env, &instr->operands[2]);
        break;
    case IR_OPCODE_INC:
        printf("inc\n");
        break;
    case IR_OPCODE_DEC:
        printf("dec\n");
        break;
    default:
        printf("opcode %d\n", instr->ir.opcode);
    }
    printf("\n");
}

static void
_display_env(compiler_env_t *env)
{
    compiler_var_t *var;
    compiler_instr_t *instr;

    /* Variables */
    printf("Printing variables:\n");
    var = env->vars->top;
    while ( NULL != var ) {
        printf("var: %s\n", var->id);
        var = var->next;
    }

    /* Analyze registers */
    printf("Analyzing registers\n");
    _analyze_registers(env);

    /* Code */
    printf("Printing code:\n");
    instr = env->code.head;
    while ( NULL != instr ) {
        _display_instr(env, instr);
        instr = instr->next;
    }
}

static void
_display_code(compiler_block_t *blocks)
{
    compiler_block_t *b;

    b = blocks;
    while ( NULL != b ) {
        switch ( b->type ) {
        case BLOCK_FUNC:
            printf("fn %s\n", b->label);
            _display_env(b->env);
            break;
        case BLOCK_COROUTINE:
            printf("coroutine %s\n", b->label);
            _display_env(b->env);
            break;
        }
        b = b->next;
    }
}

/*
 * Main routine for the parser test
 */
int
main(int argc, const char *const argv[])
{
    FILE *fp;
    st_t *code;
    compiler_t *c;

    if ( argc < 2 ) {
        fp = stdin;
        /* stdio is not supported. */
        usage(argv[0]);
    } else {
        /* Open the specified file */
        fp = fopen(argv[1], "r");
        if ( NULL == fp ) {
            perror("fopen");
            exit(EXIT_FAILURE);
        }
    }

    /* Parse the specified file */
    code = minica_parse(fp);
    if ( NULL == code ) {
        exit(EXIT_FAILURE);
    }

    /* Try to compile the code */
    c = compile(code);
    if ( NULL == c ) {
        fprintf(stderr, "Failed to compile the code.\n");
        return EXIT_FAILURE;
    }

    /* Print out the compiled code */
    printf("Print out the compiled code:\n");
    _display_code(c->blocks);

    return EXIT_SUCCESS;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
