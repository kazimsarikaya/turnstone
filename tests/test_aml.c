#include "setup.h"
#include <acpi.h>
#include <linkedlist.h>
#include <strings.h>
#include <utils.h>

typedef struct acpi_aml_scope {
	struct acpi_aml_scope* parent;
	char_t* name;
	linkedlist_t* members;
}acpi_aml_scope_t;

typedef struct {
	memory_heap_t* heap;
	uint8_t* location;
	size_t remining;
	acpi_aml_scope_t* scope;
} acpi_aml_state_t;

typedef enum {
	ACPI_OBJECT_TYPE_LOCAL0,
	ACPI_OBJECT_TYPE_LOCAL1,
	ACPI_OBJECT_TYPE_LOCAL2,
	ACPI_OBJECT_TYPE_LOCAL3,
	ACPI_OBJECT_TYPE_LOCAL4,
	ACPI_OBJECT_TYPE_LOCAL5,
	ACPI_OBJECT_TYPE_LOCAL6,
	ACPI_OBJECT_TYPE_LOCAL7,

	ACPI_OBJECT_TYPE_ARG0,
	ACPI_OBJECT_TYPE_ARG1,
	ACPI_OBJECT_TYPE_ARG2,
	ACPI_OBJECT_TYPE_ARG3,
	ACPI_OBJECT_TYPE_ARG4,
	ACPI_OBJECT_TYPE_ARG5,
	ACPI_OBJECT_TYPE_ARG6,

	ACPI_OBJECT_TYPE_NUMBER,

	ACPI_OBJECT_TYPE_MUTEX,
	ACPI_OBJECT_TYPE_EVENT,
	ACPI_OBJECT_TYPE_OPERATIONREGION,
} acpi_aml_object_type_t;

typedef struct {
	acpi_aml_object_type_t type;
	union {
		struct {
			char_t* name;
			uint8_t flags;
		} mutex;
		struct {
			char_t* name;
		} event;

		int64_t number;

		struct {
			char_t* name;
			uint8_t type;
			size_t offset;
			size_t length;
		} operation_region;
	};
} acpi_aml_object_t;

int8_t acpi_aml_parse_termlist(acpi_aml_state_t* state);
int8_t acpi_aml_parse_op_extended(acpi_aml_state_t* state);
int8_t acpi_aml_parse_namestring(acpi_aml_state_t* state, char_t** namestring);
int8_t acpi_aml_parse_termarg(acpi_aml_state_t* state, acpi_aml_object_t** arg);


