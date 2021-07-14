HOSTOS =$(shell uname -s)

MAKE = make

ifeq ($(HOSTOS),Darwin)

AS16 = i386-elf-as
CC16 = i386-elf-gcc

AS64 = x86_64-elf-as
CC64 = x86_64-elf-gcc

else

AS16 = as
CC16 = gcc

AS64 = as
CC64 = gcc

endif

DOCSGEN = doxygen
DOCSFILES = $(shell find . -type f -name \*.md)
DOCSCONF = docs.doxygen
INCLUDESDIR = includes

CCXXFLAGS = -std=gnu99 -Os -nostdlib -ffreestanding -c -I$(INCLUDESDIR) \
	-Werror -Wall -Wextra -ffunction-sections -fdata-sections \
	-mno-red-zone

AS16FLAGS = --32
CC16FLAGS = -m32 -march=i386 -mgeneral-regs-only -D___BITS=16 $(CCXXFLAGS)

AS64FLAGS    = --64
CC64FLAGS    = -m64 -march=x86-64 -D___BITS=64 $(CCXXFLAGS)
CC64INTFLAGS = -m64 -march=x86-64 -mgeneral-regs-only -D___BITS=64 $(CCXXFLAGS)

OBJDIR = output
ASOBJDIR = $(OBJDIR)/asm
CCOBJDIR = $(OBJDIR)/cc
DOCSOBJDIR = $(OBJDIR)/docs
ASSRCDIR = asm
CCSRCDIR = cc
LDSRCDIR = lds
CCGENDIR = cc-gen
CCGENSCRIPTSDIR = scripts/gen-cc
TMPDIR = tmp

DISK      = $(OBJDIR)/kernel
VBBOXDISK = /Volumes/DATA/VirtualBox\ VMs/osdev/rawdisk0.raw
QEMUDISK  = $(OBJDIR)/qemu-hda

AS16SRCS = $(shell find $(ASSRCDIR) -type f -name \*16.s)
CC16SRCS = $(shell find $(CCSRCDIR) -type f -name \*.16.c)

AS64SRCS = $(shell find $(ASSRCDIR) -type f -name \*64.s)
CC64SRCS = $(shell find $(CCSRCDIR) -type f -name \*.64.c)

CCXXSRCS = $(shell find $(CCSRCDIR) -type f -name \*.xx.c)

LDSRCS = $(shell find $(LDSRCDIR) -type f -name \*.ld)

UTILSSRCS = $(shell find $(UTILSSRCDIR) -type f -name \*.c)

CCGENSCRIPTS = $(shell find $(CCGENSCRIPTSDIR) -type f -name \*.sh)
GENCCSRCS = $(patsubst $(CCGENSCRIPTSDIR)/%.sh,$(CCGENDIR)/%,$(CCGENSCRIPTS))

ASOBJS = $(patsubst $(ASSRCDIR)/%.s,$(ASOBJDIR)/%.o,$(ASSRCS))
CC16OBJS = $(patsubst $(CCSRCDIR)/%.16.c,$(CCOBJDIR)/%.16.o,$(CC16SRCS))
CC16OBJS += $(patsubst $(CCSRCDIR)/%.xx.c,$(CCOBJDIR)/%.xx_16.o,$(CCXXSRCS))
CC64OBJS = $(patsubst $(CCSRCDIR)/%.64.c,$(CCOBJDIR)/%.64.o,$(CC64SRCS))
CC64OBJS += $(patsubst $(CCSRCDIR)/%.xx.c,$(CCOBJDIR)/%.xx_64.o,$(CCXXSRCS))
CC64OBJS += $(patsubst $(CCGENSCRIPTSDIR)/%.sh,$(CCOBJDIR)/%.cc-gen.x86_64.o,$(CCGENSCRIPTS))

DOCSFILES += $(CC16SRCS) $(CC64SRCS) $(CCXXSRCS)
DOCSFILES += $(shell find $(INCLUDESDIR) -type f -name \*.h)

OBJS = $(ASOBJS) $(CC16OBJS) $(CC64OBJS)
PROGS = $(OBJDIR)/bootsect16.bin \
	$(OBJDIR)/slottable.bin \
	$(OBJDIR)/stage2.bin \
	$(OBJDIR)/stage3.bin

SUBDIRS := tests utils

.PHONY: all clean depend $(SUBDIRS)
.PRECIOUS:
all: $(SUBDIRS) $(OBJDIR)/docs $(DISK)

qemu: $(QEMUDISK)

virtualbox: $(VBBOXDISK)

gendirs:
	mkdir -p $(CCGENDIR) $(ASOBJDIR) $(CCOBJDIR) $(DOCSOBJDIR) $(TMPDIR)
	make -C tests gendirs
	make -C utils gendirs
	find $(CCSRCDIR) -type d -exec mkdir -p $(OBJDIR)/{} \;

$(OBJDIR)/docs: $(DOCSCONF) $(DOCSFILES)
	$(DOCSGEN) $(DOCSCONF)
	touch $(OBJDIR)/docs

