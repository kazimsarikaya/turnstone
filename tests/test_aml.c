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
	size_t remaining;
	acpi_aml_scope_t* scope;
} acpi_aml_state_t;

typedef enum {
	ACPI_OBJECT_TYPE_UNDEFINED = -1,

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

	ACPI_OBJECT_TYPE_DEBUG,

	ACPI_OBJECT_TYPE_NAMESTRING,
	ACPI_OBJECT_TYPE_INDEX,
	ACPI_OBJECT_TYPE_REFOF,
	ACPI_OBJECT_TYPE_DEREFOF,
	ACPI_OBJECT_TYPE_USERTERM,

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
		char_t* string;

		struct {
			char_t* name;
			uint8_t type;
			size_t offset;
			size_t length;
		} operation_region;
	};
} acpi_aml_object_t;

int64_t acpi_aml_parse_termlist(acpi_aml_state_t* state, uint64_t remaining);
int64_t acpi_aml_parse_op_extended(acpi_aml_state_t* state, acpi_aml_object_t** obj);
int64_t acpi_aml_parse_termarg(acpi_aml_state_t* state, acpi_aml_object_t** obj);
int64_t acpi_aml_parse_argobj(acpi_aml_state_t* state, uint8_t opcode, acpi_aml_object_t** obj);
int64_t acpi_aml_parse_localobj(acpi_aml_state_t* state, uint8_t opcode, acpi_aml_object_t** obj);
int64_t acpi_aml_parse_op_type_2(acpi_aml_state_t* state, uint8_t opcode, acpi_aml_object_t** obj);
void* acpi_aml_evaluate_object(acpi_aml_object_t* obj);
int64_t acpi_aml_parse_package_length(acpi_aml_state_t* state);
int64_t acpi_aml_parse_namestring(acpi_aml_state_t* state, acpi_aml_object_t** obj);
int64_t acpi_aml_parse_supername(acpi_aml_state_t* state, acpi_aml_object_t** obj);
int64_t acpi_aml_parse_target(acpi_aml_state_t* state, acpi_aml_object_t** obj);


int64_t acpi_aml_parse_namestring(acpi_aml_state_t* state, acpi_aml_object_t** obj){
	printf("name string\n");
	int64_t strlen = 4;
	for(size_t i = 0;; i++) {
		if(state->location[i] == '^') {
			strlen++;
		} else {
			break;
		}
	}

	printf("string len: %i\n", strlen);
	char_t* namestring = memory_malloc_ext(state->heap, sizeof(char_t) * strlen + 1, 0x0);
	memory_memcopy(state->location, namestring, strlen);
	*obj = memory_malloc_ext(state->heap, sizeof(acpi_aml_object_t), 0x0);
	(*obj)->type = ACPI_OBJECT_TYPE_NAMESTRING;
	(*obj)->string = namestring;
	state->location += strlen;
	state->remaining -= strlen;
	printf("parsed name: %s\n", namestring);
	return strlen;
}

int64_t acpi_aml_parse_supername(acpi_aml_state_t* state, acpi_aml_object_t** obj){
	UNUSED(state);
	UNUSED(obj);
	return -1;
}

int64_t acpi_aml_parse_target(acpi_aml_state_t* state, acpi_aml_object_t** obj) {
	if(state->remaining <= 0) {
		return -1;
	}
	uint8_t opcode = *state->location;
	if(opcode == 0x00) {
		state->remaining--;
		state->location++;
		*obj = memory_malloc_ext(state->heap, sizeof(acpi_aml_object_t), 0x0);
		(*obj)->string = memory_malloc_ext(state->heap, sizeof(char_t), 0x0);
		return 0;
	}
	return acpi_aml_parse_supername(state, obj);
}


