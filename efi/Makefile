# This work is licensed under TURNSTONE OS Public License.
# Please read and understand latest version of Licence.

ARCH=x86_64

OUTPUT = ../output
EFIOUTPUT = $(OUTPUT)/efi

HOSTOS =$(shell uname -s)

ifeq ($(HOSTOS),Darwin)
  
CC = x86_64-elf-gcc

else

CC = gcc

endif

LD = $(OUTPUT)/linker.bin

CFLAGS += -D___BITS=64 -D___EFIBUILD=1 -std=gnu11 -O2 -I../includes -I./
CFLAGS += -ffreestanding -nostdlib -nostdinc -Wall -Werror -Wextra -mgeneral-regs-only
# CFLAGS += -fpic -fno-plt -mcmodel=large -fno-jump-tables  
CFLAGS += -ffunction-sections -fdata-sections -c ${CCXXEXTRAFLAGS}
CFLAGS += -Wshadow -Wpointer-arith -Wcast-align \
          -Wwrite-strings -Wmissing-prototypes -Wmissing-declarations \
          -Wredundant-decls -Wnested-externs -Winline -Wno-long-long \
          -Wstrict-prototypes

LDFLAGS = --for-efi --trim -o efitest.efi -e efi_main

CCSOURCEDIR = ../cc

EFISRCS = $(shell find . -type f -name \*.c)
EFIOBJS = $(patsubst ./%.c,$(EFIOUTPUT)/%.o,$(EFISRCS))

EFIAPP = $(OUTPUT)/BOOTX64.EFI

.PHONY: all clean depend
.PRECIOUS: $(EFIOUTPUT)/%.o

all: $(EFIAPP)

gendirs:
	find ../output/cc -type d |sed 's%../output/cc%../output/efi%' |xargs mkdir -p

$(EFIOUTPUT)/%.o: %.c
	$(CC) $(CFLAGS) -o $@ $<

$(EFIOUTPUT)/%.xx_64.o: $(CCSOURCEDIR)/%.xx.c
	$(CC) $(CFLAGS) -o $@ $<

$(EFIOUTPUT)/%.64.o: $(CCSOURCEDIR)/%.64.c
	$(CC) $(CFLAGS) -o $@ $<

$(EFIAPP): $(EFIOBJS)
	$(LD) $(LDFLAGS) -o $@ $(EFIOBJS)

clean:
	rm -fr $(EFIAPP)
	rm -fr $(EFIOUTPUT)/*
	rm -f .depend

print-%  : ; @echo $* = $($*)

depend: .depend

.depend: $(EFISRCS)
	../scripts/create_efi_depends.sh $(EFIAPP) $(EFISRCS)  >.depend

-include .depend
