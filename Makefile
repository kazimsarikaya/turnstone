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
INCLUDESGENDIR = includes-gen
LOCALINCLUDESDIR = includes-local

BASEFLAGS += -O3 -nostdlib -nostdinc -ffreestanding -fno-builtin -c -I$(INCLUDESDIR) -I$(INCLUDESGENDIR) \
	-Werror -Wall -Wextra -ffunction-sections -fdata-sections -fno-common \
	-mno-red-zone -fstack-protector-all -fno-omit-frame-pointer \
    -Wshadow -Wpointer-arith -Wcast-align \
	-Wwrite-strings -Wmissing-declarations \
    -Wredundant-decls -Winline -Wno-long-long \
	-D___BITS=64 -m64 -march=native \
	${CCXXEXTRAFLAGS}	

PIFLAGS = -fPIC -fpic -fplt -mcmodel=large 
NOPIFLAGS = -fno-pic -fno-PIC -fno-plt -mcmodel=large

CC64FLAGS = $(BASEFLAGS) \
		  -std=gnu23 \
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

DEPEND_FLAGS = -D___DEPEND_ANALYSIS -MM

OBJDIR = build
ASSRCDIR = asm
CCSRCDIR = cc
CCLOCALSRCDIR = cc-local
EFISRCDIR = efi
ASOBJDIR = $(OBJDIR)/$(ASSRCDIR)
CCOBJDIR = $(OBJDIR)/$(CCSRCDIR)
LOCALOBJDIR = $(OBJDIR)/$(CCLOCALSRCDIR)
EFIOBJDIR = $(OBJDIR)/$(EFISRCDIR)
DOCSOBJDIR = $(OBJDIR)/docs
CCGENDIR = cc-gen
CCGENSCRIPTSDIR = scripts/gen-cc
ASSETGENSCRIPTSDIR = scripts/gen-assets
TMPDIR = tmp
ASSETSDIR = assets
ASSETSGENDIR = assets-gen
ASSETSCCGENDIR = assets-cc-gen
ASSETOBJDIR = $(OBJDIR)/$(ASSETSDIR)
ASSETGENOBJDIR = $(OBJDIR)/$(ASSETSGENDIR)
ASSETCCGENOBJDIR = $(OBJDIR)/$(ASSETSCCGENDIR)
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

EFISRCS = $(EFICC64SRCS) $(EFICPP64SRCS)

CCGENSCRIPTS = $(shell find $(CCGENSCRIPTSDIR) -type f -name \*.sh)
CCGENSRCS = $(patsubst $(CCGENSCRIPTSDIR)/%.sh,$(CCGENDIR)/%.c,$(CCGENSCRIPTS))
CC64GENOBJS = $(patsubst $(CCGENDIR)/%.c,$(CCOBJDIR)/%.cc-gen.x86_64.o,$(CCGENSRCS))
CC64GENDEPS = $(patsubst $(CCGENDIR)/%.c,$(CCOBJDIR)/%.cc-gen.x86_64.o.dep,$(CCGENSRCS))

ASOBJS = $(patsubst $(ASSRCDIR)/%.s,$(ASOBJDIR)/%.o,$(ASSRCS))

ASTESTOBJS = $(patsubst $(ASSRCDIR)/%.s,$(ASOBJDIR)/%.test.o,$(ASSRCS))

CC64OBJS = $(patsubst $(CCSRCDIR)/%.64.c,$(CCOBJDIR)/%.64.o,$(CC64SRCS))
CC64DEPS = $(patsubst $(CCSRCDIR)/%.64.c,$(CCOBJDIR)/%.64.o.dep,$(CC64SRCS))
CC64OBJS += $(patsubst $(CCSRCDIR)/%.xx.c,$(CCOBJDIR)/%.xx_64.o,$(CCXXSRCS))
CC64DEPS += $(patsubst $(CCSRCDIR)/%.xx.c,$(CCOBJDIR)/%.xx_64.o.dep,$(CCXXSRCS))

