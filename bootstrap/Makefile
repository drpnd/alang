#
# Copyright (c) 2019-2024 Hirochika Asai
# All rights reserved.
#
# Authors:
#      Hirochika Asai  <asai@jar.jp>
#

BASEDIR=$(shell pwd)
CFLAGS=-g -Wall -DBASEDIR=\"$(BASEDIR)\"

ARCH_OBJS=arch/x86-64/x86-64.o arch/x86-64/instr.o arch/aarch64/aarch64.o
HEADERS=arch.h

all:
	$(MAKE) minica_test_ld minica_test_asm minica_test_parser minica_test_compiler

minica.l: y.tab.h
y.tab.c: minica.y
	$(YACC) -d $^
y.tab.h: minica.y
	$(YACC) -d $^
lex.yy.c: minica.l
	$(LEX) --reentrant --header-file=lex.yy.h $^
lex.yy.o: lex.yy.c lex.yy.h minica.h
y.tab.o: lex.yy.c minica.h
syntax.o: syntax.c syntax.h minica.h
arch.o: arch.h ir.h
compile.o: compile.c ir.c compile.h minica.h syntax.h ir.h

minica: y.tab.o lex.yy.o syntax.o syntax_debug.o ld/mach-o.o $(ARCH_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

minica_test_ld: tests/minica_test_ld.o ld/mach-o/mach-o.o ld/elf/elf.o
	$(CC) $(CFLAGS) -o $@ $^

minica_test_asm: tests/minica_test_asm.o arch.o $(ARCH_OBJS) ld/mach-o/mach-o.o ld/elf/elf.o
	$(CC) $(CFLAGS) -o $@ $^

tests/minica_test_parser.o: tests/minica_test_parser.c minica.h compile.h syntax.h

minica_test_parser: tests/minica_test_parser.o y.tab.o lex.yy.o syntax.o syntax_debug.o compile.o ir.o arch.o ld/mach-o/mach-o.o ld/elf/elf.o $(ARCH_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

minica_test_compiler: tests/minica_test_compiler.o y.tab.o lex.yy.o syntax.o syntax_debug.o compile.o ir.o ir_debug.o arch.o ld/mach-o/mach-o.o ld/elf/elf.o $(ARCH_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

test: minica_test_parser minica_test_compiler
	./minica_test_parser ../examples/simple1.al
	./minica_test_compiler ../examples/simple1.al

clean:
	rm -f minica_test_ld minica_test_asm minica_test_parser minica_test_compiler *.o minica y.y.tab.c y.tab.h lex.yy.c lex.yy.h

.PHONY: all test clean
