#ifndef ___ACPI_AML_RESOURCE_H
#define ___ACPI_AML_RESOURCE_H 0

#include <types.h>
#include <acpi/aml.h>

typedef enum {
	ACPI_AML_RESOURCE_SMALLITEM=0,
	ACPI_AML_RESOURCE_LARGEITEM,
}acpi_aml_resource_type_t;

typedef enum {
	ACPI_AML_RESOURCE_SMALLITEM_IRQ=4,
	ACPI_AML_RESOURCE_SMALLITEM_DMA=5,
	ACPI_AML_RESOURCE_SMALLITEM_START_DEPFUNC=6,
	ACPI_AML_RESOURCE_SMALLITEM_END_DEPFUNC=7,
	ACPI_AML_RESOURCE_SMALLITEM_IO=8,
	ACPI_AML_RESOURCE_SMALLITEM_FIXEDIO=9,
	ACPI_AML_RESOURCE_SMALLITEM_FIXEDDMA=10,
	ACPI_AML_RESOURCE_SMALLITEM_VENDOR=14,
	ACPI_AML_RESOURCE_SMALLITEM_ENDTAG=15,
}acpi_aml_resource_smallitem_name_t;

typedef enum {
	ACPI_AML_RESOURCE_LARGEITEM_24BIT_MEMORY_RANGE=1,
	ACPI_AML_RESOURCE_LARGEITEM_GENERIC_REGISTER=2,
	ACPI_AML_RESOURCE_LARGEITEM_VENDOR=4,
	ACPI_AML_RESOURCE_LARGEITEM_32BIT_MEMORY_RANGE=5,
	ACPI_AML_RESOURCE_LARGEITEM_32BIT_FIXEDMEMORY_RANGE=6,
	ACPI_AML_RESOURCE_LARGEITEM_DWORD_ADDRESS_SPACE=7,
	ACPI_AML_RESOURCE_LARGEITEM_WORD_ADDRESS_SPACE=8,
	ACPI_AML_RESOURCE_LARGEITEM_EXTENDED_INTERRUPT=9,
	ACPI_AML_RESOURCE_LARGEITEM_QWORD_ADDRESS_SPACE=10,
	ACPI_AML_RESOURCE_LARGEITEM_EXTENDED_ADDRESS_SPACE=11,
	ACPI_AML_RESOURCE_LARGEITEM_GPIO_CONNECTION=12,
	ACPI_AML_RESOURCE_LARGEITEM_PIN_FUNCTION=13,
	ACPI_AML_RESOURCE_LARGEITEM_GENERIC_SERIAL_BUS_CONNECTION=14,
	ACPI_AML_RESOURCE_LARGEITEM_PIN_CONFIGURATION=15,
	ACPI_AML_RESOURCE_LARGEITEM_PIN_GROUP=16,
	ACPI_AML_RESOURCE_LARGEITEM_PIN_GROUP_FUNCTION=17,
	ACPI_AML_RESOURCE_LARGEITEM_PIN_GROUP_CONFIGURATION=18,
}acpi_aml_resource_largeitem_name_t;

typedef enum {
	ACPI_AML_RESOURCE_WORD_ADDRESS_SPACE_TYPE_MEMORY=0,
	ACPI_AML_RESOURCE_WORD_ADDRESS_SPACE_TYPE_IO=1,
	ACPI_AML_RESOURCE_WORD_ADDRESS_SPACE_TYPE_BUS=2,
}acpi_aml_resource_word_address_space_type_t;

typedef enum {
	ACPI_AML_RESOURCE_ADDRESS_SPACE_ID_MEMORY=0,
	ACPI_AML_RESOURCE_ADDRESS_SPACE_ID_IO=1,
	ACPI_AML_RESOURCE_ADDRESS_SPACE_ID_PCI_CONF_SPACE=2,
	ACPI_AML_RESOURCE_ADDRESS_SPACE_ID_EMBEDED_CONTROLLER=3,
	ACPI_AML_RESOURCE_ADDRESS_SPACE_ID_SMBUS=4,
	ACPI_AML_RESOURCE_ADDRESS_SPACE_ID_CMOS=5,
	ACPI_AML_RESOURCE_ADDRESS_SPACE_ID_PCI_BAR_TARGET=6,
	ACPI_AML_RESOURCE_ADDRESS_SPACE_ID_IPMI=7,
	ACPI_AML_RESOURCE_ADDRESS_SPACE_ID_GPIO=8,
	ACPI_AML_RESOURCE_ADDRESS_SPACE_ID_SERIALBUS=9,
	ACPI_AML_RESOURCE_ADDRESS_SPACE_ID_PCC=10,
	ACPI_AML_RESOURCE_ADDRESS_SPACE_ID_FUNCTIONAL_FIXED_HARDWARE=0X7F,
} acpi_aml_resource_address_space_id_t;

