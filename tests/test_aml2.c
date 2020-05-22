#include "setup.h"
#include <acpi.h>
#include <linkedlist.h>
#include <strings.h>
#include <utils.h>

typedef struct acpi_aml_scope {
	struct acpi_aml_scope* parent;
	char_t* name;
	linkedlist_t members;
}acpi_aml_scope_t;

typedef struct {
	memory_heap_t* heap;
	uint8_t* location;
	int64_t remaining;
	acpi_aml_scope_t* scope;
} acpi_aml_state_t;

int64_t acpi_aml_parse_namestring(acpi_aml_state_t* state, char_t** namestring);
int64_t acpi_aml_parse_package_length(acpi_aml_state_t* state, int64_t* package_length);
int64_t acpi_aml_parse_scope_symbols(acpi_aml_state_t* state, int64_t remaining);
int64_t acpi_aml_parse_field_elements(acpi_aml_state_t* state, int64_t remaining, char_t* region_name);

int64_t acpi_aml_parse_namestring(acpi_aml_state_t* state, char_t** namestring) {
	printf("\nname string\n");

	uint8_t* save_loc = state->location;
	int64_t save_rem = state->remaining;

	size_t strlen = 0;
	int8_t root_char = 0;
	int8_t prefixpath_char = 0;
	uint8_t* tmp_location = state->location;
	uint8_t seg_count = 0;
	uint8_t multi_name = -1;

	if(state->location[0] == '\\') {
		printf("root char\n");
		root_char = 1;
		strlen++;
		state->location++;
		state->remaining--;
	} else if (state->location[0] == '^') {
		printf("prefix path\n");
		for(size_t i = 0;; i++) {
			if(state->location[i] == '^') {
				prefixpath_char++;
				strlen++;
				state->location++;
				state->remaining--;
			} else {
				break;
			}
		}
	}

	if(state->location[0] == 0x00) {
		printf("null name\n");
		seg_count = 0;
		state->location++;
		state->remaining--;
	} else if(state->location[0] == 0x2E) {
		printf("dual name\n");
		seg_count = 2;
		state->location++;
		state->remaining--;
	} else if(state->location[0] == 0x2F) {
		printf("multi name\n");
		state->location++;
		state->remaining--;
		seg_count = state->location[0];
		state->location++;
		state->remaining--;
		multi_name = 0;
	} else {
		printf("single name\n");
		seg_count = 1;
	}
	strlen = strlen + seg_count * 4;

	*namestring = memory_malloc_ext(state->heap, sizeof(char_t) * strlen + 1, 0x0);

	size_t parsed = 0;
	if(root_char == 1) {
		memory_memcopy("\\", (*namestring) + parsed, 1);
		tmp_location++;
		parsed++;
	}
	while(prefixpath_char > 0) {
		memory_memcopy("^", (*namestring) + parsed, 1);
		parsed++;
		tmp_location++;
		prefixpath_char--;
	}
	if(seg_count > 1) {
		tmp_location++;
		if(multi_name == 0) {
			tmp_location++;
		}
	}
	if(seg_count > 0) {
		memory_memcopy(tmp_location, (*namestring) + parsed, 4 * seg_count);
	}
	char_t* name_check = *namestring;
	for(size_t i = 0; i < strlen; i++) {
		if(name_check[i] >= 'A' && name_check[i] <= 'Z') {
			continue;
		} else if(name_check[i] >= '0' && name_check[i] <= '9') {
			continue;
		} else if(name_check[i] == '^' || name_check[i] == '\\' || name_check[i] == '_') {
			continue;
		} else {
			memory_free_ext(state->heap, *namestring);
			*namestring = NULL;
			strlen = 0;
			break;
		}
	}

	state->location = save_loc;
	state->remaining = save_rem;
	printf("parsed name: %s len: %li\n", *namestring, strlen);
	if(seg_count > 1) {
		strlen++;
		if(multi_name == 0) {
			strlen++;
		}
	}
	return strlen;
}


