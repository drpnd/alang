/*_
 * Copyright (c) 2024-2026 Hirochika Asai <asai@jar.jp>
 * All rights reserved.
 * MIT License
 */

/*
 * optimize.c — DFIR optimizer passes
 *
 * Passes:
 *   1. Constant folding — evaluate constant expressions at compile time
 *   2. Copy propagation — replace references to moved values with originals
 *   3. Constant branch elimination — replace br_cond with constant condition
 *   4. Unreachable block elimination — remove blocks not reachable from entry
 *   5. Block merging — merge blocks connected by single-pred unconditional br
 *   6. Dead code elimination — remove unused instructions
 *
 * Each pass operates on ir_object_t and modifies it in place.
 * Passes are run in sequence until no changes are made (fixpoint).
 */

#include "ir.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*======================================================================
 * Helpers
 *======================================================================*/

/*
 * Check if an instruction has a constant result.
 * Returns the constant value, or sets *ok=0 if not constant.
 */
static int64_t
instr_const_value(ir_instr_t *inst, int *ok)
{
    *ok = 0;
    if (inst->opcode == IR_OPCODE_CONST && inst->noperands >= 1 &&
        inst->operands[0].type == IR_OPERAND_IMM) {
        switch (inst->operands[0].u.imm.type) {
        case IR_IMM_I8:  case IR_IMM_S8:  return inst->operands[0].u.imm.u.s8;
        case IR_IMM_I16: case IR_IMM_S16: return inst->operands[0].u.imm.u.s16;
        case IR_IMM_I32: case IR_IMM_S32: return inst->operands[0].u.imm.u.s32;
        case IR_IMM_I64: case IR_IMM_S64: return inst->operands[0].u.imm.u.s64;
        case IR_IMM_BOOL: return inst->operands[0].u.imm.u.bval ? 1 : 0;
        default: return 0;
        }
    }
    return 0;
}

/*
 * Check if an instruction is a MOV (copy).
 * Returns source SSA id if it is, or NULL if not.
 */
static const char *
__attribute__((unused))
instr_mov_src(ir_instr_t *inst)
{
    if (inst->opcode == IR_OPCODE_MOV && inst->noperands >= 2 &&
        inst->operands[0].type == IR_OPERAND_REG) {
        return inst->operands[0].u.reg.id;
    }
    return NULL;
}

/*
 * Check if two SSA register ids are equal.
 */
static int
__attribute__((unused))
reg_eq(ir_reg_t *a, ir_reg_t *b)
{
    if (!a->id || !b->id) return 0;
    return strcmp(a->id, b->id) == 0;
}

/*
 * Check if an instruction is a terminator.
 */
static int
__attribute__((unused))
is_terminator(ir_opcode_t opc)
{
    return ir_opcode_is_terminator(opc);
}

/*
 * Check if an instruction has side effects (cannot be DCE'd).
 */
static int
has_side_effects(ir_opcode_t opc)
{
    switch (opc) {
    case IR_OPCODE_STORE:
    case IR_OPCODE_MEMCPY:
    case IR_OPCODE_SET_FIELD:
    case IR_OPCODE_BR:
    case IR_OPCODE_BR_COND:
    case IR_OPCODE_SWITCH:
    case IR_OPCODE_RET:
    case IR_OPCODE_CALL:
    case IR_OPCODE_YIELD:
    case IR_OPCODE_SEND:
    case IR_OPCODE_SUSPEND:
        return 1;
    default:
        return 0;
    }
}

/*======================================================================
 * Pass 1: Constant Folding
 *
 * If a binary operation has two constant operands, replace it with a
 * CONST instruction holding the computed value.
 *======================================================================*/

/*
 * Constant map: maps SSA id → constant value.
 * Uses a simple linear array.
 */
typedef struct {
    char *id;
    int64_t value;
    int valid;
} const_entry_t;

typedef struct {
    const_entry_t *entries;
    int count;
    int cap;
} const_map_t;

static void
cmap_init(const_map_t *m)
{
    m->cap = 32;
    m->count = 0;
    m->entries = calloc(m->cap, sizeof(const_entry_t));
}

static void
cmap_free(const_map_t *m)
{
    for (int i = 0; i < m->count; i++) free(m->entries[i].id);
    free(m->entries);
}

