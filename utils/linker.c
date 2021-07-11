#define RAMSIZE 0x8000000
#include "setup.h"
#include "elf64.h"
#include <utils.h>
#include <strings.h>
#include <linkedlist.h>
#include <linker.h>

typedef enum {
	LINKER_SYMBOL_TYPE_UNDEF,
	LINKER_SYMBOL_TYPE_FUNCTION,
	LINKER_SYMBOL_TYPE_OBJECT,
	LINKER_SYMBOL_TYPE_SECTION,
	LINKER_SYMBOL_TYPE_SYMBOL,
}linker_symbol_type_t;

typedef enum {
	LINKER_SYMBOL_SCOPE_LOCAL,
	LINKER_SYMBOL_SCOPE_GLOBAL,
} linker_symbol_scope_t;

typedef struct {
	uint64_t id;
	linker_section_type_t type;
	char_t* file_name;
	char_t* section_name;
	uint64_t offset;
	uint64_t size;
	uint64_t align;
	uint8_t* data;
	linkedlist_t relocations;
	uint8_t required;
} linker_section_t;

typedef struct {
	uint64_t id;
	linker_symbol_type_t type;
	linker_symbol_scope_t scope;
	uint64_t section_id;
	char_t* symbol_name;
	uint64_t value;
} linker_symbol_t;

typedef struct {
	linker_relocation_type_t type;
	uint64_t symbol_id;
	uint64_t offset;
	int64_t addend;
} linker_relocation_t;

typedef struct {
	memory_heap_t* heap;
	char_t* entry_point;
	uint64_t start;
	uint64_t stack_size;
	char_t* output;
	char_t* map_file;
	linkedlist_t symbols;
	linkedlist_t sections;
	uint64_t direct_relocation_count;
	linker_direct_relocation_t* direct_relocations;
	linker_section_locations_t section_locations[LINKER_SECTION_TYPE_NR_SECTIONS];
} linker_context_t;

linker_symbol_t* linker_lookup_symbol(linker_context_t* ctx, char_t* symbol_name, uint64_t section_id);
linker_symbol_t* linker_get_symbol_by_id(linker_context_t* ctx, uint64_t id);
void linker_print_symbols(linker_context_t* ctx);
void linker_bind_offset_of_section(linker_context_t* ctx, linker_section_type_t type, uint64_t* base_offset);
int8_t linker_section_offset_comparator(const void* sym1, const void* sym2);
int8_t linker_sort_sections_by_offset(linker_context_t* ctx);
uint64_t linker_get_section_id(linker_context_t* ctx, char_t* file_name, char_t* section_name);
linker_section_t* linker_get_section_by_id(linker_context_t* ctx, uint64_t id);
int8_t linker_write_output(linker_context_t* ctx);
int8_t linker_parse_script(linker_context_t* ctx, char_t* linker_script);
void linker_destroy_context(linker_context_t* ctx);
int8_t linker_tag_required_sections(linker_context_t* ctx);
int8_t linker_tag_required_section(linker_context_t* ctx, linker_section_t* section);



int8_t linker_tag_required_sections(linker_context_t* ctx) {
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

				if(reloc->type == LINKER_RELOCATION_TYPE_32 ||
				   reloc->type == LINKER_RELOCATION_TYPE_32S ||
				   reloc->type == LINKER_RELOCATION_TYPE_64) {
					ctx->direct_relocation_count++;
				}

				linker_symbol_t* sym = linker_get_symbol_by_id(ctx, reloc->symbol_id);

				if(sym == NULL) {
					printf("cannot find required symbol %li at for section %s reloc list\n", reloc->symbol_id, section->section_name);
					res = -1;
					break;
				}

				linker_section_t* sub_section = linker_get_section_by_id(ctx, sym->section_id);

				res = linker_tag_required_section(ctx, sub_section);

				if(res != 0) {
					break;
				}

				iter = iter->next(iter);
			}

			iter->destroy(iter);

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


	memory_free_ext(ctx->heap, ctx->entry_point);
	memory_free_ext(ctx->heap, ctx->output);
	memory_free_ext(ctx->heap, ctx->map_file);
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


