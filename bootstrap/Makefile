#
# Copyright (c) 2019-2022 Hirochika Asai
# All rights reserved.
#
# Authors:
#      Hirochika Asai  <asai@jar.jp>
#

BASEDIR=$(shell pwd)
CFLAGS=-g -Wall -DBASEDIR=\"$(BASEDIR)\"

all:
	$(MAKE) minica_test_ld minica_test_asm minica_test_parser minica_test_compiler

minica.l: y.tab.h
y.tab.c: minica.y
	$(YACC) -d $^
y.tab.h: minica.y
	$(YACC) -d $^
lex.yy.c: minica.l
	$(LEX) --reentrant --header-file=lex.yy.h $^
lex.yy.o: lex.yy.c lex.yy.h
y.tab.o: lex.yy.c
syntax.o: syntax.c syntax.h
compile.o: compile.c compile.h

minica: y.tab.o lex.yy.o syntax.o arch/x86-64/x86-64.o arch/x86-64/instr.o ld/mach-o.o
	$(CC) $(CFLAGS) -o $@ $^

minica_test_ld: tests/minica_test_ld.o ld/mach-o.o ld/elf.o
	$(CC) $(CFLAGS) -o $@ $^

minica_test_asm: tests/minica_test_asm.o arch/x86-64/x86-64.o arch/x86-64/instr.o
	$(CC) $(CFLAGS) -o $@ $^

minica_test_parser: tests/minica_test_parser.o y.tab.o lex.yy.o syntax.o compile.o
	$(CC) $(CFLAGS) -o $@ $^

minica_test_compiler: tests/minica_test_compiler.o y.tab.o lex.yy.o syntax.o compile.o
	$(CC) $(CFLAGS) -o $@ $^

test: minica_test_parser
	./minica_test_parser ../examples/simple1.al

clean:
	rm -f minica_test_ld minica_test_asm minica_test_parser *.o minica y.y.tab.c y.tab.h lex.yy.c lex.yy.h

.PHONY: all test clean
