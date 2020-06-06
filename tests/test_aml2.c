#define RAMSIZE 0x100000 * 8
#include "setup.h"
#include <acpi.h>
#include <linkedlist.h>
#include <strings.h>
#include <utils.h>

typedef enum {
	ACPI_AML_SYMBOL_TYPE_AVAIL              = 0x0001,
	ACPI_AML_SYMBOL_TYPE_STATIC             = 0x0002,
	ACPI_AML_SYMBOL_TYPE_CONDNEED           = 0x0004,
	ACPI_AML_SYMBOL_TYPE_SCOPED             = 0x0008,
	ACPI_AML_SYMBOL_TYPE_CODE               = 0x0010,
	ACPI_AML_SYMBOL_TYPE_UNPARSED           = 0x0020,
	ACPI_AML_SYMBOL_TYPE_DEVICE             = 0x0100,
	ACPI_AML_SYMBOL_TYPE_POWERRESOURCE      = 0x0200,
	ACPI_AML_SYMBOL_TYPE_PROCESSOR          = 0x0300,
	ACPI_AML_SYMBOL_TYPE_THERMALZONE        = 0x0400,
	ACPI_AML_SYMBOL_TYPE_SCOPE              = 0x0500,
	ACPI_AML_SYMBOL_TYPE_METHOD             = 0x0600,
	ACPI_AML_SYMBOL_TYPE_FIELD              = 0x0700,
	ACPI_AML_SYMBOL_TYPE_INDEXFIELD         = 0x0800,
	ACPI_AML_SYMBOL_TYPE_BANKFIELD          = 0x0900,
	ACPI_AML_SYMBOL_TYPE_ALIAS              = 0x0A00,
	ACPI_AML_SYMBOL_TYPE_EVENT              = 0x0B00,
	ACPI_AML_SYMBOL_TYPE_MUTEX              = 0x0C00,
	ACPI_AML_SYMBOL_TYPE_EXTERNAL           = 0x0D00,
	ACPI_AML_SYMBOL_TYPE_IF                 = 0x0E00,
	ACPI_AML_SYMBOL_TYPE_ELSE               = 0x0F00,
	ACPI_AML_SYMBOL_TYPE_WHILE              = 0x1000,
	ACPI_AML_SYMBOL_TYPE_BUFFER             = 0x1100,
	ACPI_AML_SYMBOL_TYPE_PACKAGE            = 0x1200,
	ACPI_AML_SYMBOL_TYPE_VARPACKAGE         = 0x1300,
	ACPI_AML_SYMBOL_TYPE_NAME               = 0x1400,
	ACPI_AML_SYMBOL_TYPE_NUMBER             = 0x1500,
	ACPI_AML_SYMBOL_TYPE_STRING             = 0x1600,
	ACPI_AML_SYMBOL_TYPE_OPREGION           = 0x1700,
	ACPI_AML_SYMBOL_TYPE_DATAREGION         = 0x1800,
	ACPI_AML_SYMBOL_TYPE_BANKFIELDGROUP     = 0x1900,
} acpi_aml_symbol_type_t;

typedef enum {
	ACPI_AML_FIELD_TYPE_BANK = ACPI_AML_SYMBOL_TYPE_BANKFIELD,
	ACPI_AML_FIELD_TYPE_INDEX = ACPI_AML_SYMBOL_TYPE_INDEXFIELD,
	ACPI_AML_FIELD_TYPE_NORMAL = ACPI_AML_SYMBOL_TYPE_FIELD
} acpi_aml_field_type_t;

typedef struct {
	uint64_t condition_id;
	uint8_t* data;
} acpi_aml_condition_t;

typedef struct {
	uint64_t condition_id;
	int8_t not;
} acpi_aml_condition_symbol_t;

typedef struct acpi_aml_symbol {
	memory_heap_t* heap;
	acpi_aml_symbol_type_t type;
	char_t* name;
	struct acpi_aml_symbol* parent;
	linkedlist_t members;
	union {
		struct {
			uint8_t proc_id;
			uint64_t pblk_len;
			uint8_t pblk_addr;
		} processor;
		struct {
			uint8_t system_level;
			uint8_t resource_order;
		} power_resource;
		struct {
			uint8_t method_flags;
		} method;
		struct {
			union {
				char_t* name;
				struct acpi_aml_symbol* symbol;
			};
			union {
				char_t* ext_name;
				struct acpi_aml_symbol* ext_symbol;
			};
			int64_t ext_data;
			uint8_t lock_rule;
			uint8_t update_rule;
			int64_t offset;
			int64_t length;
			uint8_t access_type;
			uint8_t access_attrib;
			uint8_t access_length;
		} field;
		union {
			char_t* string;
			struct acpi_aml_symbol* symbol;
		} alias;
		uint8_t mutex_flags;
		struct {
			uint8_t type;
			uint8_t argcnt;
		} external;
		struct {
			char_t* name1;
			char_t* name2;
		}bankfieldgroup;
		struct {
			uint8_t* data;
			int64_t length;
		} unparsed;
		int64_t number;
	};
	linkedlist_t condition_symbols;
} acpi_aml_symbol_t;

typedef struct {
	memory_heap_t* heap;
	uint8_t* location;
	int64_t remaining;
	acpi_aml_symbol_t* scope;
	acpi_aml_symbol_type_t default_types;
	linkedlist_t last_symbol;
	char_t* last_anonymous_namestring;
	linkedlist_t conditions;
	linkedlist_t current_conditions;
	uint64_t next_condition_id;
} acpi_aml_state_t;


char_t* acpi_aml_next_anonymous_namestring(acpi_aml_state_t* state);

int64_t acpi_aml_parse_namestring(acpi_aml_state_t* state, char_t** namestring);
int64_t acpi_aml_parse_package_length(acpi_aml_state_t* state, int64_t* package_length);
int64_t acpi_aml_parse_scope_symbols(acpi_aml_state_t* state, int64_t remaining);
int64_t acpi_aml_parse_field_elements(acpi_aml_state_t* state, int64_t remaining, acpi_aml_field_type_t idx_type,
                                      uint8_t flags, char_t* region_name, char_t* ext_name, uint64_t ext_data);

