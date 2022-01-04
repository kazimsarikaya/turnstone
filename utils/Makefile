HOSTOS=$(shell uname -s)

CC = gcc
CCFLAGS += -Wall -Werror -Wextra -D___BITS=64 -I../includes -I./ -std=gnu99 \
	-Os -nostdlib -ffreestanding -c -m64 \
	-D___TESTMODE=1 -g -Wno-address-of-packed-member \
	-L/opt/local/lib/valgrind -I/opt/local/include/valgrind -lcoregrind-amd64-darwin -lvex-amd64-darwin

ifeq ($(HOSTOS),Darwin)
LD = ld
LDFLAGS = -lc
else
LD = gcc
endif


OUTPUT = ../output
UTILOUTPUT = $(OUTPUT)/cc-local

CCSOURCEDIR = ../cc

SOURCES = $(shell find . -type f -name \*.c )

PROGS = $(patsubst ./%.c,%,$(SOURCES))

OUTPUTS := $(patsubst %,$(OUTPUT)/%.bin,$(PROGS))

.PHONY: all clean depend
.PRECIOUS: $(UTILOUTPUT)/%.o
all: $(OUTPUTS)

gendirs:
	find ../output/cc -type d |sed 's%../output/cc%../output/cc-local%' |xargs mkdir -p

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
