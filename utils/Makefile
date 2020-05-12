CC = gcc
LD = ld
CCFLAGS = -Wall -Werror -D___BITS=64 -I../includes -std=gnu99 \
	-Os -nostdlib -ffreestanding -c -m64 \
	-D___TESTMODE=1
LDFLAGS = -macosx_version_min 10.13 -lSystem

OUTPUT = ../output
UTILOUTPUT = $(OUTPUT)/cc-local

CCSOURCEDIR = ../cc

SOURCES = $(shell find . -type f -name \*.c )

PROGS = $(patsubst ./%.c,%,$(SOURCES))

OUTPUTS := $(patsubst %,$(OUTPUT)/%.bin,$(PROGS))

.PHONY: all clean depend
all: $(OUTPUTS)

$(UTILOUTPUT)/%.o: %.c setup.h
	$(CC) $(CCFLAGS) -o $@ $<

$(UTILOUTPUT)/%.xx_64.o: $(CCSOURCEDIR)/%.xx.c
	$(CC) $(CCFLAGS) -o $@ $<

$(UTILOUTPUT)/%.64.o: $(CCSOURCEDIR)/%.64.c
	$(CC) $(CCFLAGS) -o $@ $<

$(OUTPUT)/%.bin: $(UTILOUTPUT)/%.o
	$(LD) $(LDFLAGS) -o $@ $^


clean:
	rm -fr $(UTILOUTPUT)/*
	rm -fr $(OUTPUTS)
	rm -f .depend

print-%  : ; @echo $* = $($*)

depend: .depend

.depend: $(SOURCES)
	../scripts/create_depends.sh $(PROGS) >.depend

-include .depend