int64_t acpi_aml_symbol_exists(acpi_aml_symbol_t* scope, char_t* symbol_name, acpi_aml_symbol_t** symbol);
int64_t acpi_aml_symbol_insert(acpi_aml_symbol_t* scope, acpi_aml_symbol_t* symbol);

int64_t acpi_aml_symbol_table_print(acpi_aml_symbol_t* symbol, int64_t indent);

int64_t acpi_aml_symbol_table_destroy(acpi_aml_symbol_t* symbol);

char_t* acpi_aml_next_anonymous_namestring(acpi_aml_state_t* state) {
	if(state->last_anonymous_namestring == NULL) {
		state->last_anonymous_namestring = memory_malloc_ext(state->heap, sizeof(char_t) * 5, 0x0);
		memory_memset(state->last_anonymous_namestring, '0', 4);
		printf("wtf\n");
	}
	char_t* res = strdup_at_heap(state->heap, state->last_anonymous_namestring);
	state->last_anonymous_namestring[3]++;
	for(int64_t i = 3; i >= 0; i--) {
		if(state->last_anonymous_namestring[i] == 0x3A ) {
			state->last_anonymous_namestring[i] = 'A';
		}
		if(state->last_anonymous_namestring[i] > 'Z') {
			if(i != 0) {
				state->last_anonymous_namestring[i - 1]++;
				state->last_anonymous_namestring[i] = '0';
			}
		}
	}
	return res;
}

int64_t acpi_aml_symbol_table_destroy(acpi_aml_symbol_t* symbol) {
	int64_t detroy_count = 0;
	if(symbol->type & ACPI_AML_SYMBOL_TYPE_SCOPED) {
		iterator_t* iter = linkedlist_iterator_create(symbol->members);
		while(iter->end_of_iterator(iter) != 0) {
			acpi_aml_symbol_t* sym = iter->get_item(iter);
			detroy_count += acpi_aml_symbol_table_destroy(sym);
			iter = iter->next(iter);
		}
		iter->destroy(iter);
		linkedlist_destroy(symbol->members);
	}
	memory_free_ext(symbol->heap, symbol->name);
	if((symbol->type & ACPI_AML_SYMBOL_TYPE_FIELD) == ACPI_AML_SYMBOL_TYPE_FIELD) {
		memory_free_ext(symbol->heap, symbol->field.name);
	}
	if((symbol->type & ACPI_AML_SYMBOL_TYPE_INDEXFIELD) == ACPI_AML_SYMBOL_TYPE_INDEXFIELD) {
		memory_free_ext(symbol->heap, symbol->field.name);
		memory_free_ext(symbol->heap, symbol->field.ext_name);
	}
	if((symbol->type & ACPI_AML_SYMBOL_TYPE_BANKFIELD) == ACPI_AML_SYMBOL_TYPE_BANKFIELD) {
		memory_free_ext(symbol->heap, symbol->field.name);
		memory_free_ext(symbol->heap, symbol->field.ext_name);
	}
	if((symbol->type & ACPI_AML_SYMBOL_TYPE_ALIAS) == ACPI_AML_SYMBOL_TYPE_ALIAS) {
		memory_free_ext(symbol->heap, symbol->alias.string);
	}
	if((symbol->type & ACPI_AML_SYMBOL_TYPE_BANKFIELDGROUP) == ACPI_AML_SYMBOL_TYPE_BANKFIELDGROUP) {
		memory_free_ext(symbol->heap, symbol->bankfieldgroup.name1);
		memory_free_ext(symbol->heap, symbol->bankfieldgroup.name2);
	}
	memory_free_ext(symbol->heap, symbol);
	detroy_count++;
	return detroy_count;
}

int64_t acpi_aml_symbol_table_print(acpi_aml_symbol_t* symbol, int64_t indent){
	int item_count = 1;
	for(int64_t i = 0; i < indent; i++) {
		printf("  ");
	}
	printf("%s(%04x)", symbol->name, symbol->type);
	if((symbol->type & ACPI_AML_SYMBOL_TYPE_UNPARSED) == ACPI_AML_SYMBOL_TYPE_UNPARSED) {
		printf(" : ");
		if((symbol->type & ACPI_AML_SYMBOL_TYPE_NUMBER) == ACPI_AML_SYMBOL_TYPE_NUMBER) {
			printf("%x", symbol->number);
		} else {
			for(int64_t i = 0; i < symbol->unparsed.length; i++) {
				printf("%02x ", symbol->unparsed.data[i]);
			}
		}
	}
	printf("\n");
	if((symbol->type & ACPI_AML_SYMBOL_TYPE_SCOPED) == ACPI_AML_SYMBOL_TYPE_SCOPED) {
		iterator_t* iter = linkedlist_iterator_create(symbol->members);
		while(iter->end_of_iterator(iter) != 0) {
			acpi_aml_symbol_t* sym = iter->get_item(iter);
			item_count += acpi_aml_symbol_table_print(sym, indent + 1);
			iter = iter->next(iter);
		}
		iter->destroy(iter);
	}
	return item_count;
}

int64_t acpi_aml_symbol_exists(acpi_aml_symbol_t* scope, char_t* symbol_name, acpi_aml_symbol_t** symbol) {
	printf("searching started for %s at %s\n", symbol_name, scope->name);
	acpi_aml_symbol_t* tmp_scope = scope;
	if(strlen(symbol_name) == 1 && *symbol_name == '\\') {
		while(tmp_scope->parent != NULL) {
			tmp_scope = tmp_scope->parent;
		}
		*symbol = tmp_scope;
		return 0;
	}
	char_t* sym_name = symbol_name;
	int8_t found = -1;
	if(*sym_name == '\\') {
		while(tmp_scope->parent != NULL) {
			tmp_scope = tmp_scope->parent;
		}
		printf("root found %s\n", tmp_scope->name );
		sym_name++;
	}

	while(*sym_name == '^') {
		tmp_scope = tmp_scope->parent;
		sym_name++;
	}

	while(strlen(sym_name) > 0) {
		printf("searching %s at %s\n", sym_name, tmp_scope->name);
		iterator_t* iter = linkedlist_iterator_create(tmp_scope->members);
		found = -1;
		while(iter->end_of_iterator(iter) != 0) {
			acpi_aml_symbol_t* sym = iter->get_item(iter);
			if(memory_memcompare(sym_name, sym->name, 4) == 0) {
				printf("swap scope: %s\n", sym->name);
				tmp_scope = sym;
				found = 0;
				break;
			}
			iter = iter->next(iter);
		}
		iter->destroy(iter);
		if(found == -1) {
			return -1;
		}
		sym_name += 4;
	}
	*symbol = tmp_scope;
	return found;
}

