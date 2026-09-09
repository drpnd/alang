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
 * Unit tests for ir.c / ir.h (DFIR - Data Flow IR)
 *
 * Tests cover:
 *   1. Object/function/graph/block allocation and lifecycle
 *   2. Register, immediate, and operand helpers
 *   3. Structure builders (add_func, add_block, add_instr, add_node, add_edge)
 *   4. Opcode metadata (num_results, num_operands, is_terminator, is_coro_only)
 *   5. End-to-end IR construction (a simple func and coro)
 */

#include "../ir.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Test result tracking */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name)  \
    do { \
        tests_run++; \
        if ( test_##name() ) { \
            tests_passed++; \
            printf("  [PASS] %s\n", #name); \
        } else { \
            tests_failed++; \
            printf("  [FAIL] %s\n", #name); \
        } \
    } while (0)

#define ASSERT(cond) \
    do { \
        if ( !(cond) ) { \
            printf("    ASSERT failed: %s (line %d)\n", #cond, __LINE__); \
            return 0; \
        } \
    } while (0)

/*
 *======================================================================
 * 1. Allocation and lifecycle
 *======================================================================
 */

static int
test_object_new(void)
{
    ir_object_t *obj;

    obj = ir_object_new();
    ASSERT(obj != NULL);
    ASSERT(obj->funcs == NULL);
    ASSERT(obj->nfuncs == 0);
    ASSERT(obj->graphs == NULL);
    ASSERT(obj->ngraphs == 0);
    ASSERT(obj->graphs_cap == 0);
    free(obj);

    return 1;
}

static int
test_func_new(void)
{
    ir_func_t *func;

    func = ir_func_new();
    ASSERT(func != NULL);
    ASSERT(func->name == NULL);
    ASSERT(func->type == IR_FUNC_FUNC);
    ASSERT(func->nblocks == 0);
    ASSERT(func->blocks == NULL);
    ASSERT(func->next == NULL);
    free(func);

    return 1;
}

static int
test_graph_new(void)
{
    ir_graph_t *graph;

    graph = ir_graph_new("test_graph");
    ASSERT(graph != NULL);
    ASSERT(graph->name != NULL);
    ASSERT(strcmp(graph->name, "test_graph") == 0);
    ASSERT(graph->nnodes == 0);
    ASSERT(graph->nodes == NULL);
    ASSERT(graph->nedges == 0);
    ASSERT(graph->edges == NULL);

    /* Test with NULL name */
    ir_graph_t *g2 = ir_graph_new(NULL);
    ASSERT(g2 != NULL);
    ASSERT(g2->name == NULL);

    free(graph->name);
    free(graph);
    free(g2);

    return 1;
}

static int
test_block_new(void)
{
    ir_block_t *block;

    block = ir_block_new("$entry");
    ASSERT(block != NULL);
    ASSERT(block->label != NULL);
    ASSERT(block->label->name != NULL);
    ASSERT(strcmp(block->label->name, "$entry") == 0);
    ASSERT(block->label->block == block);
    ASSERT(block->ninstr == 0);
    ASSERT(block->instrs == NULL);
    ASSERT(block->last == NULL);

    /* Test with NULL label */
    ir_block_t *b2 = ir_block_new(NULL);
    ASSERT(b2 != NULL);
    ASSERT(b2->label == NULL);

    free(block->label->name);
    free(block->label);
    free(block);
    free(b2);

    return 1;
}

static int
test_instr_new_delete(void)
{
    ir_instr_t *instr;

    instr = ir_instr_new();
    ASSERT(instr != NULL);
    ASSERT(instr->opcode == 0);
    ASSERT(instr->noperands == 0);
    ASSERT(instr->result.n == 0);

    ir_instr_delete(instr);
    ir_instr_delete(NULL);  /* Should not crash */

    return 1;
}

static int
test_instr_ent_new_delete(void)
{
    ir_instr_ent_t *ent;

    ent = ir_instr_ent_new();
    ASSERT(ent != NULL);
    ASSERT(ent->next == NULL);

    ir_instr_ent_delete(ent);
    ir_instr_ent_delete(NULL);

    return 1;
}

/*
 *======================================================================
 * 2. Register, immediate, and operand helpers
 *======================================================================
 */

static int
test_reg_init_release(void)
{
    ir_reg_t reg;

    ASSERT(ir_reg_init(&reg, IR_REG_I32, "%1") != NULL);
    ASSERT(reg.type == IR_REG_I32);
    ASSERT(reg.id != NULL);
    ASSERT(strcmp(reg.id, "%1") == 0);

    ir_reg_release(&reg);
    ASSERT(reg.id == NULL);

    /* Test with NULL id */
    ASSERT(ir_reg_init(&reg, IR_REG_F64, NULL) != NULL);
    ASSERT(reg.type == IR_REG_F64);
    ASSERT(reg.id == NULL);

    ir_reg_release(&reg);

    /* Test with NULL reg pointer */
    ASSERT(ir_reg_init(NULL, IR_REG_I32, "%1") == NULL);

    return 1;
}

static int
test_imm_init_release(void)
{
    ir_imm_t imm;

    /* Integer immediate */
    ASSERT(ir_imm_init(&imm, IR_IMM_I32) != NULL);
    imm.u.s32 = 42;
    ASSERT(imm.type == IR_IMM_I32);
    ir_imm_release(&imm);

    /* String immediate */
    ASSERT(ir_imm_init(&imm, IR_IMM_STR) != NULL);
    imm.u.str = strdup("hello");
    ASSERT(imm.type == IR_IMM_STR);
    ir_imm_release(&imm);  /* Should free imm.u.str */

    /* Boolean immediate */
    ASSERT(ir_imm_init(&imm, IR_IMM_BOOL) != NULL);
    imm.u.bval = true;
    ASSERT(imm.type == IR_IMM_BOOL);
    ir_imm_release(&imm);

    /* NULL imm */
    ASSERT(ir_imm_init(NULL, IR_IMM_I32) == NULL);
    ir_imm_release(NULL);  /* Should not crash */

    return 1;
}

static int
test_operand_new_delete(void)
{
    ir_operand_t *op;

    op = ir_operand_new();
    ASSERT(op != NULL);
    ASSERT(op->type == 0);
    memset(&op->u, 0, sizeof(op->u));

    ir_operand_delete(op);
    ir_operand_delete(NULL);

    return 1;
}

/*
 *======================================================================
 * 3. Structure builders
 *======================================================================
 */

static int
test_object_add_func(void)
{
    ir_object_t *obj;
    ir_func_t *f1, *f2;

    obj = ir_object_new();
    ASSERT(obj != NULL);

    f1 = ir_func_new();
    f1->name = strdup("main");
    f1->type = IR_FUNC_FUNC;

    f2 = ir_func_new();
    f2->name = strdup("double");
    f2->type = IR_FUNC_COROUTINE;

    ASSERT(ir_object_add_func(obj, f1) == 0);
    ASSERT(obj->nfuncs == 1);
    ASSERT(obj->funcs == f1);

    ASSERT(ir_object_add_func(obj, f2) == 0);
    ASSERT(obj->nfuncs == 2);
    ASSERT(obj->funcs == f1);
    ASSERT(obj->funcs->next == f2);
    ASSERT(f2->next == NULL);

    /* NULL arguments */
    ASSERT(ir_object_add_func(obj, NULL) == -1);
    ASSERT(ir_object_add_func(NULL, f1) == -1);

    free(f1->name);
    free(f1);
    free(f2->name);
    free(f2);
    free(obj);

    return 1;
}

static int
test_object_add_graph(void)
{
    ir_object_t *obj;
    ir_graph_t *g1, *g2, *g3;

    obj = ir_object_new();
    ASSERT(obj != NULL);

    g1 = ir_graph_new("pipeline1");
    g2 = ir_graph_new("pipeline2");
    g3 = ir_graph_new("fanout");

    ASSERT(ir_object_add_graph(obj, g1) == 0);
    ASSERT(obj->ngraphs == 1);
    ASSERT(obj->graphs != NULL);
    ASSERT(strcmp(obj->graphs[0].name, "pipeline1") == 0);

    ASSERT(ir_object_add_graph(obj, g2) == 0);
    ASSERT(obj->ngraphs == 2);

    ASSERT(ir_object_add_graph(obj, g3) == 0);
    ASSERT(obj->ngraphs == 3);
    ASSERT(strcmp(obj->graphs[0].name, "pipeline1") == 0);
    ASSERT(strcmp(obj->graphs[1].name, "pipeline2") == 0);
    ASSERT(strcmp(obj->graphs[2].name, "fanout") == 0);

    /* NULL arguments */
    ASSERT(ir_object_add_graph(obj, NULL) == -1);
    ASSERT(ir_object_add_graph(NULL, g1) == -1);

    free(g1->name); free(g1);
    free(g2->name); free(g2);
    free(g3->name); free(g3);
    free(obj->graphs);
    free(obj);

    return 1;
}

static int
test_func_add_block(void)
{
    ir_func_t *func;
    ir_block_t *b1, *b2;

    func = ir_func_new();
    b1 = ir_block_new("$entry");
    b2 = ir_block_new("$loop");

    ASSERT(ir_func_add_block(func, b1) == 0);
    ASSERT(func->nblocks == 1);
    ASSERT(strcmp(func->blocks[0].label->name, "$entry") == 0);

    ASSERT(ir_func_add_block(func, b2) == 0);
    ASSERT(func->nblocks == 2);
    ASSERT(strcmp(func->blocks[1].label->name, "$loop") == 0);

    /* NULL arguments */
    ASSERT(ir_func_add_block(func, NULL) == -1);
    ASSERT(ir_func_add_block(NULL, b1) == -1);

    free(b1->label->name); free(b1->label); free(b1);
    free(b2->label->name); free(b2->label); free(b2);
    free(func->blocks);
    free(func);

    return 1;
}

static int
test_block_add_instr(void)
{
    ir_block_t *block;
    ir_instr_t instr1, instr2;
    int ret;

    block = ir_block_new("$entry");

    /* Create instruction 1: %1 = add i32 %a, %b */
    memset(&instr1, 0, sizeof(instr1));
    instr1.opcode = IR_OPCODE_ADD;
    instr1.result.n = 1;
    ir_reg_init(&instr1.result.reg[0], IR_REG_I32, "%1");
    instr1.noperands = 2;

    /* Create instruction 2: ret i32 %1 */
    memset(&instr2, 0, sizeof(instr2));
    instr2.opcode = IR_OPCODE_RET;
    instr2.noperands = 1;

    ret = ir_block_add_instr(block, &instr1);
    ASSERT(ret == 0);
    ASSERT(block->ninstr == 1);
    ASSERT(block->instrs != NULL);
    ASSERT(block->last == block->instrs);
    ASSERT(block->instrs->inst.opcode == IR_OPCODE_ADD);
    ASSERT(block->instrs->next == NULL);

    ret = ir_block_add_instr(block, &instr2);
    ASSERT(ret == 0);
    ASSERT(block->ninstr == 2);
    ASSERT(block->instrs->next != NULL);
    ASSERT(block->instrs->next->inst.opcode == IR_OPCODE_RET);
    ASSERT(block->last == block->instrs->next);
    ASSERT(block->last->next == NULL);

    /* NULL arguments */
    ASSERT(ir_block_add_instr(block, NULL) == -1);
    ASSERT(ir_block_add_instr(NULL, &instr1) == -1);

    /* Cleanup */
    ir_instr_ent_t *e = block->instrs;
    while (e != NULL) {
        ir_instr_ent_t *next = e->next;
        ir_reg_release(&e->inst.result.reg[0]);
        ir_instr_ent_delete(e);
        e = next;
    }
    free(block->label->name);
    free(block->label);
    free(block);

    return 1;
}

static int
test_graph_add_node(void)
{
    ir_graph_t *graph;
    const char *ports[] = {"in", "out"};

    graph = ir_graph_new("test");

    ASSERT(ir_graph_add_node(graph, "%src", "@source", 0, NULL) == 0);
    ASSERT(graph->nnodes == 1);
    ASSERT(strcmp(graph->nodes[0].name, "%src") == 0);
    ASSERT(strcmp(graph->nodes[0].func_ref, "@source") == 0);
    ASSERT(graph->nodes[0].nports == 0);
    ASSERT(graph->nodes[0].port_names == NULL);

    ASSERT(ir_graph_add_node(graph, "%map", "@map", 2, ports) == 0);
    ASSERT(graph->nnodes == 2);
    ASSERT(strcmp(graph->nodes[1].name, "%map") == 0);
    ASSERT(graph->nodes[1].nports == 2);
    ASSERT(strcmp(graph->nodes[1].port_names[0], "in") == 0);
    ASSERT(strcmp(graph->nodes[1].port_names[1], "out") == 0);

    /* NULL arguments */
    ASSERT(ir_graph_add_node(graph, NULL, "@f", 0, NULL) == -1);
    ASSERT(ir_graph_add_node(NULL, "%n", "@f", 0, NULL) == -1);

    /* Cleanup */
    free(graph->nodes[0].name);
    free(graph->nodes[0].func_ref);
    for (int i = 0; i < graph->nodes[1].nports; i++) {
        free(graph->nodes[1].port_names[i]);
    }
    free(graph->nodes[1].port_names);
    free(graph->nodes[1].name);
    free(graph->nodes[1].func_ref);
    free(graph->nodes);
    free(graph->name);
    free(graph);

    return 1;
}

static int
test_graph_add_edge(void)
{
    ir_graph_t *graph;

    graph = ir_graph_new("test");

    ASSERT(ir_graph_add_edge(graph, "%src", "out", "%map", "in",
                             IR_REG_I32, 128) == 0);
    ASSERT(graph->nedges == 1);
    ASSERT(strcmp(graph->edges[0].src_node, "%src") == 0);
    ASSERT(strcmp(graph->edges[0].src_port, "out") == 0);
    ASSERT(strcmp(graph->edges[0].dst_node, "%map") == 0);
    ASSERT(strcmp(graph->edges[0].dst_port, "in") == 0);
    ASSERT(graph->edges[0].elem_type == IR_REG_I32);
    ASSERT(graph->edges[0].bufsize == 128);

    /* Test default-ish bufsize */
    ASSERT(ir_graph_add_edge(graph, "%map", "out", "%snk", "in",
                             IR_REG_I32, 64) == 0);
    ASSERT(graph->nedges == 2);
    ASSERT(graph->edges[1].bufsize == 64);

    /* NULL arguments */
    ASSERT(ir_graph_add_edge(graph, NULL, "out", "%map", "in",
                             IR_REG_I32, 128) == -1);
    ASSERT(ir_graph_add_edge(NULL, "%src", "out", "%map", "in",
                             IR_REG_I32, 128) == -1);

    /* Cleanup */
    for (size_t i = 0; i < graph->nedges; i++) {
        free(graph->edges[i].src_node);
        free(graph->edges[i].src_port);
        free(graph->edges[i].dst_node);
        free(graph->edges[i].dst_port);
    }
    free(graph->edges);
    free(graph->name);
    free(graph);

    return 1;
}

/*
 *======================================================================
 * 4. Opcode metadata
 *======================================================================
 */

static int
test_num_results(void)
{
    /* 0 results */
    ASSERT(ir_num_results(IR_OPCODE_STORE) == 0);
    ASSERT(ir_num_results(IR_OPCODE_MEMCPY) == 0);
    ASSERT(ir_num_results(IR_OPCODE_SET_FIELD) == 0);
    ASSERT(ir_num_results(IR_OPCODE_BR) == 0);
    ASSERT(ir_num_results(IR_OPCODE_BR_COND) == 0);
    ASSERT(ir_num_results(IR_OPCODE_SWITCH) == 0);
    ASSERT(ir_num_results(IR_OPCODE_RET) == 0);
    ASSERT(ir_num_results(IR_OPCODE_YIELD) == 0);
    ASSERT(ir_num_results(IR_OPCODE_SUSPEND) == 0);

    /* 1 result */
    ASSERT(ir_num_results(IR_OPCODE_CONST) == 1);
    ASSERT(ir_num_results(IR_OPCODE_PHI) == 1);
    ASSERT(ir_num_results(IR_OPCODE_ALLOCA) == 1);
    ASSERT(ir_num_results(IR_OPCODE_LOAD) == 1);
    ASSERT(ir_num_results(IR_OPCODE_MOV) == 1);
    ASSERT(ir_num_results(IR_OPCODE_ADD) == 1);
    ASSERT(ir_num_results(IR_OPCODE_SUB) == 1);
    ASSERT(ir_num_results(IR_OPCODE_MUL) == 1);
    ASSERT(ir_num_results(IR_OPCODE_DIV) == 1);
    ASSERT(ir_num_results(IR_OPCODE_NEG) == 1);
    ASSERT(ir_num_results(IR_OPCODE_CMP_EQ) == 1);
    ASSERT(ir_num_results(IR_OPCODE_CMP_NE) == 1);
    ASSERT(ir_num_results(IR_OPCODE_CAST) == 1);
    ASSERT(ir_num_results(IR_OPCODE_CALL) == 1);
    ASSERT(ir_num_results(IR_OPCODE_RECV) == 1);
    ASSERT(ir_num_results(IR_OPCODE_SEND) == 1);
    ASSERT(ir_num_results(IR_OPCODE_AWAIT) == 1);

    /* Unknown */
    ASSERT(ir_num_results((ir_opcode_t)9999) == -1);

    return 1;
}

static int
test_num_operands(void)
{
    /* 0 operands */
    ASSERT(ir_num_operands(IR_OPCODE_ALLOCA) == 0);
    ASSERT(ir_num_operands(IR_OPCODE_SUSPEND) == 0);

    /* 1 operand */
    ASSERT(ir_num_operands(IR_OPCODE_CONST) == 1);
    ASSERT(ir_num_operands(IR_OPCODE_LOAD) == 1);
    ASSERT(ir_num_operands(IR_OPCODE_MOV) == 1);
    ASSERT(ir_num_operands(IR_OPCODE_NEG) == 1);
    ASSERT(ir_num_operands(IR_OPCODE_NOT) == 1);
    ASSERT(ir_num_operands(IR_OPCODE_CAST) == 1);
    ASSERT(ir_num_operands(IR_OPCODE_BR) == 1);
    ASSERT(ir_num_operands(IR_OPCODE_RET) == 1);
    ASSERT(ir_num_operands(IR_OPCODE_RECV) == 1);
    ASSERT(ir_num_operands(IR_OPCODE_AWAIT) == 1);

    /* 2 operands */
    ASSERT(ir_num_operands(IR_OPCODE_STORE) == 2);
    ASSERT(ir_num_operands(IR_OPCODE_ADD) == 2);
    ASSERT(ir_num_operands(IR_OPCODE_SUB) == 2);
    ASSERT(ir_num_operands(IR_OPCODE_MUL) == 2);
    ASSERT(ir_num_operands(IR_OPCODE_CMP_EQ) == 2);
    ASSERT(ir_num_operands(IR_OPCODE_AND) == 2);
    ASSERT(ir_num_operands(IR_OPCODE_SEND) == 2);
    ASSERT(ir_num_operands(IR_OPCODE_YIELD) == 2);

    /* 3 operands */
    ASSERT(ir_num_operands(IR_OPCODE_MEMCPY) == 3);
    ASSERT(ir_num_operands(IR_OPCODE_BR_COND) == 3);
    ASSERT(ir_num_operands(IR_OPCODE_SET_FIELD) == 3);

    /* Variable (-1) */
    ASSERT(ir_num_operands(IR_OPCODE_PHI) == -1);
    ASSERT(ir_num_operands(IR_OPCODE_SWITCH) == -1);
    ASSERT(ir_num_operands(IR_OPCODE_CALL) == -1);
    ASSERT(ir_num_operands(IR_OPCODE_MAKE_STRUCT) == -1);

    return 1;
}

static int
test_is_terminator(void)
{
    /* Terminators */
    ASSERT(ir_opcode_is_terminator(IR_OPCODE_BR) == 1);
    ASSERT(ir_opcode_is_terminator(IR_OPCODE_BR_COND) == 1);
    ASSERT(ir_opcode_is_terminator(IR_OPCODE_SWITCH) == 1);
    ASSERT(ir_opcode_is_terminator(IR_OPCODE_RET) == 1);
    ASSERT(ir_opcode_is_terminator(IR_OPCODE_SUSPEND) == 1);

    /* Non-terminators */
    ASSERT(ir_opcode_is_terminator(IR_OPCODE_ADD) == 0);
    ASSERT(ir_opcode_is_terminator(IR_OPCODE_LOAD) == 0);
    ASSERT(ir_opcode_is_terminator(IR_OPCODE_STORE) == 0);
    ASSERT(ir_opcode_is_terminator(IR_OPCODE_CALL) == 0);
    ASSERT(ir_opcode_is_terminator(IR_OPCODE_RECV) == 0);
    ASSERT(ir_opcode_is_terminator(IR_OPCODE_SEND) == 0);
    ASSERT(ir_opcode_is_terminator(IR_OPCODE_YIELD) == 0);

    return 1;
}

static int
test_is_coro_only(void)
{
    /* Coro-only opcodes */
    ASSERT(ir_opcode_is_coro_only(IR_OPCODE_RECV) == 1);
    ASSERT(ir_opcode_is_coro_only(IR_OPCODE_SEND) == 1);
    ASSERT(ir_opcode_is_coro_only(IR_OPCODE_YIELD) == 1);
    ASSERT(ir_opcode_is_coro_only(IR_OPCODE_AWAIT) == 1);
    ASSERT(ir_opcode_is_coro_only(IR_OPCODE_SUSPEND) == 1);

    /* Non-coro opcodes */
    ASSERT(ir_opcode_is_coro_only(IR_OPCODE_ADD) == 0);
    ASSERT(ir_opcode_is_coro_only(IR_OPCODE_RET) == 0);
    ASSERT(ir_opcode_is_coro_only(IR_OPCODE_BR) == 0);
    ASSERT(ir_opcode_is_coro_only(IR_OPCODE_CALL) == 0);
    ASSERT(ir_opcode_is_coro_only(IR_OPCODE_LOAD) == 0);

    return 1;
}

/*
 *======================================================================
 * 5. End-to-end IR construction
 *======================================================================
 */

/*
 * Build a simple function IR:
 *   func @add(a: i32, b: i32) -> i32 {
 *     $entry:
 *       %1 = add i32 %a, %b
 *       ret i32 %1
 *   }
 */
static int
test_build_simple_func(void)
{
    ir_object_t *obj;
    ir_func_t *func;
    ir_block_t *block;
    ir_instr_t instr_add, instr_ret;

    obj = ir_object_new();
    ASSERT(obj != NULL);

    func = ir_func_new();
    func->name = strdup("add");
    func->type = IR_FUNC_FUNC;

    block = ir_block_new("$entry");

    /* %1 = add i32 %a, %b */
    memset(&instr_add, 0, sizeof(instr_add));
    instr_add.opcode = IR_OPCODE_ADD;
    instr_add.result.n = 1;
    ir_reg_init(&instr_add.result.reg[0], IR_REG_I32, "%1");
    instr_add.noperands = 2;
    instr_add.operands[0].type = IR_OPERAND_REG;
    ir_reg_init(&instr_add.operands[0].u.reg, IR_REG_I32, "%a");
    instr_add.operands[1].type = IR_OPERAND_REG;
    ir_reg_init(&instr_add.operands[1].u.reg, IR_REG_I32, "%b");

    /* ret i32 %1 */
    memset(&instr_ret, 0, sizeof(instr_ret));
    instr_ret.opcode = IR_OPCODE_RET;
    instr_ret.noperands = 1;
    instr_ret.operands[0].type = IR_OPERAND_REG;
    ir_reg_init(&instr_ret.operands[0].u.reg, IR_REG_I32, "%1");

    ASSERT(ir_block_add_instr(block, &instr_add) == 0);
    ASSERT(ir_block_add_instr(block, &instr_ret) == 0);
    ASSERT(block->ninstr == 2);

    ASSERT(ir_func_add_block(func, block) == 0);
    ASSERT(func->nblocks == 1);

    ASSERT(ir_object_add_func(obj, func) == 0);
    ASSERT(obj->nfuncs == 1);

    /* Verify the IR structure */
    ir_func_t *f = obj->funcs;
    ASSERT(strcmp(f->name, "add") == 0);
    ASSERT(f->type == IR_FUNC_FUNC);
    ASSERT(f->nblocks == 1);

    ir_block_t *b = &f->blocks[0];
    ASSERT(strcmp(b->label->name, "$entry") == 0);
    ASSERT(b->ninstr == 2);

    /* First instruction: add */
    ir_instr_ent_t *e = b->instrs;
    ASSERT(e->inst.opcode == IR_OPCODE_ADD);
    ASSERT(e->inst.result.n == 1);
    ASSERT(e->inst.result.reg[0].type == IR_REG_I32);
    ASSERT(strcmp(e->inst.result.reg[0].id, "%1") == 0);
    ASSERT(e->inst.noperands == 2);

    /* Second instruction: ret */
    e = e->next;
    ASSERT(e->inst.opcode == IR_OPCODE_RET);
    ASSERT(e->inst.noperands == 1);
    ASSERT(e->next == NULL);

    /* Cleanup */
    ir_reg_release(&instr_add.result.reg[0]);
    ir_reg_release(&instr_add.operands[0].u.reg);
    ir_reg_release(&instr_add.operands[1].u.reg);
    ir_reg_release(&instr_ret.operands[0].u.reg);

    e = b->instrs;
    while (e != NULL) {
        ir_instr_ent_t *next = e->next;
        ir_instr_ent_delete(e);
        e = next;
    }
    free(b->label->name);
    free(b->label);
    free(f->blocks);
    free(f->name);
    free(f);
    free(obj);

    return 1;
}

/*
 * Build a coroutine state machine IR:
 *   coro @double(in: Chan<i32>, out: Chan<i32>) -> Poll {
 *     state S0_start:  br S1_recv
 *     state S1_recv:   %v = recv %in; switch %v -> Some(%x): S2_send, None: S3_done
 *     state S2_send:   %d = mul i32 %x, 2; %ok = send %out, %d; switch %ok -> Ok: S1_recv, Full: S2_send
 *     state S3_done:   ret Poll::Ready
 *   }
 */
static int
test_build_coro_state_machine(void)
{
    ir_object_t *obj;
    ir_func_t *coro;
    ir_block_t *s0, *s1, *s2, *s3;
    ir_instr_t instr;

    obj = ir_object_new();

    coro = ir_func_new();
    coro->name = strdup("double");
    coro->type = IR_FUNC_COROUTINE;

    /* S0_start: br S1_recv */
    s0 = ir_block_new("S0_start");
    memset(&instr, 0, sizeof(instr));
    instr.opcode = IR_OPCODE_BR;
    instr.noperands = 1;
    instr.operands[0].type = IR_OPERAND_LABEL;
    instr.operands[0].u.label = strdup("S1_recv");
    ASSERT(ir_block_add_instr(s0, &instr) == 0);
    free(instr.operands[0].u.label);

    /* S1_recv: %v = recv %in */
    s1 = ir_block_new("S1_recv");
    memset(&instr, 0, sizeof(instr));
    instr.opcode = IR_OPCODE_RECV;
    instr.result.n = 1;
    ir_reg_init(&instr.result.reg[0], IR_REG_I32, "%v");
    instr.noperands = 1;
    instr.operands[0].type = IR_OPERAND_REG;
    ir_reg_init(&instr.operands[0].u.reg, IR_REG_PTR, "%in");
    ASSERT(ir_block_add_instr(s1, &instr) == 0);
    ir_reg_release(&instr.result.reg[0]);
    ir_reg_release(&instr.operands[0].u.reg);

    /* S2_send: %d = mul i32 %x, 2; %ok = send %out, %d */
    s2 = ir_block_new("S2_send");
    memset(&instr, 0, sizeof(instr));
    instr.opcode = IR_OPCODE_MUL;
    instr.result.n = 1;
    ir_reg_init(&instr.result.reg[0], IR_REG_I32, "%d");
    instr.noperands = 2;
    instr.operands[0].type = IR_OPERAND_REG;
    ir_reg_init(&instr.operands[0].u.reg, IR_REG_I32, "%x");
    instr.operands[1].type = IR_OPERAND_IMM;
    ir_imm_init(&instr.operands[1].u.imm, IR_IMM_I32);
    instr.operands[1].u.imm.u.s32 = 2;
    ASSERT(ir_block_add_instr(s2, &instr) == 0);
    ir_reg_release(&instr.result.reg[0]);
    ir_reg_release(&instr.operands[0].u.reg);
    ir_imm_release(&instr.operands[1].u.imm);

    memset(&instr, 0, sizeof(instr));
    instr.opcode = IR_OPCODE_SEND;
    instr.result.n = 1;
    ir_reg_init(&instr.result.reg[0], IR_REG_BOOL, "%ok");
    instr.noperands = 2;
    instr.operands[0].type = IR_OPERAND_REG;
    ir_reg_init(&instr.operands[0].u.reg, IR_REG_PTR, "%out");
    instr.operands[1].type = IR_OPERAND_REG;
    ir_reg_init(&instr.operands[1].u.reg, IR_REG_I32, "%d");
    ASSERT(ir_block_add_instr(s2, &instr) == 0);
    ir_reg_release(&instr.result.reg[0]);
    ir_reg_release(&instr.operands[0].u.reg);
    ir_reg_release(&instr.operands[1].u.reg);

    /* S3_done: ret Poll::Ready */
    s3 = ir_block_new("S3_done");
    memset(&instr, 0, sizeof(instr));
    instr.opcode = IR_OPCODE_RET;
    instr.noperands = 1;
    instr.operands[0].type = IR_OPERAND_IMM;
    ir_imm_init(&instr.operands[0].u.imm, IR_IMM_BOOL);
    instr.operands[0].u.imm.u.bval = true;
    ASSERT(ir_block_add_instr(s3, &instr) == 0);
    ir_imm_release(&instr.operands[0].u.imm);

    /* Add blocks to coro */
    ASSERT(ir_func_add_block(coro, s0) == 0);
    ASSERT(ir_func_add_block(coro, s1) == 0);
    ASSERT(ir_func_add_block(coro, s2) == 0);
    ASSERT(ir_func_add_block(coro, s3) == 0);
    ASSERT(coro->nblocks == 4);

    /* Add coro to object */
    ASSERT(ir_object_add_func(obj, coro) == 0);
    ASSERT(obj->nfuncs == 1);

    /* Verify structure */
    ASSERT(obj->funcs->type == IR_FUNC_COROUTINE);
    ASSERT(strcmp(obj->funcs->name, "double") == 0);
    ASSERT(obj->funcs->nblocks == 4);

    ASSERT(strcmp(obj->funcs->blocks[0].label->name, "S0_start") == 0);
    ASSERT(obj->funcs->blocks[0].ninstr == 1);
    ASSERT(obj->funcs->blocks[0].instrs->inst.opcode == IR_OPCODE_BR);

    ASSERT(strcmp(obj->funcs->blocks[1].label->name, "S1_recv") == 0);
    ASSERT(obj->funcs->blocks[1].ninstr == 1);
    ASSERT(obj->funcs->blocks[1].instrs->inst.opcode == IR_OPCODE_RECV);
    ASSERT(ir_opcode_is_coro_only(IR_OPCODE_RECV));

    ASSERT(strcmp(obj->funcs->blocks[2].label->name, "S2_send") == 0);
    ASSERT(obj->funcs->blocks[2].ninstr == 2);
    ASSERT(obj->funcs->blocks[2].instrs->inst.opcode == IR_OPCODE_MUL);
    ASSERT(obj->funcs->blocks[2].instrs->next->inst.opcode == IR_OPCODE_SEND);

    ASSERT(strcmp(obj->funcs->blocks[3].label->name, "S3_done") == 0);
    ASSERT(obj->funcs->blocks[3].ninstr == 1);
    ASSERT(obj->funcs->blocks[3].instrs->inst.opcode == IR_OPCODE_RET);
    ASSERT(ir_opcode_is_terminator(IR_OPCODE_RET));

    /* Cleanup */
    for (size_t i = 0; i < coro->nblocks; i++) {
        ir_block_t *b = &coro->blocks[i];
        ir_instr_ent_t *e = b->instrs;
        while (e != NULL) {
            ir_instr_ent_t *next = e->next;
            ir_instr_ent_delete(e);
            e = next;
        }
        free(b->label->name);
        free(b->label);
    }
    free(coro->blocks);
    free(coro->name);
    free(coro);
    free(obj);

    return 1;
}

/*
 * Build a data flow graph:
 *   graph @pipeline {
 *     node %src = @source("file:input")
 *     node %map = @map(@double)
 *     node %snk = @sink("stdout")
 *     edge %src.out -> %map.in : chan<i32, 128>
 *     edge %map.out -> %snk.in : chan<i32, 128>
 *   }
 */
static int
test_build_graph(void)
{
    ir_object_t *obj;
    ir_graph_t *graph;
    const char *src_ports[] = {"out"};
    const char *map_ports[] = {"in", "out"};
    const char *snk_ports[] = {"in"};

    obj = ir_object_new();
    graph = ir_graph_new("pipeline");

    /* Add nodes */
    ASSERT(ir_graph_add_node(graph, "%src", "@source", 1, src_ports) == 0);
    ASSERT(ir_graph_add_node(graph, "%map", "@map", 2, map_ports) == 0);
    ASSERT(ir_graph_add_node(graph, "%snk", "@sink", 1, snk_ports) == 0);
    ASSERT(graph->nnodes == 3);

    /* Add edges */
    ASSERT(ir_graph_add_edge(graph, "%src", "out", "%map", "in",
                             IR_REG_I32, 128) == 0);
    ASSERT(ir_graph_add_edge(graph, "%map", "out", "%snk", "in",
                             IR_REG_I32, 128) == 0);
    ASSERT(graph->nedges == 2);

    /* Add graph to object */
    ASSERT(ir_object_add_graph(obj, graph) == 0);
    ASSERT(obj->ngraphs == 1);

    /* Verify */
    ir_graph_t *g = &obj->graphs[0];
    ASSERT(strcmp(g->name, "pipeline") == 0);
    ASSERT(g->nnodes == 3);
    ASSERT(g->nedges == 2);

    ASSERT(strcmp(g->nodes[0].name, "%src") == 0);
    ASSERT(g->nodes[0].nports == 1);
    ASSERT(strcmp(g->nodes[1].name, "%map") == 0);
    ASSERT(g->nodes[1].nports == 2);

    ASSERT(strcmp(g->edges[0].src_node, "%src") == 0);
    ASSERT(strcmp(g->edges[0].dst_node, "%map") == 0);
    ASSERT(g->edges[0].elem_type == IR_REG_I32);
    ASSERT(g->edges[0].bufsize == 128);

    /* Cleanup */
    for (size_t i = 0; i < g->nnodes; i++) {
        free(g->nodes[i].name);
        free(g->nodes[i].func_ref);
        for (int j = 0; j < g->nodes[i].nports; j++) {
            free(g->nodes[i].port_names[j]);
        }
        free(g->nodes[i].port_names);
    }
    free(g->nodes);
    for (size_t i = 0; i < g->nedges; i++) {
        free(g->edges[i].src_node);
        free(g->edges[i].src_port);
        free(g->edges[i].dst_node);
        free(g->edges[i].dst_port);
    }
    free(g->edges);
    free(g->name);
    free(obj->graphs);
    free(obj);

    return 1;
}

/*
 *======================================================================
 * Main
 *======================================================================
 */

int
main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    printf("=== DFIR IR Unit Tests ===\n\n");

    printf("--- Allocation and lifecycle ---\n");
    TEST(object_new);
    TEST(func_new);
    TEST(graph_new);
    TEST(block_new);
    TEST(instr_new_delete);
    TEST(instr_ent_new_delete);

    printf("\n--- Register, immediate, and operand helpers ---\n");
    TEST(reg_init_release);
    TEST(imm_init_release);
    TEST(operand_new_delete);

    printf("\n--- Structure builders ---\n");
    TEST(object_add_func);
    TEST(object_add_graph);
    TEST(func_add_block);
    TEST(block_add_instr);
    TEST(graph_add_node);
    TEST(graph_add_edge);

    printf("\n--- Opcode metadata ---\n");
    TEST(num_results);
    TEST(num_operands);
    TEST(is_terminator);
    TEST(is_coro_only);

    printf("\n--- End-to-end IR construction ---\n");
    TEST(build_simple_func);
    TEST(build_coro_state_machine);
    TEST(build_graph);

    printf("\n=== Results: %d passed, %d failed, %d total ===\n",
           tests_passed, tests_failed, tests_run);

    return tests_failed > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
