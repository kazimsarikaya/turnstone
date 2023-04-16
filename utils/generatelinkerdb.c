/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#define RAMSIZE (128 << 20)
#include "setup.h"
#include <utils.h>
#include <buffer.h>
#include <data.h>
#include <sha2.h>
#include <bplustree.h>
#include <map.h>
#include <xxhash.h>
#include <tosdb/tosdb.h>
#include <strings.h>
#include <bloomfilter.h>
#include <math.h>
#include <zpack.h>
#include <binarysearch.h>
#include <tokenizer.h>
#include <set.h>
#include "elf64.h"
#include <linker.h>

#define LINKERDB_CAP (32 << 20)

typedef struct linkerdb_t {
    uint64_t         capacity;
    FILE*            db_file;
    int32_t          fd;
    uint8_t*         mmap_res;
    buffer_t         backend_buffer;
    tosdb_backend_t* backend;
    tosdb_t*         tdb;
} linkerdb_t;

int32_t     main(int32_t argc, char_t** argv);
linkerdb_t* linkerdb_open(const char_t* file, uint64_t capacity);
boolean_t   linkerdb_close(linkerdb_t* ldb);
boolean_t   linkerdb_gen_config(linkerdb_t* ldb, const char_t* entry_point, const uint64_t stack_size);
boolean_t   linkerdb_create_tables(linkerdb_t* ldb);
boolean_t   linkerdb_parse_object_file(linkerdb_t*   ldb,
                                       const char_t* filename,
                                       uint64_t*     sec_id,
                                       uint64_t*     sym_id,
                                       uint64_t*     reloc_id);
boolean_t linkerdb_fix_reloc_symbol_section_ids(linkerdb_t* ldb);


linkerdb_t* linkerdb_open(const char_t* file, uint64_t capacity) {
    FILE* fp = fopen(file, "w+");

    if(!fp) {
        print_error("cannot open db file");

        return NULL;
    }

    fseek(fp, capacity - 1, SEEK_SET);
    int8_t zero = 0;
    fwrite(&zero, 1, 1, fp);
    fseek(fp, 0, SEEK_SET);

    int32_t fd = fileno(fp);

    uint8_t* mmap_res = mmap(NULL, capacity, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_SYNC, fd, 0);

    if(mmap_res == (void*)-1) {
        fclose(fp);
        print_error("cannot mmap db file");

        return NULL;
    }

    memory_memclean(mmap_res, capacity);

    buffer_t buf = buffer_encapsulate(mmap_res, capacity);

    if(!buf) {
        munmap(mmap_res, capacity);
        fclose(fp);
        print_error("cannot create buffer");

        return NULL;
    }

    buffer_set_readonly(buf, false);

    tosdb_backend_t* bend = tosdb_backend_memory_from_buffer(buf);

    if(!bend) {
        buffer_set_readonly(buf, true);
        buffer_destroy(buf);
        munmap(mmap_res, capacity);
        fclose(fp);
        print_error("cannot create backend");

        return NULL;
    }

    tosdb_t* tdb = tosdb_new(bend);

    if(!tdb) {
        buffer_set_readonly(buf, true);
        tosdb_backend_close(bend);
        munmap(mmap_res, capacity);
        fclose(fp);
        print_error("cannot create backend");

        return NULL;
    }


    linkerdb_t* ldb = memory_malloc(sizeof(linkerdb_t));

    if(!ldb) {
        tosdb_close(tdb);
        tosdb_free(tdb);
        buffer_set_readonly(buf, true);
        tosdb_backend_close(bend);
        munmap(mmap_res, capacity);
        fclose(fp);
        print_error("cannot create backend");

        return NULL;
    }

    ldb->backend = bend;
    ldb->backend_buffer = buf;
    ldb->capacity = capacity;
    ldb->db_file = fp;
    ldb->fd = fd;
    ldb->mmap_res = mmap_res;
    ldb->tdb = tdb;

    return ldb;
}

