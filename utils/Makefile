# This work is licensed under TURNSTONE OS Public License.
# Please read and understand latest version of Licence.

HOSTOS=$(shell uname -s)

CC = gcc
CCFLAGS = -Wall -Werror -Wextra -D___BITS=64 -I../includes -std=gnu18 \
	-O3 -nostdlib -ffreestanding -c -m64 -msse4.2\
	-fdata-sections -ffunction-sections \
	-Wshadow -Wpointer-arith -Wcast-align \
	-Wwrite-strings -Wmissing-prototypes -Wmissing-declarations \
    -Wredundant-decls -Wnested-externs -Winline -Wno-long-long \
    -Wstrict-prototypes -fPIC -fplt -mcmodel=large\
	-D___TESTMODE=1 -D___KERNELBUILD=0 -g ${CCXXEXTRAFLAGS}

ifeq ($(HOSTOS),Darwin)
CCFLAGS += $(shell pkg-config --cflags-only-I valgrind)
LD = ld
LDFLAGS = -lc $(shell pkgconfig --libs valgrind|cut -f1 -d " ") -lcoregrind-amd64-darwin -lvex-amd64-darwin
else
CCFLAGS += $(shell pkg-config --cflags-only-I valgrind)
LD = gcc
LDFLAGS = $(shell pkg-config --libs valgrind|cut -f1 -d " ") -lcoregrind-amd64-linux -lvex-amd64-linux \
		  -Wl,--gc-sections -static -Wl,-static -Wl,--allow-multiple-definition
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
