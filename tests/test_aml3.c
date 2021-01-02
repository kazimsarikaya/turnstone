#define RAMSIZE 0x100000 * 8
#include "setup.h"
#include <acpi.h>
#include <linkedlist.h>
#include <strings.h>
#include <utils.h>

typedef  struct acpi_aml_object {

} acpi_aml_object_t;

typedef uint32_t (* acpi_aml_parser)(uint8_t**, uint32_t*, acpi_aml_object_t**);

#define CREATE_PARSER_TRY_PARSE(name) \
	uint32_t acpi_aml_parser_try_parse_ ## name(uint8_t * *, uint32_t*, acpi_aml_object_t * *);

CREATE_PARSER_TRY_PARSE(termlist);
CREATE_PARSER_TRY_PARSE(termobj);
CREATE_PARSER_TRY_PARSE(termarglist);
CREATE_PARSER_TRY_PARSE(termarg);
CREATE_PARSER_TRY_PARSE(defalias);
CREATE_PARSER_TRY_PARSE(defname);
CREATE_PARSER_TRY_PARSE(defscope);
CREATE_PARSER_TRY_PARSE(namedobj);
CREATE_PARSER_TRY_PARSE(dataobj);
CREATE_PARSER_TRY_PARSE(datarefobj);
CREATE_PARSER_TRY_PARSE(arg_local_obj);
CREATE_PARSER_TRY_PARSE(type_1_opcode);
CREATE_PARSER_TRY_PARSE(type_2_opcode);
CREATE_PARSER_TRY_PARSE(computationaldata);

#define PARSER_TRY_PARSE(name) & acpi_aml_parser_try_parse_ ## name

acpi_aml_parser acpi_aml_parsers_termobj[] = {
	PARSER_TRY_PARSE(defalias),
	PARSER_TRY_PARSE(defname),
	PARSER_TRY_PARSE(defscope),
	PARSER_TRY_PARSE(namedobj),
	PARSER_TRY_PARSE(type_1_opcode),
	PARSER_TRY_PARSE(type_2_opcode),
	NULL
};

acpi_aml_parser acpi_aml_parsers_termarg[] = {
	PARSER_TRY_PARSE(type_2_opcode),
	PARSER_TRY_PARSE(dataobj),
	PARSER_TRY_PARSE(arg_local_obj),
	NULL
};

acpi_aml_parser acpi_aml_parsers_namedobj[] = {
	NULL
};

acpi_aml_parser acpi_aml_parsers_dataobj[] = {
	PARSER_TRY_PARSE(computationaldata),
	NULL
};

acpi_aml_parser acpi_aml_parsers_datarefobj[] = {
	&acpi_aml_parser_try_parse_dataobj,
	NULL
};

acpi_aml_parser acpi_aml_parsers_type_1_opcode[] = {
	NULL
};

acpi_aml_parser acpi_aml_parsers_type_2_opcode[] = {
	NULL
};

acpi_aml_parser acpi_aml_parsers_arg_local_obj[] = {
	NULL
};

acpi_aml_parser acpi_aml_parsers_computationaldata[] = {
	NULL
};


#define CREATE_PARSER_ITER(name) \
	uint32_t acpi_aml_parser_try_parse_ ## name(uint8_t * *buffer, uint32_t * length, acpi_aml_object_t * *data){ \
		uint32_t idx = 0; \
		uint32_t parsed = 0; \
		while(acpi_aml_parsers_ ## name[idx] != NULL) { \
			parsed = acpi_aml_parsers_ ## name[idx](buffer, length, data); \
			if(parsed > 0) { \
				break; \
			} \
			idx++; \
		} \
		return parsed; \
	}

CREATE_PARSER_ITER(termarg);
CREATE_PARSER_ITER(termobj);
CREATE_PARSER_ITER(namedobj);
CREATE_PARSER_ITER(dataobj);
CREATE_PARSER_ITER(datarefobj);
CREATE_PARSER_ITER(type_1_opcode);
CREATE_PARSER_ITER(type_2_opcode);
CREATE_PARSER_ITER(arg_local_obj);
CREATE_PARSER_ITER(computationaldata);

uint32_t acpi_aml_parser_try_parse_defalias(uint8_t** buffer, uint32_t* length, acpi_aml_object_t** data){
	UNUSED(buffer);
	UNUSED(length);
	UNUSED(data);
	return 0;
}

uint32_t acpi_aml_parser_try_parse_defname(uint8_t** buffer, uint32_t* length, acpi_aml_object_t** data){
	UNUSED(buffer);
	UNUSED(length);
	UNUSED(data);
	return 0;
}

uint32_t acpi_aml_parser_try_parse_defscope(uint8_t** buffer, uint32_t* length, acpi_aml_object_t** data){
	UNUSED(buffer);
	UNUSED(length);
	UNUSED(data);
	return 0;
}

uint32_t acpi_aml_parser_try_parse_termlist(uint8_t** buffer, uint32_t* length, acpi_aml_object_t** data){
	uint32_t total_parsed = 0;
	while((*length) > 0) {
		uint32_t parsed = acpi_aml_parser_try_parse_termobj(buffer, length, data);
		total_parsed += parsed;
	}
	return total_parsed;
}

uint32_t acpi_aml_parser_try_parse_termarglist(uint8_t** buffer, uint32_t* length, acpi_aml_object_t** data){
	uint32_t total_parsed = 0;
	while((*length) > 0) {
		uint32_t parsed = acpi_aml_parser_try_parse_termarg(buffer, length, data);
		total_parsed += parsed;
	}
	return total_parsed;
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

	//uint8_t* aml = aml_data + sizeof(acpi_sdt_header_t);
	//size -= sizeof(acpi_sdt_header_t);


	memory_free(aml_data);
	dump_ram("tmp/mem.dump");
	print_success("Tests are passed");
	return 0;
}
