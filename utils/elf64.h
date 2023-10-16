/**
 * @file elf64.h
 * @brief ELF64 format header.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___ELF64_H
#define ___ELF64_H 0

#include <types.h>

#define ELFCLASS32 1
#define ELFCLASS64 2
#define ELFDATA2LSB 1
#define ELFDATA2MSB 2

#define ET_NONE      0
#define ET_REL       1
#define ET_EXEC      2
#define ET_DYN       3
#define ET_CORE      4
#define ET_LOOS      0xFE00
#define ET_HIOS      0xFEFF
#define ET_LOPROC    0xFF00
#define ET_HIPROC    0xFFFF

#define EM_NONE      0
#define EM_M32       1
#define EM_X86_64    62

#define EV_NONE      0
#define EV_CURRENT   1

#define SHN_UNDEF    0
#define SHN_LOPROC   0xFF00
#define SHN_HIPROC   0xFF1F
#define SHN_LOOS     0xFF20
#define SHN_HIOS     0xFF3F
#define SHN_ABS      0xFFF1
#define SHN_COMMON   0xFFF2

#define SHT_NULL         0
#define SHT_PROGBITS     1
#define SHT_SYMTAB       2
#define SHT_STRTAB       3
#define SHT_RELA         4
#define SHT_HASH         5
#define SHT_DYNAMIC      6
#define SHT_NOTE         7
#define SHT_NOBITS       8
#define SHT_REL          9
#define SHT_SHLIB        10
#define SHT_DYNSYM       11
#define SHT_LOOS         0x60000000
#define SHT_HIOS         0x6FFFFFFF
#define SHT_LOPROC       0x70000000
#define SHT_HIPROC       0x7FFFFFFF

#define SHF_NONE         0
#define SHF_WRITE        1
#define SHF_ALLOC        2
#define SHF_EXECINSTR    4
#define SHF_MASKOS       0x0F000000
#define SHF_MASKPROC     0xF0000000

#define STB_LOCAL   0
#define STB_GLOBAL  1
#define STB_WEAK    2
#define STB_LOOS    10
#define STB_HIOS    12
#define STB_LOPROC  13
#define STB_HIPROC  15

#define STT_NOTYPE   0
#define STT_OBJECT   1
#define STT_FUNC     2
#define STT_SECTION  3
#define STT_FILE     4
#define STT_LOOS     10
#define STT_HIOS     12
#define STT_LOPROC   13
#define STT_HIPROC   15

#define PT_NULL    0
#define PT_LOAD    1
#define PT_DYNAMIC 2
#define PT_INTERP  3
#define PT_NOTE    4
#define PT_SHLIB   5
#define PT_PHDR    6
#define PT_LOOS    0x60000000
#define PT_HIOS    0x6FFFFFFF
#define PT_LOPROC  0x70000000
#define PT_HIPROC  0x7FFFFFFF

#define PF_X        1
#define PF_W        2
#define PF_R        4
#define PF_MASKOS   0x0F000000
#define PF_MASKPROC 0xF0000000

#define DT_NULL     0
#define DT_NEEDED   1
#define DT_PLTRELSZ 2
#define DT_PLTGOT   3
#define DT_HASH     4
#define DT_STRTAB   5
#define DT_SYMTAB   6
#define DT_RELA     7
#define DT_RELASZ   8
#define DT_RELAENT  9
#define DT_STRSZ    10
#define DT_SYMENT   11
#define DT_INIT     12
#define DT_FINI     13
#define DT_SONAME   14
#define DT_RPATH    15
#define DT_SYMBOLIC 16
#define DT_REL      17
#define DT_RELSZ    18
#define DT_RELENT   19
#define DT_PLTREL   20
#define DT_DEBUG    21
#define DT_TEXTREL  22
#define DT_JMPREL   23
#define DT_BIND_NOW 24
#define DT_INIT_ARRAY   25
#define DT_FINI_ARRAY   26
#define DT_INIT_ARRAYSZ 27
#define DT_FINI_ARRAYSZ 28
#define DT_LOOS     0x60000000
#define DT_HIOS     0x6FFFFFFF
#define DT_LOPROC   0x70000000
#define DT_HIPROC   0x7FFFFFFF


/**
 * A: addend
 * G: symbol's got entry offset
 * GOT: address of got
 * P: current offset
 * S: symbol value
 * L: plt entry offset
 * R_X86_64_64 S+A
 * R_X86_64_GOT64 G+A
 * R_X86_64_GOTOFF64 S+A-GOT
 * R_X86_64_GOTPC64 GOT-P+A
 * R_X86_64_PC32 S+A-P
 * R_X86_64_PLT32 L+A-P
 * R_X86_64_32 S+A
 * R_X86_64_32S S+A
 **/

