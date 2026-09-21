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
 *   7. Function inlining — inline small single-block function calls
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
    case IR_OPCODE_STORE8:
    case IR_OPCODE_SET_ELEM:
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

/* Check if a register is defined by a MOV in any block other than the given one.
 * If so, the register is a cross-block variable and copy propagation should
 * not replace its uses (the MOV in another block may override it). */
static int
is_redefined_in_other_blocks(ir_func_t *func, ir_block_t *blk, const char *reg_id)
{
    /* Count how many OTHER blocks define this register via MOV.
     * Only skip copy prop if 2+ other blocks define it (true cross-block
     * variable reassignment). A single other block is likely just the
     * if/else merge pattern, which is handled correctly by the backend. */
    int count = 0;
    for (size_t bi = 0; bi < func->nblocks; bi++) {
        if (&func->blocks[bi] == blk) continue;
        ir_instr_ent_t *ent = func->blocks[bi].instrs;
        while (ent) {
            if (ent->inst.opcode == IR_OPCODE_MOV &&
                ent->inst.noperands >= 2 &&
                ent->inst.operands[1].type == IR_OPERAND_REG &&
                ent->inst.operands[1].u.reg.id &&
                strcmp(ent->inst.operands[1].u.reg.id, reg_id) == 0) {
                count++;
                break;  /* One per block is enough */
            }
            ent = ent->next;
        }
    }
    return count >= 2;
}
static int
pass_copy_prop_block(ir_func_t *func, ir_block_t *blk)
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
        /* For GET_FIELD/SET_FIELD/EXTRACT_VARIANT: don't replace the struct base
         * (operand 0) because it refers to a register-based struct, not a value. */
        if (inst->opcode == IR_OPCODE_GET_FIELD ||
            inst->opcode == IR_OPCODE_SET_FIELD ||
            inst->opcode == IR_OPCODE_EXTRACT_VARIANT) max_op = 0;
        for (int i = 0; i < max_op && i < IR_MAX_OPERANDS; i++) {
            if (inst->operands[i].type == IR_OPERAND_REG &&
                inst->operands[i].u.reg.id) {
                const char *reg_id = inst->operands[i].u.reg.id;

                /* Search backwards for the NEAREST definition of this register.
                 * We must find the most recent definition, not the first one,
                 * because a register may be redefined by multiple MOVs
                 * (register-based variable model). */
                ir_instr_ent_t *prev = NULL;
                ir_instr_ent_t *scan = blk->instrs;
                while (scan && scan != ent) {
                    ir_instr_t *sinst = &scan->inst;
                    int def = 0;
                    if (sinst->result.n > 0 && sinst->result.reg[0].id &&
                        strcmp(sinst->result.reg[0].id, reg_id) == 0) {
                        def = 1;
                    }
                    if (sinst->opcode == IR_OPCODE_MOV &&
                        sinst->noperands >= 2 &&
                        sinst->operands[1].type == IR_OPERAND_REG &&
                        sinst->operands[1].u.reg.id &&
                        strcmp(sinst->operands[1].u.reg.id, reg_id) == 0) {
                        def = 1;
                    }
                    if (def) {
                        prev = scan;  /* remember most recent definition */
                    }
                    scan = scan->next;
                }

                if (prev) {
                    ir_instr_t *pinst = &prev->inst;
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
                }
            }
        }

        ent = ent->next;
    }

    return changed;
}

/*======================================================================
 * Pass: Dead Code After Terminator
 *
 * Removes instructions that follow a terminator (RET, BR) within the
 * same basic block. These are unreachable.
 *======================================================================*/
