/*_
 * Copyright (c) 2024-2026 Hirochika Asai <asai@jar.jp>
 * All rights reserved.
 * MIT License
 */

/*
 * Test: parse source file, compile to DFIR, assemble to native code,
 * and export as a Mach-O (aarch64) or ELF (x86-64) object file.
 */

#include "../minica.h"
#include "../syntax.h"
#include "../ir.h"
#include "../arch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern ir_object_t *compile_to_dfir(st_t *st);

static void
usage(const char *prog)
{
    fprintf(stderr, "Usage: %s <input.al> [output.o] [--mach-o|--elf] [--aarch64|--x86-64]\n", prog);
    exit(EXIT_FAILURE);
}

static const char *
opcode_name(ir_opcode_t opc)
{
    switch (opc) {
    case IR_OPCODE_CONST: return "const";
    case IR_OPCODE_MOV: return "mov";
    case IR_OPCODE_ADD: return "add";
    case IR_OPCODE_SUB: return "sub";
    case IR_OPCODE_MUL: return "mul";
    case IR_OPCODE_RET: return "ret";
    case IR_OPCODE_CALL: return "call";
    case IR_OPCODE_BR: return "br";
    case IR_OPCODE_BR_COND: return "br_cond";
    case IR_OPCODE_CMP_EQ: return "eq";
    case IR_OPCODE_CMP_NE: return "ne";
    case IR_OPCODE_CMP_LT: return "lt";
    case IR_OPCODE_CMP_LE: return "le";
    case IR_OPCODE_CMP_GT: return "gt";
    case IR_OPCODE_CMP_GE: return "ge";
    case IR_OPCODE_NEG: return "neg";
    case IR_OPCODE_NOT: return "not";
    case IR_OPCODE_AND: return "and";
    case IR_OPCODE_OR: return "or";
    case IR_OPCODE_XOR: return "xor";
    default: return "?";
    }
}

static void
print_dfir(ir_object_t *ir)
{
    printf("--- DFIR ---\n");
    ir_func_t *f = ir->funcs;
    while (f) {
        printf("%s @%s (%zu blocks)\n",
               f->type == IR_FUNC_COROUTINE ? "coro" : "func",
               f->name, f->nblocks);
        for (size_t i = 0; i < f->nblocks; i++) {
            ir_block_t *b = &f->blocks[i];
            printf("  %s: (%zu instrs)\n",
                   b->label ? b->label->name : "?", b->ninstr);
            ir_instr_ent_t *e = b->instrs;
            while (e) {
                printf("    ");
                if (e->inst.result.n > 0 && e->inst.result.reg[0].id)
                    printf("%s = ", e->inst.result.reg[0].id);
                printf("%s\n", opcode_name(e->inst.opcode));
                e = e->next;
            }
        }
        f = f->next;
    }
}

static void
print_native(arch_code_t *code)
{
    printf("--- Native code ---\n");
    printf("  CPU: %s\n", code->cpu == ARCH_CPU_AARCH64 ? "aarch64" : "x86-64");
    printf("  Text: %zu bytes\n", code->text.size);
    printf("  Symbols: %d\n", code->sym.n);
    for (int i = 0; i < code->sym.n; i++) {
        printf("    [%d] %s: type=%d pos=%lld size=%zu\n",
               i, code->sym.syms[i].label,
               code->sym.syms[i].type,
               (long long)code->sym.syms[i].pos,
               code->sym.syms[i].size);
    }
    printf("  Relocations: %d\n", code->rel.n);
    for (int i = 0; i < code->rel.n; i++) {
        printf("    [%d] type=%d pos=%lld sym=%d\n",
               i, code->rel.rels[i].type,
               (long long)code->rel.rels[i].pos,
               code->rel.rels[i].sym);
    }

    /* Hex dump first 64 bytes of text */
    printf("  Hex dump (first %zu bytes):\n    ",
           code->text.size < 64 ? code->text.size : 64);
    size_t limit = code->text.size < 64 ? code->text.size : 64;
    for (size_t i = 0; i < limit; i++) {
        printf("%02x ", code->text.s[i]);
        if ((i + 1) % 16 == 0) printf("\n    ");
    }
    printf("\n");
}

int
main(int argc, const char *const argv[])
{
    FILE *fp;
    st_t *st;
    ir_object_t *ir;
    arch_code_t code;
    arch_cpu_t cpu = ARCH_CPU_AARCH64;
    arch_loader_t loader = ARCH_LD_MACH_O;
    const char *infile = NULL;
    const char *outfile = "out.o";

    /* Parse args */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--aarch64") == 0) cpu = ARCH_CPU_AARCH64;
        else if (strcmp(argv[i], "--x86-64") == 0) cpu = ARCH_CPU_X86_64;
        else if (strcmp(argv[i], "--mach-o") == 0) loader = ARCH_LD_MACH_O;
        else if (strcmp(argv[i], "--elf") == 0) loader = ARCH_LD_ELF;
        else if (!infile) infile = argv[i];
        else outfile = argv[i];
    }
    if (!infile) usage(argv[0]);

    /* 1. Parse */
    fp = fopen(infile, "r");
    if (!fp) { perror("fopen"); return 1; }
    st = minica_parse(fp);
    fclose(fp);
    if (!st) { fprintf(stderr, "Parse error\n"); return 1; }

    printf("Parsed: %s\n", infile);

    /* 2. Compile to DFIR */
    ir = compile_to_dfir(st);
    if (!ir) { fprintf(stderr, "Compile error\n"); return 1; }

    print_dfir(ir);

    /* 3. Assemble to native code */
    memset(&code, 0, sizeof(code));
    arch_t *arch = arch_init(cpu, loader);
    if (!arch || !arch->assemble) {
        fprintf(stderr, "Failed to init arch\n");
        return 1;
    }

    int ret = arch->assemble(ir, &code);
    if (ret < 0) {
        fprintf(stderr, "Assemble error\n");
        return 1;
    }

    print_native(&code);

    /* 4. Mangle symbol names for Mach-O (Apple convention: _prefix) */
    if (loader == ARCH_LD_MACH_O) {
        for (int i = 0; i < code.sym.n; i++) {
            size_t len = strlen(code.sym.syms[i].label);
            char *mangled = malloc(len + 2);
            mangled[0] = '_';
            strcpy(mangled + 1, code.sym.syms[i].label);
            free(code.sym.syms[i].label);
            code.sym.syms[i].label = mangled;
        }
    }

    /* 5. Export to object file */
    if (arch->export) {
        FILE *out = fopen(outfile, "wb");
        if (!out) { perror("fopen output"); return 1; }
        ret = arch->export(out, &code);
        fclose(out);
        if (ret < 0) {
            fprintf(stderr, "Export error\n");
            return 1;
        }
        printf("\nExported: %s (%s, %s)\n", outfile,
               cpu == ARCH_CPU_AARCH64 ? "aarch64" : "x86-64",
               loader == ARCH_LD_MACH_O ? "Mach-O" : "ELF");
    } else {
        fprintf(stderr, "No export function available\n");
        return 1;
    }

    /* Cleanup */
    free(code.text.s);
    for (int i = 0; i < code.sym.n; i++) free(code.sym.syms[i].label);
    free(code.sym.syms);
    free(code.rel.rels);
    free(arch);

    return 0;
}
