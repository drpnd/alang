#
# Copyright (c) 2019 Hirochika Asai
# All rights reserved.
#
# Authors:
#      Hirochika Asai  <asai@jar.jp>
#

bootstrap:
	$(MAKE) -C bootstrap all

all:
	$(MAKE) bootstrap

.PHONY: all bootstrap