int8_t acpi_aml_parse_termarg(acpi_aml_state_t* state, acpi_aml_object_t** arg){
	if(state->remining <= 0) {
		return -1;
	}

	uint8_t opcode = *(state->location);
	state->location++;
	state->remining--;
	switch (opcode) {
	case 0x60:
		*arg = memory_malloc_ext(state->heap, sizeof(acpi_aml_object_t), 0x0);
		(*arg)->type = ACPI_OBJECT_TYPE_LOCAL0;
		break;
	case 0x61:
		*arg = memory_malloc_ext(state->heap, sizeof(acpi_aml_object_t), 0x0);
		(*arg)->type = ACPI_OBJECT_TYPE_LOCAL1;
		break;
	case 0x62:
		*arg = memory_malloc_ext(state->heap, sizeof(acpi_aml_object_t), 0x0);
		(*arg)->type = ACPI_OBJECT_TYPE_LOCAL2;
		break;
	case 0x63:
		*arg = memory_malloc_ext(state->heap, sizeof(acpi_aml_object_t), 0x0);
		(*arg)->type = ACPI_OBJECT_TYPE_LOCAL3;
		break;
	case 0x64:
		*arg = memory_malloc_ext(state->heap, sizeof(acpi_aml_object_t), 0x0);
		(*arg)->type = ACPI_OBJECT_TYPE_LOCAL4;
		break;
	case 0x65:
		*arg = memory_malloc_ext(state->heap, sizeof(acpi_aml_object_t), 0x0);
		(*arg)->type = ACPI_OBJECT_TYPE_LOCAL5;
		break;
	case 0x66:
		*arg = memory_malloc_ext(state->heap, sizeof(acpi_aml_object_t), 0x0);
		(*arg)->type = ACPI_OBJECT_TYPE_LOCAL6;
		break;
	case 0x67:
		*arg = memory_malloc_ext(state->heap, sizeof(acpi_aml_object_t), 0x0);
		(*arg)->type = ACPI_OBJECT_TYPE_LOCAL7;
		break;
	case 0x68:
		*arg = memory_malloc_ext(state->heap, sizeof(acpi_aml_object_t), 0x0);
		(*arg)->type = ACPI_OBJECT_TYPE_ARG0;
		break;
	case 0x69:
		*arg = memory_malloc_ext(state->heap, sizeof(acpi_aml_object_t), 0x0);
		(*arg)->type = ACPI_OBJECT_TYPE_ARG1;
		break;
	case 0x6A:
		*arg = memory_malloc_ext(state->heap, sizeof(acpi_aml_object_t), 0x0);
		(*arg)->type = ACPI_OBJECT_TYPE_ARG2;
		break;
	case 0x6B:
		*arg = memory_malloc_ext(state->heap, sizeof(acpi_aml_object_t), 0x0);
		(*arg)->type = ACPI_OBJECT_TYPE_ARG3;
		break;
	case 0x6C:
		*arg = memory_malloc_ext(state->heap, sizeof(acpi_aml_object_t), 0x0);
		(*arg)->type = ACPI_OBJECT_TYPE_ARG4;
		break;
	case 0x6D:
		*arg = memory_malloc_ext(state->heap, sizeof(acpi_aml_object_t), 0x0);
		(*arg)->type = ACPI_OBJECT_TYPE_ARG5;
		break;
	case 0x6E:
		*arg = memory_malloc_ext(state->heap, sizeof(acpi_aml_object_t), 0x0);
		(*arg)->type = ACPI_OBJECT_TYPE_ARG6;
		break;


	case 0x0A: // byte const
		*arg = memory_malloc_ext(state->heap, sizeof(acpi_aml_object_t), 0x0);
		(*arg)->type = ACPI_OBJECT_TYPE_NUMBER;
		int8_t* b_data = (int8_t*)(state->location);
		(*arg)->number = *b_data;
		state->location++;
		state->remining--;
		break;
	case 0x0B: // word const
		*arg = memory_malloc_ext(state->heap, sizeof(acpi_aml_object_t), 0x0);
		(*arg)->type = ACPI_OBJECT_TYPE_NUMBER;
		int16_t* w_data = (int16_t*)(state->location);
		(*arg)->number = *w_data;
		state->location += 2;
		state->remining -= 2;
		break;
	case 0x0C: // dword const
		*arg = memory_malloc_ext(state->heap, sizeof(acpi_aml_object_t), 0x0);
		(*arg)->type = ACPI_OBJECT_TYPE_NUMBER;
		int32_t* dw_data = (int32_t*)(state->location);
		(*arg)->number = *dw_data;
		state->location += 4;
		state->remining -= 4;
		break;
	case 0x0E: // qword const
		*arg = memory_malloc_ext(state->heap, sizeof(acpi_aml_object_t), 0x0);
		(*arg)->type = ACPI_OBJECT_TYPE_NUMBER;
		int64_t* qw_data = (int64_t*)(state->location);
		(*arg)->number = *qw_data;
		state->location += 8;
		state->remining -= 8;
		break;

	}
	return 0;
}

int8_t acpi_aml_parse_namestring(acpi_aml_state_t* state, char_t** namestring){
	printf("name string\n");
	int32_t strlen = 4;
	for(size_t i = 0;; i++) {
		if(state->location[i] == '^') {
			strlen++;
		} else {
			break;
		}
	}
	*namestring = memory_malloc_ext(state->heap, sizeof(char_t) * strlen + 1, 0x0);
	memory_memcopy(state->location, *namestring, strlen);
	state->location += strlen;
	state->remining -= strlen;
	return 0;
}

int8_t acpi_aml_parse_termlist(acpi_aml_state_t* state) {
	if(state->remining <= 0) {
		return -1;
	}

	int8_t res = 0;

	uint8_t opcode = *(state->location);
	state->location++;
	state->remining--;

	switch(opcode) {
	case 0x5B:
		printf("extended op started\n");
		res = acpi_aml_parse_op_extended(state);
		printf("extended op consumed\n");
		break;
	default:
		printf("unimplemented\n");
		return -1;
		break;
	};
	return res;
}