int8_t linker_write_output(linker_context_t* ctx) {
	linker_symbol_t* ep_sym = linker_lookup_symbol(ctx, ctx->entry_point, 0);

	if(ep_sym == NULL) {
		printf("entry point not found %s\n", ctx->entry_point);
		return -1;
	}

	ctx->direct_relocations = memory_malloc_ext(ctx->heap, sizeof(linker_direct_relocation_t) * ctx->direct_relocation_count, 0x0);

	if(ctx->direct_relocations == NULL) {
		return -1;
	}

	FILE* fp = fopen(ctx->output, "w" );

	if(fp == NULL) {
		return -1;
	}

	FILE* map_fp = NULL;

	if(ctx->map_file) {

		map_fp = fopen(ctx->map_file, "w" );

		if(map_fp == NULL) {
			return -1;
		}
	}

	iterator_t* iter = linkedlist_iterator_create(ctx->sections);

	if(iter == NULL) {
		return -1;
	}

	uint64_t dr_index = 0;

	while(iter->end_of_iterator(iter) != 0) {
		linker_section_t* sec  = iter->get_item(iter);

		if(sec->required == 0) {
			iter = iter->next(iter);
			continue;
		}

		if(map_fp) {
			fprintf(map_fp, "%016lx %s\n", ctx->start + sec->offset, sec->section_name);
		}

		if(sec->type <= LINKER_SECTION_TYPE_RODATA) {

			fseek (fp, sec->offset, SEEK_SET);

			fwrite(sec->data, 1, sec->size, fp);

			if(sec->relocations) {

				iterator_t* relocs_iter = linkedlist_iterator_create(sec->relocations);

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
						fprintf(map_fp, "%016lx     reference symbol %s@%s at ", ctx->start + reloc->offset, target_sym->symbol_name, target_sec->section_name);
					}

					if(reloc->type == LINKER_RELOCATION_TYPE_32) {
						ctx->direct_relocations[dr_index].type = reloc->type;
						ctx->direct_relocations[dr_index].offset = reloc->offset;
						ctx->direct_relocations[dr_index].addend = target_sym->value + target_sec->offset + reloc->addend;
						dr_index++;

						uint32_t addr = (uint32_t)ctx->start;
						addr += (uint32_t)target_sym->value + (uint32_t)target_sec->offset + (uint32_t)reloc->addend;

						if(map_fp) {
							fprintf(map_fp, "%08x\n", addr);
						}

						fwrite(&addr, 1, 4, fp);
					} else if(reloc->type == LINKER_RELOCATION_TYPE_32S) {
						ctx->direct_relocations[dr_index].type = reloc->type;
						ctx->direct_relocations[dr_index].offset = reloc->offset;
						ctx->direct_relocations[dr_index].addend = target_sym->value + target_sec->offset + reloc->addend;
						dr_index++;

						int32_t addr = (int32_t)ctx->start;
						addr += (int32_t)target_sym->value + (int32_t)target_sec->offset + (uint32_t)reloc->addend;

						if(map_fp) {
							fprintf(map_fp, "%08x\n", addr);
						}

						fwrite(&addr, 1, 4, fp);
					}  else if(reloc->type == LINKER_RELOCATION_TYPE_64) {
						ctx->direct_relocations[dr_index].type = reloc->type;
						ctx->direct_relocations[dr_index].offset = reloc->offset;
						ctx->direct_relocations[dr_index].addend = target_sym->value + target_sec->offset + reloc->addend;
						dr_index++;

						uint64_t addr = ctx->start;
						addr += target_sym->value + target_sec->offset + reloc->addend;

						if(map_fp) {
							fprintf(map_fp, "%016lx\n", addr);
						}

						fwrite(&addr, 1, 8, fp);
					}  else if(reloc->type == LINKER_RELOCATION_TYPE_PC32) {
						uint32_t addr = (uint32_t)target_sym->value + (uint32_t)target_sec->offset + (uint32_t)reloc->addend  - (uint32_t)(reloc->offset);


						if(map_fp) {
							fprintf(map_fp, "%08x\n", ctx->start + reloc->offset + addr + 4);
						}

						fwrite(&addr, 1, 4, fp);
					} else {
						print_error("unknown reloc type");
						return -1;
					}

					relocs_iter = relocs_iter->next(relocs_iter);
				}

				relocs_iter->destroy(relocs_iter);

			}
		}

		iter = iter->next(iter);
	}

	iter->destroy(iter);

	fseek (fp, 0, SEEK_SET);

	uint8_t jmp = 0xe9;

	fwrite(&jmp, 1, 1, fp);

	linker_section_t* ep_sec = linker_get_section_by_id(ctx, ep_sym->section_id);

	uint32_t addr = (uint32_t)ep_sym->value + (uint32_t)ep_sec->offset - (uint32_t)(1 + 4);

	fwrite(&addr, 1, 4, fp);

	fseek (fp, 0, SEEK_END);

	uint64_t file_size = ftell(fp);

	if(file_size % 16) {
		file_size += 16 - (file_size % 16);
	}

	printf("reloc table start %lx count: %lx\n", file_size, ctx->direct_relocation_count);

	fseek (fp, file_size, SEEK_SET);

	fwrite(ctx->direct_relocations, 1, sizeof(linker_direct_relocation_t) * ctx->direct_relocation_count, fp);

	uint64_t reloc_locaction = file_size;


	fseek (fp, 0, SEEK_END);

	file_size = ftell(fp);

	if(file_size % 512) {
		uint8_t pad = 0x00;
		uint64_t rem = 512 - (file_size % 512);
		fwrite(&pad, rem, 1, fp);
	}

	fseek (fp, 0, SEEK_END);

	file_size = ftell(fp);

	fseek (fp, 0x10, SEEK_SET);

	fwrite(&file_size, 1, sizeof(file_size), fp);

	fseek (fp, 0x18, SEEK_SET);

	fwrite(&reloc_locaction, 1, sizeof(reloc_locaction), fp);

	fseek (fp, 0x20, SEEK_SET);

	fwrite(&ctx->direct_relocation_count, 1, sizeof(uint64_t), fp);

	for(uint8_t i = 0; i < LINKER_SECTION_TYPE_STACK; i++) {
		fwrite(&ctx->section_locations[i], 1, sizeof(linker_section_locations_t), fp);
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

		printf("symbol id: %08x type: %i scope %i sectionid: %016lx value: %016lx name: %s\n", sym->id, sym->type, sym->scope, sym->section_id, sym->value, sym->symbol_name);

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

		printf("section id: %016x type: %i offset: %016lx size: %016lx align: %02x name: %s\n", sec->id, sec->type, sec->offset, sec->size, sec->align, sec->section_name);

		iter = iter->next(iter);
	}

	iter->destroy(iter);
}


