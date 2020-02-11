CC = gcc
CCFLAGS = -Wall -Werror
PROGS = formatslots
OUTPUT = ../output

OUTPUTS := $(OUTPUT)/$(PROGS).bin

all: $(OUTPUTS)

$(OUTPUT)/%.bin: %.c
	$(CC) $(CCFLAGS) -o $@ $<
