# This work is licensed under TURNSTONE OS Public License.
# Please read and understand latest version of Licence.

HOSTOS=$(shell uname -s)

CC = gcc
CPP = g++

BASEFLAGS = -Wall -Werror -Wextra -D___BITS=64 -I../includes \
	-O3 -nostdlib -ffreestanding -c -m64 -march=native \
	-fdata-sections -ffunction-sections \
	-Wshadow -Wpointer-arith -Wcast-align \
	-Wwrite-strings -Wmissing-declarations \
    -Wredundant-decls -Winline -Wno-long-long \
    -fPIC -fplt -mcmodel=large\
	-D___TESTMODE=1 -D___KERNELBUILD=0 -g ${CCXXEXTRAFLAGS}

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
		  -Wl,--gc-sections -static -Wl,-static -Wl,--allow-multiple-definition
endif


OUTPUT = ../build
UTILOUTPUT = $(OUTPUT)/cc-local

CCSOURCEDIR = ../cc

CCSOURCES = $(shell find . -type f -name \*.c )
CPPSOURCES = $(shell find . -type f -name \*.cpp )

CCPROGS = $(patsubst ./%.c,%,$(CCSOURCES))
CPPPROGS = $(patsubst ./%.cpp,%,$(CPPSOURCES))

PROGS = $(CCPROGS) $(CPPPROGS)

OUTPUTS := $(patsubst %,$(OUTPUT)/%.bin,$(PROGS))

.PHONY: all clean depend
.PRECIOUS: $(UTILOUTPUT)/%.o
all: $(OUTPUTS)

gendirs:
	find $(OUTPUT)/cc -type d |sed 's%'$(OUTPUT)'/cc%'$(OUTPUT)'/cc-local%' |xargs mkdir -p

$(UTILOUTPUT)/%.o: %.c setup.h
	$(CC) $(CCFLAGS) -o $@ $<

$(UTILOUTPUT)/%.o: %.cpp setup.h
	$(CPP) $(CPPFLAGS) -o $@ $<

$(UTILOUTPUT)/%.xx_64.o: $(CCSOURCEDIR)/%.xx.c
	$(CC) $(CCFLAGS) -o $@ $<

$(UTILOUTPUT)/%.64.o: $(CCSOURCEDIR)/%.64.c
	$(CC) $(CCFLAGS) -o $@ $<

$(UTILOUTPUT)/%.64.o: $(CCSOURCEDIR)/%.64.cpp
	$(CPP) $(CPPFLAGS) -o $@ $<

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
