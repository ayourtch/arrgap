.PHONY: all
all:
	gcc -o arrgap arrgap.c

.PHONY: indent
indent:
	indent -l100 -br -brf -npsl -i4 -npcs -nut -d0 -c0 -ce arrgap.c
