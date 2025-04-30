# This work is licensed under TURNSTONE OS Public License.
# Please read and understand latest version of Licence.

CC64 = gcc
CPP64 = g++
OBJCOPY = objcopy
LOCALLD = gcc

SEDNOBAK = sed -i

DOCSGEN = doxygen
DOCSFILES = $(shell find . -type f -name \*.md)
DOCSCONF = docs.doxygen
INCLUDESDIR = includes
LOCALINCLUDESDIR = includes-local

BASEFLAGS += -O3 -nostdlib -nostdinc -ffreestanding -fno-builtin -c -I$(INCLUDESDIR) \
	-Werror -Wall -Wextra -ffunction-sections -fdata-sections \
	-mno-red-zone -fstack-protector-all -fno-omit-frame-pointer \
    -Wshadow -Wpointer-arith -Wcast-align \
	-Wwrite-strings -Wmissing-declarations \
    -Wredundant-decls -Winline -Wno-long-long \
	-D___BITS=64 -m64 -march=native \
	${CCXXEXTRAFLAGS}	

PIFLAGS = -fPIC -fpic -fplt -mcmodel=large 
NOPIFLAGS = -fno-pic -fno-PIC -fno-plt -mcmodel=large

CC64FLAGS = $(BASEFLAGS) \
		  -std=gnu18 \
          -Wnested-externs \
		  -Wmissing-prototypes -Wstrict-prototypes
CPP64FLAGS = $(BASEFLAGS) \
		   -std=gnu++26 \
		   -fno-rtti -fno-exceptions

CXXTESTFLAGS= -D___TESTMODE=1

KERNELCC64FLAGS = -fno-ident -fno-asynchronous-unwind-tables -D___KERNELBUILD=1 $(CC64FLAGS) $(PIFLAGS)
KERNELCPP64FLAGS =  -D___KERNELBUILD=1 $(CPP64FLAGS) $(PIFLAGS)

LOCALCCFLAGS = -g -D___TESTMODE=1 -D___KERNELBUILD=0 -I$(LOCALINCLUDESDIR) $(CC64FLAGS)
LOCALCCFLAGS += $(shell pkg-config --cflags-only-I valgrind)
LOCALCCFLAGS += $(PIFLAGS)

LOCALCPPFLAGS = -g -D___TESTMODE=1 -D___KERNELBUILD=0 -I$(LOCALINCLUDESDIR) $(CPP64FLAGS)
LOCALCPPFLAGS += $(shell pkg-config --cflags-only-I valgrind)
LOCALCPPFLAGS += $(PIFLAGS)

UTILSLDFLAGS = $(shell pkg-config --libs valgrind|cut -f1 -d " ") -lcoregrind-amd64-linux -lvex-amd64-linux \
		  -Wl,--gc-sections -static -Wl,-static -Wl,--allow-multiple-definition

# Also build tests/nostdlib, now it is obsolete.
TESTSLDFLAGS = $(UTILSLDFLAGS) -Wl,-nostdlib -nostdlib -Wl,-e_tos_start -Wl,-pie

EFICC64FLAGS = -D___EFIBUILD=1 -D___KERNELBUILD=0 -Iefi $(CC64FLAGS) $(NOPIFLAGS)
EFICPP64FLAGS = -D___EFIBUILD=1 -D___KERNELBUILD=0 -Iefi $(CPP64FLAGS) $(NOPIFLAGS)

OBJDIR = build
ASOBJDIR = $(OBJDIR)/asm
CCOBJDIR = $(OBJDIR)/cc
LOCALOBJDIR = $(OBJDIR)/cc-local
EFIOBJDIR = $(OBJDIR)/efi
DOCSOBJDIR = $(OBJDIR)/docs
ASSRCDIR = asm
CCSRCDIR = cc
CCGENDIR = cc-gen
INCLUDESGENDIR = includes-gen
CCGENSCRIPTSDIR = scripts/gen-cc
TMPDIR = tmp
ASSETSDIR = assets
ASSETOBJDIR = $(OBJDIR)/assets
EFISRCDIR = efi
UTILSSRCDIR = utils
TESTSSRCDIR = tests

QEMUDISK  = $(OBJDIR)/qemu-hda
TESTQEMUDISK  = $(OBJDIR)/qemu-test-hda
TOSDBIMGNAME = tosdb.img
EFITOSDBIMGNAME = tosdb-efi.img
TOSDBIMG = $(OBJDIR)/$(TOSDBIMGNAME)
EFITOSDBIMG = $(OBJDIR)/$(EFITOSDBIMGNAME)