int64_t acpi_aml_parse_package_length(acpi_aml_state_t* state, int64_t* package_length){
	uint64_t res = 0;
	uint8_t lead = *state->location;
	uint8_t diff = 1;
	state->location++;
	state->remaining--;
	uint8_t bc = lead >> 6;
	if(bc == 0) {
		*package_length = lead - diff;
		return 1;
	}
	uint8_t byte_count = bc + 1;
	res = lead & 0x4;
	uint8_t shift = 4;
	res = lead & 0x0F;
	while(bc > 0) {
		uint16_t tmp = *state->location;
		state->location++;
		state->remaining--;
		res += (tmp << shift);
		shift += 8;
		bc--;
		diff++;
	}
	*package_length = res - diff;
	return byte_count;
}

int64_t acpi_aml_parse_field_elements(acpi_aml_state_t* state, int64_t remaining, char_t* region_name) {
	int64_t res = 0;
	char_t* namestring;
	int64_t pkg_len;
	while (remaining > 0) {
		if(*state->location == 0x00) {
			state->location++;
			state->remaining--;
			remaining--;
			res = acpi_aml_parse_package_length(state, &pkg_len);
			remaining -= res;
			pkg_len += res;
			printf("reservedfield region: %s offset: %li\n", region_name, pkg_len);
		} else if(*state->location == 0x01) {
			state->location++;
			state->remaining--;
			remaining--;
			uint8_t access_type = *state->location;
			state->location++;
			state->remaining--;
			remaining--;
			uint8_t access_attrib = *state->location;
			state->location++;
			state->remaining--;
			remaining--;
			printf("accessfield region: %s type: 0x%02x attrib: 0x%02x\n", region_name, access_type, access_attrib);
		} else if(*state->location == 0x02) {
			state->location++;
			state->remaining--;
			remaining--;
			if(*state->location == 0x11) {
				state->location++;
				state->remaining--;
				remaining--;
				res = acpi_aml_parse_package_length(state, &pkg_len);
				remaining -= res;
				printf("connectfield region: %s buffer_len: %li\n", region_name, pkg_len);
				while(pkg_len > 0) {
					printf(".");
					pkg_len--;
				}
			} else {
				res = acpi_aml_parse_namestring(state, &namestring);
				state->location += res;
				state->remaining -= res;
				remaining -= res;
				printf("connectfield region: %s name: %s\n", region_name, namestring);
			}
		} else if(*state->location == 0x03) {
			state->location++;
			state->remaining--;
			remaining--;
			uint8_t access_type = *state->location;
			state->location++;
			state->remaining--;
			remaining--;
			uint8_t ext_access_attrib = *state->location;
			state->location++;
			state->remaining--;
			remaining--;
			uint8_t access_len = *state->location;
			state->location++;
			state->remaining--;
			remaining--;
			printf("extaccessfield region: %s type: 0x%02x extattrib: 0x%02x acc_len: %li\n", region_name, access_type, ext_access_attrib, access_len);
		} else {
			res = acpi_aml_parse_namestring(state, &namestring);
			state->location += res;
			state->remaining -= res;
			remaining -= res;
			res = acpi_aml_parse_package_length(state, &pkg_len);
			remaining -= res;
			pkg_len += res;
			printf("namefield region: %s name: %s len: %li\n", region_name, namestring, pkg_len);
		}
	}
	return res;
}