CC64ASMOUTS = $(patsubst $(CCSRCDIR)/%.64.c,$(CCOBJDIR)/%.64.s,$(CC64SRCS))
CC64ASMOUTS += $(patsubst $(CCSRCDIR)/%.xx.c,$(CCOBJDIR)/%.xx_64.s,$(CCXXSRCS))
CC64GENASMOUTS = $(patsubst $(CCGENSCRIPTSDIR)/%.sh,$(CCOBJDIR)/%.cc-gen.x86_64.s,$(CCGENSCRIPTS))

CC64TESTOBJS = $(patsubst $(CCSRCDIR)/%.64.test.c,$(CCOBJDIR)/%.64.test.o,$(CC64TESTSRCS))
CC64TESTOBJS += $(patsubst $(CCSRCDIR)/%.xx.test.c,$(CCOBJDIR)/%.xx_64.test.o,$(CCXXTESTSRCS))

CPP64OBJS = $(patsubst $(CCSRCDIR)/%.64.cpp,$(CCOBJDIR)/%.64.o,$(CPP64SRCS))
CPP64DEPS = $(patsubst $(CCSRCDIR)/%.64.cpp,$(CCOBJDIR)/%.64.o.dep,$(CPP64SRCS))

TESTSOBJS = $(patsubst $(TESTSSRCDIR)/%.c,$(LOCALOBJDIR)/%.o,$(TESTSCC64SRCS))
TESTSDEPS = $(patsubst $(TESTSSRCDIR)/%.c,$(LOCALOBJDIR)/%.o.dep,$(TESTSCC64SRCS))
TESTSOBJS += $(patsubst $(TESTSSRCDIR)/%.cpp,$(LOCALOBJDIR)/%.o,$(TESTSCPP64SRCS))
TESTSDEPS += $(patsubst $(TESTSSRCDIR)/%.cpp,$(LOCALOBJDIR)/%.o.dep,$(TESTSCPP64SRCS))

UTILSOBJS = $(patsubst $(UTILSSRCDIR)/%.c,$(LOCALOBJDIR)/%.o,$(UTILSCC64SRCS))
UTILSDEPS = $(patsubst $(UTILSSRCDIR)/%.c,$(LOCALOBJDIR)/%.o.dep,$(UTILSCC64SRCS))
UTILSOBJS += $(patsubst $(UTILSSRCDIR)/%.cpp,$(LOCALOBJDIR)/%.o,$(UTILSCPP64SRCS))
UTILSDEPS += $(patsubst $(UTILSSRCDIR)/%.cpp,$(LOCALOBJDIR)/%.o.dep,$(UTILSCPP64SRCS))

DOCSFILES += $(CC64SRCS) $(CCXXSRCS) $(CPP64SRCS)
DOCSFILES += $(shell find $(INCLUDESDIR) -type f -name \*.h)
DOCSFILES += $(shell find $(EFISRCDIR) -type f -name \*.c)
DOCSFILES += $(shell find $(EFISRCDIR) -type f -name \*.h)
DOCSFILES += $(shell find $(UTILSSRCDIR) -type f -name \*.c)
DOCSFILES += $(shell find $(UTILSSRCDIR) -type f -name \*.h)

ASSETS = $(shell find $(ASSETSDIR) -type f)
ASSETOBJS = $(patsubst %,$(OBJDIR)/%.data.o,$(ASSETS))

ASSETSGEN = $(shell find $(ASSETSGENDIR) -type f)
ASSETGENOBJS = $(patsubst %,$(OBJDIR)/%.data.o,$(ASSETSGEN))

ASSETSALL = $(ASSETS) $(ASSETSGEN)
ASSETALLOBJS = $(ASSETOBJS) $(ASSETGENOBJS)
ASSETALLDEPS = $(patsubst %,$(OBJDIR)/%.data.o.dep,$(ASSETSALL))