boolean_t linkerdb_close(linkerdb_t* ldb) {
    if(!ldb) {
        return false;
    }

    if(!tosdb_close(ldb->tdb)) {
        return false;
    }

    if(!tosdb_free(ldb->tdb)) {
        return false;
    }

    buffer_set_readonly(ldb->backend_buffer, true);

    if(!tosdb_backend_close(ldb->backend)) {
        return false;
    }

    int32_t mss = msync(ldb->mmap_res, ldb->capacity, MS_SYNC);
    if(mss) {
        printf("msync res %i\n", mss);
        print_error("cannot sync contents");
    }

    munmap(ldb->mmap_res, ldb->capacity);

    fclose(ldb->db_file);

    memory_free(ldb);

    return true;
}

boolean_t linkerdb_gen_config(linkerdb_t* ldb, const char_t* entry_point, const uint64_t stack_size) {
    tosdb_t* tdb = ldb->tdb;

    tosdb_database_t* db = tosdb_database_create_or_open(tdb, (char_t*)"system");

    if(!db) {
        return false;
    }

    tosdb_table_t* tbl_config = tosdb_table_create_or_open(db, (char_t*)"config", 1 << 10, 128 << 10, 8);

    if(!tbl_config) {
        return false;
    }

    tosdb_table_column_add(tbl_config, (char_t*)"name", DATA_TYPE_STRING);
    tosdb_table_column_add(tbl_config, (char_t*)"value", DATA_TYPE_INT8_ARRAY);
    tosdb_table_index_create(tbl_config, (char_t*)"name", TOSDB_INDEX_PRIMARY);

    tosdb_record_t* rec = tosdb_table_create_record(tbl_config);
    rec->set_string(rec, "name", "entry_point");
    rec->set_data(rec, "value", DATA_TYPE_INT8_ARRAY, strlen(entry_point), entry_point);
    rec->upsert_record(rec);
    rec->destroy(rec);

    rec = tosdb_table_create_record(tbl_config);
    rec->set_string(rec, "name", "stack_size");
    rec->set_data(rec, "value", DATA_TYPE_INT8_ARRAY, sizeof(uint64_t), &stack_size);
    rec->upsert_record(rec);
    rec->destroy(rec);

    return tosdb_table_close(tbl_config);
}

