# This work is licensed under TURNSTONE OS Public License.
# Please read and understand latest version of Licence.

HOSTOS=$(shell uname -s)

CC = gcc
CPP = g++

BASEFLAGS += -Wall -Werror -Wextra -D___BITS=64 -I../includes -I. -std=gnu18 \
	-O3 -nostdlib -nostdinc -ffreestanding -c -m64 -march=native\
	-fdata-sections -ffunction-sections \
	-Wshadow -Wpointer-arith -Wcast-align \
	-Wwrite-strings -Wmissing-declarations \
    -Wredundant-decls -Winline -Wno-long-long \
    -fPIC -fplt -mcmodel=large \
	-D___TESTMODE=1 -g ${CCXXEXTRAFLAGS}

CCFLAGS = $(BASEFLAGS) \
		  -std=gnu18 \
          -Wnested-externs \
		  -Wmissing-prototypes -Wstrict-prototypes

CPPFLAGS = $(BASEFLAGS) \
		   -std=gnu++26 \
		   -fno-rtti -fno-exceptions

ifeq ($(HOSTOS),Darwin)
CCFLAGS += $(shell pkg-config --cflags-only-I valgrind)
LD = ld
LDFLAGS = -lc $(shell pkgconfig --libs valgrind|cut -f1 -d " ") -lcoregrind-amd64-darwin -lvex-amd64-darwin
else
CCFLAGS += $(shell pkg-config --cflags-only-I valgrind)
LD = gcc
LDFLAGS = $(shell pkg-config --libs valgrind|cut -f1 -d " ") -lcoregrind-amd64-linux -lvex-amd64-linux \
		  -Wl,--gc-sections -Wl,--allow-multiple-definition -Wl,-static -Wl,-nostdlib -nostdlib -Wl,-e_tos_start -Wl,-pie 
endif

OUTPUT = ../build
LDSRCDIR = ../lds
TESTOUTPUT = $(OUTPUT)/cc-local
NOSTDLIB_DIR = nostdlib
NOSTDLIB_OUTPUT = $(TESTOUTPUT)/$(NOSTDLIB_DIR)

CCSOURCEDIR = ../cc

NOSTDLIB_SOURCES = $(shell find nostdlib -maxdepth 1 -type f -name \*.c)

CCSOURCES = $(shell find . -maxdepth 1 -type f -name \*.c )
CPPSOURCES = $(shell find . -maxdepth 1 -type f -name \*.cpp )

CCPROGS = $(patsubst ./%.c,%,$(CCSOURCES))
CPPPROGS = $(patsubst ./%.cpp,%,$(CPPSOURCES))

PROGS = $(CCPROGS) $(CPPPROGS)

NOSTDLIB_OBJECTS = $(patsubst $(NOSTDLIB_DIR)/%.c,$(NOSTDLIB_OUTPUT)/%.o,$(NOSTDLIB_SOURCES))

OUTPUTS := $(patsubst %,$(OUTPUT)/%.bin,$(PROGS))

.PHONY: all clean depend
.PRECIOUS: $(TESTOUTPUT)/%.o $(NOSTDLIB_OBJECTS)
all: $(OUTPUTS) 

gendirs:
	find $(OUTPUT)/cc -type d |sed 's%'$(OUTPUT)'/cc%'$(OUTPUT)'/cc-local%' |xargs mkdir -p
	mkdir -p $(NOSTDLIB_OUTPUT)

$(TESTOUTPUT)/%.o: %.c setup.h
	$(CC) $(CCFLAGS) -o $@ $<

$(TESTOUTPUT)/%.xx_64.o: $(CCSOURCEDIR)/%.xx.c
	$(CC) $(CCFLAGS) -o $@ $<

$(TESTOUTPUT)/%.64.o: $(CCSOURCEDIR)/%.64.c
	$(CC) $(CCFLAGS) -o $@ $<

$(TESTOUTPUT)/%.64.o: $(CCSOURCEDIR)/%.64.cpp
	$(CPP) $(CPPFLAGS) -o $@ $<

$(OUTPUT)/%.bin: $(NOSTDLIB_OBJECTS) $(TESTOUTPUT)/%.o
	$(LD) $(LDFLAGS) -o $@ $^

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