int64_t acpi_aml_parse_op_type_2(acpi_aml_state_t* state, uint8_t opcode, acpi_aml_object_t** obj) {
	UNUSED(state);
	UNUSED(obj);
	int64_t res = 0;
	switch (opcode) {
	case 0x11: // DefBuffer
		printf("buffer op\n");
		return -1;
	case 0x12: // DefPackage
		printf("package op\n");
		return -1;
	case 0x13: // DefVarPackage
		printf("var package op\n");
		return -1;
	case 0x70:  // DefStore
		printf("store op\n");
		return -1;
	case 0x71:  // DefRefOf
		printf("ref of op\n");
		return -1;
	case 0x72: // DefAdd
		printf("add op\n");
		return -1;
	case 0x73: // DefConcat
		printf("concat op\n");
		return -1;
	case 0x74: // DefSubtract
		printf("substract op\n");
		return -1;
	case 0x75: // DefIncrement
		printf("increment op\n");
		return -1;
	case 0x76: // DefDecrement
		printf("decrement op\n");
		return -1;
	case 0x77: // DefMultiply
		printf("multiply op\n");
		return -1;
	case 0x78: // DefDivide
		printf("divide op\n");
		return -1;
	case 0x79:  // DefShiftLeft
		printf("shift left op\n");
		return -1;
	case 0x7A:  // DefShiftRight
		printf("shift right op\n");
		return -1;
	case 0x7B: // DefAnd
		printf("and op\n");
		return -1;
	case 0x7C: // DefNAnd
		printf("nand op\n");
		return -1;
	case 0x7D: // DefOr
		printf("or op\n");
		return -1;
	case 0x7E: // DefNOr
		printf("nor op\n");
		return -1;
	case 0x7F: // DefXOr
		printf("xor op\n");
		return -1;
	case 0x80: // DefNot
		printf("not op\n");
		return -1;
	case 0x81: // DefFindSetLeftBit
		printf("find set left bit op\n");
		return -1;
	case 0x82: // DefFindSetRightBit
		printf("find set right bit op\n");
		return -1;
	case 0x83: // DefDerefOf
		printf("derefof op\n");
		return -1;
	case 0x84: // DefConcatRes
		printf("concat res op\n");
		return -1;
	case 0x85: // DefMod
		printf("mod op\n");
		return -1;
	case 0x87:  // DefSizeOf
		printf("sizeof op\n");
		return -1;
	case 0x88: // DefIndex
		printf("index op\n");
		return -1;
	case 0x89: // DefMatch
		printf("match op\n");
		return -1;
	case 0x8E: // DefObjectType
		printf("object type op\n");
		return -1;
	case 0x90: // DefLAnd
		printf("l and op\n");
		return -1;
	case 0x91: // DefLOr
		printf("l or op\n");
		return -1;
	case 0x92: // DefLGreaterEqual|DefLLessEqual|DefLNotEqual|DefLNot
		switch (*state->location) {
		case 0x93: // DefLGreaterEqual
			state->location++;
			state->remaining--;
			printf("l greater op\n");
			break;
		case 0x94: // DefLLessEqual
			state->location++;
			state->remaining--;
			printf("l less equal op\n");
			break;
		case 0x95: // DefLNotEqual
			state->location++;
			state->remaining--;
			printf("l not equal op\n");
			break;
		default: // DefLNot
			printf("l not op\n");
			break;
		}
		return -1;
	case 0x93: // DefLEqual
		printf("l equal op\n");
		return -1;
	case 0x94: // DefLGreater
		printf("l greater op\n");
		return -1;
	case 0x95: // DefLLess
		printf("l less op\n");
		return -1;
	case 0x96: // DefToBuffer
		printf("to buffer op\n");
		return -1;
	case 0x97: // DefToDecimalString
		printf("to decimal string op\n");
		return -1;
	case 0x98: // DefToHexString
		printf("to hex string op\n");
		return -1;
	case 0x99: // DefToInteger
		printf("to integer op\n");
		return -1;
	case 0x9C: // DefToString
		printf("to string op\n");
		return -1;
	case 0x9D: // DefCopyObject
		printf("copy object op\n");
		return -1;
	case 0x9E: // DefMid
		printf("mid op\n");
		return -1;
	default: // try MethodInvocation
		res = -1;
		break;
	}
	return res;
}