#define R_X86_64_NONE            0 /* No reloc */
#define R_X86_64_64              1 /* Direct 64 bit S+A */
#define R_X86_64_PC32            2 /* PC relative 32 bit signed S+A-P */
#define R_X86_64_GOT32           3 /* 32 bit GOT entry G+A */
#define R_X86_64_PLT32           4 /* 32 bit PLT address L+A-P */
#define R_X86_64_COPY            5 /* Copy symbol at runtime */
#define R_X86_64_GLOB_DAT        6 /* Create GOT entry */
#define R_X86_64_JUMP_SLOT       7 /* Create PLT entry */
#define R_X86_64_RELATIVE        8 /* Adjust by program base */
#define R_X86_64_GOTPCREL        9 /* 32 bit signed PC relative offset to GOT G+GOT+A-P*/
#define R_X86_64_32              10 /* Direct 32 bit zero extended S+A */
#define R_X86_64_32S             11 /* Direct 32 bit sign extended S+A */
#define R_X86_64_16              12 /* Direct 16 bit zero extended */
#define R_X86_64_PC16            13 /* 16 bit sign extended pc relative */
#define R_X86_64_8               14 /* Direct 8 bit sign extended  */
#define R_X86_64_PC8             15 /* 8 bit sign extended pc relative */
#define R_X86_64_DTPMOD64        16 /* ID of module containing symbol */
#define R_X86_64_DTPOFF64        17 /* Offset in module's TLS block */
#define R_X86_64_TPOFF64         18 /* Offset in initial TLS block */
#define R_X86_64_TLSGD           19 /* 32 bit signed PC relative offset to two GOT entries for GD symbol */
#define R_X86_64_TLSLD           20 /* 32 bit signed PC relative offset to two GOT entries for LD symbol */
#define R_X86_64_DTPOFF32        21 /* Offset in TLS block */
#define R_X86_64_GOTTPOFF        22 /* 32 bit signed PC relative offset to GOT entry for IE symbol */
#define R_X86_64_TPOFF32         23 /* Offset in initial TLS block */
#define R_X86_64_PC64            24 /* PC relative 64 bit */
#define R_X86_64_GOTOFF64        25 /* 64 bit offset to GOT S+A-GOT*/
#define R_X86_64_GOTPC32         26 /* 32 bit signed pc relative offset to GOT */
#define R_X86_64_GOT64           27 /* 64-bit GOT entry offset G+A */
#define R_X86_64_GOTPCREL64      28 /* 64-bit PC relative offset to GOT entry */
#define R_X86_64_GOTPC64         29 /* 64-bit PC relative offset to GOT GOT-P+A */
#define R_X86_64_GOTPLT64        30 /* like GOT64, says PLT entry needed */
#define R_X86_64_PLTOFF64        31 /* 64-bit GOT relative offset to PLT entry */
#define R_X86_64_SIZE32          32 /* Size of symbol plus 32-bit addend */
#define R_X86_64_SIZE64          33 /* Size of symbol plus 64-bit addend */
#define R_X86_64_GOTPC32_TLSDESC 34 /* GOT offset for TLS descriptor.  */
#define R_X86_64_TLSDESC_CALL    35 /* Marker for call through TLS descriptor.  */
#define R_X86_64_TLSDESC         36 /* TLS descriptor.  */
#define R_X86_64_IRELATIVE       37 /* Adjust indirectly by program base */
#define R_X86_64_RELATIVE64      38 /* 64-bit adjust by program base */
#define R_X86_64_GOTPCRELX       41 /* like GOTPCREL, but optionally with linker optimizations */
#define R_X86_64_REX_GOTPCRELX   42 /* like GOTPCRELX, but a REX prefix is present */


