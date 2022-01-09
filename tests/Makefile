HOSTOS=$(shell uname -s)

CC = gcc
CCFLAGS += -Wall -Werror -Wextra -D___BITS=64 -I../includes -std=gnu99 \
	-O3 -nostdlib -ffreestanding -c -m64 \
	-D___TESTMODE=1 -g \
	-L/opt/local/lib/valgrind -I/opt/local/include/valgrind -lcoregrind-amd64-darwin -lvex-amd64-darwin

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
	find ../output/cc -type d |sed 's%../output/cc%../output/cc-local%' |xargs mkdir -p

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