int8_t acpi_aml_parse_op_extended(acpi_aml_state_t* state){
	if(state->remining <= 0) {
		return -1;
	}
	char_t* namestring = NULL;
	uint8_t opcode = *(state->location);
	state->location++;
	state->remining--;

	acpi_aml_object_t* tmp_object = NULL;

	switch (opcode) {
	case 0x01:
		printf("mutex op\n");
		acpi_aml_parse_namestring(state, &namestring);
		uint8_t data = *(state->location);
		acpi_aml_object_t* m = memory_malloc_ext(state->heap, sizeof(acpi_aml_object_t), 0x0);
		m->mutex.name = namestring;
		m->mutex.flags = data;
		state->location++;
		state->remining--;
		linkedlist_list_insert(state->scope, m);
		break;
	case 0x02:
		printf("event op\n");
		acpi_aml_parse_namestring(state, &namestring);
		return -1;
		break;
	case 0x12:
		printf("condrefof op\n");
		return -1;
		break;
	case 0x13:
		printf("create field op\n");
		return -1;
		break;
	case 0x1F:
		printf("load table op\n");
		return -1;
		break;
	case 0x20:
		printf("load op\n");
		return -1;
		break;
	case 0x21:
		printf("stall op\n");
		return -1;
		break;
	case 0x22:
		printf("sleep op\n");
		return -1;
		break;
	case 0x23:
		printf("acquire op\n");
		return -1;
		break;
	case 0x24:
		printf("signal op\n");
		return -1;
		break;
	case 0x25:
		printf("wait op\n");
		return -1;
		break;
	case 0x26:
		printf("reset op\n");
		return -1;
		break;
	case 0x27:
		printf("release op\n");
		return -1;
		break;
	case 0x28:
		printf("from bcd op\n");
		return -1;
		break;
	case 0x29:
		printf("to bcd op\n");
		return -1;
		break;
	case 0x30:
		printf("revision op\n");
		return -1;
		break;
	case 0x31:
		printf("debug op\n");
		return -1;
		break;
	case 0x32:
		printf("fatal op\n");
		return -1;
		break;
	case 0x33:
		printf("timer op\n");
		return -1;
		break;
	case 0x80:
		printf("op region op\n");
		acpi_aml_object_t* op_region = memory_malloc_ext(state->heap, sizeof(acpi_aml_object_t), 0x0);
		acpi_aml_parse_namestring(state, &namestring);
		op_region->operation_region.name = namestring;
		uint8_t op_type = *(state->location);
		op_region->operation_region.type = op_type;
		state->location++;
		state->remining--;
		acpi_aml_parse_termarg(state, &tmp_object);
		op_region->operation_region.offset = tmp_object->number;
		memory_free_ext(state->heap, tmp_object);
		acpi_aml_parse_termarg(state, &tmp_object);
		op_region->operation_region.length = tmp_object->number;
		memory_free_ext(state->heap, tmp_object);
		linkedlist_list_insert(state->scope, op_region);
		printf("op region consumed\n");
		break;
	case 0x81:
		printf("field op\n");
		return -1;
		break;
	case 0x82:
		printf("device op\n");
		return -1;
		break;
	case 0x83:
		printf("processor op\n");
		return -1;
		break;
	case 0x84:
		printf("power res op\n");
		return -1;
		break;
	case 0x85:
		printf("thermal zone op\n");
		return -1;
		break;
	case 0x86:
		printf("index field op\n");
		return -1;
		break;
	case 0x87:
		printf("bank field op\n");
		return -1;
		break;
	case 0x88:
		printf("data region op\n");
		return -1;
		break;
	default:
		printf("undefined extended op\n");
		return -1;
		break;
	}
	return 0;
}

