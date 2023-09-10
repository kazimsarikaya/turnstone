/**
 * @file scsi.h
 * @brief SCSI command definitions
 */

#ifndef ___SCSI_H
#define ___SCSI_H

#include <types.h>
#include <utils.h>


typedef enum scsi_command_opcode_t {
    SCSI_COMMAND_OPCODE_TEST_UNIT_READY = 0x00,
    SCSI_COMMAND_OPCODE_INQUIRY = 0x12,
    SCSI_COMMAND_OPCODE_READ_CAPACITY_16 = 0x9E,
    SCSI_COMMAND_OPCODE_READ_CAPACITY_10 = 0x25,
    SCSI_COMMAND_OPCODE_READ_16 = 0x88,
    SCSI_COMMAND_OPCODE_READ_10 = 0x28,
    SCSI_COMMAND_OPCODE_WRITE_16 = 0x8A,
    SCSI_COMMAND_OPCODE_WRITE_10 = 0x2A,
    SCSI_COMMAND_OPCODE_SYNCHRONIZE_CACHE_16 = 0x91,
    SCSI_COMMAND_OPCODE_SYNCHRONIZE_CACHE_10 = 0x35,
} scsi_command_opcode_t;

typedef struct scsi_command_status_t {
    uint8_t key;
    uint8_t code;
    uint8_t qualifier;
}__attribute__((packed)) scsi_command_status_t;

typedef struct scsi_command_test_unit_ready_t {
    uint8_t opcode;
    uint8_t reserved[4];
    uint8_t control;
}__attribute__((packed)) scsi_command_test_unit_ready_t;

_Static_assert(sizeof(scsi_command_test_unit_ready_t) == 6, "scsi_command_test_unit_ready_t size mismatch");

typedef struct scsi_command_inquiry_t {
    uint8_t opcode;
    uint8_t evpd      : 1;
    uint8_t reserved1 : 7;
    uint8_t page_code;
    uint8_t allocation_length[2];
    uint8_t control;
}__attribute__((packed)) scsi_command_inquiry_t;

_Static_assert(sizeof(scsi_command_inquiry_t) == 6, "scsi_command_inquiry_t size mismatch");

typedef struct scsi_standard_inquiry_data_t {
    uint8_t peripheral_device_type : 5;
    uint8_t peripheral_qualifier   : 3;
    uint8_t reserved1              : 7;
    uint8_t rmb                    : 1;
    uint8_t version;
    uint8_t response_data_format : 4;
    uint8_t hisup                : 1;
    uint8_t normaca              : 1;
    uint8_t reserved2            : 2;
    uint8_t additional_length;
    uint8_t protect                    : 1;
    uint8_t reserved3                  : 2;
    uint8_t third_party_copy           : 1;
    uint8_t target_port_group_support  : 2;
    uint8_t access_control_coordinator : 1;
    uint8_t scc_support                : 1;
    uint8_t reserved4                  : 4;
    uint8_t multi_port_support         : 1;
    uint8_t vs1                        : 1;
    uint8_t enclosure_services         : 1;
    uint8_t reserved5                  : 1;
    uint8_t vs2                        : 1;
    uint8_t cmdque                     : 1;
    uint8_t reserved6                  : 6;
    uint8_t vendor_id[8];
    uint8_t product_id[16];
    uint8_t product_revision_level[4];
    uint8_t drive_serial_number[8];
    uint8_t reserved7[468];
}__attribute__((packed)) scsi_standard_inquiry_data_t;

_Static_assert(sizeof(scsi_standard_inquiry_data_t) == 512, "scsi_standard_inquiry_data_t size mismatch");

typedef struct scsi_command_read_capacity_16_t {
    uint8_t opcode;
    uint8_t service_action : 5;
    uint8_t reserved1      : 3;
    uint8_t reserved2[8];
    uint8_t allocation_length[4];
    uint8_t reserved3;
    uint8_t control;
}__attribute__((packed)) scsi_command_read_capacity_16_t;

_Static_assert(sizeof(scsi_command_read_capacity_16_t) == 16, "scsi_command_read_capacity_16_t size mismatch");

typedef struct scsi_capacity_16_t {
    uint64_t last_logical_block_address;
    uint32_t logical_block_length;
    uint8_t  reserved[20];
}__attribute__((packed)) scsi_capacity_16_t;

_Static_assert(sizeof(scsi_capacity_16_t) == 32, "scsi_capacity_t size mismatch");