#define R_386_32        1 /* Direct 32 bit */
#define R_386_PC32      2 /* PC relative 32 bit */
#define R_386_16       20 /* Direct 16 bit */
#define R_386_PC16     21 /* PC relative 16 bit signed */


typedef struct {
    uint8_t magic[4];
    uint8_t class;
    uint8_t encoding;
    uint8_t version;
    uint8_t padding[9];
    uint8_t size;
}__attribute__((packed)) elf_indent_t;

typedef struct {
    uint8_t  e_indent[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
}__attribute__((packed)) elf32_hdr_t;

typedef struct {
    uint32_t sh_name;
    uint32_t sh_type;
    uint32_t sh_flags;
    uint32_t sh_addr;
    uint32_t sh_offset;
    uint32_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint32_t sh_addralign;
    uint32_t sh_entsize;
}__attribute__((packed)) elf32_shdr_t;

typedef struct {
    uint32_t st_name;
    uint32_t st_value;
    uint32_t st_size;
    uint8_t  st_type  : 4;
    uint8_t  st_scope : 4;
    uint8_t  st_other;
    uint16_t st_shndx;
}__attribute__((packed)) elf32_sym_t;

typedef struct {
    uint32_t r_offset;
    uint8_t  r_symtype;
    uint8_t  r_symindx;
    uint16_t reserved;
}__attribute__((packed)) elf32_rel_t;

typedef struct {
    uint32_t r_offset;
    uint8_t  r_symtype;
    uint8_t  r_symindx;
    uint16_t reserved;
    int32_t  r_addend;
}__attribute__((packed)) elf32_rela_t;

typedef struct {
    uint8_t  e_indent[16];
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
}__attribute__((packed)) elf64_hdr_t;

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
}__attribute__((packed)) elf64_shdr_t;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
}__attribute__((packed)) elf64_phdr_t;

typedef struct {
    uint32_t st_name;
    uint8_t  st_type  : 4;
    uint8_t  st_scope : 4;
    uint8_t  st_other;
    uint16_t st_shndx;
    uint64_t st_value;
    uint64_t st_size;
}__attribute__((packed)) elf64_sym_t;

typedef struct {
    uint64_t r_offset;
    uint32_t r_symtype;
    uint32_t r_symindx;
}__attribute__((packed)) elf64_rel_t;

typedef struct {
    uint64_t r_offset;
    uint32_t r_symtype;
    uint32_t r_symindx;
    int64_t  r_addend;
}__attribute__((packed)) elf64_rela_t;


