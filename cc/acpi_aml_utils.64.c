/**
 * @file acpi_aml_utils.64.c
 * @brief acpi parser utils
 */
#include <acpi/aml.h>
#include <strings.h>


int64_t acpi_aml_cast_as_integer(acpi_aml_object_t* obj){
	switch (obj->type) {
	case ACPI_AML_OT_NUMBER:
		return obj->number;
	case ACPI_AML_OT_STRING:
		return atoi(obj->string);
	default:
		break;
	}
	return -1;
}

int8_t acpi_aml_is_lead_name_char(uint8_t* c) {
	if(('A' <= *c && *c <= 'Z') || *c == '_') {
		return 0;
	}
	return -1;
}

int8_t acpi_aml_is_digit_char(uint8_t* c) {
	if('0' <= *c && *c <= '9') {
		return 0;
	}
	return -1;
}

int8_t acpi_aml_is_name_char(uint8_t* data) {
	if(acpi_aml_is_lead_name_char(data) == 0 ||  acpi_aml_is_digit_char(data) == 0 ) {
		return 0;
	}
	return -1;
}

int8_t acpi_aml_is_root_char(uint8_t* c) {
	if (*c == ACPI_AML_ROOT_CHAR) {
		return 0;
	}
	return -1;
}

int8_t acpi_aml_is_parent_prefix_char(uint8_t* c){
	if (*c == ACPI_AML_PARENT_CHAR) {
		return 0;
	}
	return -1;
}



char_t* acpi_aml_normalize_name(char_t* prefix, char_t* name) {
	uint64_t max_len = strlen(prefix) + strlen(name) + 1;
	char_t* dst_name = memory_malloc(sizeof(char_t) * max_len);

	if(acpi_aml_is_root_char((uint8_t*)name) == 0) {
		strcpy(name + 1, dst_name);
	} else {
		uint64_t prefix_cnt = 0;
		char_t* tmp = name;
		while(acpi_aml_is_parent_prefix_char((uint8_t*)tmp) == 0) {
			tmp++;
			prefix_cnt++;
		}
		uint64_t src_len = strlen(prefix) - (prefix_cnt * 4);
		memory_memcopy(prefix, dst_name, src_len);
		strcpy(name + prefix_cnt, dst_name + src_len);
	}
	char_t* nomname = memory_malloc(sizeof(char_t) * strlen(dst_name) + 1);
	strcpy(dst_name, nomname);
	return nomname;
}



int8_t acpi_aml_is_nameseg(uint8_t* data) {
	if(acpi_aml_is_lead_name_char(data) == 0 && acpi_aml_is_name_char(data + 1) == 0 && acpi_aml_is_name_char(data + 2) == 0
	   && acpi_aml_is_name_char(data + 3) == 0) {
		return 0;
	}
	return -1;
}

int8_t acpi_aml_is_namestring_start(uint8_t* data){
	if(acpi_aml_is_lead_name_char(data) == 0 || acpi_aml_is_root_char(data) == 0 || acpi_aml_is_parent_prefix_char(data) == 0) {
		return 0;
	}
	return -1;
}

uint64_t acpi_aml_parse_package_length(acpi_aml_parser_context_t* ctx){
	uint8_t pkgleadbyte = *ctx->data;
	ctx->data++;
	uint8_t bytecnt = pkgleadbyte >> 6;
	uint8_t usedbytes = 1 + bytecnt;
	uint64_t pkglen = 0;

	if(bytecnt == 0) {
		pkglen = pkgleadbyte & 0x3F;
	}else {
		pkglen = pkgleadbyte & 0x0F;
		uint8_t tmp8 = 0;
		uint64_t tmp64 = 0;
		if(bytecnt > 0) {
			tmp8 = *ctx->data;
			tmp64 = tmp8;
			pkglen = (tmp64 << 4) | pkglen;
			ctx->data++;
			bytecnt--;
		}
		if(bytecnt > 0) {
			tmp8 = *ctx->data;
			tmp64 = tmp8;
			pkglen = (tmp64 << 12) | pkglen;
			ctx->data++;
			bytecnt--;
		}
		if(bytecnt > 0) {
			tmp8 = *ctx->data;
			tmp64 = tmp8;
			pkglen = (tmp64 << 20) | pkglen;
			ctx->data++;
			bytecnt--;
		}
	}

	ctx->remaining -= usedbytes;
	return pkglen - usedbytes;
}

