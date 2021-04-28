#
# Copyright (c) 2019-2021 Hirochika Asai
# All rights reserved.
#
# Authors:
#      Hirochika Asai  <asai@jar.jp>
#

BASEDIR=$(shell pwd)
CFLAGS=-g -Wall -DBASEDIR=\"$(BASEDIR)\"

all:
	$(MAKE) minica_test_ld minica_test_asm minica_test_parser

y.tab.c: minica.y
	$(YACC) -d $^
y.tab.h: minica.y
	$(YACC) -d $^
lex.yy.c: minica.l y.tab.h
	$(LEX) $^
syntax.o: syntax.c syntax.h
compile.o: compile.c compile.h

minica: y.tab.o lex.yy.o syntax.o compile.o arch/x86-64/x86-64.o arch/x86-64/instr.o ld/mach-o.o
	$(CC) $(CFLAGS) -o $@ $^

minica_test_ld: tests/minica_test_ld.o ld/mach-o.o ld/elf.o
	$(CC) $(CFLAGS) -o $@ $^

minica_test_asm: tests/minica_test_asm.o arch/x86-64/x86-64.o arch/x86-64/instr.o
	$(CC) $(CFLAGS) -o $@ $^

minica_test_parser: tests/minica_test_parser.o y.tab.o lex.yy.o syntax.o compile.o
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f *.o minica y.y.tab.c y.tab.h lex.yy.c