#define ELF_SECTION_SIZE(c, s, idx) (((c) == ELFCLASS64?(((elf64_shdr_t*)s)[idx].sh_size):(((elf32_shdr_t*)s)[idx].sh_size)))
#define ELF_SECTION_OFFSET(c, s, idx) (((c) == ELFCLASS64?(((elf64_shdr_t*)s)[idx].sh_offset):(((elf32_shdr_t*)s)[idx].sh_offset)))
#define ELF_SECTION_NAME(c, s, idx) (((c) == ELFCLASS64?(((elf64_shdr_t*)s)[idx].sh_name):(((elf32_shdr_t*)s)[idx].sh_name)))
#define ELF_SECTION_TYPE(c, s, idx) (((c) == ELFCLASS64?(((elf64_shdr_t*)s)[idx].sh_type):(((elf32_shdr_t*)s)[idx].sh_type)))
#define ELF_SECTION_ALIGN(c, s, idx) (((c) == ELFCLASS64?(((elf64_shdr_t*)s)[idx].sh_addralign):(((elf32_shdr_t*)s)[idx].sh_addralign)))
#define ELF_SECTION_LINK(c, s, idx) (((c) == ELFCLASS64?(((elf64_shdr_t*)s)[idx].sh_link):(((elf32_shdr_t*)s)[idx].sh_link)))
#define ELF_SECTION_INFO(c, s, idx) (((c) == ELFCLASS64?(((elf64_shdr_t*)s)[idx].sh_info):(((elf32_shdr_t*)s)[idx].sh_info)))

#define ELF_SYMBOL_COUNT(c, s) ((c) == ELFCLASS64?(s / sizeof(elf64_sym_t)):(s / sizeof(elf32_sym_t)))

#define ELF_SYMBOL_NAME(c, s, idx) (((c) == ELFCLASS64?(((elf64_sym_t*)s)[idx].st_name):(((elf32_sym_t*)s)[idx].st_name)))
#define ELF_SYMBOL_TYPE(c, s, idx) (((c) == ELFCLASS64?(((elf64_sym_t*)s)[idx].st_type):(((elf32_sym_t*)s)[idx].st_type)))
#define ELF_SYMBOL_SCOPE(c, s, idx) (((c) == ELFCLASS64?(((elf64_sym_t*)s)[idx].st_scope):(((elf32_sym_t*)s)[idx].st_scope)))
#define ELF_SYMBOL_SHNDX(c, s, idx) (((c) == ELFCLASS64?(((elf64_sym_t*)s)[idx].st_shndx):(((elf32_sym_t*)s)[idx].st_shndx)))
#define ELF_SYMBOL_VALUE(c, s, idx) (((c) == ELFCLASS64?(((elf64_sym_t*)s)[idx].st_value):(((elf32_sym_t*)s)[idx].st_value)))
#define ELF_SYMBOL_SIZE(c, s, idx) (((c) == ELFCLASS64?(((elf64_sym_t*)s)[idx].st_size):(((elf32_sym_t*)s)[idx].st_size)))

#define ELF_RELOC_COUNT(c, rt, rs) ((rs) / ((c) == ELFCLASS64?((rt)?sizeof(elf64_rela_t):sizeof(elf64_rel_t)):((rt)?sizeof(elf32_rela_t):sizeof(elf32_rel_t))))

#define ELF_RELOC_OFFSET(c, rt, rs, idx) ((c) == ELFCLASS64?((rt)?((elf64_rela_t*)rs)[idx].r_offset:((elf64_rel_t*)rs)[idx].r_offset):((rt)?((elf32_rela_t*)rs)[idx].r_offset:((elf32_rel_t*)rs)[idx].r_offset))
#define ELF_RELOC_TYPE(c, rt, rs, idx) ((c) == ELFCLASS64?((rt)?((elf64_rela_t*)rs)[idx].r_symtype:((elf64_rel_t*)rs)[idx].r_symtype):((rt)?((elf32_rela_t*)rs)[idx].r_symtype:((elf32_rel_t*)rs)[idx].r_symtype))
#define ELF_RELOC_SYMIDX(c, rt, rs, idx) ((c) == ELFCLASS64?((rt)?((elf64_rela_t*)rs)[idx].r_symindx:((elf64_rel_t*)rs)[idx].r_symindx):((rt)?((elf32_rela_t*)rs)[idx].r_symindx:((elf32_rel_t*)rs)[idx].r_symindx))
#define ELF_RELOC_ADDEND(c, rs, idx) (((c) == ELFCLASS64?(((elf64_rela_t*)rs)[idx].r_addend):(((elf32_rela_t*)rs)[idx].r_addend)))

#endif