static void
cmap_set(const_map_t *m, const char *id, int64_t value)
{
    /* Check if already exists */
    for (int i = 0; i < m->count; i++) {
        if (m->entries[i].id && strcmp(m->entries[i].id, id) == 0) {
            m->entries[i].value = value;
            m->entries[i].valid = 1;
            return;
        }
    }
    /* Add new */
    if (m->count >= m->cap) {
        m->cap *= 2;
        m->entries = realloc(m->entries, m->cap * sizeof(const_entry_t));
    }
    m->entries[m->count].id = strdup(id);
    m->entries[m->count].value = value;
    m->entries[m->count].valid = 1;
    m->count++;
}

static int
cmap_get(const_map_t *m, const char *id, int64_t *out)
{
    for (int i = 0; i < m->count; i++) {
        if (m->entries[i].id && m->entries[i].valid &&
            strcmp(m->entries[i].id, id) == 0) {
            *out = m->entries[i].value;
            return 1;
        }
    }
    return 0;
}

static int
pass_const_fold_block(ir_block_t *blk)
{
    const_map_t cmap;
    cmap_init(&cmap);
    int changed = 0;

    ir_instr_ent_t *ent = blk->instrs;
    while (ent) {
        ir_instr_t *inst = &ent->inst;

        /* Record constants */
        if (inst->opcode == IR_OPCODE_CONST && inst->result.n > 0) {
            int ok;
            int64_t val = instr_const_value(inst, &ok);
            if (ok && inst->result.reg[0].id) {
                cmap_set(&cmap, inst->result.reg[0].id, val);
            }
        }

        /* Try to fold binary operations */
        if (inst->result.n > 0 && inst->noperands >= 2 &&
            inst->operands[0].type == IR_OPERAND_REG &&
            inst->operands[1].type == IR_OPERAND_REG) {
            int64_t v0, v1;
            int ok0 = cmap_get(&cmap, inst->operands[0].u.reg.id, &v0);
            int ok1 = cmap_get(&cmap, inst->operands[1].u.reg.id, &v1);

            if (ok0 && ok1) {
                int64_t result = 0;
                int can_fold = 1;

                switch (inst->opcode) {
                case IR_OPCODE_ADD: result = v0 + v1; break;
                case IR_OPCODE_SUB: result = v0 - v1; break;
                case IR_OPCODE_MUL: result = v0 * v1; break;
                case IR_OPCODE_DIV: if (v1 != 0) result = v0 / v1; else can_fold = 0; break;
                case IR_OPCODE_MOD: if (v1 != 0) result = v0 % v1; else can_fold = 0; break;
                case IR_OPCODE_AND: result = v0 & v1; break;
                case IR_OPCODE_OR:  result = v0 | v1; break;
                case IR_OPCODE_XOR: result = v0 ^ v1; break;
                case IR_OPCODE_CMP_EQ: result = (v0 == v1); break;
                case IR_OPCODE_CMP_NE: result = (v0 != v1); break;
                case IR_OPCODE_CMP_LT: result = (v0 < v1); break;
                case IR_OPCODE_CMP_LE: result = (v0 <= v1); break;
                case IR_OPCODE_CMP_GT: result = (v0 > v1); break;
                case IR_OPCODE_CMP_GE: result = (v0 >= v1); break;
                default: can_fold = 0; break;
                }

                if (can_fold) {
                    /* Replace with CONST */
                    inst->opcode = IR_OPCODE_CONST;
                    inst->noperands = 1;
                    inst->operands[0].type = IR_OPERAND_IMM;
                    ir_imm_init(&inst->operands[0].u.imm, IR_IMM_I64);
                    inst->operands[0].u.imm.u.s64 = result;
                    /* Clear operand[1] */
                    memset(&inst->operands[1], 0, sizeof(ir_operand_t));
                    /* Record in cmap */
                    if (inst->result.reg[0].id) {
                        cmap_set(&cmap, inst->result.reg[0].id, result);
                    }
                    changed = 1;
                }
            }
        }

        /* Try to fold unary operations */
        if (inst->result.n > 0 && inst->noperands >= 1 &&
            inst->operands[0].type == IR_OPERAND_REG) {
            int64_t v0;
            int ok0 = cmap_get(&cmap, inst->operands[0].u.reg.id, &v0);

            if (ok0) {
                int64_t result = 0;
                int can_fold = 1;

                switch (inst->opcode) {
                case IR_OPCODE_NEG: result = -v0; break;
                case IR_OPCODE_NOT: result = ~v0; break;
                default: can_fold = 0; break;
                }

                if (can_fold) {
                    inst->opcode = IR_OPCODE_CONST;
                    inst->noperands = 1;
                    inst->operands[0].type = IR_OPERAND_IMM;
                    ir_imm_init(&inst->operands[0].u.imm, IR_IMM_I64);
                    inst->operands[0].u.imm.u.s64 = result;
                    changed = 1;
                    if (inst->result.reg[0].id) {
                        cmap_set(&cmap, inst->result.reg[0].id, result);
                    }
                }
            }
        }

        ent = ent->next;
    }

    cmap_free(&cmap);
    return changed;
}

