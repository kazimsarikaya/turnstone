/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#define RAMSIZE 0x800000
#include "setup.h"
#include "elf64.h"

int32_t main(void);

int32_t main(void) {


    FILE* fp = fopen("tmp/test.bin", "wb");

    if(!fp) {
        return -1;
    }

    uint64_t offset = sizeof(elf64_hdr_t);

    fseek(fp, offset, SEEK_SET);

    uint64_t text_offset = offset;

    uint8_t text[] = {
        0x48, 0xc7, 0xc0, 0x3c, 0x00, 0x00, 0x00, // mov 60 to %rax
        0x31, 0xff, // set edi to 0
        0x0f, 0x05, // syscall
    };

    fwrite(text, sizeof(uint8_t), sizeof(text), fp);

    offset += sizeof(text);

    uint64_t shstrtab_offset = offset;

    uint8_t shstrtab[] = {
        0x0,  // null
        '.', 's', 'h', 's', 't', 'r', 't', 'a', 'b', 0x0,  // .shstrtab
        '.', 't', 'e', 'x', 't', 0x0  // .text
    };

    fwrite(shstrtab, sizeof(uint8_t), sizeof(shstrtab), fp);

    offset += sizeof(shstrtab);
    uint64_t sh_offset = offset;

    uint64_t sh_count = 3;


    elf64_shdr_t* section_header = memory_malloc(sizeof(elf64_shdr_t) * sh_count);

    if(!section_header) {
        fclose(fp);

        return -1;
    }

    section_header[1].sh_name = 11;    // section name
    section_header[1].sh_type = SHT_PROGBITS;    // program bits
    section_header[1].sh_flags = SHF_ALLOC | SHF_EXECINSTR;    // read and execute
    section_header[1].sh_addr = 0x400000 + text_offset;    // virtual address
    section_header[1].sh_offset = text_offset;    // offset
    section_header[1].sh_size = sizeof(text);    // size
    section_header[1].sh_addralign = 0x1;    // alignment


    section_header[sh_count - 1].sh_name = 1;    // section name
    section_header[sh_count - 1].sh_type = SHT_STRTAB;    // string table
    section_header[sh_count - 1].sh_flags = SHF_NONE;    // no flags
    section_header[sh_count - 1].sh_offset = shstrtab_offset;    // offset
    section_header[sh_count - 1].sh_size = sizeof(shstrtab);    // size

    fwrite(section_header, sizeof(elf64_shdr_t), sh_count, fp);
    memory_free(section_header);

    offset += sizeof(elf64_shdr_t) * sh_count;

    uint64_t ph_offset = offset;


    uint64_t ph_count = 1;
    elf64_phdr_t* program_header = memory_malloc(sizeof(elf64_phdr_t) * ph_count);

    if(!program_header) {
        fclose(fp);

        return -1;
    }

    program_header[0].p_type = PT_LOAD; // loadable segment
    program_header[0].p_flags = PF_R | PF_X; // read and execute
    program_header[0].p_offset = text_offset; // offset
    program_header[0].p_vaddr = 0x400000 + text_offset; // virtual address
    program_header[0].p_paddr = 0x400000 + text_offset; // physical address
    program_header[0].p_filesz = sizeof(text); // file size
    program_header[0].p_memsz = sizeof(text); // memory size
    program_header[0].p_align = 0x1000; // alignment

    fwrite(program_header, sizeof(elf64_phdr_t), ph_count, fp);
    memory_free(program_header);

    elf64_hdr_t* elf_header = memory_malloc(sizeof(elf64_hdr_t));

    if(!elf_header) {
        return -1;
    }

    elf_indent_t indent = {0};
    indent.magic[0] = 0x7f;
    indent.magic[1] = 'E';
    indent.magic[2] = 'L';
    indent.magic[3] = 'F';
    indent.class = ELFCLASS64;
    indent.encoding = ELFDATA2LSB;
    indent.version = EV_CURRENT;
    indent.size = 0;

    memory_memcopy(&indent, elf_header, sizeof(elf_indent_t));
    elf_header->e_type = ET_EXEC; // executable file
    elf_header->e_machine = EM_X86_64; // x86_64
    elf_header->e_version = EV_CURRENT; // current version
    elf_header->e_entry = 0x400000 + text_offset; // entry point
    elf_header->e_phoff = ph_offset; // 64 bit elf header size
    elf_header->e_shoff = sh_offset;  // no section header
    elf_header->e_flags = 0x0; // no flags
    elf_header->e_ehsize = sizeof(elf64_hdr_t); // 64 bit elf header size
    elf_header->e_phentsize = sizeof(elf64_phdr_t); // 64 bit program header size
    elf_header->e_phnum = ph_count; // program header count
    elf_header->e_shentsize = sizeof(elf64_shdr_t); // no section header
    elf_header->e_shnum = sh_count; // no section header
    elf_header->e_shstrndx = sh_count - 1; // no section header


    fseek(fp, 0, SEEK_SET);
    fwrite(elf_header, sizeof(elf64_hdr_t), 1, fp);
    memory_free(elf_header);

    fclose(fp);

    return 0;
}
