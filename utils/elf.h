#ifndef ___ELF_H
#define ___ELF_H 0

#define EI_NIDENT (16)

typedef struct {
	unsigned char e_ident[EI_NIDENT];
	uint16_t e_type;
	uint16_t e_machine;
	uint32_t e_version;
	uint64_t e_entry;
	uint64_t e_phoff;
	uint64_t e_shoff;
	uint32_t e_flags;
	uint16_t e_ehsize;
	uint16_t e_phentsize;
	uint16_t e_phnum;
	uint16_t e_shentsize;
	uint16_t e_shnum;
	uint16_t e_shstrndx;
} Elf64_Ehdr;



typedef struct {
	uint32_t sh_name;
	uint32_t sh_type;
	uint64_t sh_flags;
	uint64_t sh_addr;
	uint64_t sh_offset;
	uint64_t sh_size;
	uint32_t sh_link;
	uint32_t sh_info;
	uint64_t sh_addralign;
	uint64_t sh_entsize;
} Elf64_Shdr;

#define R_X86_64_NONE   0
#define R_X86_64_64   1
#define R_X86_64_PC32   2
#define R_X86_64_GOT32    3
#define R_X86_64_PLT32    4
#define R_X86_64_COPY   5
#define R_X86_64_GLOB_DAT 6
#define R_X86_64_JUMP_SLOT  7
#define R_X86_64_RELATIVE 8
#define R_X86_64_GOTPCREL 9

#define R_X86_64_32   10
#define R_X86_64_32S    11
#define R_X86_64_16   12
#define R_X86_64_PC16   13
#define R_X86_64_8    14
#define R_X86_64_PC8    15
#define R_X86_64_DTPMOD64 16
#define R_X86_64_DTPOFF64 17
#define R_X86_64_TPOFF64  18
#define R_X86_64_TLSGD    19

#define R_X86_64_TLSLD    20

#define R_X86_64_DTPOFF32 21
#define R_X86_64_GOTTPOFF 22

#define R_X86_64_TPOFF32  23
#define R_X86_64_PC64   24
#define R_X86_64_GOTOFF64 25
#define R_X86_64_GOTPC32  26
#define R_X86_64_GOT64    27
#define R_X86_64_GOTPCREL64 28
#define R_X86_64_GOTPC64  29
#define R_X86_64_GOTPLT64 30
#define R_X86_64_PLTOFF64 31
#define R_X86_64_SIZE32   32
#define R_X86_64_SIZE64   33

#define R_X86_64_GOTPC32_TLSDESC 34
#define R_X86_64_TLSDESC_CALL   35

#define R_X86_64_TLSDESC        36
#define R_X86_64_IRELATIVE  37
#define R_X86_64_RELATIVE64 38
#define R_X86_64_GOTPCRELX  41
#define R_X86_64_REX_GOTPCRELX  42
#define R_X86_64_NUM    43


#endif
