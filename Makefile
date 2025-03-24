# This work is licensed under TURNSTONE OS Public License.
# Please read and understand latest version of Licence.

HOSTOS =$(shell uname -s)

MAKE = make

ifeq ($(HOSTOS),Darwin)

CC64 = x86_64-elf-gcc
CPP64 = x86_64-elf-g++
OBJCOPY = x86_64-elf-objcopy

SEDNOBAK = sed -i ''

else

CC64 = gcc
CPP64 = g++
OBJCOPY = objcopy

SEDNOBAK = sed -i

endif

DOCSGEN = doxygen
DOCSFILES = $(shell find . -type f -name \*.md)
DOCSCONF = docs.doxygen
INCLUDESDIR = includes

BASEFLAGS += -O3 -nostdlib -nostdinc -ffreestanding -fno-builtin -c -I$(INCLUDESDIR) \
	-Werror -Wall -Wextra -ffunction-sections -fdata-sections \
	-mno-red-zone -fstack-protector-all -fno-omit-frame-pointer \
    -Wshadow -Wpointer-arith -Wcast-align \
	-Wwrite-strings -Wmissing-declarations \
    -Wredundant-decls -Winline -Wno-long-long \
	-fPIC -fpic -fplt -mcmodel=large -fno-ident -fno-asynchronous-unwind-tables ${CCXXEXTRAFLAGS} \
	-D___KERNELBUILD=1

CCFLAGS = $(BASEFLAGS) \
		  -std=gnu18 \
          -Wnested-externs \
		  -Wmissing-prototypes -Wstrict-prototypes
CPPFLAGS = $(BASEFLAGS) \
		   -std=gnu++26 \
		   -fno-rtti -fno-exceptions

CXXTESTFLAGS= -D___TESTMODE=1

CC64FLAGS    = -m64 -march=x86-64 -D___BITS=64 -msse4.2 -mavx -mavx2 -mavx512f $(CCFLAGS)
CC64INTFLAGS = -m64 -march=x86-64 -mgeneral-regs-only -D___BITS=64 $(CCFLAGS)

OBJDIR = build
ASOBJDIR = $(OBJDIR)/asm
CCOBJDIR = $(OBJDIR)/cc
DOCSOBJDIR = $(OBJDIR)/docs
ASSRCDIR = asm
CCSRCDIR = cc
LDSRCDIR = lds
CCGENDIR = cc-gen
INCLUDESGENDIR = includes-gen
CCGENSCRIPTSDIR = scripts/gen-cc
TMPDIR = tmp
ASSETSDIR = assets
ASSETOBJDIR = $(OBJDIR)/assets
EFISRCDIR = efi
UTILSSRCDIR = utils

VBBOXDISK = /Volumes/DATA/VirtualBox\ VMs/osdev-vms/osdev/rawdisk0.raw
QEMUDISK  = $(OBJDIR)/qemu-hda
TESTQEMUDISK  = $(OBJDIR)/qemu-test-hda
TOSDBIMGNAME = tosdb.img
TOSDBIMG = $(OBJDIR)/$(TOSDBIMGNAME)

AS64SRCS = $(shell find $(ASSRCDIR) -type f -name \*64.S)
CC64SRCS = $(shell find $(CCSRCDIR) -type f -name \*.64.c)
CPP64SRCS = $(shell find $(CCSRCDIR) -type f -name \*.64.cpp)
CC64TESTSRCS = $(shell find $(CCSRCDIR) -type f -name \*.64.test.c)

CCXXSRCS = $(shell find $(CCSRCDIR) -type f -name \*.xx.c)
CCXXTESTSRCS = $(shell find $(CCSRCDIR) -type f -name \*.xx.test.c)

LDSRCS = $(shell find $(LDSRCDIR) -type f -name \*.ld)

UTILSSRCS = $(shell find $(UTILSSRCDIR) -type f -name \*.c)

