/*_
 * Copyright (c) 2024-2026 Hirochika Asai <asai@jar.jp>
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
 * Test: parse a source file, compile to DFIR, and print the IR
 */

#include "../minica.h"
#include "../syntax.h"
#include "../ir.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* From compiler.c */
extern ir_object_t *compile_to_dfir(st_t *st);

static void
usage(const char *prog)
{
    fprintf(stderr, "Usage: %s <file>\n", prog);
    exit(EXIT_FAILURE);
}

static const char *
opcode_name(ir_opcode_t opc)
{
    switch (opc) {
    case IR_OPCODE_CONST:           return "const";
    case IR_OPCODE_PHI:             return "phi";
    case IR_OPCODE_ALLOCA:          return "alloca";
    case IR_OPCODE_LOAD:            return "load";
    case IR_OPCODE_STORE:           return "store";
    case IR_OPCODE_MOV:             return "mov";
    case IR_OPCODE_MEMCPY:          return "memcpy";
    case IR_OPCODE_ADD:             return "add";
    case IR_OPCODE_SUB:             return "sub";
    case IR_OPCODE_MUL:             return "mul";
    case IR_OPCODE_DIV:             return "div";
    case IR_OPCODE_UDIV:            return "udiv";
    case IR_OPCODE_MOD:             return "mod";
    case IR_OPCODE_UREM:            return "urem";
    case IR_OPCODE_NEG:             return "neg";
    case IR_OPCODE_CMP_EQ:          return "eq";
    case IR_OPCODE_CMP_NE:          return "ne";
    case IR_OPCODE_CMP_LT:          return "lt";
    case IR_OPCODE_CMP_LE:          return "le";
    case IR_OPCODE_CMP_GT:          return "gt";
    case IR_OPCODE_CMP_GE:          return "ge";
    case IR_OPCODE_NOT:             return "not";
    case IR_OPCODE_AND:             return "and";
    case IR_OPCODE_OR:              return "or";
    case IR_OPCODE_XOR:             return "xor";
    case IR_OPCODE_SHL:             return "shl";
    case IR_OPCODE_SHR:             return "shr";
    case IR_OPCODE_CAST:            return "cast";
    case IR_OPCODE_MAKE_STRUCT:     return "make_struct";
    case IR_OPCODE_GET_FIELD:       return "get_field";
    case IR_OPCODE_SET_FIELD:       return "set_field";
    case IR_OPCODE_GET_ELEM:        return "get_elem";
    case IR_OPCODE_MAKE_ENUM:       return "make_enum";
    case IR_OPCODE_EXTRACT_VARIANT: return "extract_variant";
    case IR_OPCODE_CHECK_VARIANT:   return "check_variant";
    case IR_OPCODE_BR:              return "br";
    case IR_OPCODE_BR_COND:         return "br_cond";
    case IR_OPCODE_SWITCH:          return "switch";
    case IR_OPCODE_RET:             return "ret";
    case IR_OPCODE_CALL:            return "call";
    case IR_OPCODE_RECV:            return "recv";
    case IR_OPCODE_SEND:            return "send";
    case IR_OPCODE_YIELD:           return "yield";
    case IR_OPCODE_AWAIT:           return "await";
    case IR_OPCODE_SUSPEND:         return "suspend";
    default:                        return "<unknown>";
    }
}

static void
print_operand(ir_operand_t *op)
{
    switch (op->type) {
    case IR_OPERAND_REG:
        printf("%s", op->u.reg.id ? op->u.reg.id : "%?");
        break;
    case IR_OPERAND_IMM:
        switch (op->u.imm.type) {
        case IR_IMM_I32:
        case IR_IMM_S32:  printf("%d", (int)op->u.imm.u.s32); break;
        case IR_IMM_I64:
        case IR_IMM_S64:  printf("%lld", (long long)op->u.imm.u.s64); break;
        case IR_IMM_F64:  printf("%g", op->u.imm.u.f64); break;
        case IR_IMM_BOOL: printf("%s", op->u.imm.u.bval ? "true" : "false"); break;
        case IR_IMM_STR:  printf("\"%s\"", op->u.imm.u.str ? op->u.imm.u.str : ""); break;
        default:          printf("<imm>"); break;
        }
        break;
    case IR_OPERAND_LABEL:
        printf("%s", op->u.label ? op->u.label : "<label>");
        break;
    default:
        printf("<op>");
        break;
    }
}

static void
print_dfir(ir_object_t *ir)
{
    int tests_run = 0;
    int tests_passed = 0;

    printf("=== DFIR Output ===\n\n");
    printf("module @program {\n\n");

    ir_func_t *f = ir->funcs;
    while (f) {
        printf("  %s @%s {\n",
               f->type == IR_FUNC_COROUTINE ? "coro" : "func", f->name);

        for (size_t i = 0; i < f->nblocks; i++) {
            ir_block_t *b = &f->blocks[i];
            printf("    %s:\n", b->label ? b->label->name : "<unnamed>");

            ir_instr_ent_t *e = b->instrs;
            while (e) {
                printf("      ");
                if (e->inst.result.n > 0 && e->inst.result.reg[0].id) {
                    printf("%s = ", e->inst.result.reg[0].id);
                }
                printf("%s", opcode_name(e->inst.opcode));
                for (int j = 0; j < e->inst.noperands && j < IR_MAX_OPERANDS; j++) {
                    printf(" ");
                    print_operand(&e->inst.operands[j]);
                }
                printf("\n");
                e = e->next;
            }
            printf("\n");
        }

        printf("  }\n\n");
        f = f->next;
    }

    printf("}\n");

    /* Basic assertions */
    tests_run++;
    if (ir->nfuncs > 0) {
        tests_passed++;
        printf("[PASS] IR object has functions (%zu)\n", ir->nfuncs);
    } else {
        printf("[FAIL] IR object has no functions\n");
    }

    tests_run++;
    f = ir->funcs;
    if (f && f->nblocks > 0) {
        tests_passed++;
        printf("[PASS] First function has blocks (%zu)\n", f->nblocks);
    } else {
        printf("[FAIL] First function has no blocks\n");
    }

    tests_run++;
    f = ir->funcs;
    if (f && f->blocks[0].ninstr > 0) {
        tests_passed++;
        printf("[PASS] First block has instructions (%zu)\n",
               f->blocks[0].ninstr);
    } else {
        printf("[FAIL] First block has no instructions\n");
    }

    printf("\n=== Results: %d/%d passed ===\n", tests_passed, tests_run);
}

int
main(int argc, const char *const argv[])
{
    FILE *fp;
    st_t *st;
    ir_object_t *ir;

    if (argc < 2) {
        usage(argv[0]);
    }

    fp = fopen(argv[1], "r");
    if (!fp) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    /* Parse */
    st = minica_parse(fp);
    fclose(fp);
    if (!st) {
        fprintf(stderr, "Parse error\n");
        exit(EXIT_FAILURE);
    }

    /* Print AST for reference */
    syntax_print_ast(st);
    printf("\n");

    /* Compile to DFIR */
    ir = compile_to_dfir(st);
    if (!ir) {
        fprintf(stderr, "Compilation error\n");
        exit(EXIT_FAILURE);
    }

    /* Print DFIR */
    print_dfir(ir);

    return EXIT_SUCCESS;
}