static int
pass_dce_after_terminator_func(ir_func_t *func)
{
    int changed = 0;
    for (size_t bi = 0; bi < func->nblocks; bi++) {
        ir_block_t *blk = &func->blocks[bi];
        ir_instr_ent_t *ent = blk->instrs;
        ir_instr_ent_t *prev = NULL;
        int found_terminator = 0;
        while (ent) {
            if (found_terminator) {
                ir_instr_ent_t *next = ent->next;
                if (prev) {
                    prev->next = next;
                } else {
                    blk->instrs = next;
                }
                if (blk->last == ent) {
                    blk->last = prev;
                }
                blk->ninstr--;
                free(ent);
                ent = next;
                changed = 1;
                continue;
            }
            if (ent->inst.opcode == IR_OPCODE_RET ||
                ent->inst.opcode == IR_OPCODE_BR) {
                found_terminator = 1;
            }
            prev = ent;
            ent = ent->next;
        }
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
                /* Don't remove MOV if the same register is defined by a MOV
                 * in a DIFFERENT block (cross-block variable reassignment). */
                if (!is_used) {
                    for (size_t bj = 0; bj < func->nblocks && !is_used; bj++) {
                        if (bj == bi) continue;
                        ir_instr_ent_t *e2 = func->blocks[bj].instrs;
                        while (e2) {
                            if (e2->inst.opcode == IR_OPCODE_MOV &&
                                e2->inst.noperands >= 2 &&
                                e2->inst.operands[1].type == IR_OPERAND_REG &&
                                e2->inst.operands[1].u.reg.id &&
                                strcmp(e2->inst.operands[1].u.reg.id,
                                       inst->operands[1].u.reg.id) == 0) {
                                is_used = 1;
                                break;
                            }
                            e2 = e2->next;
                        }
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
 * Pass 7: Function Inlining
 *
 * Inline small function calls by replacing CALL instructions with
 * the callee's body. SSA IDs are remapped to avoid conflicts.
 *======================================================================*/

#define MAX_INLINE_BLOCKS 4
#define MAX_INLINE_INSTRS 12

/* Find a function by name in the IR object */
static ir_func_t *
find_func_by_name(ir_object_t *obj, const char *name)
{
    ir_func_t *func = obj->funcs;
    while (func) {
        if (func->name && strcmp(func->name, name) == 0) {
            return func;
        }
        func = func->next;
    }
    return NULL;
}

/* Check if a function is small enough to inline */
static int
is_inlineable(ir_func_t *func)
{
    if (!func || func->type != IR_FUNC_FUNC) return 0;
    if (func->nblocks > MAX_INLINE_BLOCKS) return 0;
    int total_instrs = 0;
    for (size_t i = 0; i < func->nblocks; i++) {
        total_instrs += (int)func->blocks[i].ninstr;
        if (total_instrs > MAX_INLINE_INSTRS) return 0;
    }
    return 1;
}

/* Map an SSA id string to a new offset id */
static void
remap_ssa_id(char *buf, size_t bufsize, const char *orig_id, int offset)
{
    if (!orig_id || orig_id[0] != '%') {
        snprintf(buf, bufsize, "%s", orig_id ? orig_id : "%0");
        return;
    }
    int val = atoi(orig_id + 1);
    snprintf(buf, bufsize, "%%%d", val + offset);
}

/* Remap a register's id in-place */
static void
remap_reg(ir_reg_t *reg, int offset)
{
    if (!reg || !reg->id) return;
    char buf[64];
    remap_ssa_id(buf, sizeof(buf), reg->id, offset);
    free(reg->id);
    reg->id = strdup(buf);
}

/* Remap all operands and results of an instruction */
static void
remap_instr(ir_instr_t *inst, int offset)
{
    /* Remap result registers */
    for (int i = 0; i < inst->result.n && i < 2; i++) {
        remap_reg(&inst->result.reg[i], offset);
    }
    /* Remap operand registers (not immediates, not labels) */
    for (int i = 0; i < inst->noperands && i < IR_MAX_OPERANDS; i++) {
        if (inst->operands[i].type == IR_OPERAND_REG) {
            remap_reg(&inst->operands[i].u.reg, offset);
        }
    }
}

/* Clone an instruction with SSA remapping */
static ir_instr_ent_t *
clone_instr_remapped(ir_instr_ent_t *src, int offset)
{
    ir_instr_ent_t *new_ent = ir_instr_ent_new();
    if (!new_ent) return NULL;
    new_ent->inst = src->inst;
    new_ent->next = NULL;

    /* Deep-copy result registers */
    for (int i = 0; i < new_ent->inst.result.n && i < 2; i++) {
        if (new_ent->inst.result.reg[i].id) {
            new_ent->inst.result.reg[i].id = strdup(new_ent->inst.result.reg[i].id);
        }
    }
    /* Deep-copy operand strings/labels */
    for (int i = 0; i < new_ent->inst.noperands && i < IR_MAX_OPERANDS; i++) {
        if (new_ent->inst.operands[i].type == IR_OPERAND_REG) {
            if (new_ent->inst.operands[i].u.reg.id) {
                new_ent->inst.operands[i].u.reg.id =
                    strdup(new_ent->inst.operands[i].u.reg.id);
            }
        } else if (new_ent->inst.operands[i].type == IR_OPERAND_LABEL) {
            if (new_ent->inst.operands[i].u.label) {
                new_ent->inst.operands[i].u.label =
                    strdup(new_ent->inst.operands[i].u.label);
            }
        } else if (new_ent->inst.operands[i].type == IR_OPERAND_IMM &&
                   new_ent->inst.operands[i].u.imm.type == IR_IMM_STR) {
            if (new_ent->inst.operands[i].u.imm.u.str) {
                new_ent->inst.operands[i].u.imm.u.str =
                    strdup(new_ent->inst.operands[i].u.imm.u.str);
            }
        }
    }

    remap_instr(&new_ent->inst, offset);
    return new_ent;
}

/*
 * Inline a CALL instruction in a block.
 * Returns 1 if inlining was performed, 0 otherwise.
 *
 * Strategy: build a list of new instructions (arg MOVs + cloned callee body
 * with RET replaced by MOV to result), then splice them into the block
 * replacing the CALL instruction.
 */
static int
try_inline_call(ir_func_t *caller, ir_func_t *callee,
                size_t blk_idx, ir_instr_ent_t **ent_ptr,
                ir_instr_ent_t *prev, int ssa_offset)
{
    ir_instr_ent_t *ent = *ent_ptr;
    ir_instr_t *call_inst = &ent->inst;
    int nargs = call_inst->noperands - 1;

    /* The result register of the CALL */
    const char *result_id = NULL;
    if (call_inst->result.n > 0 && call_inst->result.reg[0].id) {
        result_id = call_inst->result.reg[0].id;
    }

    /* Save continuation (instructions after the CALL) */
    ir_instr_ent_t *continuation = ent->next;

    /* Build new instruction chain */
    ir_instr_ent_t *new_head = NULL;
    ir_instr_ent_t *new_tail = NULL;

    /* 1. Emit argument setup: for each argument, emit a CONST (if immediate)
     *    or MOV (if register) to load the value into the remapped param reg. */
    for (int i = 0; i < nargs; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%%%d", i + ssa_offset);
        if (call_inst->operands[i].type == IR_OPERAND_REG) {
            /* Register argument: mov %src, %param */
            ir_instr_ent_t *mov_ent = ir_instr_ent_new();
            mov_ent->inst.opcode = IR_OPCODE_MOV;
            mov_ent->inst.result.n = 0;
            mov_ent->inst.noperands = 2;
            mov_ent->inst.operands[0].type = IR_OPERAND_REG;
            mov_ent->inst.operands[0].u.reg = call_inst->operands[i].u.reg;
            if (mov_ent->inst.operands[0].u.reg.id) {
                mov_ent->inst.operands[0].u.reg.id =
                    strdup(mov_ent->inst.operands[0].u.reg.id);
            }
            mov_ent->inst.operands[1].type = IR_OPERAND_REG;
            ir_reg_init(&mov_ent->inst.operands[1].u.reg,
                        call_inst->operands[i].u.reg.type, buf);
            mov_ent->next = NULL;
            if (!new_head) { new_head = mov_ent; new_tail = mov_ent; }
            else { new_tail->next = mov_ent; new_tail = mov_ent; }
        } else if (call_inst->operands[i].type == IR_OPERAND_IMM) {
            /* Immediate argument: const %param, imm */
            ir_instr_ent_t *const_ent = ir_instr_ent_new();
            const_ent->inst.opcode = IR_OPCODE_CONST;
            const_ent->inst.result.n = 1;
            ir_reg_init(&const_ent->inst.result.reg[0], IR_REG_I64, buf);
            const_ent->inst.noperands = 1;
            const_ent->inst.operands[0].type = IR_OPERAND_IMM;
            const_ent->inst.operands[0].u.imm = call_inst->operands[i].u.imm;
            const_ent->next = NULL;
            if (!new_head) { new_head = const_ent; new_tail = const_ent; }
            else { new_tail->next = const_ent; new_tail = const_ent; }
        }
    }

    /* 2. Clone callee's instructions (single block assumed) */
    ir_block_t *cblk = &callee->blocks[0];
    ir_instr_ent_t *cent = cblk->instrs;
    while (cent) {
        ir_instr_ent_t *new_ent = clone_instr_remapped(cent, ssa_offset);

        /* Replace RET with MOV to call result */
        if (new_ent->inst.opcode == IR_OPCODE_RET) {
            if (new_ent->inst.noperands >= 1 &&
                new_ent->inst.operands[0].type == IR_OPERAND_REG &&
                result_id) {
                new_ent->inst.opcode = IR_OPCODE_MOV;
                new_ent->inst.noperands = 2;
                /* operand[0] = return value (already remapped) */
                /* operand[1] = call result register */
                new_ent->inst.operands[1].type = IR_OPERAND_REG;
                ir_reg_init(&new_ent->inst.operands[1].u.reg,
                            IR_REG_I64, result_id);
            } else {
                ir_instr_ent_delete(new_ent);
                cent = cent->next;
                continue;
            }
        }

        new_ent->next = NULL;
        if (!new_head) { new_head = new_ent; new_tail = new_ent; }
        else { new_tail->next = new_ent; new_tail = new_ent; }
        cent = cent->next;
    }

    /* 3. Splice: replace the CALL instruction with new_head */
    if (prev) {
        prev->next = new_head;
    } else {
        caller->blocks[blk_idx].instrs = new_head;
    }
    new_tail->next = continuation;

    /* Update block's last pointer if needed */
    if (caller->blocks[blk_idx].last == ent) {
        caller->blocks[blk_idx].last = new_tail;
    }

    /* Update instruction count: remove 1 (CALL), add new instructions */
    {
        int new_count = 0;
        ir_instr_ent_t *cnt = new_head;
        while (cnt) { new_count++; cnt = cnt->next; }
        caller->blocks[blk_idx].ninstr += new_count - 1;
    }

    /* Free the CALL instruction */
    ir_instr_ent_delete(ent);

    *ent_ptr = continuation;
    return 1;
}

static int
pass_inline_func(ir_object_t *obj, ir_func_t *caller)
{
    int changed = 0;

    /* Find the max SSA id in the caller to use as offset */
    int max_ssa = 0;
    for (size_t bi = 0; bi < caller->nblocks; bi++) {
        ir_instr_ent_t *ent = caller->blocks[bi].instrs;
        while (ent) {
            for (int i = 0; i < ent->inst.result.n && i < 2; i++) {
                if (ent->inst.result.reg[i].id) {
                    int v = atoi(ent->inst.result.reg[i].id + 1);
                    if (v > max_ssa) max_ssa = v;
                }
            }
            ent = ent->next;
        }
    }

    for (size_t bi = 0; bi < caller->nblocks; bi++) {
        ir_instr_ent_t *ent = caller->blocks[bi].instrs;
        ir_instr_ent_t *prev = NULL;
        while (ent) {
            if (ent->inst.opcode == IR_OPCODE_CALL &&
                ent->inst.noperands > 0 &&
                ent->inst.operands[ent->inst.noperands - 1].type == IR_OPERAND_IMM &&
                ent->inst.operands[ent->inst.noperands - 1].u.imm.type == IR_IMM_STR) {
                const char *callee_name =
                    ent->inst.operands[ent->inst.noperands - 1].u.imm.u.str;
                ir_func_t *callee = find_func_by_name(obj, callee_name);
                if (callee && callee != caller && is_inlineable(callee) &&
                    callee->nblocks == 1) {
                    /* Skip if any argument is a register that doesn't exist
                     * in the current block (e.g., result of a just-inlined call) */
                    int skip = 0;
                    /* Check that all register arguments are still defined */
                    int fnargs = ent->inst.noperands - 1;
                    for (int ai = 0; ai < fnargs; ai++) {
                        if (ent->inst.operands[ai].type == IR_OPERAND_REG &&
                            ent->inst.operands[ai].u.reg.id) {
                            /* Check if this register is defined in the block */
                            int found = 0;
                            ir_instr_ent_t *search = caller->blocks[bi].instrs;
                            while (search && search != ent) {
                                if (search->inst.result.n > 0 &&
                                    search->inst.result.reg[0].id &&
                                    strcmp(search->inst.result.reg[0].id,
                                           ent->inst.operands[ai].u.reg.id) == 0) {
                                    found = 1;
                                    break;
                                }
                                if (search->inst.opcode == IR_OPCODE_MOV &&
                                    search->inst.noperands >= 2 &&
                                    search->inst.operands[1].type == IR_OPERAND_REG &&
                                    search->inst.operands[1].u.reg.id &&
                                    strcmp(search->inst.operands[1].u.reg.id,
                                           ent->inst.operands[ai].u.reg.id) == 0) {
                                    found = 1;
                                    break;
                                }
                                search = search->next;
                            }
                            if (!found) { skip = 1; break; }
                        }
                    }
                    if (skip) {
                        prev = ent;
                        ent = ent->next;
                        continue;
                    }
                    int offset = max_ssa + 1;
                    if (try_inline_call(caller, callee, bi, &ent, prev, offset)) {
                        changed = 1;
                        max_ssa += 100; /* leave room for inlined SSA ids */
                        /* Only inline one call per function per iteration
                         * to avoid nested inlining issues */
                        return changed;
                    }
                }
            }
            prev = ent;
            ent = ent->next;
        }
    }

    return changed;
}

/*======================================================================
 * Main optimizer entry point
 *======================================================================
 */

/*
 * ir_optimize — run optimizer passes on an IR object
 *
 * Runs constant folding, copy propagation, and dead code elimination
 * until a fixpoint is reached (no more changes).
 *
 * Returns the number of changes made across all passes.
 */
static int pass_compact_ssa_func(ir_func_t *func);
static int pass_dce_after_terminator_func(ir_func_t *func);

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
                iter_changes += pass_copy_prop_block(func, blk);
            }
            iter_changes += pass_inline_func(obj, func);
            iter_changes += pass_const_branch_func(func);
            iter_changes += pass_unreachable_blocks_func(func);
            iter_changes += pass_block_merge_func(func);
            iter_changes += pass_dce_after_terminator_func(func);
            iter_changes += pass_dce_func(func);
            func = func->next;
        }

        /* Compact SSA IDs after all passes to eliminate gaps */
        if (iter_changes > 0) {
            ir_func_t *f2 = obj->funcs;
            while (f2) {
                iter_changes += pass_compact_ssa_func(f2);
                f2 = f2->next;
            }
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

/*======================================================================
 * Pass: SSA Compaction
 *
 * Renumbers SSA IDs sequentially (0, 1, 2, ...) to eliminate gaps left
 * by optimization passes. This keeps max SSA ID low so it fits within
 * the backend's register file without spilling.
 *======================================================================*/

/* Collect all unique SSA IDs in a function and build a compaction map.
 * Returns the number of unique IDs found. */
static int
pass_compact_ssa_func(ir_func_t *func)
{
    if (!func || func->nblocks == 0) return 0;

    /* Step 1: Collect all unique SSA IDs */
    /* Use a simple array since max SSA IDs are small (< 256) */
    int seen[256];
    int map[256];
    int n_unique = 0;

    memset(seen, 0, sizeof(seen));
    memset(map, 0, sizeof(map));

    /* Preserve function arguments and return values at their original
     * SSA IDs (0..nargs+nrets-1). The prologue moves arguments to these
     * IDs, so renumbering them would break the calling convention. */
    int n_preserve = func->nargs + func->nrets;
    if (n_preserve > 256) n_preserve = 256;
    for (int i = 0; i < n_preserve; i++) {
        seen[i] = 1;
        map[i] = i;
        n_unique++;
    }

    for (size_t bi = 0; bi < func->nblocks; bi++) {
        ir_block_t *blk = &func->blocks[bi];
        ir_instr_ent_t *e = blk->instrs;
        while (e) {
            ir_instr_t *inst = &e->inst;
            /* Check result registers */
            for (int i = 0; i < inst->result.n && i < 2; i++) {
                if (inst->result.reg[i].id) {
                    int id = atoi(inst->result.reg[i].id + 1);
                    if (id >= 0 && id < 256 && !seen[id]) {
                        seen[id] = 1;
                        map[id] = n_unique++;
                    }
                }
            }
            /* Check operand registers */
            for (int i = 0; i < inst->noperands && i < IR_MAX_OPERANDS; i++) {
                if (inst->operands[i].type == IR_OPERAND_REG &&
                    inst->operands[i].u.reg.id) {
                    int id = atoi(inst->operands[i].u.reg.id + 1);
                    if (id >= 0 && id < 256 && !seen[id]) {
                        seen[id] = 1;
                        map[id] = n_unique++;
                    }
                }
            }
            /* Also check MOV destination (operand[1] is a register) */
            if (inst->opcode == IR_OPCODE_MOV && inst->noperands >= 2) {
                if (inst->operands[1].type == IR_OPERAND_REG &&
                    inst->operands[1].u.reg.id) {
                    int id = atoi(inst->operands[1].u.reg.id + 1);
                    if (id >= 0 && id < 256 && !seen[id]) {
                        seen[id] = 1;
                        map[id] = n_unique++;
                    }
                }
            }
            e = e->next;
        }
    }

    /* Step 2: Check if compaction is needed (max ID > n_unique - 1) */
    int max_id = 0;
    for (int i = 0; i < 256; i++) {
        if (seen[i] && i > max_id) max_id = i;
    }
    if (max_id < n_unique) return 0;  /* No gaps, no compaction needed */

    /* Step 3: Rewrite all SSA IDs */
    for (size_t bi = 0; bi < func->nblocks; bi++) {
        ir_block_t *blk = &func->blocks[bi];
        ir_instr_ent_t *e = blk->instrs;
        while (e) {
            ir_instr_t *inst = &e->inst;
            /* Rewrite result registers */
            for (int i = 0; i < inst->result.n && i < 2; i++) {
                if (inst->result.reg[i].id) {
                    int id = atoi(inst->result.reg[i].id + 1);
                    if (id >= 0 && id < 256) {
                        char buf[32];
                        snprintf(buf, sizeof(buf), "%%%d", map[id]);
                        inst->result.reg[i].id = strdup(buf);
                    }
                }
            }
            /* Rewrite operand registers */
            for (int i = 0; i < inst->noperands && i < IR_MAX_OPERANDS; i++) {
                if (inst->operands[i].type == IR_OPERAND_REG &&
                    inst->operands[i].u.reg.id) {
                    int id = atoi(inst->operands[i].u.reg.id + 1);
                    if (id >= 0 && id < 256) {
                        char buf[32];
                        snprintf(buf, sizeof(buf), "%%%d", map[id]);
                        inst->operands[i].u.reg.id = strdup(buf);
                    }
                }
            }
            e = e->next;
        }
    }

    /* Update function's nargs if needed (nargs should still be valid) */
    return 1;  /* Changed */
}