ASSETSCCGEN = $(shell find $(ASSETGENSCRIPTSDIR) -type f -name \*.sh -print0 | xargs -0 -I SCRIPTFILE SCRIPTFILE output)
ASSETCCGENOBJS = $(patsubst $(ASSETSCCGENDIR)/%.64.c,$(ASSETCCGENOBJDIR)/%.64.o,$(ASSETSCCGEN))
ASSETCCGENDEPS = $(patsubst $(ASSETSCCGENDIR)/%.64.c,$(ASSETCCGENOBJDIR)/%.64.o.dep,$(ASSETSCCGEN))

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

BINOUTPUTS = $(UTILSOUTPUTS) $(TESTSOUTPUTS)

BINOUTPUTDEPS = $(patsubst $(OBJDIR)/%.bin,$(LOCALOBJDIR)/%.bin.dep,$(BINOUTPUTS))

EFISRCS = $(shell find $(EFISRCDIR) -type f -name \*.c)
EFIOBJS = $(patsubst $(EFISRCDIR)/%.c,$(EFIOBJDIR)/%.o,$(EFISRCS))
EFIDEPS = $(patsubst $(EFISRCDIR)/%.c,$(EFIOBJDIR)/%.o.dep,$(EFISRCS))
EFIDEPDEPS = $(patsubst $(EFISRCDIR)/%.c,$(EFIOBJDIR)/%.o.dep.dep,$(EFISRCS))

ifeq (,$(wildcard $(TOSDBIMG)))
LASTCCOBJS = $(CC64OBJS) $(CC64GENOBJS) $(ASSETOBJS) $(ASSETGENOBJS) $(ASSETCCGENOBJS) $(CPP64OBJS)
else
LASTCCOBJS = $(shell find $(CCOBJDIR) -type f -name \*.o -newer $(TOSDBIMG))
LASTCCOBJS += $(shell find $(ASSETOBJDIR) -type f -name \*.o -newer $(TOSDBIMG))
LASTCCOBJS += $(shell find $(ASSETGENOBJDIR) -type f -name \*.o -newer $(TOSDBIMG))
LASTCCOBJS += $(shell find $(ASSETCCGENOBJDIR) -type f -name \*.o -newer $(TOSDBIMG))
endif

ifeq (,$(wildcard $(EFITOSDBIMG)))
EFILASTCCOBJS = $(shell find $(EFIOBJDIR) -type f -name \*.o)
else
EFILASTCCOBJS = $(shell find $(EFIOBJDIR) -type f -name \*.o -newer $(EFITOSDBIMG))
endif

MKDIRSDONE = .mkdirsdone

EFIDISKTOOL = $(OBJDIR)/efi_disk.bin
EFIBOOTFILE = $(OBJDIR)/BOOTX64.EFI
TOSDBIMG_BUILDER = $(OBJDIR)/generatelinkerdb.bin
TOSDBIMG_BUILDER_FLAGS = #-compact
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
	rm -f $(TOSDBIMG) ${EFITOSDBIMG}
	# Find .o files newer than the json file and delete them
	@if [ -f $(OBJDIR)/compile_commands.json ]; then \
		find $(OBJDIR) -type f -name '*.o' -newer $(OBJDIR)/compile_commands.json -delete; \
	fi
	bear --output $(OBJDIR)/compile_commands.json --append -- make qemu
	bear --output $(OBJDIR)/compile_commands.json --append -- make -j $(shell nproc) tests

test: qemu-test
	scripts/osx-hacks/qemu-hda-test.sh

tests: $(TESTSOUTPUTS)

utils: $(UTILSOUTPUTS)

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

$(TOSDBIMG): $(TOSDBIMG_BUILDER) $(ASSETALLOBJS) $(ASSETCCGENOBJS) $(CC64OBJS) $(CC64GENOBJS) $(CPP64OBJS)
	$(TOSDBIMG_BUILDER) $(TOSDBIMG_BUILDER_FLAGS) -o $@ $(LASTCCOBJS)

