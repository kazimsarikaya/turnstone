HOSTOS=$(shell uname -s)

CC = gcc
CCFLAGS = -Wall -Werror -Wextra -D___BITS=64 -I../includes -std=gnu99 \
	-Os -nostdlib -ffreestanding -c -m64 \
	-D___TESTMODE=1 -g

ifeq ($(HOSTOS),Darwin)
LD = ld
LDFLAGS = -lc
else
LD = gcc
endif

OUTPUT = ../output
TESTOUTPUT = $(OUTPUT)/cc-local

CCSOURCEDIR = ../cc

SOURCES = $(shell find . -type f -name \*.c )

PROGS = $(patsubst ./%.c,%,$(SOURCES))

OUTPUTS := $(patsubst %,$(OUTPUT)/%.bin,$(PROGS))

.PHONY: all clean depend
.PRECIOUS: $(TESTOUTPUT)/%.o
all: $(OUTPUTS)

gendirs:
	mkdir -p $(TESTOUTPUT)

$(TESTOUTPUT)/%.o: %.c setup.h
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
	../scripts/create_depends.sh $(PROGS) >.depend

-include .depend
