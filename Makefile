CC = i386-elf-gcc
AS = i386-elf-as
LD = i386-elf-ld

LDFLAGS = --nmagic -s
CCFLAGS = -std=gnu99 -Os -nostdlib -m32 -march=i386 -ffreestanding -c
ASFLAGS = --32

OBJDIR = output
ASOBJDIR = $(OBJDIR)/asm
CCOBJDIR = $(OBJDIR)/cc
ASSRCDIR = asm
CCSRCDIR = cc

ASSRCS = $(shell find $(ASSRCDIR) -type f -name \*.asm)
CCSRCS = $(shell find $(CCSRCDIR) -type f -name \*.c)

ASOBJS = $(patsubst $(ASSRCDIR)/%.asm,$(ASOBJDIR)/%.o,$(ASSRCS))
CCOBJS = $(patsubst $(CCSRCDIR)/%.c,$(CCOBJDIR)/%.o,$(CCSRCS))
OBJS = $(ASOBJS) $(CCOBJS)
PROGS = $(OBJDIR)/bootsect.bin $(OBJDIR)/stage2.bin

all: disk

disk: $(OBJDIR)/kernel
	dd bs=512 conv=notrunc if=$< of=/Volumes/DATA/VirtualBox\ VMs/osdev/rawdisk0.raw

$(OBJDIR)/kernel: $(PROGS)
	cat $^ > $@

$(OBJDIR)/bootsect.bin: $(ASOBJDIR)/bootsect.o
	$(LD) $(LDFLAGS) --script=lds/bootsect.ld  -o $@ $^

$(OBJDIR)/stage2.bin: $(ASOBJDIR)/kentry.o $(CCOBJS)
	$(LD) $(LDFLAGS) --script=lds/stage2.ld -o $@ $^

$(CCOBJDIR)/%.o: $(CCSRCDIR)/%.c
	$(CC) $(CCFLAGS) -o $@ $^

$(ASOBJDIR)/%.o: $(ASSRCDIR)/%.asm
	$(AS) $(ASFLAGS) -o $@ $^

clean:
	find $(OBJDIR) -type f -delete

print-%  : ; @echo $* = $($*)
