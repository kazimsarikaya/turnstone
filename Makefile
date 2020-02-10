CC16 = i386-elf-gcc
CC64 = x86_64-elf-gcc
AS16 = i386-elf-as
AS64 = x86_64-elf-as
LD16 = i386-elf-ld
LD64 = x86_64-elf-ld

LD16FLAGS = --nmagic -s
LD64FLAGS = --nmagic -s
CC16FLAGS = -std=gnu99 -Os -nostdlib -m32 -march=i386 -ffreestanding -c -Iincludes/16 -Werror -Wall
CC64FLAGS = -std=gnu99 -Os -nostdlib -m64 -march=x86-64 -ffreestanding -c -Iincludes/64 -Werror -Wall
AS16FLAGS = --32
AS64FLAGS = --64

OBJDIR = output
ASOBJDIR = $(OBJDIR)/asm
CCOBJDIR = $(OBJDIR)/cc
ASSRCDIR = asm
CCSRCDIR = cc
LDSRCDIR = lds

AS16SRCS = $(shell find $(ASSRCDIR) -type f -name \*16.asm)
AS64SRCS = $(shell find $(ASSRCDIR) -type f -name \*64.asm)
CC16SRCS = $(shell find $(CCSRCDIR) -type f -name \*16.c)
CC64SRCS = $(shell find $(CCSRCDIR) -type f -name \*64.c)
LDSRCS = $(shell find $(LDSRCDIR) -type f -name \*.ld)

ASOBJS = $(patsubst $(ASSRCDIR)/%.asm,$(ASOBJDIR)/%.o,$(ASSRCS))
CC16OBJS = $(patsubst $(CCSRCDIR)/%.c,$(CCOBJDIR)/%.o,$(CC16SRCS))
CC64OBJS = $(patsubst $(CCSRCDIR)/%.c,$(CCOBJDIR)/%.o,$(CC64SRCS))
OBJS = $(ASOBJS) $(CC16OBJS) $(CC64OBJS)
PROGS = $(OBJDIR)/bootsect16.bin $(OBJDIR)/stage2.bin $(OBJDIR)/stage3.bin


.PHONY: all disk clean depend
all: disk

disk: $(OBJDIR)/kernel
	dd bs=512 conv=notrunc if=$< of=/Volumes/DATA/VirtualBox\ VMs/osdev/rawdisk0.raw

$(OBJDIR)/kernel: $(PROGS)
	cat $^ > $@

$(OBJDIR)/bootsect16.bin: $(LDSRCDIR)/bootsect.ld $(ASOBJDIR)/bootsect16.o
	$(LD16) $(LDFLAGS) --script=$<  -o $@ $(filter-out $<,$^)

$(OBJDIR)/stage2.bin: $(LDSRCDIR)/stage2.ld $(ASOBJDIR)/kentry16.o $(CC16OBJS)
	$(LD16) $(LD16FLAGS) --script=$< -o $@ $^

$(OBJDIR)/stage3.bin: $(LDSRCDIR)/stage3.ld $(ASOBJDIR)/kentry64.o $(CC64OBJS)
	$(LD64) $(LD64FLAGS) -T $< -o $@ $^

$(CCOBJDIR)/%16.o: $(CCSRCDIR)/%16.c
	$(CC16) $(CC16FLAGS) -o $@ $<

$(CCOBJDIR)/%64.o: $(CCSRCDIR)/%64.c
	$(CC64) $(CC64FLAGS) -o $@ $<

$(ASOBJDIR)/%16.o: $(ASSRCDIR)/%16.asm
	$(AS16) $(AS16FLAGS) -o $@ $^

$(ASOBJDIR)/%64.o: $(ASSRCDIR)/%64.asm
	$(AS64) $(AS64FLAGS) -o $@ $^

clean:
	find $(OBJDIR) -type f -delete
	rm -f .depend*

print-%  : ; @echo $* = $($*)

depend: .depend16 .depend64

.depend16: $(CC16SRCS)
	$(CC16) $(CC16FLAGS) -MM $^ | sed -E 's%^(.*):%'$(CCOBJDIR)'/\1:%g' > .depend16

.depend64: $(CC64SRCS)
	$(CC64) $(CC64FLAGS) -MM $^ | sed -E 's%^(.*):%'$(CCOBJDIR)'/\1:%g' > .depend64

-include .depend16 .depend64