typedef enum {
	ACPI_AML_RESOURCE_ACCESS_SIZE_UNDEFINED=0,
	ACPI_AML_RESOURCE_ACCESS_SIZE_BYTE=1,
	ACPI_AML_RESOURCE_ACCESS_SIZE_WORD=2,
	ACPI_AML_RESOURCE_ACCESS_SIZE_DWORD=3,
	ACPI_AML_RESOURCE_ACCESS_SIZE_QWORD=4,
}acpi_aml_resource_access_size_t;



typedef struct {
	uint8_t write : 1;
	uint8_t mem : 2;
	uint8_t mtp : 2;
	uint8_t ttp : 1;
	uint8_t reserved : 2;
} __attribute__((packed)) acpi_aml_resource_memory_flag_t;

typedef struct {
	uint8_t rng : 2;
	uint8_t reserved1 : 2;
	uint8_t ttp : 1;
	uint8_t trs : 1;
	uint8_t reserved2 : 2;
} __attribute__((packed)) acpi_aml_resource_io_flag_t;

typedef struct {
	uint8_t reserved;
} __attribute__((packed))  acpi_aml_resource_bus_number_flag_t;

typedef union {
	acpi_aml_resource_memory_flag_t memory_flag;
	acpi_aml_resource_io_flag_t io_flag;
	acpi_aml_resource_bus_number_flag_t bus_number_flag;
}__attribute__((packed)) acpi_aml_resource_type_specific_flag_t;

typedef struct {
	uint8_t length : 3;
	acpi_aml_resource_smallitem_name_t name : 4;
	acpi_aml_resource_type_t type : 1;

	union {
		struct {
			uint16_t irq_masks : 16;
			uint8_t mode : 1;
			uint8_t ignored : 2;
			uint8_t polarity : 1;
			uint8_t sharing : 1;
			uint8_t wake_capability : 1;
			uint8_t reserved : 2;
		}__attribute__((packed)) irq;

		struct {
			uint8_t channels : 8;
			uint8_t size : 2;
			uint8_t bus_master_status : 1;
			uint8_t ignored : 2;
			uint8_t speed_type : 2;
			uint8_t zero : 1;
		}__attribute__((packed)) dma;

		struct {
			uint8_t compability : 2;
			uint8_t performance : 2;
			uint8_t reserved : 4;
		}__attribute__((packed)) start_depfunc;

		struct {
			uint8_t decode : 1;
			uint8_t unused : 7;
			uint16_t min : 16;
			uint16_t max : 16;
			uint8_t align : 8;
			uint8_t length : 8;
		}__attribute__((packed)) io;

		struct {
			uint16_t base : 10;
			uint8_t unused : 6;
			uint16_t length : 16;
		}__attribute__((packed)) fixedio;

		struct {
			uint16_t request_line : 16;
			uint16_t channel : 16;
			uint8_t size : 8;
		}__attribute__((packed)) fixeddma;

		struct {
			uint8_t checksum : 8;
		}__attribute__((packed)) end_tag;

	}__attribute__((packed));

}__attribute__((packed)) acpi_aml_resource_smallitem_t;


