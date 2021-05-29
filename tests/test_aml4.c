#define RAMSIZE 0x100000 * 8
#include "setup.h"
#include <acpi.h>
#include <acpi/aml.h>
#include <linkedlist.h>
#include <strings.h>
#include <utils.h>


extern acpi_aml_parse_f acpi_aml_parse_fs[];


void acpi_aml_print_object(acpi_aml_object_t* obj){
	int64_t len = 0;
	printf("object name=%s ", obj->name );
	switch (obj->type) {
	case ACPI_AML_OT_NUMBER:
		printf("obj value=%i\n", obj->number );
		break;
	case ACPI_AML_OT_STRING:
		printf("obj value=%s\n", obj->string );
		break;
	case ACPI_AML_OT_ALIAS:
		printf("obj value=%s\n", obj->string );
		break;
	case ACPI_AML_OT_BUFFER:
		printf("obj buflen=%i\n", obj->buffer.buflen );
		break;
	case ACPI_AML_OT_PACKAGE:
		len = acpi_aml_cast_as_integer(obj->package.pkglen);
		printf("obj pkglen=%i initial pkglen=%i\n", len, linkedlist_size(obj->package.elements) );
		break;
	default:
		printf("unknown object\n");
	}
}


void acpi_aml_print_symbol_table(acpi_aml_parser_context_t* ctx){
	uint64_t item_count = 0;
	iterator_t* iter = linkedlist_iterator_create(ctx->symbols);
	while(iter->end_of_iterator(iter) != 0) {
		acpi_aml_object_t* sym = iter->get_item(iter);
		acpi_aml_print_object(sym);
		item_count++;
		iter = iter->next(iter);
	}
	iter->destroy(iter);

	printf("totoal syms %i\n", item_count );;
}




int8_t acpi_aml_parse_op_extended(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
	UNUSED(data);
	UNUSED(ctx);
	uint64_t t_consumed = 0;

	if(consumed != NULL) {
		*consumed = t_consumed;
	}

	return -1;
}

int8_t acpi_aml_executor_opcode(acpi_aml_parser_context_t* ctx, apci_aml_opcode_t* opcode){
	UNUSED(ctx);
	UNUSED(opcode);
	printf("exec opcode\n");
	return -1;
}

int8_t acpi_aml_parse_op1_tgt1(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
	uint64_t t_consumed = 0;
	uint64_t r_consumed = 1;

	uint8_t oc = *ctx->data;
	ctx->data++;
	ctx->remaining--;

	apci_aml_opcode_t* opcode = memory_malloc(sizeof(apci_aml_opcode_t));
	opcode->type = ACPI_AML_OCT_OP1_TGT1;
	opcode->opcode = oc;

	acpi_aml_object_t* src0 = memory_malloc(sizeof(acpi_aml_object_t));
	if(acpi_aml_parse_one_item(ctx, (void**)&src0, &t_consumed) != 0) {
		if(src0->refcount == 0) {
			memory_free(src0);
		}
		memory_free(opcode);
		return -1;
	}
	r_consumed += t_consumed;
	t_consumed = 0;
	opcode->op0 = src0;

	acpi_aml_object_t* tgt0 = memory_malloc(sizeof(acpi_aml_object_t));
	if(acpi_aml_parse_one_item(ctx, (void**)&tgt0, &t_consumed) != 0) {
		if(src0->refcount == 0) {
			memory_free(src0);
		}
		if(tgt0->refcount == 0) {
			memory_free(tgt0);
		}
		memory_free(opcode);
		return -1;
	}
	r_consumed += t_consumed;
	t_consumed = 0;
	opcode->tgt0 = tgt0;

	if(acpi_aml_executor_opcode(ctx, opcode) != 0) {
		if(src0->refcount == 0) {
			memory_free(src0);
		}
		if(tgt0->refcount == 0) {
			memory_free(tgt0);
		}
		memory_free(opcode);
		return -1;
	}


	if(data != NULL) {
		acpi_aml_object_t* resobj = (acpi_aml_object_t*)*data;
		resobj->type = ACPI_AML_OT_OPCODE_EXEC_RETURN;
		resobj->opcode_exec_return = opcode->return_obj;
	}

	memory_free(opcode);

	if(consumed != NULL) {
		*consumed = r_consumed;
	}

	return 0;
}