int64_t inserted_sym_count = 1;

int64_t acpi_aml_symbol_insert(acpi_aml_symbol_t* scope, acpi_aml_symbol_t* symbol) {
	char_t* sym_name = symbol->name;
	acpi_aml_symbol_t* tmp_scope = scope;
	if(*sym_name == '\\') {
		while(tmp_scope->parent != NULL) {
			tmp_scope = tmp_scope->parent;
		}
		sym_name++;
	}

	while(*sym_name == '^') {
		tmp_scope = tmp_scope->parent;
		sym_name++;
	}

	while(strlen(sym_name) > 0) {
		iterator_t* iter = linkedlist_iterator_create(tmp_scope->members);
		while(iter->end_of_iterator(iter) != 0) {
			acpi_aml_symbol_t* sym = iter->get_item(iter);
			if(memory_memcompare(sym_name, sym->name, 4) == 0) {
				tmp_scope = sym;
				break;
			}
			iter = iter->next(iter);
		}
		iter->destroy(iter);
		sym_name += 4;
	}
	sym_name = symbol->name;
	if(strlen(sym_name) != 1) {
		sym_name += strlen(sym_name) - 4;
	} else if(*sym_name != '\\') {
		printf("wtf!!!!\n");
		return -1;
	}
	if(memory_memcompare(sym_name, tmp_scope->name, 4) != 0) {
		symbol->name = memory_malloc_ext(tmp_scope->heap, sizeof(char_t) * 5, 0x0);
		memory_memcopy(sym_name, symbol->name, 4);
		symbol->parent = tmp_scope;
		printf("sym inserted: %s at %s\n", sym_name, tmp_scope->name);
		linkedlist_list_insert(tmp_scope->members, symbol);
		inserted_sym_count++;
		return 0;
	}
	printf("sym not inserted: %s %s\n", sym_name, tmp_scope->name);
	return -1;
}