uint32_t main(uint32_t argc, char_t** argv){
	setup_ram();
	if(argc != 4) {
		printf("Usage: %s <madt> <fadt> <dsdt>\n", *argv);
		return -1;
	}

	FILE* fp;
	long size;

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
	uint8_t* madt_data = memory_malloc(sizeof(uint8_t) * size);
	if(madt_data == NULL) {
		print_error("cannot malloc madt data area");
		return -3;
	}
	fread(madt_data, 1, size, fp);
	fclose(fp);
	print_success("file readed");

	acpi_sdt_header_t* madt_hdr = (acpi_sdt_header_t*)madt_data;
	linkedlist_t entries = acpi_get_apic_table_entries(madt_hdr);
	iterator_t* iter = linkedlist_iterator_create(entries);
	while(iter->end_of_iterator(iter) != 0) {
		acpi_table_madt_entry_t* e = iter->get_item(iter);
		printf("acpi entry type: %i entry length: %x\n", e->info.type, e->info.length);
		if(e->info.type == ACPI_MADT_ENTRY_TYPE_IOAPIC) {
			printf("ioapic: %x %x %x\n",
			       e->ioapic.ioapic_id,
			       e->ioapic.address,
			       e->ioapic.global_system_interrupt_base);
		}
		if(e->info.type == ACPI_MADT_ENTRY_TYPE_INTERRUPT_SOURCE_OVERRIDE) {
			printf("int src override: %x %x %x\n",
			       e->interrupt_source_override.bus_source,
			       e->interrupt_source_override.irq_source,
			       e->interrupt_source_override.global_system_interrupt);
		}
		iter = iter->next(iter);
	}
	iter->destroy(iter);
	acpi_table_madt_entry_t* t_e = linkedlist_delete_at_position(entries, 0);
	if(t_e->info.type == ACPI_MADT_ENTRY_TYPE_LOCAL_APIC_ADDRESS) {
		memory_free_ext(NULL, t_e);
	}
	linkedlist_destroy(entries);
	memory_free(madt_data);

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
	uint8_t* fadt_data = memory_malloc(sizeof(uint8_t) * size);
	if(fadt_data == NULL) {
		print_error("cannot malloc aml data area");
		return -3;
	}
	fread(fadt_data, 1, size, fp);
	fclose(fp);
	print_success("file readed");
	acpi_table_fadt_t* fadt = (acpi_table_fadt_t*)fadt_data;
	if(acpi_validate_checksum((acpi_sdt_header_t*)fadt) != 0) {
		print_error("fadt incorrect");
		return -4;
	}
	printf("dsdt address 32: 0x%x\n", fadt->dsdt_address_32bit);
	printf("dsdt address 64: 0x%x\n", fadt->dsdt_address_64bit);


	printf("reset reg type: %i access size: %i address 0x%x\n",
	       fadt->reset_reg.address_space,
	       fadt->reset_reg.access_size,
	       fadt->reset_reg.address);
	printf("pm1a event type: %i access size: %i address 0x%x\n",
	       fadt->pm_1a_event_block_address_64bit.address_space,
	       fadt->pm_1a_event_block_address_64bit.access_size,
	       fadt->pm_1a_event_block_address_64bit.address);
	printf("pm1b event type: %i access size: %i address 0x%x\n",
	       fadt->pm_1b_event_block_address_64bit.address_space,
	       fadt->pm_1b_event_block_address_64bit.access_size,
	       fadt->pm_1b_event_block_address_64bit.address);
	printf("pm1a control type: %i access size: %i address 0x%x\n",
	       fadt->pm_1a_control_block_address_64bit.address_space,
	       fadt->pm_1a_control_block_address_64bit.access_size,
	       fadt->pm_1a_control_block_address_64bit.address);
	printf("pm1b control type: %i access size: %i address 0x%x\n",
	       fadt->pm_1b_control_block_address_64bit.address_space,
	       fadt->pm_1b_control_block_address_64bit.access_size,
	       fadt->pm_1b_control_block_address_64bit.address);
	printf("pm2 control type: %i access size: %i address 0x%x\n",
	       fadt->pm_2_control_block_address_64bit.address_space,
	       fadt->pm_2_control_block_address_64bit.access_size,
	       fadt->pm_2_control_block_address_64bit.address);
	printf("pm timer type: %i access size: %i address 0x%x\n",
	       fadt->pm_timer_block_address_64bit.address_space,
	       fadt->pm_timer_block_address_64bit.access_size,
	       fadt->pm_timer_block_address_64bit.address);
	printf("gpe0 type: %i access size: %i address 0x%x\n",
	       fadt->gpe0_block_address_64bit.address_space,
	       fadt->gpe0_block_address_64bit.access_size,
	       fadt->gpe0_block_address_64bit.address);
	printf("gpe1 type: %i access size: %i address 0x%x\n",
	       fadt->gpe1_block_address_64bit.address_space,
	       fadt->gpe1_block_address_64bit.access_size,
	       fadt->gpe1_block_address_64bit.address);


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
		print_error("cannot malloc aml data area");
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

	char_t* global_scope_name = memory_malloc_ext(NULL, sizeof(char_t) * 7, 0x0);
	memory_memcopy("GLOBAL", global_scope_name, 6);
	acpi_aml_scope_t* scope = memory_malloc_ext(NULL, sizeof(acpi_aml_state_t), 0x0);
	scope->parent = NULL;
	scope->name = global_scope_name;
	scope->members = linkedlist_create_list_with_heap(NULL);

	acpi_aml_state_t* state = memory_malloc_ext(NULL, sizeof(acpi_aml_state_t), 0x0);
	state->heap = NULL;
	state->remining = size;
	state->location = aml;
	state->scope = scope;

	while(state->remining > 0) {
		if(acpi_aml_parse_termlist(state) == -1) {
			break;
		}
	}

	fp = fopen("tmp/rmaml.hex", "w");
	fwrite(state->location, 1, state->remining, fp);
	fclose(fp);

	memory_free(state);
	memory_free(aml_data);

	return 0;
}