typedef struct scsi_command_read_capacity_10_t {
    uint8_t  opcode;
    uint8_t  reserved1;
    uint32_t lba;
    uint8_t  reserved3[3];
    uint8_t  control;
}__attribute__((packed)) scsi_command_read_capacity_10_t;

_Static_assert(sizeof(scsi_command_read_capacity_10_t) == 10, "scsi_command_read_capacity_10_t size mismatch");

typedef struct scsi_capacity_10_t {
    uint32_t last_logical_block_address;
    uint32_t logical_block_length;
}__attribute__((packed)) scsi_capacity_10_t;

_Static_assert(sizeof(scsi_capacity_10_t) == 8, "scsi_capacity_10_t size mismatch");

typedef struct scsi_command_read_16_t {
    uint8_t  opcode;
    uint8_t  dld2      : 1;
    uint8_t  reserved1 : 1;
    uint8_t  rarc      : 1;
    uint8_t  fua       : 1;
    uint8_t  dpo       : 1;
    uint8_t  rdprotect : 3;
    uint64_t lba;
    uint32_t transfer_length;
    uint8_t  group_number : 6;
    uint8_t  dld0         : 1;
    uint8_t  dld1         : 1;
    uint8_t  control;
}__attribute__((packed)) scsi_command_read_16_t;

_Static_assert(sizeof(scsi_command_read_16_t) == 16, "scsi_command_read_16_t size mismatch");

typedef struct scsi_command_read_10_t {
    uint8_t  opcode;
    uint8_t  reserved0 : 2;
    uint8_t  rarc      : 1;
    uint8_t  fua       : 1;
    uint8_t  dpo       : 1;
    uint8_t  rdprotect : 3;
    uint32_t lba;
    uint8_t  group_number : 5;
    uint8_t  reserved2    : 3;
    uint16_t transfer_length;
    uint8_t  control;
}__attribute__((packed)) scsi_command_read_10_t;

_Static_assert(sizeof(scsi_command_read_10_t) == 10, "scsi_command_read_10_t size mismatch");

typedef struct scsi_command_write_16_t {
    uint8_t  opcode;
    uint8_t  dld2      : 1;
    uint8_t  reserved1 : 1;
    uint8_t  rarc      : 1;
    uint8_t  fua       : 1;
    uint8_t  dpo       : 1;
    uint8_t  wrprotect : 3;
    uint64_t lba;
    uint32_t transfer_length;
    uint8_t  group_number : 6;
    uint8_t  dld0         : 1;
    uint8_t  dld1         : 1;
    uint8_t  control;
}__attribute__((packed)) scsi_command_write_16_t;

_Static_assert(sizeof(scsi_command_write_16_t) == 16, "scsi_command_write_16_t size mismatch");

typedef struct scsi_command_write_10_t {
    uint8_t  opcode;
    uint8_t  reserved0 : 2;
    uint8_t  rarc      : 1;
    uint8_t  fua       : 1;
    uint8_t  dpo       : 1;
    uint8_t  rdprotect : 3;
    uint32_t lba;
    uint8_t  group_number : 5;
    uint8_t  reserved2    : 3;
    uint16_t transfer_length;
    uint8_t  control;
}__attribute__((packed)) scsi_command_write_10_t;

_Static_assert(sizeof(scsi_command_write_10_t) == 10, "scsi_command_write_10_t size mismatch");

typedef struct scsi_command_sync_cache_16_t {
    uint8_t  opcode;
    uint8_t  reserved0 : 1;
    uint8_t  immed     : 1;
    uint8_t  reserved1 : 6;
    uint64_t lba;
    uint32_t number_of_blocks;
    uint8_t  group_number : 5;
    uint8_t  reserved2    : 3;
    uint8_t  control;
}__attribute__((packed)) scsi_command_sync_cache_16_t;

_Static_assert(sizeof(scsi_command_sync_cache_16_t) == 16, "scsi_command_sync_cache_16_t size mismatch");

typedef struct scsi_command_sync_cache_10_t {
    uint8_t  opcode;
    uint8_t  reserved0 : 1;
    uint8_t  immed     : 1;
    uint8_t  reserved1 : 6;
    uint32_t lba;
    uint8_t  group_number : 5;
    uint8_t  reserved2    : 3;
    uint16_t number_of_blocks;
    uint8_t  control;
}__attribute__((packed)) scsi_command_sync_cache_10_t;

_Static_assert(sizeof(scsi_command_sync_cache_10_t) == 10, "scsi_command_sync_cache_10_t size mismatch");


#endif /* ___SCSI_H */
