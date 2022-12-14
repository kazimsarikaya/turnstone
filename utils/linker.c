/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#define RAMSIZE 0x8000000
#include "setup.h"
#include "elf64.h"
#include <utils.h>
#include <strings.h>
#include <linkedlist.h>
#include <linker.h>
#include <crc.h>
#include <efi.h>

typedef enum {
    LINKER_SYMBOL_TYPE_UNDEF,
    LINKER_SYMBOL_TYPE_OBJECT,
    LINKER_SYMBOL_TYPE_FUNCTION,
    LINKER_SYMBOL_TYPE_SECTION,
    LINKER_SYMBOL_TYPE_SYMBOL,
}linker_symbol_type_t;

typedef enum {
    LINKER_SYMBOL_SCOPE_LOCAL,
    LINKER_SYMBOL_SCOPE_GLOBAL,
} linker_symbol_scope_t;

typedef struct {
    uint64_t              id;
    linker_section_type_t type;
    char_t*               file_name;
    char_t*               section_name;
    uint64_t              offset;
    uint64_t              size;
    uint64_t              align;
    uint64_t              padding;
    uint8_t*              data;
    linkedlist_t          relocations;
    uint8_t               required;
} linker_section_t;

typedef struct {
    uint64_t              id;
    linker_symbol_type_t  type;
    linker_symbol_scope_t scope;
    uint64_t              section_id;
    char_t*               symbol_name;
    uint64_t              value;
    uint64_t              size;
    uint8_t               required;
} linker_symbol_t;

typedef struct {
    linker_relocation_type_t type;
    uint64_t                 symbol_id;
    uint64_t                 offset;
    int64_t                  addend;
} linker_relocation_t;

typedef struct {
    char_t*  file_name;
    FILE*    file;
    uint8_t  type;
    uint8_t* sections;
    char_t*  shstrtab;
    char_t*  strtab;
    uint8_t* symbols;
    uint8_t* relocs;
    uint64_t symbol_count;
    uint64_t section_count;
} linker_objectfile_ctx_t;

typedef struct {
    memory_heap_t*              heap;
    uint8_t                     class;
    char_t*                     entry_point;
    uint64_t                    start;
    uint64_t                    stack_size;
    char_t*                     output;
    char_t*                     map_file;
    linkedlist_t                symbols;
    linkedlist_t                sections;
    uint64_t                    direct_relocation_count;
    linker_direct_relocation_t* direct_relocations;
    linker_section_locations_t  section_locations[LINKER_SECTION_TYPE_NR_SECTIONS];
    uint8_t                     enable_removing_disabled_sections;
    uint8_t                     boot_flag;
    linker_objectfile_ctx_t     objectfile_ctx;
    uint8_t                     test_section_flag;
    uint64_t                    test_func_array_secid;
    linkedlist_t                test_function_names;
    uint64_t                    test_func_names_array_str_size;
    uint64_t                    test_func_names_array_str_secid;
    uint64_t                    test_functions_names_array_secid;
    boolean_t                   for_efi;
} linker_context_t;

typedef struct {
    uint32_t     start_rva;
    linkedlist_t relocs;
} linker_efi_reloc_block_t;

linker_symbol_t*  linker_lookup_symbol(linker_context_t* ctx, char_t* symbol_name, uint64_t section_id);
linker_symbol_t*  linker_get_symbol_by_id(linker_context_t* ctx, uint64_t id);
void              linker_print_symbols(linker_context_t* ctx);
void              linker_bind_offset_of_section(linker_context_t* ctx, linker_section_type_t type, uint64_t* base_offset);
int8_t            linker_section_offset_comparator(const void* sym1, const void* sym2);
int8_t            linker_sort_sections_by_offset(linker_context_t* ctx);
uint64_t          linker_get_section_id(linker_context_t* ctx, char_t* file_name, char_t* section_name);
linker_section_t* linker_get_section_by_id(linker_context_t* ctx, uint64_t id);
int8_t            linker_write_output(linker_context_t* ctx);
int8_t            linker_write_efi_output(linker_context_t* ctx);
int8_t            linker_parse_script(linker_context_t* ctx, char_t* linker_script);
void              linker_destroy_context(linker_context_t* ctx);
int8_t            linker_tag_required_sections(linker_context_t* ctx);
int8_t            linker_tag_required_section(linker_context_t* ctx, linker_section_t* section);
int8_t            linker_parse_elf_header(linker_context_t* ctx, uint64_t* section_id);
uint64_t          linker_get_symbol_count(linker_context_t* ctx);
char_t*           linker_get_section_name(linker_context_t* ctx, uint16_t sec_idx);
char_t*           linker_get_symbol_name(linker_context_t* ctx, uint16_t sec_idx);
uint64_t          linker_get_section_size(linker_context_t* ctx, uint16_t sec_idx);
uint64_t          linker_get_section_type(linker_context_t* ctx, uint16_t sec_idx);
uint64_t          linker_get_section_addralign(linker_context_t* ctx, uint16_t sec_idx);
uint64_t          linker_get_section_offset(linker_context_t* ctx, uint16_t sec_idx);
uint64_t          linker_get_section_index_of_symbol(linker_context_t* ctx, uint16_t sym_idx);
uint8_t           linker_get_type_of_symbol(linker_context_t* ctx, uint16_t sym_idx);
uint8_t           linker_get_scope_of_symbol(linker_context_t* ctx, uint16_t sym_idx);
uint64_t          linker_get_value_of_symbol(linker_context_t* ctx, uint16_t sym_idx);
uint64_t          linker_get_size_of_symbol(linker_context_t* ctx, uint16_t sym_idx);
uint64_t          linker_get_relocation_symbol_index(linker_context_t* ctx, uint16_t reloc_idx, uint8_t is_rela);
uint64_t          linker_get_relocation_symbol_offset(linker_context_t* ctx, uint16_t reloc_idx, uint8_t is_rela);
uint64_t          linker_get_relocation_symbol_type(linker_context_t* ctx, uint16_t reloc_idx, uint8_t is_rela);
int64_t           linker_get_relocation_symbol_addend(linker_context_t* ctx, uint16_t reloc_idx, uint8_t is_rela);
void              linker_destroy_objectctx(linker_context_t* ctx);
int8_t            linker_test_function_names_comparator(const void* name1, const void* name2);
int8_t            linker_relocate_test_functions(linker_context_t* ctx);

int8_t linker_test_function_names_comparator(const void* name1, const void* name2){

    char_t* name1_str = (char_t*)name1;
    char_t* name2_str = (char_t*)name2;

    uint64_t idx_name1_str = strlen(name1_str) - 1;

    while(name1_str[idx_name1_str] != 0x5f) {
        idx_name1_str--;
    }

    uint64_t idx_name2_str = strlen(name2_str) - 1;

    while(name2_str[idx_name2_str] != 0x5f) {
        idx_name2_str--;
    }

    if(idx_name1_str == idx_name2_str) {
        if(strncmp(name1_str, name2_str, idx_name1_str) == 0) {
            int64_t line1 = atoi(name1_str + idx_name1_str + 1);
            int64_t line2 = atoi(name2_str + idx_name2_str + 1);

            if(line1 < line2) {
                return -1;
            } else if(line1 > line2) {
                return 1;
            }

            return 0;
        }
    }

    return strcmp((char_t*)name1, (char_t*)name2);
}

uint64_t linker_get_relocation_symbol_index(linker_context_t* ctx, uint16_t reloc_idx, uint8_t is_rela) {
    if(ctx->objectfile_ctx.type == ELFCLASS32) {
        if(is_rela) {
            elf32_rela_t* relocs = (elf32_rela_t*)ctx->objectfile_ctx.relocs;

            return relocs[reloc_idx].r_symindx;
        } else {
            elf32_rel_t* relocs = (elf32_rel_t*)ctx->objectfile_ctx.relocs;

            return relocs[reloc_idx].r_symindx;
        }
    } else if(ctx->objectfile_ctx.type == ELFCLASS64) {
        if(is_rela) {
            elf64_rela_t* relocs = (elf64_rela_t*)ctx->objectfile_ctx.relocs;

            return relocs[reloc_idx].r_symindx;
        } else {
            elf64_rel_t* relocs = (elf64_rel_t*)ctx->objectfile_ctx.relocs;

            return relocs[reloc_idx].r_symindx;
        }
    }

    return NULL;
}

uint64_t linker_get_relocation_symbol_offset(linker_context_t* ctx, uint16_t reloc_idx, uint8_t is_rela) {
    if(ctx->objectfile_ctx.type == ELFCLASS32) {
        if(is_rela) {
            elf32_rela_t* relocs = (elf32_rela_t*)ctx->objectfile_ctx.relocs;

            return relocs[reloc_idx].r_offset;
        } else {
            elf32_rel_t* relocs = (elf32_rel_t*)ctx->objectfile_ctx.relocs;

            return relocs[reloc_idx].r_offset;
        }
    } else if(ctx->objectfile_ctx.type == ELFCLASS64) {
        if(is_rela) {
            elf64_rela_t* relocs = (elf64_rela_t*)ctx->objectfile_ctx.relocs;

            return relocs[reloc_idx].r_offset;
        } else {
            elf64_rel_t* relocs = (elf64_rel_t*)ctx->objectfile_ctx.relocs;

            return relocs[reloc_idx].r_offset;
        }
    }

    return NULL;
}

uint64_t linker_get_relocation_symbol_type(linker_context_t* ctx, uint16_t reloc_idx, uint8_t is_rela) {
    if(ctx->objectfile_ctx.type == ELFCLASS32) {
        if(is_rela) {
            elf32_rela_t* relocs = (elf32_rela_t*)ctx->objectfile_ctx.relocs;

            return relocs[reloc_idx].r_symtype;
        } else {
            elf32_rel_t* relocs = (elf32_rel_t*)ctx->objectfile_ctx.relocs;

            return relocs[reloc_idx].r_symtype;
        }
    } else if(ctx->objectfile_ctx.type == ELFCLASS64) {
        if(is_rela) {
            elf64_rela_t* relocs = (elf64_rela_t*)ctx->objectfile_ctx.relocs;

            return relocs[reloc_idx].r_symtype;
        } else {
            elf64_rel_t* relocs = (elf64_rel_t*)ctx->objectfile_ctx.relocs;

            return relocs[reloc_idx].r_symtype;
        }
    }

    return NULL;
}

int64_t linker_get_relocation_symbol_addend(linker_context_t* ctx, uint16_t reloc_idx, uint8_t is_rela) {
    if(is_rela == 0) {
        return 0;
    }

    if(ctx->objectfile_ctx.type == ELFCLASS32) {
        elf32_rela_t* relocs = (elf32_rela_t*)ctx->objectfile_ctx.relocs;

        return relocs[reloc_idx].r_addend;
    } else if(ctx->objectfile_ctx.type == ELFCLASS64) {
        elf64_rela_t* relocs = (elf64_rela_t*)ctx->objectfile_ctx.relocs;

        return relocs[reloc_idx].r_addend;
    }

    return NULL;
}