/*======================================================================
 * Pass 2: Copy Propagation
 *
 * If %a = mov %b, then all subsequent uses of %a can be replaced with %b.
 * Also, if %a = const N, all uses of %a can be replaced with the immediate.
 *======================================================================*/

typedef struct {
    char *src_id;       /* source SSA id (or NULL for immediate) */
    ir_reg_type_t type; /* register type */
    int64_t imm_val;    /* immediate value (if src_id is NULL) */
    int is_imm;
} copy_entry_t;

typedef struct {
    copy_entry_t *entries;
    int count;
    int cap;
} copy_map_t;

static void
__attribute__((unused))
cpmap_init(copy_map_t *m)
{
    m->cap = 32;
    m->count = 0;
    m->entries = calloc(m->cap, sizeof(copy_entry_t));
}

static void
__attribute__((unused))
cpmap_free(copy_map_t *m)
{
    for (int i = 0; i < m->count; i++) free(m->entries[i].src_id);
    free(m->entries);
}

static void
__attribute__((unused))
cpmap_add(copy_map_t *m, const char *dst_id, const char *src_id,
          int64_t imm, int is_imm, ir_reg_type_t type)
{
    (void)dst_id;
    if (m->count >= m->cap) {
        m->cap *= 2;
        m->entries = realloc(m->entries, m->cap * sizeof(copy_entry_t));
    }
    m->entries[m->count].src_id = src_id ? strdup(src_id) : NULL;
    m->entries[m->count].imm_val = imm;
    m->entries[m->count].is_imm = is_imm;
    m->entries[m->count].type = type;
    /* Use dst_id as key — store it in src_id field temporarily */
    /* Actually we need a separate key. Let me restructure. */
    m->count++;
}

/* Simplified: just use a find-replace approach within the block */