typedef struct {
	acpi_aml_resource_largeitem_name_t name : 7;
	acpi_aml_resource_type_t type : 1;
	uint16_t length : 16;

	union {
		struct {
			uint8_t readwrite : 1;
			uint8_t ignored : 7;
			uint16_t min : 16;
			uint16_t max : 16;
			uint16_t align : 16;
			uint16_t length : 16;
		} __attribute__((packed)) memory_range_24bit;

		struct {
			uint8_t uuid_sub_type;
			uint8_t uuid[16];
			uint8_t vendor_data[0];
		} __attribute__((packed)) vendor;

		struct {
			uint8_t rw : 1;
			uint8_t ignored : 7;
			uint32_t min;
			uint32_t max;
			uint32_t align;
			uint32_t length;
		} __attribute__((packed)) memory_range_32bit;

		struct {
			uint8_t rw : 1;
			uint8_t ignored : 7;
			uint32_t base;
			uint32_t length;
		} __attribute__((packed)) memory_range_32bit_fixed;

		struct {
			acpi_aml_resource_word_address_space_type_t type : 8;
			uint8_t ignored : 1;
			uint8_t decode_type : 1;
			uint8_t min_address_fixed : 1;
			uint8_t max_address_fixed : 1;
			uint8_t reserved : 4;
			acpi_aml_resource_type_specific_flag_t type_spesific_flags;
			uint16_t gra;
			uint16_t min;
			uint16_t max;
			uint16_t translation_offset;
			uint16_t length;
			uint8_t resource_source_index;
			char_t resource_source[0];
		} __attribute__((packed)) word_address_space;

		struct {
			acpi_aml_resource_word_address_space_type_t type : 8;
			uint8_t ignored : 1;
			uint8_t decode_type : 1;
			uint8_t min_address_fixed : 1;
			uint8_t max_address_fixed : 1;
			uint8_t reserved : 4;
			acpi_aml_resource_type_specific_flag_t type_spesific_flags;
			uint32_t gra;
			uint32_t min;
			uint32_t max;
			uint32_t translation_offset;
			uint32_t length;
			uint8_t resource_source_index;
			char_t resource_source[0];
		} __attribute__((packed)) dword_address_space;

		struct {
			acpi_aml_resource_word_address_space_type_t type : 8;
			uint8_t ignored : 1;
			uint8_t decode_type : 1;
			uint8_t min_address_fixed : 1;
			uint8_t max_address_fixed : 1;
			uint8_t reserved : 4;
			acpi_aml_resource_type_specific_flag_t type_spesific_flags;
			uint64_t gra;
			uint64_t min;
			uint64_t max;
			uint64_t translation_offset;
			uint64_t length;
			uint8_t resource_source_index;
			char_t resource_source[0];
		} __attribute__((packed)) qword_address_space;

		struct {
			uint8_t ignored : 1;
			uint8_t decode_type : 1;
			uint8_t min_address_fixed : 1;
			uint8_t max_address_fixed : 1;
			uint8_t reserved : 4;
			acpi_aml_resource_type_specific_flag_t type_spesific_flags;
			uint64_t gra;
			uint64_t min;
			uint64_t max;
			uint64_t translation_offset;
			uint64_t length;
			uint8_t type_spesific_attribute;
			uint64_t attribute;
		} __attribute__((packed)) extended_address_space;

		struct {
			uint8_t consumer : 1;
			uint8_t mode : 1;
			uint8_t polarity : 1;
			uint8_t sharing : 1;
			uint8_t wake_capability : 1;
			uint8_t reserved : 3;
			uint8_t count;
			uint32_t interrupts[0];
			// resource source and caps
		} __attribute__((packed)) extended_interrupt;
		struct {
			acpi_aml_resource_address_space_id_t asi : 8;
			uint8_t register_bit_width;
			uint8_t register_bit_offset;
			acpi_aml_resource_access_size_t access_size : 8;
			uint64_t address;
		} __attribute__((packed)) generic_register;

		struct {
			uint8_t data[0];
		} __attribute__((packed)) gpio_descriptor;

		struct {
			uint8_t data[0];
		} __attribute__((packed)) generic_serial_bus;

		struct {
			uint8_t data[0];
		} __attribute__((packed)) pin_function;

		struct {
			uint8_t data[0];
		} __attribute__((packed)) pin_configuration;

		struct {
			uint8_t data[0];
		} __attribute__((packed)) pin_group;

		struct {
			uint8_t revision_id : 8;
			uint16_t flags : 16;
			uint16_t function : 16;
			uint8_t source_index : 8;
			uint16_t source_name_offset : 16;
			uint16_t source_label_offset : 16;
			uint16_t vendor_data_offset : 16;
			uint16_t vendor_data_length : 16;
		} __attribute__((packed)) pin_group_function;

		struct {
			uint8_t revision_id : 8;
			uint16_t flags : 16;
			uint8_t type : 8;
			uint32_t value : 32;
			uint8_t source_index : 8;
			uint16_t source_name_offset : 16;
			uint16_t source_label_offset : 16;
			uint16_t vendor_data_length : 16;
		} __attribute__((packed)) pin_group_configuration;

	} __attribute__((packed));

} __attribute__((packed)) acpi_aml_resource_largeitem_t;


typedef struct {
	uint8_t unused : 7;
	acpi_aml_resource_type_t type : 1;
} __attribute__((packed)) acpi_aml_resource_type_identifier_t;


typedef union {
	acpi_aml_resource_type_identifier_t identifier;
	acpi_aml_resource_smallitem_t smallitem;
	acpi_aml_resource_largeitem_t largeitem;

}__attribute__((packed)) acpi_aml_resource_t;


int32_t acpi_aml_resource_parse(acpi_aml_parser_context_t* ctx, acpi_aml_device_t* device, acpi_aml_object_t* buffer);


#endif
