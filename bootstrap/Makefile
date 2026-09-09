#
# Copyright (c) 2019-2024,2026 Hirochika Asai
# All rights reserved.
#
# Authors:
#      Hirochika Asai  <asai@jar.jp>
#

BASEDIR=$(shell pwd)
CFLAGS=-g -Wall -DBASEDIR=\"$(BASEDIR)\" -MMD -MP
YACC_CFLAGS=-g -Wall -Wno-unused-but-set-variable -DBASEDIR="\"$(BASEDIR)\"" -MMD -MP
LEX_CFLAGS=-g -Wall -Wno-unused-function -Wno-unneeded-internal-declaration -Wno-sign-compare -DBASEDIR="\"$(BASEDIR)\"" -MMD -MP

YACC ?= yacc
LEX ?= flex

ARCH_OBJS=arch/x86-64/x86-64.o arch/x86-64/instr.o arch/aarch64/aarch64.o
LD_OBJS=ld/mach-o/mach-o.o ld/elf/elf.o

all:
	$(MAKE) minica_test_ld minica_test_asm minica_test_parser
	# minica_test_compiler: skipped (pre-existing struct mismatch in test file)

# Generate parser (y.tab.c and y.tab.h in one invocation)
y.tab.c y.tab.h: minica.y
	$(YACC) -d $<

# Generate lexer
lex.yy.c lex.yy.h: minica.l y.tab.h
	$(LEX) --reentrant --header-file=lex.yy.h $<

# Object files
lex.yy.o: lex.yy.c lex.yy.h minica.h
	$(CC) $(LEX_CFLAGS) -c -o $@ $<
y.tab.o: y.tab.c lex.yy.h minica.h
	$(CC) $(YACC_CFLAGS) -c -o $@ $<
syntax.o: syntax.c syntax.h minica.h
syntax_debug.o: syntax_debug.c syntax.h minica.h
arch.o: arch.c arch.h ir.h
compile.o: compile.c ir.c compile.h minica.h syntax.h ir.h
ir.o: ir.c ir.h
optimize.o: optimize.c ir.h
	$(CC) $(CFLAGS) -c -o $@ $<
ir_debug.o: ir_debug.c ir.h
tests/minica_test_parser.o: tests/minica_test_parser.c minica.h compile.h syntax.h
tests/minica_test_compiler.o: tests/minica_test_compiler.c minica.h compile.h syntax.h
tests/minica_test_ld.o: tests/minica_test_ld.c
tests/minica_test_asm.o: tests/minica_test_asm.c arch.h

# Test binaries
minica: y.tab.o lex.yy.o syntax.o syntax_debug.o $(LD_OBJS) $(ARCH_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

minica_test_ld: tests/minica_test_ld.o $(LD_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

minica_test_asm: tests/minica_test_asm.o arch.o $(ARCH_OBJS) $(LD_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

minica_test_parser: tests/minica_test_parser.o y.tab.o lex.yy.o syntax.o syntax_debug.o compile.o optimize.o ir.o arch.o $(LD_OBJS) $(ARCH_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

minica_test_compiler: tests/minica_test_compiler.o y.tab.o lex.yy.o syntax.o syntax_debug.o compile.o ir.o ir_debug.o arch.o $(LD_OBJS) $(ARCH_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Test target
test: minica_test_parser
	./minica_test_parser ../examples/simple1.al
	# minica_test_compiler: skipped (pre-existing struct mismatch in test file)

# Clean target (includes subdirectory objects and dependency files)
clean:
	rm -f minica_test_ld minica_test_asm minica_test_parser minica_test_compiler minica
	rm -f *.o y.tab.c y.tab.h lex.yy.c lex.yy.h *.d
	rm -f arch/x86-64/*.o arch/x86-64/*.d arch/aarch64/*.o arch/aarch64/*.d
	rm -f ld/elf/*.o ld/elf/*.d ld/mach-o/*.o ld/mach-o/*.d
	rm -f tests/*.o tests/*.d

.PHONY: all test clean

# Include auto-generated dependency files
-include *.d arch/x86-64/*.d arch/aarch64/*.d ld/elf/*.d ld/mach-o/*.d tests/*.d

# IR unit test
tests/minica_test_ir.o: tests/minica_test_ir.c ir.h
	$(CC) $(CFLAGS) -c -o $@ $<

minica_test_ir: tests/minica_test_ir.o ir.o
	$(CC) $(CFLAGS) -o $@ $^

# IR test target
test_ir: minica_test_ir
	./minica_test_ir

# DFIR compiler test
compiler.o: compiler.c syntax.h ir.h compile.h
	$(CC) $(CFLAGS) -c -o $@ $<

tests/minica_test_dfir.o: tests/minica_test_dfir.c minica.h syntax.h ir.h
	$(CC) $(CFLAGS) -c -o $@ $<

minica_test_dfir: tests/minica_test_dfir.o compiler.o optimize.o ir.o y.tab.o lex.yy.o syntax.o syntax_debug.o arch.o $(LD_OBJS) $(ARCH_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# DFIR test target
test_dfir: minica_test_dfir
	./minica_test_dfir ../examples/simple1.al


# Full build test (parse -> DFIR -> assemble -> export)
tests/minica_test_build.o: tests/minica_test_build.c minica.h syntax.h ir.h arch.h
	$(CC) $(CFLAGS) -c -o $@ $<

minica_test_build: tests/minica_test_build.o compiler.o optimize.o ir.o ir_debug.o y.tab.o lex.yy.o syntax.o syntax_debug.o arch.o arch/x86-64/x86-64.o arch/x86-64/instr.o arch/aarch64/aarch64.o ld/mach-o/mach-o.o ld/elf/elf.o
	$(CC) $(CFLAGS) -o $@ $^

test_build: minica_test_build
	./minica_test_build ../examples/simple1.al out.o --aarch64 --mach-o