static int
pass_copy_prop_block(ir_block_t *blk)
{
    int changed = 0;

    ir_instr_ent_t *ent = blk->instrs;
    while (ent) {
        ir_instr_t *inst = &ent->inst;

        /* Skip MOV instructions — their sources will be resolved when
         * the destinations are replaced in other instructions.
         * For CALL: propagate arguments but not the callee name (last operand). */
        if (inst->opcode == IR_OPCODE_MOV) {
            ent = ent->next;
            continue;
        }

        /* Replace register operands with their sources */
        int max_op = inst->noperands;
        /* For CALL: don't replace the last operand (callee name) */
        if (inst->opcode == IR_OPCODE_CALL) max_op = inst->noperands - 1;
        /* For GET_FIELD/SET_FIELD: don't replace the struct base (operand 0)
         * because it refers to a register-based struct, not a value. */
        if (inst->opcode == IR_OPCODE_GET_FIELD ||
            inst->opcode == IR_OPCODE_SET_FIELD) max_op = 0;
        for (int i = 0; i < max_op && i < IR_MAX_OPERANDS; i++) {
            if (inst->operands[i].type == IR_OPERAND_REG &&
                inst->operands[i].u.reg.id) {
                const char *reg_id = inst->operands[i].u.reg.id;

                /* Search backwards for a definition of this register */
                ir_instr_ent_t *prev = blk->instrs;
                while (prev && prev != ent) {
                    ir_instr_t *pinst = &prev->inst;

                    /* Check if this instruction defines reg_id */
                    int defines_reg = 0;

                    /* Case 1: result.reg[0] matches (most instructions) */
                    if (pinst->result.n > 0 && pinst->result.reg[0].id &&
                        strcmp(pinst->result.reg[0].id, reg_id) == 0) {
                        defines_reg = 1;
                    }

                    /* Case 2: MOV — destination is operands[1] */
                    if (pinst->opcode == IR_OPCODE_MOV &&
                        pinst->noperands >= 2 &&
                        pinst->operands[1].type == IR_OPERAND_REG &&
                        pinst->operands[1].u.reg.id &&
                        strcmp(pinst->operands[1].u.reg.id, reg_id) == 0) {
                        defines_reg = 1;
                    }

                    if (defines_reg) {
                        if (pinst->opcode == IR_OPCODE_MOV &&
                            pinst->noperands >= 2 &&
                            pinst->operands[0].type == IR_OPERAND_REG &&
                            pinst->operands[0].u.reg.id) {
                            /* mov %src, %dst → replace uses of %dst with %src */
                            inst->operands[i].u.reg = pinst->operands[0].u.reg;
                            changed = 1;
                        } else if (pinst->opcode == IR_OPCODE_CONST &&
                                   pinst->noperands >= 1 &&
                                   pinst->operands[0].type == IR_OPERAND_IMM) {
                            /* const %dst, imm → replace uses of %dst with imm */
                            inst->operands[i].type = IR_OPERAND_IMM;
                            inst->operands[i].u.imm = pinst->operands[0].u.imm;
                            changed = 1;
                        }
                        break;
                    }
                    prev = prev->next;
                }
            }
        }

        ent = ent->next;
    }

    return changed;
}

/*======================================================================
 * Pass 3: Dead Code Elimination
 *
 * Remove instructions whose results are never used and have no side effects.
 * Also remove unreachable blocks (not reachable from the entry block).
 *======================================================================*/

static int
pass_dce_func(ir_func_t *func)
{
    int changed = 0;

    /* First pass: collect all used SSA ids across ALL blocks */
    char **used = NULL;
    int nused = 0;
    int cap = 0;

    for (size_t bi = 0; bi < func->nblocks; bi++) {
        ir_block_t *blk = &func->blocks[bi];
        ir_instr_ent_t *ent = blk->instrs;
        while (ent) {
            for (int i = 0; i < ent->inst.noperands && i < IR_MAX_OPERANDS; i++) {
                /* Skip MOV destination (operands[1]) — it's a definition, not a use */
                if (ent->inst.opcode == IR_OPCODE_MOV && i == 1) continue;
                if (ent->inst.operands[i].type == IR_OPERAND_REG &&
                    ent->inst.operands[i].u.reg.id) {
                /* Check if already in used list */
                int found = 0;
                for (int j = 0; j < nused; j++) {
                    if (strcmp(used[j], ent->inst.operands[i].u.reg.id) == 0) {
                        found = 1;
                        break;
                    }
                }
                if (!found) {
                    if (nused >= cap) {
                        cap = cap ? cap * 2 : 32;
                        used = realloc(used, cap * sizeof(char *));
                    }
                    used[nused++] = ent->inst.operands[i].u.reg.id;
                }
            }
        }
        ent = ent->next;
    }
    } /* end for (bi) */

    /* Second pass: remove unused instructions without side effects */
    for (size_t bi = 0; bi < func->nblocks; bi++) {
        ir_block_t *blk = &func->blocks[bi];
        ir_instr_ent_t *prev = NULL;
        ir_instr_ent_t *cur = blk->instrs;
        while (cur) {
        ir_instr_t *inst = &cur->inst;
        int can_remove = 0;

        /* Check if this instruction can be removed */
        if (!has_side_effects(inst->opcode)) {
            /* For MOV: destination is operands[1] */
            if (inst->opcode == IR_OPCODE_MOV && inst->noperands >= 2 &&
                inst->operands[1].type == IR_OPERAND_REG &&
                inst->operands[1].u.reg.id) {
                int is_used = 0;
                for (int j = 0; j < nused; j++) {
                    if (strcmp(used[j], inst->operands[1].u.reg.id) == 0) {
                        is_used = 1;
                        break;
                    }
                }
                if (!is_used) can_remove = 1;
            }
            /* For other instructions: result is in result.reg[0] */
            else if (inst->result.n > 0 && inst->result.reg[0].id) {
                int is_used = 0;
                for (int j = 0; j < nused; j++) {
                    if (strcmp(used[j], inst->result.reg[0].id) == 0) {
                        is_used = 1;
                        break;
                    }
                }
                if (!is_used) can_remove = 1;
            }
        }

        if (can_remove) {
            /* Remove this instruction */
            if (prev) {
                prev->next = cur->next;
            } else {
                blk->instrs = cur->next;
            }
            if (blk->last == cur) {
                blk->last = prev;
            }
            ir_instr_ent_t *next = cur->next;
            /* Free resources */
            for (int i = 0; i < inst->noperands && i < IR_MAX_OPERANDS; i++) {
                if (inst->operands[i].type == IR_OPERAND_LABEL) {
                    free(inst->operands[i].u.label);
                }
                /* Don't free string immediates — they may be shared
                 * by other instructions after copy propagation */
            }
            for (int i = 0; i < inst->result.n && i < 2; i++) {
                ir_reg_release(&inst->result.reg[i]);
            }
            free(cur);
            blk->ninstr--;
            cur = next;
            changed = 1;
            } else {
                prev = cur;
                cur = cur->next;
            }
        }
    }

    free(used);
    return changed;
}