boolean_t linkerdb_create_tables(linkerdb_t* ldb) {
    tosdb_t* tdb = ldb->tdb;

    tosdb_database_t* db = tosdb_database_create_or_open(tdb, (char_t*)"system");

    if(!db) {
        return false;
    }

    tosdb_table_t* tbl_sections = tosdb_table_create_or_open(db, (char_t*)"sections", 1 << 10, 128 << 10, 8);

    if(!tbl_sections) {
        return false;
    }

    if(!tosdb_table_column_add(tbl_sections, (char_t*)"id", DATA_TYPE_INT64)) {
        return false;
    }

    if(!tosdb_table_column_add(tbl_sections, (char_t*)"name", DATA_TYPE_STRING)) {
        return false;
    }

    if(!tosdb_table_column_add(tbl_sections, (char_t*)"alignment", DATA_TYPE_INT64)) {
        return false;
    }

    if(!tosdb_table_column_add(tbl_sections, (char_t*)"class", DATA_TYPE_INT8)) {
        return false;
    }

    if(!tosdb_table_column_add(tbl_sections, (char_t*)"size", DATA_TYPE_INT64)) {
        return false;
    }

    if(!tosdb_table_column_add(tbl_sections, (char_t*)"type", DATA_TYPE_INT8)) {
        return false;
    }

    if(!tosdb_table_column_add(tbl_sections, (char_t*)"value", DATA_TYPE_INT8_ARRAY)) {
        return false;
    }

    if(!tosdb_table_index_create(tbl_sections, (char_t*)"id", TOSDB_INDEX_PRIMARY)) {
        return false;
    }

    if(!tosdb_table_index_create(tbl_sections, (char_t*)"name", TOSDB_INDEX_UNIQUE)) {
        return false;
    }

    tosdb_table_t* tbl_symbols = tosdb_table_create_or_open(db, (char_t*)"symbols", 1 << 10, 128 << 10, 8);

    if(!tbl_symbols) {
        return false;
    }

    if(!tosdb_table_column_add(tbl_symbols, (char_t*)"id", DATA_TYPE_INT64)) {
        return false;
    }

    if(!tosdb_table_column_add(tbl_symbols, (char_t*)"section_id", DATA_TYPE_INT64)) {
        return false;
    }

    if(!tosdb_table_column_add(tbl_symbols, (char_t*)"name", DATA_TYPE_STRING)) {
        return false;
    }

    if(!tosdb_table_column_add(tbl_symbols, (char_t*)"type", DATA_TYPE_INT8)) {
        return false;
    }

    if(!tosdb_table_column_add(tbl_symbols, (char_t*)"scope", DATA_TYPE_INT8)) {
        return false;
    }

    if(!tosdb_table_column_add(tbl_symbols, (char_t*)"value", DATA_TYPE_INT64)) {
        return false;
    }

    if(!tosdb_table_column_add(tbl_symbols, (char_t*)"size", DATA_TYPE_INT64)) {
        return false;
    }

    if(!tosdb_table_index_create(tbl_symbols, (char_t*)"id", TOSDB_INDEX_PRIMARY)) {
        return false;
    }

    if(!tosdb_table_index_create(tbl_symbols, (char_t*)"name", TOSDB_INDEX_UNIQUE)) {
        return false;
    }

    if(!tosdb_table_index_create(tbl_symbols, (char_t*)"section_id", TOSDB_INDEX_SECONDARY)) {
        return false;
    }

    tosdb_table_t* tbl_relocations = tosdb_table_create_or_open(db, (char_t*)"relocations", 1 << 10, 128 << 10, 8);

    if(!tbl_relocations) {
        return false;
    }

    if(!tosdb_table_column_add(tbl_relocations, (char_t*)"id", DATA_TYPE_INT64)) {
        return false;
    }

    if(!tosdb_table_column_add(tbl_relocations, (char_t*)"section_id", DATA_TYPE_INT64)) {
        return false;
    }

    if(!tosdb_table_column_add(tbl_relocations, (char_t*)"symbol_name", DATA_TYPE_STRING)) {
        return false;
    }

    if(!tosdb_table_column_add(tbl_relocations, (char_t*)"symbol_section_id", DATA_TYPE_INT64)) {
        return false;
    }

    if(!tosdb_table_column_add(tbl_relocations, (char_t*)"type", DATA_TYPE_INT8)) {
        return false;
    }

    if(!tosdb_table_column_add(tbl_relocations, (char_t*)"offset", DATA_TYPE_INT64)) {
        return false;
    }

    if(!tosdb_table_column_add(tbl_relocations, (char_t*)"addend", DATA_TYPE_INT64)) {
        return false;
    }

    if(!tosdb_table_index_create(tbl_relocations, (char_t*)"id", TOSDB_INDEX_PRIMARY)) {
        return false;
    }

    if(!tosdb_table_index_create(tbl_relocations, (char_t*)"section_id", TOSDB_INDEX_SECONDARY)) {
        return false;
    }

    if(!tosdb_table_index_create(tbl_relocations, (char_t*)"symbol_name", TOSDB_INDEX_SECONDARY)) {
        return false;
    }

    if(!tosdb_table_index_create(tbl_relocations, (char_t*)"symbol_section_id", TOSDB_INDEX_SECONDARY)) {
        return false;
    }

    return true;
}

