#
# Copyright (c) 2019-2020 Hirochika Asai
# All rights reserved.
#
# Authors:
#      Hirochika Asai  <asai@jar.jp>
#

all:
	$(MAKE) minica minica_test_ld

y.tab.c: minica.y
	$(YACC) -d $^
y.tab.h: minica.y
	$(YACC) -d $^
lex.yy.c: minica.l y.tab.h
	$(LEX) $^
syntax.o: syntax.c syntax.h
compile.o: compile.c compile.h

minica: y.tab.o lex.yy.o syntax.o compile.o arch/x86-64/x86-64.o ld/mach-o.o
	$(CC) -o $@ $^

minica_test_ld: tests/minica_test_ld.o ld/mach-o.o ld/elf.o
	$(CC) -o $@ $^

clean:
	rm -f *.o minica y.y.tab.c y.tab.h lex.yy.c