CCGENSCRIPTS = $(shell find $(CCGENSCRIPTSDIR) -type f -name \*.sh)
GENCCSRCS = $(patsubst $(CCGENSCRIPTSDIR)/%.sh,$(CCGENDIR)/%.c,$(CCGENSCRIPTS))

ASOBJS = $(patsubst $(ASSRCDIR)/%.s,$(ASOBJDIR)/%.o,$(ASSRCS))

ASTESTOBJS = $(patsubst $(ASSRCDIR)/%.s,$(ASOBJDIR)/%.test.o,$(ASSRCS))

CC64OBJS = $(patsubst $(CCSRCDIR)/%.64.c,$(CCOBJDIR)/%.64.o,$(CC64SRCS))
CC64OBJS += $(patsubst $(CCSRCDIR)/%.xx.c,$(CCOBJDIR)/%.xx_64.o,$(CCXXSRCS))
CC64GENOBJS = $(patsubst $(CCGENSCRIPTSDIR)/%.sh,$(CCOBJDIR)/%.cc-gen.x86_64.o,$(CCGENSCRIPTS))

CC64ASMOUTS = $(patsubst $(CCSRCDIR)/%.64.c,$(CCOBJDIR)/%.64.s,$(CC64SRCS))
CC64ASMOUTS += $(patsubst $(CCSRCDIR)/%.xx.c,$(CCOBJDIR)/%.xx_64.s,$(CCXXSRCS))
CC64GENASMOUTS = $(patsubst $(CCGENSCRIPTSDIR)/%.sh,$(CCOBJDIR)/%.cc-gen.x86_64.s,$(CCGENSCRIPTS))

CC64TESTOBJS = $(patsubst $(CCSRCDIR)/%.64.test.c,$(CCOBJDIR)/%.64.test.o,$(CC64TESTSRCS))
CC64TESTOBJS += $(patsubst $(CCSRCDIR)/%.xx.test.c,$(CCOBJDIR)/%.xx_64.test.o,$(CCXXTESTSRCS))

CPP64OBJS = $(patsubst $(CCSRCDIR)/%.64.cpp,$(CCOBJDIR)/%.64.o,$(CPP64SRCS))

DOCSFILES += $(CC64SRCS) $(CCXXSRCS) $(CPP64SRCS)
DOCSFILES += $(shell find $(INCLUDESDIR) -type f -name \*.h)
DOCSFILES += $(shell find $(EFISRCDIR) -type f -name \*.c)
DOCSFILES += $(shell find $(EFISRCDIR) -type f -name \*.h)
DOCSFILES += $(shell find $(UTILSSRCDIR) -type f -name \*.c)
DOCSFILES += $(shell find $(UTILSSRCDIR) -type f -name \*.h)

ASSETS = $(shell find $(ASSETSDIR) -type f)
ASSETOBJS = $(patsubst %,$(OBJDIR)/%.data.o,$(ASSETS))

OBJS = $(ASOBJS) $(CC64OBJS) $(ASSETOBJS) $(CC64ASMOUTS) $(CPP64OBJS)
TESTOBJS= $(ASTESTOBJS) $(CC64TESTOBJS)

ifeq (,$(wildcard $(TOSDBIMG)))
LASTCCOBJS = $(CC64OBJS) $(CC64GENOBJS) $(ASSETOBJS) $(CPP64OBJS)
else
LASTCCOBJS = $(shell find $(CCOBJDIR) -type f -name \*.o -newer $(TOSDBIMG))
LASTCCOBJS += $(shell find $(ASSETOBJDIR) -type f -name \*.o -newer $(TOSDBIMG))
endif

MKDIRSDONE = .mkdirsdone

EFIDISKTOOL = $(OBJDIR)/efi_disk.bin
EFIBOOTFILE = $(OBJDIR)/BOOTX64.EFI
TOSDBIMG_BUILDER = $(OBJDIR)/generatelinkerdb.bin
PXECONFGEN = $(OBJDIR)/pxeconfgen.bin

PROGS = $(OBJDIR)/stage3.bin.pack