int64_t acpi_aml_parse_scope_symbols(acpi_aml_state_t* state, int64_t remaining) {
	int64_t res = 0;
	while(remaining > 0) {
		//printf("rem: %li %li\n", state->remaining, remaining);
		if(*state->location == 0x00) {
			state->location++;
			state->remaining--;
			remaining--;
			printf(".");
		} else if(*state->location == 0x01) {
			state->location++;
			state->remaining--;
			remaining--;
			printf(".");
		} else if(*state->location == 0xFF) {
			state->location++;
			state->remaining--;
			remaining--;
			printf(".");
		} else if(*state->location == 0x0A) {
			state->location += 2;
			state->remaining -= 2;
			remaining -= 2;
			printf("..");
		} else if(*state->location == 0x0B) {
			state->location += 3;
			state->remaining -= 3;
			remaining -= 3;
			printf("...");
		} else if(*state->location == 0x0C) {
			state->location += 5;
			state->remaining -= 5;
			remaining -= 5;
			printf(".....");
		} else if(*state->location == 0x0E) {
			state->location += 9;
			state->remaining -= 9;
			remaining -= 9;
			printf(".........");
		} else if(*state->location == 0x5B) {
			state->location++;
			state->remaining--;
			remaining--;
			if(*state->location == 0x01) {
				state->location++;
				state->remaining--;
				remaining--;
				char_t* mutex_name;
				res = acpi_aml_parse_namestring(state, &mutex_name);
				if(res <= 0) {
					continue;
				}
				state->location += res;
				state->remaining -= res;
				remaining -= res;
				uint8_t mutex_flags = *state->location;
				state->location += 1;
				state->remaining -= 1;
				remaining -= 1;
				printf("mutex name: %s, flags: 0x%x\n", mutex_flags);
			} else if(*state->location == 0x02) {
				state->location++;
				state->remaining--;
				remaining--;
				char_t* event_name;
				res = acpi_aml_parse_namestring(state, &event_name);
				if(res <= 0) {
					continue;
				}
				state->location += res;
				state->remaining -= res;
				remaining -= res;
				printf("event: %s\n", event_name);
			} else if(*state->location == 0x80) {
				state->location++;
				state->remaining--;
				remaining--;
				char_t* opregion_name;
				res = acpi_aml_parse_namestring(state, &opregion_name);
				if(res <= 0) {
					continue;
				}
				state->location += res;
				state->remaining -= res;
				remaining -= res;
				printf("opregion %s\n", opregion_name);
			} else if(*state->location == 0x81) {
				state->location++;
				state->remaining--;
				remaining--;
				char_t* field_grp_name;
				int64_t field_grp_len;
				res = acpi_aml_parse_package_length(state, &field_grp_len);
				remaining -= res;
				res = acpi_aml_parse_namestring(state, &field_grp_name);
				if(res <= 0) {
					continue;
				}
				state->location += res;
				state->remaining -= res;
				remaining -= res;
				field_grp_len -= res;
				uint8_t field_flags = *state->location;
				state->location += 1;
				state->remaining -= 1;
				remaining -= 1;
				field_grp_len -= 1;
				printf("field group: %s len: %li flags: 0x%x\n", field_grp_name, field_grp_len, field_flags);
				res = acpi_aml_parse_field_elements(state, field_grp_len, field_grp_name);
				remaining -= field_grp_len;
			} else if(*state->location == 0x82) {
				state->location++;
				state->remaining--;
				remaining--;
				char_t* dev_name;
				int64_t dev_len;
				res = acpi_aml_parse_package_length(state, &dev_len);
				remaining -= res;
				res = acpi_aml_parse_namestring(state, &dev_name);
				if(res <= 0) {
					continue;
				}
				state->location += res;
				state->remaining -= res;
				remaining -= res;
				dev_len -= res;
				printf("dev name: %s len: %li\n", dev_name, dev_len);
				acpi_aml_parse_scope_symbols(state, dev_len);
				remaining -= dev_len;
			} else if(*state->location == 0x83) {
				state->location++;
				state->remaining--;
				remaining--;
				char_t* dev_name;
				int64_t dev_len;
				res = acpi_aml_parse_package_length(state, &dev_len);
				remaining -= res;
				res = acpi_aml_parse_namestring(state, &dev_name);
				if(res <= 0) {
					continue;
				}
				state->location += res;
				state->remaining -= res;
				remaining -= res;
				dev_len -= res;
				uint8_t proc_id = *state->location;
				state->location += 1;
				state->remaining -= 1;
				remaining -= 1;
				dev_len -= 1;
				uint16_t pblk_addr = *((uint16_t*)(state->location));
				state->location += 2;
				state->remaining -= 2;
				remaining -= 2;
				dev_len -= 2;
				uint8_t pblk_len = *state->location;
				state->location += 1;
				state->remaining -= 1;
				remaining -= 1;
				dev_len -= 1;
				printf("processor name: %s len: %li procid: 0x%x pblkaddr: 0x%x pblklen: 0x%x\n", dev_name, dev_len, proc_id, pblk_addr, pblk_len);
				acpi_aml_parse_scope_symbols(state, dev_len);
				remaining -= dev_len;
			} else if(*state->location == 0x84) {
				state->location++;
				state->remaining--;
				remaining--;
				char_t* dev_name;
				int64_t dev_len;
				res = acpi_aml_parse_package_length(state, &dev_len);
				remaining -= res;
				res = acpi_aml_parse_namestring(state, &dev_name);
				if(res <= 0) {
					continue;
				}
				state->location += res;
				state->remaining -= res;
				remaining -= res;
				dev_len -= res;
				uint8_t system_level = *state->location;
				state->location += 1;
				state->remaining -= 1;
				remaining -= 1;
				dev_len -= 1;
				uint8_t resource_order = *state->location;
				state->location += 1;
				state->remaining -= 1;
				remaining -= 1;
				dev_len -= 1;
				printf("powerres name: %s len: %li sl: 0x%x ro: 0x%x\n", dev_name, dev_len, system_level, resource_order);
				acpi_aml_parse_scope_symbols(state, dev_len);
				remaining -= dev_len;
			} else if(*state->location == 0x85) {
				state->location++;
				state->remaining--;
				remaining--;
				char_t* dev_name;
				int64_t dev_len;
				res = acpi_aml_parse_package_length(state, &dev_len);
				remaining -= res;
				res = acpi_aml_parse_namestring(state, &dev_name);
				if(res <= 0) {
					continue;
				}
				state->location += res;
				state->remaining -= res;
				remaining -= res;
				dev_len -= res;
				printf("termalzone name: %s len: %li\n", dev_name, dev_len);
				acpi_aml_parse_scope_symbols(state, dev_len);
				remaining -= dev_len;
			} else if(*state->location == 0x86) {
				state->location++;
				state->remaining--;
				remaining--;
				char_t* field_grp_name1;
				char_t* field_grp_name2;
				int64_t field_grp_len;
				res = acpi_aml_parse_package_length(state, &field_grp_len);
				remaining -= res;
				res = acpi_aml_parse_namestring(state, &field_grp_name1);
				if(res <= 0) {
					continue;
				}
				state->location += res;
				state->remaining -= res;
				remaining -= res;
				field_grp_len -= res;
				res = acpi_aml_parse_namestring(state, &field_grp_name2);
				if(res <= 0) {
					continue;
				}
				state->location += res;
				state->remaining -= res;
				remaining -= res;
				field_grp_len -= res;
				uint8_t field_flags = *state->location;
				state->location += 1;
				state->remaining -= 1;
				remaining -= 1;
				field_grp_len -= 1;
				printf("idxfield group: %s:%s len: %li flags: 0x%x\n", field_grp_name1, field_grp_name2, field_grp_len, field_flags);
				char_t* region_name = memory_malloc_ext(state->heap, sizeof(char_t) * (strlen(field_grp_name1) + strlen(field_grp_name2) + 2), 0x0);
				memory_memcopy(field_grp_name1, region_name, strlen(field_grp_name1));
				region_name[strlen(field_grp_name1)] = ':';
				memory_memcopy(field_grp_name2, region_name + strlen(field_grp_name1) + 1, strlen(field_grp_name2));
				res = acpi_aml_parse_field_elements(state, field_grp_len, region_name);
				memory_free_ext(state->heap, region_name);
				remaining -= field_grp_len;
			} else if(*state->location == 0x87) {
				state->location++;
				state->remaining--;
				remaining--;
				char_t* field_grp_name1;
				char_t* field_grp_name2;
				int64_t field_grp_len;
				res = acpi_aml_parse_package_length(state, &field_grp_len);
				remaining -= res;
				res = acpi_aml_parse_namestring(state, &field_grp_name1);
				if(res <= 0) {
					continue;
				}
				state->location += res;
				state->remaining -= res;
				remaining -= res;
				field_grp_len -= res;
				res = acpi_aml_parse_namestring(state, &field_grp_name2);
				if(res <= 0) {
					continue;
				}
				state->location += res;
				state->remaining -= res;
				remaining -= res;
				field_grp_len -= res;
				printf("bankfield group: %s,%s len: %li\n", field_grp_name1, field_grp_name2, field_grp_len);
				state->location += field_grp_len;
				state->remaining -= field_grp_len;
				remaining -= field_grp_len;
				while (field_grp_len > 0) {
					printf(".");
					field_grp_len--;
				}
			} else if(*state->location == 0x88) {
				state->location++;
				state->remaining--;
				remaining--;
				char_t* dataregion_name;
				res = acpi_aml_parse_namestring(state, &dataregion_name);
				if(res <= 0) {
					continue;
				}
				state->location += res;
				state->remaining -= res;
				remaining -= res;
				printf("dataregion %s\n", dataregion_name);
			} else {
				state->location++;
				state->remaining--;
				remaining--;
				printf("..");
			}
		} else if(*state->location == 0x11 || *state->location == 0x12 || *state->location == 0x13) {
			state->location++;
			state->remaining--;
			remaining--;
			int64_t skip_len;
			res = acpi_aml_parse_package_length(state, &skip_len);
			remaining -= res;
			printf("skip buf|pack|varpack! length: %li\n", skip_len);
			state->location += skip_len;
			state->remaining -= skip_len;
			remaining -= skip_len;
			while(skip_len > 0) {
				printf(".");
				skip_len--;
			}
		} else if(*state->location == 0x06) {
			state->location++;
			state->remaining--;
			remaining--;
			char_t* src_name;
			char_t* alias;
			res = acpi_aml_parse_namestring(state, &src_name);
			if(res <= 0) {
				continue;
			}
			state->location += res;
			state->remaining -= res;
			remaining -= res;
			res = acpi_aml_parse_namestring(state, &alias);
			if(res <= 0) {
				continue;
			}
			state->location += res;
			state->remaining -= res;
			remaining -= res;
			printf("alias %s of %s\n", alias, src_name);
		} else if(*state->location == 0x08) {
			state->location++;
			state->remaining--;
			remaining--;
			char_t* var_name;
			res = acpi_aml_parse_namestring(state, &var_name);
			if(res <= 0) {
				continue;
			}
			state->location += res;
			state->remaining -= res;
			remaining -= res;
			printf("var defined: %s\n", var_name);
		} else if(*state->location == 0x10) {
			state->location++;
			state->remaining--;
			remaining--;
			int64_t scope_len;
			res = acpi_aml_parse_package_length(state, &scope_len);
			remaining -= res;
			char_t* scope_name;
			res = acpi_aml_parse_namestring(state, &scope_name);
			if(res <= 0) {
				continue;
			}
			state->location += res;
			state->remaining -= res;
			remaining -= res;
			scope_len -= res;
			printf("scope %s defined with length: %li\n", scope_name, scope_len);
			acpi_aml_parse_scope_symbols(state, scope_len);
			remaining -= scope_len;
		} else if(*state->location == 0x14) {
			state->location++;
			state->remaining--;
			remaining--;
			int64_t method_len;
			res = acpi_aml_parse_package_length(state, &method_len);
			remaining -= res;
			char_t* method_name = NULL;
			res = acpi_aml_parse_namestring(state, &method_name);
			if(res <= 0) {
				continue;
			}
			state->location += res;
			state->remaining -= res;
			remaining -= res;
			method_len -= res;
			uint8_t method_flags = *state->location;
			state->location += 1;
			state->remaining -= 1;
			remaining -= 1;
			method_len -= 1;
			printf("method found! length: %li, method name: %s, flags: 0x%x\n", method_len, method_name, method_flags);
			acpi_aml_parse_scope_symbols(state, method_len);
			remaining -= method_len;
		} else if(*state->location == 0x15) {
			state->location++;
			state->remaining--;
			remaining--;
			char_t* external_name = NULL;
			res = acpi_aml_parse_namestring(state, &external_name);
			if(res <= 0) {
				continue;
			}
			state->location += res;
			state->remaining -= res;
			remaining -= res;
			uint8_t object_type = *state->location;
			state->location += 1;
			state->remaining -= 1;
			remaining -= 1;
			uint8_t arg_count = *state->location;
			state->location += 1;
			state->remaining -= 1;
			remaining -= 1;
			printf("external: name: %s, ot: %x argc: %i\n", external_name, object_type, arg_count);
		} else if(*state->location == 0xA0) {
			state->location++;
			state->remaining--;
			remaining--;
			int64_t if_pkg_len;
			res = acpi_aml_parse_package_length(state, &if_pkg_len);
			remaining -= res;
			printf("if pkg len: %li\n", if_pkg_len);
			acpi_aml_parse_scope_symbols(state, if_pkg_len);
			remaining -= if_pkg_len;
			if(remaining != 0 && *state->location == 0xA1) {
				state->location++;
				state->remaining--;
				remaining--;
				int64_t else_pkg_len;
				res = acpi_aml_parse_package_length(state, &else_pkg_len);
				remaining -= res;
				printf("else pkg len: %li\n", else_pkg_len);
				acpi_aml_parse_scope_symbols(state, else_pkg_len);
				remaining -= else_pkg_len;
			}
		} else if(*state->location == 0xA2) {
			state->location++;
			state->remaining--;
			remaining--;
			int64_t while_pkg_len;
			res = acpi_aml_parse_package_length(state, &while_pkg_len);
			remaining -= res;
			printf("while pkg len: %li\n", while_pkg_len);
			acpi_aml_parse_scope_symbols(state, while_pkg_len);
			remaining -= while_pkg_len;
		} else {
			state->location++;
			state->remaining--;
			remaining--;
			printf(".");
		}
	}
	return res;
}