void linker_bind_offset_of_section(linker_context_t* ctx, linker_section_type_t type, uint64_t* base_offset) {

	ctx->section_locations[type].section_start = *base_offset;

	iterator_t* sec_iter = linkedlist_iterator_create(ctx->sections);

	while(sec_iter->end_of_iterator(sec_iter) != 0) {
		linker_section_t* sec = sec_iter->get_item(sec_iter);

		if(sec->type == type && sec->required) {
			if(sec->align) {
				if(*base_offset % sec->align) {
					uint64_t padding = sec->align - (*base_offset % sec->align);
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
	setup_ram();

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
				return -1;
			}
		}

		if(strcmp(*argv, "-e") == 0) {
			argc--;
			argv++;

			if(argc) {
				entry_point = *argv;

				argc--;
				argv++;
				continue;
			} else {
				print_error("argument error");
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

			print_sections = 1;
		}
	}

	if(output_file == NULL) {
		printf("no output file given exiting\n");
		return -1;
	}

	if(entry_point == NULL) {
		printf("no entry point given exiting\n");
		return -1;
	}



	linker_context_t* ctx = memory_malloc_ext(NULL, sizeof(linker_context_t), 0x0);

	ctx->heap = NULL;
	ctx->entry_point = entry_point;
	ctx->start = 0x20000;
	ctx->stack_size = 0x10000;
	ctx->output = output_file;
	ctx->map_file = map_file;
	ctx->symbols = linkedlist_create_list_with_heap(ctx->heap);
	ctx->sections = linkedlist_create_list_with_heap(ctx->heap);

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
	stack_sec->align = 0x1000;
	stack_sec->type = LINKER_SECTION_TYPE_STACK;
	stack_sec->required = 1;

	linkedlist_list_insert(ctx->sections, stack_sec);

	linker_symbol_t* stack_top_sym =  memory_malloc_ext(ctx->heap, sizeof(linker_symbol_t), 0x0);
	stack_top_sym->id = symbol_id++;
	stack_top_sym->symbol_name = strdup_at_heap(ctx->heap, "__stack_top");
	stack_top_sym->scope = LINKER_SYMBOL_SCOPE_GLOBAL;
	stack_top_sym->type = LINKER_SYMBOL_TYPE_SYMBOL;
	stack_top_sym->value = stack_sec->size;
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


	char_t* file_name;

	while(argc > 0) {
		file_name = *argv;
		FILE* obj_file = fopen(file_name, "r");

		elf64_hdr_t obj_hdr;

		fread(&obj_hdr, sizeof(elf64_hdr_t), 1, obj_file);

		elf64_shdr_t* sections = memory_malloc_ext(ctx->heap, obj_hdr.e_shentsize * obj_hdr.e_shnum, 0x0);
		fseek(obj_file, obj_hdr.e_shoff, SEEK_SET);
		fread(sections, obj_hdr.e_shentsize * obj_hdr.e_shnum, 1, obj_file);

		elf64_shdr_t shstrtabsec = sections[obj_hdr.e_shstrndx];

		char_t* shstrtab = memory_malloc_ext(ctx->heap, shstrtabsec.sh_size, 0x0);
		fseek(obj_file, shstrtabsec.sh_offset, SEEK_SET);
		fread(shstrtab, shstrtabsec.sh_size, 1, obj_file);

		elf64_shdr_t* strtabsec = NULL;
		elf64_shdr_t* symtabsec = NULL;

		for(uint16_t sec_idx = 0; sec_idx < obj_hdr.e_shnum; sec_idx++) {
			if(strcmp(".strtab", shstrtab + sections[sec_idx].sh_name) == 0) {
				strtabsec = &sections[sec_idx];
			}

			if(sections[sec_idx].sh_type == SHT_SYMTAB) {
				symtabsec = &sections[sec_idx];
			}

			if(sections[sec_idx].sh_size &&   (
					 strstarts(shstrtab + sections[sec_idx].sh_name, ".text") == 0 ||
					 strstarts(shstrtab + sections[sec_idx].sh_name, ".data") == 0 ||
					 strstarts(shstrtab + sections[sec_idx].sh_name, ".rodata") == 0 ||
					 strstarts(shstrtab + sections[sec_idx].sh_name, ".bss") == 0
					 )) {
				linker_section_t* sec = memory_malloc_ext(ctx->heap, sizeof(linker_section_t), 0x0);
				sec->id = section_id++;
				sec->file_name = strdup_at_heap(ctx->heap, file_name);
				sec->section_name = strdup_at_heap(ctx->heap, shstrtab + sections[sec_idx].sh_name);
				sec->size = sections[sec_idx].sh_size;
				sec->align = sections[sec_idx].sh_addralign;

				if(strstarts(shstrtab + sections[sec_idx].sh_name, ".text") == 0) {
					sec->type = LINKER_SECTION_TYPE_TEXT;
				} else if(strstarts(shstrtab + sections[sec_idx].sh_name, ".data") == 0) {
					sec->type = LINKER_SECTION_TYPE_DATA;
				} else if(strstarts(shstrtab + sections[sec_idx].sh_name, ".rodata") == 0) {
					sec->type = LINKER_SECTION_TYPE_RODATA;
				} else if(strstarts(shstrtab + sections[sec_idx].sh_name, ".bss") == 0) {
					sec->type = LINKER_SECTION_TYPE_BSS;
				}

				if(strstarts(shstrtab + sections[sec_idx].sh_name, ".bss") != 0) {
					sec->data = memory_malloc_ext(ctx->heap, sec->size, 0x0);

					fseek(obj_file, sections[sec_idx].sh_offset, SEEK_SET);
					fread(sec->data, sec->size, 1, obj_file);
				}

				linkedlist_list_insert(ctx->sections, sec);
			}

		}

		if(strtabsec == NULL) {
			print_error("string table not found");
			linker_destroy_context(ctx);
			return -1;
		}

		char_t* strtab = memory_malloc_ext(ctx->heap, strtabsec->sh_size, 0x0);
		fseek(obj_file, strtabsec->sh_offset, SEEK_SET);
		fread(strtab, strtabsec->sh_size, 1, obj_file);

		if(symtabsec == NULL) {
			print_error("symbol table not found");
			linker_destroy_context(ctx);
			return -1;
		}


		elf64_sym_t* symtab = memory_malloc_ext(ctx->heap, symtabsec->sh_size, 0x0);
		fseek(obj_file, symtabsec->sh_offset, SEEK_SET);
		fread(symtab, symtabsec->sh_size, 1, obj_file);
		uint64_t symcnt = symtabsec->sh_size / sizeof(elf64_sym_t);

		for(uint64_t sym_idx = 1; sym_idx < symcnt; sym_idx++) {
			if(symtab[sym_idx].st_type > STT_SECTION) {
				continue;
			}

			char_t* tmp_section_name = shstrtab + sections[symtab[sym_idx].st_shndx].sh_name;
			char_t* tmp_symbol_name = strtab + symtab[sym_idx].st_name;

			if(symtab[sym_idx].st_type == STT_SECTION) {
				tmp_symbol_name = tmp_section_name;
			}


			if(tmp_symbol_name == NULL ||
			   strcmp(tmp_symbol_name, ".eh_frame") == 0 ||
			   strcmp(tmp_symbol_name, ".comment") == 0 ||
			   (symtab[sym_idx].st_shndx != 0 && sections[symtab[sym_idx].st_shndx].sh_size == 0)) {
				continue;
			}

			uint64_t tmp_section_id = linker_get_section_id(ctx, file_name, tmp_section_name);

			linker_symbol_t* sym = linker_lookup_symbol(ctx, tmp_symbol_name, tmp_section_id);

			if(sym == NULL) {
				sym = memory_malloc_ext(ctx->heap, sizeof(linker_symbol_t), 0x0);
				sym->id = symbol_id++;
				sym->symbol_name = strdup_at_heap(ctx->heap, tmp_symbol_name);

				if(symtab[sym_idx].st_scope == STB_GLOBAL) {
					sym->scope = LINKER_SYMBOL_SCOPE_GLOBAL;
				} else {
					sym->scope = LINKER_SYMBOL_SCOPE_LOCAL;
				}

				linkedlist_list_insert(ctx->symbols, sym);
			}

			if(tmp_section_id) {
				sym->type = symtab[sym_idx].st_type;
				sym->value = symtab[sym_idx].st_value;
				sym->section_id = tmp_section_id;
			}
		}


		for(uint16_t sec_idx = 0; sec_idx < obj_hdr.e_shnum; sec_idx++) {
			if(strstarts(shstrtab + sections[sec_idx].sh_name, ".rela") != 0) {
				continue;
			}

			char_t* tmp_section_name = shstrtab + sections[sec_idx].sh_name + strlen(".rela");

			if(strcmp(tmp_section_name, ".eh_frame") == 0) {
				continue;
			}

			uint64_t tmp_section_id = linker_get_section_id(ctx, file_name, tmp_section_name);

			if(tmp_section_id == 0) {
				print_error("unknown section");
				printf("%s %s\n", shstrtab + sections[sec_idx].sh_name, tmp_section_name);
				linker_destroy_context(ctx);
				return -1;
			}

			linker_section_t* sec = linker_get_section_by_id(ctx, tmp_section_id);

			if(sec == NULL) {
				print_error("unknown section");
				printf("%s %s\n", shstrtab + sections[sec_idx].sh_name, tmp_section_name);
				linker_destroy_context(ctx);
				return -1;
			}

			sec->relocations = linkedlist_create_list_with_heap(ctx->heap);

			elf64_shdr_t* relocsec = &sections[sec_idx];

			elf64_rela_t* relocs = memory_malloc_ext(ctx->heap, relocsec->sh_size, 0x0);

			if(relocs == NULL) {
				return -1;
			}

			fseek(obj_file, relocsec->sh_offset, SEEK_SET);
			fread(relocs, relocsec->sh_size, 1, obj_file);

			uint32_t reloc_cnt = relocsec->sh_size / sizeof(elf64_rela_t);

			for(uint32_t reloc_idx = 0; reloc_idx < reloc_cnt; reloc_idx++) {

				uint32_t sym_idx = relocs[reloc_idx].r_symindx;

				char_t* tmp_section_name = shstrtab + sections[symtab[sym_idx].st_shndx].sh_name;
				char_t* tmp_symbol_name = strtab + symtab[sym_idx].st_name;

				if(symtab[sym_idx].st_type == STT_SECTION) {
					tmp_symbol_name = tmp_section_name;
				}

				tmp_section_id = linker_get_section_id(ctx, file_name, tmp_section_name);

				linker_symbol_t* sym = linker_lookup_symbol(ctx, tmp_symbol_name, tmp_section_id);

				if(sym == NULL) {
					print_error("unknown sym at reloc");
					printf("%s %s %s\n", file_name, tmp_section_name, tmp_symbol_name );
					linker_print_symbols(ctx);
					linker_destroy_context(ctx);
					return -1;
				}

				linker_relocation_t* reloc = memory_malloc_ext(ctx->heap, sizeof(linker_relocation_t), 0x0);

				reloc->symbol_id = sym->id;
				reloc->offset = relocs[reloc_idx].r_offset;
				reloc->addend = relocs[reloc_idx].r_addend;

				switch (relocs[reloc_idx].r_symtype) {
				case R_X86_64_32:
					reloc->type = LINKER_RELOCATION_TYPE_32;
					break;
				case R_X86_64_32S:
					reloc->type = LINKER_RELOCATION_TYPE_32S;
					break;
				case R_X86_64_64:
					reloc->type = LINKER_RELOCATION_TYPE_64;
					break;
				case R_X86_64_PC32:
				case R_X86_64_PLT32:
					reloc->type = LINKER_RELOCATION_TYPE_PC32;
					break;
				default:
					print_error("unknown reloc type");
					printf("%i %lx %lx %lx\n", relocs[reloc_idx].r_symtype, reloc->symbol_id, reloc->offset, reloc->addend);
					return -1;
				}

				linkedlist_list_insert(sec->relocations, reloc);
			}

			memory_free_ext(ctx->heap, relocs);
		}

		memory_free_ext(ctx->heap, symtab);
		memory_free_ext(ctx->heap, strtab);
		memory_free_ext(ctx->heap, shstrtab);
		memory_free_ext(ctx->heap, sections);

		fclose(obj_file);

		argc--;
		argv++;
	}

	if(linker_tag_required_sections(ctx) != 0) {
		print_error("error at tagging required sections");
		linker_destroy_context(ctx);
		return -1;

	}

	uint64_t output_offset_base = 0x100;

	linker_bind_offset_of_section(ctx, LINKER_SECTION_TYPE_TEXT, &output_offset_base);
	linker_bind_offset_of_section(ctx, LINKER_SECTION_TYPE_DATA, &output_offset_base);
	linker_bind_offset_of_section(ctx, LINKER_SECTION_TYPE_RODATA, &output_offset_base);

	if(output_offset_base % 8) {
		output_offset_base += 8 - (output_offset_base % 8);
	}

	output_offset_base += sizeof(linker_direct_relocation_t) * ctx->direct_relocation_count;

	if(output_offset_base % 0x100) {
		output_offset_base +=  0x100 - (output_offset_base %  0x100);
	}

	linker_bind_offset_of_section(ctx, LINKER_SECTION_TYPE_BSS, &output_offset_base);

	linker_bind_offset_of_section(ctx, LINKER_SECTION_TYPE_STACK, &output_offset_base);

	output_offset_base += 1;

	linker_bind_offset_of_section(ctx, LINKER_SECTION_TYPE_HEAP, &output_offset_base);

	if(linker_sort_sections_by_offset(ctx) != 0) {
		printf("cannot sort sections\n");
		linker_destroy_context(ctx);
		return -1;
	}

	if(print_sections) {
		linker_print_sections(ctx);
	}

	if(print_symbols) {
		linker_print_symbols(ctx);
	}

	if(linker_write_output(ctx) != 0) {
		print_error("error at writing output");
		linker_destroy_context(ctx);
		return -1;
	}

	linker_destroy_context(ctx);

	print_success("OK");
	return 0;
}