$(EFITOSDBIMG): $(EFIOBJS) $(TOSDBIMG_BUILDER)
	$(TOSDBIMG_BUILDER) $(EFITOSDBIMG_BUILDER_FLAGS) -o $@ $(EFILASTCCOBJS)

$(EFIBOOTFILE): $(EFITOSDBIMG) $(EFILD)
	$(EFILD) $(EFILDFLAGS) -o $@ -db $(EFITOSDBIMG)

$(CCOBJDIR)/%.64.o.dep: $(CCSRCDIR)/%.64.c
	$(CC64) $(KERNELCC64FLAGS) $(DEPEND_FLAGS) -MF $@ -MT $(CCOBJDIR)/$*.64.o $<

$(CCOBJDIR)/%.64.o: $(CCSRCDIR)/%.64.c
	$(CC64) $(KERNELCC64FLAGS) -o $@ $<

$(CCOBJDIR)/%.64.o.dep: $(CCSRCDIR)/%.64.cpp
	$(CPP64) $(KERNELCPP64FLAGS) $(DEPEND_FLAGS) -MF $@ -MT $(CCOBJDIR)/$*.64.o $<

$(CCOBJDIR)/%.64.o: $(CCSRCDIR)/%.64.cpp
	$(CPP64) $(KERNELCPP64FLAGS) -o $@ $<

$(CCOBJDIR)/%.xx_64.o.dep: $(CCSRCDIR)/%.xx.c
	$(CC64) $(KERNELCC64FLAGS) $(DEPEND_FLAGS) -MF $@ -MT $(CCOBJDIR)/$*.xx_64.o $<

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
	$^ > $@

$(CCOBJDIR)/%.cc-gen.x86_64.o.dep: $(CCGENDIR)/%.c
	$(CC64) $(KERNELCC64FLAGS) $(DEPEND_FLAGS) -MF $@ -MT $(CCOBJDIR)/$*.cc-gen.x86_64.o $<

$(CCOBJDIR)/%.cc-gen.x86_64.o: $(CCGENDIR)/%.c
	$(CC64) $(KERNELCC64FLAGS) -o $@ $<

$(LOCALOBJDIR)/%.o.dep: $(UTILSSRCDIR)/%.c
	$(CC64) $(LOCALCCFLAGS) $(DEPEND_FLAGS) -MF $@ -MT $(LOCALOBJDIR)/$*.o $<

$(LOCALOBJDIR)/%.o: $(UTILSSRCDIR)/%.c
	$(CC64) $(LOCALCCFLAGS) -o $@ $<

$(LOCALOBJDIR)/%.o.dep: $(UTILSSRCDIR)/%.cpp
	$(CPP64) $(LOCALCPPFLAGS) $(DEPEND_FLAGS) -MF $@ -MT $(LOCALOBJDIR)/$*.o $<

$(LOCALOBJDIR)/%.o: $(UTILSSRCDIR)/%.cpp
	$(CPP64) $(LOCALCPPFLAGS) -o $@ $<

$(LOCALOBJDIR)/%.o.dep: $(TESTSSRCDIR)/%.c
	$(CC64) $(LOCALCCFLAGS) $(DEPEND_FLAGS) -MF $@ -MT $(LOCALOBJDIR)/$*.o $<

$(LOCALOBJDIR)/%.o: $(TESTSSRCDIR)/%.c
	$(CC64) $(LOCALCCFLAGS) -o $@ $<

$(LOCALOBJDIR)/%.o.dep: $(TESTSSRCDIR)/%.cpp
	$(CPP64) $(LOCALCPPFLAGS) $(DEPEND_FLAGS) -MF $@ -MT $(LOCALOBJDIR)/$*.o $<

$(LOCALOBJDIR)/%.o: $(TESTSSRCDIR)/%.cpp
	$(CPP64) $(LOCALCPPFLAGS) -o $@ $<

