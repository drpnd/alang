#
# Copyright (c) 2019 Hirochika Asai
# All rights reserved.
#
# Authors:
#      Hirochika Asai  <asai@jar.jp>
#

all:
	$(MAKE) minica

y.tab.c: minica.y
	$(YACC) -d $^
y.tab.h: minica.y
	$(YACC) -d $^
lex.yy.c: minica.l y.tab.h
	$(LEX) $^
syntax.o: syntax.c syntax.h
compile.o: compile.c

minica: y.tab.o lex.yy.o syntax.o compile.o
	$(CC) -o $@ $^

clean:
	rm -f *.o minica y.y.tab.c y.tab.h lex.yy.c

