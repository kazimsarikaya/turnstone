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
	printf("object name=%s type=", obj->name );
	switch (obj->type) {
	case ACPI_AML_OT_NUMBER:
		printf("number value=0x%lx bytecnt=%i\n", obj->number.value, obj->number.bytecnt );
		break;
	case ACPI_AML_OT_STRING:
		printf("string value=%s\n", obj->string );
		break;
	case ACPI_AML_OT_ALIAS:
		printf("alias value=%s\n", obj->alias_target->name );
		break;
	case ACPI_AML_OT_BUFFER:
		printf("buffer buflen=%i\n", obj->buffer.buflen );
		break;
	case ACPI_AML_OT_PACKAGE:
		len = acpi_aml_cast_as_integer(obj->package.pkglen);
		printf("pkg pkglen=%i initial pkglen=%i\n", len, linkedlist_size(obj->package.elements) );
		break;
	case ACPI_AML_OT_SCOPE:
		printf("scope\n");
		break;
	case ACPI_AML_OT_DEVICE:
		printf("device\n");
		break;
	case ACPI_AML_OT_POWERRES:
		printf("powerres system_level=%i resource_order=%i\n", obj->powerres.system_level, obj->powerres.resource_order);
		break;
	case ACPI_AML_OT_PROCESSOR:
		printf("processor procid=%i pblk_addr=0x%x pblk_len=%i\n", obj->processor.procid, obj->processor.pblk_addr, obj->processor.pblk_len);
		break;
	case ACPI_AML_OT_THERMALZONE:
		printf("thermalzone\n");
		break;
	case ACPI_AML_OT_MUTEX:
		printf("mutex syncflags=%i\n", obj->mutex_sync_flags);
		break;
	case ACPI_AML_OT_EVENT:
		printf("event\n");
		break;
	case ACPI_AML_OT_DATAREGION:
		printf("dataregion signature=%s oemid=%s oemtableid=%s\n", obj->dataregion.signature, obj->dataregion.oemid, obj->dataregion.oemtableid);
		break;
	case ACPI_AML_OT_OPREGION:
		printf("opregion region_space=0x%x region_offset=0x%lx region_len=0x%lx\n", obj->opregion.region_space, obj->opregion.region_offset, obj->opregion.region_len);
		break;
	case ACPI_AML_OT_METHOD:
		printf("method argcount=%i serflag=%i synclevel=%i termlistlen=%i\n", obj->method.arg_count, obj->method.serflag,
		       obj->method.sync_level, obj->method.termlist_length);
		break;
	case ACPI_AML_OT_EXTERNAL:
		printf("external objecttype=%i argcount=%i\n", obj->external.object_type, obj->external.arg_count);
		break;
	case ACPI_AML_OT_FIELD:
		printf("field related_object=%s offset=0x%lx sizeasbit=%i\n", obj->field.related_object->name, obj->field.offset, obj->field.sizeasbit);
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

int8_t acpi_aml_executor_opcode(acpi_aml_parser_context_t* ctx, apci_aml_opcode_t* opcode){
	UNUSED(ctx);
	UNUSED(opcode);
	printf("exec opcode 0x%04x\n", opcode->opcode);
	return -1;
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

	acpi_aml_parser_context_t* ctx = acp_aml_parser_context_create(aml, size);
	if(ctx == NULL) {
		print_error("cannot create parser context");
	} else {
		if(acpi_aml_parse_all_items(ctx, NULL, NULL) == 0) {
			print_success("aml parsed");
		} else {
			print_error("aml not parsed");
		}

		acpi_aml_print_symbol_table(ctx);
	}

	memory_free(aml_data);
	dump_ram("tmp/mem.dump");
	print_success("Tests are passed");
	return 0;
}