uint32_t main(uint32_t argc, char_t** argv) {
	setup_ram();
	FILE* fp;
	int64_t size;

	argv++;
	printf("file name: %s\n", *argv);
	fp = fopen(*argv, "r");
	if(fp == NULL) {
		print_error("can not open the file");
		return -2;
	}
	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	printf("file size: %i\n", size);
	fseek(fp, 0x0, SEEK_SET);
	uint8_t* aml_data = memory_malloc(sizeof(uint8_t) * size);
	if(aml_data == NULL) {
		print_error("cannot malloc madt data area");
		return -3;
	}
	fread(aml_data, 1, size, fp);
	fclose(fp);
	print_success("file readed");

	acpi_sdt_header_t* hdr = (acpi_sdt_header_t*)aml_data;

	char_t* table_name = memory_malloc(sizeof(uint8_t) * 5);
	memory_memcopy(hdr->signature, table_name, 4);
	printf("table name: %s\n", table_name);
	memory_free(table_name);

	uint8_t* aml = aml_data + sizeof(acpi_sdt_header_t);
	size -= sizeof(acpi_sdt_header_t);


	char_t* global_scope_name = memory_malloc_ext(NULL, sizeof(char_t) * 2, 0x0);
	memory_memcopy("\\", global_scope_name, 1);
	acpi_aml_scope_t* scope = memory_malloc_ext(NULL, sizeof(acpi_aml_scope_t), 0x0);
	scope->parent = NULL;
	scope->name = global_scope_name;
	scope->members = linkedlist_create_list_with_heap(NULL);

	acpi_aml_state_t* state = memory_malloc_ext(NULL, sizeof(acpi_aml_state_t), 0x0);
	state->heap = NULL;
	state->remaining = size;
	state->location = aml;
	state->scope = scope;

	acpi_aml_parse_scope_symbols(state, state->remaining);



	linkedlist_destroy_with_data(scope->members);
	memory_free(scope->name);
	memory_free(scope);
	memory_free(state);
	memory_free(aml_data);

	dump_ram("tmp/mem.dump");
	print_success("Tests are passed");
	return 0;
}