int64_t acpi_aml_parse_package_length(acpi_aml_state_t* state){
	uint64_t res = 0;
	uint8_t lead = *state->location;
	uint8_t diff = 1;
	state->location++;
	state->remaining--;
	uint8_t bc = lead >> 6;
	if(bc == 0) {
		return lead - diff;
	}
	res = lead & 0x4;
	uint8_t shift = 4;
	while(bc > 0) {
		uint16_t tmp = *state->location;
		state->location++;
		state->remaining--;
		res += (tmp << shift);
		shift += 8;
		bc--;
		diff++;
	}
	return res - diff;
}

void* acpi_aml_evaluate_object(acpi_aml_object_t* obj){
	switch (obj->type) {
	case ACPI_OBJECT_TYPE_NAMESTRING:
		return obj->string;
	case ACPI_OBJECT_TYPE_NUMBER:
		return &obj->number;
	default:
		return NULL;
	}
	return NULL;
}

int64_t acpi_aml_parse_localobj(acpi_aml_state_t* state, uint8_t opcode, acpi_aml_object_t** obj) {
	acpi_aml_object_type_t type = ACPI_OBJECT_TYPE_UNDEFINED;
	switch (opcode) {
	case 0x60:
		type = ACPI_OBJECT_TYPE_LOCAL0;
		break;
	case 0x61:
		type = ACPI_OBJECT_TYPE_LOCAL1;
		break;
	case 0x62:
		type = ACPI_OBJECT_TYPE_LOCAL2;
		break;
	case 0x63:
		type = ACPI_OBJECT_TYPE_LOCAL3;
		break;
	case 0x64:
		type = ACPI_OBJECT_TYPE_LOCAL4;
		break;
	case 0x65:
		type = ACPI_OBJECT_TYPE_LOCAL5;
		break;
	case 0x66:
		type = ACPI_OBJECT_TYPE_LOCAL6;
		break;
	case 0x67:
		type = ACPI_OBJECT_TYPE_LOCAL7;
		break;
	default:
		*obj = NULL;
		return -1;
	}
	*obj = memory_malloc_ext(state->heap, sizeof(acpi_aml_object_t), 0x0);
	(*obj)->type = type;
	return 0;
}

int64_t acpi_aml_parse_argobj(acpi_aml_state_t* state, uint8_t opcode, acpi_aml_object_t** obj) {
	acpi_aml_object_type_t type = ACPI_OBJECT_TYPE_UNDEFINED;
	switch (opcode) {
	case 0x68:
		type = ACPI_OBJECT_TYPE_ARG0;
		break;
	case 0x69:
		type = ACPI_OBJECT_TYPE_ARG1;
		break;
	case 0x6A:
		type = ACPI_OBJECT_TYPE_ARG2;
		break;
	case 0x6B:
		type = ACPI_OBJECT_TYPE_ARG3;
		break;
	case 0x6C:
		type = ACPI_OBJECT_TYPE_ARG4;
		break;
	case 0x6D:
		type = ACPI_OBJECT_TYPE_ARG5;
		break;
	case 0x6E:
		type = ACPI_OBJECT_TYPE_ARG6;
		break;
	default:
		*obj = NULL;
		return -1;
	}
	*obj = memory_malloc_ext(state->heap, sizeof(acpi_aml_object_t), 0x0);
	(*obj)->type = type;
	return 0;
}