uint64_t acpi_aml_len_namestring(acpi_aml_parser_context_t* ctx){
	uint64_t res = 0;
	uint8_t* local_data = ctx->data;

	while(acpi_aml_is_root_char(local_data) == 0 || acpi_aml_is_parent_prefix_char(local_data) == 0) {
		res++;
		local_data++;
	}

	if(*local_data == ACPI_AML_DUAL_PREFIX) {
		return res + 8;
	}else if(*local_data == ACPI_AML_MULTI_PREFIX) {
		local_data++;
		return res + 4 * (*local_data);
	} else if(*local_data == ACPI_AML_ZERO) {
		return 0;
	}

	return 4;
}

acpi_aml_object_t* acpi_aml_symbol_lookup_at_table(linkedlist_t table, char_t* prefix, char_t* symbol_name){
	char_t* tmp_prefix = memory_malloc(strlen(prefix) + 1);
	strcpy(prefix, tmp_prefix);
	while(1) {
		char_t* nomname = acpi_aml_normalize_name(tmp_prefix, symbol_name);
		for(int64_t len = linkedlist_size(table) - 1; len >= 0; len--) {
			acpi_aml_object_t* obj = (acpi_aml_object_t*)linkedlist_get_data_at_position(table, len);
			if(strcmp(obj->name, nomname) == 0) {
				memory_free(tmp_prefix);
				return obj;
			}
		}
		if(strlen(tmp_prefix) == 0) {
			break;
		}
		tmp_prefix[strlen(tmp_prefix) - 4] = NULL;
	}
	memory_free(tmp_prefix);
	return NULL;
}

acpi_aml_object_t* acpi_aml_symbol_lookup(acpi_aml_parser_context_t* ctx, char_t* symbol_name){
	if(ctx->local_symbols != NULL) {
		acpi_aml_object_t* obj = acpi_aml_symbol_lookup_at_table(ctx->local_symbols, ctx->scope_prefix, symbol_name);
		if(obj != NULL) {
			return obj;
		}
	}
	return acpi_aml_symbol_lookup_at_table(ctx->symbols, ctx->scope_prefix, symbol_name);
}

int8_t acpi_aml_add_obj_to_symboltable(acpi_aml_parser_context_t* ctx, acpi_aml_object_t* obj) {
	acpi_aml_object_t* oldsym = acpi_aml_symbol_lookup(ctx, obj->name);
	if(oldsym != NULL) {
		if(obj->type == ACPI_AML_OT_SCOPE && (
				 oldsym->type == ACPI_AML_OT_SCOPE ||
				 oldsym->type == ACPI_AML_OT_DEVICE ||
				 oldsym->type == ACPI_AML_OT_POWERRES ||
				 oldsym->type == ACPI_AML_OT_PROCESSOR ||
				 oldsym->type == ACPI_AML_OT_THERMALZONE
				 )
		   ) {
			return 0;
		}
		return -1;
	}
	if(ctx->local_symbols != NULL) {
		linkedlist_list_insert(ctx->local_symbols, obj);
	}else {
		linkedlist_list_insert(ctx->symbols, obj);
	}
	return 0;
}

uint8_t acpi_aml_get_index_of_extended_code(uint8_t code) {
	if(code >= 0x80) {
		return code - 0x6d;
	}
	if(code >= 0x30) {
		return code - 0x21;
	}
	if(code >= 0x1f) {
		return code - 0x1b;
	}
	if(code >= 0x12) {
		return code - 0x10;
	}
	return code - 0x01;
}
