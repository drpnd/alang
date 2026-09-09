/*_
 * Copyright (c) 2024-2026 Hirochika Asai <asai@jar.jp>
 * All rights reserved.
 * MIT License
 */

/*
 * ir_debug.c — DFIR pretty-printer
 *
 * Prints an ir_object_t in human-readable DFIR textual format,
 * including functions, coroutines, and graphs with all operands.
 */

#include "ir.h"
#include <stdio.h>
#include <string.h>

/*======================================================================
 * Opcode names
 *======================================================================*/

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

/*======================================================================
 * Register type names
 *======================================================================*/

static const char *
reg_type_name(ir_reg_type_t t)
{
    switch (t) {
    case IR_REG_NONE:  return "none";
    case IR_REG_PTR:   return "ptr";
    case IR_REG_I8:    return "i8";
    case IR_REG_I16:   return "i16";
    case IR_REG_I32:   return "i32";
    case IR_REG_I64:   return "i64";
    case IR_REG_F16:   return "f16";
    case IR_REG_F32:   return "f32";
    case IR_REG_F64:   return "f64";
    case IR_REG_FP8:   return "fp8";
    case IR_REG_FP4:   return "fp4";
    case IR_REG_BOOL:  return "bool";
    case IR_REG_STR:   return "str";
    case IR_REG_VOID:  return "void";
    default:           return "?";
    }
}

/*======================================================================
 * Operand printer
 *======================================================================*/

static void
print_operand(ir_operand_t *op)
{
    if (!op) { printf("<null>"); return; }

    switch (op->type) {
    case IR_OPERAND_REG:
        printf("%s", op->u.reg.id ? op->u.reg.id : "%?");
        break;

    case IR_OPERAND_IMM:
        switch (op->u.imm.type) {
        case IR_IMM_I8:   case IR_IMM_S8:
            printf("%d", (int)op->u.imm.u.s8); break;
        case IR_IMM_I16:  case IR_IMM_S16:
            printf("%d", (int)op->u.imm.u.s16); break;
        case IR_IMM_I32:  case IR_IMM_S32:
            printf("%d", (int)op->u.imm.u.s32); break;
        case IR_IMM_I64:  case IR_IMM_S64:
            printf("%lld", (long long)op->u.imm.u.s64); break;
        case IR_IMM_F32:
            printf("%g", (double)op->u.imm.u.f32); break;
        case IR_IMM_F64:
            printf("%g", op->u.imm.u.f64); break;
        case IR_IMM_BOOL:
            printf("%s", op->u.imm.u.bval ? "true" : "false"); break;
        case IR_IMM_STR:
            printf("\"%s\"", op->u.imm.u.str ? op->u.imm.u.str : ""); break;
        default:
            printf("<imm>"); break;
        }
        break;

    case IR_OPERAND_LABEL:
        printf("%s", op->u.label ? op->u.label : "<label>");
        break;

    case IR_OPERAND_REF:
        printf("<ref>");
        break;

    default:
        printf("<op>");
        break;
    }
}

/*======================================================================
 * Instruction printer
 *======================================================================*/

static void
print_instr(ir_instr_t *inst, int indent)
{
    /* Indent */
    for (int i = 0; i < indent; i++) printf(" ");

    /* Result register(s) */
    if (inst->result.n > 0 && inst->result.reg[0].id) {
        printf("%s : %s = ", inst->result.reg[0].id,
               reg_type_name(inst->result.reg[0].type));
    }

    /* Opcode */
    printf("%s", opcode_name(inst->opcode));

    /* Operands */
    for (int i = 0; i < inst->noperands && i < IR_MAX_OPERANDS; i++) {
        if (i == 0) printf(" ");
        else printf(", ");
        print_operand(&inst->operands[i]);
    }

    printf("\n");
}

/*======================================================================
 * Block printer
 *======================================================================*/

static void
print_block(ir_block_t *blk, int indent)
{
    for (int i = 0; i < indent; i++) printf(" ");
    printf("%s:  (%zu instructions)\n",
           (blk->label && blk->label->name) ? blk->label->name : "<unnamed>",
           blk->ninstr);

    ir_instr_ent_t *ent = blk->instrs;
    while (ent) {
        print_instr(&ent->inst, indent + 2);
        ent = ent->next;
    }
    printf("\n");
}

/*======================================================================
 * Function/coroutine printer
 *======================================================================*/

static void
print_func(ir_func_t *func)
{
    printf("  %s @%s", func->type == IR_FUNC_COROUTINE ? "coro" : "func",
           func->name ? func->name : "<anonymous>");

    if (func->nargs > 0) printf("  ; nargs=%d", func->nargs);
    if (func->nrets > 0) printf("  ; nrets=%d", func->nrets);
    printf("  (%zu blocks)\n", func->nblocks);
    printf("  {\n");

    for (size_t i = 0; i < func->nblocks; i++) {
        print_block(&func->blocks[i], 4);
    }

    printf("  }\n\n");
}

/*======================================================================
 * Graph printer
 *======================================================================*/

static void
print_graph(ir_graph_t *g)
{
    printf("  graph @%s  (%zu nodes, %zu edges)\n",
           g->name ? g->name : "<anonymous>", g->nnodes, g->nedges);
    printf("  {\n");

    for (size_t i = 0; i < g->nnodes; i++) {
        printf("    node %s = %s", g->nodes[i].name, g->nodes[i].func_ref);
        if (g->nodes[i].nports > 0) {
            printf("  [");
            for (int j = 0; j < g->nodes[i].nports; j++) {
                if (j > 0) printf(", ");
                printf("%s", g->nodes[i].port_names[j]);
            }
            printf("]");
        }
        printf("\n");
    }

    for (size_t i = 0; i < g->nedges; i++) {
        printf("    edge %s.%s -> %s.%s  : chan<%s, %d>\n",
               g->edges[i].src_node, g->edges[i].src_port,
               g->edges[i].dst_node, g->edges[i].dst_port,
               reg_type_name(g->edges[i].elem_type),
               g->edges[i].bufsize);
    }

    printf("  }\n\n");
}

/*======================================================================
 * Main entry point
 *======================================================================*/

/*
 * ir_print_code -- print the IR object in DFIR textual format
 */
int
ir_print_code(ir_object_t *obj)
{
    if (!obj) return -1;

    printf("module @program {\n\n");

    /* Functions and coroutines */
    ir_func_t *func = obj->funcs;
    while (func) {
        print_func(func);
        func = func->next;
    }

    /* Graphs */
    for (size_t i = 0; i < obj->ngraphs; i++) {
        print_graph(&obj->graphs[i]);
    }

    /* Data sections */
    if (obj->data.n > 0) {
        printf("  ; Data sections: %zu entries\n", obj->data.n);
    }

    printf("}\n");
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