int64_t acpi_aml_parse_termarg(acpi_aml_state_t* state, acpi_aml_object_t** obj){
	if(state->remaining <= 0) {
		return -1;
	}
	int8_t res = 0;
	uint8_t opcode = *(state->location);
	state->location++;
	state->remaining--;
	switch (opcode) {
	// ConstObj
	case 0x00: // ZeroOP
		printf("zero const op\n");
		return -1;
	case 0x01: // OneOp
		printf("one const op\n");
		return -1;
	case 0xFF: // OnesOp
		printf("ones const op\n");
		return -1;


	// LocalObj
	case 0x60:
	case 0x61:
	case 0x62:
	case 0x63:
	case 0x64:
	case 0x65:
	case 0x66:
	case 0x67:
		res = acpi_aml_parse_localobj(state, opcode, obj);
		break;
	// ArgObj
	case 0x68:
	case 0x69:
	case 0x6A:
	case 0x6B:
	case 0x6C:
	case 0x6D:
	case 0x6E:
		res = acpi_aml_parse_argobj(state, opcode, obj);
		break;


	case 0x0A: // byte const
		*obj = memory_malloc_ext(state->heap, sizeof(acpi_aml_object_t), 0x0);
		(*obj)->type = ACPI_OBJECT_TYPE_NUMBER;
		int8_t* b_data = (int8_t*)(state->location);
		(*obj)->number = *b_data;
		state->location++;
		state->remaining--;
		break;
	case 0x0B: // word const
		*obj = memory_malloc_ext(state->heap, sizeof(acpi_aml_object_t), 0x0);
		(*obj)->type = ACPI_OBJECT_TYPE_NUMBER;
		int16_t* w_data = (int16_t*)(state->location);
		(*obj)->number = *w_data;
		state->location += 2;
		state->remaining -= 2;
		break;
	case 0x0C: // dword const
		*obj = memory_malloc_ext(state->heap, sizeof(acpi_aml_object_t), 0x0);
		(*obj)->type = ACPI_OBJECT_TYPE_NUMBER;
		int32_t* dw_data = (int32_t*)(state->location);
		(*obj)->number = *dw_data;
		state->location += 4;
		state->remaining -= 4;
		break;
	case 0x0E: // qword const
		*obj = memory_malloc_ext(state->heap, sizeof(acpi_aml_object_t), 0x0);
		(*obj)->type = ACPI_OBJECT_TYPE_NUMBER;
		int64_t* qw_data = (int64_t*)(state->location);
		(*obj)->number = *qw_data;
		state->location += 8;
		state->remaining -= 8;
		break;

	case 0x0D: // StringOp
		printf("string op\n");
		return -1;


	case 0x5B:
		res = acpi_aml_parse_op_extended(state, obj);
		break;

	default:
		res = acpi_aml_parse_op_type_2(state, opcode, obj);
		break;
	}
	return res;
}

int64_t acpi_aml_parse_termlist(acpi_aml_state_t* state, uint64_t remaining) {
	int64_t res = 0;
	while(remaining > 0) {
		acpi_aml_object_t* tmp_object = NULL;

		uint8_t opcode = *(state->location);
		state->location++;
		state->remaining--;
		remaining--;

		switch(opcode) {
		// NameSpaceModifierObj
		case 0x06: // DefAlias
			printf("alias op\n");
			return -1;
		case 0x08: // DefName
			printf("name op\n");
			return -1;
		case 0x10: // DefScope
			printf("scope op\n");
			return -1;

		// NamedObj
		case 0x8D: // DefCreateBitField
			printf("create bit field op\n");
			return -1;
		case 0x8C: // DefCreateByteField
			printf("create byte field op\n");
			return -1;
		case 0x8B: // DefCreateWordField
			printf("create word field op\n");
			return -1;
		case 0x8A: // DefCreateDWordField
			printf("create dword field op\n");
			return -1;
		case 0x8F: // DefCreateQWordField
			printf("create qword field op\n");
			return -1;
		case 0x15: // DefExternal
			printf("external op\n");
			return -1;
		case 0x14: // DefMethod
			printf("method op\n");
			return -1;

		// Type1Opcode
		case 0x86: // DefNotify
			printf("notify op\n");
			return -1;
		case 0x9F: // DefContinue
			printf("continue op\n");
			return -1;
		case 0xA0: // DefIfElse extra check for 0xA1
			printf("if-else op\n");
			return -1;
		case 0xA2: // DefWhile
			printf("while op\n");
			return -1;
		case 0xA3: // DefNoop
			printf("noop op\n");
			return -1;
		case 0xA4: // DefReturn
			printf("return op\n");
			return -1;
		case 0xA5: // DefBreak
			printf("break op\n");
			return -1;
		case 0xCC: // DefBreakPoint
			printf("breakpoint op\n");
			return -1;

		// Extended Ops (type1 and type2 covarage)
		case 0x5B:
			printf("extended op started\n");
			res = acpi_aml_parse_op_extended(state, &tmp_object);
			printf("extended op consumed\n");
			break;

		default:
			res = acpi_aml_parse_op_type_2(state, opcode, &tmp_object);
			break;
		};
		if(res == -1) {
			break;
		} else {
			if(tmp_object != NULL) {
				linkedlist_list_insert(state->scope->members, tmp_object);
			}
		}
	}
	return res;
}


