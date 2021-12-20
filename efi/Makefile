ARCH=x86_64

CC = x86_64-w64-mingw32-gcc
LD = x86_64-w64-mingw32-gcc

CFLAGS += -D___BITS=64 -I/Users/kazim/Development/osdev/includes -I/Users/kazim/Development/osdev/utils
CFLAGS +=  -ffreestanding -nostdlib -Wall -Werror

LDFLAGS += -nostdlib -Wl,-dll -shared -Wl,--subsystem,10 -e efi_main

OUTPUT = ../output
EFIOUTPUT = $(OUTPUT)/efi


EFISRCS = $(shell find . -type f -name \*.c)
EFIOBJS = $(patsubst ./%.c,$(EFIOUTPUT)/%.o,$(EFISRCS))

EFIAPP = $(OUTPUT)/BOOTX64.EFI

all: $(EFIAPP)

gendirs:
	mkdir -p $(EFIOUTPUT)

$(OUTPUT)/BOOTX64.EFI: $(EFIOBJS)
	$(LD) $(LDFLAGS) -o $@ $<

$(EFIOUTPUT)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -fr $(EFIAPP)
	rm -fr $(EFIOUTPUT)/*