boolean_t linkerdb_parse_object_file(linkerdb_t*   ldb,
                                     const char_t* filename,
                                     uint64_t*     sec_id,
                                     uint64_t*     sym_id,
                                     uint64_t*     reloc_id) {

    FILE* fp = fopen(filename, "r");

    if(!fp) {
        return false;
    }

    elf_indent_t e_indent;

    fread(&e_indent, sizeof(elf_indent_t), 1, fp);
    fseek(fp, 0, SEEK_SET);

    uint8_t e_class = e_indent.class;
    uint16_t e_shnum = 0;
    uint64_t e_shoff = 0;
    uint16_t e_shstrndx = 0;
    uint64_t e_shsize = 0;

    if(e_class == ELFCLASS32) {

        elf32_hdr_t hdr;
        fread(&hdr, sizeof(elf32_hdr_t), 1, fp);

        e_shnum = hdr.e_shnum;
        e_shoff = hdr.e_shoff;
        e_shstrndx = hdr.e_shstrndx;
        e_shsize = sizeof(elf32_shdr_t) * e_shnum;

    } else if(e_class == ELFCLASS64) {

        elf64_hdr_t hdr;
        fread(&hdr, sizeof(elf64_hdr_t), 1, fp);

        e_shnum = hdr.e_shnum;
        e_shoff = hdr.e_shoff;
        e_shstrndx = hdr.e_shstrndx;
        e_shsize = sizeof(elf64_shdr_t) * e_shnum;

    } else {
        printf("unknown file class %i\n", e_indent.class );
        fclose(fp);

        return false;
    }

    if(e_shnum == 0) {
        printf("no section in file %s\n", filename);
        fclose(fp);

        return false;
    }

    uint8_t* sections  = memory_malloc(e_shsize);

    if(!sections) {
        printf("cannot allocate sections for file %s\n", filename);
        fclose(fp);

        return false;
    }

    fseek(fp, e_shoff, SEEK_SET);
    fread(sections, e_shsize, 1, fp);


    uint64_t shstrtab_size = ELF_SECTION_SIZE(e_class, sections, e_shstrndx);
    uint64_t shstrtab_offset = ELF_SECTION_OFFSET(e_class, sections, e_shstrndx);

    char_t* shstrtab = memory_malloc(shstrtab_size);

    if(!shstrtab) {
        memory_free(sections);
        printf("cannot allocate section string table for file %s\n", filename);
        fclose(fp);

        return false;
    }

    fseek(fp, shstrtab_offset, SEEK_SET);
    fread(shstrtab, shstrtab_size, 1, fp);

    int8_t req_sec_found = 0;
    char_t* strtab = NULL;
    uint8_t* symbols = NULL;
    uint64_t symbol_count = 0;

    tosdb_database_t* db_system = tosdb_database_create_or_open(ldb->tdb, (char_t*)"system");
    tosdb_table_t* tbl_sections = tosdb_table_create_or_open(db_system, (char_t*)"sections", 1 << 10, 128 << 10, 8);


    boolean_t error = false;

    uint64_t sec_id_base = *sec_id;
    (*sec_id) += e_shnum;

    for(uint16_t sec_idx = 0; sec_idx < e_shnum; sec_idx++) {
        uint64_t sec_size = ELF_SECTION_SIZE(e_class, sections, sec_idx);
        uint64_t sec_offset = ELF_SECTION_OFFSET(e_class, sections, sec_idx);
        char_t* sec_name = shstrtab + ELF_SECTION_NAME(e_class, sections, sec_idx);

        if(strcmp(".strtab", sec_name) == 0) {
            strtab = memory_malloc(sec_size);

            if(!strtab) {
                error = true;

                break;
            }

            fseek(fp, sec_offset, SEEK_SET);
            fread(strtab, sec_size, 1, fp);
            req_sec_found++;
        }

        if(ELF_SECTION_TYPE(e_class, sections, sec_idx) == SHT_SYMTAB) {
            symbols = memory_malloc(sec_size);

            if(!symbols) {
                error = true;

                break;
            }

            fseek(fp, sec_offset, SEEK_SET);
            fread(symbols, sec_size, 1, fp);
            req_sec_found++;

            symbol_count = ELF_SYMBOL_COUNT(e_class, sec_size);
        }

        uint8_t sec_type = LINKER_SECTION_TYPE_TEXT;

        if(strstarts(sec_name, ".data") == 0) {
            sec_type = LINKER_SECTION_TYPE_DATA;
        } else if(strstarts(sec_name, ".rodata") == 0) {
            sec_type = LINKER_SECTION_TYPE_RODATA;
        } else if(strstarts(sec_name, ".bss") == 0) {
            sec_type = LINKER_SECTION_TYPE_BSS;
        }

        if(sec_size &&   (
               strstarts(sec_name, ".text") == 0 ||
               strstarts(sec_name, ".data") == 0 ||
               strstarts(sec_name, ".rodata") == 0 ||
               strstarts(sec_name, ".bss") == 0
               )) {
            tosdb_record_t* rec = tosdb_table_create_record(tbl_sections);

            if(!rec) {
                print_error("cannot create section record");
                error = true;

                break;
            }

            rec->set_int64(rec, "id", sec_id_base + sec_idx);
            rec->set_string(rec, "name", sec_name);
            rec->set_int64(rec, "alignment", ELF_SECTION_ALIGN(e_class, sections, sec_idx));
            rec->set_int8(rec, "class", e_class);
            rec->set_int64(rec, "size", sec_size);
            rec->set_int8(rec, "type", sec_type);

            if(strstarts(sec_name, ".bss") != 0) {
                uint8_t* sec_data = memory_malloc(sec_size);

                if(!sec_data) {
                    rec->destroy(rec);
                    error = true;

                    break;
                }

                fseek(fp, sec_offset, SEEK_SET);
                fread(sec_data, sec_size, 1, fp);

                rec->set_data(rec, "value", DATA_TYPE_INT8_ARRAY, sec_size, sec_data);

                memory_free(sec_data);

            }

            boolean_t res = rec->upsert_record(rec);
            rec->destroy(rec);

            if(!res) {
                print_error("cannot insert section");
                error = true;

                break;
            }

        }
    }

    if(req_sec_found != 2) {
        print_error("required sections not found");
        error = true;
    }

    if(error) {
        goto close;
    }

    uint64_t sym_id_base = *sym_id;
    (*sym_id) += symbol_count;

    tosdb_table_t* tbl_symbols = tosdb_table_create_or_open(db_system, (char_t*)"symbols", 1 << 10, 128 << 10, 8);

    for(uint16_t sym_idx = 0; sym_idx < symbol_count; sym_idx++) {
        uint8_t sym_type = ELF_SYMBOL_TYPE(e_class, symbols, sym_idx);

        if(sym_type > STT_SECTION) {
            continue;
        }

        uint64_t sym_shndx = ELF_SYMBOL_SHNDX(e_class, symbols, sym_idx);

        if(sym_shndx == 0 || sym_shndx >= SHN_LOPROC) {
            continue;
        }

        char_t* sym_name = strtab + ELF_SYMBOL_NAME(e_class, symbols, sym_idx);
        uint64_t sym_sec_id = sec_id_base + ELF_SYMBOL_SHNDX(e_class, symbols, sym_idx);
        uint8_t sym_scope = ELF_SYMBOL_SCOPE(e_class, symbols, sym_idx);
        uint64_t sym_value = ELF_SYMBOL_VALUE(e_class, symbols, sym_idx);
        uint64_t sym_size = ELF_SYMBOL_SIZE(e_class, symbols, sym_idx);

        if(sym_type == STT_SECTION) {
            sym_name = shstrtab + ELF_SECTION_NAME(e_class, sections, sym_shndx);
        }


        if(sym_sec_id == sec_id_base) {
            print_error("unknown symbol");
            error = true;

            break;
        }

        if(strlen(sym_name) == 0) {
            print_error("symbol name not found");
            error = true;

            break;
        }

        boolean_t free_sym_name = false;

        if(sym_type == STB_LOCAL) {
            sym_name = strcat(shstrtab + ELF_SECTION_NAME(e_class, sections, sym_shndx), sym_name);
            free_sym_name = true;
        }

        tosdb_record_t* rec = tosdb_table_create_record(tbl_symbols);

        rec->set_int64(rec, "id", sym_id_base + sym_idx);
        rec->set_int64(rec, "section_id", sym_sec_id);
        rec->set_string(rec, "name", sym_name);
        rec->set_int8(rec, "type", sym_type);
        rec->set_int8(rec, "scope", sym_scope);
        rec->set_int64(rec, "value", sym_value);
        rec->set_int64(rec, "size", sym_size);

        boolean_t res = rec->upsert_record(rec);
        rec->destroy(rec);

        if(free_sym_name) {
            memory_free(sym_name);
        }

        if(!res) {
            print_error("cannot insert symbol");
            error = true;

            break;
        }

    }

    if(error) {
        goto close;
    }

    tosdb_table_t* tbl_relocations = tosdb_table_create_or_open(db_system, (char_t*)"relocations", 1 << 10, 128 << 10, 8);

    for(uint16_t sec_idx = 0; sec_idx < e_shnum; sec_idx++) {
        uint8_t is_rela = 0;

        char_t* sec_name = shstrtab + ELF_SECTION_NAME(e_class, sections, sec_idx);

        if(strstarts(sec_name, ".rela.") == 0) {
            is_rela = 1;
        } else if(strstarts(sec_name, ".rel.") == 0) {
            is_rela = 0;
        } else {
            continue;
        }

        const char_t* tmp_section_name = sec_name + strlen(".rel") + is_rela;

        if(strcmp(tmp_section_name, ".eh_frame") == 0) {
            continue;
        }

        uint64_t reloc_sec_id = 0;

        for(uint16_t s_sec_idx = 0; s_sec_idx < e_shnum; s_sec_idx++) {
            char_t* s_sec_name = shstrtab + ELF_SECTION_NAME(e_class, sections, s_sec_idx);
            if(strcmp(tmp_section_name, s_sec_name) == 0) {
                reloc_sec_id = sec_id_base + s_sec_idx;
                break;
            }

        }

        if(!reloc_sec_id) {
            print_error("cannot find section of relocations");
            error = true;

            break;
        }

        uint64_t sec_size = ELF_SECTION_SIZE(e_class, sections, sec_idx);
        uint64_t sec_offset = ELF_SECTION_OFFSET(e_class, sections, sec_idx);

        uint8_t* relocs = memory_malloc(sec_size);

        if(!relocs) {
            print_error("cannot create reloc array");
            error = true;

            break;
        }

        fseek(fp, sec_offset, SEEK_SET);
        fread(relocs, sec_size, 1, fp);

        uint64_t reloc_count = ELF_RELOC_COUNT(e_class, is_rela, sec_size);

        for(uint64_t reloc_idx = 0; reloc_idx < reloc_count; reloc_idx++) {

            uint64_t reloc_offset = ELF_RELOC_OFFSET(e_class, is_rela, relocs, reloc_idx);
            uint32_t reloc_type = ELF_RELOC_TYPE(e_class, is_rela, relocs, reloc_idx);
            uint32_t reloc_symidx = ELF_RELOC_SYMIDX(e_class, is_rela, relocs, reloc_idx);
            uint64_t reloc_sym_sec_id = ELF_SYMBOL_SHNDX(e_class, symbols, reloc_symidx);


            char_t* reloc_sym_name = strtab + ELF_SYMBOL_NAME(e_class, symbols, reloc_symidx);
            uint8_t reloc_sym_type = ELF_SYMBOL_TYPE(e_class, symbols, reloc_symidx);

            if(reloc_sym_type == STT_SECTION) {
                reloc_sym_name = shstrtab + ELF_SECTION_NAME(e_class, sections, reloc_sym_sec_id);
            }

            boolean_t free_reloc_sym_name = false;

            if(reloc_sym_type == STB_LOCAL) {
                reloc_sym_name = strcat(shstrtab + ELF_SECTION_NAME(e_class, sections, reloc_sym_sec_id), reloc_sym_name);
                free_reloc_sym_name = true;
            }

            if(strlen(reloc_sym_name) == 0) {
                printf("!!! at file %s at sec %s symbol name of reloc %lli unknown\n", filename, sec_name, reloc_idx);
                print_error("cannot countinue");

                if(free_reloc_sym_name) {
                    memory_free(reloc_sym_name);
                }

                error = true;
                break;
            }

            if(reloc_sym_sec_id) {
                reloc_sym_sec_id += sec_id_base;
            }

            if(e_class == ELFCLASS32) {
                switch (reloc_type) {
                case R_386_16:
                    reloc_type = LINKER_RELOCATION_TYPE_32_16;
                    break;
                case R_386_32:
                    reloc_type = LINKER_RELOCATION_TYPE_32_32;
                    break;
                case R_386_PC16:
                    reloc_type = LINKER_RELOCATION_TYPE_32_PC16;
                    break;
                case R_386_PC32:
                    reloc_type = LINKER_RELOCATION_TYPE_32_PC32;
                    break;
                default:
                    print_error("unknown 32 bit reloc");
                    error = true;
                    break;
                }
            } else {
                switch (reloc_type) {
                case R_X86_64_32:
                    reloc_type = LINKER_RELOCATION_TYPE_64_32;
                    break;
                case R_X86_64_32S:
                    reloc_type = LINKER_RELOCATION_TYPE_64_32S;
                    break;
                case R_X86_64_64:
                    reloc_type = LINKER_RELOCATION_TYPE_64_64;
                    break;
                case R_X86_64_PC32:
                case R_X86_64_PLT32:
                    reloc_type = LINKER_RELOCATION_TYPE_64_PC32;
                    break;
                case R_X86_64_GOT64:
                    reloc_type = LINKER_RELOCATION_TYPE_64_GOT64;
                    break;
                case R_X86_64_GOTOFF64:
                    reloc_type = LINKER_RELOCATION_TYPE_64_GOTOFF64;
                    break;
                case R_X86_64_GOTPC64:
                    reloc_type = LINKER_RELOCATION_TYPE_64_GOTPC64;
                    break;
                default:
                    print_error("unknown 64 bit reloc");
                    error = true;
                    break;
                }
            }

            if(error) {
                print_error("error at reloc type");
                break;
            }

            tosdb_record_t* rec = tosdb_table_create_record(tbl_relocations);

            if(!rec) {
                print_error("cannot create record object");
                error = true;
                break;
            }

            rec->set_int64(rec, "id", *reloc_id);
            rec->set_int64(rec, "section_id", reloc_sec_id);
            rec->set_string(rec, "symbol_name", reloc_sym_name);
            rec->set_int64(rec, "symbol_section_id", reloc_sym_sec_id);
            rec->set_int8(rec, "type", reloc_type);
            rec->set_int64(rec, "offset", reloc_offset);

            if(is_rela) {
                int64_t reloc_addend = ELF_RELOC_ADDEND(e_class, relocs, reloc_idx);

                rec->set_int64(rec, "addend", reloc_addend);
            }


            if(free_reloc_sym_name) {
                memory_free(reloc_sym_name);
            }

            boolean_t res = rec->upsert_record(rec);
            rec->destroy(rec);

            if(!res) {
                print_error("cannot insert reloc");
                error = true;

                break;
            }

            (*reloc_id)++;
        }

        memory_free(relocs);

        if(error) {
            break;
        }
    }

close:
    memory_free(symbols);
    memory_free(strtab);
    memory_free(sections);
    memory_free(shstrtab);

    fclose(fp);

    return !error;
}