int64_t acpi_aml_parse_op_extended(acpi_aml_state_t* state, acpi_aml_object_t** obj){
	if(state->remaining <= 0) {
		return -1;
	}

	int64_t res = 0;

	uint8_t opcode = *(state->location);
	state->location++;
	state->remaining--;

	acpi_aml_object_t* tmp_object = NULL;
	uint64_t pkg_len = 0;
	uint8_t field_flags;
	char_t* region_name = NULL;


	switch (opcode) {
	case 0x01:
		printf("mutex op\n");
		acpi_aml_parse_namestring(state, &tmp_object);
		uint8_t data = *(state->location);
		*obj = memory_malloc_ext(state->heap, sizeof(acpi_aml_object_t), 0x0);
		(*obj)->mutex.name = (char_t*)acpi_aml_evaluate_object(tmp_object);
		(*obj)->mutex.flags = data;
		state->location++;
		state->remaining--;
		break;
	case 0x02:
		printf("event op\n");
		acpi_aml_parse_namestring(state, &tmp_object);
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
		*obj = memory_malloc_ext(state->heap, sizeof(acpi_aml_object_t), 0x0);
		(*obj)->type = ACPI_OBJECT_TYPE_OPERATIONREGION;
		res = acpi_aml_parse_namestring(state, &tmp_object);
		if(res == -1) {
			printf("cannot parse namestring\n");
			break;
		}
		(*obj)->operation_region.name = (char_t*)acpi_aml_evaluate_object(tmp_object);
		printf("namestring: %s\n", (*obj)->operation_region.name);
		memory_free_ext(state->heap, tmp_object);
		uint8_t op_type = *(state->location);
		(*obj)->operation_region.type = op_type;
		state->location++;
		state->remaining--;
		res = acpi_aml_parse_termarg(state, &tmp_object);
		if(res == -1) {
			printf("cannot parse offset\n");
			break;
		}
		(*obj)->operation_region.offset = *((uint64_t*)acpi_aml_evaluate_object(tmp_object));
		printf("offset: 0x%x\n", (*obj)->operation_region.offset);
		memory_free_ext(state->heap, tmp_object);
		res = acpi_aml_parse_termarg(state, &tmp_object);
		if(res == -1) {
			printf("cannot parse length\n");
			break;
		}
		(*obj)->operation_region.length = *((uint64_t*)acpi_aml_evaluate_object(tmp_object));
		printf("length: 0x%x\n", (*obj)->operation_region.length);
		memory_free_ext(state->heap, tmp_object);
		printf("op region consumed\n");
		break;
	case 0x81:
		printf("field op\n");
		pkg_len = acpi_aml_parse_package_length(state);
		printf("pkg len: %i\n", pkg_len);
		res = acpi_aml_parse_namestring(state, &tmp_object);
		if(res == -1) {
			break;
		}
		printf("namestring len: %i\n", res);
		pkg_len -= res;
		region_name = (char_t*)acpi_aml_evaluate_object(tmp_object);
		memory_free_ext(state->heap, tmp_object);
		field_flags = *(state->location);
		state->location++;
		state->remaining--;
		pkg_len--;
		printf("pkg len: %i regname: %s flags: %x\n", pkg_len, region_name, field_flags);
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
	return res;
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
	memory_free(fadt_data);

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

	if(acpi_aml_parse_termlist(state, state->remaining) == -1) {
		printf("parse failed\n");
	} else {
		printf("parse completed\n");
	}

	linkedlist_destroy_with_data(scope->members);
	memory_free(scope);
	memory_free(state);
	memory_free(aml_data);

	dump_ram("tmp/mem.dump");

	return 0;
}
