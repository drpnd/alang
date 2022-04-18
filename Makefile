#
# Copyright (c) 2019,2022 Hirochika Asai
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

.PHONY: all test clean bootstrap
