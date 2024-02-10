/*_
 * Copyright (c) 2022-2024 Hirochika Asai <asai@jar.jp>
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

#include "ir.h"
#include <stdlib.h>
#include <string.h>

/*
 * ir_object_new -- allocate a new object
 */
ir_object_t *
ir_object_new(void)
{
    ir_object_t *obj;

    obj = malloc(sizeof(ir_object_t));
    if ( obj == NULL ) {
        return NULL;
    }
    memset(obj, 0, sizeof(ir_object_t));

    return obj;
}

/*
 * ir_func_new -- allocate a new function
 */
ir_func_t *
ir_func_new(void)
{
    ir_func_t *func;

    func = malloc(sizeof(ir_func_t));
    if ( func == NULL ) {
        return NULL;
    }
    memset(func, 0, sizeof(ir_func_t));

    return func;
}

/*
 * ir_instr_new -- allocate a new instruction
 */
ir_instr_t *
ir_instr_new(void)
{
    ir_instr_t *i;

    i = malloc(sizeof(ir_instr_t));
    if ( i == NULL ) {
        return NULL;
    }
    memset(i, 0, sizeof(ir_instr_t));

    return i;
}

/*
 * ir_instr_delete -- delete an instruction
 */
void
ir_instr_delete(ir_instr_t *i)
{
    free(i);
}

/*
 * ir_reg_init -- initialize a new register
 */
ir_reg_t *
ir_reg_init(ir_reg_t *reg)
{
    return reg;
}

/*
 * ir_reg_release -- destruct a register
 */
void
ir_reg_release(ir_reg_t *reg)
{
}

/*
 * ir_imm_init -- initialize a new immediate value
 */
ir_imm_t *
ir_imm_init(ir_imm_t *imm, ir_imm_type_t type)
{
    imm->type = type;
    return imm;
}

/*
 * ir_imm_release -- destruct an immediate value
 */
void
ir_imm_release(ir_imm_t *imm)
{
    switch ( imm->type ) {
    default:
        /* Do nothing */
        break;
    }
}

/*
 * ir_operand_new -- allocate a new operand
 */
ir_operand_t *
ir_operand_new(void)
{
    ir_operand_t *o;

    o = malloc(sizeof(ir_operand_t));
    if ( o == NULL ) {
        return NULL;
    }
    memset(o, 0, sizeof(ir_operand_t));

    return o;
}

/*
 * ir_operand_delete -- delete an operand
 */
void
ir_operand_delete(ir_operand_t *o)
{
    free(o);
}

/*
 * ir_num_operands -- return the number of operands for the specified opcode
 */
int
ir_num_operands(ir_opcode_t opcode)
{
    int cnt;

    switch (opcode) {
    case IR_OPCODE_INC:
    case IR_OPCODE_DEC:
        cnt = 1;
        break;
    case IR_OPCODE_MOV:
        cnt = 2;
        break;
    case IR_OPCODE_ADD:
    case IR_OPCODE_SUB:
    case IR_OPCODE_MUL:
    case IR_OPCODE_DIV:
    case IR_OPCODE_MOD:
        cnt = 3;
        break;
    default:
        cnt = -1;
    }

    return cnt;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
