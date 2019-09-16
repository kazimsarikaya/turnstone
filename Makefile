CC = i386-elf-gcc
AS = i386-elf-as
LD = i386-elf-ld

LDFLAGS = --nmagic -s
CCFLAGS = -std=gnu99 -Os -nostdlib -m32 -march=i386 -ffreestanding -c -Iincludes
ASFLAGS = --32

OBJDIR = output
ASOBJDIR = $(OBJDIR)/asm
CCOBJDIR = $(OBJDIR)/cc
ASSRCDIR = asm
CCSRCDIR = cc
LDSRCDIR = lds

ASSRCS = $(shell find $(ASSRCDIR) -type f -name \*.asm)
CCSRCS = $(shell find $(CCSRCDIR) -type f -name \*.c)
LDSRCS = $(shell find $(LDSRCDIR) -type f -name \*.ld)

ASOBJS = $(patsubst $(ASSRCDIR)/%.asm,$(ASOBJDIR)/%.o,$(ASSRCS))
CCOBJS = $(patsubst $(CCSRCDIR)/%.c,$(CCOBJDIR)/%.o,$(CCSRCS))
OBJS = $(ASOBJS) $(CCOBJS)
PROGS = $(OBJDIR)/bootsect.bin $(OBJDIR)/stage2.bin


.PHONY: all disk clean
all: disk

disk: $(OBJDIR)/kernel
	dd bs=512 conv=notrunc if=$< of=/Volumes/DATA/VirtualBox\ VMs/osdev/rawdisk0.raw

$(OBJDIR)/kernel: $(PROGS)
	cat $^ > $@

$(OBJDIR)/bootsect.bin: $(LDSRCDIR)/bootsect.ld $(ASOBJDIR)/bootsect.o
	$(LD) $(LDFLAGS) --script=$<  -o $@ $(filter-out $<,$^)

$(OBJDIR)/stage2.bin: $(LDSRCDIR)/stage2.ld $(ASOBJDIR)/kentry.o $(CCOBJS)
	$(LD) $(LDFLAGS) --script=$< -o $@ $^

$(CCOBJDIR)/%.o: $(CCSRCDIR)/%.c
	$(CC) $(CCFLAGS) -o $@ $^

$(ASOBJDIR)/%.o: $(ASSRCDIR)/%.asm
	$(AS) $(ASFLAGS) -o $@ $^

clean:
	find $(OBJDIR) -type f -delete

print-%  : ; @echo $* = $($*)