$(LOCALOBJDIR)/%.xx_64.o.dep: $(CCSRCDIR)/%.xx.c
	$(CC64) $(LOCALCCFLAGS) $(DEPEND_FLAGS) -MF $@ -MT $(LOCALOBJDIR)/$*.xx_64.o $<

$(LOCALOBJDIR)/%.xx_64.o: $(CCSRCDIR)/%.xx.c
	$(CC64) $(LOCALCCFLAGS) -o $@ $<

$(LOCALOBJDIR)/%.64.o.dep: $(CCSRCDIR)/%.64.c
	$(CC64) $(LOCALCCFLAGS) $(DEPEND_FLAGS) -MF $@ -MT $(LOCALOBJDIR)/$*.64.o $<

$(LOCALOBJDIR)/%.64.o: $(CCSRCDIR)/%.64.c
	$(CC64) $(LOCALCCFLAGS) -o $@ $<

$(LOCALOBJDIR)/%.64.o.dep: $(CCSRCDIR)/%.64.cpp
	$(CPP64) $(LOCALCPPFLAGS) $(DEPEND_FLAGS) -MF $@ -MT $(LOCALOBJDIR)/$*.64.o $<

$(LOCALOBJDIR)/%.64.o: $(CCSRCDIR)/%.64.cpp
	$(CPP64) $(LOCALCPPFLAGS) -o $@ $<

$(LOCALOBJDIR)/%.bin.dep: $(LOCALOBJDIR)/%.o.dep scripts/create_depends.sh
	scripts/create_depends.sh cc-local $< > $@

$(OBJDIR)/%.bin: $(LOCALOBJDIR)/%.o
	$(LOCALLD) $(UTILSLDFLAGS) -o $@ $^

$(EFIOBJDIR)/%.o.dep.dep: $(EFIOBJDIR)/%.o.dep scripts/create_depends.sh
	scripts/create_depends.sh efi $< $(EFITOSDBIMGNAME) > $@

$(EFIOBJDIR)/%.o.dep: $(EFISRCDIR)/%.c
	$(CC64) $(EFICC64FLAGS) $(DEPEND_FLAGS) -MF $@ -MT $(EFIOBJDIR)/$*.o $<

$(EFIOBJDIR)/%.o: $(EFISRCDIR)/%.c
	$(CC64) $(EFICC64FLAGS) -o $@ $<

$(EFIOBJDIR)/%.o.dep: $(EFISRCDIR)/%.cpp
	$(CPP64) $(EFICPP64FLAGS) $(DEPEND_FLAGS) -MF $@ -MT $(EFIOBJDIR)/$*.o $<

$(EFIOBJDIR)/%.o: $(EFISRCDIR)/%.cpp
	$(CPP64) $(EFICPP64FLAGS) -o $@ $<

$(EFIOBJDIR)/%.xx_64.o.dep: $(CCSRCDIR)/%.xx.c
	$(CC64) $(EFICC64FLAGS) $(DEPEND_FLAGS) -MF $@ -MT $(EFIOBJDIR)/$*.xx_64.o $<

$(EFIOBJDIR)/%.xx_64.o: $(CCSRCDIR)/%.xx.c
	$(CC64) $(EFICC64FLAGS) -o $@ $<

$(EFIOBJDIR)/%.64.o.dep: $(CCSRCDIR)/%.64.c
	$(CC64) $(EFICC64FLAGS) $(DEPEND_FLAGS) -MF $@ -MT $(EFIOBJDIR)/$*.64.o $<

$(EFIOBJDIR)/%.64.o: $(CCSRCDIR)/%.64.c
	$(CC64) $(EFICC64FLAGS) -o $@ $<

$(EFIOBJDIR)/%.64.o.dep: $(CCSRCDIR)/%.64.cpp
	$(CPP64) $(EFICPP64FLAGS) $(DEPEND_FLAGS) -MF $@ -MT $(EFIOBJDIR)/$*.64.o $<

