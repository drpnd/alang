/*_
 * Copyright (c) 2022-2024,2026 Hirochika Asai <asai@jar.jp>
 * All rights reserved.
 * MIT License
 */

#include "ir.h"
#include <stdlib.h>
#include <string.h>

ir_object_t *ir_object_new(void) {
    ir_object_t *obj = malloc(sizeof(ir_object_t));
    if (obj) memset(obj, 0, sizeof(ir_object_t));
    return obj;
}

ir_func_t *ir_func_new(void) {
    ir_func_t *f = malloc(sizeof(ir_func_t));
    if (f) memset(f, 0, sizeof(ir_func_t));
    return f;
}

ir_graph_t *ir_graph_new(const char *name) {
    ir_graph_t *g = malloc(sizeof(ir_graph_t));
    if (g) { memset(g, 0, sizeof(ir_graph_t)); if (name) g->name = strdup(name); }
    return g;
}

ir_block_t *ir_block_new(const char *label_name) {
    ir_block_t *b = malloc(sizeof(ir_block_t));
    if (b) {
        memset(b, 0, sizeof(ir_block_t));
        if (label_name) {
            b->label = malloc(sizeof(ir_label_t));
            if (b->label) {
                b->label->name = strdup(label_name);
                b->label->block = b;
            }
        }
    }
    return b;
}

ir_instr_t *ir_instr_new(void) {
    ir_instr_t *i = malloc(sizeof(ir_instr_t));
    if (i) memset(i, 0, sizeof(ir_instr_t));
    return i;
}

void ir_instr_delete(ir_instr_t *i) { free(i); }

ir_instr_ent_t *ir_instr_ent_new(void) {
    ir_instr_ent_t *e = malloc(sizeof(ir_instr_ent_t));
    if (e) memset(e, 0, sizeof(ir_instr_ent_t));
    return e;
}

void ir_instr_ent_delete(ir_instr_ent_t *e) { free(e); }

ir_reg_t *ir_reg_init(ir_reg_t *reg, ir_reg_type_t type, const char *id) {
    if (!reg) return NULL;
    reg->type = type;
    reg->id = id ? strdup(id) : NULL;
    return reg;
}

void ir_reg_release(ir_reg_t *reg) {
    if (reg && reg->id) { free(reg->id); reg->id = NULL; }
}

ir_imm_t *ir_imm_init(ir_imm_t *imm, ir_imm_type_t type) {
    if (!imm) return NULL;
    memset(imm, 0, sizeof(ir_imm_t));
    imm->type = type;
    return imm;
}

void ir_imm_release(ir_imm_t *imm) {
    if (!imm) return;
    if (imm->type == IR_IMM_STR && imm->u.str) { free(imm->u.str); imm->u.str = NULL; }
}

ir_operand_t *ir_operand_new(void) {
    ir_operand_t *o = malloc(sizeof(ir_operand_t));
    if (o) memset(o, 0, sizeof(ir_operand_t));
    return o;
}

void ir_operand_delete(ir_operand_t *o) { free(o); }

int ir_object_add_func(ir_object_t *obj, ir_func_t *func) {
    if (!obj || !func) return -1;
    if (!obj->funcs) { obj->funcs = func; }
    else { ir_func_t *t = obj->funcs; while (t->next) t = t->next; t->next = func; }
    obj->nfuncs++;
    return 0;
}

int ir_object_add_graph(ir_object_t *obj, ir_graph_t *graph) {
    if (!obj || !graph) return -1;
    if (obj->ngraphs >= obj->graphs_cap) {
        size_t nc = obj->graphs_cap ? obj->graphs_cap * 2 : 8;
        ir_graph_t *na = realloc(obj->graphs, nc * sizeof(ir_graph_t));
        if (!na) return -1;
        obj->graphs = na; obj->graphs_cap = nc;
    }
    obj->graphs[obj->ngraphs++] = *graph;
    return 0;
}

int ir_func_add_block(ir_func_t *func, ir_block_t *block) {
    if (!func || !block) return -1;
    ir_block_t *nb = realloc(func->blocks, (func->nblocks + 1) * sizeof(ir_block_t));
    if (!nb) return -1;
    func->blocks = nb;
    func->blocks[func->nblocks++] = *block;
    return 0;
}

int ir_graph_add_node(ir_graph_t *graph, const char *name, const char *func_ref,
                      int nports, const char **port_names) {
    if (!graph || !name || !func_ref) return -1;
    ir_graph_node_t *nn = realloc(graph->nodes, (graph->nnodes + 1) * sizeof(ir_graph_node_t));
    if (!nn) return -1;
    graph->nodes = nn;
    ir_graph_node_t *n = &graph->nodes[graph->nnodes++];
    memset(n, 0, sizeof(*n));
    n->name = strdup(name); n->func_ref = strdup(func_ref); n->nports = nports;
    if (nports > 0 && port_names) {
        n->port_names = malloc(nports * sizeof(char *));
        if (!n->port_names) return -1;
        for (int i = 0; i < nports; i++) n->port_names[i] = strdup(port_names[i]);
    }
    return 0;
}

