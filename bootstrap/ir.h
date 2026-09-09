/*_
 * Copyright (c) 2022-2024,2026 Hirochika Asai <asai@jar.jp>
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

#ifndef _IR_H
#define _IR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <unistd.h>

typedef enum {
    IR_OPCODE_CONST, IR_OPCODE_PHI,
    IR_OPCODE_ALLOCA, IR_OPCODE_LOAD, IR_OPCODE_STORE, IR_OPCODE_MOV, IR_OPCODE_MEMCPY,
    IR_OPCODE_ADD, IR_OPCODE_SUB, IR_OPCODE_MUL, IR_OPCODE_DIV, IR_OPCODE_UDIV,
    IR_OPCODE_MOD, IR_OPCODE_UREM, IR_OPCODE_NEG,
    IR_OPCODE_CMP_EQ, IR_OPCODE_CMP_NE, IR_OPCODE_CMP_LT, IR_OPCODE_CMP_LE,
    IR_OPCODE_CMP_GT, IR_OPCODE_CMP_GE,
    IR_OPCODE_NOT, IR_OPCODE_AND, IR_OPCODE_OR, IR_OPCODE_XOR, IR_OPCODE_SHL, IR_OPCODE_SHR,
    IR_OPCODE_CAST,
    IR_OPCODE_MAKE_STRUCT, IR_OPCODE_GET_FIELD, IR_OPCODE_SET_FIELD, IR_OPCODE_GET_ELEM,
    IR_OPCODE_MAKE_ENUM, IR_OPCODE_EXTRACT_VARIANT, IR_OPCODE_CHECK_VARIANT,
    IR_OPCODE_BR, IR_OPCODE_BR_COND, IR_OPCODE_SWITCH, IR_OPCODE_RET,
    IR_OPCODE_CALL,
    IR_OPCODE_RECV, IR_OPCODE_SEND, IR_OPCODE_YIELD, IR_OPCODE_AWAIT, IR_OPCODE_SUSPEND,
} ir_opcode_t;

typedef enum {
    IR_IMM_I8, IR_IMM_S8, IR_IMM_I16, IR_IMM_S16,
    IR_IMM_I32, IR_IMM_S32, IR_IMM_I64, IR_IMM_S64,
    IR_IMM_F16, IR_IMM_F32, IR_IMM_F64, IR_IMM_FP8, IR_IMM_FP4,
    IR_IMM_BOOL, IR_IMM_STR,
} ir_imm_type_t;

typedef enum { IR_DATA_DATA, IR_DATA_BSS, IR_DATA_RODATA, } ir_data_type_t;

typedef enum {
    IR_REG_NONE = 0, IR_REG_PTR,
    IR_REG_I8, IR_REG_I16, IR_REG_I32, IR_REG_I64,
    IR_REG_F16, IR_REG_F32, IR_REG_F64, IR_REG_FP8, IR_REG_FP4,
    IR_REG_BOOL, IR_REG_STR, IR_REG_VOID,
} ir_reg_type_t;

typedef struct { ir_reg_type_t type; char *id; } ir_reg_t;

typedef struct {
    ir_imm_type_t type;
    union {
        uint8_t u8; int8_t s8; uint16_t u16; int16_t s16;
        uint32_t u32; int32_t s32; uint64_t u64; int64_t s64;
        _Float16 f16; float f32; double f64;
        uint8_t fp8; uint8_t fp4; bool bval; char *str;
    } u;
} ir_imm_t;

typedef enum { IR_OPERAND_REG, IR_OPERAND_IMM, IR_OPERAND_REF, IR_OPERAND_LABEL, } ir_operand_type_t;

typedef struct { ir_reg_t base; ir_reg_t index; int scale; int64_t disp; } ir_ref_t;

typedef struct {
    ir_operand_type_t type;
    union { ir_reg_t reg; ir_imm_t imm; ir_ref_t ref; char *label; } u;
} ir_operand_t;

typedef struct { int n; ir_reg_t reg[2]; } ir_result_t;

#define IR_MAX_OPERANDS 8

typedef struct {
    ir_opcode_t opcode; ir_result_t result; int noperands;
    ir_operand_t operands[IR_MAX_OPERANDS];
} ir_instr_t;

typedef struct _instr_ent ir_instr_ent_t;
typedef struct _block ir_block_t;
typedef struct _func ir_func_t;

struct _instr_ent { ir_instr_t inst; ir_instr_ent_t *next; };

typedef struct { char *name; ir_block_t *block; } ir_label_t;

struct _block {
    ir_label_t *label; size_t ninstr;
    ir_instr_ent_t *instrs; ir_instr_ent_t *last;
};

typedef enum { IR_FUNC_FUNC, IR_FUNC_COROUTINE, } ir_func_type_t;

struct _func {
    char *name; ir_func_type_t type; size_t nblocks;
    ir_block_t *blocks; ir_func_t *next;
    int nargs;              /* number of arguments (for ABI) */
    int nrets;              /* number of return values (for ABI) */
};

