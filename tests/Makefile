# This work is licensed under TURNSTONE OS Public License.
# Please read and understand latest version of Licence.

HOSTOS=$(shell uname -s)

CC = gcc
CCFLAGS += -Wall -Werror -Wextra -D___BITS=64 -I../includes -I. -std=gnu11 \
	-O2 -nostdlib -nostdinc -ffreestanding -c -m64 -msse4.2\
	-fdata-sections -ffunction-sections \
	-Wshadow -Wpointer-arith -Wcast-align \
	-Wwrite-strings -Wmissing-prototypes -Wmissing-declarations \
    -Wredundant-decls -Wnested-externs -Winline -Wno-long-long \
    -Wstrict-prototypes -fpic -fPIC -fno-plt -mcmodel=large -fno-jump-tables\
	-D___TESTMODE=1 -g ${CCXXEXTRAFLAGS}

ifeq ($(HOSTOS),Darwin)
CCFLAGS += $(shell pkg-config --cflags-only-I valgrind)
LD = ld
LDFLAGS = -lc $(shell pkgconfig --libs valgrind|cut -f1 -d " ") -lcoregrind-amd64-darwin -lvex-amd64-darwin
else
CCFLAGS += $(shell pkg-config --cflags-only-I valgrind)
LD = gcc
LDFLAGS = $(shell pkg-config --libs valgrind|cut -f1 -d " ") -lcoregrind-amd64-linux -lvex-amd64-linux \
		  -Wl,--gc-sections -Wl,--allow-multiple-definition -Wl,-static -static -nostdlib -Wl,-e_tos_start -Wl,-pie -Wl,-I/lib/ld-musl-x86_64.so.1
endif

OUTPUT = ../output
LDSRCDIR = ../lds
TESTOUTPUT = $(OUTPUT)/cc-local
NOSTDLIB_DIR = nostdlib
NOSTDLIB_OUTPUT = $(TESTOUTPUT)/$(NOSTDLIB_DIR)

CCSOURCEDIR = ../cc

SOURCES = $(shell find . -maxdepth 1 -type f -name \*.c )
NOSTDLIB_SOURCES = $(shell find nostdlib -maxdepth 1 -type f -name \*.c)

PROGS = $(patsubst ./%.c,%,$(SOURCES))

NOSTDLIB_OBJECTS = $(patsubst $(NOSTDLIB_DIR)/%.c,$(NOSTDLIB_OUTPUT)/%.o,$(NOSTDLIB_SOURCES))

OUTPUTS := $(patsubst %,$(OUTPUT)/%.bin,$(PROGS))
TOSOUTPUTS := $(patsubst %,$(OUTPUT)/%.tosbin,$(PROGS))

.PHONY: all clean depend
.PRECIOUS: $(TESTOUTPUT)/%.o $(NOSTDLIB_OBJECTS)
all: $(OUTPUTS) 

gendirs:
	find ../output/cc -type d |sed 's%../output/cc%../output/cc-local%' |xargs mkdir -p
	mkdir -p $(NOSTDLIB_OUTPUT)

$(TESTOUTPUT)/%.o: %.c setup.h
	$(CC) $(CCFLAGS) -o $@ $<

$(TESTOUTPUT)/%.xx_64.o: $(CCSOURCEDIR)/%.xx.c
	$(CC) $(CCFLAGS) -o $@ $<

$(TESTOUTPUT)/%.64.o: $(CCSOURCEDIR)/%.64.c
	$(CC) $(CCFLAGS) -o $@ $<

$(OUTPUT)/%.bin: $(NOSTDLIB_OBJECTS) $(TESTOUTPUT)/%.o
	$(LD) $(LDFLAGS) -o $@ $^

$(OUTPUT)/%.tosbin: $(LINKER) $(LDSRCDIR)/local.ld $(NOSTDLIB_OBJECTS) $(TESTOUTPUT)/%.o
	$(LINKER) --init-section --trim -o $@ -M $@.map -T $(filter-out $<,$^) 
	chmod +x $@

$(NOSTDLIB_OUTPUT)/%.o: $(NOSTDLIB_DIR)/%.c 
	$(CC) $(CCFLAGS) -o $@ $<

clean:
	rm -fr $(TESTOUTPUT)/*
	rm -fr $(OUTPUTS)
	rm -f .depend

print-%  : ; @echo $* = $($*)

depend: .depend

.depend: $(SOURCES)
	../scripts/create_depends.sh $(PROGS) >.depend

-include .depend