void linker_destroy_objectctx(linker_context_t* ctx) {
    if(ctx->objectfile_ctx.file) {
        fclose(ctx->objectfile_ctx.file);
    }

    // memory_free_ext(ctx->heap, ctx->objectfile_ctx.file_name);
    memory_free_ext(ctx->heap, ctx->objectfile_ctx.sections);
    memory_free_ext(ctx->heap, ctx->objectfile_ctx.shstrtab);
    memory_free_ext(ctx->heap, ctx->objectfile_ctx.strtab);
    memory_free_ext(ctx->heap, ctx->objectfile_ctx.symbols);
    memory_free_ext(ctx->heap, ctx->objectfile_ctx.relocs);

    memory_memclean(&ctx->objectfile_ctx, sizeof(linker_objectfile_ctx_t));
}

uint64_t linker_get_symbol_count(linker_context_t* ctx){
    return ctx->objectfile_ctx.symbol_count;
}

char_t* linker_get_section_name(linker_context_t* ctx, uint16_t sec_idx){
    if(ctx->objectfile_ctx.type == ELFCLASS32) {
        elf32_shdr_t* sections = (elf32_shdr_t*)ctx->objectfile_ctx.sections;

        return ctx->objectfile_ctx.shstrtab + sections[sec_idx].sh_name;
    } else if(ctx->objectfile_ctx.type == ELFCLASS64) {
        elf64_shdr_t* sections = (elf64_shdr_t*)ctx->objectfile_ctx.sections;

        return ctx->objectfile_ctx.shstrtab + sections[sec_idx].sh_name;
    }

    return NULL;
}

uint64_t linker_get_section_size(linker_context_t* ctx, uint16_t sec_idx){
    if(ctx->objectfile_ctx.type  == ELFCLASS32) {
        elf32_shdr_t* sections = (elf32_shdr_t*)ctx->objectfile_ctx.sections;

        return sections[sec_idx].sh_size;
    } else if(ctx->objectfile_ctx.type == ELFCLASS64) {
        elf64_shdr_t* sections = (elf64_shdr_t*)ctx->objectfile_ctx.sections;

        return sections[sec_idx].sh_size;
    }

    return 0;
}

uint64_t linker_get_section_offset(linker_context_t* ctx, uint16_t sec_idx){
    if(ctx->objectfile_ctx.type == ELFCLASS32) {
        elf32_shdr_t* sections = (elf32_shdr_t*)ctx->objectfile_ctx.sections;

        return sections[sec_idx].sh_offset;
    } else if(ctx->objectfile_ctx.type == ELFCLASS64) {
        elf64_shdr_t* sections = (elf64_shdr_t*)ctx->objectfile_ctx.sections;

        return sections[sec_idx].sh_offset;
    }

    return 0;
}

uint64_t linker_get_section_addralign(linker_context_t* ctx, uint16_t sec_idx){
    if(ctx->objectfile_ctx.type == ELFCLASS32) {
        elf32_shdr_t* sections = (elf32_shdr_t*)ctx->objectfile_ctx.sections;

        return sections[sec_idx].sh_addralign;
    } else if(ctx->objectfile_ctx.type == ELFCLASS64) {
        elf64_shdr_t* sections = (elf64_shdr_t*)ctx->objectfile_ctx.sections;

        return sections[sec_idx].sh_addralign;
    }

    return 0;
}

uint64_t linker_get_section_type(linker_context_t* ctx, uint16_t sec_idx){
    if(ctx->objectfile_ctx.type == ELFCLASS32) {
        elf32_shdr_t* sections = (elf32_shdr_t*)ctx->objectfile_ctx.sections;

        return sections[sec_idx].sh_type;
    } else if(ctx->objectfile_ctx.type == ELFCLASS64) {
        elf64_shdr_t* sections = (elf64_shdr_t*)ctx->objectfile_ctx.sections;

        return sections[sec_idx].sh_type;
    }

    return SHT_NULL;
}

char_t* linker_get_symbol_name(linker_context_t* ctx, uint16_t sym_idx){
    if(ctx->objectfile_ctx.type == ELFCLASS32) {
        elf32_sym_t* symbols = (elf32_sym_t*)ctx->objectfile_ctx.symbols;

        return ctx->objectfile_ctx.strtab + symbols[sym_idx].st_name;
    } else if(ctx->objectfile_ctx.type == ELFCLASS64) {
        elf64_sym_t* symbols = (elf64_sym_t*)ctx->objectfile_ctx.symbols;

        return ctx->objectfile_ctx.strtab + symbols[sym_idx].st_name;
    }

    return NULL;
}

uint64_t linker_get_section_index_of_symbol(linker_context_t* ctx, uint16_t sym_idx) {
    if(ctx->objectfile_ctx.type == ELFCLASS32) {
        elf32_sym_t* symbols = (elf32_sym_t*)ctx->objectfile_ctx.symbols;

        return symbols[sym_idx].st_shndx;
    } else if(ctx->objectfile_ctx.type == ELFCLASS64) {
        elf64_sym_t* symbols = (elf64_sym_t*)ctx->objectfile_ctx.symbols;

        return symbols[sym_idx].st_shndx;
    }

    return 0;
}

uint64_t linker_get_value_of_symbol(linker_context_t* ctx, uint16_t sym_idx) {
    if(ctx->objectfile_ctx.type == ELFCLASS32) {
        elf32_sym_t* symbols = (elf32_sym_t*)ctx->objectfile_ctx.symbols;

        return symbols[sym_idx].st_value;
    } else if(ctx->objectfile_ctx.type == ELFCLASS64) {
        elf64_sym_t* symbols = (elf64_sym_t*)ctx->objectfile_ctx.symbols;

        return symbols[sym_idx].st_value;
    }

    return 0;
}

uint64_t linker_get_size_of_symbol(linker_context_t* ctx, uint16_t sym_idx) {
    if(ctx->objectfile_ctx.type == ELFCLASS32) {
        elf32_sym_t* symbols = (elf32_sym_t*)ctx->objectfile_ctx.symbols;

        return symbols[sym_idx].st_size;
    } else if(ctx->objectfile_ctx.type == ELFCLASS64) {
        elf64_sym_t* symbols = (elf64_sym_t*)ctx->objectfile_ctx.symbols;

        return symbols[sym_idx].st_size;
    }

    return 0;
}

uint8_t linker_get_type_of_symbol(linker_context_t* ctx, uint16_t sym_idx) {
    if(ctx->objectfile_ctx.type == ELFCLASS32) {
        elf32_sym_t* symbols = (elf32_sym_t*)ctx->objectfile_ctx.symbols;

        return symbols[sym_idx].st_type;
    } else if(ctx->objectfile_ctx.type == ELFCLASS64) {
        elf64_sym_t* symbols = (elf64_sym_t*)ctx->objectfile_ctx.symbols;

        return symbols[sym_idx].st_type;
    }

    return 0;
}

uint8_t linker_get_scope_of_symbol(linker_context_t* ctx, uint16_t sym_idx) {
    if(ctx->objectfile_ctx.type == ELFCLASS32) {
        elf32_sym_t* symbols = (elf32_sym_t*)ctx->objectfile_ctx.symbols;

        return symbols[sym_idx].st_scope;
    } else if(ctx->objectfile_ctx.type == ELFCLASS64) {
        elf64_sym_t* symbols = (elf64_sym_t*)ctx->objectfile_ctx.symbols;

        return symbols[sym_idx].st_scope;
    }

    return 0;
}

int8_t linker_parse_elf_header(linker_context_t* ctx, uint64_t* section_id) {
    ctx->objectfile_ctx.file = fopen(ctx->objectfile_ctx.file_name, "r");

    elf_indent_t e_indent;

    fread(&e_indent, sizeof(elf_indent_t), 1, ctx->objectfile_ctx.file);

    if(ctx->class == 0) {
        ctx->class = e_indent.class;
    } else if(ctx->class != e_indent.class) {
        print_error("class change\n");
        return -1;
    }

    ctx->objectfile_ctx.type = e_indent.class;

    fseek(ctx->objectfile_ctx.file, 0, SEEK_SET);

    uint16_t e_shnum = 0;
    uint64_t e_shoff = 0;
    uint16_t e_shstrndx = 0;

    uint64_t e_shsize = 0;

    if(e_indent.class == ELFCLASS32) {

        elf32_hdr_t hdr;
        fread(&hdr, sizeof(elf32_hdr_t), 1, ctx->objectfile_ctx.file);

        e_shnum = hdr.e_shnum;
        e_shoff = hdr.e_shoff;
        e_shstrndx = hdr.e_shstrndx;
        e_shsize = sizeof(elf32_shdr_t) * e_shnum;

    } else if(e_indent.class == ELFCLASS64) {

        elf64_hdr_t hdr;
        fread(&hdr, sizeof(elf64_hdr_t), 1, ctx->objectfile_ctx.file);

        e_shnum = hdr.e_shnum;
        e_shoff = hdr.e_shoff;
        e_shstrndx = hdr.e_shstrndx;
        e_shsize = sizeof(elf64_shdr_t) * e_shnum;

    } else {
        printf("unknown file class %i\n", e_indent.class );
        return -1;
    }

    if(e_shnum == 0) {
        return -1;
    }

    ctx->objectfile_ctx.section_count = e_shnum;

    ctx->objectfile_ctx.sections  = memory_malloc_ext(ctx->heap, e_shsize, 0x0);
    fseek(ctx->objectfile_ctx.file, e_shoff, SEEK_SET);
    fread(ctx->objectfile_ctx.sections, e_shsize, 1, ctx->objectfile_ctx.file);


    ctx->objectfile_ctx.shstrtab = memory_malloc_ext(ctx->heap, linker_get_section_size(ctx, e_shstrndx), 0x0);
    fseek(ctx->objectfile_ctx.file, linker_get_section_offset(ctx, e_shstrndx), SEEK_SET);
    fread(ctx->objectfile_ctx.shstrtab, linker_get_section_size(ctx, e_shstrndx), 1, ctx->objectfile_ctx.file);


    uint8_t req_sec_found = 0;

    for(uint16_t sec_idx = 0; sec_idx < e_shnum; sec_idx++) {
        uint64_t sec_size = linker_get_section_size(ctx, sec_idx);
        uint64_t sec_off = linker_get_section_offset(ctx, sec_idx);

        if(strcmp(".strtab", linker_get_section_name(ctx, sec_idx)) == 0) {
            ctx->objectfile_ctx.strtab = memory_malloc_ext(ctx->heap, sec_size, 0x0);
            fseek(ctx->objectfile_ctx.file, sec_off, SEEK_SET);
            fread(ctx->objectfile_ctx.strtab, sec_size, 1, ctx->objectfile_ctx.file);
            req_sec_found++;
        }

        if(linker_get_section_type(ctx, sec_idx) == SHT_SYMTAB) {
            ctx->objectfile_ctx.symbols = memory_malloc_ext(ctx->heap, sec_size, 0x0);
            fseek(ctx->objectfile_ctx.file, sec_off, SEEK_SET);
            fread(ctx->objectfile_ctx.symbols, sec_size, 1, ctx->objectfile_ctx.file);
            req_sec_found++;

            if(e_indent.class == ELFCLASS32) {
                ctx->objectfile_ctx.symbol_count = sec_size / sizeof(elf32_sym_t);
            } else {
                ctx->objectfile_ctx.symbol_count = sec_size / sizeof(elf64_sym_t);
            }
        }

        char_t* secname = linker_get_section_name(ctx, sec_idx);

        if(linker_get_section_size(ctx, sec_idx) &&   (
               strstarts(secname, ".text") == 0 ||
               strstarts(secname, ".data") == 0 ||
               strstarts(secname, ".rodata") == 0 ||
               strstarts(secname, ".bss") == 0
               )) {
            linker_section_t* sec = memory_malloc_ext(ctx->heap, sizeof(linker_section_t), 0x0);

            sec->id = *section_id;

            (*section_id)++;

            sec->file_name = strdup_at_heap(ctx->heap, ctx->objectfile_ctx.file_name);
            sec->section_name = strdup_at_heap(ctx->heap, secname);
            sec->size = sec_size;
            sec->align = linker_get_section_addralign(ctx, sec_idx);

            if(strstarts(secname, ".text") == 0) {
                sec->type = LINKER_SECTION_TYPE_TEXT;
            } else if(strstarts(secname, ".data") == 0) {
                sec->type = LINKER_SECTION_TYPE_DATA;
            } else if(strstarts(secname, ".rodata") == 0) {
                sec->type = LINKER_SECTION_TYPE_RODATA;
            } else if(strstarts(secname, ".bss") == 0) {
                sec->type = LINKER_SECTION_TYPE_BSS;
            }

            if(strstarts(secname, ".bss") != 0) {
                sec->data = memory_malloc_ext(ctx->heap, sec->size, 0x0);

                fseek(ctx->objectfile_ctx.file, sec_off, SEEK_SET);
                fread(sec->data, sec->size, 1, ctx->objectfile_ctx.file);
            }

            linkedlist_list_insert(ctx->sections, sec);
        }

    }

    if(req_sec_found != 2) {
        print_error("strtab/symtab not found!");
        return -1;
    }

    return 0;
}