TESTPROGS = $(OBJDIR)/stage3.test.bin

.PHONY: all clean depend $(SUBDIRS) $(CC64GENOBJS) asm bear
.PRECIOUS:

qemu-without-analyzer:
	CCXXEXTRAFLAGS= make -j $(shell nproc) -C utils 
	CCXXEXTRAFLAGS= make -j $(shell nproc) -C efi 
	CCXXEXTRAFLAGS= make -j 1 -f Makefile-np 
	CCXXEXTRAFLAGS= make -j $(shell nproc) -f Makefile qemu-internal

qemu: 
	CCXXEXTRAFLAGS=-fanalyzer make -j $(shell nproc) -C utils 
	CCXXEXTRAFLAGS=-fanalyzer make -j $(shell nproc) -C efi 
	CCXXEXTRAFLAGS=-fanalyzer make -j 1 -f Makefile-np 
	CCXXEXTRAFLAGS=-fanalyzer make -j $(shell nproc) -f Makefile qemu-internal

qemu-internal: $(QEMUDISK)

$(PXECONFGEN): utils/pxeconfgen.c
	make -C utils ../$@

$(EFIBOOTFILE): efi/main.c
	make -C efi ../$@

qemu-pxe: $(TOSDBIMG) $(PXECONFGEN) $(EFIBOOTFILE)
	$(PXECONFGEN) -dbn $(TOSDBIMGNAME) -dbp $(TOSDBIMG) -o $(OBJDIR)/pxeconf.bson

qemu-test: $(TESTQEMUDISK)

virtualbox: $(VBBOXDISK)

asm:
	CCXXEXTRAFLAGS=-fanalyzer make -j 1 -f Makefile-np asm
	CCXXEXTRAFLAGS=-fanalyzer make -j $(shell nproc) -f Makefile asm-internal

asm-internal: $(CC64ASMOUTS)

bear:
	bear --output $(OBJDIR)/compile_commands.json --append -- make qemu
	bear --output $(OBJDIR)/compile_commands.json --append -- make -j $(shell nproc) -C tests

test: qemu-test
	scripts/osx-hacks/qemu-hda-test.sh

gendirs:
	mkdir -p $(CCGENDIR) $(INCLUDESGENDIR) $(ASOBJDIR) $(CCOBJDIR) $(DOCSOBJDIR) $(TMPDIR)
	find $(CCSRCDIR) -type d -exec mkdir -p $(OBJDIR)/{} \;
	find $(ASSETSDIR) -type d -exec mkdir -p $(OBJDIR)/{} \;
	make -C efi gendirs
	make -C tests gendirs
	make -C utils gendirs

$(OBJDIR)/docs: $(DOCSCONF) $(DOCSFILES)
	$(DOCSGEN) $(DOCSCONF)
	find $(OBJDIR)/docs/html/ -name "*.html"|sed 's-'$(OBJDIR)'/docs/html-https://turnstoneos.com-' > $(OBJDIR)/docs/html/sitemap.txt
	touch $(OBJDIR)/docs

$(VBBOXDISK): $(MKDIRSDONE) $(CC64GENOBJS) $(TOSDBIMG)
	$(EFIDISKTOOL) $(VBBOXDISK) $(EFIBOOTFILE) $(TOSDBIMG)

$(QEMUDISK): $(MKDIRSDONE) $(CC64GENOBJS) $(TOSDBIMG)
	$(EFIDISKTOOL) $(QEMUDISK) $(EFIBOOTFILE) $(TOSDBIMG)

$(TESTQEMUDISK): $(TESTDISK) $(TOSDBIMG)
	$(EFIDISKTOOL) $(QEMUDISK) $(EFIBOOTFILE) $(TOSDBIMG)

$(MKDIRSDONE):
	mkdir -p $(CCGENDIR) $(ASOBJDIR) $(CCOBJDIR)
	touch $(MKDIRSDONE)

