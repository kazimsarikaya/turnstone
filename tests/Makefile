CC = gcc
LD = ld
CCFLAGS = -Wall -Werror -D___BITS=64 -I../includes -std=gnu99 \
	-Os -nostdlib -ffreestanding -c -m64
LDFLAGS = -macosx_version_min 10.13 -lSystem

OUTPUT = ../output
CCOUTPUT = $(OUTPUT)/cc
TESTOUTPUT = $(OUTPUT)/tests

CCSOURCEDIR = ../cc

SOURCES = $(shell find . -type f -name \*.c )

PROGS = $(patsubst ./%.c,%,$(SOURCES))

OUTPUTS := $(patsubst %,$(OUTPUT)/%.bin,$(PROGS))

.PHONY: all clean depend
all: $(OUTPUTS)

$(TESTOUTPUT)/%.o: %.c
	$(CC) $(CCFLAGS) -o $@ $<

$(TESTOUTPUT)/%.xx_64.o: $(CCSOURCEDIR)/%.xx.c
	$(CC) $(CCFLAGS) -o $@ $<

$(TESTOUTPUT)/%.64.o: $(CCSOURCEDIR)/%.64.c
	$(CC) $(CCFLAGS) -o $@ $<

$(OUTPUT)/%.bin: $(TESTOUTPUT)/%.o
	$(LD) $(LDFLAGS) -o $@ $^


clean:
	rm -fr $(TESTOUTPUT)/*
	rm -fr $(OUTPUTS)
	rm -f .depend

print-%  : ; @echo $* = $($*)

depend: .depend

.depend: $(SOURCES)
	./create_depends.sh $(PROGS) >.depend

-include .depend