int8_t linker_tag_required_sections(linker_context_t* ctx) {
    if(ctx->enable_removing_disabled_sections == 0) {
        return 0;
    }

    if(ctx->test_section_flag == 0) {
        ctx->direct_relocation_count = 0;
    }

    linker_symbol_t* ep_sym = linker_lookup_symbol(ctx, ctx->entry_point, 0);

    if(ep_sym == NULL) {
        printf("entry point not found %s\n", ctx->entry_point);
        return -1;
    }

    linker_section_t* ep_sec = linker_get_section_by_id(ctx, ep_sym->section_id);

    if(ep_sym == NULL) {
        printf("entry point section not found %s\n", ctx->entry_point);
        return -1;
    }

    return linker_tag_required_section(ctx, ep_sec);
}


int8_t linker_tag_required_section(linker_context_t* ctx, linker_section_t* section) {
    if(ctx->enable_removing_disabled_sections == 0) {
        return 0;
    }

    if(section == NULL) {
        printf("error at tagging required section. section is null\n");
        return -1;
    }

    if(section->required) {
        return 0;
    }

    section->required = 1;

    int8_t res = 0;


    if(section->relocations) {
        iterator_t* iter = linkedlist_iterator_create(section->relocations);

        if(iter != NULL) {
            while(iter->end_of_iterator(iter) != 0) {
                linker_relocation_t* reloc = iter->get_item(iter);

                if(reloc->type == LINKER_RELOCATION_TYPE_32_16 ||
                   reloc->type == LINKER_RELOCATION_TYPE_32_32 ||
                   reloc->type == LINKER_RELOCATION_TYPE_64_32 ||
                   reloc->type == LINKER_RELOCATION_TYPE_64_32S ||
                   reloc->type == LINKER_RELOCATION_TYPE_64_64 ||
                   reloc->type == LINKER_RELOCATION_TYPE_64_PC32) {
                    ctx->direct_relocation_count++;
                }

                linker_symbol_t* sym = linker_get_symbol_by_id(ctx, reloc->symbol_id);

                sym->required = 1;

                if(sym == NULL) {
                    printf("cannot find required symbol %li at for section %s reloc list\n", reloc->symbol_id, section->section_name);
                    res = -1;
                    break;
                }

                linker_section_t* sub_section = linker_get_section_by_id(ctx, sym->section_id);

                res = linker_tag_required_section(ctx, sub_section);

                if(res != 0) {
                    printf("error at symbol %li %s symsec %li\n", sym->id, sym->symbol_name, sym->section_id);
                    break;
                }

                iter = iter->next(iter);
            }

            iter->destroy(iter);

        }

    }

    return res;
}

int8_t linker_relocate_test_functions(linker_context_t* ctx) {
    int8_t res = -1;
    int8_t iter_err = 0;

    iterator_t* iter = linkedlist_iterator_create(ctx->test_function_names);

    if(iter != NULL) {
        uint64_t tf_cnt = linkedlist_size(ctx->test_function_names);
        uint64_t tf_sec_size = 0;
        uint64_t tfn_sec_size = 0;

        if(ctx->class == ELFCLASS32) {
            tf_sec_size = tf_cnt * 4;
            tfn_sec_size = tf_cnt * 4;
        } else {
            tf_sec_size = tf_cnt * 8;
            tfn_sec_size = tf_cnt * 8;
        }

        linker_section_t* test_func_array = linker_get_section_by_id(ctx, ctx->test_func_array_secid);

        if(test_func_array == NULL) {
            return -1;
        }

        test_func_array->size = tf_sec_size;

        linker_symbol_t* test_functions_array_end = linker_lookup_symbol(ctx, "__test_functions_array_end", 0);

        if(test_functions_array_end == NULL) {
            return -1;
        }

        test_functions_array_end->value = tf_sec_size;

        linker_section_t* test_functions_names_array = linker_get_section_by_id(ctx, ctx->test_functions_names_array_secid);

        if(test_functions_names_array == NULL) {
            return -1;
        }

        test_functions_names_array->size = tfn_sec_size;


        linker_section_t* test_functions_names_array_str = linker_get_section_by_id(ctx, ctx->test_func_names_array_str_secid);

        if(test_functions_names_array_str == NULL) {
            return -1;
        }

        test_functions_names_array_str->size = ctx->test_func_names_array_str_size;

        test_func_array->relocations = linkedlist_create_list_with_heap(ctx->heap);

        if(test_func_array->relocations == NULL) {
            return -1;
        }

        test_functions_names_array->relocations = linkedlist_create_list_with_heap(ctx->heap);

        if(test_functions_names_array->relocations == NULL) {
            return -1;
        }

        test_func_array->data = memory_malloc_ext(ctx->heap, test_func_array->size, 0x0);

        if(test_func_array->data == NULL) {
            return -1;
        }

        test_functions_names_array->data = memory_malloc_ext(ctx->heap, test_functions_names_array->size, 0x0);

        if(test_functions_names_array->data == NULL) {
            return -1;
        }

        test_functions_names_array_str->data = memory_malloc_ext(ctx->heap, test_functions_names_array_str->size, 0x0);

        if(test_functions_names_array_str->data == NULL) {
            return -1;
        }


        linker_symbol_t* test_functions_names_str = linker_lookup_symbol(ctx, "__test_functions_names_str", 0);

        if(test_functions_names_str == NULL) {
            return -1;
        }


        uint64_t f_offset = 0;
        uint64_t fn_offset = 0;
        uint64_t str_offset = 0;


        while(iter->end_of_iterator(iter) != 0) {
            char_t* test_func_name = iter->get_item(iter);

            strcpy(test_func_name + 2, (char_t*)test_functions_names_array_str->data + str_offset);

            linker_symbol_t* tf_sym = linker_lookup_symbol(ctx, test_func_name, 0);

            linker_relocation_t* tf_reloc = memory_malloc_ext(ctx->heap, sizeof(linker_relocation_t), 0x0);

            if(tf_reloc == NULL) {
                iter_err = -1;
                break;
            }

            tf_reloc->symbol_id = tf_sym->id;
            tf_reloc->offset = f_offset;

            linkedlist_list_insert(test_func_array->relocations, tf_reloc);

            linker_relocation_t* tfn_reloc = memory_malloc_ext(ctx->heap, sizeof(linker_relocation_t), 0x0);

            if(tfn_reloc == NULL) {
                iter_err = -1;
                break;
            }

            tfn_reloc->symbol_id = test_functions_names_str->id;
            tfn_reloc->offset = fn_offset;
            tfn_reloc->addend = str_offset;

            linkedlist_list_insert(test_functions_names_array->relocations, tfn_reloc);

            if(ctx->class == ELFCLASS32) {
                tf_reloc->type = LINKER_RELOCATION_TYPE_32_32;
                tfn_reloc->type = LINKER_RELOCATION_TYPE_32_32;
                f_offset += 4;
                fn_offset += 4;
            } else {
                tf_reloc->type = LINKER_RELOCATION_TYPE_64_64;
                tfn_reloc->type = LINKER_RELOCATION_TYPE_64_64;
                f_offset += 8;
                fn_offset += 8;
            }

            str_offset += strlen(test_func_name) - 2 + 1; // remove begining 2 chars and add null

            iter = iter->next(iter);
        }

        iter->destroy(iter);

        res = iter_err;

        if(res == 0) {

            if(ctx->enable_removing_disabled_sections) {
                ctx->direct_relocation_count = 0;
            }

            res = linker_tag_required_section(ctx, test_func_array);

            if(res == 0) {
                res = linker_tag_required_section(ctx, test_functions_names_array);
            }
        }
    }

    return res;
}

void linker_destroy_context(linker_context_t* ctx) {
    if(ctx->direct_relocations) {
        memory_free_ext(ctx->heap, ctx->direct_relocations);
    }

    if(ctx->sections) {
        iterator_t* iter = linkedlist_iterator_create(ctx->sections);

        if(iter != NULL) {
            while(iter->end_of_iterator(iter) != 0) {
                linker_section_t* sec = iter->get_item(iter);

                memory_free_ext(ctx->heap, sec->section_name);
                memory_free_ext(ctx->heap, sec->file_name);
                memory_free_ext(ctx->heap, sec->data);

                linkedlist_destroy_with_data(sec->relocations);

                memory_free_ext(ctx->heap, sec);

                iter = iter->next(iter);
            }

            iter->destroy(iter);

            linkedlist_destroy(ctx->sections);
        }

    }

    if(ctx->symbols) {
        iterator_t* iter = linkedlist_iterator_create(ctx->symbols);

        if(iter != NULL) {
            while(iter->end_of_iterator(iter) != 0) {
                linker_symbol_t* sym = iter->get_item(iter);

                memory_free_ext(ctx->heap, sym->symbol_name);
                memory_free_ext(ctx->heap, sym);

                iter = iter->next(iter);
            }

            iter->destroy(iter);

            linkedlist_destroy(ctx->symbols);
        }

    }

    if(ctx->test_section_flag) {
        linkedlist_destroy(ctx->test_function_names);
    }


    memory_free_ext(ctx->heap, ctx->entry_point);
    //memory_free_ext(ctx->heap, ctx->output);
    //memory_free_ext(ctx->heap, ctx->map_file);
    memory_free_ext(ctx->heap, ctx);
}