AS64SRCS = $(shell find $(ASSRCDIR) -type f -name \*64.S)
CC64SRCS = $(shell find $(CCSRCDIR) -type f -name \*.64.c)
CPP64SRCS = $(shell find $(CCSRCDIR) -type f -name \*.64.cpp)
CC64TESTSRCS = $(shell find $(CCSRCDIR) -type f -name \*.64.test.c)

CCXXSRCS = $(shell find $(CCSRCDIR) -type f -name \*.xx.c)
CCXXTESTSRCS = $(shell find $(CCSRCDIR) -type f -name \*.xx.test.c)

LDSRCS = $(shell find $(LDSRCDIR) -type f -name \*.ld)

UTILSCC64SRCS = $(shell find $(UTILSSRCDIR) -type f -name \*.c)
UTILSCPP64SRCS = $(shell find $(UTILSSRCDIR) -type f -name \*.cpp)

TESTSCC64SRCS = $(shell find $(TESTSSRCDIR) -maxdepth 1 -type f -name \*.c)
TESTSCPP64SRCS = $(shell find $(TESTSSRCDIR) -maxdepth 1 -type f -name \*.cpp)

EFICC64SRCS = $(shell find $(EFISRCDIR) -maxdepth 1 -type f -name \*.c)
EFICPP64SRCS = $(shell find $(EFISRCDIR) -maxdepth 1 -type f -name \*.cpp)

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

UTILSCCPROGS = $(patsubst %.c,%,$(UTILSCC64SRCS))
UTILSCPPPROGS = $(patsubst %.cpp,%,$(UTILSCPP64SRCS))

UTILSPROGS = $(UTILSCCPROGS) $(UTILSCPPPROGS) 

UTILSOUTPUTS := $(patsubst $(UTILSSRCDIR)/%,$(OBJDIR)/%.bin,$(UTILSPROGS))

TESTSCCPROGS = $(patsubst %.c,%,$(TESTSCC64SRCS))
TESTSCPPPROGS = $(patsubst %.cpp,%,$(TESTSCPP64SRCS))

TESTSPROGS = $(TESTSCCPROGS) $(TESTSCPPPROGS) 

TESTSOUTPUTS := $(patsubst $(TESTSSRCDIR)/%,$(OBJDIR)/%.bin,$(TESTSPROGS))

EFISRCS = $(shell find $(EFISRCDIR) -type f -name \*.c)
EFIOBJS = $(patsubst $(EFISRCDIR)/%.c,$(EFIOBJDIR)/%.o,$(EFISRCS))

ifeq (,$(wildcard $(TOSDBIMG)))
LASTCCOBJS = $(CC64OBJS) $(CC64GENOBJS) $(ASSETOBJS) $(CPP64OBJS)
else
LASTCCOBJS = $(shell find $(CCOBJDIR) -type f -name \*.o -newer $(TOSDBIMG))
LASTCCOBJS += $(shell find $(ASSETOBJDIR) -type f -name \*.o -newer $(TOSDBIMG))
endif

ifeq (,$(wildcard $(EFITOSDBIMG)))
EFILASTCCOBJS = $(EFIOBJS)
else
EFILASTCCOBJS = $(shell find $(EFIOBJDIR) -type f -name \*.o -newer $(EFITOSDBIMG))
endif

MKDIRSDONE = .mkdirsdone

EFIDISKTOOL = $(OBJDIR)/efi_disk.bin
EFIBOOTFILE = $(OBJDIR)/BOOTX64.EFI
TOSDBIMG_BUILDER = $(OBJDIR)/generatelinkerdb.bin
PXECONFGEN = $(OBJDIR)/pxeconfgen.bin

EFITOSDBIMG_BUILDER_FLAGS = -e efi_main 
EFILD = $(OBJDIR)/linker-tosdb.bin
EFILDFLAGS = -e efi_main -r  -psp 4096 -psv 4096 --for-efi

.PHONY: all clean $(SUBDIRS) asm bear
.PRECIOUS:

qemu-without-analyzer:
	CCXXEXTRAFLAGS= make -j $(shell nproc) -f Makefile qemu-internal

qemu: 
	CCXXEXTRAFLAGS=-fanalyzer make -j $(shell nproc) -f Makefile qemu-internal

qemu-internal: $(QEMUDISK)

qemu-pxe: $(TOSDBIMG) $(PXECONFGEN) $(EFIBOOTFILE)
	$(PXECONFGEN) -dbn $(TOSDBIMGNAME) -dbp $(TOSDBIMG) -o $(OBJDIR)/pxeconf.bson

qemu-test: $(TESTQEMUDISK)

asm:
	CCXXEXTRAFLAGS=-fanalyzer make -j $(shell nproc) -f Makefile asm-internal

