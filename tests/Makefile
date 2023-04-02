# This work is licensed under TURNSTONE OS Public License.
# Please read and understand latest version of Licence.

HOSTOS=$(shell uname -s)

CC = gcc
CCFLAGS += -Wall -Werror -Wextra -D___BITS=64 -I../includes -std=gnu11 \
	-O3 -nostdlib -ffreestanding -c -m64 -msse4.2\
	-fdata-sections -ffunction-sections \
	-Wshadow -Wpointer-arith -Wcast-align \
	-Wwrite-strings -Wmissing-prototypes -Wmissing-declarations \
    -Wredundant-decls -Wnested-externs -Winline -Wno-long-long \
    -Wstrict-prototypes \
	-D___TESTMODE=1 -g ${CCXXEXTRAFLAGS}

ifeq ($(HOSTOS),Darwin)
CCFLAGS += $(shell pkg-config --cflags-only-I valgrind)
LD = ld
LDFLAGS = -lc $(shell pkgconfig --libs valgrind|cut -f1 -d " ") -lcoregrind-amd64-darwin -lvex-amd64-darwin
else
CCFLAGS += $(shell pkg-config --cflags-only-I valgrind)
LD = gcc
LDFLAGS = $(shell pkg-config --libs valgrind|cut -f1 -d " ") -lcoregrind-amd64-linux -lvex-amd64-linux -Wl,--gc-sections
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