$(TOSDBIMG): $(TOSDBIMG_BUILDER) $(CC64OBJS) $(CC64GENOBJS) $(ASSETOBJS) $(CPP64OBJS)
	$(TOSDBIMG_BUILDER) -o $@ $(LASTCCOBJS)

$(CCOBJDIR)/%.64.o: $(CCSRCDIR)/%.64.c
	$(CC64) $(CC64FLAGS) -o $@ $<

$(CCOBJDIR)/%.64.o: $(CCSRCDIR)/%.64.cpp
	$(CPP64) $(CPPFLAGS) -o $@ $<

$(CCOBJDIR)/%.xx_64.o: $(CCSRCDIR)/%.xx.c
	$(CC64) $(CC64FLAGS) -o $@ $<

$(CCOBJDIR)/%.xx_64.test.o: $(CCSRCDIR)/%.xx.test.c
	$(CC64) $(CC64FLAGS) $(CXXTESTFLAGS) -o $@ $<
	
$(CCOBJDIR)/%.64.s: $(CCSRCDIR)/%.64.c
	$(CC64) $(CC64FLAGS) -S -o $@ $<

$(CCOBJDIR)/%.xx_64.s: $(CCSRCDIR)/%.xx.c
	$(CC64) $(CC64FLAGS) -S -o $@ $<
	
$(CCOBJDIR)/%.64.test.s: $(CCSRCDIR)/%.64.test.c
	$(CC64) $(CC64FLAGS) -S $(CXXTESTFLAGS) -o $@ $<

$(ASOBJDIR)/%64.o: $(ASSRCDIR)/%64.S
	$(CC64) $(CC64FLAGS) -o $@ $^

$(ASOBJDIR)/%64.test.o: $(ASSRCDIR)/%64.S
	$(CC64) $(CC64FLAGS) $(CXXTESTFLAGS) -o $@ $^

$(CCOBJDIR)/cpu/interrupt.64.o: $(CCSRCDIR)/cpu/interrupt.64.c
	$(CC64) $(CC64INTFLAGS) -o $@ $<
	
$(CCOBJDIR)/cpu/interrupt.64.s: $(CCSRCDIR)/cpu/interrupt.64.c
	$(CC64) $(CC64INTFLAGS) -S -o $@ $<

$(CCOBJDIR)/%.cc-gen.x86_64.o: $(CCGENDIR)/%.c
	$(CC64) $(CC64INTFLAGS) -o $@ $<

$(SUBDIRS):
	$(MAKE) -j $(nproc) -C $@

$(ASSETOBJS): $(ASSETS) 
	scripts/assets/build.sh $@

clean:
	rm -fr $(DOCSOBJDIR)/*
	if [ -d $(OBJDIR) ]; then find $(OBJDIR) -type f -delete; fi
	find . -type f -name .depend\* -delete
	if [ -d $(CCGENDIR) ]; then find $(CCGENDIR) -type f -delete; fi
	if [ -d $(INCLUDESGENDIR) ]; then find $(INCLUDESGENDIR) -type f -delete; fi
	rm -f $(MKDIRSDONE)
	rm -f compile_commands.json

cleandirs:
	rm -fr $(CCGENDIR) $(INCLUDESGENDIR) $(OBJDIR)

print-%: ; @echo $* = $($*)

depend: .depend64c .depend64cpp

.depend64c: $(CC64SRCS) $(CCXXSRCS) $(CC64TESTSRCS)
	scripts/create-cc-deps.sh "$(CC64) $(CC64FLAGS) -D___DEPEND_ANALYSIS -MM" "$^" > .depend64
	$(SEDNOBAK) 's/xx.o:/xx_64.o:/g' .depend64

.depend64cpp: $(CPP64SRCS)
	scripts/create-cc-deps.sh "$(CPP64) $(CPPFLAGS) -D___DEPEND_ANALYSIS -MM" "$^" >> .depend64
	$(SEDNOBAK) 's/xx.o:/xx_64.o:/g' .depend64


-include .depend64
