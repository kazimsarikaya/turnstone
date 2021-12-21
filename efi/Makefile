ARCH=x86_64

CC = x86_64-w64-mingw32-gcc
LD = x86_64-w64-mingw32-gcc

CFLAGS += -D___BITS=64 -I../includes -I./ -mno-ms-bitfields
CFLAGS +=  -ffreestanding -nostdlib -Wall -Werror -c

LDFLAGS += -nostdlib -Wl,-dll -shared -Wl,--subsystem,10 -e efi_main

OUTPUT = ../output
EFIOUTPUT = $(OUTPUT)/efi

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