int8_t linker_parse_script(linker_context_t* ctx, char_t* linker_script) {
    FILE* ls = fopen(linker_script, "r");


    if(!ls) {
        return -1;
    }

    char_t key[64];
    char_t svalue[64];
    uint64_t ivalue = 0;

    int8_t res = 0;

    while(fscanf(ls, "%64s = ", key) != -1) {
        if(strcmp(key, "entry_point") == 0) {
            if(fscanf(ls, "%64s\n", svalue) != -1) {
                ctx->entry_point = strdup_at_heap(ctx->heap, svalue);
                printf("entry_point setted to %s\n", svalue);
            } else {
                res = -1;
                break;
            }
        }

        if(strcmp(key, "program_start") == 0) {
            if(fscanf(ls, "0x%lx\n", &ivalue) != -1) {
                ctx->start = ivalue;
                printf("program_start setted to %lx\n", ivalue);
            } else {
                res = -1;
                break;
            }
        }

        if(strcmp(key, "stack_size") == 0) {
            if(fscanf(ls, "0x%lx\n", &ivalue) != -1) {
                ctx->stack_size = ivalue;
                printf("stack_size setted to %lx\n", ivalue);
            } else {
                res = -1;
                break;
            }
        }

        memory_memclean(key, 64);
        memory_memclean(svalue, 64);
        ivalue = 0;
    }

    fclose(ls);

    printf("context entry_point %s program_start 0x%016lx stack_size  0x%016lx\n", ctx->entry_point, ctx->start, ctx->stack_size);

    return res;
}

int8_t linker_write_efi_output(linker_context_t* ctx) {
    linker_symbol_t* ep_sym = linker_lookup_symbol(ctx, ctx->entry_point, 0);

    if(ep_sym == NULL) {
        printf("entry point not found %s\n", ctx->entry_point);
        return -1;
    }

    linker_section_t* ep_sec = linker_get_section_by_id(ctx, ep_sym->section_id);

    uint32_t ep_addr = (uint32_t)ep_sym->value + (uint32_t)ep_sec->offset;

    FILE* fp = fopen(ctx->output, "w" );

    if(fp == NULL) {
        return -1;
    }

    uint16_t dosstub_magic = EFI_IMAGE_DOSSTUB_HEADER_MAGIC;

    fwrite(&dosstub_magic, 1, sizeof(uint16_t), fp);

    fseek(fp, EFI_IMAGE_DOSSTUB_EFI_IMAGE_OFFSET_LOCATION, SEEK_SET);

    uint32_t ep_offset = EFI_IMAGE_DOSSTUB_LENGTH;

    fwrite(&ep_offset, 1, sizeof(uint32_t), fp);

    efi_image_header_t p_hdr = {};
    p_hdr.magic = EFI_IMAGE_HEADER_MAGIC;
    p_hdr.machine = EFI_IMAGE_MACHINE_AMD64;
    p_hdr.number_of_sections = 5;
    p_hdr.size_of_optional_header = sizeof(efi_image_optional_header_t);
    p_hdr.characteristics = EFI_IMAGE_CHARACTERISTISCS;


    efi_image_optional_header_t opt_hdr = {};
    opt_hdr.magic = EFI_IMAGE_OPTIONAL_HEADER_MAGIC;
    opt_hdr.address_of_entrypoint = ep_addr;
    opt_hdr.base_of_code = 0x1000;
    opt_hdr.section_alignment = 0x1000;
    opt_hdr.file_alignment = 0x20;
    opt_hdr.subsystem = 10;
    opt_hdr.number_of_rva_nd_sizes = 16;

    efi_image_section_header_t sec_hdrs[5] = {};

    strcpy(".text", sec_hdrs[0].name);
    sec_hdrs[0].characteristics = 0x68000020;

    strcpy(".rodata", sec_hdrs[1].name);
    sec_hdrs[1].characteristics = 0x48000040;

    strcpy(".data", sec_hdrs[2].name);
    sec_hdrs[2].characteristics = 0xC8000040;

    strcpy(".bss", sec_hdrs[3].name);
    sec_hdrs[3].characteristics = 0xC8000080;
    sec_hdrs[3].virtual_address = ctx->section_locations[LINKER_SECTION_TYPE_BSS].section_start;
    sec_hdrs[3].virtual_size = ctx->section_locations[LINKER_SECTION_TYPE_BSS].section_size;
    opt_hdr.size_of_uninitialized_data = sec_hdrs[3].virtual_size;

    strcpy(".reloc", sec_hdrs[4].name);
    sec_hdrs[4].characteristics = 0x48000040;
    sec_hdrs[4].virtual_address = sec_hdrs[3].virtual_address + sec_hdrs[3].virtual_size;

    if(sec_hdrs[4].virtual_address % 0x1000) {
        sec_hdrs[4].virtual_address += 0x1000 - (sec_hdrs[4].virtual_address % 0x1000);
    }

    int64_t w_offset = EFI_IMAGE_DOSSTUB_LENGTH + sizeof(efi_image_header_t) + sizeof(efi_image_optional_header_t) + sizeof(sec_hdrs);

    if(w_offset % opt_hdr.file_alignment) {
        w_offset += 0x20 - (w_offset % opt_hdr.file_alignment);
    }

    opt_hdr.size_of_headers = w_offset;

    fseek(fp, w_offset, SEEK_SET);

    boolean_t text_offset_setted = false;
    boolean_t data_offset_setted = false;
    boolean_t rodata_offset_setted = false;

    linkedlist_t tmp_relocs = linkedlist_create_list_with_heap(ctx->heap);


    iterator_t* iter = linkedlist_iterator_create(ctx->sections);

    if(iter == NULL) {
        return -1;
    }

    while(iter->end_of_iterator(iter) != 0) {
        linker_section_t* sec  = iter->get_item(iter);

        if(sec->required == 0 && ctx->enable_removing_disabled_sections != 0) {
            iter = iter->next(iter);
            continue;
        }

        if(sec->type <= LINKER_SECTION_TYPE_RODATA) {

            if(sec->type == LINKER_SECTION_TYPE_TEXT) {
                sec_hdrs[0].virtual_size += sec->size + sec->padding;
                sec_hdrs[0].size_of_raw_data += sec->size + sec->padding;
                opt_hdr.size_of_code += sec->size + sec->padding;

                if(!text_offset_setted) {
                    sec_hdrs[0].virtual_address = sec->offset + sec->padding;
                    sec_hdrs[0].pointer_to_raw_data = sec->offset + sec->padding;
                    text_offset_setted = true;
                }
            }

            if(sec->type == LINKER_SECTION_TYPE_RODATA) {
                sec_hdrs[1].virtual_size += sec->size + sec->padding;
                sec_hdrs[1].size_of_raw_data += sec->size + sec->padding;
                opt_hdr.size_of_initialized_data += sec->size + sec->padding;

                if(!rodata_offset_setted) {
                    sec_hdrs[1].virtual_address = sec->offset;
                    sec_hdrs[1].pointer_to_raw_data = sec->offset;
                    rodata_offset_setted = true;
                }
            }

            if(sec->type == LINKER_SECTION_TYPE_DATA) {
                sec_hdrs[2].virtual_size += sec->size + sec->padding;
                sec_hdrs[2].size_of_raw_data += sec->size + sec->padding;
                opt_hdr.size_of_initialized_data += sec->size + sec->padding;

                if(!data_offset_setted) {
                    sec_hdrs[2].virtual_address = sec->offset;
                    sec_hdrs[2].pointer_to_raw_data = sec->offset;
                    data_offset_setted = true;
                }
            }


            fseek (fp, sec->offset, SEEK_SET);

            fwrite(sec->data, 1, sec->size, fp);


            if(sec->relocations) {

                iterator_t* relocs_iter = linkedlist_iterator_create(sec->relocations);

                if(relocs_iter == NULL) {
                    printf("cannot create iterator for relocations\n");
                    return -1;
                }

                while(relocs_iter->end_of_iterator(relocs_iter) != 0) {
                    linker_relocation_t* reloc = relocs_iter->get_item(relocs_iter);

                    fseek (fp, reloc->offset, SEEK_SET);

                    linker_symbol_t* target_sym = linker_get_symbol_by_id(ctx, reloc->symbol_id);

                    if(target_sym == NULL) {
                        print_error("unknown target sym");
                        return -1;
                    }

                    linker_section_t* target_sec = linker_get_section_by_id(ctx, target_sym->section_id);


                    if(target_sym == NULL) {
                        print_error("unknown target sec");
                        return -1;
                    }

                    if(reloc->type == LINKER_RELOCATION_TYPE_64_32) {

                        uint32_t addr = (uint32_t)ctx->start;
                        addr += (uint32_t)target_sym->value + (uint32_t)target_sec->offset + (uint32_t)reloc->addend;

                        efi_image_relocation_entry_t* re = memory_malloc_ext(ctx->heap, sizeof(efi_image_relocation_entry_t), 0);
                        re->page_rva = reloc->offset & ~(0x1000 - 1);
                        re->block_size = sizeof(efi_image_relocation_entry_t);
                        re->type = 0x3;
                        re->offset = reloc->offset & (0x1000 - 1);

                        linkedlist_list_insert(tmp_relocs, re);

                        fwrite(&addr, 1, 4, fp);
                    } else if(reloc->type == LINKER_RELOCATION_TYPE_64_32S) {

                        int32_t addr = (int32_t)ctx->start;
                        addr += (int32_t)target_sym->value + (int32_t)target_sec->offset + (uint32_t)reloc->addend;

                        efi_image_relocation_entry_t* re = memory_malloc_ext(ctx->heap, sizeof(efi_image_relocation_entry_t), 0);
                        re->page_rva = reloc->offset & ~(0x1000 - 1);
                        re->block_size = sizeof(efi_image_relocation_entry_t);
                        re->type = 0x3;
                        re->offset = reloc->offset & (0x1000 - 1);

                        linkedlist_list_insert(tmp_relocs, re);

                        fwrite(&addr, 1, 4, fp);
                    }  else if(reloc->type == LINKER_RELOCATION_TYPE_64_64) {

                        uint64_t addr = ctx->start;
                        addr += target_sym->value + target_sec->offset + reloc->addend;

                        efi_image_relocation_entry_t* re = memory_malloc_ext(ctx->heap, sizeof(efi_image_relocation_entry_t), 0);
                        re->page_rva = reloc->offset & ~(0x1000 - 1);
                        re->block_size = sizeof(efi_image_relocation_entry_t);
                        re->type = 0xa;
                        re->offset = reloc->offset & (0x1000 - 1);

                        linkedlist_list_insert(tmp_relocs, re);

                        fwrite(&addr, 1, 8, fp);
                    }  else if(reloc->type == LINKER_RELOCATION_TYPE_64_PC32) {

                        uint32_t addr = (uint32_t)target_sym->value + (uint32_t)target_sec->offset + (uint32_t)reloc->addend  - (uint32_t)(reloc->offset);

                        fwrite(&addr, 1, 4, fp);
                    } else{
                        print_error("unknown reloc type");
                        return -1;
                    }

                    relocs_iter = relocs_iter->next(relocs_iter);
                }

                relocs_iter->destroy(relocs_iter);

            }
        }

        fflush(fp);

        iter = iter->next(iter);
    }

    iter->destroy(iter);


    opt_hdr.base_relocation_table.virtual_address = sec_hdrs[4].virtual_address;
    opt_hdr.base_relocation_table.size = sizeof(efi_image_relocation_entry_t) * linkedlist_size(tmp_relocs);
    sec_hdrs[4].virtual_size = opt_hdr.base_relocation_table.size;
    sec_hdrs[4].size_of_raw_data = opt_hdr.base_relocation_table.size;
    sec_hdrs[4].pointer_to_raw_data = ftell(fp);

    if(sec_hdrs[4].pointer_to_raw_data % opt_hdr.file_alignment) {
        sec_hdrs[4].pointer_to_raw_data += opt_hdr.file_alignment - (sec_hdrs[4].pointer_to_raw_data % opt_hdr.file_alignment);
    }

    fseek(fp, sec_hdrs[4].pointer_to_raw_data, SEEK_SET);

    iter = linkedlist_iterator_create(tmp_relocs);

    while(iter->end_of_iterator(iter) != 0) {
        efi_image_relocation_entry_t* re = iter->get_item(iter);

        fwrite(re, 1, sizeof(efi_image_relocation_entry_t), fp);

        iter = iter->next(iter);
    }

    iter->destroy(iter);


    linkedlist_destroy_with_data(tmp_relocs);

    opt_hdr.size_of_image = sec_hdrs[4].virtual_address;

    uint64_t soi = sec_hdrs[4].virtual_size;

    if(soi % 0x1000) {
        soi += 0x1000 - (soi % 0x1000);
    }

    opt_hdr.size_of_image += soi;

    fseek(fp, EFI_IMAGE_DOSSTUB_LENGTH, SEEK_SET);
    fwrite(&p_hdr, 1, sizeof(efi_image_header_t), fp);
    fwrite(&opt_hdr, 1, sizeof(efi_image_optional_header_t), fp);
    fwrite(&sec_hdrs, 1, sizeof(sec_hdrs), fp);


    fclose(fp);

    return 0;
}