boolean_t linkerdb_fix_reloc_symbol_section_ids(linkerdb_t* ldb) {
    tosdb_database_t* db_system = tosdb_database_create_or_open(ldb->tdb, (char_t*)"system");
    tosdb_table_t* tbl_relocations = tosdb_table_create_or_open(db_system, (char_t*)"relocations", 1 << 10, 128 << 10, 8);
    tosdb_table_t* tbl_symbols = tosdb_table_create_or_open(db_system, (char_t*)"symbols", 1 << 10, 128 << 10, 8);

    tosdb_record_t* s_recs_need = tosdb_table_create_record(tbl_relocations);

    if(!s_recs_need) {
        print_error("cannot create search record");

        return false;
    }

    s_recs_need->set_int64(s_recs_need, "symbol_section_id", 0);

    linkedlist_t res_recs = s_recs_need->search_record(s_recs_need);

    if(!res_recs) {
        s_recs_need->destroy(s_recs_need);
        print_error("search records null");

        return false;
    }

    iterator_t* iter = linkedlist_iterator_create(res_recs);

    if(!iter) {
        s_recs_need->destroy(s_recs_need);
        linkedlist_destroy_with_data(s_recs_need);
        print_error("cannot create iterator");

        return false;
    }

    boolean_t error = false;

    while(iter->end_of_iterator(iter) != 0) {
        tosdb_record_t* reloc_rec = (tosdb_record_t*)iter->delete_item(iter);

        if(!reloc_rec) {
            error = true;
            break;
        }

        char_t* sym_name = NULL;

        reloc_rec->get_string(reloc_rec, "symbol_name", &sym_name);

        if(!sym_name) {
            error = true;
            reloc_rec->destroy(reloc_rec);

            break;
        }

        tosdb_record_t* s_sym_rec = tosdb_table_create_record(tbl_symbols);

        if(!s_sym_rec) {
            error = true;
            reloc_rec->destroy(reloc_rec);

            break;
        }

        s_sym_rec->set_string(s_sym_rec, "name", sym_name);

        if(!s_sym_rec->get_record(s_sym_rec)) {
            PRINTLOG(LINKER, LOG_WARNING, "cannot find symbol %s", sym_name);

            memory_free(sym_name);
            s_sym_rec->destroy(s_sym_rec);
            reloc_rec->destroy(reloc_rec);
            iter = iter->next(iter);

            continue;
        }

        memory_free(sym_name);

        uint64_t sec_id = 0;

        if(!s_sym_rec->get_int64(s_sym_rec, "section_id", (int64_t*)&sec_id)) {
            s_sym_rec->destroy(s_sym_rec);
            error = true;
            reloc_rec->destroy(reloc_rec);

            break;
        }

        s_sym_rec->destroy(s_sym_rec);

        reloc_rec->set_int64(reloc_rec, "symbol_section_id", sec_id);

        boolean_t res = reloc_rec->upsert_record(reloc_rec);
        reloc_rec->destroy(reloc_rec);

        if(!res) {
            error = true;

            break;
        }

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    linkedlist_destroy(res_recs);
    s_recs_need->destroy(s_recs_need);

    return !error;
}

int32_t main(int32_t argc, char_t** argv) {

    if(argc <= 1) {
        print_error("not enough params");
        return -1;
    }

    argc--;
    argv++;

    char_t* output_file = NULL;
    const char_t* entry_point = "___kstart64";
    uint64_t stack_size = 0x10000;

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
                print_error("LINKERDB FAILED");

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
                print_error("LINKERDB FAILED");

                return -1;
            }
        }

        if(strcmp(*argv, "-ss") == 0) {
            argc--;
            argv++;

            if(argc) {
                stack_size = atoi(*argv);

                argc--;
                argv++;
                continue;
            } else {
                print_error("argument error");
                print_error("LINKERDB FAILED");

                return -1;
            }
        }
    }

    int32_t exit_code = 0;

    linkerdb_t* ldb = linkerdb_open(output_file, LINKERDB_CAP);

    if(!ldb) {
        print_error("cannot open linkerdb");
        print_error("LINKERDB FAILED");

        exit_code = -1;
        goto close;
    }

    if(!linkerdb_gen_config(ldb, entry_point, stack_size)) {
        print_error("cannot gen config table");

        exit_code = -1;
        goto close;
    }

    if(!linkerdb_create_tables(ldb)) {
        print_error("cannot create tables");

        exit_code = -1;
        goto close;
    }

    uint64_t sec_id = 1;
    uint64_t sym_id = 1;
    uint64_t reloc_id = 1;

    while(argc) {
        if(!linkerdb_parse_object_file(ldb, *argv, &sec_id, &sym_id, &reloc_id)) {
            print_error("cannot parse object file");

            exit_code = -1;
            break;
        }

        argc--;
        argv++;
    }

    if(!linkerdb_fix_reloc_symbol_section_ids(ldb)) {
        print_error("cannot fix relocations missing symbol sections");
        exit_code = -1;
    }

close:
    if(!linkerdb_close(ldb)) {
        print_error("cannot close linkerdb");
        print_error("LINKERDB FAILED");

        exit_code = -1;
    }

    return exit_code;

}