/*======================================================================
 * Pass 4: Constant Branch Elimination
 *
 * If a br_cond instruction has a constant condition, replace it with
 * an unconditional branch to the taken target.
 *======================================================================*/

static int
pass_const_branch_func(ir_func_t *func)
{
    int changed = 0;

    /* Build constant map from CONST instructions across all blocks */
    const_map_t cmap;
    cmap_init(&cmap);

    for (size_t bi = 0; bi < func->nblocks; bi++) {
        ir_block_t *blk = &func->blocks[bi];
        ir_instr_ent_t *ent = blk->instrs;
        while (ent) {
            if (ent->inst.opcode == IR_OPCODE_CONST && ent->inst.result.n > 0) {
                int ok;
                int64_t val = instr_const_value(&ent->inst, &ok);
                if (ok && ent->inst.result.reg[0].id) {
                    cmap_set(&cmap, ent->inst.result.reg[0].id, val);
                }
            }
            ent = ent->next;
        }
    }

    /* Replace br_cond with constant condition */
    for (size_t bi = 0; bi < func->nblocks; bi++) {
        ir_block_t *blk = &func->blocks[bi];
        ir_instr_ent_t *ent = blk->instrs;
        while (ent) {
            ir_instr_t *inst = &ent->inst;
            if (inst->opcode == IR_OPCODE_BR_COND && inst->noperands >= 3 &&
                inst->operands[0].type == IR_OPERAND_REG &&
                inst->operands[0].u.reg.id) {
                int64_t cond_val;
                if (cmap_get(&cmap, inst->operands[0].u.reg.id, &cond_val)) {
                    /* Condition is constant: pick the right target */
                    const char *target_label;
                    if (cond_val != 0) {
                        /* True: branch to operands[1] (then) */
                        if (inst->operands[1].type == IR_OPERAND_LABEL) {
                            target_label = inst->operands[1].u.label;
                        } else {
                            ent = ent->next;
                            continue;
                        }
                    } else {
                        /* False: branch to operands[2] (else) */
                        if (inst->operands[2].type == IR_OPERAND_LABEL) {
                            target_label = inst->operands[2].u.label;
                        } else {
                            ent = ent->next;
                            continue;
                        }
                    }
                    /* Replace with unconditional branch */
                    inst->opcode = IR_OPCODE_BR;
                    inst->noperands = 1;
                    inst->operands[0].type = IR_OPERAND_LABEL;
                    inst->operands[0].u.label = strdup(target_label);
                    /* Clear operands[1] and [2] */
                    memset(&inst->operands[1], 0, sizeof(ir_operand_t));
                    memset(&inst->operands[2], 0, sizeof(ir_operand_t));
                    changed = 1;
                }
            }
            ent = ent->next;
        }
    }

    cmap_free(&cmap);
    return changed;
}