int64_t acpi_aml_parse_namestring(acpi_aml_state_t* state, char_t** namestring) {
	uint8_t* save_loc = state->location;
	int64_t save_rem = state->remaining;

	size_t strlen = 0;
	int8_t root_char = 0;
	int8_t prefixpath_char = 0;
	uint8_t* tmp_location = state->location;
	uint8_t seg_count = 0;
	uint8_t multi_name = -1;

	if(state->location[0] == '\\') {
		root_char = 1;
		strlen++;
		state->location++;
		state->remaining--;
	} else if (state->location[0] == '^') {
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
		seg_count = 0;
		state->location++;
		state->remaining--;
	} else if(state->location[0] == 0x2E) {
		seg_count = 2;
		state->location++;
		state->remaining--;
	} else if(state->location[0] == 0x2F) {
		state->location++;
		state->remaining--;
		seg_count = state->location[0];
		state->location++;
		state->remaining--;
		multi_name = 0;
	} else {
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

int64_t acpi_aml_parse_field_elements(acpi_aml_state_t* state, int64_t remaining, acpi_aml_field_type_t idx_type,
                                      uint8_t flags, char_t* region_name, char_t* ext_name, uint64_t ext_data) {
	int64_t res = 0;
	char_t* namestring;
	int64_t pkg_len;

	uint8_t access_type = flags & 0x0F;
	uint8_t access_attrib = 0;
	uint8_t lock_rule = (flags & 0x10) >> 4;
	uint8_t update_rule = (flags & 0x60) >> 5;
	uint8_t access_length = 0;
	int64_t local_offset = 0;

	while (remaining > 0) {
		if(*state->location == 0x00) {
			state->location++;
			state->remaining--;
			remaining--;
			res = acpi_aml_parse_package_length(state, &pkg_len);
			remaining -= res;
			pkg_len += res;
			local_offset = pkg_len * 8;
			printf("reservedfield region: %s offset: %li\n", region_name, pkg_len);
		} else if(*state->location == 0x01) {
			state->location++;
			state->remaining--;
			remaining--;
			access_type = *state->location;
			state->location++;
			state->remaining--;
			remaining--;
			access_attrib = *state->location;
			state->location++;
			state->remaining--;
			remaining--;
			printf("accessfield region: %s type: 0x%02x attrib: 0x%02x\n", region_name, access_type, access_attrib);
		} else if(*state->location == 0x02) {
			// TODO: handle it
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
			access_type = *state->location;
			state->location++;
			state->remaining--;
			remaining--;
			access_attrib = *state->location;
			state->location++;
			state->remaining--;
			remaining--;
			access_length = *state->location;
			state->location++;
			state->remaining--;
			remaining--;
			printf("extaccessfield region: %s type: 0x%02x extattrib: 0x%02x acc_len: %li\n", region_name, access_type, access_attrib, access_length);
		} else {
			res = acpi_aml_parse_namestring(state, &namestring);
			state->location += res;
			state->remaining -= res;
			remaining -= res;
			res = acpi_aml_parse_package_length(state, &pkg_len);
			remaining -= res;
			pkg_len += res;
			printf("namefield region: %s name: %s len: %li\n", region_name, namestring, pkg_len);
			acpi_aml_symbol_t* sym = memory_malloc_ext(state->heap, sizeof(acpi_aml_symbol_t), 0x0);
			sym->type = state->default_types | idx_type;
			sym->name = namestring;
			sym->heap = state->heap;
			sym->field.name = strdup_at_heap(state->heap, region_name); // TODO: convert symbol
			sym->field.ext_name = strdup_at_heap(state->heap, ext_name); // TODO: convert symbol
			sym->field.ext_data = ext_data;
			sym->field.lock_rule = lock_rule;
			sym->field.update_rule = update_rule;
			sym->field.offset = local_offset;
			sym->field.length = pkg_len;
			sym->field.access_type = access_type;
			sym->field.access_attrib = access_attrib;
			sym->field.access_length = access_length;
			acpi_aml_symbol_insert(state->scope, sym);
			memory_free_ext(state->heap, namestring);

			local_offset += pkg_len;
		}
	}
	return res;
}

int64_t acpi_aml_parse_scope_symbols(acpi_aml_state_t* state, int64_t remaining) {
	int8_t end_of_unparsed = 0;
	int64_t res = 0;
	while(remaining > 0) {
		//printf("rem: %li %li\n", state->remaining, remaining);
		acpi_aml_symbol_t* dbg_sym = linkedlist_get_data_at_position(state->last_symbol, 0);
		printf("unparsed to sym: %s\n", dbg_sym->name);
		if(*state->location == 0x00) {
			end_of_unparsed = 0;
			acpi_aml_symbol_t* sym = memory_malloc_ext(state->heap, sizeof(acpi_aml_symbol_t), 0x0);
			char_t* sym_name = acpi_aml_next_anonymous_namestring(state);
			sym->heap = state->heap;
			sym->type = state->default_types | ACPI_AML_SYMBOL_TYPE_NUMBER | ACPI_AML_SYMBOL_TYPE_UNPARSED;
			sym->name = sym_name;
			sym->number = 0;
			acpi_aml_symbol_insert(state->scope, sym);
			memory_free_ext(state->heap, sym_name);
			state->location++;
			state->remaining--;
			remaining--;
		} else if(*state->location == 0x01) {
			end_of_unparsed = 0;
			acpi_aml_symbol_t* sym = memory_malloc_ext(state->heap, sizeof(acpi_aml_symbol_t), 0x0);
			char_t* sym_name = acpi_aml_next_anonymous_namestring(state);
			sym->heap = state->heap;
			sym->type = state->default_types | ACPI_AML_SYMBOL_TYPE_NUMBER | ACPI_AML_SYMBOL_TYPE_UNPARSED;
			sym->name = sym_name;
			sym->number = 1;
			acpi_aml_symbol_insert(state->scope, sym);
			memory_free_ext(state->heap, sym_name);
			state->location++;
			state->remaining--;
			remaining--;
		} else if(*state->location == 0xFF) {
			end_of_unparsed = 0;
			acpi_aml_symbol_t* sym = memory_malloc_ext(state->heap, sizeof(acpi_aml_symbol_t), 0x0);
			char_t* sym_name = acpi_aml_next_anonymous_namestring(state);
			sym->heap = state->heap;
			sym->type = state->default_types | ACPI_AML_SYMBOL_TYPE_NUMBER | ACPI_AML_SYMBOL_TYPE_UNPARSED;
			sym->name = sym_name;
			sym->number = 0xFF;
			acpi_aml_symbol_insert(state->scope, sym);
			memory_free_ext(state->heap, sym_name);
			state->location++;
			state->remaining--;
			remaining--;
		} else if(*state->location == 0x0A) {
			end_of_unparsed = 0;
			state->location++;
			state->remaining--;
			acpi_aml_symbol_t* sym = memory_malloc_ext(state->heap, sizeof(acpi_aml_symbol_t), 0x0);
			char_t* sym_name = acpi_aml_next_anonymous_namestring(state);
			sym->heap = state->heap;
			sym->type = state->default_types | ACPI_AML_SYMBOL_TYPE_NUMBER | ACPI_AML_SYMBOL_TYPE_UNPARSED;
			sym->name = sym_name;
			sym->number = *((uint8_t*)state->location);
			acpi_aml_symbol_insert(state->scope, sym);
			memory_free_ext(state->heap, sym_name);
			state->location++;
			state->remaining--;
			remaining -= 2;
		} else if(*state->location == 0x0B) {
			end_of_unparsed = 0;
			state->location++;
			state->remaining--;
			acpi_aml_symbol_t* sym = memory_malloc_ext(state->heap, sizeof(acpi_aml_symbol_t), 0x0);
			char_t* sym_name = acpi_aml_next_anonymous_namestring(state);
			sym->heap = state->heap;
			sym->type = state->default_types | ACPI_AML_SYMBOL_TYPE_NUMBER | ACPI_AML_SYMBOL_TYPE_UNPARSED;
			sym->name = sym_name;
			sym->number = *((uint16_t*)state->location);
			acpi_aml_symbol_insert(state->scope, sym);
			memory_free_ext(state->heap, sym_name);
			state->location += 2;
			state->remaining -= 2;
			remaining -= 3;
		} else if(*state->location == 0x0C) {
			end_of_unparsed = 0;
			state->location++;
			state->remaining--;
			acpi_aml_symbol_t* sym = memory_malloc_ext(state->heap, sizeof(acpi_aml_symbol_t), 0x0);
			char_t* sym_name = acpi_aml_next_anonymous_namestring(state);
			sym->heap = state->heap;
			sym->type = state->default_types | ACPI_AML_SYMBOL_TYPE_NUMBER | ACPI_AML_SYMBOL_TYPE_UNPARSED;
			sym->name = sym_name;
			sym->number = *((uint32_t*)state->location);
			acpi_aml_symbol_insert(state->scope, sym);
			memory_free_ext(state->heap, sym_name);
			state->location += 4;
			state->remaining -= 4;
			remaining -= 5;
		} else if(*state->location == 0x0E) {
			end_of_unparsed = 0;
			state->location++;
			state->remaining--;
			acpi_aml_symbol_t* sym = memory_malloc_ext(state->heap, sizeof(acpi_aml_symbol_t), 0x0);
			char_t* sym_name = acpi_aml_next_anonymous_namestring(state);
			sym->heap = state->heap;
			sym->type = state->default_types | ACPI_AML_SYMBOL_TYPE_NUMBER | ACPI_AML_SYMBOL_TYPE_UNPARSED;
			sym->name = sym_name;
			sym->number = *((uint64_t*)state->location);
			acpi_aml_symbol_insert(state->scope, sym);
			memory_free_ext(state->heap, sym_name);
			state->location += 8;
			state->remaining -= 8;
			remaining -= 9;
		} else if(*state->location == 0x5B) {
			state->location++;
			state->remaining--;
			remaining--;
			if(*state->location == 0x01) {
				end_of_unparsed = 0;
				acpi_aml_symbol_t* ls = linkedlist_get_data_at_position(state->last_symbol, 0);
				if((ls->type & ACPI_AML_SYMBOL_TYPE_SCOPED) != ACPI_AML_SYMBOL_TYPE_SCOPED) {
					linkedlist_stack_pop(state->last_symbol);
				}
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
				acpi_aml_symbol_t* sym;
				if(acpi_aml_symbol_exists(state->scope, mutex_name, &sym) == -1) {
					sym = memory_malloc_ext(state->heap, sizeof(acpi_aml_symbol_t), 0x0);
					sym->heap = state->heap;
					sym->type = state->default_types | ACPI_AML_SYMBOL_TYPE_MUTEX;
					sym->name = mutex_name;
					sym->mutex_flags = mutex_flags;
					acpi_aml_symbol_insert(state->scope, sym);
				}
				memory_free_ext(state->heap, mutex_name);
			} else if(*state->location == 0x02) {
				end_of_unparsed = 0;
				acpi_aml_symbol_t* ls = linkedlist_get_data_at_position(state->last_symbol, 0);
				if((ls->type & ACPI_AML_SYMBOL_TYPE_SCOPED) != ACPI_AML_SYMBOL_TYPE_SCOPED) {
					linkedlist_stack_pop(state->last_symbol);
				}
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
				acpi_aml_symbol_t* sym;
				if(acpi_aml_symbol_exists(state->scope, event_name, &sym) == -1) {
					sym = memory_malloc_ext(state->heap, sizeof(acpi_aml_symbol_t), 0x0);
					sym->heap = state->heap;
					sym->type = state->default_types | ACPI_AML_SYMBOL_TYPE_EVENT;
					sym->name = event_name;
					acpi_aml_symbol_insert(state->scope, sym);
				}
				memory_free_ext(state->heap, event_name);
			} else if(*state->location == 0x80) {
				end_of_unparsed = 0;
				acpi_aml_symbol_t* ls = linkedlist_get_data_at_position(state->last_symbol, 0);
				if((ls->type & ACPI_AML_SYMBOL_TYPE_SCOPED) != ACPI_AML_SYMBOL_TYPE_SCOPED) {
					linkedlist_stack_pop(state->last_symbol);
				}
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
				acpi_aml_symbol_t* sym;
				if(acpi_aml_symbol_exists(state->scope, opregion_name, &sym) == -1) {
					sym = memory_malloc_ext(state->heap, sizeof(acpi_aml_symbol_t), 0x0);
					sym->heap = state->heap;
					sym->type = state->default_types | ACPI_AML_SYMBOL_TYPE_OPREGION;
					sym->name = opregion_name;
					acpi_aml_symbol_insert(state->scope, sym);
				}
				linkedlist_stack_push(state->last_symbol, sym);
				memory_free_ext(state->heap, opregion_name);
			} else if(*state->location == 0x81) {
				end_of_unparsed = 0;
				acpi_aml_symbol_t* ls = linkedlist_get_data_at_position(state->last_symbol, 0);
				if((ls->type & ACPI_AML_SYMBOL_TYPE_SCOPED) != ACPI_AML_SYMBOL_TYPE_SCOPED) {
					linkedlist_stack_pop(state->last_symbol);
				}
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
				res = acpi_aml_parse_field_elements(state, field_grp_len, ACPI_AML_FIELD_TYPE_NORMAL, field_flags,
				                                    field_grp_name, NULL, -1);
				memory_free_ext(state->heap, field_grp_name);
				remaining -= field_grp_len;
			} else if(*state->location == 0x82) {
				end_of_unparsed = 0;
				acpi_aml_symbol_t* ls = linkedlist_get_data_at_position(state->last_symbol, 0);
				if((ls->type & ACPI_AML_SYMBOL_TYPE_SCOPED) != ACPI_AML_SYMBOL_TYPE_SCOPED) {
					linkedlist_stack_pop(state->last_symbol);
				}
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
				acpi_aml_symbol_t* child_scope;
				acpi_aml_symbol_t* restore_scope = state->scope;
				if(acpi_aml_symbol_exists(state->scope, dev_name, &child_scope) == -1) {
					child_scope = memory_malloc_ext(state->heap, sizeof(acpi_aml_symbol_t), 0x0);
					child_scope->heap = state->heap;
					child_scope->type = state->default_types | ACPI_AML_SYMBOL_TYPE_SCOPED | ACPI_AML_SYMBOL_TYPE_DEVICE;
					child_scope->name = dev_name;
					child_scope->members = linkedlist_create_list_with_heap(state->heap);
					acpi_aml_symbol_insert(state->scope, child_scope);
				}
				state->scope = child_scope;
				linkedlist_stack_push(state->last_symbol, child_scope);
				acpi_aml_parse_scope_symbols(state, dev_len);
				linkedlist_stack_pop(state->last_symbol);
				state->scope = restore_scope;
				remaining -= dev_len;
				memory_free_ext(state->heap, dev_name);
			} else if(*state->location == 0x83) {
				end_of_unparsed = 0;
				acpi_aml_symbol_t* ls = linkedlist_get_data_at_position(state->last_symbol, 0);
				if((ls->type & ACPI_AML_SYMBOL_TYPE_SCOPED) != ACPI_AML_SYMBOL_TYPE_SCOPED) {
					linkedlist_stack_pop(state->last_symbol);
				}
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
				acpi_aml_symbol_t* child_scope;
				acpi_aml_symbol_t* restore_scope = state->scope;
				if(acpi_aml_symbol_exists(state->scope, dev_name, &child_scope) == -1) {
					child_scope = memory_malloc_ext(state->heap, sizeof(acpi_aml_symbol_t), 0x0);
					child_scope->heap = state->heap;
					child_scope->type = state->default_types | ACPI_AML_SYMBOL_TYPE_SCOPED | ACPI_AML_SYMBOL_TYPE_PROCESSOR;
					child_scope->name = dev_name;
					child_scope->members = linkedlist_create_list_with_heap(state->heap);
					child_scope->processor.proc_id = proc_id;
					child_scope->processor.pblk_addr = pblk_addr;
					child_scope->processor.pblk_len = pblk_len;
					acpi_aml_symbol_insert(state->scope, child_scope);
				}
				state->scope = child_scope;
				linkedlist_stack_push(state->last_symbol, child_scope);
				acpi_aml_parse_scope_symbols(state, dev_len);
				linkedlist_stack_pop(state->last_symbol);
				state->scope = restore_scope;
				remaining -= dev_len;
				memory_free_ext(state->heap, dev_name);
			} else if(*state->location == 0x84) {
				end_of_unparsed = 0;
				acpi_aml_symbol_t* ls = linkedlist_get_data_at_position(state->last_symbol, 0);
				if((ls->type & ACPI_AML_SYMBOL_TYPE_SCOPED) != ACPI_AML_SYMBOL_TYPE_SCOPED) {
					linkedlist_stack_pop(state->last_symbol);
				}
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
				acpi_aml_symbol_t* child_scope;
				acpi_aml_symbol_t* restore_scope = state->scope;
				if(acpi_aml_symbol_exists(state->scope, dev_name, &child_scope) == -1) {
					child_scope = memory_malloc_ext(state->heap, sizeof(acpi_aml_symbol_t), 0x0);
					child_scope->heap = state->heap;
					child_scope->type = state->default_types | ACPI_AML_SYMBOL_TYPE_SCOPED | ACPI_AML_SYMBOL_TYPE_POWERRESOURCE;
					child_scope->name = dev_name;
					child_scope->members = linkedlist_create_list_with_heap(state->heap);
					child_scope->power_resource.system_level = system_level;
					child_scope->power_resource.resource_order = resource_order;
					acpi_aml_symbol_insert(state->scope, child_scope);
				}
				state->scope = child_scope;
				linkedlist_stack_push(state->last_symbol, child_scope);
				acpi_aml_parse_scope_symbols(state, dev_len);
				linkedlist_stack_pop(state->last_symbol);
				state->scope = restore_scope;
				remaining -= dev_len;
				memory_free_ext(state->heap, dev_name);
			} else if(*state->location == 0x85) {
				end_of_unparsed = 0;
				acpi_aml_symbol_t* ls = linkedlist_get_data_at_position(state->last_symbol, 0);
				if((ls->type & ACPI_AML_SYMBOL_TYPE_SCOPED) != ACPI_AML_SYMBOL_TYPE_SCOPED) {
					linkedlist_stack_pop(state->last_symbol);
				}
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
				acpi_aml_symbol_t* child_scope;
				acpi_aml_symbol_t* restore_scope = state->scope;
				if(acpi_aml_symbol_exists(state->scope, dev_name, &child_scope) == -1) {
					child_scope = memory_malloc_ext(state->heap, sizeof(acpi_aml_symbol_t), 0x0);
					child_scope->heap = state->heap;
					child_scope->type = state->default_types | ACPI_AML_SYMBOL_TYPE_SCOPED | ACPI_AML_SYMBOL_TYPE_THERMALZONE;
					child_scope->name = dev_name;
					child_scope->members = linkedlist_create_list_with_heap(state->heap);
					acpi_aml_symbol_insert(state->scope, child_scope);
				}
				state->scope = child_scope;
				linkedlist_stack_push(state->last_symbol, child_scope);
				acpi_aml_parse_scope_symbols(state, dev_len);
				linkedlist_stack_pop(state->last_symbol);
				state->scope = restore_scope;
				remaining -= dev_len;
				memory_free_ext(state->heap, dev_name);
			} else if(*state->location == 0x86) {
				end_of_unparsed = 0;
				acpi_aml_symbol_t* ls = linkedlist_get_data_at_position(state->last_symbol, 0);
				if((ls->type & ACPI_AML_SYMBOL_TYPE_SCOPED) != ACPI_AML_SYMBOL_TYPE_SCOPED) {
					linkedlist_stack_pop(state->last_symbol);
				}
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
				res = acpi_aml_parse_field_elements(state, field_grp_len, ACPI_AML_FIELD_TYPE_INDEX, field_flags,
				                                    field_grp_name1, field_grp_name2, -1);
				memory_free_ext(state->heap, field_grp_name1);
				memory_free_ext(state->heap, field_grp_name2);
				remaining -= field_grp_len;
			} else if(*state->location == 0x87) {
				end_of_unparsed = 0;
				acpi_aml_symbol_t* ls = linkedlist_get_data_at_position(state->last_symbol, 0);
				if((ls->type & ACPI_AML_SYMBOL_TYPE_SCOPED) != ACPI_AML_SYMBOL_TYPE_SCOPED) {
					linkedlist_stack_pop(state->last_symbol);
				}
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
				char_t* aname = acpi_aml_next_anonymous_namestring(state);
				acpi_aml_symbol_t* sym = memory_malloc_ext(state->heap, sizeof(acpi_aml_symbol_t), 0x0);
				sym->heap = state->heap;
				sym->type = state->default_types | ACPI_AML_SYMBOL_TYPE_BANKFIELDGROUP;
				sym->bankfieldgroup.name1 = field_grp_name1;
				sym->bankfieldgroup.name2 = field_grp_name2;
				sym->name = aname;
				acpi_aml_symbol_insert(state->scope, sym);
				memory_free_ext(state->heap, aname);

				aname = acpi_aml_next_anonymous_namestring(state);
				sym = memory_malloc_ext(state->heap, sizeof(acpi_aml_symbol_t), 0x0);
				sym->heap = state->heap;
				sym->type = state->default_types | ACPI_AML_SYMBOL_TYPE_UNPARSED;
				sym->name = aname;
				sym->unparsed.length = field_grp_len;
				sym->unparsed.data = state->location;
				acpi_aml_symbol_insert(state->scope, sym);
				memory_free_ext(state->heap, aname);
				end_of_unparsed = 0;

				state->location += field_grp_len;
				state->remaining -= field_grp_len;
				remaining -= field_grp_len;
			} else if(*state->location == 0x88) {
				end_of_unparsed = 0;
				acpi_aml_symbol_t* ls = linkedlist_get_data_at_position(state->last_symbol, 0);
				if((ls->type & ACPI_AML_SYMBOL_TYPE_SCOPED) != ACPI_AML_SYMBOL_TYPE_SCOPED) {
					linkedlist_stack_pop(state->last_symbol);
				}
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
				acpi_aml_symbol_t* sym;
				if(acpi_aml_symbol_exists(state->scope, dataregion_name, &sym) == -1) {
					sym = memory_malloc_ext(state->heap, sizeof(acpi_aml_symbol_t), 0x0);
					sym->heap = state->heap;
					sym->type = state->default_types | ACPI_AML_SYMBOL_TYPE_DATAREGION;
					sym->name = dataregion_name;
					acpi_aml_symbol_insert(state->scope, sym);
				}
				linkedlist_stack_push(state->last_symbol, sym);
				memory_free_ext(state->heap, dataregion_name);
				printf("dataregion %s\n", dataregion_name);
			} else {
				if(end_of_unparsed == 0) {
					char_t* sym_name = acpi_aml_next_anonymous_namestring(state);
					acpi_aml_symbol_t* sym = memory_malloc_ext(state->heap, sizeof(acpi_aml_symbol_t), 0x0);
					sym->heap = state->heap;
					sym->type = state->default_types | ACPI_AML_SYMBOL_TYPE_UNPARSED;
					sym->name = sym_name;
					sym->unparsed.length = 2;
					sym->unparsed.data = state->location - 1;
					acpi_aml_symbol_insert(state->scope, sym);
					memory_free_ext(state->heap, sym_name);
				} else {
					acpi_aml_symbol_t* sym = linkedlist_get_data_at_position(state->scope->members, linkedlist_size(state->scope->members) - 1);
					sym->unparsed.length += 1;
				}
				end_of_unparsed = -1;
				state->location++;
				state->remaining--;
				remaining--;
			}
		} else if(*state->location == 0x11 || *state->location == 0x12 || *state->location == 0x13) {
			uint16_t unparsed_type = *state->location;
			end_of_unparsed = 0;
			state->location++;
			state->remaining--;
			remaining--;
			int64_t skip_len;
			res = acpi_aml_parse_package_length(state, &skip_len);
			remaining -= res;
			printf("skip buf|pack|varpack! length: %li\n", skip_len);
			char_t* sym_name = acpi_aml_next_anonymous_namestring(state);
			acpi_aml_symbol_t* sym = memory_malloc_ext(state->heap, sizeof(acpi_aml_symbol_t), 0x0);
			sym->heap = state->heap;
			sym->type = state->default_types | ACPI_AML_SYMBOL_TYPE_UNPARSED | unparsed_type << 8;
			sym->unparsed.length = skip_len;
			sym->unparsed.data = state->location;
			sym->name = sym_name;
			acpi_aml_symbol_insert(state->scope, sym);
			memory_free_ext(state->heap, sym_name);
			state->location += skip_len;
			state->remaining -= skip_len;
			remaining -= skip_len;
			end_of_unparsed = 0;
		} else if(*state->location == 0x06) {
			end_of_unparsed = 0;
			acpi_aml_symbol_t* ls = linkedlist_get_data_at_position(state->last_symbol, 0);
			if((ls->type & ACPI_AML_SYMBOL_TYPE_SCOPED) != ACPI_AML_SYMBOL_TYPE_SCOPED) {
				linkedlist_stack_pop(state->last_symbol);
			}
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
			acpi_aml_symbol_t* sym;
			if(acpi_aml_symbol_exists(state->scope, alias, &sym) == -1) {
				sym = memory_malloc_ext(state->heap, sizeof(acpi_aml_symbol_t), 0x0);
				sym->heap = state->heap;
				sym->type = state->default_types | ACPI_AML_SYMBOL_TYPE_ALIAS;
				sym->name = alias;
				sym->alias.string = src_name;
				acpi_aml_symbol_insert(state->scope, sym);
			}
			memory_free_ext(state->heap, alias);
		} else if(*state->location == 0x08) {
			end_of_unparsed = 0;
			acpi_aml_symbol_t* ls = linkedlist_get_data_at_position(state->last_symbol, 0);
			if((ls->type & ACPI_AML_SYMBOL_TYPE_SCOPED) != ACPI_AML_SYMBOL_TYPE_SCOPED) {
				linkedlist_stack_pop(state->last_symbol);
			}
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
			acpi_aml_symbol_t* sym;
			if(acpi_aml_symbol_exists(state->scope, var_name, &sym) == -1) {
				sym = memory_malloc_ext(state->heap, sizeof(acpi_aml_symbol_t), 0x0);
				sym->heap = state->heap;
				sym->type = state->default_types | ACPI_AML_SYMBOL_TYPE_NAME;
				sym->name = var_name;
				acpi_aml_symbol_insert(state->scope, sym);
			}
			memory_free_ext(state->heap, var_name);
		} else if(*state->location == 0x10) {
			end_of_unparsed = 0;
			acpi_aml_symbol_t* ls = linkedlist_get_data_at_position(state->last_symbol, 0);
			if((ls->type & ACPI_AML_SYMBOL_TYPE_SCOPED) != ACPI_AML_SYMBOL_TYPE_SCOPED) {
				linkedlist_stack_pop(state->last_symbol);
			}
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
			acpi_aml_symbol_t* child_scope;
			acpi_aml_symbol_t* restore_scope = state->scope;
			if(acpi_aml_symbol_exists(state->scope, scope_name, &child_scope) == -1) {
				child_scope = memory_malloc_ext(state->heap, sizeof(acpi_aml_symbol_t), 0x0);
				child_scope->heap = state->heap;
				child_scope->type = state->default_types | ACPI_AML_SYMBOL_TYPE_SCOPED | ACPI_AML_SYMBOL_TYPE_SCOPE;
				child_scope->name = scope_name;
				child_scope->members = linkedlist_create_list_with_heap(state->heap);
				acpi_aml_symbol_insert(state->scope, child_scope);
			}
			state->scope = child_scope;
			linkedlist_stack_push(state->last_symbol, child_scope);
			acpi_aml_parse_scope_symbols(state, scope_len);
			linkedlist_stack_pop(state->last_symbol);
			state->scope = restore_scope;
			remaining -= scope_len;
			memory_free_ext(state->heap, scope_name);
		} else if(*state->location == 0x14) {
			end_of_unparsed = 0;
			acpi_aml_symbol_t* ls = linkedlist_get_data_at_position(state->last_symbol, 0);
			if((ls->type & ACPI_AML_SYMBOL_TYPE_SCOPED) != ACPI_AML_SYMBOL_TYPE_SCOPED) {
				linkedlist_stack_pop(state->last_symbol);
			}
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
			acpi_aml_symbol_t* child_scope;
			acpi_aml_symbol_t* restore_scope = state->scope;
			if(acpi_aml_symbol_exists(state->scope, method_name, &child_scope) == -1) {
				child_scope = memory_malloc_ext(state->heap, sizeof(acpi_aml_symbol_t), 0x0);
				child_scope->heap = state->heap;
				child_scope->type = state->default_types | ACPI_AML_SYMBOL_TYPE_SCOPED | ACPI_AML_SYMBOL_TYPE_METHOD;
				child_scope->name = method_name;
				child_scope->members = linkedlist_create_list_with_heap(state->heap);
				acpi_aml_symbol_insert(state->scope, child_scope);
			}
			state->scope = child_scope;
			linkedlist_stack_push(state->last_symbol, child_scope);
			acpi_aml_parse_scope_symbols(state, method_len);
			linkedlist_stack_pop(state->last_symbol);
			state->scope = restore_scope;
			remaining -= method_len;
			memory_free_ext(state->heap, method_name);
		} else if(*state->location == 0x15) {
			end_of_unparsed = 0;
			acpi_aml_symbol_t* ls = linkedlist_get_data_at_position(state->last_symbol, 0);
			if((ls->type & ACPI_AML_SYMBOL_TYPE_SCOPED) != ACPI_AML_SYMBOL_TYPE_SCOPED) {
				linkedlist_stack_pop(state->last_symbol);
			}
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
			acpi_aml_symbol_t* sym;
			if(acpi_aml_symbol_exists(state->scope, external_name, &sym) == -1) {
				sym = memory_malloc_ext(state->heap, sizeof(acpi_aml_symbol_t), 0x0);
				sym->heap = state->heap;
				sym->type = state->default_types | ACPI_AML_SYMBOL_TYPE_EXTERNAL;
				sym->name = external_name;
				sym->external.type = object_type;
				sym->external.argcnt = arg_count;
				acpi_aml_symbol_insert(state->scope, sym);
			}
			memory_free_ext(state->heap, external_name);
		} else if(*state->location == 0xA0) {
			end_of_unparsed = 0;
			acpi_aml_symbol_t* ls = linkedlist_get_data_at_position(state->last_symbol, 0);
			if((ls->type & ACPI_AML_SYMBOL_TYPE_SCOPED) != ACPI_AML_SYMBOL_TYPE_SCOPED) {
				linkedlist_stack_pop(state->last_symbol);
			}
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
				end_of_unparsed = 0;
				acpi_aml_symbol_t* ls = linkedlist_get_data_at_position(state->last_symbol, 0);
				if((ls->type & ACPI_AML_SYMBOL_TYPE_SCOPED) != ACPI_AML_SYMBOL_TYPE_SCOPED) {
					linkedlist_stack_pop(state->last_symbol);
				}
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
			end_of_unparsed = 0;
			acpi_aml_symbol_t* ls = linkedlist_get_data_at_position(state->last_symbol, 0);
			if((ls->type & ACPI_AML_SYMBOL_TYPE_SCOPED) != ACPI_AML_SYMBOL_TYPE_SCOPED) {
				linkedlist_stack_pop(state->last_symbol);
			}
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
			if(end_of_unparsed == 0) {
				char_t* sym_name = acpi_aml_next_anonymous_namestring(state);
				acpi_aml_symbol_t* sym = memory_malloc_ext(state->heap, sizeof(acpi_aml_symbol_t), 0x0);
				sym->heap = state->heap;
				sym->type = state->default_types | ACPI_AML_SYMBOL_TYPE_UNPARSED;
				sym->name = sym_name;
				sym->unparsed.length = 1;
				sym->unparsed.data = state->location;
				acpi_aml_symbol_insert(state->scope, sym);
				memory_free_ext(state->heap, sym_name);
			} else {
				acpi_aml_symbol_t* sym = linkedlist_get_data_at_position(state->scope->members, linkedlist_size(state->scope->members) - 1);
				sym->unparsed.length += 1;
			}
			end_of_unparsed = -1;
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
	if(argc < 2) {
		print_error("no required parameter");
		return -1;
	}

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

	memory_heap_t* heap = NULL;
	char_t* global_scope_name = memory_malloc_ext(heap, sizeof(char_t) * 2, 0x0);
	memory_memcopy("\\", global_scope_name, 1);
	acpi_aml_symbol_t* scope = memory_malloc_ext(heap, sizeof(acpi_aml_symbol_t), 0x0);
	scope->type = ACPI_AML_SYMBOL_TYPE_AVAIL | ACPI_AML_SYMBOL_TYPE_STATIC | ACPI_AML_SYMBOL_TYPE_SCOPED | ACPI_AML_SYMBOL_TYPE_SCOPE;
	scope->parent = NULL;
	scope->name = global_scope_name;
	scope->members = linkedlist_create_list_with_heap(heap);

	acpi_aml_state_t* state = memory_malloc_ext(heap, sizeof(acpi_aml_state_t), 0x0);
	state->heap = heap;
	state->remaining = size;
	state->location = aml;
	state->scope = scope;
	state->default_types = ACPI_AML_SYMBOL_TYPE_AVAIL | ACPI_AML_SYMBOL_TYPE_STATIC;
	state->last_symbol = linkedlist_create_stack_with_heap(heap);
	linkedlist_stack_push(state->last_symbol, scope);

	acpi_aml_parse_scope_symbols(state, state->remaining);

	printf("\nsymbol table:\n");
	int64_t p_s_c = acpi_aml_symbol_table_print(scope, 0);

	int64_t d_s_c = acpi_aml_symbol_table_destroy(scope);

	printf("inserted: %li printed: %li destroyed: %li\n", inserted_sym_count, p_s_c, d_s_c);

	linkedlist_destroy(state->last_symbol);
	memory_free_ext(state->heap, state->last_anonymous_namestring);
	memory_free_ext(state->heap, state);
	memory_free(aml_data);

	dump_ram("tmp/mem.dump");
	print_success("Tests are passed");
	return 0;
}
