# Top level makefile

export

DEBUG = no

default: all

.DEFAULT:
	$(MAKE) -C src $@

debug: DEBUG = yes
debug:
	$(MAKE) -C src all

install:
	$(MAKE) -C src install

.PHONY: debug install