/*======================================================================
 * Pass 5: Unreachable Block Elimination
 *
 * Remove blocks that are not reachable from the entry (first) block.
 *======================================================================*/

static int
pass_unreachable_blocks_func(ir_func_t *func)
{
    int changed = 0;
    if (func->nblocks == 0) return 0;

    /* Mark reachable blocks using a worklist */
    int *reachable = calloc(func->nblocks, sizeof(int));
    int *worklist = calloc(func->nblocks, sizeof(int));
    int whead = 0, wtail = 0;

    /* Entry block is block 0 */
    reachable[0] = 1;
    worklist[wtail++] = 0;

    while (whead < wtail) {
        int idx = worklist[whead++];
        ir_block_t *blk = &func->blocks[idx];
        ir_instr_ent_t *ent = blk->instrs;
        while (ent) {
            ir_instr_t *inst = &ent->inst;
            /* Find branch targets */
            for (int i = 0; i < inst->noperands && i < IR_MAX_OPERANDS; i++) {
                if (inst->operands[i].type == IR_OPERAND_LABEL &&
                    inst->operands[i].u.label) {
                    /* Find block with this label */
                    for (size_t j = 0; j < func->nblocks; j++) {
                        if (func->blocks[j].label &&
                            func->blocks[j].label->name &&
                            strcmp(func->blocks[j].label->name,
                                   inst->operands[i].u.label) == 0) {
                            if (!reachable[j]) {
                                reachable[j] = 1;
                                worklist[wtail++] = (int)j;
                            }
                            break;
                        }
                    }
                }
            }
            ent = ent->next;
        }
    }

    /* Remove unreachable blocks by compacting the blocks array */
    size_t write_idx = 0;
    for (size_t i = 0; i < func->nblocks; i++) {
        if (reachable[i]) {
            if (write_idx != i) {
                func->blocks[write_idx] = func->blocks[i];
            }
            write_idx++;
        } else {
            /* Free instructions in unreachable block */
            ir_instr_ent_t *ent = func->blocks[i].instrs;
            while (ent) {
                ir_instr_ent_t *next = ent->next;
                ir_instr_ent_delete(ent);
                ent = next;
            }
            if (func->blocks[i].label) {
                free(func->blocks[i].label->name);
                free(func->blocks[i].label);
            }
            changed = 1;
        }
    }
    func->nblocks = write_idx;

    free(reachable);
    free(worklist);
    return changed;
}

/*======================================================================
 * Pass 6: Block Merging
 *
 * If a block B1 ends with an unconditional branch (br) to block B2,
 * and B2 has only one predecessor (B1), merge B2 into B1.
 *======================================================================*/

