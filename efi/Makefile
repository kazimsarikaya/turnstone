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


CFLAGS = -D___BITS=64 -D___EFIBUILD=1 -D___KERNELBUILD=0 -std=gnu11 -O2 -I../includes -I./
CFLAGS += -ffreestanding -nostdlib -nostdinc -Wall -Werror -Wextra
CFLAGS += -fno-omit-frame-pointer -msse4.2 -fno-jump-tables
# CFLAGS += -fpic -fno-plt -mcmodel=large -fno-jump-tables  
CFLAGS += -ffunction-sections -fdata-sections -c ${CCXXEXTRAFLAGS}
CFLAGS += -Wshadow -Wpointer-arith -Wcast-align \
          -Wwrite-strings -Wmissing-prototypes -Wmissing-declarations \
          -Wredundant-decls -Wnested-externs -Winline -Wno-long-long \
          -Wstrict-prototypes -fplt -fno-pic

CCSOURCEDIR = ../cc

EFISRCS = $(shell find . -type f -name \*.c)
EFIOBJS = $(patsubst ./%.c,$(EFIOUTPUT)/%.o,$(EFISRCS))

EFIAPP = $(OUTPUT)/BOOTX64.EFI

TOSDB_IMAGE = $(OUTPUT)/tosdb-efi.img
TOSDB_BUILDER = $(OUTPUT)/generatelinkerdb.bin
TOSDB_BUILDER_FLAGS = -e efi_main 

LD = $(OUTPUT)/linker-tosdb.bin
LDFLAGS = -e efi_main -r  -psp 4096 -psv 4096 --for-efi

ifeq (,$(wildcard $(TOSDB_IMAGE)))
LASTCCOBJS = $(EFIOBJS)
else
LASTCCOBJS = $(shell find $(EFIOUTPUT) -type f -name \*.o -newer $(TOSDB_IMAGE))
endif

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

$(TOSDB_IMAGE): $(EFIOBJS) $(TOSDB_BUILDER)
	$(TOSDB_BUILDER) $(TOSDB_BUILDER_FLAGS) -o $@ $(LASTCCOBJS)

$(EFIAPP): $(TOSDB_IMAGE) $(LD)
	$(LD) $(LDFLAGS) -o $@ -db $(TOSDB_IMAGE)

$(TOSDB_BUILDER): ../utils/generatelinkerdb.c 
	make -C ../utils $@

$(LD): ../utils/linker-tosdb.c
	make -C ../utils $@

clean:
	rm -fr $(EFIAPP) $(TOSDB_IMAGE)
	rm -fr $(EFIOUTPUT)/*
	rm -f .depend

print-%  : ; @echo $* = $($*)

depend: .depend

.depend: $(EFISRCS)
	../scripts/create_efi_depends.sh $(TOSDB_IMAGE) $(EFISRCS)  >.depend

-include .depend