int8_t acpi_aml_parse_name(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
	UNUSED(data);
	UNUSED(consumed);

	ctx->data++;
	ctx->remaining--;

	uint64_t namelen = acpi_aml_len_namestring(ctx);
	char_t* name = memory_malloc(sizeof(char_t) * namelen + 1);

	if(acpi_aml_parse_namestring(ctx, (void**)&name, NULL) != 0) {
		memory_free(name);
		return -1;
	}

	char_t* nomname = acpi_aml_normalize_name(ctx->scope_prefix, name);
	memory_free(name);

	acpi_aml_object_t* obj = memory_malloc(sizeof(acpi_aml_object_t));

	obj->name = nomname;

	if(acpi_aml_parse_one_item(ctx, (void**)&obj, NULL) != 0) {
		memory_free(obj);
		memory_free(nomname);
		return -1;
	}

	linkedlist_list_insert(ctx->symbols, obj);

	acpi_aml_print_object(obj);

	return 0;
}



int8_t acpi_aml_parse_method(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
	UNUSED(data);
	UNUSED(ctx);
	uint64_t t_consumed = 0;

	if(consumed != NULL) {
		*consumed = t_consumed;
	}

	return -1;
}

int8_t acpi_aml_parse_external(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
	UNUSED(data);
	UNUSED(ctx);
	uint64_t t_consumed = 0;

	if(consumed != NULL) {
		*consumed = t_consumed;
	}

	return -1;
}

int8_t acpi_aml_parse_symbol(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
	uint64_t t_consumed = 0;

	uint64_t namelen = acpi_aml_len_namestring(ctx);
	char_t* name = memory_malloc(sizeof(char_t) * namelen + 1);

	if(acpi_aml_parse_namestring(ctx, (void**)&name, NULL) != 0) {
		memory_free(name);
		return -1;
	}

	//TODO: some symbols may be avail at runtime
	acpi_aml_object_t* tmp_obj = acpi_aml_symbol_lookup(ctx, name);
	if(tmp_obj == NULL) {
		memory_free(name);
		return -1;
	}
	tmp_obj->refcount++;

	if(*data != NULL) {
		*data = (void*)tmp_obj;
	} else {
		return -1;
	}

	// TODO: method call has params

	t_consumed = namelen;


	if(consumed != NULL) {
		*consumed = t_consumed;
	}

	return 0;
}

int8_t acpi_aml_parse_one_item(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
	printf("scope: -%s- one_item data: 0x%02x length: %li remaining: %li\n", ctx->scope_prefix, *ctx->data, ctx->length, ctx->remaining);
	int8_t res = -1;
	if (*ctx->data != NULL && acpi_aml_is_namestring_start(ctx->data) == 0) {
		res = acpi_aml_parse_symbol(ctx, data, consumed);
	} else {
		acpi_aml_parse_f parser = acpi_aml_parse_fs[*ctx->data];
		if( parser == NULL ) {
			print_error("null parser\n");
			res = -1;
		}else {
			res = parser(ctx, data, consumed);
		}
	}
	printf("one item completed\n");
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

	char_t* table_name = memory_malloc(sizeof(char_t) * 5);
	memory_memcopy(hdr->signature, table_name, 4);
	printf("table name: %s\n", table_name);
	memory_free(table_name);

	uint8_t* aml = aml_data + sizeof(acpi_sdt_header_t);
	size -= sizeof(acpi_sdt_header_t);

	char_t* root_prefix = "";

	acpi_aml_parser_context_t* ctx = memory_malloc(sizeof(acpi_aml_parser_context_t));
	ctx->data = aml;
	ctx->length = size;
	ctx->remaining = size;
	ctx->scope_prefix = root_prefix;
	ctx->symbols = linkedlist_create_list_with_heap(NULL);

	if(acpi_aml_parse_all_items(ctx, NULL, NULL) == 0) {
		print_success("aml parsed");
	} else {
		print_error("aml not parsed");
	}

	acpi_aml_print_symbol_table(ctx);

	memory_free(ctx);

	memory_free(aml_data);
	dump_ram("tmp/mem.dump");
	print_success("Tests are passed");
	return 0;
}