static int
pass_block_merge_func(ir_func_t *func)
{
    int changed = 0;
    if (func->nblocks <= 1) return 0;

    /* Count predecessors for each block */
    int *npred = calloc(func->nblocks, sizeof(int));

    for (size_t i = 0; i < func->nblocks; i++) {
        ir_block_t *blk = &func->blocks[i];
        ir_instr_ent_t *ent = blk->instrs;
        while (ent) {
            ir_instr_t *inst = &ent->inst;
            for (int j = 0; j < inst->noperands && j < IR_MAX_OPERANDS; j++) {
                if (inst->operands[j].type == IR_OPERAND_LABEL &&
                    inst->operands[j].u.label) {
                    for (size_t k = 0; k < func->nblocks; k++) {
                        if (func->blocks[k].label &&
                            func->blocks[k].label->name &&
                            strcmp(func->blocks[k].label->name,
                                   inst->operands[j].u.label) == 0) {
                            npred[k]++;
                            break;
                        }
                    }
                }
            }
            ent = ent->next;
        }
    }

    /* Find merge candidates: block ending with br to a block with 1 pred */
    for (size_t i = 0; i < func->nblocks; i++) {
        ir_block_t *blk = &func->blocks[i];
        /* Find the last instruction */
        ir_instr_ent_t *last_ent = blk->instrs;
        ir_instr_ent_t *prev_ent = NULL;
        while (last_ent && last_ent->next) {
            prev_ent = last_ent;
            last_ent = last_ent->next;
        }
        if (!last_ent) continue;

        /* Check if last instruction is an unconditional branch */
        if (last_ent->inst.opcode != IR_OPCODE_BR ||
            last_ent->inst.noperands < 1 ||
            last_ent->inst.operands[0].type != IR_OPERAND_LABEL) {
            continue;
        }

        const char *target = last_ent->inst.operands[0].u.label;
        /* Find the target block */
        int target_idx = -1;
        for (size_t k = 0; k < func->nblocks; k++) {
            if (k == i) continue;  /* Don't merge with self */
            if (func->blocks[k].label &&
                func->blocks[k].label->name &&
                strcmp(func->blocks[k].label->name, target) == 0) {
                target_idx = (int)k;
                break;
            }
        }
        if (target_idx < 0) continue;
        if (npred[target_idx] != 1) continue;  /* Target has other preds */

        /* Don't merge if target is a loop header (target has a back-edge
         * from a block at or after the source block). Check if any block
         * at index >= i branches to the target. */
        int is_loop_header = 0;
        for (size_t k = i; k < func->nblocks; k++) {
            ir_block_t *kblk = &func->blocks[k];
            ir_instr_ent_t *kent = kblk->instrs;
            while (kent) {
                for (int j = 0; j < kent->inst.noperands && j < IR_MAX_OPERANDS; j++) {
                    if (kent->inst.operands[j].type == IR_OPERAND_LABEL &&
                        kent->inst.operands[j].u.label &&
                        strcmp(kent->inst.operands[j].u.label, target) == 0) {
                        is_loop_header = 1;
                    }
                }
                if (is_loop_header) break;
                kent = kent->next;
            }
            if (is_loop_header) break;
        }
        if (is_loop_header) continue;

        /* Merge: remove the br from block i, then append target's
         * instructions to block i. Mark target as empty. */
        /* Remove the br instruction */
        if (prev_ent) {
            prev_ent->next = NULL;
            blk->last = prev_ent;
        } else {
            blk->instrs = NULL;
            blk->last = NULL;
        }
        blk->ninstr--;
        /* Free the removed br instruction */
        for (int j = 0; j < last_ent->inst.noperands && j < IR_MAX_OPERANDS; j++) {
            if (last_ent->inst.operands[j].type == IR_OPERAND_LABEL) {
                free(last_ent->inst.operands[j].u.label);
            }
        }
        free(last_ent);

        /* Append target block's instructions to block i */
        ir_block_t *tblk = &func->blocks[target_idx];
        if (blk->last) {
            blk->last->next = tblk->instrs;
            blk->last = tblk->last;
        } else {
            blk->instrs = tblk->instrs;
            blk->last = tblk->last;
        }
        blk->ninstr += tblk->ninstr;
        tblk->instrs = NULL;
        tblk->last = NULL;
        tblk->ninstr = 0;

        /* Update predecessor counts: target is now gone */
        npred[target_idx] = 0;
        changed = 1;
    }

    free(npred);
    return changed;
}

/*======================================================================
 * Main optimizer entry point
 *======================================================================*/

/*
 * ir_optimize — run optimizer passes on an IR object
 *
 * Runs constant folding, copy propagation, and dead code elimination
 * until a fixpoint is reached (no more changes).
 *
 * Returns the number of changes made across all passes.
 */
int
ir_optimize(ir_object_t *obj)
{
    int total_changes = 0;
    int max_iterations = 10;

    if (!obj) return 0;

    for (int iter = 0; iter < max_iterations; iter++) {
        int iter_changes = 0;

        ir_func_t *func = obj->funcs;
        while (func) {
            for (size_t bi = 0; bi < func->nblocks; bi++) {
                ir_block_t *blk = &func->blocks[bi];

                /* Run passes in order */
                iter_changes += pass_const_fold_block(blk);
                iter_changes += pass_copy_prop_block(blk);
            }
            iter_changes += pass_const_branch_func(func);
            iter_changes += pass_unreachable_blocks_func(func);
            iter_changes += pass_block_merge_func(func);
            iter_changes += pass_dce_func(func);
            func = func->next;
        }

        if (iter_changes == 0) {
            break;  /* Fixpoint reached */
        }
        total_changes += iter_changes;
    }

    return total_changes;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