asm-internal: $(CC64ASMOUTS)

bear:
	bear --output $(OBJDIR)/compile_commands.json --append -- make qemu
	bear --output $(OBJDIR)/compile_commands.json --append -- make -j $(shell nproc) tests

test: qemu-test
	scripts/osx-hacks/qemu-hda-test.sh

tests: $(TESTSOUTPUTS)

utils: $(UTILSOUTPUTS)

gendirs:
	mkdir -p $(CCGENDIR) $(INCLUDESGENDIR) $(ASOBJDIR) $(CCOBJDIR) $(DOCSOBJDIR) $(TMPDIR)
	find $(CCSRCDIR) -type d -exec mkdir -p $(OBJDIR)/{} \;
	find $(ASSETSDIR) -type d -exec mkdir -p $(OBJDIR)/{} \;
	find $(CCOBJDIR) -type d |sed 's%'$(OBJDIR)'/cc%'$(OBJDIR)'/efi%' |xargs mkdir -p
	find $(CCOBJDIR) -type d |sed 's%'$(OBJDIR)'/cc%'$(OBJDIR)'/cc-local%' |xargs mkdir -p

$(OBJDIR)/docs: $(DOCSCONF) $(DOCSFILES)
	$(DOCSGEN) $(DOCSCONF)
	find $(OBJDIR)/docs/html/ -name "*.html"|sed 's-'$(OBJDIR)'/docs/html-https://turnstoneos.com-' > $(OBJDIR)/docs/html/sitemap.txt
	touch $(OBJDIR)/docs

$(QEMUDISK): $(MKDIRSDONE) $(EFIBOOTFILE) $(EFIDISKTOOL) $(TOSDBIMG)
	$(EFIDISKTOOL) $(QEMUDISK) $(EFIBOOTFILE) $(TOSDBIMG)

$(TESTQEMUDISK): $(TESTDISK) $(TOSDBIMG)
	$(EFIDISKTOOL) $(QEMUDISK) $(EFIBOOTFILE) $(TOSDBIMG)

$(MKDIRSDONE):
	mkdir -p $(CCGENDIR) $(ASOBJDIR) $(CCOBJDIR)
	touch $(MKDIRSDONE)

$(TOSDBIMG): $(TOSDBIMG_BUILDER) $(CC64OBJS) $(CC64GENOBJS) $(ASSETOBJS) $(CPP64OBJS)
	$(TOSDBIMG_BUILDER) -compact -o $@ $(LASTCCOBJS)

$(EFITOSDBIMG): $(EFIOBJS) $(TOSDBIMG_BUILDER)
	$(TOSDBIMG_BUILDER) $(EFITOSDBIMG_BUILDER_FLAGS) -o $@ $(EFILASTCCOBJS)

$(EFIBOOTFILE): $(EFITOSDBIMG) $(EFILD)
	$(EFILD) $(EFILDFLAGS) -o $@ -db $(EFITOSDBIMG)

$(CCOBJDIR)/%.64.o: $(CCSRCDIR)/%.64.c
	$(CC64) $(KERNELCC64FLAGS) -o $@ $<

$(CCOBJDIR)/%.64.o: $(CCSRCDIR)/%.64.cpp
	$(CPP64) $(KERNELCPP64FLAGS) -o $@ $<

$(CCOBJDIR)/%.xx_64.o: $(CCSRCDIR)/%.xx.c
	$(CC64) $(KERNELCC64FLAGS) -o $@ $<

$(CCOBJDIR)/%.xx_64.test.o: $(CCSRCDIR)/%.xx.test.c
	$(CC64) $(KERNELCC64FLAGS) $(CXXTESTFLAGS) -o $@ $<
	
$(CCOBJDIR)/%.64.s: $(CCSRCDIR)/%.64.c
	$(CC64) $(KERNELCC64FLAGS) -S -o $@ $<

$(CCOBJDIR)/%.xx_64.s: $(CCSRCDIR)/%.xx.c
	$(CC64) $(KERNELCC64FLAGS) -S -o $@ $<
	
$(CCOBJDIR)/%.64.test.s: $(CCSRCDIR)/%.64.test.c
	$(CC64) $(KERNELCC64FLAGS) -S $(CXXTESTFLAGS) -o $@ $<

$(ASOBJDIR)/%64.o: $(ASSRCDIR)/%64.S
	$(CC64) $(KERNELCC64FLAGS) -o $@ $^

$(ASOBJDIR)/%64.test.o: $(ASSRCDIR)/%64.S
	$(CC64) $(KERNELCC64FLAGS) $(CXXTESTFLAGS) -o $@ $^