int ir_graph_add_edge(ir_graph_t *graph, const char *src_node, const char *src_port,
                      const char *dst_node, const char *dst_port,
                      ir_reg_type_t elem_type, int bufsize) {
    if (!graph || !src_node || !src_port || !dst_node || !dst_port) return -1;
    ir_graph_edge_t *ne = realloc(graph->edges, (graph->nedges + 1) * sizeof(ir_graph_edge_t));
    if (!ne) return -1;
    graph->edges = ne;
    ir_graph_edge_t *e = &graph->edges[graph->nedges++];
    memset(e, 0, sizeof(*e));
    e->src_node = strdup(src_node); e->src_port = strdup(src_port);
    e->dst_node = strdup(dst_node); e->dst_port = strdup(dst_port);
    e->elem_type = elem_type; e->bufsize = bufsize;
    return 0;
}

int ir_block_add_instr(ir_block_t *block, ir_instr_t *instr) {
    if (!block || !instr) return -1;
    ir_instr_ent_t *ent = ir_instr_ent_new();
    if (!ent) return -1;
    ent->inst = *instr; ent->next = NULL;
    if (block->instrs) block->last->next = ent;
    else block->instrs = ent;
    block->last = ent; block->ninstr++;
    return 0;
}

int ir_num_results(ir_opcode_t o) {
    switch (o) {
    case IR_OPCODE_STORE: case IR_OPCODE_MEMCPY: case IR_OPCODE_SET_FIELD:
    case IR_OPCODE_BR: case IR_OPCODE_BR_COND: case IR_OPCODE_SWITCH:
    case IR_OPCODE_RET: case IR_OPCODE_YIELD: case IR_OPCODE_SUSPEND: return 0;
    case IR_OPCODE_CONST: case IR_OPCODE_PHI: case IR_OPCODE_ALLOCA:
    case IR_OPCODE_LOAD: case IR_OPCODE_MOV: case IR_OPCODE_ADD:
    case IR_OPCODE_SUB: case IR_OPCODE_MUL: case IR_OPCODE_DIV:
    case IR_OPCODE_UDIV: case IR_OPCODE_MOD: case IR_OPCODE_UREM:
    case IR_OPCODE_NEG: case IR_OPCODE_CMP_EQ: case IR_OPCODE_CMP_NE:
    case IR_OPCODE_CMP_LT: case IR_OPCODE_CMP_LE: case IR_OPCODE_CMP_GT:
    case IR_OPCODE_CMP_GE: case IR_OPCODE_NOT: case IR_OPCODE_AND:
    case IR_OPCODE_OR: case IR_OPCODE_XOR: case IR_OPCODE_SHL:
    case IR_OPCODE_SHR: case IR_OPCODE_CAST: case IR_OPCODE_MAKE_STRUCT:
    case IR_OPCODE_GET_FIELD: case IR_OPCODE_GET_ELEM:
    case IR_OPCODE_MAKE_ENUM: case IR_OPCODE_EXTRACT_VARIANT:
    case IR_OPCODE_CHECK_VARIANT: case IR_OPCODE_CALL:
    case IR_OPCODE_RECV: case IR_OPCODE_SEND: case IR_OPCODE_AWAIT: return 1;
    default: return -1;
    }
}

int ir_num_operands(ir_opcode_t o) {
    switch (o) {
    case IR_OPCODE_ALLOCA: case IR_OPCODE_SUSPEND: return 0;
    case IR_OPCODE_CONST: case IR_OPCODE_LOAD: case IR_OPCODE_MOV:
    case IR_OPCODE_NEG: case IR_OPCODE_NOT: case IR_OPCODE_CAST:
    case IR_OPCODE_GET_FIELD: case IR_OPCODE_MAKE_ENUM:
    case IR_OPCODE_EXTRACT_VARIANT: case IR_OPCODE_CHECK_VARIANT:
    case IR_OPCODE_BR: case IR_OPCODE_RET:
    case IR_OPCODE_RECV: case IR_OPCODE_AWAIT: return 1;
    case IR_OPCODE_STORE: case IR_OPCODE_ADD: case IR_OPCODE_SUB:
    case IR_OPCODE_MUL: case IR_OPCODE_DIV: case IR_OPCODE_UDIV:
    case IR_OPCODE_MOD: case IR_OPCODE_UREM: case IR_OPCODE_CMP_EQ:
    case IR_OPCODE_CMP_NE: case IR_OPCODE_CMP_LT: case IR_OPCODE_CMP_LE:
    case IR_OPCODE_CMP_GT: case IR_OPCODE_CMP_GE: case IR_OPCODE_AND:
    case IR_OPCODE_OR: case IR_OPCODE_XOR: case IR_OPCODE_SHL:
    case IR_OPCODE_SHR: case IR_OPCODE_GET_ELEM:
    case IR_OPCODE_SEND: case IR_OPCODE_YIELD: return 2;
    case IR_OPCODE_MEMCPY: case IR_OPCODE_BR_COND:
    case IR_OPCODE_SET_FIELD: return 3;
    case IR_OPCODE_PHI: case IR_OPCODE_SWITCH:
    case IR_OPCODE_CALL: case IR_OPCODE_MAKE_STRUCT: return -1;
    default: return -1;
    }
}

int ir_opcode_is_terminator(ir_opcode_t o) {
    switch (o) {
    case IR_OPCODE_BR: case IR_OPCODE_BR_COND:
    case IR_OPCODE_SWITCH: case IR_OPCODE_RET: case IR_OPCODE_SUSPEND: return 1;
    default: return 0;
    }
}

int ir_opcode_is_coro_only(ir_opcode_t o) {
    switch (o) {
    case IR_OPCODE_RECV: case IR_OPCODE_SEND:
    case IR_OPCODE_YIELD: case IR_OPCODE_AWAIT: case IR_OPCODE_SUSPEND: return 1;
    default: return 0;
    }
}
