CC = gcc
CCFLAGS = -Wall -Werror
SOURCES = $(shell find . -type f -name \*.c)
PROGS = $(patsubst ./%.c,%,$(SOURCES))
OUTPUT = ../output

OUTPUTS := $(patsubst %,$(OUTPUT)/%.bin,$(PROGS))

all: $(OUTPUTS)

$(OUTPUT)/%.bin: %.c
	$(CC) $(CCFLAGS) -o $@ $<

print-%  : ; @echo $* = $($*)