$(CCGENDIR)/%.c: $(CCGENSCRIPTSDIR)/%.sh
	if [[ ! -f $@ ]]; then $^ > $@; else $^ > $@.tmp; diff $@  $@.tmp  2>/dev/null && rm -f $@.tmp || mv $@.tmp $@; fi

$(CCOBJDIR)/%.cc-gen.x86_64.o: $(CCGENDIR)/%.c
	$(CC64) $(KERNELCC64FLAGS) -o $@ $<

$(CCOBJDIR)/interrupt_handlers.cc-gen.x86_64.o: $(CCGENDIR)/interrupt_handlers.c
	$(CC64) $(KERNELCC64FLAGS) -o $@ $<

$(CCOBJDIR)/vm_guest_interrupt_handlers.cc-gen.x86_64.o: $(CCGENDIR)/vm_guest_interrupt_handlers.c
	$(CC64) $(KERNELCC64FLAGS) -o $@ $<

$(LOCALOBJDIR)/%.o: $(UTILSSRCDIR)/%.c $(LOCALINCLUDESDIR)/setup.h
	$(CC64) $(LOCALCCFLAGS) -o $@ $<

$(LOCALOBJDIR)/%.o: $(UTILSSRCDIR)/%.cpp $(LOCALINCLUDESDIR)/setup.h
	$(CPP64) $(LOCALCPPFLAGS) -o $@ $<

$(LOCALOBJDIR)/%.o: $(TESTSSRCDIR)/%.c $(LOCALINCLUDESDIR)/setup.h
	$(CC64) $(LOCALCCFLAGS) -o $@ $<

$(LOCALOBJDIR)/%.o: $(TESTSSRCDIR)/%.cpp $(LOCALINCLUDESDIR)/setup.h
	$(CPP64) $(LOCALCPPFLAGS) -o $@ $<

$(LOCALOBJDIR)/%.xx_64.o: $(CCSRCDIR)/%.xx.c
	$(CC64) $(LOCALCCFLAGS) -o $@ $<

$(LOCALOBJDIR)/%.64.o: $(CCSRCDIR)/%.64.c
	$(CC64) $(LOCALCCFLAGS) -o $@ $<

$(LOCALOBJDIR)/%.64.o: $(CCSRCDIR)/%.64.cpp
	$(CPP64) $(LOCALCPPFLAGS) -o $@ $<

$(OBJDIR)/%.bin: $(LOCALOBJDIR)/%.o
	$(LOCALLD) $(UTILSLDFLAGS) -o $@ $^

$(EFIOBJDIR)/%.o: $(EFISRCDIR)/%.c
	$(CC64) $(EFICC64FLAGS) -o $@ $<

$(EFIOBJDIR)/%.o: $(EFISRCDIR)/%.cpp
	$(CPP64) $(EFICPP64FLAGS) -o $@ $<

$(EFIOBJDIR)/%.xx_64.o: $(CCSRCDIR)/%.xx.c
	$(CC64) $(EFICC64FLAGS) -o $@ $<

$(EFIOBJDIR)/%.64.o: $(CCSRCDIR)/%.64.c
	$(CC64) $(EFICC64FLAGS) -o $@ $<

$(EFIOBJDIR)/%.64.o: $(CCSRCDIR)/%.64.cpp
	$(CPP64) $(EFICPP64FLAGS) -o $@ $<

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

.PHONY: depend
depend: .depend64c .depend64cpp .dependutils .dependtests .dependefi
	@echo "Dependencies generated" 

.depend64c: $(CC64SRCS) $(CCXXSRCS) $(CC64TESTSRCS)
	scripts/create-cc-deps.sh "$(CC64) $(KERNELCC64FLAGS) -D___DEPEND_ANALYSIS -MM" "$^" > .depend64
	$(SEDNOBAK) 's/xx.o:/xx_64.o:/g' .depend64

.depend64cpp: $(CPP64SRCS)
	scripts/create-cc-deps.sh "$(CPP64) $(KERNELCPP64FLAGS) -D___DEPEND_ANALYSIS -MM" "$^" >> .depend64
	$(SEDNOBAK) 's/xx.o:/xx_64.o:/g' .depend64

.dependutils:
	scripts/create_depends.sh $(UTILSPROGS) >.dependutils

.dependtests:
	scripts/create_depends.sh $(TESTSPROGS) >.dependtests

.dependefi: $(EFISRCS)
	scripts/create_efi_depends.sh $(EFITOSDBIMG) $(EFISRCS)  >.dependefi

-include .depend64
-include .dependutils
-include .dependtests
-include .dependefi