int8_t linker_write_output(linker_context_t* ctx) {
    linker_symbol_t* ep_sym = linker_lookup_symbol(ctx, ctx->entry_point, 0);

    if(ep_sym == NULL) {
        printf("entry point not found %s\n", ctx->entry_point);
        return -1;
    }

    if(ctx->class == ELFCLASS64) {
        ctx->direct_relocations = memory_malloc_ext(ctx->heap, sizeof(linker_direct_relocation_t) * ctx->direct_relocation_count, 0x0);

        if(ctx->direct_relocations == NULL && ctx->direct_relocation_count != 0) {
            printf("cannot malloc relocation list reloc count 0x%llx\n", ctx->direct_relocation_count);
            return -1;
        }
    }

    FILE* fp = fopen(ctx->output, "w" );

    if(fp == NULL) {
        printf("cannot open output file\n");
        return -1;
    }

    FILE* map_fp = NULL;

    if(ctx->map_file) {

        map_fp = fopen(ctx->map_file, "w" );

        if(map_fp == NULL) {
            printf("cannot open map file\n");
            return -1;
        }
    }

    iterator_t* iter = linkedlist_iterator_create(ctx->sections);

    if(iter == NULL) {
        printf("cannot create section iter\n");
        return -1;
    }

    uint64_t dr_index = 0;

    int8_t res = 0;

    while(iter->end_of_iterator(iter) != 0) {
        linker_section_t* sec  = iter->get_item(iter);

        if(sec->required == 0 && ctx->enable_removing_disabled_sections != 0) {
            iter = iter->next(iter);
            continue;
        }

        if(map_fp) {
            fprintf(map_fp, "%016lx %s secid %llx\n", ctx->start + sec->offset, sec->section_name, sec->id);
            fflush(map_fp);
        }

        if(sec->type <= LINKER_SECTION_TYPE_RODATA) {

            fseek (fp, sec->offset, SEEK_SET);

            fwrite(sec->data, 1, sec->size, fp);

            if(sec->relocations) {

                iterator_t* relocs_iter = linkedlist_iterator_create(sec->relocations);

                if(relocs_iter == NULL) {
                    printf("cannot create iterator for relocations\n");
                    res = -1;
                    break;
                }

                while(relocs_iter->end_of_iterator(relocs_iter) != 0) {
                    linker_relocation_t* reloc = relocs_iter->get_item(relocs_iter);

                    fseek (fp, reloc->offset, SEEK_SET);

                    linker_symbol_t* target_sym = linker_get_symbol_by_id(ctx, reloc->symbol_id);

                    if(target_sym == NULL) {
                        print_error("unknown target sym");
                        return -1;
                    }

                    linker_section_t* target_sec = linker_get_section_by_id(ctx, target_sym->section_id);


                    if(target_sym == NULL) {
                        print_error("unknown target sec");
                        return -1;
                    }

                    if(map_fp) {
                        fprintf(map_fp, "%016lx     reference symbol %s@%s ( symid %llx ) at ", ctx->start + reloc->offset, target_sym->symbol_name, target_sec->section_name, reloc->symbol_id);
                    }

                    if(reloc->type == LINKER_RELOCATION_TYPE_64_32) {
                        ctx->direct_relocations[dr_index].section_type = sec->type;
                        ctx->direct_relocations[dr_index].relocation_type = reloc->type;
                        ctx->direct_relocations[dr_index].offset = reloc->offset;
                        ctx->direct_relocations[dr_index].addend = target_sym->value + target_sec->offset + reloc->addend;
                        dr_index++;

                        uint32_t addr = (uint32_t)ctx->start;
                        addr += (uint32_t)target_sym->value + (uint32_t)target_sec->offset + (uint32_t)reloc->addend;

                        if(map_fp) {
                            fprintf(map_fp, "%08x ", addr);
                        }

                        fwrite(&addr, 1, 4, fp);
                    } else if(reloc->type == LINKER_RELOCATION_TYPE_64_32S) {
                        ctx->direct_relocations[dr_index].section_type = sec->type;
                        ctx->direct_relocations[dr_index].relocation_type = reloc->type;
                        ctx->direct_relocations[dr_index].offset = reloc->offset;
                        ctx->direct_relocations[dr_index].addend = target_sym->value + target_sec->offset + reloc->addend;
                        dr_index++;

                        int32_t addr = (int32_t)ctx->start;
                        addr += (int32_t)target_sym->value + (int32_t)target_sec->offset + (uint32_t)reloc->addend;

                        if(map_fp) {
                            fprintf(map_fp, "%08x ", addr);
                        }

                        fwrite(&addr, 1, 4, fp);
                    }  else if(reloc->type == LINKER_RELOCATION_TYPE_64_64) {
                        ctx->direct_relocations[dr_index].section_type = sec->type;
                        ctx->direct_relocations[dr_index].relocation_type = reloc->type;
                        ctx->direct_relocations[dr_index].offset = reloc->offset;
                        ctx->direct_relocations[dr_index].addend = target_sym->value + target_sec->offset + reloc->addend;
                        dr_index++;

                        uint64_t addr = ctx->start;
                        addr += target_sym->value + target_sec->offset + reloc->addend;

                        if(map_fp) {
                            fprintf(map_fp, "%016lx ", addr);
                        }

                        fwrite(&addr, 1, 8, fp);
                    }  else if(reloc->type == LINKER_RELOCATION_TYPE_64_PC32) {
                        ctx->direct_relocations[dr_index].section_type = sec->type;
                        ctx->direct_relocations[dr_index].relocation_type = reloc->type;
                        ctx->direct_relocations[dr_index].offset = reloc->offset;
                        ctx->direct_relocations[dr_index].addend = target_sym->value + target_sec->offset + reloc->addend - reloc->offset;
                        dr_index++;

                        uint32_t addr = (uint32_t)target_sym->value + (uint32_t)target_sec->offset + (uint32_t)reloc->addend  - (uint32_t)(reloc->offset);


                        if(map_fp) {
                            fprintf(map_fp, "%08x ", ctx->start + reloc->offset + addr + 4);
                        }

                        fwrite(&addr, 1, 4, fp);
                    } else if(reloc->type == LINKER_RELOCATION_TYPE_32_16) {
                        uint16_t addr = (uint16_t)ctx->start;
                        uint16_t addend = *((uint16_t*)(sec->data + reloc->offset - sec->offset));
                        addr += (uint16_t)target_sym->value + (uint16_t)target_sec->offset + (uint16_t)reloc->addend + addend;

                        if(map_fp) {
                            fprintf(map_fp, "%08x ", addr);
                        }

                        fwrite(&addr, 1, 2, fp);
                    } else if(reloc->type == LINKER_RELOCATION_TYPE_32_32) {
                        uint32_t addr = (uint32_t)ctx->start;
                        uint32_t addend = *((uint32_t*)(sec->data + reloc->offset - sec->offset));
                        addr += (uint32_t)target_sym->value + (uint32_t)target_sec->offset + (uint32_t)reloc->addend + addend;

                        if(map_fp) {
                            fprintf(map_fp, "%08x ", addr);
                        }

                        fwrite(&addr, 1, 4, fp);
                    } else if(reloc->type == LINKER_RELOCATION_TYPE_32_PC16) {
                        int16_t addend = *((uint16_t*)(sec->data + reloc->offset - sec->offset));
                        uint16_t addr = (uint16_t)target_sym->value + (uint16_t)target_sec->offset + (uint16_t)reloc->addend  - (uint16_t)(reloc->offset) + addend;


                        if(map_fp) {
                            fprintf(map_fp, "%08x ", ctx->start + reloc->offset + addr + 2);
                        }

                        fwrite(&addr, 1, 2, fp);
                    } else if(reloc->type == LINKER_RELOCATION_TYPE_32_PC32) {
                        int32_t addend = *((uint32_t*)(sec->data + reloc->offset - sec->offset));
                        uint32_t addr = (uint32_t)target_sym->value + (uint32_t)target_sec->offset + (uint32_t)reloc->addend  - (uint32_t)(reloc->offset) + addend;


                        if(map_fp) {
                            fprintf(map_fp, "%08x ", ctx->start + reloc->offset + addr + 4);
                        }

                        fwrite(&addr, 1, 4, fp);
                    } else{
                        print_error("unknown reloc type");
                        return -1;
                    }

                    if(map_fp) {
                        fprintf(map_fp, "with reloc type %i\n", reloc->type);
                    }

                    relocs_iter = relocs_iter->next(relocs_iter);
                }

                relocs_iter->destroy(relocs_iter);

            }
        }

        fflush(fp);

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    if(res != 0) {
        return res;
    }

    fseek (fp, 0, SEEK_END);

    uint64_t file_size = ftell(fp);

    if(ctx->class == ELFCLASS64) {
        fseek (fp, 0, SEEK_SET);

        uint8_t jmp = 0xe9;

        fwrite(&jmp, 1, 1, fp);

        linker_section_t* ep_sec = linker_get_section_by_id(ctx, ep_sym->section_id);

        uint32_t addr = (uint32_t)ep_sym->value + (uint32_t)ep_sec->offset - (uint32_t)(1 + 4);

        printf("entry point address 0x%x\n", addr);

        fwrite(&addr, 4, 1, fp);

        file_size = ctx->section_locations[LINKER_SECTION_TYPE_RELOCATION_TABLE].section_start;

        printf("reloc table start %lx count: %lx ", file_size, ctx->direct_relocation_count);

        fseek (fp, file_size, SEEK_SET);

        fwrite(ctx->direct_relocations, 1, sizeof(linker_direct_relocation_t) * ctx->direct_relocation_count, fp);

        fseek (fp, 0, SEEK_END);

        file_size = ftell(fp);

        printf(" end %lx\n", file_size);
    }

    if(file_size % 512) {
        uint64_t rem = 512 - (file_size % 512);
        uint8_t* pad = memory_malloc_ext(ctx->heap, rem, 0x0);
        fwrite(pad, 1, rem, fp);
        memory_free_ext(ctx->heap, pad);
    }

    if(ctx->class == ELFCLASS64) {

        fseek (fp, 0, SEEK_END);

        file_size = ftell(fp);

        fseek (fp, 0x10, SEEK_SET);

        fwrite(&file_size, 1, sizeof(file_size), fp);

        fseek (fp, 0x18, SEEK_SET);

        fwrite(&ctx->section_locations[LINKER_SECTION_TYPE_RELOCATION_TABLE].section_start, 1, sizeof(uint64_t), fp);

        fseek (fp, 0x20, SEEK_SET);

        fwrite(&ctx->direct_relocation_count, 1, sizeof(uint64_t), fp);

        for(uint8_t i = 0; i < LINKER_SECTION_TYPE_NR_SECTIONS; i++) {
            fwrite(&ctx->section_locations[i], 1, sizeof(linker_section_locations_t), fp);
        }
    }

    if(ctx->boot_flag) {
        fseek(fp, 0x1FE, SEEK_SET);
        uint16_t flag = 0xaa55;
        fwrite(&flag, 1, 2, fp);
    }

    fclose(fp);

    if(map_fp) {
        fclose(map_fp);
    }

    return 0;
}

int8_t linker_section_offset_comparator(const void* sec1, const void* sec2) {
    int64_t offset1 = ((linker_section_t*)sec1)->offset;
    int64_t offset2 = ((linker_section_t*)sec2)->offset;

    if(offset1 == offset2) {
        return 0;
    } else if(offset1 < offset2) {
        return -1;
    }

    return 1;
}

int8_t linker_sort_sections_by_offset(linker_context_t* ctx) {
    linkedlist_t sorted_sections = linkedlist_create_sortedlist_with_heap(ctx->heap, linker_section_offset_comparator);

    iterator_t* iter = linkedlist_iterator_create(ctx->sections);

    if(iter == NULL) {
        return -1;
    }

    while(iter->end_of_iterator(iter) != 0) {
        linker_section_t* sec = iter->get_item(iter);

        linkedlist_sortedlist_insert(sorted_sections, sec);

        iter->delete_item(iter);

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    linkedlist_destroy(ctx->sections);

    ctx->sections = sorted_sections;

    return 0;
}

linker_symbol_t* linker_lookup_symbol(linker_context_t* ctx, char_t* symbol_name, uint64_t section_id){
    if(ctx == NULL) {
        return NULL;
    }

    iterator_t* iter = linkedlist_iterator_create(ctx->symbols);

    if(iter == NULL) {
        return NULL;
    }

    linker_symbol_t* res = NULL;

    while(iter->end_of_iterator(iter) != 0) {
        linker_symbol_t* sym = iter->get_item(iter);

        if(sym->scope == LINKER_SYMBOL_SCOPE_LOCAL) {
            if(strcmp(symbol_name, sym->symbol_name) == 0 && sym->section_id == section_id) {
                res = sym;
                break;
            }
        } else {
            if(strcmp(symbol_name, sym->symbol_name) == 0) {
                res = sym;
                break;
            }
        }

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    return res;
}

linker_symbol_t* linker_get_symbol_by_id(linker_context_t* ctx, uint64_t id) {
    if(ctx == NULL) {
        return NULL;
    }

    iterator_t* iter = linkedlist_iterator_create(ctx->symbols);

    if(iter == NULL) {
        return NULL;
    }

    linker_symbol_t* res = NULL;

    while(iter->end_of_iterator(iter) != 0) {
        linker_symbol_t* sym = iter->get_item(iter);

        if(sym->id == id) {
            res = sym;
            break;
        }

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    return res;
}

linker_section_t* linker_get_section_by_id(linker_context_t* ctx, uint64_t id) {
    if(ctx == NULL) {
        return NULL;
    }

    iterator_t* iter = linkedlist_iterator_create(ctx->sections);

    if(iter == NULL) {
        return NULL;
    }

    linker_section_t* res = NULL;

    while(iter->end_of_iterator(iter) != 0) {
        linker_section_t* sec = iter->get_item(iter);

        if(sec->id == id) {
            res = sec;
            break;
        }

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    return res;
}

uint64_t linker_get_section_id(linker_context_t* ctx, char_t* file_name, char_t* section_name) {
    uint64_t id = 0;

    if(section_name == NULL || file_name == NULL) {
        return id;
    }

    if(ctx == NULL) {
        return id;
    }

    iterator_t* iter = linkedlist_iterator_create(ctx->sections);

    if(iter == NULL) {
        return id;
    }

    while(iter->end_of_iterator(iter) != 0) {
        linker_section_t* sec  = iter->get_item(iter);

        if(strcmp(file_name, sec->file_name) == 0 && strcmp(section_name, sec->section_name) == 0 ) {
            id = sec->id;
            break;
        }

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    return id;
}

void linker_print_symbols(linker_context_t* ctx) {
    if(ctx == NULL) {
        return;
    }

    iterator_t* iter = linkedlist_iterator_create(ctx->symbols);

    if(iter == NULL) {
        return;
    }

    while(iter->end_of_iterator(iter) != 0) {
        linker_symbol_t* sym = iter->get_item(iter);


        if(sym->required == 0 && ctx->enable_removing_disabled_sections != 0) {
            iter = iter->next(iter);
            continue;
        }

        printf("symbol id: %08x type: %i scope %i sectionid: %016llx value: %016llx size: %016llx name: %s\n", sym->id, sym->type, sym->scope, sym->section_id, sym->value, sym->size, sym->symbol_name);

        iter = iter->next(iter);
    }

    iter->destroy(iter);
}

void linker_print_sections(linker_context_t* ctx) {
    if(ctx == NULL) {
        return;
    }

    iterator_t* iter = linkedlist_iterator_create(ctx->sections);

    if(iter == NULL) {
        return;
    }

    while(iter->end_of_iterator(iter) != 0) {
        linker_section_t* sec = iter->get_item(iter);


        if(sec->required == 0 && ctx->enable_removing_disabled_sections != 0) {
            iter = iter->next(iter);
            continue;
        }

        printf("section id: %016x type: %i offset: %016lx size: %016lx align: %02x name: %s\n", sec->id, sec->type, sec->offset, sec->size, sec->align, sec->section_name);

        iter = iter->next(iter);
    }

    iter->destroy(iter);
}


void linker_bind_offset_of_section(linker_context_t* ctx, linker_section_type_t type, uint64_t* base_offset) {

    ctx->section_locations[type].section_start = *base_offset;
    ctx->section_locations[type].section_pyhsical_start = *base_offset;

    iterator_t* sec_iter = linkedlist_iterator_create(ctx->sections);

    while(sec_iter->end_of_iterator(sec_iter) != 0) {
        linker_section_t* sec = sec_iter->get_item(sec_iter);

        if(sec->type == type && (sec->required || ctx->enable_removing_disabled_sections == 0)) {
            if(sec->align) {
                if(*base_offset % sec->align) {
                    uint64_t padding = sec->align - (*base_offset % sec->align);
                    sec->padding = padding;
                    *base_offset += padding;
                }
            }

            sec->offset = *base_offset;

            if(sec->relocations != NULL) {
                iterator_t* relocs_iter = linkedlist_iterator_create(sec->relocations);

                while(relocs_iter->end_of_iterator(relocs_iter) != 0) {
                    linker_relocation_t* reloc = relocs_iter->get_item(relocs_iter);

                    reloc->offset += sec->offset;

                    relocs_iter = relocs_iter->next(relocs_iter);
                }

                relocs_iter->destroy(relocs_iter);
            }

            *base_offset += sec->size;
        }

        sec_iter = sec_iter->next(sec_iter);
    }

    sec_iter->destroy(sec_iter);


    ctx->section_locations[type].section_size = *base_offset - ctx->section_locations[type].section_start;
}


int32_t main(int32_t argc, char** argv) {

    if(argc <= 1) {
        print_error("not enough params");
        return -1;
    }

    argc--;
    argv++;

    char_t* output_file = NULL;
    char_t* entry_point = "___start";
    char_t* map_file = NULL;
    char_t* linker_script = NULL;
    uint8_t print_sections = 0;
    uint8_t print_symbols = 0;
    uint8_t trim = 0;
    uint8_t boot_flag = 0;
    uint8_t test_section_flag = 0;
    boolean_t for_efi = false;

    while(argc > 0) {
        if(strstarts(*argv, "-") != 0) {
            break;
        }


        if(strcmp(*argv, "-o") == 0) {
            argc--;
            argv++;

            if(argc) {
                output_file = *argv;

                argc--;
                argv++;
                continue;
            } else {
                print_error("argument error");
                print_error("LINKER FAILED");

                return -1;
            }
        }

        if(strcmp(*argv, "-e") == 0) {
            argc--;
            argv++;

            if(argc) {
                entry_point = strdup(*argv);

                argc--;
                argv++;
                continue;
            } else {
                print_error("argument error");
                print_error("LINKER FAILED");

                return -1;
            }
        }

        if(strcmp(*argv, "-M") == 0) {
            argc--;
            argv++;

            if(argc) {
                map_file = *argv;

                argc--;
                argv++;
                continue;
            } else {
                print_error("argument error");
                print_error("LINKER FAILED");

                return -1;
            }
        }

        if(strcmp(*argv, "-T") == 0) {
            argc--;
            argv++;

            if(argc) {
                linker_script = *argv;

                argc--;
                argv++;
                continue;
            } else {
                print_error("argument error");
                print_error("LINKER FAILED");

                return -1;
            }
        }

        if(strcmp(*argv, "--print-sections") == 0) {
            argc--;
            argv++;

            print_sections = 1;
        }

        if(strcmp(*argv, "--print-symbols") == 0) {
            argc--;
            argv++;

            print_symbols = 1;
        }

        if(strcmp(*argv, "--trim") == 0) {
            argc--;
            argv++;

            trim = 1;
        }

        if(strcmp(*argv, "--boot-flag") == 0) {
            argc--;
            argv++;

            boot_flag = 1;
        }

        if(strcmp(*argv, "--test-section") == 0) {
            argc--;
            argv++;

            test_section_flag = 1;
        }

        if(strcmp(*argv, "--for-efi") == 0) {
            argc--;
            argv++;

            for_efi = true;
        }
    }

    if(output_file == NULL) {
        printf("no output file given exiting\n");
        print_error("LINKER FAILED");

        return -1;
    }

    if(entry_point == NULL) {
        printf("no entry point given exiting\n");
        print_error("LINKER FAILED");

        return -1;
    }



    linker_context_t* ctx = memory_malloc_ext(NULL, sizeof(linker_context_t), 0x0);

    ctx->heap = NULL;
    ctx->entry_point = entry_point;

    if(!for_efi) {
        ctx->start = 0x20000;
    }

    ctx->stack_size = 0x10000;
    ctx->output = output_file;
    ctx->map_file = map_file;
    ctx->symbols = linkedlist_create_list_with_heap(ctx->heap);
    ctx->sections = linkedlist_create_list_with_heap(ctx->heap);
    ctx->enable_removing_disabled_sections = trim;
    ctx->boot_flag = boot_flag;
    ctx->test_section_flag = test_section_flag;
    ctx->for_efi = for_efi;

    if(ctx->test_section_flag) {
        ctx->test_function_names = linkedlist_create_sortedlist_with_heap(ctx->heap, linker_test_function_names_comparator);
    }

    if(linker_script) {
        linker_parse_script(ctx, linker_script);
    }

    uint64_t section_id = 1;
    uint64_t symbol_id = 1;

    linker_section_t* stack_sec = memory_malloc_ext(ctx->heap, sizeof(linker_section_t), 0x0);
    stack_sec->id = section_id++;
    stack_sec->file_name = NULL;
    stack_sec->section_name = strdup_at_heap(ctx->heap, ".stack");
    stack_sec->size = ctx->stack_size;

    if(ctx->class == ELFCLASS64) {
        stack_sec->align = 0x1000;
    } else {
        stack_sec->align = 0x100;
    }

    stack_sec->type = LINKER_SECTION_TYPE_STACK;
    stack_sec->required = 1;

    linkedlist_list_insert(ctx->sections, stack_sec);

    linker_symbol_t* stack_top_sym =  memory_malloc_ext(ctx->heap, sizeof(linker_symbol_t), 0x0);
    stack_top_sym->id = symbol_id++;
    stack_top_sym->symbol_name = strdup_at_heap(ctx->heap, "__stack_top");
    stack_top_sym->scope = LINKER_SYMBOL_SCOPE_GLOBAL;
    stack_top_sym->type = LINKER_SYMBOL_TYPE_SYMBOL;
    stack_top_sym->value = stack_sec->size - 16;
    stack_top_sym->section_id = stack_sec->id;

    linkedlist_list_insert(ctx->symbols, stack_top_sym);

    linker_section_t* kheap_sec = memory_malloc_ext(ctx->heap, sizeof(linker_section_t), 0x0);
    kheap_sec->id = section_id++;
    kheap_sec->file_name = NULL;
    kheap_sec->section_name = strdup_at_heap(ctx->heap, ".heap");
    kheap_sec->align = 0x100;
    kheap_sec->type = LINKER_SECTION_TYPE_HEAP;
    kheap_sec->required = 1;

    linkedlist_list_insert(ctx->sections, kheap_sec);

    linker_symbol_t* kheap_bottom_sym =  memory_malloc_ext(ctx->heap, sizeof(linker_symbol_t), 0x0);
    kheap_bottom_sym->id = symbol_id++;
    kheap_bottom_sym->symbol_name = strdup_at_heap(ctx->heap, "__kheap_bottom");
    kheap_bottom_sym->scope = LINKER_SYMBOL_SCOPE_GLOBAL;
    kheap_bottom_sym->type = LINKER_SYMBOL_TYPE_SYMBOL;
    kheap_bottom_sym->value = 0;
    kheap_bottom_sym->section_id = kheap_sec->id;

    linkedlist_list_insert(ctx->symbols, kheap_bottom_sym);

    if(ctx->test_section_flag) {
        linker_section_t* test_func_array = memory_malloc_ext(ctx->heap, sizeof(linker_section_t), 0x0);
        test_func_array->id = section_id++;
        test_func_array->file_name = NULL;
        test_func_array->section_name = strdup_at_heap(ctx->heap, ".rodata.__test_functions_array");
        test_func_array->align = 0x10;
        test_func_array->type = LINKER_SECTION_TYPE_RODATA;

        ctx->test_func_array_secid = test_func_array->id;

        linkedlist_list_insert(ctx->sections, test_func_array);

        linker_symbol_t* test_functions_array_start =  memory_malloc_ext(ctx->heap, sizeof(linker_symbol_t), 0x0);
        test_functions_array_start->id = symbol_id++;
        test_functions_array_start->symbol_name = strdup_at_heap(ctx->heap, "__test_functions_array_start");
        test_functions_array_start->scope = LINKER_SYMBOL_SCOPE_GLOBAL;
        test_functions_array_start->type = LINKER_SYMBOL_TYPE_SYMBOL;
        test_functions_array_start->value = 0;
        test_functions_array_start->section_id = test_func_array->id;

        linkedlist_list_insert(ctx->symbols, test_functions_array_start);

        linker_symbol_t* test_functions_array_end =  memory_malloc_ext(ctx->heap, sizeof(linker_symbol_t), 0x0);
        test_functions_array_end->id = symbol_id++;
        test_functions_array_end->symbol_name = strdup_at_heap(ctx->heap, "__test_functions_array_end");
        test_functions_array_end->scope = LINKER_SYMBOL_SCOPE_GLOBAL;
        test_functions_array_end->type = LINKER_SYMBOL_TYPE_SYMBOL;
        test_functions_array_end->value = 0;
        test_functions_array_end->section_id = test_func_array->id;

        linkedlist_list_insert(ctx->symbols, test_functions_array_end);

        linker_section_t* test_func_names_array_str = memory_malloc_ext(ctx->heap, sizeof(linker_section_t), 0x0);
        test_func_names_array_str->id = section_id++;
        test_func_names_array_str->file_name = NULL;
        test_func_names_array_str->section_name = strdup_at_heap(ctx->heap, ".rodata.__test_functions_names.str");
        test_func_names_array_str->align = 0x10;
        test_func_names_array_str->type = LINKER_SECTION_TYPE_RODATA;
        test_func_names_array_str->required = 1;

        ctx->test_func_names_array_str_secid = test_func_names_array_str->id;

        linkedlist_list_insert(ctx->sections, test_func_names_array_str);


        linker_symbol_t* test_functions_names_str =  memory_malloc_ext(ctx->heap, sizeof(linker_symbol_t), 0x0);
        test_functions_names_str->id = symbol_id++;
        test_functions_names_str->symbol_name = strdup_at_heap(ctx->heap, "__test_functions_names_str");
        test_functions_names_str->scope = LINKER_SYMBOL_SCOPE_GLOBAL;
        test_functions_names_str->type = LINKER_SYMBOL_TYPE_SYMBOL;
        test_functions_names_str->value = 0;
        test_functions_names_str->section_id = test_func_names_array_str->id;

        linkedlist_list_insert(ctx->symbols, test_functions_names_str);


        linker_section_t* test_functions_names_array = memory_malloc_ext(ctx->heap, sizeof(linker_section_t), 0x0);
        test_functions_names_array->id = section_id++;
        test_functions_names_array->file_name = NULL;
        test_functions_names_array->section_name = strdup_at_heap(ctx->heap, ".rodata.__test_functions_names_array");
        test_functions_names_array->align = 0x10;
        test_functions_names_array->type = LINKER_SECTION_TYPE_RODATA;

        ctx->test_functions_names_array_secid = test_functions_names_array->id;

        linkedlist_list_insert(ctx->sections, test_functions_names_array);

        linker_symbol_t* test_functions_names =  memory_malloc_ext(ctx->heap, sizeof(linker_symbol_t), 0x0);
        test_functions_names->id = symbol_id++;
        test_functions_names->symbol_name = strdup_at_heap(ctx->heap, "__test_functions_names");
        test_functions_names->scope = LINKER_SYMBOL_SCOPE_GLOBAL;
        test_functions_names->type = LINKER_SYMBOL_TYPE_SYMBOL;
        test_functions_names->value = 0;
        test_functions_names->section_id = test_functions_names_array->id;

        linkedlist_list_insert(ctx->symbols, test_functions_names);


    }


    char_t* file_name;

    while(argc > 0) {
        file_name = *argv;

        ctx->objectfile_ctx.file_name = file_name;

        if(linker_parse_elf_header(ctx, &section_id) != 0) {
            linker_destroy_objectctx(ctx);
            linker_destroy_context(ctx);
            return -1;
        }

        uint64_t symcnt = linker_get_symbol_count(ctx);

        for(uint64_t sym_idx = 1; sym_idx < symcnt; sym_idx++) {
            uint8_t sym_type = linker_get_type_of_symbol(ctx, sym_idx);
            uint64_t sym_shndx = linker_get_section_index_of_symbol(ctx, sym_idx);

            if(sym_type > STT_SECTION) {
                continue;
            }

            char_t* tmp_section_name = linker_get_section_name(ctx, sym_shndx);
            char_t* tmp_symbol_name = linker_get_symbol_name(ctx, sym_idx);

            if(sym_type == STT_SECTION) {
                tmp_symbol_name = tmp_section_name;
            }


            if(tmp_symbol_name == NULL ||
               strcmp(tmp_symbol_name, ".eh_frame") == 0 ||
               strcmp(tmp_symbol_name, ".comment") == 0 ||
               (sym_shndx != 0 && linker_get_section_size(ctx, sym_shndx) == 0)) {
                continue;
            }

            uint64_t tmp_section_id = linker_get_section_id(ctx, file_name, tmp_section_name);

            linker_symbol_t* sym = linker_lookup_symbol(ctx, tmp_symbol_name, tmp_section_id);

            if(sym == NULL) {
                sym = memory_malloc_ext(ctx->heap, sizeof(linker_symbol_t), 0x0);
                sym->id = symbol_id++;
                sym->symbol_name = strdup_at_heap(ctx->heap, tmp_symbol_name);

                if(linker_get_scope_of_symbol(ctx, sym_idx) == STB_GLOBAL) {
                    sym->scope = LINKER_SYMBOL_SCOPE_GLOBAL;
                } else {
                    sym->scope = LINKER_SYMBOL_SCOPE_LOCAL;
                }

                linkedlist_list_insert(ctx->symbols, sym);
            }

            if(tmp_section_id) {
                sym->type = sym_type;
                sym->value = linker_get_value_of_symbol(ctx, sym_idx);
                sym->section_id = tmp_section_id;
                sym->size = linker_get_size_of_symbol(ctx, sym_idx);

                if(strstarts(tmp_section_name, ".text.__test_") == 0 && sym->type == LINKER_SYMBOL_TYPE_FUNCTION) {
                    linkedlist_sortedlist_insert(ctx->test_function_names, sym->symbol_name);
                    ctx->test_func_names_array_str_size += strlen(sym->symbol_name) - 2 + 1; // remove __ at begin and add null
                }
            }
        }


        for(uint16_t sec_idx = 0; sec_idx < ctx->objectfile_ctx.section_count; sec_idx++) {
            uint8_t is_rela = 0;
            if(strstarts(linker_get_section_name(ctx, sec_idx), ".rela.") == 0) {
                is_rela = 1;
            } else if(strstarts(linker_get_section_name(ctx, sec_idx), ".rel.") == 0) {
                is_rela = 0;
            } else {
                continue;
            }

            char_t* tmp_section_name = linker_get_section_name(ctx, sec_idx) + strlen(".rel") + is_rela;

            if(strcmp(tmp_section_name, ".eh_frame") == 0) {
                continue;
            }

            uint64_t tmp_section_id = linker_get_section_id(ctx, file_name, tmp_section_name);

            if(tmp_section_id == 0) {
                print_error("unknown section");
                printf("%s %s\n", linker_get_section_name(ctx, sec_idx), tmp_section_name);
                linker_destroy_objectctx(ctx);
                linker_destroy_context(ctx);
                print_error("LINKER FAILED");

                return -1;
            }

            linker_section_t* sec = linker_get_section_by_id(ctx, tmp_section_id);

            if(sec == NULL) {
                print_error("unknown section");
                printf("%s %s\n", linker_get_section_name(ctx, sec_idx), tmp_section_name);
                linker_destroy_objectctx(ctx);
                linker_destroy_context(ctx);
                print_error("LINKER FAILED");

                return -1;
            }

            sec->relocations = linkedlist_create_list_with_heap(ctx->heap);

            if(sec->relocations == NULL) {
                print_error("cannot create relocations list");
                linker_destroy_objectctx(ctx);
                linker_destroy_context(ctx);
                print_error("LINKER FAILED");

                return -1;
            }

            uint64_t relocsec_off = linker_get_section_offset(ctx, sec_idx);
            uint64_t relocsec_size = linker_get_section_size(ctx, sec_idx);

            ctx->objectfile_ctx.relocs = memory_malloc_ext(ctx->heap, relocsec_size, 0x0);

            if( ctx->objectfile_ctx.relocs  == NULL) {
                print_error("cannot create relocs buffer");
                linker_destroy_objectctx(ctx);
                linker_destroy_context(ctx);
                print_error("LINKER FAILED");

                return -1;
            }

            fseek(ctx->objectfile_ctx.file, relocsec_off, SEEK_SET);
            fread(ctx->objectfile_ctx.relocs, relocsec_size, 1, ctx->objectfile_ctx.file);

            uint32_t reloc_cnt = relocsec_size / sizeof(elf64_rela_t);

            if(ctx->objectfile_ctx.type == ELFCLASS32) {
                if(is_rela) {
                    reloc_cnt = relocsec_size / sizeof(elf32_rela_t);
                } else {
                    reloc_cnt = relocsec_size / sizeof(elf32_rel_t);
                }
            } else {
                if(is_rela) {
                    reloc_cnt = relocsec_size / sizeof(elf64_rela_t);
                } else {
                    reloc_cnt = relocsec_size / sizeof(elf64_rel_t);
                }
            }

            for(uint32_t reloc_idx = 0; reloc_idx < reloc_cnt; reloc_idx++) {

                uint64_t sym_idx = linker_get_relocation_symbol_index(ctx, reloc_idx, is_rela);

                uint8_t sym_type = linker_get_type_of_symbol(ctx, sym_idx);
                uint64_t sym_shndx = linker_get_section_index_of_symbol(ctx, sym_idx);

                char_t* tmp_section_name = linker_get_section_name(ctx, sym_shndx);
                char_t* tmp_symbol_name = linker_get_symbol_name(ctx, sym_idx);

                if(sym_type == STT_SECTION) {
                    tmp_symbol_name = tmp_section_name;
                }

                tmp_section_id = linker_get_section_id(ctx, file_name, tmp_section_name);

                linker_symbol_t* sym = linker_lookup_symbol(ctx, tmp_symbol_name, tmp_section_id);

                if(sym == NULL) {
                    print_error("unknown sym at reloc");
                    printf("%s %s %s\n", file_name, tmp_section_name, tmp_symbol_name );
                    linker_print_symbols(ctx);
                    linker_destroy_context(ctx);
                    print_error("LINKER FAILED");

                    return -1;
                }

                linker_relocation_t* reloc = memory_malloc_ext(ctx->heap, sizeof(linker_relocation_t), 0x0);

                reloc->symbol_id = sym->id;
                reloc->offset = linker_get_relocation_symbol_offset(ctx, reloc_idx, is_rela);
                reloc->addend = linker_get_relocation_symbol_addend(ctx, reloc_idx, is_rela);

                uint64_t reloc_type = linker_get_relocation_symbol_type(ctx, reloc_idx, is_rela);

                if(ctx->objectfile_ctx.type == ELFCLASS32) {
                    switch (reloc_type) {
                    case R_386_16:
                        reloc->type = LINKER_RELOCATION_TYPE_32_16;
                        ctx->direct_relocation_count++;
                        break;
                    case R_386_32:
                        reloc->type = LINKER_RELOCATION_TYPE_32_32;
                        ctx->direct_relocation_count++;
                        break;
                    case R_386_PC16:
                        reloc->type = LINKER_RELOCATION_TYPE_32_PC16;
                        break;
                    case R_386_PC32:
                        reloc->type = LINKER_RELOCATION_TYPE_32_PC32;
                        break;
                    default:
                        print_error("unknown reloc type");
                        printf("%i %lx %lx %lx\n", reloc_type, reloc->symbol_id, reloc->offset, reloc->addend);
                        linker_destroy_objectctx(ctx);
                        linker_destroy_context(ctx);
                        print_error("LINKER FAILED");

                        return -1;
                    }
                } else {
                    switch (reloc_type) {
                    case R_X86_64_32:
                        reloc->type = LINKER_RELOCATION_TYPE_64_32;
                        ctx->direct_relocation_count++;
                        break;
                    case R_X86_64_32S:
                        reloc->type = LINKER_RELOCATION_TYPE_64_32S;
                        ctx->direct_relocation_count++;
                        break;
                    case R_X86_64_64:
                        reloc->type = LINKER_RELOCATION_TYPE_64_64;
                        ctx->direct_relocation_count++;
                        break;
                    case R_X86_64_PC32:
                    case R_X86_64_PLT32:
                        reloc->type = LINKER_RELOCATION_TYPE_64_PC32;
                        ctx->direct_relocation_count++;
                        break;
                    default:
                        print_error("unknown reloc type");
                        printf("%i %lx %lx %lx\n", reloc_type, reloc->symbol_id, reloc->offset, reloc->addend);
                        linker_destroy_objectctx(ctx);
                        linker_destroy_context(ctx);
                        print_error("LINKER FAILED");

                        return -1;
                    }
                }



                linkedlist_list_insert(sec->relocations, reloc);
            }

            memory_free_ext(ctx->heap,  ctx->objectfile_ctx.relocs);
        }

        linker_destroy_objectctx(ctx);

        argc--;
        argv++;
    }

    if(ctx->test_section_flag && linker_relocate_test_functions(ctx) != 0) {
        print_error("error at relocation test functions");
        linker_destroy_context(ctx);
        print_error("LINKER FAILED");

        return -1;
    }

    if(linker_tag_required_sections(ctx) != 0) {
        print_error("error at tagging required sections");
        linker_destroy_context(ctx);
        print_error("LINKER FAILED");

        return -1;
    }

    uint64_t output_offset_base = 0;

    if(ctx->class == ELFCLASS64 && !ctx->for_efi) {
        output_offset_base = 0x100;
    }

    if(ctx->class == ELFCLASS64 && ctx->for_efi) {
        output_offset_base = 0x1000;
    }

    linker_bind_offset_of_section(ctx, LINKER_SECTION_TYPE_TEXT, &output_offset_base);

    if(output_offset_base % 0x1000) {
        output_offset_base +=  0x1000 - (output_offset_base %  0x1000);
    }

    linker_bind_offset_of_section(ctx, LINKER_SECTION_TYPE_RODATA, &output_offset_base);

    if(output_offset_base % 0x1000) {
        output_offset_base +=  0x1000 - (output_offset_base %  0x1000);
    }

    linker_bind_offset_of_section(ctx, LINKER_SECTION_TYPE_DATA, &output_offset_base);

    if(ctx->class == ELFCLASS64 && !ctx->for_efi) {
        if(output_offset_base % 0x1000) {
            output_offset_base += 0x1000 - (output_offset_base % 0x1000);
        }

        printf("reloc table should start at 0x%lx ", output_offset_base);
        ctx->section_locations[LINKER_SECTION_TYPE_RELOCATION_TABLE].section_start = output_offset_base;
        ctx->section_locations[LINKER_SECTION_TYPE_RELOCATION_TABLE].section_pyhsical_start = output_offset_base;

        output_offset_base += sizeof(linker_direct_relocation_t) * ctx->direct_relocation_count;

        printf("ends at 0x%lx\n", output_offset_base);

        ctx->section_locations[LINKER_SECTION_TYPE_RELOCATION_TABLE].section_size = output_offset_base -  ctx->section_locations[LINKER_SECTION_TYPE_RELOCATION_TABLE].section_start;

    }

    if(output_offset_base % 0x1000) {
        output_offset_base +=  0x1000 - (output_offset_base %  0x1000);
    }


    printf("section .bss should start at 0x%lx\n", output_offset_base);

    linker_bind_offset_of_section(ctx, LINKER_SECTION_TYPE_BSS, &output_offset_base);

    if(!ctx->for_efi) {
        if(output_offset_base % 0x1000) {
            output_offset_base +=  0x1000 - (output_offset_base %  0x1000);
        }


        linker_bind_offset_of_section(ctx, LINKER_SECTION_TYPE_STACK, &output_offset_base);

        if(output_offset_base % 0x1000) {
            output_offset_base +=  0x1000 - (output_offset_base %  0x1000);
        }

        linker_bind_offset_of_section(ctx, LINKER_SECTION_TYPE_HEAP, &output_offset_base);
    }

    if(linker_sort_sections_by_offset(ctx) != 0) {
        printf("cannot sort sections\n");
        linker_destroy_context(ctx);
        print_error("LINKER FAILED");

        return -1;
    }

    if(print_sections) {
        linker_print_sections(ctx);
    }

    if(print_symbols) {
        linker_print_symbols(ctx);
    }

    if(!ctx->for_efi) {
        if(linker_write_output(ctx) != 0) {
            print_error("error at writing output");
            linker_destroy_context(ctx);
            print_error("LINKER FAILED");

            return -1;
        }
    } else {
        if(linker_write_efi_output(ctx) != 0) {
            print_error("error at writing efi output");
            linker_destroy_context(ctx);
            print_error("LINKER FAILED");

            return -1;
        }
    }

    linker_destroy_context(ctx);

    print_success("LINKER OK");
    return 0;
}
