# This work is licensed under TURNSTONE OS Public License.
# Please read and understand latest version of Licence.

ARCH=x86_64

OUTPUT = ../build
EFIOUTPUT = $(OUTPUT)/efi

HOSTOS =$(shell uname -s)

ifeq ($(HOSTOS),Darwin)
  
CC = x86_64-elf-gcc
CPP = x86_64-elf-g++

else

CC = gcc
CPP = g++

endif


BASEFLAGS = -D___BITS=64 -D___EFIBUILD=1 -D___KERNELBUILD=0 -O3 -I../includes -I./
BASEFLAGS += -ffreestanding -nostdlib -nostdinc -fno-builtin -Wall -Werror -Wextra
BASEFLAGS += -fno-omit-frame-pointer -msse4.2 
BASEFLAGS += -fno-pic -fno-PIC -fno-plt -mcmodel=large
BASEFLAGS += -fno-ident -fno-asynchronous-unwind-tables
BASEFLAGS += -ffunction-sections -fdata-sections -c ${CCXXEXTRAFLAGS}
BASEFLAGS += -Wshadow -Wpointer-arith -Wcast-align \
          -Wwrite-strings -Wmissing-declarations \
          -Wredundant-decls -Winline -Wno-long-long

CCFLAGS = $(BASEFLAGS) \
		  -std=gnu18 \
          -Wnested-externs \
		  -Wmissing-prototypes -Wstrict-prototypes

CPPFLAGS = $(BASEFLAGS) \
		   -std=gnu++26 \
		   -fno-rtti -fno-exceptions

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
	find $(OUTPUT)/cc -type d |sed 's%'$(OUTPUT)'/cc%'$(OUTPUT)'/efi%' |xargs mkdir -p

$(EFIOUTPUT)/%.o: %.c
	$(CC) $(CCFLAGS) -o $@ $<

$(EFIOUTPUT)/%.xx_64.o: $(CCSOURCEDIR)/%.xx.c
	$(CC) $(CCFLAGS) -o $@ $<

$(EFIOUTPUT)/%.64.o: $(CCSOURCEDIR)/%.64.c
	$(CC) $(CCFLAGS) -o $@ $<

$(EFIOUTPUT)/%.64.o: $(CCSOURCEDIR)/%.64.cpp
	$(CPP) $(CPPFLAGS) -o $@ $<

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