$(EFIOBJDIR)/%.64.o: $(CCSRCDIR)/%.64.cpp
	$(CPP64) $(EFICPP64FLAGS) -o $@ $<

$(ASSETOBJDIR)/%.data.o.dep: $(ASSETSDIR)/%
	mkdir -p $(dir $@)
	echo $(ASSETOBJDIR)/$*.data.o: $< > $@

$(ASSETGENOBJDIR)/%.data.o.dep: $(ASSETSGENDIR)/%
	mkdir -p $(dir $@)
	echo $(ASSETOBJDIR)/$*.data.o: $< > $@

$(ASSETOBJDIR)/%.data.o: $(ASSETSDIR)/% scripts/assets/build.sh
	scripts/assets/build.sh $< $@

$(ASSETGENOBJDIR)/%.data.o: $(ASSETSGENDIR)/% scripts/assets/build.sh
	scripts/assets/build.sh $< $@

$(ASSETSCCGENDIR)/%.64.c: $(ASSETGENSCRIPTSDIR)/%.sh
	$^ build

$(ASSETCCGENOBJDIR)/%.64.o.dep: $(ASSETSCCGENDIR)/%.64.c
	$(CC64) $(KERNELCC64FLAGS) $(DEPEND_FLAGS) -MF $@ -MT $(ASSETCCGENOBJDIR)/$*.64.o $<

$(ASSETCCGENOBJDIR)/%.64.o: $(ASSETSCCGENDIR)/%.64.c
	$(CC64) $(KERNELCC64FLAGS) -o $@ $<

print-%: ; @echo $* = $($*)

clean:
	rm -fr $(DOCSOBJDIR)/*
	if [ -d $(OBJDIR) ]; then find $(OBJDIR) -type f -delete; fi
	if [ -d $(CCGENDIR) ]; then find $(CCGENDIR) -type f -delete; fi
	if [ -d $(INCLUDESGENDIR) ]; then find $(INCLUDESGENDIR) -type f -delete; fi
	if [ -d $(ASSETSGENDIR) ]; then find $(ASSETSGENDIR) -type f -delete; fi
	if [ -d $(ASSETSCCGENDIR) ]; then find $(ASSETSCCGENDIR) -type f -delete; fi
	rm -f $(MKDIRSDONE)

cleandirs:
	rm -fr $(CCGENDIR) $(INCLUDESGENDIR) $(OBJDIR)

gendirs:
	mkdir -p $(CCGENDIR) $(INCLUDESGENDIR) $(ASSETSGENDIR) $(ASSETSCCGENDIR) $(ASOBJDIR) $(CCOBJDIR) $(DOCSOBJDIR) $(TMPDIR)
	find $(CCSRCDIR) -type d -exec mkdir -p $(OBJDIR)/{} \;
	find $(ASSETSDIR) -type d -exec mkdir -p $(OBJDIR)/{} \;
	find $(ASSETSGENDIR) -type d -exec mkdir -p $(OBJDIR)/{} \;
	find $(ASSETSCCGENDIR) -type d -exec mkdir -p $(OBJDIR)/{} \;
	find $(CCOBJDIR) -type d |sed 's%'$(OBJDIR)'/cc%'$(OBJDIR)'/efi%' |xargs mkdir -p
	find $(CCOBJDIR) -type d |sed 's%'$(OBJDIR)'/cc%'$(OBJDIR)'/cc-local%' |xargs mkdir -p

-include $(ASSETCCGENDEPS)
-include $(ASSETALLDEPS)
-include $(CC64DEPS)
-include $(CPP64DEPS)
-include $(CC64GENDEPS)
-include $(UTILSDEPS)
-include $(TESTSDEPS)
-include $(EFIDEPS)
-include $(EFIDEPDEPS)
-include ${BINOUTPUTDEPS}
