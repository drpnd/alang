#
# Copyright (c) 2019,2022-2023 Hirochika Asai
# All rights reserved.
#
# Authors:
#      Hirochika Asai  <asai@jar.jp>
#

bootstrap:
	$(MAKE) -C bootstrap all

all:
	$(MAKE) bootstrap

test: bootstrap
	$(MAKE) -C bootstrap test

clean:
	$(MAKE) -C bootstrap clean

x86-test:
	arch -x86_64 clang -o x86-test mach-o-test.o test_main.c
	arch -x86_64 ./x86-test

.PHONY: all test clean bootstrap x86-test