typedef struct { ir_data_type_t type; size_t len; uint8_t *d; } ir_data_entry_t;
typedef struct { size_t n; size_t used; ir_data_entry_t *entries; } ir_data_table_t;

typedef struct {
    char *name; char *func_ref; int nports; char **port_names;
} ir_graph_node_t;

typedef struct {
    char *src_node; char *src_port; char *dst_node; char *dst_port;
    ir_reg_type_t elem_type; int bufsize;
} ir_graph_edge_t;

typedef struct {
    char *name; size_t nnodes; ir_graph_node_t *nodes;
    size_t nedges; ir_graph_edge_t *edges;
} ir_graph_t;

typedef struct {
    size_t nfuncs; ir_func_t *funcs;
    size_t ngraphs; size_t graphs_cap; ir_graph_t *graphs;
    ir_data_table_t data;
} ir_object_t;

#ifdef __cplusplus
extern "C" {
#endif

ir_object_t *ir_object_new(void);
ir_func_t *ir_func_new(void);
ir_graph_t *ir_graph_new(const char *name);
ir_block_t *ir_block_new(const char *label_name);
ir_instr_t *ir_instr_new(void);
void ir_instr_delete(ir_instr_t *);
ir_instr_ent_t *ir_instr_ent_new(void);
void ir_instr_ent_delete(ir_instr_ent_t *);

ir_reg_t *ir_reg_init(ir_reg_t *reg, ir_reg_type_t type, const char *id);
void ir_reg_release(ir_reg_t *reg);
ir_imm_t *ir_imm_init(ir_imm_t *imm, ir_imm_type_t type);
void ir_imm_release(ir_imm_t *imm);
ir_operand_t *ir_operand_new(void);
void ir_operand_delete(ir_operand_t *);

int ir_object_add_func(ir_object_t *obj, ir_func_t *func);
int ir_object_add_graph(ir_object_t *obj, ir_graph_t *graph);
int ir_func_add_block(ir_func_t *func, ir_block_t *block);
int ir_graph_add_node(ir_graph_t *graph, const char *name, const char *func_ref,
                      int nports, const char **port_names);
int ir_graph_add_edge(ir_graph_t *graph, const char *src_node, const char *src_port,
                      const char *dst_node, const char *dst_port,
                      ir_reg_type_t elem_type, int bufsize);
int ir_block_add_instr(ir_block_t *block, ir_instr_t *instr);

int ir_num_results(ir_opcode_t opcode);
int ir_num_operands(ir_opcode_t opcode);
int ir_opcode_is_terminator(ir_opcode_t opcode);
int ir_opcode_is_coro_only(ir_opcode_t opcode);

int ir_print_code(ir_object_t *);

#ifdef __cplusplus
}
#endif

#endif /* _IR_H */