$(VBBOXDISK): $(DISK)
	dd bs=512 conv=notrunc if=$< of=$(VBBOXDISK)

$(QEMUDISK): $(DISK)
	rm -fr $(QEMUDISK)
	dd if=/dev/zero of=output/qemu-hda bs=1 count=0 seek=1073741824
	dd bs=512 conv=notrunc if=$< of=$(QEMUDISK)

$(DISK): $(MKDIRSDONE) $(GENCCSRCS) $(PROGS) utils
	cat $(PROGS) > $@
	$(OBJDIR)/formatslots.bin $@ $(PROGS)

$(MKDIRSDONE):
	mkdir -p $(CCGENDIR) $(ASOBJDIR) $(CCOBJDIR)
	touch $(MKDIRSDONE)

$(OBJDIR)/bootsect16.bin: $(OBJDIR)/linker.bin $(LDSRCDIR)/bootsect.ld $(ASOBJDIR)/bootsect16.o
	$(OBJDIR)/linker.bin --trim --boot-flag -o $@ -M $@.map -T $(filter-out $<,$^)

$(OBJDIR)/slottable.bin: $(OBJDIR)/linker.bin $(LDSRCDIR)/slottableprotect.ld $(ASOBJDIR)/slottableprotect16.o
	$(OBJDIR)/linker.bin --trim -o $@ -M $@.map -T $(filter-out $<,$^)

$(OBJDIR)/stage2.bin: $(OBJDIR)/linker.bin $(LDSRCDIR)/stage2.ld $(ASOBJDIR)/kentry16.o $(CC16OBJS)
	$(OBJDIR)/linker.bin --trim -o $@ -M $@.map -T $(filter-out $<,$^)

$(OBJDIR)/stage3.bin: $(OBJDIR)/linker.bin $(LDSRCDIR)/stage3.ld $(ASOBJDIR)/kentry64.o $(CC64OBJS)
	$(OBJDIR)/linker.bin --trim -o $@ -M $@.map -T $(filter-out $<,$^)

$(CCOBJDIR)/%.16.o: $(CCSRCDIR)/%.16.c
	$(CC16) $(CC16FLAGS) -o $@ $<

$(CCOBJDIR)/%.xx_16.o: $(CCSRCDIR)/%.xx.c
	$(CC16) $(CC16FLAGS) -o $@ $<

$(CCOBJDIR)/%.64.o: $(CCSRCDIR)/%.64.c
	$(CC64) $(CC64FLAGS) -o $@ $<

$(CCOBJDIR)/%.xx_64.o: $(CCSRCDIR)/%.xx.c
	$(CC64) $(CC64FLAGS) -o $@ $<

$(ASOBJDIR)/%16.o: $(ASSRCDIR)/%16.s
	$(AS16) $(AS16FLAGS) -o $@ $^

$(ASOBJDIR)/%64.o: $(ASSRCDIR)/%64.s
	$(AS64) $(AS64FLAGS) -o $@ $^

$(CCGENDIR)/%: $(CCGENSCRIPTSDIR)/%.sh
	$^ > $@.c.tmp
	diff $@.c  $@.c.tmp  2>/dev/null && rm $@.c.tmp || mv $@.c.tmp $@.c

$(CCOBJDIR)/%.cc-gen.x86_64.o: $(CCGENDIR)/%.c
	$(CC64) $(CC64FLAGS) -o $@ $<

$(CCOBJDIR)/cpu/interrupt.64.o: $(CCSRCDIR)/cpu/interrupt.64.c
	$(CC64) $(CC64INTFLAGS) -o $@ $<

$(CCOBJDIR)/interrupt_handlers.cc-gen.x86_64.o: $(CCGENDIR)/interrupt_handlers.c
	$(CC64) $(CC64INTFLAGS) -o $@ $<

$(SUBDIRS):
	$(MAKE) -C $@

$(OBJDIR)/linker.bin: utils ;


clean:
	rm -fr $(DOCSOBJDIR)/*
	find $(OBJDIR) -type f -delete
	rm -f .depend*
	find cc-gen -type f -delete
	find includes-gen -type f -delete

cleandirs:
	rm -fr $(CCGENDIR) $(OBJDIR)

print-%: ; @echo $* = $($*)

depend: .depend16 .depend64

.depend16: $(CC16SRCS) $(CCXXSRCS)
	$(CC16) $(CC16FLAGS) -D___DEPEND_ANALYSIS -MM $^ | sed -E 's%^(.*):%'$(CCOBJDIR)'/\1:%g' > .depend16
	sed -i '' 's/xx.o:/xx_16.o:/g' .depend16

.depend64: $(CC64SRCS) $(CCXXSRCS)
	$(CC64) $(CC64FLAGS) -D___DEPEND_ANALYSIS -MM $^ | sed -E 's%^(.*):%'$(CCOBJDIR)'/\1:%g' > .depend64
	sed -i '' 's/xx.o:/xx_64.o:/g' .depend64

-include .depend16 .depend64
